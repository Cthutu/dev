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
#include <unistd.h>

internal bool _log_write_all(int fd, const u8* data, usize len)
{
    while (len > 0) {
        ssize_t written = write(fd, data, len);
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

internal bool _log_ensure_path(Log* log)
{
    if (!log) {
        return false;
    }

    if (mkdir("_tmp", 0777) < 0 && errno != EEXIST) {
        return false;
    }

    if (log->path[0]) {
        return true;
    }

    cstr name = "log";
    int  len  = snprintf(log->path,
                        sizeof(log->path),
                        "%s/%s-XXXXXX",
                        "_tmp",
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

    close(fd);
    return true;
}

bool log_append(Log* log, const void* data, usize len)
{
    if (!log || (!data && len > 0) || !_log_ensure_path(log)) {
        return false;
    }

    int fd = open(log->path, O_WRONLY | O_APPEND | O_CREAT, 0666);
    if (fd < 0) {
        return false;
    }

    bool ok = _log_write_all(fd, data, len);
    close(fd);
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
