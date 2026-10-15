#include "service/UploadService.h"

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

std::string UploadService::normalizeMd5(const std::string& md5) {
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

void UploadService::requireOwnedDir(int64_t userId, int64_t id) {
    auto f = fileDao_->findById(id);
    if (!f || f->userId != userId || f->isDir != 1) {
        throw BusinessError(400, "目录不存在");
    }
}

Json::Value UploadService::upload(int64_t userId, int64_t parentId, const std::string& fileName,
                                  const char* data, size_t len, const std::string& md5Param) {
    if (data == nullptr || len == 0) {
        throw BusinessError(400, "文件不能为空");
    }
    if (parentId != 0) {
        requireOwnedDir(userId, parentId);
    }
    if (isBlank(fileName)) {
        throw BusinessError(400, "文件名不能为空");
    }
    if (fileDao_->findByNameAndParent(userId, parentId, fileName)) {
        throw BusinessError(400, "同名文件或文件夹已存在");
    }
    auto user = userDao_->findById(userId);
    if (!user) {
        throw BusinessError(404, "用户不存在");
    }
    if (static_cast<int64_t>(len) > user->quotaLimit - user->quotaUsed) {
        throw BusinessError(400, "空间不足");
    }
    std::string md5 = normalizeMd5(md5Param);
    if (!isBlank(md5Param) && md5.empty()) {
        throw BusinessError(400, "文件 MD5 无效");
    }

    std::string storageKey;
    try {
        storageKey = storage_->store(userId, data, len);
        quotaService_->deductQuota(userId, static_cast<int64_t>(len));

        model::File f;
        f.userId = userId;
        f.parentId = parentId;
        f.name = fileName;
        f.isDir = 0;
        f.size = static_cast<int64_t>(len);
        f.md5 = md5;
        f.storageKey = storageKey;
        f.refCount = 1;
        f.isDeleted = 0;
        int rc = fileDao_->insert(f);
        if (rc == 1062) {
            throw BusinessError(400, "同名文件或文件夹已存在");
        }
        if (rc != 0) {
            throw BusinessError(500, "上传失败");
        }
        return vo::fileVO(f);
    } catch (...) {
        if (!storageKey.empty()) {
            storage_->deleteFile(storageKey);  // 回滚已落盘文件
        }
        throw;
    }
}

Json::Value UploadService::instant(int64_t userId, const std::string& name, int64_t parentId,
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

    // 悲观锁：FOR UPDATE 锁最早插入的 canonical 行，串行化同一内容的并发秒传（ref_count 读改写）
    db::DbTransaction txn(db::pool());
    auto source = fileDao_->findByMd5ForUpdate(normalized);
    if (!source) {
        txn.commit();  // 只读事务，直接提交（无锁可持）
        Json::Value v;
        v["instant"] = false;
        v["file"] = Json::Value();
        return v;
    }

    // 命中：扣配额（乐观锁）+ 复用物理文件 + 引用计数 +1（全局去重，跨用户复用）
    quotaService_->deductQuota(userId, size);
    model::File f;
    f.userId = userId;
    f.parentId = parentId;
    f.name = name;
    f.isDir = 0;
    f.size = size;
    f.md5 = normalized;
    f.storageKey = source->storageKey;
    f.refCount = source->refCount;
    f.isDeleted = 0;
    int rc = fileDao_->insert(f);
    if (rc != 0) {
        throw BusinessError(500, "秒传失败");
    }
    fileDao_->incrementRefCountByStorageKey(source->storageKey);
    txn.commit();

    Json::Value v;
    v["instant"] = true;
    v["file"] = vo::fileVO(f);
    return v;
}

}  // namespace netdisk::service
