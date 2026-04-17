//> use: core log

#include <core/core.h>
#include <log/log.h>
#include <test.h>

#include <fcntl.h>

#if OS_WINDOWS
#    include <io.h>
typedef int log_test_ssize_t;
#    define LOG_TEST_OPEN _open
#    define LOG_TEST_READ _read
#    define LOG_TEST_CLOSE _close
#    define LOG_TEST_UNLINK _unlink
#    define LOG_TEST_OPEN_FLAGS (O_BINARY)
#else
#    include <unistd.h>
typedef ssize_t log_test_ssize_t;
#    define LOG_TEST_OPEN open
#    define LOG_TEST_READ read
#    define LOG_TEST_CLOSE close
#    define LOG_TEST_UNLINK unlink
#    define LOG_TEST_OPEN_FLAGS (0)
#endif

internal bool _log_test_read_all(cstr path, char* buffer, usize buffer_size)
{
    int fd = LOG_TEST_OPEN(path, O_RDONLY | LOG_TEST_OPEN_FLAGS);
    if (fd < 0) {
        return false;
    }

    usize total = 0;
    while (total + 1 < buffer_size) {
        log_test_ssize_t read_len =
            LOG_TEST_READ(fd, buffer + total, buffer_size - total - 1);
        if (read_len < 0) {
            LOG_TEST_CLOSE(fd);
            return false;
        }
        if (read_len == 0) {
            break;
        }

        total += (usize)read_len;
    }

    buffer[total] = 0;
    LOG_TEST_CLOSE(fd);
    return true;
}

TEST_CASE(log, temp_file_can_be_created_and_appended)
{
    Log log = {0};

    TEST_ASSERT(log_append_cstr(&log, "hello"));
    TEST_ASSERT(log.path[0] != 0);
    TEST_ASSERT(log_append(&log, " ", 1));
    TEST_ASSERT(log_format(&log, "%s %d", "world", 42));

    char path[sizeof(log.path)];
    memcpy(path, log.path, sizeof(path));

    char contents[64];
    TEST_ASSERT(_log_test_read_all(path, contents, sizeof(contents)));
    TEST_ASSERT_STR_EQ(contents, "hello world 42");

    TEST_ASSERT_EQ(LOG_TEST_UNLINK(path), 0);
}
