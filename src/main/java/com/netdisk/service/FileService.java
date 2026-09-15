package com.netdisk.service;

import com.netdisk.common.BusinessException;
import com.netdisk.dto.CreateFolderRequest;
import com.netdisk.dto.RenameRequest;
import com.netdisk.entity.File;
import com.netdisk.mapper.FileMapper;
import com.netdisk.vo.FileVO;
import org.springframework.stereotype.Service;

import java.util.ArrayList;
import java.util.List;

/**
 * 文件/目录服务：列表、新建文件夹、重命名、删除。
 */
@Service
public class FileService {

    private final FileMapper fileMapper;
    private final StorageService storageService;

    public FileService(FileMapper fileMapper, StorageService storageService) {
        this.fileMapper = fileMapper;
        this.storageService = storageService;
    }

    public List<FileVO> list(Long userId, Long parentId) {
        Long pid = parentId == null ? 0L : parentId;
        if (pid != 0) {
            requireOwnedDir(userId, pid);
        }
        return fileMapper.findByUserIdAndParentId(userId, pid)
                .stream()
                .map(FileVO::from)
                .toList();
    }

    public FileVO createFolder(Long userId, CreateFolderRequest req) {
        if (isBlank(req.getName())) {
            throw new BusinessException(400, "文件夹名不能为空");
        }
        Long pid = req.getParentId() == null ? 0L : req.getParentId();
        if (pid != 0) {
            requireOwnedDir(userId, pid);
        }
        if (fileMapper.findByNameAndParent(userId, pid, req.getName()) != null) {
            throw new BusinessException(400, "同名文件或文件夹已存在");
        }

        File f = new File();
        f.setUserId(userId);
        f.setParentId(pid);
        f.setName(req.getName());
        f.setIsDir(1);
        f.setSize(0L);
        f.setRefCount(1);
        f.setIsDeleted(0);
        fileMapper.insert(f);
        return FileVO.from(f);
    }

    public FileVO rename(Long userId, Long id, RenameRequest req) {
        if (isBlank(req.getName())) {
            throw new BusinessException(400, "名称不能为空");
        }
        File f = requireOwned(userId, id);
        if (fileMapper.findByNameAndParent(userId, f.getParentId(), req.getName()) != null) {
            throw new BusinessException(400, "同名文件或文件夹已存在");
        }
        fileMapper.updateName(id, req.getName());
        f.setName(req.getName());
        return FileVO.from(f);
    }

    public void delete(Long userId, Long id) {
        File root = requireOwned(userId, id);
        List<File> all = new ArrayList<>();
        collectAll(userId, id, all);
        all.add(root);
        // 先删 DB 记录（单条批量 DELETE，原子），再删物理文件（best-effort）
        fileMapper.deleteByIds(all.stream().map(File::getId).toList());
        for (File f : all) {
            if (f.getIsDir() != null && f.getIsDir() == 0 && f.getStorageKey() != null) {
                storageService.delete(f.getStorageKey());
            }
        }
    }

    private void collectAll(Long userId, Long parentId, List<File> acc) {
        List<File> children = fileMapper.findByUserIdAndParentId(userId, parentId);
        for (File child : children) {
            acc.add(child);
            if (child.getIsDir() != null && child.getIsDir() == 1) {
                collectAll(userId, child.getId(), acc);
            }
        }
    }

    private File requireOwned(Long userId, Long id) {
        File f = fileMapper.findById(id);
        if (f == null || !f.getUserId().equals(userId)) {
            throw new BusinessException(404, "文件或文件夹不存在");
        }
        return f;
    }

    private void requireOwnedDir(Long userId, Long id) {
        File f = fileMapper.findById(id);
        if (f == null || !f.getUserId().equals(userId) || f.getIsDir() == null || f.getIsDir() != 1) {
            throw new BusinessException(400, "目录不存在");
        }
    }

    private boolean isBlank(String s) {
        return s == null || s.isBlank();
    }
}
