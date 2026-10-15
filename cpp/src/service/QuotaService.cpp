#include "service/QuotaService.h"

#include "common/BusinessException.h"

namespace netdisk::service {

void QuotaService::deductQuota(int64_t userId, int64_t delta) {
    for (int i = 0; i < 3; ++i) {
        auto user = userDao_->findById(userId);
        if (!user) {
            throw BusinessError(404, "用户不存在");
        }
        if (delta > user->quotaLimit - user->quotaUsed) {
            throw BusinessError(400, "空间不足");
        }
        if (userDao_->increaseQuotaUsedWithVersion(userId, delta, user->version) == 1) {
            return;
        }
        // version 被并发事务改过，重试（每条语句各自自动提交，再读即最新已提交数据）
    }
    throw BusinessError(409, "并发更新冲突，请重试");
}

}  // namespace netdisk::service
