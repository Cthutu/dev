//------------------------------------------------------------------------------
// Log module
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <log/log.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>

#if OS_WINDOWS
#    include <direct.h>
#    include <io.h>
typedef int log_ssize_t;
#else
#    include <unistd.h>
typedef ssize_t log_ssize_t;
#endif

#if OS_WINDOWS
#    define LOG_OPEN _open
#    define LOG_READ _read
#    define LOG_WRITE _write
#    define LOG_CLOSE _close
#    define LOG_UNLINK _unlink
#    define LOG_MKDIR(path) _mkdir(path)
#    define LOG_OPEN_FLAGS (O_BINARY)
#else
#    define LOG_OPEN open
#    define LOG_READ read
#    define LOG_WRITE write
#    define LOG_CLOSE close
#    define LOG_UNLINK unlink
#    define LOG_MKDIR(path) mkdir((path), 0777)
#    define LOG_OPEN_FLAGS (0)
#endif

internal bool _log_write_all(int fd, const u8* data, usize len)
{
    while (len > 0) {
        log_ssize_t written = LOG_WRITE(fd, data, len);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }

        data += written;
        len -= (usize)written;
    }

    return true;
}

internal bool _log_repo_tmp_dir(char* buffer, usize buffer_size)
{
    cstr source = __FILE__;
    cstr src_marker = "/src/log/log.c";
    cstr src_marker_windows = "\\src\\log\\log.c";
    cstr marker = strstr(source, src_marker);

    if (!marker) {
        marker = strstr(source, src_marker_windows);
        src_marker = src_marker_windows;
    }

    if (!marker) {
        return false;
    }

    usize root_len = (usize)(marker - source);
    int len = snprintf(buffer,
                       buffer_size,
                       "%.*s%c_tmp",
                       (int)root_len,
                       source,
#if OS_WINDOWS
                       '\\'
#else
                       '/'
#endif
    );

    return len >= 0 && (usize)len < buffer_size;
}

internal bool _log_ensure_path(Log* log)
{
    char tmp_dir[sizeof(log->path)] = {0};

    if (!log) {
        return false;
    }

    if (!_log_repo_tmp_dir(tmp_dir, sizeof(tmp_dir))) {
        return false;
    }

    if (LOG_MKDIR(tmp_dir) < 0 && errno != EEXIST) {
        return false;
    }

    if (log->path[0]) {
        return true;
    }

#if OS_WINDOWS
    UINT result = GetTempFileNameA(tmp_dir, "log", 0, log->path);
    if (result == 0) {
        log->path[0] = 0;
        return false;
    }
#else
    cstr name = "log";
    int  len  = snprintf(log->path,
                         sizeof(log->path),
                         "%s/%s-XXXXXX",
                         tmp_dir,
                         name);
    if (len < 0 || (usize)len >= sizeof(log->path)) {
        log->path[0] = 0;
        return false;
    }

    int fd = mkstemp(log->path);
    if (fd < 0) {
        log->path[0] = 0;
        return false;
    }

    LOG_CLOSE(fd);
#endif
    return true;
}

bool log_append(Log* log, const void* data, usize len)
{
    if (!log || (!data && len > 0) || !_log_ensure_path(log)) {
        return false;
    }

    int fd = LOG_OPEN(log->path, O_WRONLY | O_APPEND | O_CREAT | LOG_OPEN_FLAGS, 0666);
    if (fd < 0) {
        return false;
    }

    bool ok = _log_write_all(fd, data, len);
    LOG_CLOSE(fd);
    return ok;
}

bool log_append_cstr(Log* log, cstr text)
{
    if (!text) {
        return false;
    }

    return log_append(log, text, strlen(text));
}

bool log_formatv(Log* log, cstr fmt, va_list args)
{
    if (!log || !fmt) {
        return false;
    }

    va_list copy;
    va_copy(copy, args);
    int length = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (length < 0) {
        return false;
    }

    usize needed = (usize)length + 1;
    char* buffer = mem_realloc(NULL, needed, __FILE__, __LINE__);
    vsnprintf(buffer, needed, fmt, args);

    bool ok = log_append(log, buffer, (usize)length);
    mem_free(buffer, __FILE__, __LINE__);
    return ok;
}

bool log_format(Log* log, cstr fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    bool ok = log_formatv(log, fmt, args);
    va_end(args);
    return ok;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
