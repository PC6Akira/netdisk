package com.netdisk.vo;

import com.netdisk.entity.User;

/**
 * 用户信息视图对象（不含密码）。
 */
public class UserVO {

    private Long id;
    private String username;
    private String nickname;
    private Long quotaLimit;
    private Long quotaUsed;

    public static UserVO from(User u) {
        UserVO vo = new UserVO();
        vo.id = u.getId();
        vo.username = u.getUsername();
        vo.nickname = u.getNickname();
        vo.quotaLimit = u.getQuotaLimit();
        vo.quotaUsed = u.getQuotaUsed();
        return vo;
    }

    public Long getId() {
        return id;
    }

    public void setId(Long id) {
        this.id = id;
    }

    public String getUsername() {
        return username;
    }

    public void setUsername(String username) {
        this.username = username;
    }

    public String getNickname() {
        return nickname;
    }

    public void setNickname(String nickname) {
        this.nickname = nickname;
    }

    public Long getQuotaLimit() {
        return quotaLimit;
    }

    public void setQuotaLimit(Long quotaLimit) {
        this.quotaLimit = quotaLimit;
    }

    public Long getQuotaUsed() {
        return quotaUsed;
    }

    public void setQuotaUsed(Long quotaUsed) {
        this.quotaUsed = quotaUsed;
    }
}
