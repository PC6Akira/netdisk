#include "auth/JwtFilter.h"

#include <cstdint>

#include "auth/Jwt.h"
#include "common/ApiResponse.h"

namespace netdisk::auth {

void JwtFilter::doFilter(const drogon::HttpRequestPtr& req,
                         drogon::FilterCallback&& fcb,
                         drogon::FilterChainCallback&& fccb) {
    std::string token;
    const std::string& auth = req->getHeader("Authorization");
    if (auth.rfind("Bearer ", 0) == 0) {
        token = auth.substr(7);
    } else {
        token = req->getParameter("token");  // 兼容 <a>/<img> 无法自定义请求头
    }

    if (token.empty()) {
        fcb(drogon::HttpResponse::newHttpJsonResponse(
            ApiResponse::error(401, "未登录或登录已过期")));
        return;
    }

    auto claims = parseToken(token, cfg_.jwtSecret);
    if (!claims) {
        fcb(drogon::HttpResponse::newHttpJsonResponse(
            ApiResponse::error(401, "未登录或登录已过期")));
        return;
    }

    req->getAttributes()->insert("userId", static_cast<int64_t>(claims->userId));
    req->getAttributes()->insert("username", claims->username);
    fccb();
}

}  // namespace netdisk::auth
