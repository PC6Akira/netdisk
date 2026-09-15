package com.netdisk.controller;

import com.netdisk.common.ApiResponse;
import com.netdisk.dto.CreateFolderRequest;
import com.netdisk.service.FileService;
import com.netdisk.vo.FileVO;
import jakarta.servlet.http.HttpServletRequest;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

/**
 * 文件夹接口：新建文件夹（受 JWT 保护）。
 */
@RestController
@RequestMapping("/api/folders")
public class FolderController {

    private final FileService fileService;

    public FolderController(FileService fileService) {
        this.fileService = fileService;
    }

    @PostMapping
    public ApiResponse<FileVO> create(HttpServletRequest request, @RequestBody CreateFolderRequest req) {
        Long userId = (Long) request.getAttribute("userId");
        return ApiResponse.ok(fileService.createFolder(userId, req));
    }
}
