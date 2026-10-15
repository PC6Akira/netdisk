#pragma once

#include <cstdint>
#include <string>

namespace netdisk::model {

// 对应 file 表（文件与目录统一建模，isDir 区分），与 Java 版 entity.File 字段一致
struct File {
    int64_t id = 0;
    int64_t userId = 0;
    int64_t parentId = 0;
    std::string name;
    int isDir = 0;        // 1=目录 0=文件
    int64_t size = 0;
    std::string md5;
    std::string storageKey;
    int refCount = 1;
    int isDeleted = 0;
    std::string updatedAt;  // "YYYY-MM-DD HH:MM:SS"
};

}  // namespace netdisk::model
