#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <drogon/drogon.h>

#include "dao/FileChunkDao.h"
#include "dao/FileDao.h"
#include "dao/UploadSessionDao.h"
#include "dao/UserDao.h"
#include "service/QuotaService.h"
#include "service/StorageService.h"

namespace netdisk::service {

// 分片上传服务：创建会话、上传分片、断点续传、合并完成（等价 Java ChunkUploadService）
class ChunkUploadService {
public:
    ChunkUploadService(std::shared_ptr<dao::UploadSessionDao> sessionDao,
                       std::shared_ptr<dao::FileChunkDao> chunkDao,
                       std::shared_ptr<dao::FileDao> fileDao,
                       std::shared_ptr<dao::UserDao> userDao,
                       std::shared_ptr<StorageService> storage,
                       std::shared_ptr<QuotaService> quotaService)
        : sessionDao_(std::move(sessionDao)),
          chunkDao_(std::move(chunkDao)),
          fileDao_(std::move(fileDao)),
          userDao_(std::move(userDao)),
          storage_(std::move(storage)),
          quotaService_(std::move(quotaService)) {}

    Json::Value init(int64_t userId, const std::string& name, int64_t parentId, int64_t size,
                     const std::string& md5);
    void uploadChunk(int64_t userId, int64_t sessionId, int chunkIndex, const char* data,
                     size_t len);
    Json::Value status(int64_t userId, int64_t sessionId);
    Json::Value complete(int64_t userId, int64_t sessionId);

private:
    static constexpr int CHUNK_SIZE = 5 * 1024 * 1024;  // 单片 5MB

    model::UploadSession requireSession(int64_t userId, int64_t sessionId);
    void requireOwnedDir(int64_t userId, int64_t id);
    std::string normalizeMd5(const std::string& md5);
    Json::Value buildInitVO(int64_t sessionId, int chunkSize, int totalChunks,
                            const std::vector<int>& uploaded);

    std::shared_ptr<dao::UploadSessionDao> sessionDao_;
    std::shared_ptr<dao::FileChunkDao> chunkDao_;
    std::shared_ptr<dao::FileDao> fileDao_;
    std::shared_ptr<dao::UserDao> userDao_;
    std::shared_ptr<StorageService> storage_;
    std::shared_ptr<QuotaService> quotaService_;
};

}  // namespace netdisk::service
