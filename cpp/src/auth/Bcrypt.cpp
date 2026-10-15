#include "auth/Bcrypt.h"

#include <crypt.h>
#include <cstring>

#include <openssl/rand.h>

namespace netdisk::auth {
namespace {

constexpr const char* kPrefix = "$2b$";  // 现代 bcrypt 前缀（兼容 $2a$ 校验）
constexpr unsigned long kCost = 10;

}  // namespace

std::string hashPassword(const std::string& password) {
    unsigned char rnd[16];
    if (RAND_bytes(rnd, sizeof(rnd)) != 1) return "";

    char salt[CRYPT_GENSALT_OUTPUT_SIZE] = {0};
    char* s = crypt_gensalt_rn(kPrefix, kCost, reinterpret_cast<const char*>(rnd),
                               static_cast<int>(sizeof(rnd)), salt, sizeof(salt));
    if (!s) return "";

    struct crypt_data data{};
    char* h = crypt_rn(password.c_str(), salt, &data, sizeof(data));
    return h ? std::string(h) : std::string();
}

bool verifyPassword(const std::string& password, const std::string& hash) {
    struct crypt_data data{};
    char* r = crypt_rn(password.c_str(), hash.c_str(), &data, sizeof(data));
    return r && std::strcmp(r, hash.c_str()) == 0;
}

}  // namespace netdisk::auth
