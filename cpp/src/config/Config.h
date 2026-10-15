#pragma once

#include <cstdlib>
#include <string>

namespace netdisk {

// 应用配置：从环境变量读取，带默认值（默认指向本机 MySQL，可用环境变量覆盖）。
struct Config {
    // 数据库
    std::string mysqlHost = env("MYSQL_HOST", "127.0.0.1");
    int mysqlPort = std::stoi(env("MYSQL_PORT", "3306"));
    std::string mysqlUser = env("MYSQL_USER", "netdisk");
    std::string mysqlPassword = env("MYSQL_PASSWORD", "netdisk");
    std::string mysqlDb = env("MYSQL_DB", "netdisk");

    // 文件落盘根目录（相对运行目录）
    std::string storageRoot = env("NETDISK_STORAGE_ROOT", "./data");

    // 单用户默认配额（字节）：10GB
    long long defaultQuota = std::stoll(env("NETDISK_DEFAULT_QUOTA", "10737418240"));

    // JWT（HS256）
    std::string jwtSecret = env(
        "NETDISK_JWT_SECRET",
        "netdisk-dev-jwt-secret-key-please-change-in-production-0123456789");
    int jwtExpireMinutes = std::stoi(env("NETDISK_JWT_EXPIRE_MINUTES", "120"));

private:
    static std::string env(const char* key, const char* fallback) {
        const char* v = std::getenv(key);
        return v ? std::string(v) : std::string(fallback);
    }
};

}  // namespace netdisk
