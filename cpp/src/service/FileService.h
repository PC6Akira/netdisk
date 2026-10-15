#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <drogon/drogon.h>

#include "dao/FileDao.h"
#include "model/File.h"
#include "service/StorageService.h"

namespace netdisk::service {

// 文件/目录服务：列表、新建文件夹、重命名、删除（等价 Java FileService）
class FileService {
public:
    FileService(std::shared_ptr<dao::FileDao> fileDao, std::shared_ptr<StorageService> storage)
        : fileDao_(std::move(fileDao)), storage_(std::move(storage)) {}

    Json::Value list(int64_t userId, int64_t parentId);  // 返回 FileVO 数组
    Json::Value createFolder(int64_t userId, const std::string& name, int64_t parentId);
    Json::Value rename(int64_t userId, int64_t id, const std::string& name);
    void remove(int64_t userId, int64_t id);

private:
    model::File requireOwned(int64_t userId, int64_t id);
    void requireOwnedDir(int64_t userId, int64_t id);
    void collectAll(int64_t userId, int64_t parentId, std::vector<model::File>& acc);
    static int refCountOf(const std::vector<model::File>& files, const std::string& storageKey);

    std::shared_ptr<dao::FileDao> fileDao_;
    std::shared_ptr<StorageService> storage_;
};

}  // namespace netdisk::service
