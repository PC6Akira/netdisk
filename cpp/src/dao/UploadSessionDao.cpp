#include "dao/UploadSessionDao.h"

#include <cstring>

#include <mysql/mysql.h>

#include "db/Db.h"

namespace netdisk::dao {
namespace {

const char* kSelectCols =
    "SELECT id, user_id, file_name, parent_id, total_size, chunk_size, total_chunks, md5, status "
    "FROM `upload_session` ";

struct SessionRow {
    long long id = 0;
    long long userId = 0;
    char fileName[256] = {0};
    long long parentId = 0;
    long long totalSize = 0;
    int chunkSize = 0;
    int totalChunks = 0;
    char md5[64] = {0};
    signed char status = 0;
    unsigned long fileNameLen = 0, md5Len = 0;

    MYSQL_BIND* bind(MYSQL_BIND* b) {
        std::memset(b, 0, sizeof(MYSQL_BIND) * 9);
        b[0].buffer_type = MYSQL_TYPE_LONGLONG;
        b[0].buffer = &id;
        b[1].buffer_type = MYSQL_TYPE_LONGLONG;
        b[1].buffer = &userId;
        b[2].buffer_type = MYSQL_TYPE_STRING;
        b[2].buffer = fileName;
        b[2].buffer_length = sizeof(fileName);
        b[2].length = &fileNameLen;
        b[3].buffer_type = MYSQL_TYPE_LONGLONG;
        b[3].buffer = &parentId;
        b[4].buffer_type = MYSQL_TYPE_LONGLONG;
        b[4].buffer = &totalSize;
        b[5].buffer_type = MYSQL_TYPE_LONG;
        b[5].buffer = &chunkSize;
        b[6].buffer_type = MYSQL_TYPE_LONG;
        b[6].buffer = &totalChunks;
        b[7].buffer_type = MYSQL_TYPE_STRING;
        b[7].buffer = md5;
        b[7].buffer_length = sizeof(md5);
        b[7].length = &md5Len;
        b[8].buffer_type = MYSQL_TYPE_TINY;
        b[8].buffer = &status;
        return b;
    }

    model::UploadSession toSession() const {
        model::UploadSession s;
        s.id = id;
        s.userId = userId;
        s.fileName.assign(fileName, fileNameLen);
        s.parentId = parentId;
        s.totalSize = totalSize;
        s.chunkSize = chunkSize;
        s.totalChunks = totalChunks;
        s.md5.assign(md5, md5Len);
        s.status = status;
        return s;
    }
};

}  // namespace

int UploadSessionDao::insert(model::UploadSession& s) {
    db::ScopedConn conn(db::pool());
    MYSQL* c = conn.get();

    MYSQL_STMT* stmt = mysql_stmt_init(c);
    if (!stmt) return -1;

    const char* sql =
        "INSERT INTO `upload_session` (user_id, file_name, parent_id, total_size, chunk_size, "
        "total_chunks, md5, status) VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
    if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
        mysql_stmt_close(stmt);
        return -1;
    }

    signed char status = static_cast<signed char>(s.status);
    MYSQL_BIND p[8];
    std::memset(p, 0, sizeof(p));
    p[0].buffer_type = MYSQL_TYPE_LONGLONG;
    p[0].buffer = &s.userId;
    p[1].buffer_type = MYSQL_TYPE_STRING;
    p[1].buffer = const_cast<char*>(s.fileName.data());
    p[1].buffer_length = static_cast<unsigned long>(s.fileName.size());
    p[2].buffer_type = MYSQL_TYPE_LONGLONG;
    p[2].buffer = &s.parentId;
    p[3].buffer_type = MYSQL_TYPE_LONGLONG;
    p[3].buffer = &s.totalSize;
    p[4].buffer_type = MYSQL_TYPE_LONG;
    p[4].buffer = &s.chunkSize;
    p[5].buffer_type = MYSQL_TYPE_LONG;
    p[5].buffer = &s.totalChunks;
    p[6].buffer_type = MYSQL_TYPE_STRING;
    p[6].buffer = const_cast<char*>(s.md5.data());
    p[6].buffer_length = static_cast<unsigned long>(s.md5.size());
    p[7].buffer_type = MYSQL_TYPE_TINY;
    p[7].buffer = &status;

    if (mysql_stmt_bind_param(stmt, p) != 0 || mysql_stmt_execute(stmt) != 0) {
        unsigned int err = mysql_stmt_errno(stmt);
        mysql_stmt_close(stmt);
        return static_cast<int>(err);
    }
    s.id = static_cast<int64_t>(mysql_stmt_insert_id(stmt));
    mysql_stmt_close(stmt);
    return 0;
}

std::optional<model::UploadSession> UploadSessionDao::findById(int64_t id) {
    db::ScopedConn conn(db::pool());
    MYSQL* c = conn.get();

    MYSQL_STMT* stmt = mysql_stmt_init(c);
    if (!stmt) return std::nullopt;

    std::string sql = std::string(kSelectCols) + "WHERE id = ?";
    if (mysql_stmt_prepare(stmt, sql.c_str(), static_cast<unsigned long>(sql.size())) != 0) {
        mysql_stmt_close(stmt);
        return std::nullopt;
    }

    MYSQL_BIND param{};
    std::memset(&param, 0, sizeof(param));
    param.buffer_type = MYSQL_TYPE_LONGLONG;
    param.buffer = &id;
    if (mysql_stmt_bind_param(stmt, &param) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return std::nullopt;
    }

    SessionRow row;
    MYSQL_BIND res[9];
    mysql_stmt_bind_result(stmt, row.bind(res));

    std::optional<model::UploadSession> out;
    if (mysql_stmt_fetch(stmt) == 0) out = row.toSession();
    mysql_stmt_close(stmt);
    return out;
}

std::optional<model::UploadSession> UploadSessionDao::findByUserMd5ParentStatus(
    int64_t userId, const std::string& md5, int64_t parentId, int status) {
    db::ScopedConn conn(db::pool());
    MYSQL* c = conn.get();

    MYSQL_STMT* stmt = mysql_stmt_init(c);
    if (!stmt) return std::nullopt;

    std::string sql = std::string(kSelectCols) +
                      "WHERE user_id = ? AND md5 = ? AND parent_id = ? AND status = ? LIMIT 1";
    if (mysql_stmt_prepare(stmt, sql.c_str(), static_cast<unsigned long>(sql.size())) != 0) {
        mysql_stmt_close(stmt);
        return std::nullopt;
    }

    signed char st = static_cast<signed char>(status);
    MYSQL_BIND p[4];
    std::memset(p, 0, sizeof(p));
    p[0].buffer_type = MYSQL_TYPE_LONGLONG;
    p[0].buffer = &userId;
    p[1].buffer_type = MYSQL_TYPE_STRING;
    p[1].buffer = const_cast<char*>(md5.data());
    p[1].buffer_length = static_cast<unsigned long>(md5.size());
    p[2].buffer_type = MYSQL_TYPE_LONGLONG;
    p[2].buffer = &parentId;
    p[3].buffer_type = MYSQL_TYPE_TINY;
    p[3].buffer = &st;
    if (mysql_stmt_bind_param(stmt, p) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return std::nullopt;
    }

    SessionRow row;
    MYSQL_BIND res[9];
    mysql_stmt_bind_result(stmt, row.bind(res));

    std::optional<model::UploadSession> out;
    if (mysql_stmt_fetch(stmt) == 0) out = row.toSession();
    mysql_stmt_close(stmt);
    return out;
}

bool UploadSessionDao::updateStatus(int64_t id, int status) {
    db::ScopedConn conn(db::pool());
    MYSQL* c = conn.get();

    MYSQL_STMT* stmt = mysql_stmt_init(c);
    if (!stmt) return false;

    const char* sql = "UPDATE `upload_session` SET status = ? WHERE id = ?";
    if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    signed char st = static_cast<signed char>(status);
    MYSQL_BIND p[2];
    std::memset(p, 0, sizeof(p));
    p[0].buffer_type = MYSQL_TYPE_TINY;
    p[0].buffer = &st;
    p[1].buffer_type = MYSQL_TYPE_LONGLONG;
    p[1].buffer = &id;

    bool ok = mysql_stmt_bind_param(stmt, p) == 0 && mysql_stmt_execute(stmt) == 0;
    mysql_stmt_close(stmt);
    return ok;
}

}  // namespace netdisk::dao
