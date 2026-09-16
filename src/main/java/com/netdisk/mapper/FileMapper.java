package com.netdisk.mapper;

import com.netdisk.entity.File;
import org.apache.ibatis.annotations.Param;

import java.util.List;

/**
 * 文件表 Mapper。
 */
public interface FileMapper {

    List<File> findByUserIdAndParentId(@Param("userId") Long userId, @Param("parentId") Long parentId);

    File findById(@Param("id") Long id);

    File findByNameAndParent(@Param("userId") Long userId,
                             @Param("parentId") Long parentId,
                             @Param("name") String name);

    File findByMd5ForUpdate(@Param("md5") String md5);

    int incrementRefCountByStorageKey(@Param("storageKey") String storageKey);

    int decrementRefCountByStorageKey(@Param("storageKey") String storageKey,
                                      @Param("delta") int delta);

    int insert(File file);

    int updateName(@Param("id") Long id, @Param("name") String name);

    int deleteById(@Param("id") Long id);

    int deleteByIds(@Param("ids") List<Long> ids);
}
