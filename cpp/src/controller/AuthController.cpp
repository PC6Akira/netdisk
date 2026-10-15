#include "controller/AuthController.h"

#include "common/HttpUtil.h"

namespace netdisk::controller {

void AuthController::registerUser(const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    auto json = req->getJsonObject();
    std::string username = json ? (*json)["username"].asString() : "";
    std::string password = json ? (*json)["password"].asString() : "";
    std::string nickname = json ? (*json)["nickname"].asString() : "";
    respond([&] { return userService_->registerUser(username, password, nickname); },
            std::move(callback));
}

void AuthController::login(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    auto json = req->getJsonObject();
    std::string username = json ? (*json)["username"].asString() : "";
    std::string password = json ? (*json)["password"].asString() : "";
    respond([&] { return userService_->login(username, password); }, std::move(callback));
}

}  // namespace netdisk::controller
