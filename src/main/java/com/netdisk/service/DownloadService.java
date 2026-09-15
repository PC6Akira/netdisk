package com.netdisk.service;

import com.netdisk.common.BusinessException;
import com.netdisk.entity.File;
import com.netdisk.mapper.FileMapper;
import org.springframework.core.io.FileSystemResource;
import org.springframework.http.ContentDisposition;
import org.springframework.http.HttpHeaders;
import org.springframework.http.HttpRange;
import org.springframework.http.HttpStatus;
import org.springframework.http.MediaType;
import org.springframework.http.MediaTypeFactory;
import org.springframework.http.ResponseEntity;
import org.springframework.stereotype.Service;
import org.springframework.web.servlet.mvc.method.annotation.StreamingResponseBody;

import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.List;

/**
 * 下载/预览服务：流式输出文件，支持 Range（206 断点下载、视频拖动）。
 * 用 StreamingResponseBody 分块读写，不把文件整体读入内存。
 */
@Service
public class DownloadService {

    private static final int BUFFER_SIZE = 8192;

    private final FileMapper fileMapper;
    private final StorageService storageService;

    public DownloadService(FileMapper fileMapper, StorageService storageService) {
        this.fileMapper = fileMapper;
        this.storageService = storageService;
    }

    public ResponseEntity<StreamingResponseBody> download(Long userId, Long id, String rangeHeader) {
        return stream(userId, id, rangeHeader, false);
    }

    public ResponseEntity<StreamingResponseBody> preview(Long userId, Long id, String rangeHeader) {
        return stream(userId, id, rangeHeader, true);
    }

    private ResponseEntity<StreamingResponseBody> stream(Long userId, Long id, String rangeHeader, boolean inline) {
        File f = fileMapper.findById(id);
        if (f == null || !f.getUserId().equals(userId)) {
            throw new BusinessException(404, "文件不存在");
        }
        if (f.getIsDir() == null || f.getIsDir() == 1) {
            throw new BusinessException(400, "文件夹不支持下载/预览");
        }
        if (f.getStorageKey() == null) {
            throw new BusinessException(404, "文件内容不存在");
        }
        FileSystemResource resource = new FileSystemResource(storageService.resolve(f.getStorageKey()));
        if (!resource.exists() || !resource.isReadable()) {
            throw new BusinessException(404, "文件内容不存在");
        }

        MediaType mediaType = MediaTypeFactory.getMediaType(f.getName())
                .orElse(MediaType.APPLICATION_OCTET_STREAM);
        long total = f.getSize() == null ? 0 : f.getSize();

        long start = 0;
        long end = total - 1;
        boolean partial = false;
        if (rangeHeader != null && !rangeHeader.isBlank()) {
            List<HttpRange> ranges = HttpRange.parseRanges(rangeHeader);
            if (!ranges.isEmpty()) {
                HttpRange range = ranges.get(0);
                start = range.getRangeStart(total);
                end = range.getRangeEnd(total);
                partial = true;
            }
        }
        long length = end - start + 1;

        HttpHeaders headers = new HttpHeaders();
        headers.setContentType(mediaType);
        headers.set(HttpHeaders.ACCEPT_RANGES, "bytes");
        headers.setContentLength(length);
        if (partial) {
            headers.set(HttpHeaders.CONTENT_RANGE, "bytes " + start + "-" + end + "/" + total);
        }
        ContentDisposition disposition = (inline ? ContentDisposition.inline() : ContentDisposition.attachment())
                .filename(f.getName(), StandardCharsets.UTF_8).build();
        headers.setContentDisposition(disposition);

        final long startPos = start;
        final long len = length;

        StreamingResponseBody body = outputStream -> {
            try (InputStream in = resource.getInputStream()) {
                long skipped = 0;
                while (skipped < startPos) {
                    long s = in.skip(startPos - skipped);
                    if (s <= 0) {
                        break;
                    }
                    skipped += s;
                }
                byte[] buf = new byte[BUFFER_SIZE];
                long remaining = len;
                int n;
                while (remaining > 0 && (n = in.read(buf, 0, (int) Math.min(buf.length, remaining))) != -1) {
                    outputStream.write(buf, 0, n);
                    remaining -= n;
                }
            }
        };

        return ResponseEntity.status(partial ? HttpStatus.PARTIAL_CONTENT : HttpStatus.OK)
                .headers(headers)
                .body(body);
    }
}
