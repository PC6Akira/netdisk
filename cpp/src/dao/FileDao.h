#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "model/File.h"

namespace netdisk::dao {

// file 表数据访问（预编译语句）
class FileDao {
public:
    std::vector<model::File> findByUserIdAndParentId(int64_t userId, int64_t parentId);
    std::optional<model::File> findById(int64_t id);
    std::optional<model::File> findByNameAndParent(int64_t userId, int64_t parentId,
                                                   const std::string& name);
    int insert(model::File& f);  // 成功回填 f.id 并返回 0，失败返回 errno
    bool updateName(int64_t id, const std::string& name);
    void decrementRefCountByStorageKey(const std::string& storageKey, int delta);
    void incrementRefCountByStorageKey(const std::string& storageKey);
    // 秒传查重：锁最早插入的 canonical 行（FOR UPDATE，需在事务内调用）
    std::optional<model::File> findByMd5ForUpdate(const std::string& md5);
    void deleteByIds(const std::vector<int64_t>& ids);
};

}  // namespace netdisk::dao
