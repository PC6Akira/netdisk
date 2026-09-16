package com.netdisk.dto;

/**
 * 分片上传：合并完成请求体。
 */
public class CompleteUploadRequest {

    private Long sessionId;

    public Long getSessionId() {
        return sessionId;
    }

    public void setSessionId(Long sessionId) {
        this.sessionId = sessionId;
    }
}
