#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace netdisk::auth {

struct JwtClaims {
    int64_t userId = 0;
    std::string username;
};

// HS256 签发：sub=userId，claim username，exp = now + expireMinutes
std::string generateToken(int64_t userId, const std::string& username,
                          const std::string& secret, int expireMinutes);

// 解析并校验签名 + 过期；失败返回 std::nullopt
std::optional<JwtClaims> parseToken(const std::string& token, const std::string& secret);

}  // namespace netdisk::auth
