#pragma once

#include <cstdint>
#include <string>

namespace netdisk::model {

// 对应 user 表（与 Java 版 entity.User 字段一致）
struct User {
    int64_t id = 0;
    std::string username;
    std::string password;  // BCrypt 哈希
    std::string nickname;
    int64_t quotaLimit = 0;
    int64_t quotaUsed = 0;
    int32_t version = 0;
};

}  // namespace netdisk::model
