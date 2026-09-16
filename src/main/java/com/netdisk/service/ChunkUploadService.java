package com.netdisk.service;

import com.netdisk.common.BusinessException;
import com.netdisk.dto.CompleteUploadRequest;
import com.netdisk.dto.InitUploadRequest;
import com.netdisk.entity.File;
import com.netdisk.entity.UploadSession;
import com.netdisk.entity.User;
import com.netdisk.mapper.FileChunkMapper;
import com.netdisk.mapper.FileMapper;
import com.netdisk.mapper.UploadSessionMapper;
import com.netdisk.mapper.UserMapper;
import com.netdisk.vo.FileVO;
import com.netdisk.vo.InitUploadVO;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Isolation;
import org.springframework.transaction.annotation.Transactional;
import org.springframework.web.multipart.MultipartFile;

import java.util.List;

/**
 * 分片上传服务：创建会话、上传分片、断点续传、合并完成。
 * 合并时服务端重算 MD5 做完整性校验，成功后再原子扣配额 + 写元数据。
 */
@Service
public class ChunkUploadService {

    /** 单片大小：5MB */
    private static final int CHUNK_SIZE = 5 * 1024 * 1024;

    private final UploadSessionMapper sessionMapper;
    private final FileChunkMapper chunkMapper;
    private final FileMapper fileMapper;
    private final UserMapper userMapper;
    private final StorageService storageService;
    private final QuotaService quotaService;

    public ChunkUploadService(UploadSessionMapper sessionMapper, FileChunkMapper chunkMapper,
                              FileMapper fileMapper, UserMapper userMapper, StorageService storageService,
                              QuotaService quotaService) {
        this.sessionMapper = sessionMapper;
        this.chunkMapper = chunkMapper;
        this.fileMapper = fileMapper;
        this.userMapper = userMapper;
        this.storageService = storageService;
        this.quotaService = quotaService;
    }

    public InitUploadVO init(Long userId, InitUploadRequest req) {
        if (req == null || isBlank(req.getName())) {
            throw new BusinessException(400, "文件名不能为空");
        }
        String md5 = normalizeMd5(req.getMd5());
        if (md5 == null) {
            throw new BusinessException(400, "文件 MD5 无效");
        }
        if (req.getSize() == null || req.getSize() <= 0) {
            throw new BusinessException(400, "文件大小无效");
        }
        Long pid = req.getParentId() == null ? 0L : req.getParentId();
        if (pid != 0) {
            requireOwnedDir(userId, pid);
        }
        if (fileMapper.findByNameAndParent(userId, pid, req.getName()) != null) {
            throw new BusinessException(400, "同名文件或文件夹已存在");
        }
        // 预检配额
        User user = userMapper.findById(userId);
        if (req.getSize() > user.getQuotaLimit() - user.getQuotaUsed()) {
            throw new BusinessException(400, "空间不足");
        }

        int totalChunks = (int) ((req.getSize() + CHUNK_SIZE - 1) / CHUNK_SIZE);

        // 断点续传：同一个 (userId, md5, parentId) 下存在未完成会话则复用，返回已传分片
        UploadSession session = sessionMapper.findByUserMd5ParentStatus(userId, md5, pid, 0);
        if (session == null) {
            session = new UploadSession();
            session.setUserId(userId);
            session.setFileName(req.getName());
            session.setParentId(pid);
            session.setTotalSize(req.getSize());
            session.setChunkSize(CHUNK_SIZE);
            session.setTotalChunks(totalChunks);
            session.setMd5(md5);
            session.setStatus(0);
            sessionMapper.insert(session);
        }
        List<Integer> uploaded = chunkMapper.findIndexesBySessionId(session.getId());
        return InitUploadVO.of(session.getId(), CHUNK_SIZE, session.getTotalChunks(), uploaded);
    }

    public void uploadChunk(Long userId, Long sessionId, int chunkIndex, MultipartFile chunk) {
        UploadSession session = requireSession(userId, sessionId);
        if (session.getStatus() != null && session.getStatus() != 0) {
            throw new BusinessException(400, "会话已结束，无法继续上传");
        }
        if (chunk == null || chunk.isEmpty()) {
            throw new BusinessException(400, "分片不能为空");
        }
        if (chunkIndex < 0 || chunkIndex >= session.getTotalChunks()) {
            throw new BusinessException(400, "分片序号非法");
        }
        storageService.storeChunk(sessionId, chunkIndex, chunk);
        chunkMapper.upsert(sessionId, chunkIndex);
    }

    public InitUploadVO status(Long userId, Long sessionId) {
        UploadSession session = requireSession(userId, sessionId);
        List<Integer> uploaded = chunkMapper.findIndexesBySessionId(sessionId);
        return InitUploadVO.of(sessionId, session.getChunkSize(), session.getTotalChunks(), uploaded);
    }

    @Transactional(isolation = Isolation.READ_COMMITTED)
    public FileVO complete(Long userId, CompleteUploadRequest req) {
        if (req == null || req.getSessionId() == null) {
            throw new BusinessException(400, "会话 ID 不能为空");
        }
        UploadSession session = requireSession(userId, req.getSessionId());
        if (session.getStatus() != null && session.getStatus() != 0) {
            throw new BusinessException(400, "会话已结束");
        }
        int uploaded = chunkMapper.countBySessionId(session.getId());
        if (uploaded != session.getTotalChunks()) {
            throw new BusinessException(400, "分片不完整：" + uploaded + "/" + session.getTotalChunks());
        }

        // 流式合并 + 服务端重算 MD5 做完整性校验
        String targetKey = storageService.newKey(userId);
        String actualMd5 = storageService.mergeChunks(session.getId(), session.getTotalChunks(), targetKey);
        if (session.getMd5() != null && !session.getMd5().equalsIgnoreCase(actualMd5)) {
            storageService.delete(targetKey);
            throw new BusinessException(400, "文件校验失败（MD5 不一致）");
        }

        File f = null;
        try {
            quotaService.deductQuota(userId, session.getTotalSize());
            f = new File();
            f.setUserId(userId);
            f.setParentId(session.getParentId());
            f.setName(session.getFileName());
            f.setIsDir(0);
            f.setSize(session.getTotalSize());
            f.setMd5(session.getMd5());
            f.setStorageKey(targetKey);
            f.setRefCount(1);
            f.setIsDeleted(0);
            fileMapper.insert(f);
            sessionMapper.updateStatus(session.getId(), 2);
        } catch (RuntimeException e) {
            // 事务回滚 DB 写入，这里回滚已合并落盘的文件
            storageService.delete(targetKey);
            throw e;
        }

        // 清理分片临时文件与分片记录
        storageService.deleteChunks(session.getId());
        chunkMapper.deleteBySessionId(session.getId());
        return FileVO.from(f);
    }

    private UploadSession requireSession(Long userId, Long sessionId) {
        UploadSession s = sessionMapper.findById(sessionId);
        if (s == null || !s.getUserId().equals(userId)) {
            throw new BusinessException(404, "会话不存在");
        }
        return s;
    }

    private void requireOwnedDir(Long userId, Long id) {
        File f = fileMapper.findById(id);
        if (f == null || !f.getUserId().equals(userId) || f.getIsDir() == null || f.getIsDir() != 1) {
            throw new BusinessException(400, "目录不存在");
        }
    }

    private String normalizeMd5(String md5) {
        if (md5 == null) {
            return null;
        }
        String m = md5.trim().toLowerCase();
        return m.matches("[0-9a-f]{32}") ? m : null;
    }

    private boolean isBlank(String s) {
        return s == null || s.isBlank();
    }
}
