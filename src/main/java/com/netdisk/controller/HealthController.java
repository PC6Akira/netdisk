package com.netdisk.controller;

import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RestController;

import java.util.Map;

/**
 * 健康检查接口。
 */
@RestController
public class HealthController {

    @GetMapping("/api/ping")
    public Map<String, Object> ping() {
        return Map.of("code", 0, "message", "ok", "data", "pong");
    }
}
