#include "service/StorageService.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <system_error>

#include <openssl/evp.h>
#include <openssl/rand.h>

#include "common/BusinessException.h"

namespace netdisk::service {
namespace {

// 随机 UUID（v4），对应 Java 的 UUID.randomUUID()
std::string uuid() {
    unsigned char b[16];
    if (RAND_bytes(b, sizeof(b)) != 1) {
        auto ns = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (int i = 0; i < 16; ++i) b[i] = static_cast<unsigned char>(ns >> (i * 8));
    }
    b[6] = (b[6] & 0x0f) | 0x40;  // version 4
    b[8] = (b[8] & 0x3f) | 0x80;  // variant 10
    char buf[37];
    std::snprintf(buf, sizeof(buf),
                  "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                  b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
                  b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
    return std::string(buf);
}

// 当前日期 yyyyMMdd，对应 Java LocalDate.now().format("yyyyMMdd")
std::string today() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y%m%d", &tm);
    return std::string(buf);
}

// 小写十六进制（对齐 Java StorageService.hex）
std::string hex(const unsigned char* data, size_t len) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        out += digits[(data[i] >> 4) & 0xf];
        out += digits[data[i] & 0xf];
    }
    return out;
}

}  // namespace

StorageService::StorageService(const std::string& storageRoot) {
    root_ = std::filesystem::absolute(storageRoot).lexically_normal();
    std::error_code ec;
    std::filesystem::create_directories(root_, ec);
}

std::string StorageService::newKey(int64_t userId) {
    return std::to_string(userId) + "/" + today() + "/" + uuid();
}

std::string StorageService::store(int64_t userId, const char* data, size_t len) {
    std::string key = newKey(userId);
    auto target = resolve(key);
    std::error_code ec;
    std::filesystem::create_directories(target.parent_path(), ec);
    std::ofstream out(target, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw BusinessError(500, "文件保存失败");
    }
    out.write(data, static_cast<std::streamsize>(len));
    if (!out) {
        throw BusinessError(500, "文件保存失败");
    }
    return key;
}

std::filesystem::path StorageService::resolve(const std::string& storageKey) const {
    auto p = (root_ / storageKey).lexically_normal();
    std::string s = p.string();
    std::string r = root_.string();
    // 归一化后仍以根目录为前缀（防 ../ 穿越）
    if (s != r && s.rfind(r + "/", 0) != 0) {
        throw BusinessError(400, "非法存储路径");
    }
    return p;
}

void StorageService::deleteFile(const std::string& storageKey) const {
    if (storageKey.empty()) return;
    std::error_code ec;
    std::filesystem::remove(resolve(storageKey), ec);  // best-effort，失败忽略
}

std::filesystem::path StorageService::chunkDir(int64_t sessionId) const {
    return root_ / "chunks" / std::to_string(sessionId);
}

std::filesystem::path StorageService::chunkFile(int64_t sessionId, int index) const {
    return chunkDir(sessionId) / std::to_string(index);
}

void StorageService::storeChunk(int64_t sessionId, int index, const char* data, size_t len) {
    auto target = chunkFile(sessionId, index);
    std::error_code ec;
    std::filesystem::create_directories(target.parent_path(), ec);
    std::ofstream out(target, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw BusinessError(500, "分片保存失败");
    }
    out.write(data, static_cast<std::streamsize>(len));
    if (!out) {
        throw BusinessError(500, "分片保存失败");
    }
}

std::string StorageService::mergeChunks(int64_t sessionId, int totalChunks,
                                        const std::string& targetKey) {
    auto target = resolve(targetKey);
    std::error_code ec;
    std::filesystem::create_directories(target.parent_path(), ec);

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_md5(), nullptr);

    try {
        std::ofstream out(target, std::ios::binary | std::ios::trunc);
        if (!out) {
            throw BusinessError(500, "分片合并失败");
        }
        char buf[8192];
        for (int i = 0; i < totalChunks; ++i) {
            std::ifstream in(chunkFile(sessionId, i), std::ios::binary);
            if (!in) {
                throw BusinessError(400, "分片 " + std::to_string(i) + " 缺失");
            }
            while (in) {
                in.read(buf, sizeof(buf));
                std::streamsize n = in.gcount();
                if (n <= 0) break;
                out.write(buf, n);
                EVP_DigestUpdate(ctx, buf, static_cast<size_t>(n));
            }
        }
        out.flush();
    } catch (...) {
        EVP_MD_CTX_free(ctx);
        std::filesystem::remove(target, ec);  // 清理合并失败的目标文件
        throw;
    }

    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int mdLen = 0;
    EVP_DigestFinal_ex(ctx, md, &mdLen);
    EVP_MD_CTX_free(ctx);
    return hex(md, mdLen);
}

void StorageService::deleteChunks(int64_t sessionId) const {
    std::error_code ec;
    std::filesystem::remove_all(chunkDir(sessionId), ec);  // best-effort
}

}  // namespace netdisk::service
