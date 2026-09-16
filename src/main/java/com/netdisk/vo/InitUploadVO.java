package com.netdisk.vo;

import java.util.List;

/**
 * 分片上传：初始化 / 查询状态返回体。
 */
public class InitUploadVO {

    private Long sessionId;
    private Integer chunkSize;
    private Integer totalChunks;
    private List<Integer> uploadedChunks;

    public static InitUploadVO of(Long sessionId, Integer chunkSize, Integer totalChunks, List<Integer> uploadedChunks) {
        InitUploadVO vo = new InitUploadVO();
        vo.sessionId = sessionId;
        vo.chunkSize = chunkSize;
        vo.totalChunks = totalChunks;
        vo.uploadedChunks = uploadedChunks;
        return vo;
    }

    public Long getSessionId() {
        return sessionId;
    }

    public void setSessionId(Long sessionId) {
        this.sessionId = sessionId;
    }

    public Integer getChunkSize() {
        return chunkSize;
    }

    public void setChunkSize(Integer chunkSize) {
        this.chunkSize = chunkSize;
    }

    public Integer getTotalChunks() {
        return totalChunks;
    }

    public void setTotalChunks(Integer totalChunks) {
        this.totalChunks = totalChunks;
    }

    public List<Integer> getUploadedChunks() {
        return uploadedChunks;
    }

    public void setUploadedChunks(List<Integer> uploadedChunks) {
        this.uploadedChunks = uploadedChunks;
    }
}
