#pragma once

#include <cstdint>
#include <string>

namespace netdisk::model {

// 对应 upload_session 表，与 Java 版 entity.UploadSession 字段一致
struct UploadSession {
    int64_t id = 0;
    int64_t userId = 0;
    std::string fileName;
    int64_t parentId = 0;
    int64_t totalSize = 0;
    int chunkSize = 0;
    int totalChunks = 0;
    std::string md5;
    int status = 0;  // 0=上传中 1=合并中 2=完成
};

}  // namespace netdisk::model
