#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include <drogon/drogon.h>

#include "dao/FileDao.h"
#include "service/StorageService.h"

namespace netdisk::service {

// 下载/预览服务：流式输出文件，支持 Range（206 断点下载 / 视频拖动）
class DownloadService {
public:
    DownloadService(std::shared_ptr<dao::FileDao> fileDao, std::shared_ptr<StorageService> storage)
        : fileDao_(std::move(fileDao)), storage_(std::move(storage)) {}

    // 成功时通过 callback 返回文件流；失败抛 BusinessError（由 controller 转 JSON）
    void stream(int64_t userId, int64_t id, const std::string& rangeHeader, bool isPreview,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    std::shared_ptr<dao::FileDao> fileDao_;
    std::shared_ptr<StorageService> storage_;
};

}  // namespace netdisk::service
