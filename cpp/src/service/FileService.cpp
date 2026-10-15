#include "service/FileService.h"

#include <map>

#include "common/BusinessException.h"
#include "vo/FileVO.h"

namespace netdisk::service {
namespace {

bool isBlank(const std::string& s) {
    return s.find_first_not_of(" \t\r\n") == std::string::npos;
}

}  // namespace

Json::Value FileService::list(int64_t userId, int64_t parentId) {
    if (parentId != 0) {
        requireOwnedDir(userId, parentId);
    }
    auto files = fileDao_->findByUserIdAndParentId(userId, parentId);
    Json::Value arr(Json::arrayValue);
    for (const auto& f : files) {
        arr.append(vo::fileVO(f));
    }
    return arr;
}

Json::Value FileService::createFolder(int64_t userId, const std::string& name, int64_t parentId) {
    if (isBlank(name)) {
        throw BusinessError(400, "文件夹名不能为空");
    }
    if (parentId != 0) {
        requireOwnedDir(userId, parentId);
    }
    if (fileDao_->findByNameAndParent(userId, parentId, name)) {
        throw BusinessError(400, "同名文件或文件夹已存在");
    }

    model::File f;
    f.userId = userId;
    f.parentId = parentId;
    f.name = name;
    f.isDir = 1;
    f.size = 0;
    f.refCount = 1;
    f.isDeleted = 0;
    int rc = fileDao_->insert(f);
    if (rc == 1062) {  // ER_DUP_ENTRY：并发下唯一约束兜底
        throw BusinessError(400, "同名文件或文件夹已存在");
    }
    if (rc != 0) {
        throw BusinessError(500, "创建失败");
    }
    return vo::fileVO(f);
}

Json::Value FileService::rename(int64_t userId, int64_t id, const std::string& name) {
    if (isBlank(name)) {
        throw BusinessError(400, "名称不能为空");
    }
    model::File f = requireOwned(userId, id);
    if (fileDao_->findByNameAndParent(userId, f.parentId, name)) {
        throw BusinessError(400, "同名文件或文件夹已存在");
    }
    fileDao_->updateName(id, name);
    f.name = name;
    return vo::fileVO(f);
}

void FileService::remove(int64_t userId, int64_t id) {
    model::File root = requireOwned(userId, id);

    std::vector<model::File> all;
    collectAll(userId, id, all);
    all.push_back(root);

    // 非目录文件按 storage_key 分组，维护引用计数：归零才删物理文件
    std::map<std::string, int> delCountByKey;
    for (const auto& f : all) {
        if (f.isDir == 0 && !f.storageKey.empty()) {
            delCountByKey[f.storageKey]++;
        }
    }
    for (const auto& [key, nDel] : delCountByKey) {
        int before = refCountOf(all, key);
        int after = before - nDel;
        if (after <= 0) {
            storage_->deleteFile(key);  // 无其他引用，删物理文件
        } else {
            fileDao_->decrementRefCountByStorageKey(key, nDel);
        }
    }

    std::vector<int64_t> ids;
    ids.reserve(all.size());
    for (const auto& f : all) {
        ids.push_back(f.id);
    }
    fileDao_->deleteByIds(ids);  // 单条批量 DELETE，原子
}

model::File FileService::requireOwned(int64_t userId, int64_t id) {
    auto f = fileDao_->findById(id);
    if (!f || f->userId != userId) {
        throw BusinessError(404, "文件或文件夹不存在");
    }
    return *f;
}

void FileService::requireOwnedDir(int64_t userId, int64_t id) {
    auto f = fileDao_->findById(id);
    if (!f || f->userId != userId || f->isDir != 1) {
        throw BusinessError(400, "目录不存在");
    }
}

void FileService::collectAll(int64_t userId, int64_t parentId, std::vector<model::File>& acc) {
    auto children = fileDao_->findByUserIdAndParentId(userId, parentId);
    for (const auto& child : children) {
        acc.push_back(child);
        if (child.isDir == 1) {
            collectAll(userId, child.id, acc);
        }
    }
}

int FileService::refCountOf(const std::vector<model::File>& files, const std::string& storageKey) {
    for (const auto& f : files) {
        if (f.storageKey == storageKey) {
            return f.refCount;
        }
    }
    return 0;
}

}  // namespace netdisk::service
