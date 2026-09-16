package com.netdisk.service;

import com.netdisk.common.BusinessException;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;
import org.springframework.web.multipart.MultipartFile;

import java.io.BufferedOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
import java.nio.file.StandardOpenOption;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.time.LocalDate;
import java.time.format.DateTimeFormatter;
import java.util.Comparator;
import java.util.UUID;

/**
 * 文件存储服务：负责文件落盘、路径解析、删除，以及分片上传的分片存储与合并。
 * storage_key 是相对存储根目录的路径，不含原始文件名（防路径穿越与重名冲突）。
 */
@Service
public class StorageService {

    private static final DateTimeFormatter DATE_FMT = DateTimeFormatter.ofPattern("yyyyMMdd");

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
     * 生成一个新的 storage_key。
     */
    public String newKey(Long userId) {
        return userId + "/" + LocalDate.now().format(DATE_FMT) + "/" + UUID.randomUUID();
    }

    /**
     * 流式落盘，返回 storage_key。
     */
    public String store(Long userId, MultipartFile file) {
        String key = newKey(userId);
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

    /**
     * 保存单个分片到临时目录 data/chunks/{sessionId}/{index}。
     */
    public void storeChunk(Long sessionId, int index, MultipartFile chunk) {
        Path target = chunkFile(sessionId, index);
        try {
            Files.createDirectories(target.getParent());
            try (InputStream in = chunk.getInputStream()) {
                Files.copy(in, target, StandardCopyOption.REPLACE_EXISTING);
            }
        } catch (IOException e) {
            throw new BusinessException(500, "分片保存失败");
        }
    }

    /**
     * 按序流式合并分片到 targetKey，边写边计算 MD5，返回合并后文件的 MD5（小写十六进制）。
     */
    public String mergeChunks(Long sessionId, int totalChunks, String targetKey) {
        Path target = resolve(targetKey);
        try {
            Files.createDirectories(target.getParent());
            MessageDigest digest = MessageDigest.getInstance("MD5");
            byte[] buf = new byte[8192];
            try (OutputStream out = new BufferedOutputStream(Files.newOutputStream(target,
                    StandardOpenOption.CREATE, StandardOpenOption.TRUNCATE_EXISTING))) {
                for (int i = 0; i < totalChunks; i++) {
                    Path part = chunkFile(sessionId, i);
                    if (!Files.exists(part)) {
                        throw new BusinessException(400, "分片 " + i + " 缺失");
                    }
                    try (InputStream in = Files.newInputStream(part)) {
                        int n;
                        while ((n = in.read(buf)) != -1) {
                            out.write(buf, 0, n);
                            digest.update(buf, 0, n);
                        }
                    }
                }
            }
            return hex(digest.digest());
        } catch (NoSuchAlgorithmException e) {
            throw new IllegalStateException("当前环境不支持 MD5", e);
        } catch (IOException e) {
            try {
                Files.deleteIfExists(target);
            } catch (IOException ignored) {
                // best-effort
            }
            throw new BusinessException(500, "分片合并失败");
        }
    }

    /**
     * 清理某个会话的所有临时分片。
     */
    public void deleteChunks(Long sessionId) {
        Path dir = chunkDir(sessionId);
        try (var walk = Files.walk(dir)) {
            walk.sorted(Comparator.reverseOrder()).forEach(p -> {
                try {
                    Files.deleteIfExists(p);
                } catch (IOException ignored) {
                    // best-effort
                }
            });
        } catch (IOException ignored) {
            // best-effort
        }
    }

    private Path chunkDir(Long sessionId) {
        return root.resolve("chunks").resolve(String.valueOf(sessionId));
    }

    private Path chunkFile(Long sessionId, int index) {
        return chunkDir(sessionId).resolve(String.valueOf(index));
    }

    private String hex(byte[] bytes) {
        StringBuilder sb = new StringBuilder(bytes.length * 2);
        for (byte b : bytes) {
            sb.append(Character.forDigit((b >> 4) & 0xF, 16));
            sb.append(Character.forDigit(b & 0xF, 16));
        }
        return sb.toString();
    }
}
