# 并发控制设计（C++ 版，阶段 7）

> 对应 Java 版 `docs/concurrency.md`，机制一致，语言/框架不同。本文记录每个并发问题的「问题 → 方案 → 为什么这么选 → 面试追问」。

---

## 1. 配额扣减 —— 乐观锁（version 重试）

### 问题
并发上传「读 quota_used → 判断剩余 → 写 quota_used」三步分开，会读到同一个剩余值，双双通过校验，配额超卖。

### 方案
`QuotaService::deductQuota(userId, delta)`：

```
循环（最多 3 次）:
    读 user（version、quota_used、quota_limit）
    若 delta > quota_limit - quota_used → 抛「空间不足」
    UPDATE user SET quota_used = quota_used + ?, version = version + 1
        WHERE id = ? AND version = ? AND quota_used + ? <= quota_limit
    若影响 1 行 → 成功返回；否则重试
```

三个扣配额入口（普通上传 / 秒传 / 分片合并）统一走这个方法。

### 与 Java 版的关键差异（面试亮点）
Java 用 `@Transactional(isolation = READ_COMMITTED)` 包裹，否则 MySQL 默认 **REPEATABLE READ** 下事务内快照读会让重试读到旧 version、永远重试失败。

C++ 版**每条 DAO 语句各自自动提交**（无长事务），所以 `findById` 的再读天然读到最新已提交的 version，**不需要**为乐观锁专门开 READ_COMMITTED 事务。只有需要「多语句原子 + FOR UPDATE 持锁」的场景（秒传、分片合并）才显式开 `DbTransaction`（内部设 READ COMMITTED）。

### 为什么乐观锁，不用悲观锁
- 冲突概率低：配额是「单用户独立变量」。
- 不占行锁：上传事务含磁盘 I/O，悲观锁会一直锁到事务结束，放大锁竞争。
- 兜底不超卖：`WHERE quota_used + delta <= quota_limit` 极端竞态下也绝不超卖。

---

## 2. 秒传引用计数 —— 悲观锁（FOR UPDATE）

### 问题
秒传「读 source.ref_count → 插新行 → 全行 +1」三步，并发秒传会读到旧 ref_count，各行计数不一致。

### 方案
`FileDao::findByMd5ForUpdate`：`SELECT ... WHERE md5=? AND is_dir=0 AND is_deleted=0 AND storage_key IS NOT NULL ORDER BY id ASC LIMIT 1 FOR UPDATE`，锁最早插入的 canonical 行。

- `ORDER BY id ASC LIMIT 1` 保证并发请求锁**同一行**（否则各锁各的行，串行化失效）。
- `DbTransaction`（RAII：autocommit=0 + `SET TRANSACTION ISOLATION LEVEL READ COMMITTED`，析构未提交则回滚）保证 FOR UPDATE 锁持有到 commit。
- 用 `thread_local` 让事务期间本线程所有 `ScopedConn` 复用同一条连接，**DAO 方法签名零改动**即可参与事务。

### 删除时的引用计数
按 storage_key 分组，`UPDATE ... SET ref_count = ref_count - ? WHERE storage_key=?` 是单条原子 UPDATE；`after == 0` 才删物理文件（已实测：删 2/3 引用后 ref_count=1、物理文件保留；删最后引用 ref_count=0、物理文件删除）。

### 已知边界（沿用 Java 版）
「删除最后一个引用」与「秒传新增引用」并发时可能误删物理文件。当前以 FOR UPDATE 锁 canonical 行为主，深度优化暂记录。

---

## 3. 同名文件 —— 唯一约束（DB 兜底）

- `file` 表 `UNIQUE KEY uk_user_parent_name (user_id, parent_id, name)`。
- 插入撞唯一约束 → MySQL errno 1062（ER_DUP_ENTRY）→ 400「同名文件或文件夹已存在」。
- 前置 `findByNameAndParent` 校验保留作 fast-fail（体验优化），唯一约束才是正确性保证。

---

## 4. 并发删除 —— 幂等 + 原子

- `requireOwned` 先查存在性 + 归属，后到者查到已删 → 404。
- 批量 `deleteByIds` 对已删记录是 no-op，天然幂等。
- 递归删除 + 引用计数递减见 §2。

---

## 5. 线程模型 —— Drogon 事件循环 + 工作线程池（对应 Tomcat NIO）

核心思想：**连接多 ≠ 线程多**。

| Java/Tomcat | C++/Drogon | 说明 |
|---|---|---|
| Acceptor + Poller（NIO） | 事件循环（epoll/kqueue） | 少量线程管理海量连接 |
| 工作线程池 `maxThreads` | 工作线程池 `threads_num` | 只处理「就绪」请求 |
| `maxConnections` | 内核级连接管理 | keep-alive 连接占用低 |

参数（当前，压测再微调）：`threads_num=32`（I/O 密集，8 核可超核数）、连接池 `32`（与线程数匹配，避免线程空等连接）。

> 面试追问「为什么 1000 并发不需要 1000 线程」：事件循环把连接管理与请求处理解耦，工作线程只处理就绪请求；大文件走流式读写，工作线程不整块持有文件字节，处理快、及时释放。

---

## 6. 全链路流式 I/O 复查清单

| 环节 | 实现 | 是否整读内存 |
|---|---|---|
| 普通上传 | Drogon multipart（有界，body 上限 25MB）→ `ofstream` 落盘 | 否（有界） |
| 分片上传 | 单片 ≤5MB → `storeChunk` 落盘 | 否（有界） |
| 分片合并 | 8KB 缓冲逐片追加 + EVP MD5 边写边算 | 否 |
| 下载/预览 | `newFileResponse` 带 offset/length 从磁盘流式发送（异步文件流/sendfile） | 否 |
| 秒传 | 不传文件本体 | 否 |

结论：上传、下载、分片、合并全程分块/流式，不存在把大文件整体读入内存的路径。

---

## 7. 面试 Q&A 速记

1. **乐观锁 vs 悲观锁？** 配额低冲突用乐观锁（不占行锁、重试成本低）；ref_count 跨用户共享、读改写用悲观锁（FOR UPDATE 锁 canonical 行）。
2. **秒传原理？** 客户端算 MD5 → 服务端 FOR UPDATE 查重 → 命中复用物理文件 + ref_count+1 秒回，未命中走正常上传。
3. **引用计数怎么保证并发正确？** 悲观锁串行化秒传；删除用单条原子 UPDATE 递减，归零才删物理文件。
4. **同名并发怎么处理？** 唯一约束兜底（ER_DUP_ENTRY→400）+ 前置校验 fast-fail。
5. **千人在线如何撑住？** 事件循环（连接多≠线程多）+ 工作线程只处理就绪请求 + 全链路流式 I/O。
6. **为什么 C++ 版乐观锁不需要 READ_COMMITTED 事务？** 每条 DAO 语句自动提交，重试再读即最新已提交数据；只有 FOR UPDATE 持锁场景才显式开事务（内部设 READ COMMITTED）。
7. **连接池是什么？** 复用 DB 连接的「线程池」，避免每次请求重建 TCP 连接 + 鉴权；大小与工作线程数匹配。
