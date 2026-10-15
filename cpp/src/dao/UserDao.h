#pragma once

#include <optional>
#include <string>

#include "model/User.h"

namespace netdisk::dao {

// user 表数据访问（预编译语句，等价 MyBatis #{} 防注入）
class UserDao {
public:
    std::optional<model::User> findByUsername(const std::string& username);
    std::optional<model::User> findById(int64_t id);
    // 插入：成功回填 user.id 并返回 0；失败返回 MySQL errno（1062=用户名重复）
    int insert(model::User& user);
    // 乐观锁扣减配额：version 匹配且不超限才更新，返回受影响行数（0/1）
    int increaseQuotaUsedWithVersion(int64_t id, int64_t delta, int32_t version);
};

}  // namespace netdisk::dao
