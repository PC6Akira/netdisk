#include "service/DownloadService.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <system_error>

#include "common/BusinessException.h"

namespace netdisk::service {
namespace {

// 按扩展名映射 MIME（对齐 Java MediaTypeFactory；未知格式 → application/octet-stream）
std::string mimeTypeFor(const std::string& name) {
    auto dot = name.find_last_of('.');
    std::string ext = (dot == std::string::npos) ? "" : name.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "png") return "image/png";
    if (ext == "gif") return "image/gif";
    if (ext == "webp") return "image/webp";
    if (ext == "bmp") return "image/bmp";
    if (ext == "svg") return "image/svg+xml";
    if (ext == "mp4") return "video/mp4";
    if (ext == "webm") return "video/webm";
    if (ext == "mov") return "video/quicktime";
    if (ext == "avi") return "video/x-msvideo";
    if (ext == "mkv") return "video/x-matroska";
    if (ext == "pdf") return "application/pdf";
    if (ext == "txt" || ext == "log") return "text/plain";
    if (ext == "md") return "text/markdown";
    if (ext == "html" || ext == "htm") return "text/html";
    if (ext == "css") return "text/css";
    if (ext == "js") return "application/javascript";
    if (ext == "json") return "application/json";
    if (ext == "xml") return "application/xml";
    return "application/octet-stream";
}

// 解析 Range：bytes=start-end | bytes=start- | bytes=-suffix；非法返回 false
bool parseRange(const std::string& header, long long total, long long& start, long long& end) {
    if (header.rfind("bytes=", 0) != 0) return false;
    std::string spec = header.substr(6);
    auto comma = spec.find(',');
    if (comma != std::string::npos) spec = spec.substr(0, comma);
    auto dash = spec.find('-');
    if (dash == std::string::npos) return false;
    std::string startStr = spec.substr(0, dash);
    std::string endStr = spec.substr(dash + 1);
    try {
        if (startStr.empty()) {
            long long suffix = std::stoll(endStr);
            if (suffix <= 0) return false;
            start = std::max(0LL, total - suffix);
            end = total - 1;
        } else {
            start = std::stoll(startStr);
            end = endStr.empty() ? (total - 1) : std::min(std::stoll(endStr), total - 1);
        }
        if (start < 0) start = 0;
        if (start > end || start >= total) return false;
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace

void DownloadService::stream(int64_t userId, int64_t id, const std::string& rangeHeader,
                             bool isPreview,
                             std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    auto f = fileDao_->findById(id);
    if (!f || f->userId != userId) {
        throw BusinessError(404, "文件不存在");
    }
    if (f->isDir == 1) {
        throw BusinessError(400, "文件夹不支持下载/预览");
    }
    if (f->storageKey.empty()) {
        throw BusinessError(404, "文件内容不存在");
    }

    auto path = storage_->resolve(f->storageKey);
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || !std::filesystem::is_regular_file(path, ec)) {
        throw BusinessError(404, "文件内容不存在");
    }

    long long total = f->size > 0 ? f->size : 0;
    long long start = 0;
    long long end = total > 0 ? total - 1 : 0;
    bool partial = parseRange(rangeHeader, total, start, end);

    // 用 Drogon newFileResponse 从磁盘流式发送（异步文件流/sendfile，不整读内存）
    drogon::HttpResponsePtr resp;
    if (partial) {
        size_t length = static_cast<size_t>(end - start + 1);
        resp = drogon::HttpResponse::newFileResponse(
            path.string(), static_cast<size_t>(start), length,
            false /* 手动设 Content-Range */, "", drogon::CT_NONE, mimeTypeFor(f->name));
        resp->setStatusCode(drogon::k206PartialContent);
        resp->addHeader("Content-Range", "bytes " + std::to_string(start) + "-" +
                                             std::to_string(end) + "/" + std::to_string(total));
    } else {
        resp = drogon::HttpResponse::newFileResponse(
            path.string(), "", drogon::CT_NONE, mimeTypeFor(f->name));
    }
    resp->addHeader("Accept-Ranges", "bytes");
    std::string disposition = (isPreview ? "inline" : "attachment");
    disposition += "; filename=\"" + f->name + "\"";
    resp->addHeader("Content-Disposition", disposition);
    callback(resp);
}

}  // namespace netdisk::service
