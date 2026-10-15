#include "dao/UserDao.h"

#include <cstring>

#include <mysql/mysql.h>

#include "db/Db.h"

namespace netdisk::dao {
namespace {

const char* kSelectCols =
    "SELECT id, username, password, nickname, quota_limit, quota_used, version FROM `user` ";

// 结果行绑定：把一行读成 User
struct UserRow {
    long long id = 0;
    char username[256] = {0};
    char password[256] = {0};
    char nickname[256] = {0};
    long long quotaLimit = 0;
    long long quotaUsed = 0;
    int version = 0;
    unsigned long usernameLen = 0, passwordLen = 0, nicknameLen = 0;

    MYSQL_BIND* bind(MYSQL_BIND* b) {
        std::memset(b, 0, sizeof(MYSQL_BIND) * 7);
        b[0].buffer_type = MYSQL_TYPE_LONGLONG;
        b[0].buffer = &id;
        b[1].buffer_type = MYSQL_TYPE_STRING;
        b[1].buffer = username;
        b[1].buffer_length = sizeof(username);
        b[1].length = &usernameLen;
        b[2].buffer_type = MYSQL_TYPE_STRING;
        b[2].buffer = password;
        b[2].buffer_length = sizeof(password);
        b[2].length = &passwordLen;
        b[3].buffer_type = MYSQL_TYPE_STRING;
        b[3].buffer = nickname;
        b[3].buffer_length = sizeof(nickname);
        b[3].length = &nicknameLen;
        b[4].buffer_type = MYSQL_TYPE_LONGLONG;
        b[4].buffer = &quotaLimit;
        b[5].buffer_type = MYSQL_TYPE_LONGLONG;
        b[5].buffer = &quotaUsed;
        b[6].buffer_type = MYSQL_TYPE_LONG;
        b[6].buffer = &version;
        return b;
    }

    model::User toUser() const {
        model::User u;
        u.id = id;
        u.username.assign(username, usernameLen);
        u.password.assign(password, passwordLen);
        u.nickname.assign(nickname, nicknameLen);  // NULL 时 length=0 → 空串
        u.quotaLimit = quotaLimit;
        u.quotaUsed = quotaUsed;
        u.version = version;
        return u;
    }
};

}  // namespace

std::optional<model::User> UserDao::findByUsername(const std::string& username) {
    db::ScopedConn conn(db::pool());
    MYSQL* c = conn.get();

    MYSQL_STMT* stmt = mysql_stmt_init(c);
    if (!stmt) return std::nullopt;

    std::string sql = std::string(kSelectCols) + "WHERE username = ?";
    if (mysql_stmt_prepare(stmt, sql.c_str(), static_cast<unsigned long>(sql.size())) != 0) {
        mysql_stmt_close(stmt);
        return std::nullopt;
    }

    MYSQL_BIND param{};
    std::memset(&param, 0, sizeof(param));
    param.buffer_type = MYSQL_TYPE_STRING;
    param.buffer = const_cast<char*>(username.data());
    param.buffer_length = static_cast<unsigned long>(username.size());
    if (mysql_stmt_bind_param(stmt, &param) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return std::nullopt;
    }

    UserRow row;
    MYSQL_BIND res[7];
    mysql_stmt_bind_result(stmt, row.bind(res));

    std::optional<model::User> out;
    if (mysql_stmt_fetch(stmt) == 0) out = row.toUser();
    mysql_stmt_close(stmt);
    return out;
}

std::optional<model::User> UserDao::findById(int64_t id) {
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

    UserRow row;
    MYSQL_BIND res[7];
    mysql_stmt_bind_result(stmt, row.bind(res));

    std::optional<model::User> out;
    if (mysql_stmt_fetch(stmt) == 0) out = row.toUser();
    mysql_stmt_close(stmt);
    return out;
}

int UserDao::insert(model::User& user) {
    db::ScopedConn conn(db::pool());
    MYSQL* c = conn.get();

    MYSQL_STMT* stmt = mysql_stmt_init(c);
    if (!stmt) return -1;

    const char* sql =
        "INSERT INTO `user` (username, password, nickname, quota_limit, quota_used, version) "
        "VALUES (?, ?, ?, ?, ?, ?)";
    if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
        mysql_stmt_close(stmt);
        return -1;
    }

    MYSQL_BIND p[6];
    std::memset(p, 0, sizeof(p));
    p[0].buffer_type = MYSQL_TYPE_STRING;
    p[0].buffer = const_cast<char*>(user.username.data());
    p[0].buffer_length = static_cast<unsigned long>(user.username.size());
    p[1].buffer_type = MYSQL_TYPE_STRING;
    p[1].buffer = const_cast<char*>(user.password.data());
    p[1].buffer_length = static_cast<unsigned long>(user.password.size());
    p[2].buffer_type = MYSQL_TYPE_STRING;
    p[2].buffer = const_cast<char*>(user.nickname.data());
    p[2].buffer_length = static_cast<unsigned long>(user.nickname.size());
    p[3].buffer_type = MYSQL_TYPE_LONGLONG;
    p[3].buffer = &user.quotaLimit;
    p[4].buffer_type = MYSQL_TYPE_LONGLONG;
    p[4].buffer = &user.quotaUsed;
    p[5].buffer_type = MYSQL_TYPE_LONG;
    p[5].buffer = &user.version;

    if (mysql_stmt_bind_param(stmt, p) != 0 || mysql_stmt_execute(stmt) != 0) {
        unsigned int err = mysql_stmt_errno(stmt);
        mysql_stmt_close(stmt);
        return static_cast<int>(err);
    }

    user.id = static_cast<int64_t>(mysql_stmt_insert_id(stmt));
    mysql_stmt_close(stmt);
    return 0;
}

int UserDao::increaseQuotaUsedWithVersion(int64_t id, int64_t delta, int32_t version) {
    db::ScopedConn conn(db::pool());
    MYSQL* c = conn.get();

    MYSQL_STMT* stmt = mysql_stmt_init(c);
    if (!stmt) return 0;

    const char* sql =
        "UPDATE `user` SET quota_used = quota_used + ?, version = version + 1 "
        "WHERE id = ? AND version = ? AND quota_used + ? <= quota_limit";
    if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    MYSQL_BIND p[4];
    std::memset(p, 0, sizeof(p));
    p[0].buffer_type = MYSQL_TYPE_LONGLONG;
    p[0].buffer = &delta;
    p[1].buffer_type = MYSQL_TYPE_LONGLONG;
    p[1].buffer = &id;
    p[2].buffer_type = MYSQL_TYPE_LONG;
    p[2].buffer = &version;
    p[3].buffer_type = MYSQL_TYPE_LONGLONG;
    p[3].buffer = &delta;

    if (mysql_stmt_bind_param(stmt, p) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }
    int affected = static_cast<int>(mysql_stmt_affected_rows(stmt));
    mysql_stmt_close(stmt);
    return affected;
}

}  // namespace netdisk::dao
