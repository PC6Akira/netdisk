#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "model/UploadSession.h"

namespace netdisk::dao {

// upload_session 表数据访问（预编译语句）
class UploadSessionDao {
public:
    int insert(model::UploadSession& s);  // 成功回填 s.id 并返回 0，失败返回 errno
    std::optional<model::UploadSession> findById(int64_t id);
    // 断点续传：同 (userId, md5, parentId, status) 复用未完成会话
    std::optional<model::UploadSession> findByUserMd5ParentStatus(int64_t userId,
                                                                  const std::string& md5,
                                                                  int64_t parentId, int status);
    bool updateStatus(int64_t id, int status);
};

}  // namespace netdisk::dao
