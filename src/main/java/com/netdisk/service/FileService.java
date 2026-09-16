package com.netdisk.service;

import com.netdisk.common.BusinessException;
import com.netdisk.dto.CreateFolderRequest;
import com.netdisk.dto.RenameRequest;
import com.netdisk.entity.File;
import com.netdisk.mapper.FileMapper;
import com.netdisk.vo.FileVO;
import org.springframework.stereotype.Service;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

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

        // 非目录文件按 storage_key 分组，维护引用计数：归零才删物理文件
        Map<String, Integer> delCountByKey = new HashMap<>();
        for (File f : all) {
            if (f.getIsDir() != null && f.getIsDir() == 0 && f.getStorageKey() != null) {
                delCountByKey.merge(f.getStorageKey(), 1, Integer::sum);
            }
        }
        for (Map.Entry<String, Integer> e : delCountByKey.entrySet()) {
            String key = e.getKey();
            int nDel = e.getValue();
            int before = refCountOf(all, key);
            int after = before - nDel;
            if (after <= 0) {
                // 该物理文件没有其他引用，删除物理文件
                storageService.delete(key);
            } else {
                // 仍有其他行引用同一物理文件，只把引用计数扣掉（含本批待删行，稍后一并删除）
                fileMapper.decrementRefCountByStorageKey(key, nDel);
            }
        }

        // 先删 DB 记录（单条批量 DELETE，原子）
        fileMapper.deleteByIds(all.stream().map(File::getId).toList());
    }

    private int refCountOf(List<File> files, String storageKey) {
        for (File f : files) {
            if (storageKey.equals(f.getStorageKey())) {
                return f.getRefCount() == null ? 0 : f.getRefCount();
            }
        }
        return 0;
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
