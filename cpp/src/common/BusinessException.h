#pragma once

#include <stdexcept>
#include <string>

namespace netdisk {

// 业务异常：携带业务错误码，由 controller 统一转成 ApiResponse（等价 Java BusinessException）
class BusinessError : public std::runtime_error {
public:
    BusinessError(int code, const std::string& message) : std::runtime_error(message), code_(code) {}
    int code() const { return code_; }

private:
    int code_;
};

}  // namespace netdisk
