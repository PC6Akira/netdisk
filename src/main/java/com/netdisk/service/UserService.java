package com.netdisk.service;

import com.netdisk.common.BusinessException;
import com.netdisk.config.JwtUtil;
import com.netdisk.dto.LoginRequest;
import com.netdisk.dto.RegisterRequest;
import com.netdisk.entity.User;
import com.netdisk.mapper.UserMapper;
import com.netdisk.vo.LoginResponse;
import com.netdisk.vo.UserVO;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.security.crypto.bcrypt.BCryptPasswordEncoder;
import org.springframework.stereotype.Service;

/**
 * 用户服务：注册、登录、查询当前用户。
 */
@Service
public class UserService {

    private final UserMapper userMapper;
    private final JwtUtil jwtUtil;
    private final BCryptPasswordEncoder passwordEncoder = new BCryptPasswordEncoder();
    private final long defaultQuota;

    public UserService(UserMapper userMapper,
                       JwtUtil jwtUtil,
                       @Value("${netdisk.default-quota:10737418240}") long defaultQuota) {
        this.userMapper = userMapper;
        this.jwtUtil = jwtUtil;
        this.defaultQuota = defaultQuota;
    }

    public LoginResponse register(RegisterRequest req) {
        if (isBlank(req.getUsername()) || isBlank(req.getPassword())) {
            throw new BusinessException(400, "用户名和密码不能为空");
        }
        if (userMapper.findByUsername(req.getUsername()) != null) {
            throw new BusinessException(400, "用户名已存在");
        }

        User user = new User();
        user.setUsername(req.getUsername());
        user.setPassword(passwordEncoder.encode(req.getPassword()));
        user.setNickname(req.getNickname());
        user.setQuotaLimit(defaultQuota);
        user.setQuotaUsed(0L);
        user.setVersion(0);
        userMapper.insert(user);

        String token = jwtUtil.generateToken(user.getId(), user.getUsername());
        return new LoginResponse(token, UserVO.from(user));
    }

    public LoginResponse login(LoginRequest req) {
        User user = userMapper.findByUsername(req.getUsername());
        if (user == null || !passwordEncoder.matches(req.getPassword(), user.getPassword())) {
            throw new BusinessException(400, "用户名或密码错误");
        }
        String token = jwtUtil.generateToken(user.getId(), user.getUsername());
        return new LoginResponse(token, UserVO.from(user));
    }

    public UserVO getById(Long id) {
        User user = userMapper.findById(id);
        if (user == null) {
            throw new BusinessException(404, "用户不存在");
        }
        return UserVO.from(user);
    }

    private boolean isBlank(String s) {
        return s == null || s.isBlank();
    }
}
