package com.netdisk.controller;

import com.netdisk.common.ApiResponse;
import com.netdisk.dto.RenameRequest;
import com.netdisk.service.FileService;
import com.netdisk.service.UploadService;
import com.netdisk.vo.FileVO;
import jakarta.servlet.http.HttpServletRequest;
import org.springframework.web.bind.annotation.DeleteMapping;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.multipart.MultipartFile;

import java.util.List;

/**
 * 文件接口：列表、上传、重命名、删除（受 JWT 保护）。
 */
@RestController
@RequestMapping("/api/files")
public class FileController {

    private final FileService fileService;
    private final UploadService uploadService;

    public FileController(FileService fileService, UploadService uploadService) {
        this.fileService = fileService;
        this.uploadService = uploadService;
    }

    @GetMapping
    public ApiResponse<List<FileVO>> list(HttpServletRequest request,
                                          @RequestParam(required = false) Long parentId) {
        Long userId = (Long) request.getAttribute("userId");
        return ApiResponse.ok(fileService.list(userId, parentId));
    }

    @PostMapping("/upload")
    public ApiResponse<FileVO> upload(HttpServletRequest request,
                                      @RequestParam(required = false) Long parentId,
                                      @RequestParam("file") MultipartFile file) {
        Long userId = (Long) request.getAttribute("userId");
        return ApiResponse.ok(uploadService.upload(userId, parentId, file));
    }

    @PostMapping("/{id}/rename")
    public ApiResponse<FileVO> rename(HttpServletRequest request,
                                      @PathVariable Long id,
                                      @RequestBody RenameRequest req) {
        Long userId = (Long) request.getAttribute("userId");
        return ApiResponse.ok(fileService.rename(userId, id, req));
    }

    @DeleteMapping("/{id}")
    public ApiResponse<Void> delete(HttpServletRequest request, @PathVariable Long id) {
        Long userId = (Long) request.getAttribute("userId");
        fileService.delete(userId, id);
        return ApiResponse.ok(null);
    }
}
