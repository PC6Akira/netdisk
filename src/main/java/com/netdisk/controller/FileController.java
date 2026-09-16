package com.netdisk.controller;

import com.netdisk.common.ApiResponse;
import com.netdisk.dto.InstantUploadRequest;
import com.netdisk.dto.RenameRequest;
import com.netdisk.service.DownloadService;
import com.netdisk.service.FileService;
import com.netdisk.service.UploadService;
import com.netdisk.vo.FileVO;
import com.netdisk.vo.InstantUploadVO;
import jakarta.servlet.http.HttpServletRequest;
import org.springframework.http.ResponseEntity;
import org.springframework.web.servlet.mvc.method.annotation.StreamingResponseBody;
import org.springframework.web.bind.annotation.DeleteMapping;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestHeader;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.multipart.MultipartFile;

import java.util.List;

/**
 * 文件接口：列表、上传、下载、预览、重命名、删除（受 JWT 保护）。
 */
@RestController
@RequestMapping("/api/files")
public class FileController {

    private final FileService fileService;
    private final UploadService uploadService;
    private final DownloadService downloadService;

    public FileController(FileService fileService, UploadService uploadService, DownloadService downloadService) {
        this.fileService = fileService;
        this.uploadService = uploadService;
        this.downloadService = downloadService;
    }

    @GetMapping
    public ApiResponse<List<FileVO>> list(HttpServletRequest request,
                                          @RequestParam(required = false) Long parentId) {
        Long userId = (Long) request.getAttribute("userId");
        return ApiResponse.ok(fileService.list(userId, parentId));
    }

    @PostMapping("/instant")
    public ApiResponse<InstantUploadVO> instant(HttpServletRequest request,
                                                @RequestBody InstantUploadRequest req) {
        Long userId = (Long) request.getAttribute("userId");
        return ApiResponse.ok(uploadService.instant(userId, req));
    }

    @PostMapping("/upload")
    public ApiResponse<FileVO> upload(HttpServletRequest request,
                                      @RequestParam(required = false) Long parentId,
                                      @RequestParam("file") MultipartFile file,
                                      @RequestParam(required = false) String md5) {
        Long userId = (Long) request.getAttribute("userId");
        return ApiResponse.ok(uploadService.upload(userId, parentId, file, md5));
    }

    @GetMapping("/{id}/download")
    public ResponseEntity<StreamingResponseBody> download(HttpServletRequest request,
                                      @PathVariable Long id,
                                      @RequestHeader(value = "Range", required = false) String range) {
        Long userId = (Long) request.getAttribute("userId");
        return downloadService.download(userId, id, range);
    }

    @GetMapping("/{id}/preview")
    public ResponseEntity<StreamingResponseBody> preview(HttpServletRequest request,
                                     @PathVariable Long id,
                                     @RequestHeader(value = "Range", required = false) String range) {
        Long userId = (Long) request.getAttribute("userId");
        return downloadService.preview(userId, id, range);
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
