# 网盘系统 C++ 重写版 —— 架构设计文档

> 定位：把 Java/Spring Boot 版用 C++ 重写，功能与对外接口契约完全一致。本文是总设计文档，并发细节见 `concurrency-cpp.md`，面试问答见 `interview-qa-cpp.md`。原 Java 版代码保留作参照实现。

---

## 1. 项目定位与目标

对齐简历原句「多人在线网页版网盘系统」。核心可讲点：

1. **事件循环 + 多线程** 支撑 1000 人同时在线，请求不阻塞；
2. **完整用户体系**（注册/登录/退出），单用户 10GB 独立配额，实时监控并拦截超限上传；
3. **文件上传/下载/删除 + 在线预览**，全格式文件 + 可视化列表管理。

交付物：可编译运行的 C++ 工程 + 复用前端 + 压测脚本 + 设计/并发/面试文档。

---

## 2. 总体架构

```
浏览器 (复用 templates + static，仅去 Thymeleaf th:)
        │ HTTP (JWT 鉴权)
Controller（Drogon HttpController：参数校验、鉴权路由、响应装配）
        │
Service（业务：用户/文件/上传/秒传/分片/下载/配额，事务边界）
        │
DAO（手写预编译 SQL：mysql_stmt_*，事务/行锁/乐观锁）
        │
MySQL（元数据，自研连接池 DbPool）  +  本地磁盘（文件实体，data/）
```

- **元数据与文件分离**：MySQL 存记录（name/md5/storage_key/ref_count…），磁盘存文件实体（`data/`）。
- **JWT 无状态会话**：不占服务端内存，适合千人在线。

---

## 3. 技术选型（Java → C++ 对照）

| 原 Java 项 | C++ 替换 | 理由 |
|---|---|---|
| Spring Boot 3 / Tomcat | **Drogon 1.9**（异步非阻塞） | 事件循环 + 工作线程池，天然对应「多线程 + NIO」 |
| Tomcat NIO + 线程池 | Drogon 事件循环（epoll/kqueue）+ 线程池 | 见 §5.1 |
| MyBatis | **libmysqlclient + 手写预编译 SQL + 自研连接池** | SQL 可控，事务/行锁/乐观锁照写 |
| MySQL 8（Docker） | 本机 MySQL 9.4 | 原生进程，省内存 |
| JWT（jjwt） | **OpenSSL 手写 HS256** | 零新依赖，好讲 |
| BCrypt（spring-security） | **libxcrypt**（`crypt_gensalt_rn`/`crypt_rn`） | `$2b$`，cost=10 |
| JSON（Jackson） | drogon 内置 jsoncpp（`Json::Value`） | 直接喂 `newHttpJsonResponse` |
| Thymeleaf | 静态文件服务 | 前端已近乎纯 JS，去 `th:` 即可 |
| application.yml | config.json + 环境变量 | DB/配额/JWT 走环境变量覆盖 |
| Maven | CMake + Homebrew 依赖 | 可复现 |
| 事务 @Transactional | 自研 `DbTransaction`（RAII + thread-local） | 见 `concurrency-cpp.md` §2 |
| JMeter | JMeter（不变） | 语言无关 |

语言标准 **C++17**；构建 `cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/homebrew`。

---

## 4. 数据模型（与 Java 版完全一致）

| 表 | 关键字段 | 说明 |
|---|---|---|
| `user` | username(唯一)/password/nickname/quota_limit/quota_used/version | version 用于乐观锁 |
| `file` | user_id/parent_id/name/is_dir/size/md5/storage_key/ref_count | 文件与目录统一建模，`uk(user_id,parent_id,name)` 防同名 |
| `upload_session` | user_id/file_name/parent_id/total_size/chunk_size/total_chunks/md5/status | 分片会话，`(user_id,md5,parent_id,status=0)` 幂等复用 |
| `file_chunk` | session_id/chunk_index/status | 只记已上传分片，`uk(session_id,chunk_index)` 幂等 |

`schema.sql` 原样复用。

---

## 5. 核心机制

### 5.1 线程模型（Drogon 事件循环 + 工作线程池）

Drogon 事件循环（epoll/kqueue）管理海量连接，工作线程池（`threads_num`）只处理「就绪」请求。**核心思想：连接多 ≠ 线程多**。

参数：`threads_num=32`（I/O 密集，8 核可超核数）、连接池 `32`（与线程数匹配）。取舍与映射见 `concurrency-cpp.md` §5。

### 5.2 用户体系

- 注册：用户名唯一（`uk_username` + ER_DUP_ENTRY 兜底）+ BCrypt（libxcrypt `$2b$` cost=10）。
- 登录：校验 BCrypt → 签发 JWT（手写 HS256，`sub=userId`、claim `username`、`exp=120min`）。
- 鉴权：`JwtFilter`（Drogon 过滤器）保护 `/api/**`，放行注册/登录/健康检查/静态资源；兼容 `?token=`（`<a>`/`<img>` 无法带头）。
- 退出：无状态，前端删 token。

### 5.3 文件存储与安全

- `storage_key = {userId}/{yyyyMMdd}/{uuid}`，不含原始文件名（防穿越 + 重名）。
- 路径穿越防护：`StorageService::resolve` 归一化后校验仍以存储根目录为前缀。
- 全链路流式：上传 Drogon multipart（body 有界）→ `ofstream`；下载 `newFileResponse`（offset/length，磁盘流式发送）；分片合并 8KB 缓冲 + EVP MD5。

### 5.4 配额扣减（乐观锁 version 重试）

`QuotaService::deductQuota`：读 version → `UPDATE ... SET quota_used=quota_used+?, version=version+1 WHERE id=? AND version=? AND quota_used+?<=quota_limit` → 冲突重试，兜底不超卖。三处扣配额（普通上传/秒传/分片合并）统一走它。见 `concurrency-cpp.md` §1。

### 5.5 秒传（MD5 去重 + 引用计数 + 悲观锁）

客户端 SparkMD5 算 MD5 → `POST /api/files/instant` → 服务端 `findByMd5ForUpdate`（`FOR UPDATE` 锁 canonical 行）查重 → 命中复用 `storage_key` + `ref_count+1` 秒回；未命中走正常上传。删除按 `storage_key` 分组递减，归零才删物理文件。见 `concurrency-cpp.md` §2。

### 5.6 分片上传 + 断点续传

`创建会话(init) → 并发传片(chunk) → 合并(complete)`，`status` 查已传分片。

- 阈值：>20MB 走分片，单片 5MB（前端 `File.slice` 切片，并发 3 片）。
- 断点续传：`init` 按 `(userId, md5, parentId, status=0)` 幂等复用会话并返回已传分片。
- 完整性：合并时边写边 `EVP_md5` 重算整文件 MD5，与客户端 md5 比对，不一致拒绝落库。

### 5.7 下载/预览（Range + 流式）

`newFileResponse(path, offset, length)` 从磁盘流式发送，支持 `Range`（206 + Content-Range），断点下载 + 视频拖动都靠它。按扩展名映射 MIME：图片/视频/PDF/文本在线预览，其余下载。

### 5.8 并发控制

配额乐观锁、秒传悲观锁、同名唯一约束、并发删除幂等，详见 `concurrency-cpp.md`。

---

## 6. 接口清单（与 Java 版完全一致）

| 方法 | 路径 | 说明 |
|---|---|---|
| POST | /api/auth/register | 注册 |
| POST | /api/auth/login | 登录（返回 JWT） |
| GET | /api/files?parentId= | 文件列表 |
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
| GET | /api/ping | 健康检查（无鉴权） |

响应统一 `{code, message, data}`，业务错误一律 HTTP 200 + 非 0 code（与 Java `@RestControllerAdvice` 一致）。

---

## 7. 关键决策记录

| 决策 | 选择 | 理由 |
|---|---|---|
| 删除策略 | 物理删除，不做回收站 | 简历未提回收站 |
| 秒传去重范围 | 全局去重 + 引用计数 | 面试亮点 |
| 图片缩略图 | 前端 `<img>` 原生缩略 | 后端不生成 |
| 单文件上限 | 5GB；>20MB 分片，单片 5MB | 可配置 |
| 配额扣减 | 乐观锁（version 重试） | 低冲突 |
| ORM | 手写预编译 SQL（不用 ORM） | SQL 可控 |
| JWT 实现 | 手写 HS256（不引 jwt-cpp） | 零依赖、好讲 |

---

## 8. 与 Java 版的关键差异（面试可讲）

1. **事务模型**：Java `@Transactional(READ_COMMITTED)` 包裹乐观锁；C++ 每条 DAO 自动提交，重试天然读最新已提交，只有 FOR UPDATE 场景开 `DbTransaction`。
2. **流式下载**：Java `StreamingResponseBody` 分块写；C++ 用 `newFileResponse`（offset/length）磁盘流式发送。
3. **并发模型**：Tomcat NIO（Acceptor/Poller + 工作线程池）→ Drogon 事件循环（epoll/kqueue + 工作线程池），「连接多≠线程多」同一套道理。
4. **依赖**：Maven/Spring 全家桶 → CMake + Homebrew 下的少量库（Drogon/libmysqlclient/libxcrypt/OpenSSL）。
5. **连接池**：Java 数据源连接池 → 自研 `DbPool`（mutex + condition_variable + 空闲队列），是「线程池的 DB 版」这个面试点的载体。

---

## 9. 已知边界（诚实记录）

- 「删除最后一个引用」与「秒传新增引用」并发可能误删物理文件（`concurrency-cpp.md` §2）。
- 孤儿分片会话无定时清理（用户传一半不传了，失败/中断会话的临时分片保留）。
- 普通上传信任客户端 MD5（分片合并会重算校验，普通上传未重算）。
- 压测 P99 达标情况以 JMeter 实测为准（阶段 8）。
