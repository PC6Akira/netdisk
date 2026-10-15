#include "controller/UserController.h"

#include "common/HttpUtil.h"

namespace netdisk::controller {

void UserController::me(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    int64_t userId = req->getAttributes()->get<int64_t>("userId");
    respond([&] { return userService_->getById(userId); }, std::move(callback));
}

}  // namespace netdisk::controller
