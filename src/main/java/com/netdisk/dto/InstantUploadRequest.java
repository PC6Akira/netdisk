package com.netdisk.dto;

/**
 * 秒传请求体：客户端先算好 MD5，再提交给服务端查重。
 */
public class InstantUploadRequest {

    private String name;
    private Long parentId;
    private Long size;
    private String md5;

    public String getName() {
        return name;
    }

    public void setName(String name) {
        this.name = name;
    }

    public Long getParentId() {
        return parentId;
    }

    public void setParentId(Long parentId) {
        this.parentId = parentId;
    }

    public Long getSize() {
        return size;
    }

    public void setSize(Long size) {
        this.size = size;
    }

    public String getMd5() {
        return md5;
    }

    public void setMd5(String md5) {
        this.md5 = md5;
    }
}
