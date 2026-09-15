# 多人在线网页版网盘系统 —— 需求文档

> 版本：v0.1（初稿，待逐条审核）
> 定位：面试深挖用项目，每个技术点要能经得起追问，附压测数据与设计文档。

---

## 1. 项目定位与目标

- 对齐简历原句：多人在线网页版网盘系统（2026.4 – 2026.5）。
- 核心可讲点（面试主线）：
  1. **多线程架构 + NIO** 支撑 1000 人同时在线，请求不阻塞；
  2. **完整用户体系**（注册/登录/退出），单用户 10GB 独立配额，实时监控空间并拦截超限上传；
  3. **文件上传/下载/删除 + 在线预览**，全格式文件 + 可视化列表管理。
- 交付物：可运行的源码 + 压测脚本与报告 + 设计文档（含面试 Q&A）。

---

## 2. 技术选型（已确认，锁定）

| 项 | 选型 | 说明 |
|---|---|---|
| 语言 / 框架 | Java 21 LTS + Spring Boot 3.x | 稳定 LTS，生态成熟 |
| 并发模型 | Tomcat 内置线程池 + NIO + 调优 | 贴近"多线程"表述，好讲 |
| 持久层 | MyBatis | SQL 可控，便于讲事务 / 行锁 / 乐观锁 |
| 数据库 | MySQL 8.x | 生产级，真实项目 |
| 会话 | JWT（无状态） | 千人在线不占服务端内存 |
| 密码 | BCrypt | 加盐哈希，防彩虹表 |
| 前端 | Thymeleaf 服务端渲染 + 原生 JS（fetch） | 聚焦后端并发这个简历重点 |
| 文件存储 | 本地磁盘落盘 + 元数据入 MySQL | 文件流式读写，不整读内存 |
| 压测 | JMeter | Java 生态标准，简历好看 |
| 部署 | Docker Compose（MySQL + 应用） | 一键复现 |

---

## 3. 功能需求

### 3.1 用户体系
- 注册：用户名 + 密码，用户名唯一（唯一约束 + 冲突提示）。
- 登录：校验 BCrypt 密码，签发 JWT（access token）。
- 退出：无状态，前端删除 token 即可。
- 单用户独立配额 10GB（`quota_limit` 可配置，默认 10GB）。

### 3.2 文件管理
- 目录：创建文件夹、浏览文件列表（可视化：图标 + 图片缩略图）。
- 上传：
  - 普通上传（单文件直传，流式写盘）；
  - 分片上传（大文件按片上传，前端并发多片）；
  - 断点续传（只传缺失分片）。
- 下载：流式下载，支持 `Range` 断点下载（浏览器拖动视频也依赖它）。
- 删除：单文件 / 文件夹删除（策略见 §8 待定项）。
- 秒传：上传前按 MD5 去重，命中则秒传（全局去重 + 引用计数）。

### 3.3 在线预览
- 图片：浏览器直接渲染。
- 视频：HTML5 `<video>` 流式播放，服务端支持 `Range`（返回 206）。
- PDF：浏览器 / iframe 预览。
- 文本：后端读取渲染为页面。
- 其余格式：提供下载，不做预览。

### 3.4 配额与并发控制（简历核心，面试深挖重点）
- **实时监控空间**：上传前校验 `剩余配额 = quota_limit - quota_used`，超限直接拦截并返回明确提示。
- **配额扣减原子性**：并发上传时用乐观锁（`version` 字段）重试，或悲观锁（`SELECT ... FOR UPDATE`）——面试要能讲清两种方案及取舍。
- **并发冲突处理**：同名文件、并发删除、同一文件并发上传的互斥。
- **请求不阻塞**：Tomcat NIO + 线程池调优；大文件全程流式处理，不把大文件整块读入内存，避免占用工作线程和堆内存。

---

## 4. 非功能需求

- **并发**：架构上承载 1000 人同时在线，请求不阻塞。
- **性能（压测目标，最终以 JMeter 实测为准）**：
  - 混合场景（上传/下载/列表/预览）1000 并发下，P99 < 500ms；
  - 错误率 0%，无 5xx、无请求超时。
- **安全**：
  - JWT 鉴权 + BCrypt 密码；
  - 下载/预览路径穿越防护（文件名与路径校验，禁止 `../`）；
  - SQL 注入防护（MyBatis `#{}` 预编译）；
  - 上传大小与分片数量上限校验。
- **可维护**：Controller / Service / Mapper 分层清晰；接口文档与设计文档齐备。

---

## 5. 架构设计要点

### 5.1 分层
```
浏览器 (Thymeleaf + 原生JS)
        │ HTTP
Controller（鉴权、参数校验、路由）
   │
Service（业务：用户/文件/上传/预览，事务边界）
   │
Mapper（MyBatis，SQL 与锁）
   │
MySQL（元数据）  +  本地磁盘（文件实体）
```

### 5.2 线程模型（面试重点）
- Tomcat 默认 NIO 连接器（一个 Acceptor 线程 + 多个 Poller 线程 + 工作线程池）。
- 调优项：`maxThreads`（工作线程数）、`acceptCount`（等待队列）、`maxConnections`、`connectionTimeout`、`server.tomcat.max-swallow-size`。
- 核心思想：**连接多 ≠ 线程多**——NIO 让 1000 个长连接不必对应 1000 个工作线程，工作线程只处理"就绪"的请求，避免阻塞。

### 5.3 关键机制
- **秒传**：客户端先算 MD5 → 服务端查 `file.md5` 是否存在 → 命中则直接复用已有物理文件 + 引用计数 +1，秒回；未命中走正常上传。
- **分片上传**：`创建会话 → 上传各分片（前端并发）→ 请求合并 → 校验完整性 → 合并落盘 → 扣配额 → 写入元数据`。
- **断点续传**：客户端查询已上传分片列表，只补传缺失分片。
- **流式 I/O**：上传 `MultipartFile` 流式写盘；下载 `StreamingResponseBody` / `FileSystemResource` 流式输出，不整读内存。

---

## 6. 数据模型（草案）

### user 用户表
| 字段 | 类型 | 说明 |
|---|---|---|
| id | BIGINT PK | 自增 |
| username | VARCHAR(64) UNIQUE NOT NULL | 登录名 |
| password | VARCHAR(128) NOT NULL | BCrypt 哈希 |
| nickname | VARCHAR(64) | 昵称 |
| quota_limit | BIGINT NOT NULL | 配额上限（默认 10737418240 = 10GB） |
| quota_used | BIGINT NOT NULL DEFAULT 0 | 已用空间 |
| version | INT | 乐观锁版本号 |
| created_at / updated_at | DATETIME | 时间戳 |

### file 文件表
| 字段 | 类型 | 说明 |
|---|---|---|
| id | BIGINT PK | 自增 |
| user_id | BIGINT NOT NULL | 归属用户（索引） |
| parent_id | BIGINT NOT NULL | 父目录 id（0 = 根） |
| name | VARCHAR(255) | 文件名 / 文件夹名 |
| is_dir | TINYINT | 1=目录 0=文件 |
| size | BIGINT | 文件大小（目录为 0） |
| md5 | CHAR(32) | 文件 MD5（秒传去重） |
| storage_key | VARCHAR(512) | 磁盘相对路径 |
| ref_count | INT DEFAULT 1 | 引用计数（秒传复用） |
| is_deleted | TINYINT DEFAULT 0 | 软删除标记 |
| created_at / updated_at | DATETIME | 时间戳 |

### upload_session 分片上传会话表
| 字段 | 类型 | 说明 |
|---|---|---|
| id | BIGINT PK | 会话 id |
| user_id | BIGINT | 上传者 |
| file_name | VARCHAR(255) | 目标文件名 |
| total_size | BIGINT | 总大小 |
| chunk_size | INT | 单片大小 |
| total_chunks | INT | 总片数 |
| md5 | CHAR(32) | 整文件 MD5 |
| status | TINYINT | 0=上传中 1=合并中 2=完成 |
| created_at / updated_at | DATETIME | 时间戳 |

### file_chunk 分片记录表
| 字段 | 类型 | 说明 |
|---|---|---|
| id | BIGINT PK | 自增 |
| session_id | BIGINT | 所属会话 |
| chunk_index | INT | 分片序号 |
| status | TINYINT | 0=未传 1=已传 |
| created_at / updated_at | DATETIME | 时间戳 |

---

## 7. 接口设计概要（REST）

| 方法 | 路径 | 说明 |
|---|---|---|
| POST | /api/auth/register | 注册 |
| POST | /api/auth/login | 登录，返回 JWT |
| GET | /api/files | 文件列表（按 parent_id） |
| POST | /api/folders | 新建文件夹 |
| POST | /api/files/upload | 普通上传（单文件） |
| POST | /api/upload/init | 分片上传：创建会话 |
| POST | /api/upload/chunk | 分片上传：上传单片 |
| POST | /api/upload/complete | 分片上传：合并完成 |
| GET | /api/upload/status | 查询已传分片（断点续传） |
| GET | /api/files/{id}/download | 下载（支持 Range） |
| GET | /api/files/{id}/preview | 预览（按类型分流） |
| DELETE | /api/files/{id} | 删除 |
| POST | /api/files/{id}/rename | 重命名 |
| GET | /api/quota | 查询当前用户配额用量 |

---

## 8. 待定 / 需确认项

| 项 | 我的默认建议 | 说明 |
|---|---|---|
| 删除策略 | 物理删除（文件+记录），暂不做回收站 | 简历未提回收站，回收站增加复杂度 |
| 图片缩略图 | 前端用 `<img>` 原生缩略即可，后端不生成缩略图 | 视频缩略图暂不做 |
| 管理员角色 | 暂不做 | 简历只写注册/登录/退出 |
| 单文件大小上限 | 默认 5GB；> 20MB 走分片，单片 5MB | 可配置 |
| 秒传去重范围 | 全局去重 + 引用计数 | 面试亮点，但删除逻辑需处理引用计数 |
| ORM | MyBatis | 已在选型锁定 |

---

## 9. 验收标准

1. 注册/登录/退出全流程可用，密码 BCrypt、会话 JWT。
2. 单用户 10GB 配额：上传超限被拦截并提示，配额用量实时准确。
3. 上传（普通 + 分片 + 断点续传）、下载（含 Range）、删除、重命名、新建文件夹全部可用。
4. 秒传：相同文件第二次上传命中秒传。
5. 预览：图片/视频/PDF/文本可在线预览。
6. 压测：JMeter 报告证明 1000 并发下请求不阻塞、无 5xx，P99 达标。
