package com.netdisk.mapper;

import com.netdisk.entity.UploadSession;
import org.apache.ibatis.annotations.Param;

/**
 * 分片上传会话表 Mapper。
 */
public interface UploadSessionMapper {

    int insert(UploadSession session);

    UploadSession findById(@Param("id") Long id);

    UploadSession findByUserMd5ParentStatus(@Param("userId") Long userId,
                                            @Param("md5") String md5,
                                            @Param("parentId") Long parentId,
                                            @Param("status") int status);

    int updateStatus(@Param("id") Long id, @Param("status") int status);
}
