package com.netdisk.controller;

import com.netdisk.common.ApiResponse;
import com.netdisk.dto.CompleteUploadRequest;
import com.netdisk.dto.InitUploadRequest;
import com.netdisk.service.ChunkUploadService;
import com.netdisk.vo.FileVO;
import com.netdisk.vo.InitUploadVO;
import jakarta.servlet.http.HttpServletRequest;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.multipart.MultipartFile;

/**
 * 分片上传接口：创建会话、上传分片、查询已传分片（断点续传）、合并完成（受 JWT 保护）。
 */
@RestController
@RequestMapping("/api/upload")
public class UploadController {

    private final ChunkUploadService chunkUploadService;

    public UploadController(ChunkUploadService chunkUploadService) {
        this.chunkUploadService = chunkUploadService;
    }

    @PostMapping("/init")
    public ApiResponse<InitUploadVO> init(HttpServletRequest request, @RequestBody InitUploadRequest req) {
        Long userId = (Long) request.getAttribute("userId");
        return ApiResponse.ok(chunkUploadService.init(userId, req));
    }

    @PostMapping("/chunk")
    public ApiResponse<Void> chunk(HttpServletRequest request,
                                   @RequestParam Long sessionId,
                                   @RequestParam int chunkIndex,
                                   @RequestParam("file") MultipartFile file) {
        Long userId = (Long) request.getAttribute("userId");
        chunkUploadService.uploadChunk(userId, sessionId, chunkIndex, file);
        return ApiResponse.ok(null);
    }

    @GetMapping("/status")
    public ApiResponse<InitUploadVO> status(HttpServletRequest request, @RequestParam Long sessionId) {
        Long userId = (Long) request.getAttribute("userId");
        return ApiResponse.ok(chunkUploadService.status(userId, sessionId));
    }

    @PostMapping("/complete")
    public ApiResponse<FileVO> complete(HttpServletRequest request, @RequestBody CompleteUploadRequest req) {
        Long userId = (Long) request.getAttribute("userId");
        return ApiResponse.ok(chunkUploadService.complete(userId, req));
    }
}
