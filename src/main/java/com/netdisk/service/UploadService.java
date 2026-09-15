package com.netdisk.service;

import com.netdisk.common.BusinessException;
import com.netdisk.entity.File;
import com.netdisk.entity.User;
import com.netdisk.mapper.FileMapper;
import com.netdisk.mapper.UserMapper;
import com.netdisk.vo.FileVO;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import org.springframework.web.multipart.MultipartFile;

/**
 * 上传服务：普通上传（流式落盘 + 配额校验 + 原子扣减 + 写元数据）。
 */
@Service
public class UploadService {

    private final FileMapper fileMapper;
    private final UserMapper userMapper;
    private final StorageService storageService;

    public UploadService(FileMapper fileMapper, UserMapper userMapper, StorageService storageService) {
        this.fileMapper = fileMapper;
        this.userMapper = userMapper;
        this.storageService = storageService;
    }

    @Transactional
    public FileVO upload(Long userId, Long parentId, MultipartFile file) {
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

        String storageKey = null;
        try {
            storageKey = storageService.store(userId, file);
            // 原子扣减配额（条件更新，防止并发超卖）
            int updated = userMapper.increaseQuotaUsed(userId, file.getSize());
            if (updated == 0) {
                throw new BusinessException(400, "空间不足");
            }
            File f = new File();
            f.setUserId(userId);
            f.setParentId(pid);
            f.setName(originalName);
            f.setIsDir(0);
            f.setSize(file.getSize());
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

    private void requireOwnedDir(Long userId, Long id) {
        File f = fileMapper.findById(id);
        if (f == null || !f.getUserId().equals(userId) || f.getIsDir() == null || f.getIsDir() != 1) {
            throw new BusinessException(400, "目录不存在");
        }
    }
}
