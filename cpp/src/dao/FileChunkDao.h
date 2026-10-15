#pragma once

#include <cstdint>
#include <vector>

namespace netdisk::dao {

// file_chunk 表数据访问（预编译语句）
class FileChunkDao {
public:
    void upsert(int64_t sessionId, int chunkIndex);  // 幂等：唯一约束 + ON DUPLICATE KEY
    std::vector<int> findIndexesBySessionId(int64_t sessionId);
    int countBySessionId(int64_t sessionId);
    void deleteBySessionId(int64_t sessionId);
};

}  // namespace netdisk::dao
