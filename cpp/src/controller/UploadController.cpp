#include "controller/UploadController.h"

#include <cstdint>
#include <string>

#include <drogon/MultiPart.h>

#include "common/HttpUtil.h"

namespace netdisk::controller {

void UploadController::init(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    int64_t userId = req->getAttributes()->get<int64_t>("userId");
    auto json = req->getJsonObject();
    std::string name = json ? (*json)["name"].asString() : "";
    int64_t parentId = json ? (*json)["parentId"].asInt64() : 0;
    int64_t size = json ? (*json)["size"].asInt64() : 0;
    std::string md5 = json ? (*json)["md5"].asString() : "";
    respond([&] { return svc_->init(userId, name, parentId, size, md5); }, std::move(callback));
}

void UploadController::chunk(const drogon::HttpRequestPtr& req,
                             std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    int64_t userId = req->getAttributes()->get<int64_t>("userId");

    drogon::MultiPartParser parser;
    parser.parse(req);
    int64_t sessionId = parser.getParameter<int64_t>("sessionId");
    int chunkIndex = parser.getParameter<int>("chunkIndex");
    const auto& files = parser.getFiles();
    const char* data = files.empty() ? nullptr : files[0].fileData();
    size_t len = files.empty() ? 0 : files[0].fileLength();

    respond(
        [&]() -> Json::Value {
            svc_->uploadChunk(userId, sessionId, chunkIndex, data, len);
            return Json::Value();
        },
        std::move(callback));
}

void UploadController::status(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    int64_t userId = req->getAttributes()->get<int64_t>("userId");
    std::string sid = req->getParameter("sessionId");
    int64_t sessionId = sid.empty() ? 0 : std::stoll(sid);
    respond([&] { return svc_->status(userId, sessionId); }, std::move(callback));
}

void UploadController::complete(const drogon::HttpRequestPtr& req,
                                std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    int64_t userId = req->getAttributes()->get<int64_t>("userId");
    auto json = req->getJsonObject();
    int64_t sessionId = json ? (*json)["sessionId"].asInt64() : 0;
    respond([&] { return svc_->complete(userId, sessionId); }, std::move(callback));
}

}  // namespace netdisk::controller
