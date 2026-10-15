#include "controller/FolderController.h"

#include <cstdint>
#include <string>

#include "common/HttpUtil.h"

namespace netdisk::controller {

void FolderController::create(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    int64_t userId = req->getAttributes()->get<int64_t>("userId");
    auto json = req->getJsonObject();
    std::string name = json ? (*json)["name"].asString() : "";
    int64_t parentId = json ? (*json)["parentId"].asInt64() : 0;
    respond([&] { return fileService_->createFolder(userId, name, parentId); },
            std::move(callback));
}

}  // namespace netdisk::controller
