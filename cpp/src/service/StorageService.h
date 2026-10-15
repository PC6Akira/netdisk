#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace netdisk::service {

// 文件存储服务：负责路径解析、流式落盘与物理文件删除
class StorageService {
public:
    explicit StorageService(const std::string& storageRoot);

    // 生成 storage_key：{userId}/{yyyyMMdd}/{uuid}（不含原始文件名，防穿越/重名）
    std::string newKey(int64_t userId);

    // 落盘，返回 storage_key
    std::string store(int64_t userId, const char* data, size_t len);

    // storage_key → 磁盘绝对路径，含路径穿越防护（归一化后仍在根目录内）
    std::filesystem::path resolve(const std::string& storageKey) const;

    // 删除物理文件（best-effort，等价 Java StorageService.delete）
    void deleteFile(const std::string& storageKey) const;

    // 分片上传：分片落盘 / 合并（边写边算 MD5）/ 清理临时分片
    void storeChunk(int64_t sessionId, int index, const char* data, size_t len);
    std::string mergeChunks(int64_t sessionId, int totalChunks, const std::string& targetKey);
    void deleteChunks(int64_t sessionId) const;

private:
    std::filesystem::path chunkDir(int64_t sessionId) const;
    std::filesystem::path chunkFile(int64_t sessionId, int index) const;

    std::filesystem::path root_;
};

}  // namespace netdisk::service
