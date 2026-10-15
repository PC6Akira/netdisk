#pragma once

#include <drogon/HttpController.h>

#include <functional>
#include <memory>

#include "service/FileService.h"

namespace netdisk::controller {

// 文件夹接口：新建文件夹（受 JWT 保护，等价 Java FolderController）
class FolderController : public drogon::HttpController<FolderController, false> {
public:
    explicit FolderController(std::shared_ptr<service::FileService> fileService)
        : fileService_(std::move(fileService)) {}

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(FolderController::create, "/api/folders", drogon::Post, "JwtFilter");
    METHOD_LIST_END

    void create(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    std::shared_ptr<service::FileService> fileService_;
};

}  // namespace netdisk::controller
