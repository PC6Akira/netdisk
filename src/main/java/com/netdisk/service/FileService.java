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

    public FileService(FileMapper fileMapper) {
        this.fileMapper = fileMapper;
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
        File f = requireOwned(userId, id);
        // 物理删除：收集文件夹下所有后代 id，连同自身一起删
        List<Long> ids = new ArrayList<>();
        collectDescendants(userId, id, ids);
        ids.add(id);
        fileMapper.deleteByIds(ids);
    }

    private void collectDescendants(Long userId, Long parentId, List<Long> acc) {
        List<File> children = fileMapper.findByUserIdAndParentId(userId, parentId);
        for (File child : children) {
            acc.add(child.getId());
            if (child.getIsDir() != null && child.getIsDir() == 1) {
                collectDescendants(userId, child.getId(), acc);
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
