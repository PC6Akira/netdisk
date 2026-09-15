package com.netdisk.common;

/**
 * 业务异常，携带业务错误码，由全局异常处理器统一转成 ApiResponse。
 */
public class BusinessException extends RuntimeException {

    private final int code;

    public BusinessException(int code, String message) {
        super(message);
        this.code = code;
    }

    public int getCode() {
        return code;
    }
}
