#pragma once

#include <drogon/HttpController.h>

#include <functional>
#include <memory>

#include "service/UserService.h"

namespace netdisk::controller {

// 认证接口：注册、登录（等价 Java AuthController）
class AuthController : public drogon::HttpController<AuthController, false> {
public:
    explicit AuthController(std::shared_ptr<service::UserService> userService)
        : userService_(std::move(userService)) {}

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AuthController::registerUser, "/api/auth/register", drogon::Post);
    ADD_METHOD_TO(AuthController::login, "/api/auth/login", drogon::Post);
    METHOD_LIST_END

    void registerUser(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void login(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    std::shared_ptr<service::UserService> userService_;
};

}  // namespace netdisk::controller
