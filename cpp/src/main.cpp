#include <drogon/drogon.h>

#include <cstdio>
#include <functional>
#include <memory>

#include "auth/JwtFilter.h"
#include "common/ApiResponse.h"
#include "config/Config.h"
#include "controller/AuthController.h"
#include "controller/FileController.h"
#include "controller/FolderController.h"
#include "controller/UploadController.h"
#include "controller/UserController.h"
#include "dao/FileChunkDao.h"
#include "dao/FileDao.h"
#include "dao/UploadSessionDao.h"
#include "dao/UserDao.h"
#include "db/Db.h"
#include "service/ChunkUploadService.h"
#include "service/DownloadService.h"
#include "service/FileService.h"
#include "service/QuotaService.h"
#include "service/StorageService.h"
#include "service/UploadService.h"
#include "service/UserService.h"

using namespace drogon;

int main() {
    // 服务端日志不缓冲：实时可见，进程异常退出也不丢日志
    setvbuf(stdout, nullptr, _IONBF, 0);

    netdisk::Config cfg;

    app().loadConfigFile("config.json");
    app().setThreadNum(32);        // I/O 密集（文件/DB），工作线程可超核数（8 核 → 32），压测再微调
    app().setDocumentRoot("web");  // 静态资源（css/js）
    app().setStaticFilesCacheTime(0);
    app().setClientMaxBodySize(25 * 1024 * 1024);       // 普通上传 ≤20MB + 表单开销
    app().setClientMaxMemoryBodySize(1 * 1024 * 1024);  // 超过 1MB 的 body 落临时文件

    // 连接池 + 幂等建表
    netdisk::db::initPool(cfg, 32);  // 连接池与线程数匹配，避免线程空等连接
    netdisk::db::initSchema();

    // 依赖装配（等价 Spring 依赖注入）
    auto userDao = std::make_shared<netdisk::dao::UserDao>();
    auto userService = std::make_shared<netdisk::service::UserService>(userDao, cfg);
    auto fileDao = std::make_shared<netdisk::dao::FileDao>();
    auto storageService = std::make_shared<netdisk::service::StorageService>(cfg.storageRoot);
    auto fileService = std::make_shared<netdisk::service::FileService>(fileDao, storageService);
    auto quotaService = std::make_shared<netdisk::service::QuotaService>(userDao);
    auto uploadService = std::make_shared<netdisk::service::UploadService>(
        fileDao, userDao, storageService, quotaService);
    auto downloadService =
        std::make_shared<netdisk::service::DownloadService>(fileDao, storageService);
    auto sessionDao = std::make_shared<netdisk::dao::UploadSessionDao>();
    auto chunkDao = std::make_shared<netdisk::dao::FileChunkDao>();
    auto chunkUploadService = std::make_shared<netdisk::service::ChunkUploadService>(
        sessionDao, chunkDao, fileDao, userDao, storageService, quotaService);

    app().registerFilter(std::make_shared<netdisk::auth::JwtFilter>(cfg));
    app().registerController(std::make_shared<netdisk::controller::AuthController>(userService));
    app().registerController(std::make_shared<netdisk::controller::UserController>(userService));
    app().registerController(std::make_shared<netdisk::controller::FileController>(
        fileService, uploadService, downloadService));
    app().registerController(std::make_shared<netdisk::controller::FolderController>(fileService));
    app().registerController(
        std::make_shared<netdisk::controller::UploadController>(chunkUploadService));

    // 健康检查（无鉴权）
    app().registerHandler("/api/ping",
                          [](const HttpRequestPtr&,
                             std::function<void(const HttpResponsePtr&)>&& callback) {
                              callback(HttpResponse::newHttpJsonResponse(
                                  netdisk::ApiResponse::ok(Json::Value("pong"))));
                          });

    // 页面路由（等价 Java PageController）
    app().registerHandler("/",
                          [](const HttpRequestPtr&,
                             std::function<void(const HttpResponsePtr&)>&& callback) {
                              callback(HttpResponse::newFileResponse("web/index.html"));
                          });
    app().registerHandler("/login",
                          [](const HttpRequestPtr&,
                             std::function<void(const HttpResponsePtr&)>&& callback) {
                              callback(HttpResponse::newFileResponse("web/login.html"));
                          });
    app().registerHandler("/register",
                          [](const HttpRequestPtr&,
                             std::function<void(const HttpResponsePtr&)>&& callback) {
                              callback(HttpResponse::newFileResponse("web/register.html"));
                          });

    LOG_INFO << "netdisk (C++) 启动：http://localhost:8080";
    app().run();
    return 0;
}
