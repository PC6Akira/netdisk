#pragma once

#include <drogon/drogon.h>

#include <functional>

#include "common/ApiResponse.h"
#include "common/BusinessException.h"

namespace netdisk {

// 统一把业务结果/异常转成 ApiResponse（等价 Java GlobalExceptionHandler）。
// 用法：respond([&]{ return service->xxx(...); }, std::move(callback));
// 注意：Java 版所有业务错误都返回 HTTP 200 + body{code:...}，这里保持一致。
template <typename F>
void respond(F&& fn, std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    try {
        callback(drogon::HttpResponse::newHttpJsonResponse(ApiResponse::ok(fn())));
    } catch (const BusinessError& e) {
        callback(drogon::HttpResponse::newHttpJsonResponse(ApiResponse::error(e.code(), e.what())));
    } catch (const std::exception& e) {
        LOG_ERROR << "未处理异常: " << e.what();
        callback(drogon::HttpResponse::newHttpJsonResponse(ApiResponse::error(500, "服务器内部错误")));
    }
}

}  // namespace netdisk
