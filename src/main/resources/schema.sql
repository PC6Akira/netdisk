-- 网盘系统建表脚本（幂等，可重复执行）

CREATE TABLE IF NOT EXISTS `user` (
    `id`          BIGINT       NOT NULL AUTO_INCREMENT COMMENT '用户ID',
    `username`    VARCHAR(64)  NOT NULL COMMENT '登录名',
    `password`    VARCHAR(128) NOT NULL COMMENT 'BCrypt 密码哈希',
    `nickname`    VARCHAR(64)  DEFAULT NULL COMMENT '昵称',
    `quota_limit` BIGINT       NOT NULL DEFAULT 10737418240 COMMENT '配额上限（字节，默认10GB）',
    `quota_used`  BIGINT       NOT NULL DEFAULT 0 COMMENT '已用空间（字节）',
    `version`     INT          NOT NULL DEFAULT 0 COMMENT '乐观锁版本号',
    `created_at`  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    `updated_at`  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_username` (`username`)
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4 COMMENT = '用户表';

CREATE TABLE IF NOT EXISTS `file` (
    `id`          BIGINT       NOT NULL AUTO_INCREMENT COMMENT '文件ID',
    `user_id`     BIGINT       NOT NULL COMMENT '归属用户ID',
    `parent_id`   BIGINT       NOT NULL DEFAULT 0 COMMENT '父目录ID（0=根目录）',
    `name`        VARCHAR(255) NOT NULL COMMENT '文件/目录名',
    `is_dir`      TINYINT      NOT NULL DEFAULT 0 COMMENT '是否目录 1=是 0=否',
    `size`        BIGINT       NOT NULL DEFAULT 0 COMMENT '文件大小（字节，目录为0）',
    `md5`         CHAR(32)     DEFAULT NULL COMMENT '文件MD5（秒传去重）',
    `storage_key` VARCHAR(512) DEFAULT NULL COMMENT '磁盘存储相对路径',
    `ref_count`   INT          NOT NULL DEFAULT 1 COMMENT '引用计数（秒传复用）',
    `is_deleted`  TINYINT      NOT NULL DEFAULT 0 COMMENT '软删除标记',
    `created_at`  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    `updated_at`  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
    PRIMARY KEY (`id`),
    KEY `idx_user_parent` (`user_id`, `parent_id`)
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4 COMMENT = '文件表';

CREATE TABLE IF NOT EXISTS `upload_session` (
    `id`           BIGINT       NOT NULL AUTO_INCREMENT COMMENT '会话ID',
    `user_id`      BIGINT       NOT NULL COMMENT '上传者ID',
    `file_name`    VARCHAR(255) NOT NULL COMMENT '目标文件名',
    `total_size`   BIGINT       NOT NULL COMMENT '文件总大小',
    `chunk_size`   INT          NOT NULL COMMENT '单片大小（字节）',
    `total_chunks` INT          NOT NULL COMMENT '总片数',
    `md5`          CHAR(32)     DEFAULT NULL COMMENT '整文件MD5',
    `status`       TINYINT      NOT NULL DEFAULT 0 COMMENT '0=上传中 1=合并中 2=完成',
    `created_at`   DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    `updated_at`   DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
    PRIMARY KEY (`id`),
    KEY `idx_user` (`user_id`)
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4 COMMENT = '分片上传会话表';

CREATE TABLE IF NOT EXISTS `file_chunk` (
    `id`          BIGINT   NOT NULL AUTO_INCREMENT COMMENT '分片记录ID',
    `session_id`  BIGINT   NOT NULL COMMENT '所属会话ID',
    `chunk_index` INT      NOT NULL COMMENT '分片序号',
    `status`      TINYINT  NOT NULL DEFAULT 0 COMMENT '0=未上传 1=已上传',
    `created_at`  DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    `updated_at`  DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_session_chunk` (`session_id`, `chunk_index`)
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4 COMMENT = '分片记录表';
