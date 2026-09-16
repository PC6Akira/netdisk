package com.netdisk.vo;

/**
 * 秒传结果：instant=true 表示命中秒传，file 为新建的文件记录；
 * instant=false 表示未命中，客户端应转正常上传。
 */
public class InstantUploadVO {

    private boolean instant;
    private FileVO file;

    public static InstantUploadVO hit(FileVO file) {
        InstantUploadVO vo = new InstantUploadVO();
        vo.instant = true;
        vo.file = file;
        return vo;
    }

    public static InstantUploadVO miss() {
        InstantUploadVO vo = new InstantUploadVO();
        vo.instant = false;
        vo.file = null;
        return vo;
    }

    public boolean isInstant() {
        return instant;
    }

    public void setInstant(boolean instant) {
        this.instant = instant;
    }

    public FileVO getFile() {
        return file;
    }

    public void setFile(FileVO file) {
        this.file = file;
    }
}
