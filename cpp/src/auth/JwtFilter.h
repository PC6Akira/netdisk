#pragma once

#include <drogon/HttpFilter.h>

#include "config/Config.h"

namespace netdisk::auth {

// JWT 鉴权过滤器：校验 Authorization: Bearer 或 ?token=，把 userId/username 放入请求属性
class JwtFilter : public drogon::HttpFilter<JwtFilter, false> {
public:
    explicit JwtFilter(const Config& cfg) : cfg_(cfg) {}

    // 用短名注册，控制器里以 "JwtFilter" 引用（默认是全限定名 netdisk::auth::JwtFilter）
    const std::string& className() const override {
        static const std::string name = "JwtFilter";
        return name;
    }

    void doFilter(const drogon::HttpRequestPtr& req,
                  drogon::FilterCallback&& fcb,
                  drogon::FilterChainCallback&& fccb) override;

private:
    Config cfg_;
};

}  // namespace netdisk::auth
