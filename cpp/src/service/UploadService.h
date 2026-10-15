#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include <drogon/drogon.h>

#include "dao/FileDao.h"
#include "dao/UserDao.h"
#include "service/QuotaService.h"
#include "service/StorageService.h"

namespace netdisk::service {

// 上传服务：普通上传（流式落盘 + 配额校验 + 原子扣减 + 写元数据）+ 秒传（阶段 5 补去重）
class UploadService {
public:
    UploadService(std::shared_ptr<dao::FileDao> fileDao, std::shared_ptr<dao::UserDao> userDao,
                  std::shared_ptr<StorageService> storage,
                  std::shared_ptr<QuotaService> quotaService)
        : fileDao_(std::move(fileDao)),
          userDao_(std::move(userDao)),
          storage_(std::move(storage)),
          quotaService_(std::move(quotaService)) {}

    Json::Value upload(int64_t userId, int64_t parentId, const std::string& fileName,
                       const char* data, size_t len, const std::string& md5Param);

    // 秒传：校验 + 配额预检，返回未命中（阶段 5 实现 MD5 去重 + 引用计数）
    Json::Value instant(int64_t userId, const std::string& name, int64_t parentId,
                        int64_t size, const std::string& md5);

private:
    void requireOwnedDir(int64_t userId, int64_t id);
    std::string normalizeMd5(const std::string& md5);

    std::shared_ptr<dao::FileDao> fileDao_;
    std::shared_ptr<dao::UserDao> userDao_;
    std::shared_ptr<StorageService> storage_;
    std::shared_ptr<QuotaService> quotaService_;
};

}  // namespace netdisk::service
