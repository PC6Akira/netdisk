#include "controller/FileController.h"

#include <cstdint>
#include <string>

#include <drogon/MultiPart.h>

#include "common/HttpUtil.h"

namespace netdisk::controller {

void FileController::list(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    int64_t userId = req->getAttributes()->get<int64_t>("userId");
    std::string pidStr = req->getParameter("parentId");
    int64_t parentId = pidStr.empty() ? 0 : std::stoll(pidStr);
    respond([&] { return fileService_->list(userId, parentId); }, std::move(callback));
}

void FileController::upload(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    int64_t userId = req->getAttributes()->get<int64_t>("userId");

    drogon::MultiPartParser parser;
    parser.parse(req);
    const auto& files = parser.getFiles();
    int64_t parentId = parser.getParameter<int64_t>("parentId");
    std::string md5 = parser.getParameter<std::string>("md5");
    const char* data = files.empty() ? nullptr : files[0].fileData();
    size_t len = files.empty() ? 0 : files[0].fileLength();
    std::string name = files.empty() ? "" : files[0].getFileName();

    respond(
        [&] { return uploadService_->upload(userId, parentId, name, data, len, md5); },
        std::move(callback));
}

void FileController::instant(const drogon::HttpRequestPtr& req,
                             std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    int64_t userId = req->getAttributes()->get<int64_t>("userId");
    auto json = req->getJsonObject();
    std::string name = json ? (*json)["name"].asString() : "";
    int64_t parentId = json ? (*json)["parentId"].asInt64() : 0;
    int64_t size = json ? (*json)["size"].asInt64() : 0;
    std::string md5 = json ? (*json)["md5"].asString() : "";
    respond(
        [&] { return uploadService_->instant(userId, name, parentId, size, md5); },
        std::move(callback));
}

void FileController::download(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    int64_t userId = req->getAttributes()->get<int64_t>("userId");
    const auto& pathParams = req->getRoutingParameters();
    int64_t id = pathParams.empty() ? 0 : std::stoll(pathParams[0]);
    std::string range = req->getHeader("Range");
    try {
        downloadService_->stream(userId, id, range, false, std::move(callback));
    } catch (const netdisk::BusinessError& e) {
        callback(drogon::HttpResponse::newHttpJsonResponse(
            netdisk::ApiResponse::error(e.code(), e.what())));
    } catch (const std::exception& e) {
        callback(drogon::HttpResponse::newHttpJsonResponse(
            netdisk::ApiResponse::error(500, "服务器内部错误")));
    }
}

void FileController::preview(const drogon::HttpRequestPtr& req,
                             std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    int64_t userId = req->getAttributes()->get<int64_t>("userId");
    const auto& pathParams = req->getRoutingParameters();
    int64_t id = pathParams.empty() ? 0 : std::stoll(pathParams[0]);
    std::string range = req->getHeader("Range");
    try {
        downloadService_->stream(userId, id, range, true, std::move(callback));
    } catch (const netdisk::BusinessError& e) {
        callback(drogon::HttpResponse::newHttpJsonResponse(
            netdisk::ApiResponse::error(e.code(), e.what())));
    } catch (const std::exception& e) {
        callback(drogon::HttpResponse::newHttpJsonResponse(
            netdisk::ApiResponse::error(500, "服务器内部错误")));
    }
}

void FileController::rename(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    int64_t userId = req->getAttributes()->get<int64_t>("userId");
    const auto& pathParams = req->getRoutingParameters();  // 路径参数按位置存
    int64_t id = pathParams.empty() ? 0 : std::stoll(pathParams[0]);
    auto json = req->getJsonObject();
    std::string name = json ? (*json)["name"].asString() : "";
    respond([&] { return fileService_->rename(userId, id, name); }, std::move(callback));
}

void FileController::remove(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    int64_t userId = req->getAttributes()->get<int64_t>("userId");
    const auto& pathParams = req->getRoutingParameters();  // 路径参数按位置存
    int64_t id = pathParams.empty() ? 0 : std::stoll(pathParams[0]);
    respond(
        [&]() -> Json::Value {
            fileService_->remove(userId, id);
            return Json::Value();
        },
        std::move(callback));
}

}  // namespace netdisk::controller
