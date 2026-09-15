package com.netdisk.vo;

import com.netdisk.entity.File;

import java.time.LocalDateTime;

/**
 * 文件/目录视图对象。
 */
public class FileVO {

    private Long id;
    private Long parentId;
    private String name;
    private Integer isDir;
    private Long size;
    private String md5;
    private LocalDateTime updatedAt;

    public static FileVO from(File f) {
        FileVO vo = new FileVO();
        vo.id = f.getId();
        vo.parentId = f.getParentId();
        vo.name = f.getName();
        vo.isDir = f.getIsDir();
        vo.size = f.getSize();
        vo.md5 = f.getMd5();
        vo.updatedAt = f.getUpdatedAt();
        return vo;
    }

    public Long getId() {
        return id;
    }

    public void setId(Long id) {
        this.id = id;
    }

    public Long getParentId() {
        return parentId;
    }

    public void setParentId(Long parentId) {
        this.parentId = parentId;
    }

    public String getName() {
        return name;
    }

    public void setName(String name) {
        this.name = name;
    }

    public Integer getIsDir() {
        return isDir;
    }

    public void setIsDir(Integer isDir) {
        this.isDir = isDir;
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

    public LocalDateTime getUpdatedAt() {
        return updatedAt;
    }

    public void setUpdatedAt(LocalDateTime updatedAt) {
        this.updatedAt = updatedAt;
    }
}
