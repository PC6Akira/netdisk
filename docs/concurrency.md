# 并发控制设计（阶段 7）

> 核心目标：1000 人同时在线不阻塞、无超卖配额、无脏数据。本文记录每个并发问题的「问题 → 方案 → 为什么这么选 → 面试追问答案」。

---

## 1. 配额扣减 —— 乐观锁（version 重试）

### 问题
并发上传时，若「读 quota_used → 判断剩余 → 写 quota_used」三步分开，两个请求会读到同一个剩余值，双双通过校验，导致配额超卖（quota_used 超过 quota_limit）。

### 方案
`QuotaService.deductQuota(userId, delta)`：

```
循环（最多 3 次）:
    读 user（version、quota_used、quota_limit）
    若 delta > quota_limit - quota_used → 抛"空间不足"
    UPDATE user SET quota_used = quota_used + delta, version = version + 1
        WHERE id = ? AND version = ? AND quota_used + delta <= quota_limit
    若影响 1 行 → 成功返回
    否则 → 重试（version 被并发改过）
```

三个扣配额入口（普通上传 / 秒传 / 分片合并）统一走这个方法。

### 为什么乐观锁，不用悲观锁
- **冲突概率低**：配额是「单用户独立变量」，两个请求同时扣同一用户配额的概率不高。
- **不占行锁**：乐观锁在提交前不持有数据库行锁，事务（尤其是含磁盘 I/O 的上传事务）不会长时间锁住 user 行；悲观锁 `SELECT ... FOR UPDATE` 会一直锁到事务结束，上传要写盘、耗时较长，会放大锁竞争。
- **兜底不超卖**：`WHERE quota_used + delta <= quota_limit` 这条即使在极端竞态下也保证绝不超卖。

### 一个关键坑：事务隔离级别
MySQL 默认 **REPEATABLE READ** 下，事务内的普通 `SELECT` 读的是**快照**，重试时的再读会读到旧 version，乐观锁永远重试不成功。所以扣配额所在的事务隔离级别改为 **READ_COMMITTED**，让每次再读都拿到最新提交的 version。

> 面试追问：「乐观锁重试为什么会失败？」答：REPEATABLE READ 快照读导致重试再读旧数据；解决：READ_COMMITTED 或把重试放到事务级别（每次重试新事务）。

### 备选：悲观锁 / CAS
- 悲观锁：`SELECT ... FOR UPDATE` 锁用户行，适合「冲突概率高 + 临界区长」。
- 纯 CAS（单条 `UPDATE ... WHERE quota_used + delta <= quota_limit`）：对「单字段增减」这种场景，一条条件更新本身就是原子 CAS，无需 version。version 的优势在于能处理「多字段读改写」。

---

## 2. 秒传引用计数 —— 悲观锁（SELECT ... FOR UPDATE）

### 问题
秒传是「读 source.ref_count → 插入新行 → 所有行 ref_count +1」三步，两个并发秒传会读到同一个旧 ref_count，最终各行的 ref_count 不一致（脏数据）。

### 方案
`findByMd5ForUpdate`：`SELECT ... WHERE md5=? AND is_dir=0 ORDER BY id ASC LIMIT 1 FOR UPDATE`，锁**最早插入的 canonical 行**，串行化同一内容的秒传。第二个请求阻塞到第一个提交，再读（当前读）拿到新的 ref_count。

### 为什么这里用悲观锁
- `ref_count` 是「跨用户共享的计数器」，本质是读改写，且秒传要「插入新行 + 多行更新」两步，**没法用一条 CAS 表达**。
- 用 `ORDER BY id ASC LIMIT 1` 保证并发请求锁**同一行**（否则各锁各的行，串行化失效）。

### 删除时的引用计数
删除按 `storage_key` 分组，`UPDATE ... SET ref_count = ref_count - nDel WHERE storage_key=?` 是**单条原子 UPDATE**，无需锁；`after == 0` 才删物理文件。

### 已知边界竞态（记录，非阻塞）
「删除最后一个引用」与「秒传新增引用」并发时，删除可能误判归零而删掉物理文件，而秒传刚插入的新行指向已删文件。缓解：删除/秒传都锁定 canonical 行，或用「延迟物理回收 + 回收前再查 ref_count」。当前项目以「删除前 `FOR UPDATE` 锁 canonical 行」为主，此边界属深度优化，暂记录。

---

## 3. 同名文件 —— 唯一约束（DB 兜底）

### 问题
`findByNameAndParent` 前置校验是 TOCTOU（检查与插入之间有竞态），并发同名会插入两条脏数据。

### 方案
- `file` 表加 `UNIQUE KEY uk_user_parent_name (user_id, parent_id, name)`，数据库兜底。
- `GlobalExceptionHandler` 把 `DuplicateKeyException` 统一转 400「同名文件或文件夹已存在」。
- 前置校验保留作 fast-fail（友好、少一次 DB 报错）。

> 面试追问：「为什么还要保留前置校验？」答：前置校验是体验优化（提前友好提示），唯一约束才是正确性保证；竞态下的漏网请求由约束 + 全局异常兜底。

---

## 4. 并发删除 —— 幂等 + 原子

- 删除先 `requireOwned`（查存在性），并发删除同一 id 时，后到者查到记录已删 → 404。
- 批量 `deleteByIds` 对已删记录是 no-op，天然幂等。
- 递归删除 + 引用计数递减见 §2。

---

## 5. Tomcat NIO 线程池调优

核心思想：**连接多 ≠ 线程多**。NIO 连接器用少量 Acceptor/Poller 线程管理连接，工作线程池只处理「就绪」请求，1000 个长连接不必对应 1000 个工作线程。

| 参数 | 值 | 依据 |
|---|---|---|
| `threads.max` | 200 | 工作线程数。文件流式 I/O + 轻 CPU，200 足够；瓶颈在磁盘 I/O 而非线程数 |
| `threads.min-spare` | 10 | 常驻空闲线程，突发请求不频繁建线程 |
| `accept-count` | 100 | 工作线程全忙时 OS 层等待队列，满了快速失败而非无限排队 |
| `max-connections` | 10000 | NIO 最大并发连接（含 keep-alive），给 1000 并发 + 空闲连接留余量 |
| `max-swallow-size` | 2MB | 请求失败/中止时 Tomcat 最多吞掉的请求体，防止大文件占满内存 |

> 面试追问：「为什么 1000 并发不需要 1000 线程？」答：NIO 连接器把连接管理与请求处理解耦，工作线程只处理就绪请求；大文件走流式读写，工作线程不整块持有文件字节，处理快、及时释放回池。

---

## 6. 全链路流式 I/O 复查清单

| 环节 | 实现 | 是否整读内存 |
|---|---|---|
| 普通上传 | `StorageService.store` 用 `Files.copy(InputStream, path)` 分块写盘 | 否 |
| 分片上传 | `storeChunk` 分片流式落盘 | 否 |
| 分片合并 | `mergeChunks` 8KB 缓冲逐片追加 + 边写边算 MD5 | 否 |
| 下载/预览 | `StreamingResponseBody` 8KB 缓冲分块输出 + `Range` | 否 |
| 请求体上限 | `multipart.max-file-size=20MB`、`max-request-size=25MB`、`max-swallow-size=2MB` | 否 |

结论：上传、下载、分片、合并全程分块读写，不存在把大文件整体读入内存的路径。

---

## 7. 面试 Q&A 速记

1. **乐观锁 vs 悲观锁？** 见 §1。配额低冲突用乐观锁（不占行锁、重试成本低）；ref_count 跨用户共享、读改写用悲观锁（见 §2）。
2. **秒传原理？** 客户端算 MD5 → 服务端 `FOR UPDATE` 查重 → 命中复用物理文件 + ref_count+1 秒回，未命中走正常上传。
3. **引用计数怎么保证并发正确？** 悲观锁串行化秒传；删除用单条原子 UPDATE 递减，归零才删物理文件。
4. **同名文件并发怎么处理？** 唯一约束兜底 + 前置校验 fast-fail。
5. **千人在线如何撑住？** NIO 线程模型（连接多≠线程多）+ 全链路流式 I/O + 工作线程只处理就绪请求。
6. **为什么这里事务要 READ_COMMITTED？** 见 §1，乐观锁重试需要再读看到最新数据。
