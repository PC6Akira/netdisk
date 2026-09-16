package com.netdisk.service;

import com.netdisk.common.BusinessException;
import com.netdisk.dto.InstantUploadRequest;
import com.netdisk.entity.File;
import com.netdisk.entity.User;
import com.netdisk.mapper.FileMapper;
import com.netdisk.mapper.UserMapper;
import com.netdisk.vo.FileVO;
import com.netdisk.vo.InstantUploadVO;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Isolation;
import org.springframework.transaction.annotation.Transactional;
import org.springframework.web.multipart.MultipartFile;

/**
 * 上传服务：普通上传（流式落盘 + 配额校验 + 原子扣减 + 写元数据）+ 秒传（MD5 去重 + 引用计数）。
 */
@Service
public class UploadService {

    private final FileMapper fileMapper;
    private final UserMapper userMapper;
    private final StorageService storageService;
    private final QuotaService quotaService;

    public UploadService(FileMapper fileMapper, UserMapper userMapper, StorageService storageService,
                         QuotaService quotaService) {
        this.fileMapper = fileMapper;
        this.userMapper = userMapper;
        this.storageService = storageService;
        this.quotaService = quotaService;
    }

    /**
     * 秒传：客户端已算好 MD5，命中则复用已有物理文件 + 引用计数 +1，不传文件本体直接秒回。
     */
    @Transactional(isolation = Isolation.READ_COMMITTED)
    public InstantUploadVO instant(Long userId, InstantUploadRequest req) {
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

        // 悲观锁：FOR UPDATE 锁最早的 canonical 行，串行化同一内容的并发秒传（ref_count 读改写）
        File source = fileMapper.findByMd5ForUpdate(md5);
        if (source == null) {
            return InstantUploadVO.miss();
        }

        // 命中：扣配额（乐观锁 version 重试）+ 复用物理文件 + 引用计数 +1（全局去重，跨用户复用）
        quotaService.deductQuota(userId, req.getSize());
        File f = new File();
        f.setUserId(userId);
        f.setParentId(pid);
        f.setName(req.getName());
        f.setIsDir(0);
        f.setSize(req.getSize());
        f.setMd5(md5);
        f.setStorageKey(source.getStorageKey());
        f.setRefCount(source.getRefCount());
        f.setIsDeleted(0);
        fileMapper.insert(f);
        // 统一把该 storage_key 下所有行的引用计数 +1（含 source 与本行，最终一致）
        fileMapper.incrementRefCountByStorageKey(source.getStorageKey());
        return InstantUploadVO.hit(FileVO.from(f));
    }

    @Transactional(isolation = Isolation.READ_COMMITTED)
    public FileVO upload(Long userId, Long parentId, MultipartFile file, String md5Param) {
        if (file == null || file.isEmpty()) {
            throw new BusinessException(400, "文件不能为空");
        }
        Long pid = parentId == null ? 0L : parentId;
        if (pid != 0) {
            requireOwnedDir(userId, pid);
        }
        String originalName = file.getOriginalFilename();
        if (originalName == null || originalName.isBlank()) {
            throw new BusinessException(400, "文件名不能为空");
        }
        if (fileMapper.findByNameAndParent(userId, pid, originalName) != null) {
            throw new BusinessException(400, "同名文件或文件夹已存在");
        }
        // 预检配额
        User user = userMapper.findById(userId);
        if (file.getSize() > user.getQuotaLimit() - user.getQuotaUsed()) {
            throw new BusinessException(400, "空间不足");
        }
        String md5 = normalizeMd5(md5Param);
        if (md5Param != null && !md5Param.isBlank() && md5 == null) {
            throw new BusinessException(400, "文件 MD5 无效");
        }

        String storageKey = null;
        try {
            storageKey = storageService.store(userId, file);
            // 扣减配额（乐观锁 version 重试）
            quotaService.deductQuota(userId, file.getSize());
            File f = new File();
            f.setUserId(userId);
            f.setParentId(pid);
            f.setName(originalName);
            f.setIsDir(0);
            f.setSize(file.getSize());
            f.setMd5(md5);
            f.setStorageKey(storageKey);
            f.setRefCount(1);
            f.setIsDeleted(0);
            fileMapper.insert(f);
            return FileVO.from(f);
        } catch (RuntimeException e) {
            // 事务回滚 DB 写入，这里回滚已落盘的文件
            if (storageKey != null) {
                storageService.delete(storageKey);
            }
            throw e;
        }
    }

    /**
     * 归一化 MD5：去空白、转小写，校验 32 位十六进制；不合法返回 null。
     */
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

    private void requireOwnedDir(Long userId, Long id) {
        File f = fileMapper.findById(id);
        if (f == null || !f.getUserId().equals(userId) || f.getIsDir() == null || f.getIsDir() != 1) {
            throw new BusinessException(400, "目录不存在");
        }
    }
}
