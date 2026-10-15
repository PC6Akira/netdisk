#pragma once

#include <string>

namespace netdisk::auth {

// BCrypt 加盐哈希（libxcrypt 的 crypt_gensalt_rn / crypt_rn，$2b$，cost=10）
std::string hashPassword(const std::string& password);

// 校验明文密码与已存 BCrypt 哈希是否匹配
bool verifyPassword(const std::string& password, const std::string& hash);

}  // namespace netdisk::auth
