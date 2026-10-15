# 网盘系统 C++ 重写版 —— 任务拆解

> 版本：v0.1（初稿，逐条审核后再动手）
> 定位：把现有 Java/Spring Boot 版**用 C++ 重写一版**，功能与对外接口契约完全对齐，替换掉 Java 技术栈。
>
> **对外契约不变**是本次重写的第一原则：HTTP 响应体格式、JWT 规则、接口路径与参数、错误码全部与 Java 版保持一致，这样：
> - 前端（`static/` + `templates/`）几乎原样复用；
> - JMeter 压测脚本（`loadtest/netdisk-loadtest.jmx`）不改就能跑；
> - 压测报告与设计文档里「多线程 + NIO / 秒传 / 分片 / 配额乐观锁」这些面试点可以继续讲。
>
> 本文是任务拆解；技术选型映射见 §3，架构与分层见 §4，并发模型映射见 §8。原 Java 版代码在 `src/main/java`，作为**契约与行为的参照实现**保留。

---

## 1. 目标与原则

1. **功能等价**：注册/登录/退出、文件列表/新建文件夹/重命名/删除、普通上传、分片上传、断点续传、秒传、下载（Range）、在线预览、单用户 10GB 配额全部保留。
2. **契约兼容**：`ApiResponse{code,message,data}`、JWT（HS256，subject=userId，claim username，120min）、错误码 0/401/4xx 语义与 Java 版一致。
3. **面试主线平移**：把「Tomcat NIO + 线程池」替换为「Drogon 事件循环 + 线程池」，把「MyBatis + 连接池」替换为「手写 SQL + 连接池」，秒传/分片/配额乐观锁/引用计数/流式 I/O 的**机制不变**，只是换语言实现。
4. **交付物**：可编译运行的 C++ 工程 + 前端静态资源 + 压测脚本与报告 + 设计文档（C++ 版技术点）。

---

## 2. 范围与不改动的部分

| 项 | 处理方式 |
|---|---|
| MySQL 表结构 | **原样复用** `schema.sql`（4 张表：user / file / upload_session / file_chunk） |
| 前端页面 | **复用** `templates/*.html` + `static/*`，仅去掉 Thymeleaf 的 `th:href`/`th:src`，改成普通 `href`/`src` 后当静态资源由 Drogon 提供 |
| SparkMD5 | 复用，秒传 MD5 由前端计算，后端只做比对与合并校验 |
| JMeter 压测 | 复用现有 `.jmx`（协议层无关语言），压测报告重跑 |
| Docker MySQL | 复用 `docker-compose.yml`（MySQL 8，端口 3307） |
| 业务规则 | 复用 `docs/design.md` / `docs/concurrency.md` 的机制与取舍（乐观锁/悲观锁/唯一约束/引用计数） |

---

## 3. 技术选型（C++ 版）

### 3.1 选型总表（Java → C++）

| 原 Java 项 | C++ 替换 | 说明 |
|---|---|---|
| Spring Boot 3 / Tomcat | **Drogon**（异步非阻塞 Web 框架） | 事件循环（epoll/kqueue）+ 工作线程池，天然对应「多线程 + NIO」故事 |
| Tomcat NIO + 线程池 | Drogon 事件循环 + 线程池（`threads_num`） | 见 §8，机制映射「连接多 ≠ 线程多」 |
| MyBatis | **手写 SQL + 连接池**（Drogon `DbClient`/MySQL，或 Connector/C++ 自研池） | SQL 可控，事务/行锁/乐观锁照写 |
| MySQL 8 | MySQL 8（不变） | `libmysqlclient` 或 `mysql-connector-c++` |
| JWT（jjwt） | **jwt-cpp**（header-only，OpenSSL，HS256） | 校验 `Bearer` 与 `?token=` 两种传递 |
| BCrypt | **bcrypt 实现**（Openwall crypt_blowfish 或等价库） | 加盐哈希，与 Java 版 BCrypt 互不干扰（各自独立校验） |
| JSON | **nlohmann/json** | 序列化 `ApiResponse` 与各 DTO/VO |
| Thymeleaf | **静态文件服务**（Drogon `document_root`） | 前端已近乎纯 JS，去掉 `th:` 即可 |
| 配置（application.yml） | **config.json + 环境变量覆盖** | 对应 DB/storage-root/jwt/配额等项 |
| 构建（Maven） | **CMake + vcpkg（或 Conan）** | 依赖可复现，等价 `pom.xml` |
| 日志 | spdlog（或 Drogon 内置日志） | 等价 slf4j/logback |
| 压测 JMeter | JMeter（不变） | 语言无关 |
| 单元测试 | GoogleTest（可选，覆盖关键逻辑） | 配额扣减/路径穿越/Range 解析 |

### 3.2 推荐依赖清单（vcpkg manifest，C++17）

- `drogon`（含 MySQL 支持时依赖 `libmysqlclient`）
- `jwt-cpp` + `openssl`
- `nlohmann-json`
- `bcrypt`（或 vendored 单文件实现）
- `spdlog`
- （可选）`gtest`

> 语言标准：**C++17**（Drogon 稳定支持；如想用 `std::jthread` 等可上 C++20，二选一并在阶段 0 定死）。
> 依赖管理：优先 **vcpkg + `vcpkg.json` manifest**（跨平台、可复现）；macOS 本地也可用 Homebrew 装 drogon 等作为快速通道。

### 3.3 关键待确认项（阶段 0 前敲定）

| 项 | 建议 | 需在阶段 0 验证 |
|---|---|---|
| Drogon 是否原生支持 HTTP `Range`/206 | 不确定则自实现（解析 `Range` → `seek` → 分块读 → 206 + Content-Range） | 阶段 4 前验证 |
| Drogon multipart 上传 body 上限 | `client_max_body_size` / `client_max_memory_body_size` 需配成普通上传 20MB、分片单片 5MB | 阶段 3/6 前验证 |
| MySQL 访问层 | 优先 Drogon `DbClient`（自带连接池 + 同步/异步 SQL）；否则 Connector/C++ + 自研连接池 | 阶段 0 验证 |
| BCrypt 库 | macOS 无 `crypt(3)`，选可移植实现；备选 OpenSSL PBKDF2 | 阶段 1 前验证 |

---

## 4. 架构与分层（C++ 版）

```
浏览器 (复用 templates + static，仅去 Thymeleaf th:)
        │ HTTP (JWT 鉴权)
Controller 层（Drogon HttpController / handler：参数校验、鉴权路由、响应装配）
        │
Service 层（业务：用户/文件/上传/秒传/分片/下载/配额，事务边界）
        │
DAO/Repository 层（手写 SQL：预编译语句、事务、行锁/乐观锁）
        │
MySQL（元数据，连接池）  +  本地磁盘（文件实体，data/）
```

- **元数据与文件分离**：不变（MySQL 存记录，磁盘存 `storage_key` 对应文件）。
- **`storage_key = {userId}/{yyyyMMdd}/{uuid}`**：不含原始文件名，防路径穿越 + 重名冲突（不变）。
- **依赖方向**：Controller → Service → DAO，禁止跨层；对应 Java 版的 `controller/service/mapper` 分层。

### 4.1 建议目录结构

```
netdisk-cpp/
├── CMakeLists.txt
├── vcpkg.json
├── config.json                 # 等价 application.yml
├── sql/
│   └── schema.sql              # 原样复用
├── web/                        # 前端静态资源（从 Java 版拷贝，去 th:）
│   ├── index.html  login.html  register.html
│   ├── css/style.css
│   └── js/app.js  spark-md5.min.js
├── src/
│   ├── main.cpp                # 启动入口：加载配置、连库、注册路由/过滤器
│   ├── common/                 # ApiResponse、错误码、BusinessError、日期/字节工具
│   ├── config/                 # 配置读取（JSON + 环境变量覆盖）
│   ├── auth/                   # Jwt 生成/校验、BCrypt、JwtFilter（中间件）
│   ├── controller/             # AuthController / FileController / UploadController / UserController
│   ├── service/                # UserService / FileService / UploadService / ChunkUploadService / DownloadService / QuotaService / StorageService
│   ├── dao/                    # UserDao / FileDao / UploadSessionDao / FileChunkDao（+ 连接池封装）
│   └── model/                  # User / File / UploadSession / FileChunk 结构体
└── test/                       # GoogleTest（可选）
```

---

## 5. 对外契约锁定（必须与 Java 版逐字段一致）

> 契约以 Java 版现有代码为**唯一参照**：`src/main/java/com/netdisk/{common,dto,vo,controller}`。重写时逐字段对齐，禁止自行增删字段或改错误码。

### 5.1 统一响应体

```json
{ "code": 0, "message": "ok", "data": { ... } }
```

- `code == 0`：成功；`code == 401`：未登录/过期（前端会清 token 跳登录）；其余为非 0 业务错误。
- `message`：给用户看的提示；`data`：各接口返回体。

### 5.2 JWT 规则

- 算法 HS256，`sub = userId`（字符串），claim `username`，`exp = 120min`。
- 传递方式二选一：`Authorization: Bearer <token>`；或查询参数 `?token=<token>`（兼容 `<a href>`/`<img>` 无法带头的场景，预览/下载依赖它）。
- 中间件放行：`/api/auth/register`、`/api/auth/login`、`/api/ping`、静态页面与资源；其余 `/api/**` 需鉴权。

### 5.3 接口清单（对齐 `docs/design.md` §6）

| 方法 | 路径 | 说明 |
|---|---|---|
| POST | /api/auth/register | 注册 |
| POST | /api/auth/login | 登录（返回 JWT + user） |
| GET | /api/files?parentId= | 文件列表 |
| POST | /api/files/upload | 普通上传（multipart：file/parentId/md5，≤20MB） |
| POST | /api/files/instant | 秒传（JSON：name/parentId/size/md5；命中 `{instant:true}`，未命中 `{instant:false}` 后继续普通/分片上传） |
| POST | /api/folders | 新建文件夹（JSON：name/parentId） |
| POST | /api/files/{id}/rename | 重命名（JSON：name） |
| DELETE | /api/files/{id} | 删除（递归 + 引用计数） |
| GET | /api/files/{id}/download | 下载（支持 Range/206） |
| GET | /api/files/{id}/preview | 预览（按 MIME 分流） |
| POST | /api/upload/init | 分片：创建/复用会话（返回 sessionId/chunkSize/totalChunks/uploadedChunks） |
| POST | /api/upload/chunk | 分片：上传单片（multipart：sessionId/chunkIndex/file） |
| GET | /api/upload/status | 分片：查询已传分片（断点续传） |
| POST | /api/upload/complete | 分片：合并完成 |
| GET | /api/user/me | 当前用户配额用量 |
| GET | /api/ping | 健康检查（无鉴权） |

### 5.4 关键字段与错误码（照搬 Java 版）

- 秒传未命中返回 `data = { instant: false }`，**不是错误**（前端据此 fallback 到普通/分片上传）。
- 重命名/新建同名：数据库唯一约束兜底 + 前置校验 fast-fail，统一 400「同名文件或文件夹已存在」。
- 配额不足：明确提示「空间不足」，对应 Java 版 `BusinessException` 的语义。

---

## 6. 前端复用（改动极小）

- 把 `templates/*.html` 里的 `th:href="@{/css/style.css}"` → `href="/css/style.css"`、`th:src="@{/js/...}"` → `src="/js/..."`，删除 `xmlns:th` 命名空间声明。
- 三页（login/register/index）及 `static/js/app.js`、`static/js/spark-md5.min.js` 逻辑**不动**。
- Drogon 以 `web/` 为静态根目录提供；`/` → index.html，`/login` → login.html，`/register` → register.html（等价 Java 版 `PageController` 的路由）。

---

## 7. 并发模型映射（面试点，先理解再动手）

| Java 版概念 | C++ 版落地 | 要点 |
|---|---|---|
| Tomcat NIO 连接器 | Drogon 事件循环（epoll/kqueue） | 少量事件线程管理海量连接，连接多 ≠ 线程多 |
| Tomcat 工作线程池（maxThreads 等） | Drogon 工作线程池（`threads_num` / 队列） | 工作线程只处理「就绪」请求；大文件走流式，处理快、及时归还线程 |
| MyBatis 连接池 | 自建/框架连接池 | 连接池是「线程池的 DB 版」，复用连接避免频繁建连 |
| 流式 I/O（不整读内存） | 上传 `ifstream`/`istream` 分块写盘；下载分块读 + 8KB 缓冲输出 | 全链路分块，与 Java 版 `StreamingResponseBody` 等价 |

> 面试原句「多线程 + NIO 支撑 1000 人同时在线」在 C++ 版照样成立：Drogon 的事件循环 + 线程池就是 NIO + 多线程，故事不变，只是框架换成了 C++。

---

## 8. 分阶段任务（每阶段一次 commit）

> 开发方式同 Java 版：**垂直切片**，每阶段前后端一起交付，做完都能在浏览器点着测。
> 每阶段 checkbox：`[ ]` 待办；完成后勾 `[x]` 并 commit（中文 message）。
> 阶段内任务先逐条审核再动手，不一口气建完。

---

### 阶段 0：项目初始化 + 页面骨架 ✅

- [x] 建立 `cpp/` 工程：`CMakeLists.txt`（依赖见下「实现记录」）
- [x] 复用 `docker-compose.yml`（MySQL 8，端口 3307）+ `sql/schema.sql`（原样复用）
- [x] 配置加载：`config.json`（端口/线程数/静态目录）+ 环境变量覆盖（DB/配额/JWT）
- [x] Drogon 骨架：`main.cpp` 启动、`/api/ping` 健康检查
- [x] 静态资源服务：拷贝前端并去 `th:`，跑通 `/`、`/login`、`/register` 三页 + css/js
- [x] 打通 DB：启动即连库 + 建表（幂等执行 schema.sql）
- [x] 分层目录与 `common/`（ApiResponse）、`config/`、`db/` 就位

**可测点**：`docker start netdisk-mysql` 起 MySQL；`cmake --build build` 后启动，`/api/ping` 返回 `{"code":0,"data":"pong"}`；三页能开、能互跳；启动日志显示「连接成功，schema 初始化完成（成功 4 条，失败 0 条）」。

> **实现记录（阶段 0，与 §3 的差异，后续阶段沿用）**
> - 工程目录用仓库内 `cpp/`（原 Java 版保留作参照）。
> - 依赖走 **Homebrew**（drogon 1.9.14 / mysql-client 26.7.0 / nlohmann-json 3.12.0 / openssl@4），暂未引入 vcpkg。
> - DB 访问用 **libmysqlclient（C API）**，并在连接时强制 `MYSQL_OPT_PROTOCOL=TCP`——libmysqlclient 会把 host=`localhost` 当 Unix socket 连错目标，JDBC 则恒走 TCP，此坑已修。
> - JSON 暂用 drogon 内置 jsoncpp（`Json::Value`，直接喂 `newHttpJsonResponse`）；nlohmann/json 已装未用，阶段 1 再定。
> - 连接池：阶段 0 仅「连接→建表→关闭」单连接；连接池随 DAO 层在阶段 1 落地。
> - 构建/运行：`cd cpp && cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/homebrew && cmake --build build -j && ./build/netdisk`。

---

### 阶段 1：用户体系（后端 + 登录注册页）✅

- [x] `model/User` + `dao/UserDao`（预编译语句：按 username 查、按 id 查、插入）
- [x] 注册：用户名唯一校验（前置 + 唯一约束/ER_DUP_ENTRY 兜底）+ BCrypt 加密
- [x] 登录：校验 BCrypt → 签发 JWT（HS256，sub/claim/exp 对齐 §5.2）
- [x] `auth/JwtFilter`（Drogon 过滤器）：放行注册/登录/ping/静态资源，校验 `Bearer` 与 `?token=`
- [x] `/api/user/me`（受保护）+ 登录/注册页接真数据（前端复用 app.js）
- [x] DB 连接池（`db/DbPool` + `ScopedConn`，16 连接）

**可测点**：注册 → 登录 → 带 token 查 `/api/user/me` 全流程可用；错误密码被拒；`/api/**` 未带/带错 token 返回 401（`Bearer` 与 `?token=` 均支持）；库中密码为 `$2b$10$` 哈希。

> **实现记录（阶段 1，与 §3 的差异，后续阶段沿用）**
> - **环境切换**：由 Docker MySQL 改成本机 MySQL 9.4.0（`/usr/local/mysql`，原生进程），账号 `netdisk`/`netdisk`，端口 3306；`Config.h` 默认值已改（可用环境变量覆盖）；Docker 已关闭。
> - JWT：未用 jwt-cpp，改为 **OpenSSL 手写 HS256**（base64url + HMAC-SHA256 + 常数时间签名比对），零新依赖。
> - BCrypt：用 **libxcrypt**（`crypt_gensalt_rn`/`crypt_rn`，`$2b$`，cost=10）。
> - 连接池：自研 `DbPool`（mutex + condition_variable + 空闲队列，16 连接）+ RAII `ScopedConn`。
> - Drogon 两个坑：① 过滤器默认按全限定名注册，需覆盖 `className()` 返回短名 `JwtFilter`，控制器才能引用；② MySQL 8+/9+ `caching_sha2_password` 非 SSL TCP 首连需 `MYSQL_OPT_GET_SERVER_PUBLIC_KEY`。
> - 业务错误统一返回 **HTTP 200 + body `{code:...}`**（与 Java `@RestControllerAdvice` 一致）。

---

### 阶段 2：文件列表 + 目录（后端 + 列表页）✅

- [x] `model/File` + `dao/FileDao`（预编译：按 user_id+parent_id 查、按 id 查、按 name+parent 查、插入、重命名、批量删除）
- [x] 文件列表（按 parent_id，`ORDER BY is_dir DESC, name ASC`，返回字段对齐 Java `FileVO`）
- [x] 新建文件夹、重命名、删除（物理删除 + 递归 + 引用计数递减）
- [x] 同名唯一约束：`uk(user_id,parent_id,name)` 兜底（ER_DUP_ENTRY）+ 前置校验 fast-fail
- [x] 归属校验（requireOwned/requireOwnedDir），越权返回 404/400
- [x] `StorageService`：`resolve`（路径穿越防护）+ 物理删除

**可测点**：建文件夹 → 列表 → 重命名 → 递归删除全流程；同名 400；删除不存在 404；递归删除连子目录一起删。

> **实现记录（阶段 2）**
> - 路径参数坑：Drogon 控制器里的 `{id}` 走 `req->getRoutingParameters()[0]`（按位置），**不是** `getParameter("id")`（那只取 query 参数）。已修。
> - `updatedAt` 对齐：DB 的 DATETIME `"YYYY-MM-DD HH:MM:SS"` → 空格换 `T`，对齐 Java LocalDateTime 序列化；新建/重命名尚未回读落库时间时返回 null。
> - 递归删除 + 引用计数照搬 Java `FileService.delete`（按 storage_key 分组扣 ref_count，归零才删物理文件）。

---

### 阶段 3：普通上传 + 配额拦截（后端 + 上传 UI）✅

- [x] `service/StorageService`：流式落盘，`storage_key={userId}/{yyyyMMdd}/{uuid}` 生成规则
- [x] 配额实时校验：上传前查剩余配额，超限拦截（返回「空间不足」）
- [x] 上传成功后原子扣减 `quota_used`（乐观锁 version 重试，`WHERE quota_used+delta<=quota_limit` 兜底不超卖）
- [x] `service/QuotaService` + 配额用量查询 `/api/user/me`
- [x] 普通上传接口 `/api/files/upload`（multipart ≤20MB，带 md5）+ `Drogon body 上限`（25MB/1MB）
- [x] 秒传探测 `/api/files/instant`（本期返回未命中，阶段 5 补去重）
- [x] 页面上传按钮 + 进度 + 配额条（前端复用）

**可测点**：上传成功 + 配额条准确；超限拦截「空间不足」；物理文件落 `data/`；删除文件引用计数归零删物理文件。

> **实现记录（阶段 3）**
> - Drogon multipart 用 `MultiPartParser`（`getFiles()`/`getParameter<T>()`），文件内容在内存（`fileData()`/`fileLength()`），再 `ofstream` 落盘；严格 8KB 流式体现在下载/分片合并（后续阶段）。
> - 乐观锁：每条 DAO 语句各自自动提交，`findById` 重试天然读到最新已提交 version，**无需** Java 的 READ_COMMITTED 事务包裹（面试可讲）。
> - 秒传探测 `/api/files/instant` 本期只返回 `{instant:false}`，MD5 去重 + 引用计数命中留到阶段 5。

---

### 阶段 4：下载 + 在线预览（后端 + 预览页）✅

- [x] 流式下载 + `Range` 支持（206 Partial Content，自实现 Range 解析）
- [x] 图片 / 视频 / PDF / 文本 预览分流（按扩展名 MIME；`?token=` 鉴权）
- [x] `Content-Disposition`：下载 `attachment`、预览 `inline`
- [x] 路径穿越防护：`storage_key` 解析时校验 normalize 后仍在存储根目录内
- [x] 预览页 + 下载入口（前端复用）

**可测点**：四格式可预览；视频可拖动进度（206）；下载走流式 + Range，`curl -H "Range: bytes=..."` 返回 206 + Content-Range。

> **实现记录（阶段 4）**
> - `DownloadService::stream`：8KB 分块读目标区间 → `setBody` + 自定义状态码/头（`setStatusCode(k206PartialContent)`、`setContentTypeCodeAndCustomString`、`Content-Range`/`Accept-Ranges`/`Content-Disposition`）。
> - Range 解析支持 `bytes=start-end` / `bytes=start-` / `bytes=-suffix`，非法回退 200 全量。
> - 注：Drogon 的 `newStreamResponse` 走 chunked 编码，与 `Content-Length`/`Content-Range` 冲突，故本阶段 Range 响应读入内存；严格「不整读内存」的大文件下载留到阶段 7 复查（可改用 `newFileResponse` 全量流式 + 区间内存的混合方案）。

---

### 阶段 5：秒传（MD5 去重）✅

- [x] 上传前客户端算 MD5（前端已实现，复用）
- [x] `/api/files/instant`：按 md5 查重，命中复用物理文件 + 引用计数 +1，秒回 `{instant:true}`；未命中 `{instant:false}`
- [x] 删除时按 storage_key 分组递减 ref_count，归零才删物理文件（阶段 2 已实现，本次验证）
- [x] 并发正确性：`SELECT ... FOR UPDATE ORDER BY id LIMIT 1` 锁 canonical 行（悲观锁）

**可测点**：相同文件第二次上传秒回；删除后引用计数正确；跨用户去重生效（3 行同 key、ref_count 都=3、物理文件仅 1 份）。

> **实现记录（阶段 5）**
> - 新增 `db::DbTransaction`（RAII：autocommit=0 + `SET TRANSACTION ISOLATION LEVEL READ COMMITTED`，析构未提交则回滚）。
> - 用 `thread_local` 让事务期间本线程所有 `ScopedConn` 复用同一条连接，**DAO 方法签名零改动**即可参与事务。
> - 秒传命中流程：事务内 `findByMd5ForUpdate`（FOR UPDATE 锁 canonical 行）→ 扣配额 → 插入新行（ref_count=旧值）→ `incrementRefCountByStorageKey`（全行 +1）→ commit。
> - 已验证跨用户去重 + 删除引用递减（删 2/3 引用后 ref_count=1、物理文件保留；删最后引用 ref_count=0、物理文件删除）。

---

### 阶段 6：分片上传 + 断点续传 + 进度条 ✅

- [x] `init / chunk / status / complete` 四接口（契约对齐 §5.3）
- [x] init 按 `(userId,md5,parentId,status=0)` 幂等复用会话，返回已传分片（断点续传）
- [x] 分片落盘 `data/chunks/{sessionId}/{index}`；`uk(session_id,chunk_index)` 幂等（upsert）
- [x] 合并：边写边重算 MD5（EVP，8KB 缓冲），与客户端 md5 比对，不一致拒绝落库；合并后事务内扣配额 + 写元数据 + 更新会话状态
- [x] 前端 `File.slice` 分片 + 并发上传 + 进度条（前端复用）

**可测点**：大文件分片上传成功；中断后能续传（复用会话 + 返回已传分片）；合并后文件 MD5 一致；MD5 不一致/分片不完整被拒。

> **实现记录（阶段 6）**
> - 新增 `UploadSessionDao` / `FileChunkDao`，`StorageService::storeChunk/mergeChunks/deleteChunks`。
> - 合并 MD5 用 OpenSSL EVP（`EVP_md5()`，边写边 update），8KB 缓冲。
> - `complete` 用 `DbTransaction` 把「扣配额 + 写元数据 + 更新会话状态」做成原子。
> - 孤儿分片：失败/中断会话的临时分片不清理（与 Java 一致，设计文档已记录的边界）。

---

### 阶段 7：并发控制强化（面试核心）✅

- [x] 配额扣减乐观锁（version 重试，阶段 3）+ 秒传悲观锁（阶段 5）+ 同名唯一约束 + 并发删除幂等（阶段 2）
- [x] Drogon 线程池调优：`threads_num=32`（I/O 密集，8 核可超核）、连接池 `32`（与线程数匹配）
- [x] 全链路流式 I/O 复查：下载改用 `newFileResponse`（带 offset/length，磁盘流式发送），消除「整读内存」
- [x] 并发文档：`docs/concurrency-cpp.md`（机制 + 取舍 + 面试 Q&A，对应 Java 版 `concurrency.md`）

**可测点**：并发机制已随阶段 3/5/2 验证；流式下载/线程调优已实测；文档齐全可讲。

> **实现记录（阶段 7）**
> - 核心并发机制早在阶段 3/5/2 落地，本阶段补「线程调优 + 流式复查 + 文档」。
> - **下载流式化**：原来 Range 读入内存，改成 Drogon `newFileResponse(path, offset, length, ...)` 从磁盘流式发送，全量/区间都走它。
> - **乐观锁差异**（面试亮点）：Java 需 READ_COMMITTED 事务包裹乐观锁，C++ 每条 DAO 自动提交，天然读到最新已提交 version；只有 FOR UPDATE 持锁场景才开 `DbTransaction`。

---

### 阶段 8：压测（JMeter）

- [ ] 复用 `loadtest/netdisk-loadtest.jmx`（接口契约一致，脚本不改）
- [ ] 1000 并发压测 + 结果报告（P99 / 错误率 / 吞吐 / TPS）
- [ ] 压测报告文档（C++ 版，与 Java 版数据对比）

**可测点**：报告证明 1000 并发不阻塞、无 5xx、P99 达标（或有数据支撑的边界说明）。

---

### 阶段 9：设计文档 + 面试 Q&A（C++ 版）✅

- [x] 架构设计文档 `docs/design-cpp.md`（C++ 版线程模型、秒传、分片、并发控制、配额）
- [x] 面试 Q&A 清单 `docs/interview-qa-cpp.md`（17 问：事件循环 vs 线程池、乐观锁 vs 悲观锁、秒传、Range、千人在线、连接池、JWT、BCrypt、SQL 注入、路径穿越等）
- [x] 与 Java 版的技术选型对比（`design-cpp.md` §3 对照表 + §8 关键差异）
- [x] 并发文档 `docs/concurrency-cpp.md`（阶段 7）

**可测点**：文档齐全（design-cpp / interview-qa-cpp / concurrency-cpp），每个技术点能经得起追问。

---

## 9. 已确认的默认项（沿用需求文档 §8，不变）

- 删除策略：物理删除，暂不做回收站
- 图片缩略图：前端 `<img>` 原生缩略，后端不生成缩略图
- 管理员角色：暂不做
- 大文件阈值：单文件上限 5GB；>20MB 走分片，单片 5MB
- 秒传去重范围：全局去重 + 引用计数
- 配额扣减：先乐观锁（version 重试），冲突高再换悲观锁
- ORM：**手写 SQL + 连接池**（对应原 MyBatis，SQL 可控）

---

## 10. 风险与已知边界（诚实记录）

- **BCrypt 可移植性**：macOS 无 `crypt(3)`，需选可移植实现；若换 PBKDF2 则与「BCrypt」面试表述需一致口径。
- **Drogon Range 支持**：若不支持需自实现（工作量小，但要测视频拖动）。
- **multipart body 上限**：Drogon 默认上限可能偏小，需按 20MB 普通上传 + 5MB 单片配置，并验证大 body 是否流式落盘而非整读内存。
- **连接池**：若 Drogon DbClient 的 MySQL 支持在所选版本不完整，退回 Connector/C++ 自研池，需多写一个池（可作面试点）。
- **孤儿分片会话**：与 Java 版一致，暂不做定时清理（记录边界）。
- **秒传与删除的边界竞态**：沿用 `docs/concurrency.md` §2 的已知边界与缓解方向。

---

## 11. 验收标准（对齐需求文档 §9）

1. 注册/登录/退出全流程可用，密码 BCrypt、会话 JWT。
2. 单用户 10GB 配额：上传超限被拦截并提示，配额用量实时准确。
3. 上传（普通 + 分片 + 断点续传）、下载（含 Range）、删除、重命名、新建文件夹全部可用。
4. 秒传：相同文件第二次上传命中秒传。
5. 预览：图片/视频/PDF/文本可在线预览。
6. 压测：JMeter 报告证明 1000 并发下请求不阻塞、无 5xx、P99 达标。
7. **接口契约兼容**：现有前端与 JMeter 脚本不改即可对接 C++ 版。

---

## 12. 里程碑顺序

阶段 0（骨架）→ 1（用户）→ 2（列表/目录）→ 3（上传/配额）→ 4（下载/预览）→ 5（秒传）→ 6（分片/续传）→ 7（并发强化）→ 8（压测）→ 9（文档）。

每阶段：先审阶段任务 → 实现 → 按「可测点」在浏览器/curl 验证 → commit。
