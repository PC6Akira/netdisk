package com.netdisk.mapper;

import com.netdisk.entity.User;
import org.apache.ibatis.annotations.Param;

/**
 * 用户表 Mapper。
 */
public interface UserMapper {

    User findByUsername(@Param("username") String username);

    User findById(@Param("id") Long id);

    int insert(User user);

    int increaseQuotaUsedWithVersion(@Param("id") Long id, @Param("delta") long delta, @Param("version") int version);
}
