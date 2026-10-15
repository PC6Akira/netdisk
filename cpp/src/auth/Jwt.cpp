#include "auth/Jwt.h"

#include <chrono>
#include <cstring>
#include <memory>
#include <sstream>

#include <drogon/drogon.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

namespace netdisk::auth {
namespace {

constexpr const char* kBase64Url =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

std::string b64urlEncode(const std::string& raw) {
    std::string out;
    const unsigned char* d = reinterpret_cast<const unsigned char*>(raw.data());
    size_t len = raw.size();
    size_t i = 0;
    while (i + 3 <= len) {
        unsigned int v = (d[i] << 16) | (d[i + 1] << 8) | d[i + 2];
        out += kBase64Url[(v >> 18) & 63];
        out += kBase64Url[(v >> 12) & 63];
        out += kBase64Url[(v >> 6) & 63];
        out += kBase64Url[v & 63];
        i += 3;
    }
    if (i + 1 == len) {
        unsigned int v = d[i] << 16;
        out += kBase64Url[(v >> 18) & 63];
        out += kBase64Url[(v >> 12) & 63];
    } else if (i + 2 == len) {
        unsigned int v = (d[i] << 16) | (d[i + 1] << 8);
        out += kBase64Url[(v >> 18) & 63];
        out += kBase64Url[(v >> 12) & 63];
        out += kBase64Url[(v >> 6) & 63];
    }
    return out;
}

std::string b64urlDecode(const std::string& in) {
    std::string out;
    unsigned int buf = 0;
    int bits = 0;
    for (char c : in) {
        int v = -1;
        if (c >= 'A' && c <= 'Z') v = c - 'A';
        else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
        else if (c >= '0' && c <= '9') v = c - '0' + 52;
        else if (c == '-') v = 62;
        else if (c == '_') v = 63;
        if (v < 0) continue;
        buf = (buf << 6) | static_cast<unsigned int>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out += static_cast<char>((buf >> bits) & 0xff);
        }
    }
    return out;
}

std::string hmacSha256(const std::string& key, const std::string& msg) {
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int mdLen = 0;
    HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(msg.data()), msg.size(), md, &mdLen);
    return std::string(reinterpret_cast<char*>(md), mdLen);
}

bool constantTimeEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i) diff |= a[i] ^ b[i];
    return diff == 0;
}

std::string toJson(const Json::Value& v) {
    Json::StreamWriterBuilder b;
    b["indentation"] = "";
    return Json::writeString(b, v);
}

bool parseJson(const std::string& s, Json::Value& out) {
    Json::CharReaderBuilder b;
    std::string errs;
    std::istringstream ss(s);
    return Json::parseFromStream(b, ss, &out, &errs);
}

long long nowEpochSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

}  // namespace

std::string generateToken(int64_t userId, const std::string& username,
                          const std::string& secret, int expireMinutes) {
    long long now = nowEpochSeconds();
    long long exp = now + static_cast<long long>(expireMinutes) * 60;

    Json::Value header;
    header["alg"] = "HS256";
    header["typ"] = "JWT";
    Json::Value payload;
    payload["sub"] = std::to_string(userId);
    payload["username"] = username;
    payload["iat"] = static_cast<Json::Int64>(now);
    payload["exp"] = static_cast<Json::Int64>(exp);

    std::string h = b64urlEncode(toJson(header));
    std::string p = b64urlEncode(toJson(payload));
    std::string sig = b64urlEncode(hmacSha256(secret, h + "." + p));
    return h + "." + p + "." + sig;
}

std::optional<JwtClaims> parseToken(const std::string& token, const std::string& secret) {
    size_t dot1 = token.find('.');
    size_t dot2 = token.find('.', dot1 + 1);
    if (dot1 == std::string::npos || dot2 == std::string::npos) return std::nullopt;

    std::string h = token.substr(0, dot1);
    std::string p = token.substr(dot1 + 1, dot2 - dot1 - 1);
    std::string sig = token.substr(dot2 + 1);

    // 校验签名（常数时间比较，防时序攻击）
    if (!constantTimeEquals(sig, b64urlEncode(hmacSha256(secret, h + "." + p)))) {
        return std::nullopt;
    }

    Json::Value payload;
    if (!parseJson(b64urlDecode(p), payload)) return std::nullopt;

    long long exp = payload["exp"].asInt64();
    if (exp <= nowEpochSeconds()) return std::nullopt;

    JwtClaims claims;
    claims.userId = std::stoll(payload["sub"].asString());
    claims.username = payload["username"].asString();
    return claims;
}

}  // namespace netdisk::auth
