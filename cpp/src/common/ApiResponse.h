#pragma once

#include <drogon/drogon.h>
#include <string>

namespace netdisk {

// 统一响应体：code=0 成功；401 未登录/过期；非 0 业务错误。
// 字段与 Java 版 com.netdisk.common.ApiResponse 保持一致（code/message/data）。
class ApiResponse {
public:
    static Json::Value ok(const Json::Value& data = Json::Value()) {
        Json::Value r;
        r["code"] = 0;
        r["message"] = "ok";
        r["data"] = data;
        return r;
    }

    static Json::Value error(int code, const std::string& message) {
        Json::Value r;
        r["code"] = code;
        r["message"] = message;
        r["data"] = Json::Value();
        return r;
    }
};

}  // namespace netdisk
