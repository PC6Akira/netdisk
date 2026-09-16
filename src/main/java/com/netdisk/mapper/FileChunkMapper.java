package com.netdisk.mapper;

import org.apache.ibatis.annotations.Param;

import java.util.List;

/**
 * 分片记录表 Mapper。
 */
public interface FileChunkMapper {

    int upsert(@Param("sessionId") Long sessionId, @Param("chunkIndex") int chunkIndex);

    List<Integer> findIndexesBySessionId(@Param("sessionId") Long sessionId);

    int countBySessionId(@Param("sessionId") Long sessionId);

    int deleteBySessionId(@Param("sessionId") Long sessionId);
}
