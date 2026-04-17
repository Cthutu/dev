//------------------------------------------------------------------------------
// Nexus test networking helpers
//------------------------------------------------------------------------------

#pragma once

#include <core/core.h>

#if OS_WINDOWS
#    include <winsock2.h>
#    include <ws2tcpip.h>
typedef SOCKET nexus_test_fd_t;
typedef int    nexus_test_ssize_t;
typedef int    nexus_test_socklen_t;
#    define NEXUS_TEST_INVALID_FD INVALID_SOCKET
#else
#    include <arpa/inet.h>
#    include <netinet/in.h>
#    include <sys/socket.h>
#    include <unistd.h>
typedef int       nexus_test_fd_t;
typedef ssize_t   nexus_test_ssize_t;
typedef socklen_t nexus_test_socklen_t;
#    define NEXUS_TEST_INVALID_FD (-1)
#endif

#if OS_WINDOWS
internal void _nexus_test_net_init(void)
{
    local_persist bool initialised = false;
    if (!initialised) {
        WSADATA wsa_data;
        ASSERT(WSAStartup(MAKEWORD(2, 2), &wsa_data) == 0,
               "Failed to initialise Winsock for nexus tests");
        initialised = true;
    }
}
#else
internal void _nexus_test_net_init(void) {}
#endif

internal int _nexus_test_setsockopt_reuseaddr(nexus_test_fd_t fd)
{
    int one = 1;
#if OS_WINDOWS
    return setsockopt(
        fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&one, sizeof(one));
#else
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
#endif
}

internal void _nexus_test_close_fd(nexus_test_fd_t fd)
{
#if OS_WINDOWS
    closesocket(fd);
#else
    close(fd);
#endif
}

internal u16 _nexus_choose_test_port(int sock_type, int proto)
{
    _nexus_test_net_init();

    nexus_test_fd_t fd = socket(AF_INET, sock_type, proto);
    ASSERT(fd != NEXUS_TEST_INVALID_FD, "Failed to create test socket");

    ASSERT(_nexus_test_setsockopt_reuseaddr(fd) == 0,
           "Failed to set SO_REUSEADDR");

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
        .sin_port        = 0,
    };

    ASSERT(bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0,
           "Failed to bind test socket");

    nexus_test_socklen_t addr_len = sizeof(addr);
    ASSERT(getsockname(fd, (struct sockaddr*)&addr, &addr_len) == 0,
           "Failed to query test socket port");

    _nexus_test_close_fd(fd);
    return ntohs(addr.sin_port);
}

internal bool _nexus_test_parse_loopback(struct sockaddr_in* out_addr)
{
    _nexus_test_net_init();
    return inet_pton(AF_INET, "127.0.0.1", &out_addr->sin_addr) == 1;
}
