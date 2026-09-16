package com.netdisk.service;

import com.netdisk.common.BusinessException;
import com.netdisk.entity.User;
import com.netdisk.mapper.UserMapper;
import org.springframework.stereotype.Service;

/**
 * 配额服务：乐观锁（version 重试）扣减用户已用空间。
 * 读 version → 条件更新（version 匹配且不超限）→ 冲突重试，兜底保证不超卖。
 */
@Service
public class QuotaService {

    private static final int MAX_RETRY = 3;

    private final UserMapper userMapper;

    public QuotaService(UserMapper userMapper) {
        this.userMapper = userMapper;
    }

    /**
     * 扣减配额。调用方需处于 READ_COMMITTED 事务中，重试的再读才能看到并发提交的新 version。
     */
    public void deductQuota(Long userId, long delta) {
        for (int i = 0; i < MAX_RETRY; i++) {
            User user = userMapper.findById(userId);
            if (delta > user.getQuotaLimit() - user.getQuotaUsed()) {
                throw new BusinessException(400, "空间不足");
            }
            int updated = userMapper.increaseQuotaUsedWithVersion(userId, delta, user.getVersion());
            if (updated == 1) {
                return;
            }
            // version 被并发事务改过，重试（再读会拿到新 version）
        }
        throw new BusinessException(409, "并发更新冲突，请重试");
    }
}
