cmake_minimum_required(VERSION 3.20)
project(netdisk_cpp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Drogon（Homebrew 提供 DrogonConfig.cmake）----
find_package(Drogon CONFIG REQUIRED)

# ---- OpenSSL（JWT HMAC-SHA256 + bcrypt 随机盐）----
find_package(OpenSSL REQUIRED)

# ---- MySQL C 客户端（libmysqlclient，keg-only）----
find_path(MYSQL_INCLUDE_DIR
    NAMES mysql/mysql.h
    HINTS /opt/homebrew/opt/mysql-client/include /usr/local/opt/mysql-client/include)
find_library(MYSQL_LIBRARY
    NAMES mysqlclient
    HINTS /opt/homebrew/opt/mysql-client/lib /usr/local/opt/mysql-client/lib)
if(NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_LIBRARY)
    message(FATAL_ERROR "libmysqlclient 未找到，请先执行: brew install mysql-client")
endif()

# ---- libxcrypt（BCrypt 密码哈希）----
find_path(LIBCRYPT_INCLUDE_DIR
    NAMES crypt.h
    HINTS /opt/homebrew/opt/libxcrypt/include /usr/local/opt/libxcrypt/include)
find_library(LIBCRYPT_LIBRARY
    NAMES crypt
    HINTS /opt/homebrew/opt/libxcrypt/lib /usr/local/opt/libxcrypt/lib)
if(NOT LIBCRYPT_INCLUDE_DIR OR NOT LIBCRYPT_LIBRARY)
    message(FATAL_ERROR "libxcrypt 未找到，请先执行: brew install libxcrypt")
endif()

add_executable(netdisk
    src/main.cpp
    src/db/Db.cpp
    src/dao/UserDao.cpp
    src/dao/FileDao.cpp
    src/dao/UploadSessionDao.cpp
    src/dao/FileChunkDao.cpp
    src/auth/Bcrypt.cpp
    src/auth/Jwt.cpp
    src/auth/JwtFilter.cpp
    src/service/UserService.cpp
    src/service/StorageService.cpp
    src/service/QuotaService.cpp
    src/service/FileService.cpp
    src/service/UploadService.cpp
    src/service/DownloadService.cpp
    src/service/ChunkUploadService.cpp
    src/controller/AuthController.cpp
    src/controller/UserController.cpp
    src/controller/FileController.cpp
    src/controller/FolderController.cpp
    src/controller/UploadController.cpp
)

target_include_directories(netdisk PRIVATE
    src
    ${MYSQL_INCLUDE_DIR}
    ${LIBCRYPT_INCLUDE_DIR}
)
target_link_libraries(netdisk PRIVATE
    Drogon::Drogon
    OpenSSL::Crypto
    ${MYSQL_LIBRARY}
    ${LIBCRYPT_LIBRARY}
)
