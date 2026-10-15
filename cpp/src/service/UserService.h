#pragma once

#include <memory>

#include <drogon/drogon.h>

#include "config/Config.h"
#include "dao/UserDao.h"

namespace netdisk::service {

// 用户服务：注册、登录、查询（等价 Java UserService）
class UserService {
public:
    UserService(std::shared_ptr<dao::UserDao> userDao, const Config& cfg)
        : userDao_(std::move(userDao)), cfg_(cfg) {}

    // 均返回给 ApiResponse 的 data；业务失败抛 BusinessError
    Json::Value registerUser(const std::string& username, const std::string& password,
                             const std::string& nickname);
    Json::Value login(const std::string& username, const std::string& password);
    Json::Value getById(int64_t id);

private:
    Json::Value buildLoginResponse(const model::User& user);

    std::shared_ptr<dao::UserDao> userDao_;
    Config cfg_;
};

}  // namespace netdisk::service
