#pragma once

#include <drogon/drogon.h>

#include "model/User.h"

namespace netdisk::vo {

// 用户信息视图对象（不含密码），字段与 Java 版 vo.UserVO 一致
inline Json::Value userVO(const model::User& u) {
    Json::Value v;
    v["id"] = static_cast<Json::Int64>(u.id);
    v["username"] = u.username;
    v["nickname"] = u.nickname;
    v["quotaLimit"] = static_cast<Json::Int64>(u.quotaLimit);
    v["quotaUsed"] = static_cast<Json::Int64>(u.quotaUsed);
    return v;
}

}  // namespace netdisk::vo
