#pragma once

#include <drogon/HttpController.h>

#include <functional>
#include <memory>

#include "service/UserService.h"

namespace netdisk::controller {

// 用户接口：查询当前登录用户（受 JWT 保护，等价 Java UserController）
class UserController : public drogon::HttpController<UserController, false> {
public:
    explicit UserController(std::shared_ptr<service::UserService> userService)
        : userService_(std::move(userService)) {}

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(UserController::me, "/api/user/me", drogon::Get, "JwtFilter");
    METHOD_LIST_END

    void me(const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    std::shared_ptr<service::UserService> userService_;
};

}  // namespace netdisk::controller
