#include "dao/FileDao.h"

#include <cstring>

#include <mysql/mysql.h>

#include "db/Db.h"

namespace netdisk::dao {
namespace {

const char* kSelectCols =
    "SELECT id, user_id, parent_id, name, is_dir, size, md5, storage_key, "
    "ref_count, is_deleted, updated_at FROM `file` ";

// 结果行绑定：把一行读成 File
struct FileRow {
    long long id = 0;
    long long userId = 0;
    long long parentId = 0;
    char name[256] = {0};
    signed char isDir = 0;
    long long size = 0;
    char md5[64] = {0};
    char storageKey[512] = {0};
    int refCount = 0;
    signed char isDeleted = 0;
    char updatedAt[32] = {0};
    unsigned long nameLen = 0, md5Len = 0, storageKeyLen = 0, updatedAtLen = 0;

    MYSQL_BIND* bind(MYSQL_BIND* b) {
        std::memset(b, 0, sizeof(MYSQL_BIND) * 11);
        b[0].buffer_type = MYSQL_TYPE_LONGLONG;
        b[0].buffer = &id;
        b[1].buffer_type = MYSQL_TYPE_LONGLONG;
        b[1].buffer = &userId;
        b[2].buffer_type = MYSQL_TYPE_LONGLONG;
        b[2].buffer = &parentId;
        b[3].buffer_type = MYSQL_TYPE_STRING;
        b[3].buffer = name;
        b[3].buffer_length = sizeof(name);
        b[3].length = &nameLen;
        b[4].buffer_type = MYSQL_TYPE_TINY;
        b[4].buffer = &isDir;
        b[5].buffer_type = MYSQL_TYPE_LONGLONG;
        b[5].buffer = &size;
        b[6].buffer_type = MYSQL_TYPE_STRING;
        b[6].buffer = md5;
        b[6].buffer_length = sizeof(md5);
        b[6].length = &md5Len;
        b[7].buffer_type = MYSQL_TYPE_STRING;
        b[7].buffer = storageKey;
        b[7].buffer_length = sizeof(storageKey);
        b[7].length = &storageKeyLen;
        b[8].buffer_type = MYSQL_TYPE_LONG;
        b[8].buffer = &refCount;
        b[9].buffer_type = MYSQL_TYPE_TINY;
        b[9].buffer = &isDeleted;
        b[10].buffer_type = MYSQL_TYPE_STRING;
        b[10].buffer = updatedAt;
        b[10].buffer_length = sizeof(updatedAt);
        b[10].length = &updatedAtLen;
        return b;
    }

    model::File toFile() const {
        model::File f;
        f.id = id;
        f.userId = userId;
        f.parentId = parentId;
        f.name.assign(name, nameLen);
        f.isDir = isDir;
        f.size = size;
        f.md5.assign(md5, md5Len);  // NULL → 空串
        f.storageKey.assign(storageKey, storageKeyLen);
        f.refCount = refCount;
        f.isDeleted = isDeleted;
        f.updatedAt.assign(updatedAt, updatedAtLen);
        return f;
    }
};

}  // namespace

std::vector<model::File> FileDao::findByUserIdAndParentId(int64_t userId, int64_t parentId) {
    db::ScopedConn conn(db::pool());
    MYSQL* c = conn.get();

    MYSQL_STMT* stmt = mysql_stmt_init(c);
    if (!stmt) return {};

    std::string sql = std::string(kSelectCols) +
                      "WHERE user_id = ? AND parent_id = ? ORDER BY is_dir DESC, name ASC";
    if (mysql_stmt_prepare(stmt, sql.c_str(), static_cast<unsigned long>(sql.size())) != 0) {
        mysql_stmt_close(stmt);
        return {};
    }

    MYSQL_BIND param[2];
    std::memset(param, 0, sizeof(param));
    param[0].buffer_type = MYSQL_TYPE_LONGLONG;
    param[0].buffer = &userId;
    param[1].buffer_type = MYSQL_TYPE_LONGLONG;
    param[1].buffer = &parentId;
    if (mysql_stmt_bind_param(stmt, param) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return {};
    }

    FileRow row;
    MYSQL_BIND res[11];
    mysql_stmt_bind_result(stmt, row.bind(res));

    std::vector<model::File> out;
    int rc;
    while ((rc = mysql_stmt_fetch(stmt)) == 0) {
        out.push_back(row.toFile());
    }
    mysql_stmt_close(stmt);
    return out;
}

std::optional<model::File> FileDao::findById(int64_t id) {
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

    FileRow row;
    MYSQL_BIND res[11];
    mysql_stmt_bind_result(stmt, row.bind(res));

    std::optional<model::File> out;
    if (mysql_stmt_fetch(stmt) == 0) out = row.toFile();
    mysql_stmt_close(stmt);
    return out;
}

std::optional<model::File> FileDao::findByNameAndParent(int64_t userId, int64_t parentId,
                                                        const std::string& name) {
    db::ScopedConn conn(db::pool());
    MYSQL* c = conn.get();

    MYSQL_STMT* stmt = mysql_stmt_init(c);
    if (!stmt) return std::nullopt;

    std::string sql = std::string(kSelectCols) +
                      "WHERE user_id = ? AND parent_id = ? AND name = ? LIMIT 1";
    if (mysql_stmt_prepare(stmt, sql.c_str(), static_cast<unsigned long>(sql.size())) != 0) {
        mysql_stmt_close(stmt);
        return std::nullopt;
    }

    MYSQL_BIND param[3];
    std::memset(param, 0, sizeof(param));
    param[0].buffer_type = MYSQL_TYPE_LONGLONG;
    param[0].buffer = &userId;
    param[1].buffer_type = MYSQL_TYPE_LONGLONG;
    param[1].buffer = &parentId;
    param[2].buffer_type = MYSQL_TYPE_STRING;
    param[2].buffer = const_cast<char*>(name.data());
    param[2].buffer_length = static_cast<unsigned long>(name.size());
    if (mysql_stmt_bind_param(stmt, param) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return std::nullopt;
    }

    FileRow row;
    MYSQL_BIND res[11];
    mysql_stmt_bind_result(stmt, row.bind(res));

    std::optional<model::File> out;
    if (mysql_stmt_fetch(stmt) == 0) out = row.toFile();
    mysql_stmt_close(stmt);
    return out;
}

int FileDao::insert(model::File& f) {
    db::ScopedConn conn(db::pool());
    MYSQL* c = conn.get();

    MYSQL_STMT* stmt = mysql_stmt_init(c);
    if (!stmt) return -1;

    const char* sql =
        "INSERT INTO `file` (user_id, parent_id, name, is_dir, size, md5, storage_key, "
        "ref_count, is_deleted) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";
    if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
        mysql_stmt_close(stmt);
        return -1;
    }

    signed char isDir = static_cast<signed char>(f.isDir);
    signed char isDeleted = static_cast<signed char>(f.isDeleted);

    MYSQL_BIND p[9];
    std::memset(p, 0, sizeof(p));
    p[0].buffer_type = MYSQL_TYPE_LONGLONG;
    p[0].buffer = &f.userId;
    p[1].buffer_type = MYSQL_TYPE_LONGLONG;
    p[1].buffer = &f.parentId;
    p[2].buffer_type = MYSQL_TYPE_STRING;
    p[2].buffer = const_cast<char*>(f.name.data());
    p[2].buffer_length = static_cast<unsigned long>(f.name.size());
    p[3].buffer_type = MYSQL_TYPE_TINY;
    p[3].buffer = &isDir;
    p[4].buffer_type = MYSQL_TYPE_LONGLONG;
    p[4].buffer = &f.size;
    if (f.md5.empty()) {
        p[5].buffer_type = MYSQL_TYPE_NULL;
    } else {
        p[5].buffer_type = MYSQL_TYPE_STRING;
        p[5].buffer = const_cast<char*>(f.md5.data());
        p[5].buffer_length = static_cast<unsigned long>(f.md5.size());
    }
    if (f.storageKey.empty()) {
        p[6].buffer_type = MYSQL_TYPE_NULL;
    } else {
        p[6].buffer_type = MYSQL_TYPE_STRING;
        p[6].buffer = const_cast<char*>(f.storageKey.data());
        p[6].buffer_length = static_cast<unsigned long>(f.storageKey.size());
    }
    p[7].buffer_type = MYSQL_TYPE_LONG;
    p[7].buffer = &f.refCount;
    p[8].buffer_type = MYSQL_TYPE_TINY;
    p[8].buffer = &isDeleted;

    if (mysql_stmt_bind_param(stmt, p) != 0 || mysql_stmt_execute(stmt) != 0) {
        unsigned int err = mysql_stmt_errno(stmt);
        mysql_stmt_close(stmt);
        return static_cast<int>(err);
    }

    f.id = static_cast<int64_t>(mysql_stmt_insert_id(stmt));
    mysql_stmt_close(stmt);
    return 0;
}

bool FileDao::updateName(int64_t id, const std::string& name) {
    db::ScopedConn conn(db::pool());
    MYSQL* c = conn.get();

    MYSQL_STMT* stmt = mysql_stmt_init(c);
    if (!stmt) return false;

    const char* sql = "UPDATE `file` SET name = ? WHERE id = ?";
    if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND p[2];
    std::memset(p, 0, sizeof(p));
    p[0].buffer_type = MYSQL_TYPE_STRING;
    p[0].buffer = const_cast<char*>(name.data());
    p[0].buffer_length = static_cast<unsigned long>(name.size());
    p[1].buffer_type = MYSQL_TYPE_LONGLONG;
    p[1].buffer = &id;

    bool ok = mysql_stmt_bind_param(stmt, p) == 0 && mysql_stmt_execute(stmt) == 0;
    mysql_stmt_close(stmt);
    return ok;
}

void FileDao::decrementRefCountByStorageKey(const std::string& storageKey, int delta) {
    db::ScopedConn conn(db::pool());
    MYSQL* c = conn.get();

    MYSQL_STMT* stmt = mysql_stmt_init(c);
    if (!stmt) return;

    const char* sql = "UPDATE `file` SET ref_count = ref_count - ? WHERE storage_key = ?";
    if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
        mysql_stmt_close(stmt);
        return;
    }

    MYSQL_BIND p[2];
    std::memset(p, 0, sizeof(p));
    p[0].buffer_type = MYSQL_TYPE_LONG;
    p[0].buffer = &delta;
    p[1].buffer_type = MYSQL_TYPE_STRING;
    p[1].buffer = const_cast<char*>(storageKey.data());
    p[1].buffer_length = static_cast<unsigned long>(storageKey.size());

    if (mysql_stmt_bind_param(stmt, p) == 0) mysql_stmt_execute(stmt);
    mysql_stmt_close(stmt);
}

void FileDao::deleteByIds(const std::vector<int64_t>& ids) {
    if (ids.empty()) return;

    db::ScopedConn conn(db::pool());
    MYSQL* c = conn.get();

    MYSQL_STMT* stmt = mysql_stmt_init(c);
    if (!stmt) return;

    std::string sql = "DELETE FROM `file` WHERE id IN (";
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i) sql += ",";
        sql += "?";
    }
    sql += ")";
    if (mysql_stmt_prepare(stmt, sql.c_str(), static_cast<unsigned long>(sql.size())) != 0) {
        mysql_stmt_close(stmt);
        return;
    }

    std::vector<long long> idList(ids.begin(), ids.end());
    std::vector<MYSQL_BIND> params(ids.size());
    std::memset(params.data(), 0, sizeof(MYSQL_BIND) * params.size());
    for (size_t i = 0; i < ids.size(); ++i) {
        params[i].buffer_type = MYSQL_TYPE_LONGLONG;
        params[i].buffer = &idList[i];
    }

    if (mysql_stmt_bind_param(stmt, params.data()) == 0) mysql_stmt_execute(stmt);
    mysql_stmt_close(stmt);
}

void FileDao::incrementRefCountByStorageKey(const std::string& storageKey) {
    db::ScopedConn conn(db::pool());
    MYSQL* c = conn.get();

    MYSQL_STMT* stmt = mysql_stmt_init(c);
    if (!stmt) return;

    const char* sql = "UPDATE `file` SET ref_count = ref_count + 1 WHERE storage_key = ?";
    if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
        mysql_stmt_close(stmt);
        return;
    }

    MYSQL_BIND p[1];
    std::memset(p, 0, sizeof(p));
    p[0].buffer_type = MYSQL_TYPE_STRING;
    p[0].buffer = const_cast<char*>(storageKey.data());
    p[0].buffer_length = static_cast<unsigned long>(storageKey.size());

    if (mysql_stmt_bind_param(stmt, p) == 0) mysql_stmt_execute(stmt);
    mysql_stmt_close(stmt);
}

std::optional<model::File> FileDao::findByMd5ForUpdate(const std::string& md5) {
    db::ScopedConn conn(db::pool());
    MYSQL* c = conn.get();

    MYSQL_STMT* stmt = mysql_stmt_init(c);
    if (!stmt) return std::nullopt;

    std::string sql = std::string(kSelectCols) +
                      "WHERE md5 = ? AND is_dir = 0 AND is_deleted = 0 AND storage_key IS NOT NULL "
                      "ORDER BY id ASC LIMIT 1 FOR UPDATE";
    if (mysql_stmt_prepare(stmt, sql.c_str(), static_cast<unsigned long>(sql.size())) != 0) {
        mysql_stmt_close(stmt);
        return std::nullopt;
    }

    MYSQL_BIND param{};
    std::memset(&param, 0, sizeof(param));
    param.buffer_type = MYSQL_TYPE_STRING;
    param.buffer = const_cast<char*>(md5.data());
    param.buffer_length = static_cast<unsigned long>(md5.size());
    if (mysql_stmt_bind_param(stmt, &param) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return std::nullopt;
    }

    FileRow row;
    MYSQL_BIND res[11];
    mysql_stmt_bind_result(stmt, row.bind(res));

    std::optional<model::File> out;
    if (mysql_stmt_fetch(stmt) == 0) out = row.toFile();
    mysql_stmt_close(stmt);
    return out;
}

}  // namespace netdisk::dao
