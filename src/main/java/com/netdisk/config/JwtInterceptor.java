package com.netdisk.config;

import com.netdisk.common.BusinessException;
import io.jsonwebtoken.Claims;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.springframework.web.servlet.HandlerInterceptor;

/**
 * JWT 鉴权拦截器：校验 Authorization 头，解析后把 userId/username 放入请求属性。
 */
public class JwtInterceptor implements HandlerInterceptor {

    private final JwtUtil jwtUtil;

    public JwtInterceptor(JwtUtil jwtUtil) {
        this.jwtUtil = jwtUtil;
    }

    @Override
    public boolean preHandle(HttpServletRequest request, HttpServletResponse response, Object handler) {
        if ("OPTIONS".equalsIgnoreCase(request.getMethod())) {
            return true;
        }
        String auth = request.getHeader("Authorization");
        String token;
        if (auth != null && auth.startsWith("Bearer ")) {
            token = auth.substring(7);
        } else {
            // 兼容 <a href>/<img src> 等无法自定义请求头的场景：token 走查询参数
            token = request.getParameter("token");
        }
        if (token == null || token.isBlank()) {
            throw new BusinessException(401, "未登录或登录已过期");
        }
        try {
            Claims claims = jwtUtil.parseToken(token);
            request.setAttribute("userId", Long.valueOf(claims.getSubject()));
            request.setAttribute("username", claims.get("username", String.class));
            return true;
        } catch (Exception e) {
            throw new BusinessException(401, "未登录或登录已过期");
        }
    }
}
