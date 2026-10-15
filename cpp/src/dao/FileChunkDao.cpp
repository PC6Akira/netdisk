#include "dao/FileChunkDao.h"

#include <cstring>

#include <mysql/mysql.h>

#include "db/Db.h"

namespace netdisk::dao {

void FileChunkDao::upsert(int64_t sessionId, int chunkIndex) {
    db::ScopedConn conn(db::pool());
    MYSQL* c = conn.get();

    MYSQL_STMT* stmt = mysql_stmt_init(c);
    if (!stmt) return;

    const char* sql =
        "INSERT INTO `file_chunk` (session_id, chunk_index, status) VALUES (?, ?, 1) "
        "ON DUPLICATE KEY UPDATE status = 1";
    if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
        mysql_stmt_close(stmt);
        return;
    }

    MYSQL_BIND p[2];
    std::memset(p, 0, sizeof(p));
    p[0].buffer_type = MYSQL_TYPE_LONGLONG;
    p[0].buffer = &sessionId;
    p[1].buffer_type = MYSQL_TYPE_LONG;
    p[1].buffer = &chunkIndex;

    if (mysql_stmt_bind_param(stmt, p) == 0) mysql_stmt_execute(stmt);
    mysql_stmt_close(stmt);
}

std::vector<int> FileChunkDao::findIndexesBySessionId(int64_t sessionId) {
    db::ScopedConn conn(db::pool());
    MYSQL* c = conn.get();

    MYSQL_STMT* stmt = mysql_stmt_init(c);
    if (!stmt) return {};

    const char* sql = "SELECT chunk_index FROM `file_chunk` WHERE session_id = ? ORDER BY chunk_index";
    if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
        mysql_stmt_close(stmt);
        return {};
    }

    MYSQL_BIND param{};
    std::memset(&param, 0, sizeof(param));
    param.buffer_type = MYSQL_TYPE_LONGLONG;
    param.buffer = &sessionId;
    if (mysql_stmt_bind_param(stmt, &param) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return {};
    }

    int idx = 0;
    MYSQL_BIND res{};
    std::memset(&res, 0, sizeof(res));
    res.buffer_type = MYSQL_TYPE_LONG;
    res.buffer = &idx;
    mysql_stmt_bind_result(stmt, &res);

    std::vector<int> out;
    while (mysql_stmt_fetch(stmt) == 0) {
        out.push_back(idx);
    }
    mysql_stmt_close(stmt);
    return out;
}

int FileChunkDao::countBySessionId(int64_t sessionId) {
    db::ScopedConn conn(db::pool());
    MYSQL* c = conn.get();

    MYSQL_STMT* stmt = mysql_stmt_init(c);
    if (!stmt) return 0;

    const char* sql = "SELECT COUNT(*) FROM `file_chunk` WHERE session_id = ?";
    if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    MYSQL_BIND param{};
    std::memset(&param, 0, sizeof(param));
    param.buffer_type = MYSQL_TYPE_LONGLONG;
    param.buffer = &sessionId;
    if (mysql_stmt_bind_param(stmt, &param) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    long long cnt = 0;
    MYSQL_BIND res{};
    std::memset(&res, 0, sizeof(res));
    res.buffer_type = MYSQL_TYPE_LONGLONG;
    res.buffer = &cnt;
    mysql_stmt_bind_result(stmt, &res);

    int out = 0;
    if (mysql_stmt_fetch(stmt) == 0) out = static_cast<int>(cnt);
    mysql_stmt_close(stmt);
    return out;
}

void FileChunkDao::deleteBySessionId(int64_t sessionId) {
    db::ScopedConn conn(db::pool());
    MYSQL* c = conn.get();

    MYSQL_STMT* stmt = mysql_stmt_init(c);
    if (!stmt) return;

    const char* sql = "DELETE FROM `file_chunk` WHERE session_id = ?";
    if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
        mysql_stmt_close(stmt);
        return;
    }

    MYSQL_BIND p[1];
    std::memset(p, 0, sizeof(p));
    p[0].buffer_type = MYSQL_TYPE_LONGLONG;
    p[0].buffer = &sessionId;

    if (mysql_stmt_bind_param(stmt, p) == 0) mysql_stmt_execute(stmt);
    mysql_stmt_close(stmt);
}

}  // namespace netdisk::dao
