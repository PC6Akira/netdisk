#pragma once

#include <cstdint>
#include <memory>

#include "dao/UserDao.h"

namespace netdisk::service {

// 配额服务：乐观锁（version 重试）扣减用户已用空间，兜底不超卖（等价 Java QuotaService）
class QuotaService {
public:
    explicit QuotaService(std::shared_ptr<dao::UserDao> userDao) : userDao_(std::move(userDao)) {}

    void deductQuota(int64_t userId, int64_t delta);

private:
    std::shared_ptr<dao::UserDao> userDao_;
};

}  // namespace netdisk::service
