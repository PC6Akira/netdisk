#include "db/Db.h"

#include <fstream>
#include <sstream>
#include <vector>

#include <drogon/drogon.h>

namespace netdisk::db {
namespace {

DbPool* g_pool = nullptr;
thread_local MYSQL* g_txnConn = nullptr;  // 当前线程的事务连接

std::string trim(const std::string& s) {
    auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// 去掉注释行后按 ';' 拆分（schema.sql 内语句无含分号的字符串字面量，够用）
std::vector<std::string> splitStatements(const std::string& sql) {
    std::istringstream iss(sql);
    std::string line, cleaned;
    while (std::getline(iss, line)) {
        if (trim(line).rfind("--", 0) == 0) continue;
        cleaned += line;
        cleaned += "\n";
    }
    std::vector<std::string> out;
    std::string cur;
    for (char c : cleaned) {
        if (c == ';') {
            std::string s = trim(cur);
            if (!s.empty()) out.push_back(s);
            cur.clear();
        } else {
            cur += c;
        }
    }
    return out;
}

}  // namespace

DbPool::DbPool(const Config& cfg, int poolSize) : cfg_(cfg) {
    for (int i = 0; i < poolSize; ++i) {
        MYSQL* c = connect();
        if (c) {
            idle_.push(c);
            all_.push_back(c);
        }
    }
    LOG_INFO << "[db] 连接池就绪，连接数=" << all_.size();
}

DbPool::~DbPool() {
    for (MYSQL* c : all_) mysql_close(c);
}

MYSQL* DbPool::connect() {
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) return nullptr;

    unsigned int timeout = 5;
    mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

    // 强制走 TCP：对齐 JDBC（Java 版 URL 恒为 TCP），否则 host="localhost" 会被当 Unix socket
    unsigned int proto = MYSQL_PROTOCOL_TCP;
    mysql_options(conn, MYSQL_OPT_PROTOCOL, &proto);

    // MySQL 8+/9+ 默认 caching_sha2_password：非 SSL TCP 首连需要服务端 RSA 公钥
    bool getKey = true;
    mysql_options(conn, MYSQL_OPT_GET_SERVER_PUBLIC_KEY, &getKey);

    if (!mysql_real_connect(conn, cfg_.mysqlHost.c_str(), cfg_.mysqlUser.c_str(),
                            cfg_.mysqlPassword.c_str(), cfg_.mysqlDb.c_str(),
                            static_cast<unsigned int>(cfg_.mysqlPort), nullptr, 0)) {
        LOG_WARN << "[db] 连接失败: " << mysql_error(conn);
        mysql_close(conn);
        return nullptr;
    }
    mysql_set_character_set(conn, "utf8mb4");
    return conn;
}

MYSQL* DbPool::acquire() {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [this] { return !idle_.empty(); });
    MYSQL* c = idle_.front();
    idle_.pop();
    return c;
}

void DbPool::release(MYSQL* conn) {
    if (!conn) return;
    std::lock_guard<std::mutex> lk(mu_);
    idle_.push(conn);
    cv_.notify_one();
}

DbTransaction::DbTransaction(DbPool& p) : pool_(p), conn_(p.acquire()) {
    mysql_autocommit(conn_, false);
    // 事务内 READ COMMITTED：乐观锁重试的再读能看到并发提交的新 version（对齐 Java 版隔离级别）
    mysql_query(conn_, "SET TRANSACTION ISOLATION LEVEL READ COMMITTED");
    g_txnConn = conn_;
}

DbTransaction::~DbTransaction() {
    g_txnConn = nullptr;
    if (!committed_) mysql_rollback(conn_);
    mysql_autocommit(conn_, true);
    pool_.release(conn_);
}

void DbTransaction::commit() {
    mysql_commit(conn_);
    committed_ = true;
}

MYSQL* DbTransaction::current() {
    return g_txnConn;
}

ScopedConn::ScopedConn(DbPool& p) : pool_(p) {
    if (MYSQL* txn = DbTransaction::current()) {
        conn_ = txn;  // 复用事务连接
        own_ = false;
    } else {
        conn_ = p.acquire();
        own_ = true;
    }
}

ScopedConn::~ScopedConn() {
    if (own_) pool_.release(conn_);
}

void initPool(const Config& cfg, int poolSize) {
    static DbPool instance(cfg, poolSize);
    g_pool = &instance;
}

DbPool& pool() { return *g_pool; }

void initSchema() {
    ScopedConn scoped(pool());
    MYSQL* conn = scoped.get();

    std::ifstream in("sql/schema.sql");
    if (!in) {
        LOG_WARN << "[db] 无法打开 sql/schema.sql";
        return;
    }
    std::stringstream ss;
    ss << in.rdbuf();

    int ok = 0, fail = 0;
    for (const auto& stmt : splitStatements(ss.str())) {
        if (mysql_real_query(conn, stmt.c_str(), static_cast<unsigned long>(stmt.size())) != 0) {
            ++fail;
            LOG_WARN << "[db] 建表语句执行失败: " << mysql_error(conn) << " | " << trim(stmt);
        } else {
            ++ok;
        }
    }
    LOG_INFO << "[db] schema 初始化完成（成功 " << ok << " 条，失败 " << fail << " 条）";
}

}  // namespace netdisk::db
