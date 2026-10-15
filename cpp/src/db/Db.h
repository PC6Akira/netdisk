#pragma once

#include <mysql/mysql.h>

#include <condition_variable>
#include <mutex>
#include <queue>
#include <vector>

#include "config/Config.h"

namespace netdisk::db {

// MySQL 连接池（等价 Java 数据源连接池；连接池即「线程池的 DB 版」）
class DbPool {
public:
    DbPool(const Config& cfg, int poolSize);
    ~DbPool();

    DbPool(const DbPool&) = delete;
    DbPool& operator=(const DbPool&) = delete;

    MYSQL* acquire();
    void release(MYSQL* conn);

private:
    MYSQL* connect();

    Config cfg_;
    std::mutex mu_;
    std::condition_variable cv_;
    std::queue<MYSQL*> idle_;
    std::vector<MYSQL*> all_;  // 析构时统一关闭
};

// 事务：RAII 包裹，autocommit=0 + READ COMMITTED；析构未提交则回滚。
// 事务期间本线程的所有 ScopedConn 都复用这条连接。
class DbTransaction {
public:
    explicit DbTransaction(DbPool& pool);
    ~DbTransaction();

    DbTransaction(const DbTransaction&) = delete;
    DbTransaction& operator=(const DbTransaction&) = delete;

    void commit();
    MYSQL* get() const { return conn_; }
    static MYSQL* current();  // 当前线程事务连接，无则 nullptr

private:
    DbPool& pool_;
    MYSQL* conn_;
    bool committed_ = false;
};

// RAII 连接句柄：处于事务中时复用事务连接（不归还），否则从池取、析构归还
class ScopedConn {
public:
    explicit ScopedConn(DbPool& pool);
    ~ScopedConn();

    ScopedConn(const ScopedConn&) = delete;
    ScopedConn& operator=(const ScopedConn&) = delete;

    MYSQL* get() const { return conn_; }

private:
    DbPool& pool_;
    MYSQL* conn_;
    bool own_ = false;
};

// 全局单例：main 启动时 initPool 创建，之后 pool() 获取
void initPool(const Config& cfg, int poolSize);
DbPool& pool();

// 幂等建表（CREATE TABLE IF NOT EXISTS）
void initSchema();

}  // namespace netdisk::db
