#pragma once

#include <drogon/drogon.h>

#include "model/File.h"

namespace netdisk::vo {

// 文件/目录视图对象，字段与 Java 版 vo.FileVO 一致（不含 userId/storageKey/refCount）
inline Json::Value fileVO(const model::File& f) {
    Json::Value v;
    v["id"] = static_cast<Json::Int64>(f.id);
    v["parentId"] = static_cast<Json::Int64>(f.parentId);
    v["name"] = f.name;
    v["isDir"] = f.isDir;
    v["size"] = static_cast<Json::Int64>(f.size);
    v["md5"] = f.md5.empty() ? Json::Value() : Json::Value(f.md5);  // 空 → null
    // "YYYY-MM-DD HH:MM:SS" → "YYYY-MM-DDTHH:MM:SS"（对齐 Java LocalDateTime 序列化）
    std::string ts = f.updatedAt;
    if (ts.empty()) {
        v["updatedAt"] = Json::Value();  // 新建/重命名尚未回读落库时间 → null，对齐 Java
    } else {
        auto pos = ts.find(' ');
        if (pos != std::string::npos) ts[pos] = 'T';
        v["updatedAt"] = ts;
    }
    return v;
}

}  // namespace netdisk::vo
