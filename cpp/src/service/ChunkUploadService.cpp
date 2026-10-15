#include "service/ChunkUploadService.h"

#include <algorithm>
#include <cctype>

#include "common/BusinessException.h"
#include "db/Db.h"
#include "vo/FileVO.h"

namespace netdisk::service {
namespace {

bool isBlank(const std::string& s) {
    return s.find_first_not_of(" \t\r\n") == std::string::npos;
}

}  // namespace

std::string ChunkUploadService::normalizeMd5(const std::string& md5) {
    auto b = md5.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    auto e = md5.find_last_not_of(" \t\r\n");
    std::string m = md5.substr(b, e - b + 1);
    std::transform(m.begin(), m.end(), m.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (m.size() != 32) return "";
    for (char c : m) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) return "";
    }
    return m;
}

void ChunkUploadService::requireOwnedDir(int64_t userId, int64_t id) {
    auto f = fileDao_->findById(id);
    if (!f || f->userId != userId || f->isDir != 1) {
        throw BusinessError(400, "目录不存在");
    }
}

model::UploadSession ChunkUploadService::requireSession(int64_t userId, int64_t sessionId) {
    auto s = sessionDao_->findById(sessionId);
    if (!s || s->userId != userId) {
        throw BusinessError(404, "会话不存在");
    }
    return *s;
}

Json::Value ChunkUploadService::buildInitVO(int64_t sessionId, int chunkSize, int totalChunks,
                                            const std::vector<int>& uploaded) {
    Json::Value v;
    v["sessionId"] = static_cast<Json::Int64>(sessionId);
    v["chunkSize"] = chunkSize;
    v["totalChunks"] = totalChunks;
    Json::Value arr(Json::arrayValue);
    for (int i : uploaded) {
        arr.append(i);
    }
    v["uploadedChunks"] = arr;
    return v;
}

Json::Value ChunkUploadService::init(int64_t userId, const std::string& name, int64_t parentId,
                                     int64_t size, const std::string& md5) {
    if (isBlank(name)) {
        throw BusinessError(400, "文件名不能为空");
    }
    std::string normalized = normalizeMd5(md5);
    if (normalized.empty()) {
        throw BusinessError(400, "文件 MD5 无效");
    }
    if (size <= 0) {
        throw BusinessError(400, "文件大小无效");
    }
    if (parentId != 0) {
        requireOwnedDir(userId, parentId);
    }
    if (fileDao_->findByNameAndParent(userId, parentId, name)) {
        throw BusinessError(400, "同名文件或文件夹已存在");
    }
    auto user = userDao_->findById(userId);
    if (!user) {
        throw BusinessError(404, "用户不存在");
    }
    if (size > user->quotaLimit - user->quotaUsed) {
        throw BusinessError(400, "空间不足");
    }

    int totalChunks = static_cast<int>((size + CHUNK_SIZE - 1) / CHUNK_SIZE);

    // 断点续传：复用同 (userId, md5, parentId) 下未完成会话，返回已传分片
    auto session = sessionDao_->findByUserMd5ParentStatus(userId, normalized, parentId, 0);
    if (!session) {
        model::UploadSession s;
        s.userId = userId;
        s.fileName = name;
        s.parentId = parentId;
        s.totalSize = size;
        s.chunkSize = CHUNK_SIZE;
        s.totalChunks = totalChunks;
        s.md5 = normalized;
        s.status = 0;
        int rc = sessionDao_->insert(s);
        if (rc != 0) {
            throw BusinessError(500, "创建会话失败");
        }
        session = s;
    }

    auto uploaded = chunkDao_->findIndexesBySessionId(session->id);
    return buildInitVO(session->id, CHUNK_SIZE, session->totalChunks, uploaded);
}

void ChunkUploadService::uploadChunk(int64_t userId, int64_t sessionId, int chunkIndex,
                                     const char* data, size_t len) {
    auto session = requireSession(userId, sessionId);
    if (session.status != 0) {
        throw BusinessError(400, "会话已结束，无法继续上传");
    }
    if (data == nullptr || len == 0) {
        throw BusinessError(400, "分片不能为空");
    }
    if (chunkIndex < 0 || chunkIndex >= session.totalChunks) {
        throw BusinessError(400, "分片序号非法");
    }
    storage_->storeChunk(sessionId, chunkIndex, data, len);
    chunkDao_->upsert(sessionId, chunkIndex);  // 幂等
}

Json::Value ChunkUploadService::status(int64_t userId, int64_t sessionId) {
    auto session = requireSession(userId, sessionId);
    auto uploaded = chunkDao_->findIndexesBySessionId(sessionId);
    return buildInitVO(sessionId, session.chunkSize, session.totalChunks, uploaded);
}

Json::Value ChunkUploadService::complete(int64_t userId, int64_t sessionId) {
    auto session = requireSession(userId, sessionId);
    if (session.status != 0) {
        throw BusinessError(400, "会话已结束");
    }
    int uploaded = chunkDao_->countBySessionId(sessionId);
    if (uploaded != session.totalChunks) {
        throw BusinessError(400,
                            "分片不完整：" + std::to_string(uploaded) + "/" +
                                std::to_string(session.totalChunks));
    }

    // 流式合并 + 服务端重算 MD5 做完整性校验
    std::string targetKey = storage_->newKey(userId);
    std::string actualMd5 = storage_->mergeChunks(sessionId, session.totalChunks, targetKey);
    if (!session.md5.empty() && session.md5 != actualMd5) {
        storage_->deleteFile(targetKey);
        throw BusinessError(400, "文件校验失败（MD5 不一致）");
    }

    try {
        // 事务：扣配额 + 写元数据 + 更新会话状态（原子）
        db::DbTransaction txn(db::pool());
        quotaService_->deductQuota(userId, session.totalSize);
        model::File f;
        f.userId = userId;
        f.parentId = session.parentId;
        f.name = session.fileName;
        f.isDir = 0;
        f.size = session.totalSize;
        f.md5 = session.md5;
        f.storageKey = targetKey;
        f.refCount = 1;
        f.isDeleted = 0;
        int rc = fileDao_->insert(f);
        if (rc == 1062) {
            throw BusinessError(400, "同名文件或文件夹已存在");
        }
        if (rc != 0) {
            throw BusinessError(500, "上传失败");
        }
        sessionDao_->updateStatus(sessionId, 2);
        txn.commit();

        // 清理分片临时文件与分片记录
        storage_->deleteChunks(sessionId);
        chunkDao_->deleteBySessionId(sessionId);
        return vo::fileVO(f);
    } catch (...) {
        storage_->deleteFile(targetKey);  // 回滚已合并落盘的文件
        throw;
    }
}

}  // namespace netdisk::service
