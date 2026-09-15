package com.netdisk.vo;

/**
 * 登录 / 注册成功后的返回体：token + 用户信息。
 */
public class LoginResponse {

    private String token;
    private UserVO user;

    public LoginResponse() {
    }

    public LoginResponse(String token, UserVO user) {
        this.token = token;
        this.user = user;
    }

    public String getToken() {
        return token;
    }

    public void setToken(String token) {
        this.token = token;
    }

    public UserVO getUser() {
        return user;
    }

    public void setUser(UserVO user) {
        this.user = user;
    }
}
