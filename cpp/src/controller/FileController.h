#pragma once

#include <drogon/HttpController.h>

#include <functional>
#include <memory>

#include "service/DownloadService.h"
#include "service/FileService.h"
#include "service/UploadService.h"

namespace netdisk::controller {

// 文件接口：列表、上传、秒传、下载、预览、重命名、删除（受 JWT 保护，等价 Java FileController）
class FileController : public drogon::HttpController<FileController, false> {
public:
    FileController(std::shared_ptr<service::FileService> fileService,
                   std::shared_ptr<service::UploadService> uploadService,
                   std::shared_ptr<service::DownloadService> downloadService)
        : fileService_(std::move(fileService)),
          uploadService_(std::move(uploadService)),
          downloadService_(std::move(downloadService)) {}

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(FileController::list, "/api/files", drogon::Get, "JwtFilter");
    ADD_METHOD_TO(FileController::upload, "/api/files/upload", drogon::Post, "JwtFilter");
    ADD_METHOD_TO(FileController::instant, "/api/files/instant", drogon::Post, "JwtFilter");
    ADD_METHOD_TO(FileController::download, "/api/files/{id}/download", drogon::Get, "JwtFilter");
    ADD_METHOD_TO(FileController::preview, "/api/files/{id}/preview", drogon::Get, "JwtFilter");
    ADD_METHOD_TO(FileController::rename, "/api/files/{id}/rename", drogon::Post, "JwtFilter");
    ADD_METHOD_TO(FileController::remove, "/api/files/{id}", drogon::Delete, "JwtFilter");
    METHOD_LIST_END

    void list(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void upload(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void instant(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void download(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void preview(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void rename(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void remove(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    std::shared_ptr<service::FileService> fileService_;
    std::shared_ptr<service::UploadService> uploadService_;
    std::shared_ptr<service::DownloadService> downloadService_;
};

}  // namespace netdisk::controller
