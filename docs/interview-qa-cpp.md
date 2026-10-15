# 面试 Q&A 清单（C++ 版）

> 每个问题给「主答案」+「可能的追问」，力求经得起深挖。对应实现细节见 `design-cpp.md` / `concurrency-cpp.md`。

---

## 1. 为什么用事件循环（NIO）？（vs 传统阻塞模型）

**主答案**：阻塞模型一个连接一个线程，1000 连接就要 1000 线程，每个线程 1MB 栈 → 内存和上下文切换都扛不住。Drogon 的事件循环（epoll/kqueue）用**少量线程管理大量连接**：事件循环负责连接就绪检测，工作线程池只处理「有数据可读」的请求，所以「连接多 ≠ 线程多」。

**追问「请求多了工作线程不还是要排队吗？」**：对，事件循环解决的是「连接管理」的线程开销，不是「计算」的线程开销。工作线程数仍要按 CPU 密集 vs I/O 密集比例定。本项目文件/DB I/O 密集，`threads_num=32`（8 核可超核数）。

## 2. 乐观锁 vs 悲观锁？怎么选？

**主答案**：乐观锁不占行锁、冲突时重试，适合冲突概率低；悲观锁（`FOR UPDATE`）占行锁、串行化，适合冲突高 + 临界区长。

- 配额扣减用乐观锁（version 重试）：单用户配额冲突低，且上传事务含磁盘 I/O，悲观锁会长时间锁 user 行。
- 秒传引用计数用悲观锁：`ref_count` 是跨用户共享的读改写，秒传要「插新行 + 多行更新」，单条 CAS 表达不了。

**追问「乐观锁重试为什么可能失败？」**：Java 版是 MySQL 默认 REPEATABLE READ 下事务内 SELECT 读快照，重试读到旧 version。**C++ 版每条 DAO 语句各自自动提交**，重试的再读天然读到最新已提交 version，不需要事务包裹——这是 C++ 版比 Java 版更省心的一点（见 `concurrency-cpp.md` §1）。

## 3. 秒传原理？

**主答案**：客户端先算文件 MD5（SparkMD5 分片读，不阻塞主线程）→ 调秒传接口 → 服务端按 MD5 查重（`FOR UPDATE` 锁 canonical 行）→ 命中则复用已有物理文件 + 引用计数 +1，秒回不传文件本体；未命中才走正常上传。

**追问「为什么客户端算 MD5，不服务端算？」**：秒传省的是带宽——服务端算 MD5 意味着文件已传上来，就没意义了。客户端算好只发 32 字节哈希问「你有没有」。

**追问「客户端能伪造 MD5 吗？」**：能。严肃场景服务端落盘时重算校验，或只把客户端 MD5 当「去重提示」。本项目分片合并时服务端重算整文件 MD5 比对；普通上传信任客户端 MD5（记为可加强点）。

## 4. Range 原理（206 断点下载）？

**主答案**：HTTP 请求头 `Range: bytes=start-end`，服务端解析后只返回该区间，响应 `206 Partial Content` + `Content-Range: bytes start-end/total` + `Accept-Ranges: bytes`。断点下载和视频拖动都靠它。

**追问「服务端怎么只吐一段？」**：C++ 版用 Drogon `newFileResponse(path, offset, length)`，从磁盘按 offset/length 流式发送（异步文件流/sendfile），不把文件整读内存。（Java 版是 `InputStream.skip(start)` 后读 length 字节。）

## 5. 千人在线如何撑住？

**主答案**：四件事——① 事件循环线程模型（连接多≠线程多）；② 全链路流式 I/O（大文件不整读内存，工作线程快速回池）；③ JWT 无状态会话（不占服务端内存）；④ 线程池/连接池参数调优（`threads_num=32`、连接池 32）。

**追问「瓶颈在哪？」**：单机下是 CPU（BCrypt 登录密集）+ 磁盘 I/O（上传下载）。要往上，加核/分布式、BCrypt 降 cost、文件走对象存储/CDN、DB 读写分离。

## 6. 引用计数怎么保证并发正确？

**主答案**：`ref_count` = 指向同一 `storage_key` 的文件行数。秒传用 `SELECT ... FOR UPDATE`（`ORDER BY id LIMIT 1` 锁最早 canonical 行）串行化；删除用单条原子 `UPDATE ... ref_count=ref_count-nDel` 递减，归零才删物理文件。

**追问「为什么 ORDER BY id LIMIT 1？」**：保证并发请求锁**同一行**，否则各锁各的行，串行化失效。

**追问「FOR UPDATE 怎么持锁？」**：C++ 版用 `DbTransaction`（RAII + thread-local 复用连接，autocommit=0）保证锁持有到 commit。

## 7. 分片上传 + 断点续传原理？

**主答案**：`创建会话 → 前端 File.slice 切片并发上传 → 合并`。断点续传靠 `init` 按 `(userId, md5, parentId, status=0)` 幂等复用会话并返回已传分片，客户端只补缺失片。合并时服务端边写边算 MD5（OpenSSL EVP）校验完整性，成功才在事务里扣配额 + 写元数据。

**追问「为什么断点续传服务端驱动，不前端存 sessionId？」**：前端 localStorage 存 sessionId 清缓存就丢了；按内容 MD5 幂等，重试天然命中同一会话，更健壮。

## 8. 为什么不整读内存（流式 I/O）？

**主答案**：大文件整读进内存会打爆堆 + 长时间占用工作线程。本项目上传/下载/分片/合并全程分块/流式，内存 O(1)。（`concurrency-cpp.md` §6 有复查清单。）

## 9. 路径穿越怎么防？

**主答案**：`storage_key` 不存原始文件名（`userId/日期/uuid`），且 `resolve` 时归一化后校验仍以存储根目录为前缀，拒绝 `../` 逃逸。

## 10. SQL 注入怎么防？

**主答案**：DAO 全部用 `mysql_stmt_*` 预编译语句（占位符 `?` + `MYSQL_BIND` 绑定），不拼接 SQL；用户输入不进 SQL 字符串。（等价 Java 的 MyBatis `#{}`。）

## 11. JWT vs Session？

**主答案**：Session 状态存服务端，千人在线占内存 + 分布式共享麻烦；JWT 无状态、客户端持有、服务端只验签，天然水平扩展。代价是无法主动踢人（靠短过期 + 前端删 token）。

## 12. BCrypt 为什么加盐？

**主答案**：BCrypt 自带随机盐 + 慢哈希（cost 参数），同一密码两次哈希结果不同，防彩虹表；慢哈希抬高暴力破解成本。C++ 版用 libxcrypt（`$2b$` cost=10）。

## 13. 同名文件并发怎么处理？

**主答案**：前置 `findByNameAndParent` 校验是 TOCTOU（检查与插入有竞态），所以加 `UNIQUE(user_id, parent_id, name)` 兜底 + 撞约束返回 MySQL errno 1062 → 400「同名文件或文件夹已存在」。前置校验是体验优化，唯一约束才是正确性保证。

## 14. 事务隔离级别为什么用 READ_COMMITTED？

**主答案**：乐观锁重试依赖「再读看到最新 version」。REPEATABLE READ 下事务内 SELECT 读固定快照，重试读到旧 version，乐观锁永远不成功；READ_COMMITTED 每次 SELECT 读最新已提交。

**追问「C++ 版怎么处理？」**：日常 CRUD 每条语句自动提交，天然无快照问题；只有秒传/分片合并这种「多语句原子 + FOR UPDATE 持锁」场景才开 `DbTransaction`，其内部设 READ_COMMITTED。

## 15. 全链路是怎么防超卖配额的？

**主答案**：扣配额不是「读→判断→写」三步（会超卖），而是单条 `UPDATE ... WHERE quota_used+delta<=quota_limit AND version=?`，把「检查 + 扣减」压成一条原子语句，再配 version 重试兜底。三处扣配额（普通上传/秒传/分片合并）统一走 `QuotaService::deductQuota`。

## 16. 连接池是什么？（C++ 版补充点）

**主答案**：复用 DB 连接的「线程池」——避免每次请求重建 TCP 连接 + 鉴权。自研 `DbPool`：固定 32 连接，`std::mutex + condition_variable + 空闲队列`，`acquire` 阻塞取、`release` 归还，RAII `ScopedConn` 自动还。大小与工作线程数匹配，避免线程空等连接。

## 17. 为什么 C++ 重写后还能讲清这些点？（选型对比）

**主答案**：机制不变，只是语言/框架换了。Java 的「Tomcat NIO + 线程池」→ C++ 的「Drogon 事件循环 + 线程池」，都是「连接多≠线程多」；MyBatis → 手写预编译 SQL + 自研连接池，SQL 可控；Spring @Transactional → 自研 DbTransaction。反而多了几个 C++ 特有的可讲点：手写 JWT（HS256 + 常数时间签名比对）、libxcrypt BCrypt、自研连接池、RAII/thread-local 事务。
