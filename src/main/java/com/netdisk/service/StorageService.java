package com.netdisk.service;

import com.netdisk.common.BusinessException;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;
import org.springframework.web.multipart.MultipartFile;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
import java.time.LocalDate;
import java.time.format.DateTimeFormatter;
import java.util.UUID;

/**
 * 文件存储服务：负责文件落盘、路径解析与删除。
 * storage_key 是相对存储根目录的路径，不含原始文件名（防路径穿越与重名冲突）。
 */
@Service
public class StorageService {

    private final Path root;

    public StorageService(@Value("${netdisk.storage-root:./data}") String storageRoot) {
        this.root = Paths.get(storageRoot).toAbsolutePath().normalize();
        try {
            Files.createDirectories(root);
        } catch (IOException e) {
            throw new IllegalStateException("无法创建存储目录: " + root, e);
        }
    }

    /**
     * 流式落盘，返回 storage_key。
     */
    public String store(Long userId, MultipartFile file) {
        String key = userId + "/" + LocalDate.now().format(DateTimeFormatter.ofPattern("yyyyMMdd"))
                + "/" + UUID.randomUUID();
        Path target = resolve(key);
        try {
            Files.createDirectories(target.getParent());
            try (InputStream in = file.getInputStream()) {
                Files.copy(in, target, StandardCopyOption.REPLACE_EXISTING);
            }
        } catch (IOException e) {
            throw new BusinessException(500, "文件保存失败");
        }
        return key;
    }

    public Path resolve(String storageKey) {
        Path p = root.resolve(storageKey).normalize();
        if (!p.startsWith(root)) {
            throw new BusinessException(400, "非法存储路径");
        }
        return p;
    }

    public void delete(String storageKey) {
        if (storageKey == null) {
            return;
        }
        try {
            Files.deleteIfExists(resolve(storageKey));
        } catch (IOException ignored) {
            // best-effort：删除失败忽略
        }
    }
}
