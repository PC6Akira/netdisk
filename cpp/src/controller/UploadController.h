#pragma once

#include <drogon/HttpController.h>

#include <functional>
#include <memory>

#include "service/ChunkUploadService.h"

namespace netdisk::controller {

// 分片上传接口：创建会话、上传分片、查询已传分片、合并完成（受 JWT 保护）
class UploadController : public drogon::HttpController<UploadController, false> {
public:
    explicit UploadController(std::shared_ptr<service::ChunkUploadService> svc)
        : svc_(std::move(svc)) {}

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(UploadController::init, "/api/upload/init", drogon::Post, "JwtFilter");
    ADD_METHOD_TO(UploadController::chunk, "/api/upload/chunk", drogon::Post, "JwtFilter");
    ADD_METHOD_TO(UploadController::status, "/api/upload/status", drogon::Get, "JwtFilter");
    ADD_METHOD_TO(UploadController::complete, "/api/upload/complete", drogon::Post, "JwtFilter");
    METHOD_LIST_END

    void init(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void chunk(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void status(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void complete(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    std::shared_ptr<service::ChunkUploadService> svc_;
};

}  // namespace netdisk::controller
