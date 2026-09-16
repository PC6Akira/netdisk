# 多人在线网页版网盘系统 —— 架构设计文档

> 定位：面试深挖用项目，每个技术点要能经得起追问。本文是总设计文档，并发细节见 `concurrency.md`，压测数据见 `loadtest-report.md`。

---

## 1. 项目定位与目标

对齐简历原句「多人在线网页版网盘系统」。核心可讲点：

1. **多线程 + NIO** 支撑 1000 人同时在线，请求不阻塞；
2. **完整用户体系**（注册/登录/退出），单用户 10GB 独立配额，实时监控并拦截超限上传；
3. **文件上传/下载/删除 + 在线预览**，全格式文件 + 可视化列表管理。

交付物：可运行源码 + 压测脚本与报告 + 设计文档（含面试 Q&A）。

## 2. 总体架构

```
浏览器 (Thymeleaf + 原生 JS + SparkMD5)
        │ HTTP (JWT 鉴权)
Controller（参数校验、鉴权路由）
        │
Service（业务：用户/文件/上传/秒传/分片/下载，事务边界）
        │
Mapper（MyBatis，SQL 与锁）
        │
MySQL（元数据）  +  本地磁盘（文件实体）
```

- **元数据与文件分离**：MySQL 存文件记录（name/md5/storage_key/ref_count…），磁盘存文件实体（`data/` 下）。
- **JWT 无状态会话**：不占服务端内存，适合千人在线（对比 Session 的服务器内存占用）。

## 3. 技术选型与理由

| 项 | 选型 | 理由 |
|---|---|---|
| 语言/框架 | Java 21 + Spring Boot 3.3 | LTS + 生态成熟 |
| 并发模型 | Tomcat 内置线程池 + NIO | 贴近「多线程」表述，好讲 |
| 持久层 | MyBatis | SQL 可控，便于讲事务/行锁/乐观锁 |
| 数据库 | MySQL 8（Docker） | 生产级 |
| 会话 | JWT | 无状态，千人在线不占内存 |
| 密码 | BCrypt | 加盐哈希，防彩虹表 |
| 前端 | Thymeleaf + 原生 JS | 聚焦后端并发这个简历重点 |
| 文件存储 | 本地磁盘 + 元数据入 MySQL | 全链路流式，不整读内存 |
| 压测 | JMeter | Java 生态标准 |

## 4. 数据模型

| 表 | 关键字段 | 说明 |
|---|---|---|
| `user` | username(唯一)/password/nickname/quota_limit/quota_used/version | version 用于乐观锁 |
| `file` | user_id/parent_id/name/is_dir/size/md5/storage_key/ref_count | 文件与目录统一建模，`is_dir` 区分；`uk(user_id,parent_id,name)` 防同名 |
| `upload_session` | user_id/file_name/parent_id/total_size/chunk_size/total_chunks/md5/status | 分片会话，`(user_id,md5,parent_id,status=0)` 幂等复用（断点续传） |
| `file_chunk` | session_id/chunk_index/status | 只记录已上传分片，`uk(session_id,chunk_index)` 幂等 |

## 5. 核心机制

### 5.1 线程模型（NIO + 线程池调优）

Tomcat NIO 连接器：少量 Acceptor/Poller 线程管理连接，工作线程池只处理「就绪」请求。

**核心思想：连接多 ≠ 线程多** —— 1000 个长连接不必对应 1000 个工作线程。压测实证：1000 并发、6000 请求，0 错误、无连接拒绝，请求排队而非失败。

调优项（`application.yml`）：`maxThreads=800`、`min-spare=50`、`acceptCount=500`、`maxConnections=10000`、`maxSwallowSize=2MB`。取舍见 `concurrency.md` §5 与 `loadtest-report.md` §4（线程数要按 CPU 密集 vs I/O 密集比例定）。

### 5.2 用户体系

- 注册：用户名唯一（`uk_username`）+ BCrypt 加密；冲突由唯一约束 + 全局异常兜底。
- 登录：校验 BCrypt → 签发 JWT（HMAC 签名，120 分钟过期）。
- 鉴权：`JwtInterceptor` 保护 `/api/**`，放行注册/登录/健康检查；兼容查询参数 token（`<a href>`/`<img>` 无法自定义请求头）。
- 退出：无状态，前端删 token。

### 5.3 文件存储与安全

- `storage_key = {userId}/{yyyyMMdd}/{uuid}`，**不含原始文件名**（防路径穿越 + 重名冲突）。
- 路径穿越防护：`StorageService.resolve` 校验 `normalize` 后仍以存储根目录为前缀。
- 全链路流式 I/O：上传 `Files.copy(InputStream)`、下载 `StreamingResponseBody` 分块、分片合并 8KB 缓冲，**不整读内存**。

### 5.4 配额扣减（乐观锁 version 重试）

`QuotaService.deductQuota`：读 version → `UPDATE ... SET quota_used=quota_used+delta, version=version+1 WHERE id=? AND version=? AND quota_used+delta<=quota_limit` → 冲突重试。兜底不超卖。三处扣配额（普通上传/秒传/分片合并）统一走它。为什么用乐观锁、为什么事务要 READ_COMMITTED，见 `concurrency.md` §1。

### 5.5 秒传（MD5 去重 + 引用计数）

流程：客户端 SparkMD5 分片算 MD5 → `POST /api/files/instant` → 服务端 `SELECT ... FOR UPDATE`（锁 canonical 行）查重 → 命中则复用 `storage_key` + `ref_count+1` 秒回，未命中走正常上传。

- **真省带宽**：命中不传文件本体。
- **引用计数**：`ref_count` = 指向同一 `storage_key` 的文件行数（全局去重，跨用户）。删除时按 `storage_key` 分组递减，归零才删物理文件。
- 并发正确性用悲观锁串行化，见 `concurrency.md` §2。

### 5.6 分片上传 + 断点续传

`创建会话(init) → 并发传片(chunk) → 合并(complete)`，`status` 查询已传分片。

- 阈值：>20MB 走分片，单片 5MB（前端 `File.slice` 切片，并发 3 片）。
- 断点续传：`init` 按 `(userId, md5, parentId, status=0)` **幂等复用会话**并返回已传分片，客户端只补缺失片（刷新/中断重传自动续上，不用前端存 sessionId）。
- 完整性：合并时边写边 `MessageDigest("MD5")` 重算整文件 MD5，与客户端提交的 md5 比对，不一致拒绝落库。

### 5.7 下载/预览（Range + 流式）

`StreamingResponseBody` 分块输出，支持 `Range`（`206 Partial Content`）——断点下载 + 视频拖动进度都靠它。按 MIME 分流：图片/视频/PDF/文本在线预览，其余下载。

### 5.8 并发控制

配额乐观锁、秒传悲观锁、同名唯一约束、并发删除幂等，详见 `concurrency.md`（含取舍与面试追问）。

## 6. 接口清单

| 方法 | 路径 | 说明 |
|---|---|---|
| POST | /api/auth/register | 注册 |
| POST | /api/auth/login | 登录（返回 JWT） |
| GET | /api/files | 文件列表（按 parent_id） |
| POST | /api/files/upload | 普通上传（≤20MB，带 md5） |
| POST | /api/files/instant | 秒传（MD5 查重） |
| POST | /api/folders | 新建文件夹 |
| POST | /api/files/{id}/rename | 重命名 |
| DELETE | /api/files/{id} | 删除（递归 + 引用计数） |
| GET | /api/files/{id}/download | 下载（Range） |
| GET | /api/files/{id}/preview | 预览 |
| POST | /api/upload/init | 分片：创建会话 |
| POST | /api/upload/chunk | 分片：上传单片 |
| GET | /api/upload/status | 分片：查询已传（断点续传） |
| POST | /api/upload/complete | 分片：合并完成 |
| GET | /api/user/me | 当前用户配额用量 |

## 7. 关键决策记录

| 决策 | 选择 | 理由 |
|---|---|---|
| 删除策略 | 物理删除，不做回收站 | 简历未提回收站，降低复杂度 |
| 秒传去重范围 | 全局去重 + 引用计数 | 面试亮点，删除逻辑处理引用计数 |
| 图片缩略图 | 前端 `<img>` 原生缩略 | 后端不生成缩略图 |
| 管理员角色 | 暂不做 | 简历只写注册/登录/退出 |
| 单文件上限 | 5GB；>20MB 分片，单片 5MB | 可配置 |
| 配额扣减 | 先乐观锁（version 重试） | 低冲突场景，压测冲突高再换悲观锁 |

## 8. 已知边界（诚实记录）

- 「删除最后一个引用」与「秒传新增引用」并发可能误删物理文件（`concurrency.md` §2，缓解方向已记录）。
- 孤儿分片会话无定时清理（用户传一半不传了）。
- 压测 P99 未达 500ms（单机 CPU/磁盘瓶颈，`loadtest-report.md` §5）。
