#include "service/UserService.h"

#include "auth/Bcrypt.h"
#include "auth/Jwt.h"
#include "common/BusinessException.h"
#include "vo/UserVO.h"

namespace netdisk::service {

Json::Value UserService::buildLoginResponse(const model::User& user) {
    std::string token =
        auth::generateToken(user.id, user.username, cfg_.jwtSecret, cfg_.jwtExpireMinutes);
    Json::Value resp;
    resp["token"] = token;
    resp["user"] = vo::userVO(user);
    return resp;
}

Json::Value UserService::registerUser(const std::string& username, const std::string& password,
                                      const std::string& nickname) {
    if (username.empty() || password.empty()) {
        throw BusinessError(400, "用户名和密码不能为空");
    }
    if (userDao_->findByUsername(username)) {
        throw BusinessError(400, "用户名已存在");
    }

    model::User user;
    user.username = username;
    user.password = auth::hashPassword(password);
    user.nickname = nickname;
    user.quotaLimit = cfg_.defaultQuota;
    user.quotaUsed = 0;
    user.version = 0;

    int rc = userDao_->insert(user);
    if (rc == 1062) {  // ER_DUP_ENTRY：并发下唯一约束兜底
        throw BusinessError(400, "用户名已存在");
    }
    if (rc != 0) {
        throw BusinessError(500, "注册失败，请稍后再试");
    }
    return buildLoginResponse(user);
}

Json::Value UserService::login(const std::string& username, const std::string& password) {
    auto user = userDao_->findByUsername(username);
    if (!user || !auth::verifyPassword(password, user->password)) {
        throw BusinessError(400, "用户名或密码错误");
    }
    return buildLoginResponse(*user);
}

Json::Value UserService::getById(int64_t id) {
    auto user = userDao_->findById(id);
    if (!user) {
        throw BusinessError(404, "用户不存在");
    }
    return vo::userVO(*user);
}

}  // namespace netdisk::service
