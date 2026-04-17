//------------------------------------------------------------------------------
// Low-level socket abstraction
//
// This file is the only place in the nexus module that touches the native
// socket headers. Every other nexus source file reaches the operating system
// through the `net_os_*` API declared in `internal.h`.
//
// On POSIX the implementation forwards directly to BSD sockets. On Windows the
// implementation forwards to Winsock 2, which has the same C types but uses
// `SOCKET` handles, `closesocket`, `ioctlsocket`, `WSAPoll`, and
// `WSAGetLastError`.
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <nexus/internal.h>

#include <limits.h>
#include <stdio.h>

#if OS_WINDOWS
//
// Windows build notes:
//   - `WSAStartup` must be called before any socket call (`net_os_init()`).
//   - Link with `ws2_32` (e.g. add `-lws2_32` when clang-on-mingw, or
//     `#pragma comment(lib, "ws2_32.lib")` under MSVC). The build system does
//     not yet support platform-conditional `//> lib:` directives, so this
//     needs wiring up separately when a Windows build is attempted.
//
#    include <winsock2.h>
#    include <ws2tcpip.h>
#    pragma comment(lib, "ws2_32.lib")
typedef int       net_os_socklen_t;
typedef int       net_os_sendrecv_len_t;
typedef char      net_os_sockopt_t;
#    define NET_OS_SEND_FLAGS 0
#elif OS_POSIX
#    include <arpa/inet.h>
#    include <errno.h>
#    include <fcntl.h>
#    include <netinet/in.h>
#    include <poll.h>
#    include <sys/ioctl.h>
#    include <sys/socket.h>
#    include <unistd.h>
typedef socklen_t net_os_socklen_t;
typedef ssize_t   net_os_sendrecv_len_t;
typedef int       net_os_sockopt_t;
#    if defined(MSG_NOSIGNAL)
#        define NET_OS_SEND_FLAGS MSG_NOSIGNAL
#    else
#        define NET_OS_SEND_FLAGS 0
#    endif
#else
#    error "nexus lowlevel: unsupported platform"
#endif

//------------------------------------------------------------------------------
// Platform-native helpers

#if OS_WINDOWS

internal int _net_os_last_native_error(void) { return WSAGetLastError(); }

internal void _net_os_close_native(Net_Fd fd)
{
    closesocket((SOCKET)fd);
}

#elif OS_POSIX

internal int _net_os_last_native_error(void) { return errno; }

internal void _net_os_close_native(Net_Fd fd) { close((int)fd); }

#endif

//------------------------------------------------------------------------------
// net_os_init / net_os_done

internal bool _net_os_initialised = false;

void net_os_init(void)
{
    if (_net_os_initialised) {
        return;
    }

#if OS_WINDOWS
    WSADATA wsa_data;
    int     result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    ASSERT(result == 0, "WSAStartup failed");
#endif

    _net_os_initialised = true;
}

void net_os_done(void)
{
    if (!_net_os_initialised) {
        return;
    }

#if OS_WINDOWS
    WSACleanup();
#endif

    _net_os_initialised = false;
}

//------------------------------------------------------------------------------
// Error mapping

Net_OsError net_os_last_error(void)
{
    int err = _net_os_last_native_error();

#if OS_WINDOWS
    switch (err) {
    case 0:
        return NET_OS_OK;
    case WSAEWOULDBLOCK:
        return NET_OS_WOULD_BLOCK;
    case WSAEINTR:
        return NET_OS_INTR;
    case WSAENETDOWN:
        return NET_OS_NET_DOWN;
    case WSAECONNRESET:
        return NET_OS_CONN_RESET;
    case WSAECONNREFUSED:
        return NET_OS_CONN_REFUSED;
    case WSAETIMEDOUT:
        return NET_OS_TIMED_OUT;
    case WSAEHOSTUNREACH:
        return NET_OS_HOST_UNREACH;
    case WSAENETUNREACH:
        return NET_OS_NET_UNREACH;
    case WSAEADDRINUSE:
        return NET_OS_ADDR_IN_USE;
    case WSAEACCES:
        return NET_OS_ACCESS_DENIED;
    case WSAEMFILE:
        return NET_OS_OUT_OF_FD;
    case WSAESHUTDOWN:
    case WSAECONNABORTED:
        return NET_OS_PIPE_BROKEN;
    case WSAEPROTONOSUPPORT:
    case WSAEAFNOSUPPORT:
        return NET_OS_PROTO_NOT_SUPPORTED;
    case WSAENOTCONN:
    case WSAEDISCON:
        return NET_OS_CLOSED;
    default:
        return NET_OS_OTHER;
    }
#else
    switch (err) {
    case 0:
        return NET_OS_OK;
    case EAGAIN:
#    if EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:
#    endif
        return NET_OS_WOULD_BLOCK;
    case EINTR:
        return NET_OS_INTR;
    case ENETDOWN:
        return NET_OS_NET_DOWN;
    case ECONNRESET:
        return NET_OS_CONN_RESET;
    case ECONNREFUSED:
        return NET_OS_CONN_REFUSED;
    case ETIMEDOUT:
        return NET_OS_TIMED_OUT;
    case EHOSTUNREACH:
        return NET_OS_HOST_UNREACH;
    case ENETUNREACH:
        return NET_OS_NET_UNREACH;
    case EADDRINUSE:
        return NET_OS_ADDR_IN_USE;
    case EACCES:
        return NET_OS_ACCESS_DENIED;
    case EMFILE:
    case ENFILE:
        return NET_OS_OUT_OF_FD;
    case EPIPE:
        return NET_OS_PIPE_BROKEN;
    case EPROTONOSUPPORT:
    case EAFNOSUPPORT:
        return NET_OS_PROTO_NOT_SUPPORTED;
    case ENOTCONN:
        return NET_OS_CLOSED;
    default:
        return NET_OS_OTHER;
    }
#endif
}

void net_os_log_error(void)
{
#if DEBUG
    int err = _net_os_last_native_error();
#    if OS_WINDOWS
    prn("Network error: WSAGetLastError=%d", err);
#    else
    prn("Network error: %s", strerror(err));
#    endif
#else
    (void)0;
#endif
}

//------------------------------------------------------------------------------
// Poll-event translation

#if OS_WINDOWS
#    define NET_OS_POLL_FN WSAPoll
typedef WSAPOLLFD net_os_native_pollfd;
#else
#    define NET_OS_POLL_FN poll
typedef struct pollfd net_os_native_pollfd;
#endif

internal short _net_os_events_to_native(u16 events)
{
    short out = 0;
    if (events & NET_POLL_IN) {
        out |= POLLIN;
    }
    if (events & NET_POLL_OUT) {
        out |= POLLOUT;
    }
    return out;
}

internal u16 _net_os_events_from_native(short revents)
{
    u16 out = 0;
    if (revents & POLLIN) {
        out |= NET_POLL_IN;
    }
    if (revents & POLLOUT) {
        out |= NET_POLL_OUT;
    }
    if (revents & POLLERR) {
        out |= NET_POLL_ERR;
    }
    if (revents & POLLHUP) {
        out |= NET_POLL_HUP;
    }
    if (revents & POLLNVAL) {
        out |= NET_POLL_NVAL;
    }
    return out;
}

int net_os_poll(Net_PollFd* fds, usize n, int timeout_ms)
{
    if (n == 0) {
        return 0;
    }

    net_os_native_pollfd stack_fds[16];
    net_os_native_pollfd* native = stack_fds;
    net_os_native_pollfd* heap   = NULL;
    if (n > sizeof(stack_fds) / sizeof(stack_fds[0])) {
        heap = (net_os_native_pollfd*)mem_realloc(
            NULL, n * sizeof(net_os_native_pollfd), __FILE__, __LINE__);
        native = heap;
    }

    for (usize i = 0; i < n; ++i) {
#if OS_WINDOWS
        native[i].fd      = (SOCKET)fds[i].fd;
#else
        native[i].fd      = (int)fds[i].fd;
#endif
        native[i].events  = _net_os_events_to_native(fds[i].events);
        native[i].revents = 0;
    }

#if OS_WINDOWS
    int result = WSAPoll(native, (ULONG)n, timeout_ms);
#else
    int result = poll(native, (nfds_t)n, timeout_ms);
#endif

    for (usize i = 0; i < n; ++i) {
        fds[i].revents = _net_os_events_from_native(native[i].revents);
    }

    if (heap) {
        mem_free(heap, __FILE__, __LINE__);
    }
    return result;
}

//------------------------------------------------------------------------------
// Address conversions

internal void _net_os_addr_to_sockaddr(const Net_Addr*     addr,
                                       struct sockaddr_in* out)
{
    *out             = (struct sockaddr_in){0};
    out->sin_family  = AF_INET;
    out->sin_port    = htons(addr->port);
    memcpy(&out->sin_addr, addr->ip, NET_OS_IP4_BYTES);
}

internal void _net_os_addr_from_sockaddr(const struct sockaddr_in* in,
                                         Net_Addr*                 out)
{
    memcpy(out->ip, &in->sin_addr, NET_OS_IP4_BYTES);
    out->port = ntohs(in->sin_port);
}

void net_os_addr_from_endpoint(const Net_Endpoint* endpoint, Net_Addr* out_addr)
{
    memcpy(out_addr->ip, endpoint->ip, NET_OS_IP4_BYTES);
    out_addr->port = endpoint->port;
}

bool net_os_addr_equal(const Net_Addr* a, const Net_Addr* b)
{
    return a->port == b->port &&
           memcmp(a->ip, b->ip, NET_OS_IP4_BYTES) == 0;
}

bool net_os_addr_format_ip(const Net_Addr* addr, char* out_buf, usize buf_size)
{
    struct sockaddr_in sa;
    _net_os_addr_to_sockaddr(addr, &sa);
#if OS_WINDOWS
    if (InetNtopA(AF_INET, &sa.sin_addr, out_buf, buf_size) == NULL) {
        return false;
    }
#else
    if (!inet_ntop(AF_INET, &sa.sin_addr, out_buf, (socklen_t)buf_size)) {
        return false;
    }
#endif
    return true;
}

//------------------------------------------------------------------------------
// Byte-order helpers

u16 net_os_hton16(u16 value) { return htons(value); }
u32 net_os_hton32(u32 value) { return htonl(value); }
u16 net_os_ntoh16(u16 value) { return ntohs(value); }
u32 net_os_ntoh32(u32 value) { return ntohl(value); }

//------------------------------------------------------------------------------
// Socket creation / teardown

Net_Fd net_os_socket(Net_Protocol proto)
{
    net_os_init();

    int sock_type  = 0;
    int proto_type = 0;

    switch (proto) {
    case NET_PROTO_TCP:
        sock_type  = SOCK_STREAM;
        proto_type = IPPROTO_TCP;
        break;
    case NET_PROTO_UDP:
        sock_type  = SOCK_DGRAM;
        proto_type = IPPROTO_UDP;
        break;
    default:
        return NET_INVALID_FD;
    }

#if OS_WINDOWS
    SOCKET s = socket(AF_INET, sock_type, proto_type);
    if (s == INVALID_SOCKET) {
        return NET_INVALID_FD;
    }
    return (Net_Fd)s;
#else
    int s = socket(AF_INET, sock_type, proto_type);
    if (s < 0) {
        return NET_INVALID_FD;
    }
    return (Net_Fd)s;
#endif
}

void net_os_close(Net_Fd fd)
{
    if (fd == NET_INVALID_FD) {
        return;
    }
    _net_os_close_native(fd);
}

void net_os_shutdown_send(Net_Fd fd)
{
#if OS_WINDOWS
    shutdown((SOCKET)fd, SD_SEND);
#else
    shutdown((int)fd, SHUT_WR);
#endif
}

Net_OsError net_os_set_nonblocking(Net_Fd fd, bool nonblocking)
{
#if OS_WINDOWS
    u_long mode = nonblocking ? 1 : 0;
    if (ioctlsocket((SOCKET)fd, FIONBIO, &mode) != 0) {
        return net_os_last_error();
    }
#else
    int flags = fcntl((int)fd, F_GETFL, 0);
    if (flags < 0) {
        return net_os_last_error();
    }
    if (nonblocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    if (fcntl((int)fd, F_SETFL, flags) < 0) {
        return net_os_last_error();
    }
#endif
    return NET_OS_OK;
}

Net_OsError net_os_set_reuseaddr(Net_Fd fd)
{
    net_os_sockopt_t reuse = 1;
#if OS_WINDOWS
    if (setsockopt((SOCKET)fd,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &reuse,
                   sizeof(reuse)) != 0) {
        return net_os_last_error();
    }
#else
    if (setsockopt((int)fd,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &reuse,
                   sizeof(reuse)) != 0) {
        return net_os_last_error();
    }
#endif
    return NET_OS_OK;
}

Net_OsError net_os_available_bytes(Net_Fd fd, usize* out_available)
{
#if OS_WINDOWS
    u_long available = 0;
    if (ioctlsocket((SOCKET)fd, FIONREAD, &available) != 0) {
        return net_os_last_error();
    }
#else
    int available = 0;
    if (ioctl((int)fd, FIONREAD, &available) < 0) {
        return net_os_last_error();
    }
    if (available < 0) {
        available = 0;
    }
#endif
    *out_available = (usize)available;
    return NET_OS_OK;
}

//------------------------------------------------------------------------------
// Bind / listen / connect / accept

Net_OsError net_os_bind(Net_Fd fd, const Net_Addr* addr)
{
    struct sockaddr_in sa;
    _net_os_addr_to_sockaddr(addr, &sa);
#if OS_WINDOWS
    if (bind((SOCKET)fd, (struct sockaddr*)&sa, sizeof(sa)) == SOCKET_ERROR) {
        return net_os_last_error();
    }
#else
    if (bind((int)fd, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
        return net_os_last_error();
    }
#endif
    return NET_OS_OK;
}

Net_OsError net_os_listen(Net_Fd fd)
{
#if OS_WINDOWS
    if (listen((SOCKET)fd, SOMAXCONN) == SOCKET_ERROR) {
        return net_os_last_error();
    }
#else
    if (listen((int)fd, SOMAXCONN) < 0) {
        return net_os_last_error();
    }
#endif
    return NET_OS_OK;
}

Net_OsError net_os_connect(Net_Fd fd, const Net_Addr* addr)
{
    struct sockaddr_in sa;
    _net_os_addr_to_sockaddr(addr, &sa);
#if OS_WINDOWS
    if (connect((SOCKET)fd, (struct sockaddr*)&sa, sizeof(sa)) ==
        SOCKET_ERROR) {
        return net_os_last_error();
    }
#else
    if (connect((int)fd, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
        return net_os_last_error();
    }
#endif
    return NET_OS_OK;
}

Net_OsError net_os_accept(Net_Fd fd, Net_Fd* out_client)
{
#if OS_WINDOWS
    SOCKET client = accept((SOCKET)fd, NULL, NULL);
    if (client == INVALID_SOCKET) {
        *out_client = NET_INVALID_FD;
        return net_os_last_error();
    }
    *out_client = (Net_Fd)client;
#else
    int client = accept((int)fd, NULL, NULL);
    if (client < 0) {
        *out_client = NET_INVALID_FD;
        return net_os_last_error();
    }
    *out_client = (Net_Fd)client;
#endif
    return NET_OS_OK;
}

Net_OsError net_os_getpeername(Net_Fd fd, Net_Addr* out_addr)
{
    struct sockaddr_in sa;
    net_os_socklen_t   sa_len = sizeof(sa);
#if OS_WINDOWS
    if (getpeername((SOCKET)fd, (struct sockaddr*)&sa, &sa_len) ==
        SOCKET_ERROR) {
        return net_os_last_error();
    }
#else
    if (getpeername((int)fd, (struct sockaddr*)&sa, &sa_len) < 0) {
        return net_os_last_error();
    }
#endif
    _net_os_addr_from_sockaddr(&sa, out_addr);
    return NET_OS_OK;
}

//------------------------------------------------------------------------------
// I/O

internal int _net_os_recv_flags_to_native(u32 flags)
{
    int out = 0;
    if (flags & NET_OS_RECV_PEEK) {
        out |= MSG_PEEK;
    }
#if OS_POSIX && defined(MSG_DONTWAIT)
    if (flags & NET_OS_RECV_DONTWAIT) {
        out |= MSG_DONTWAIT;
    }
#endif
#if defined(MSG_TRUNC)
    if (flags & NET_OS_RECV_TRUNC) {
        out |= MSG_TRUNC;
    }
#endif
    return out;
}

isize net_os_send(Net_Fd fd, const void* buffer, usize len)
{
#if OS_WINDOWS
    int sent =
        send((SOCKET)fd, (const char*)buffer, (int)len, NET_OS_SEND_FLAGS);
    if (sent == SOCKET_ERROR) {
        return -1;
    }
    return (isize)sent;
#else
    ssize_t sent = send((int)fd, buffer, len, NET_OS_SEND_FLAGS);
    return (isize)sent;
#endif
}

isize net_os_recv(Net_Fd fd, void* buffer, usize len, u32 flags)
{
    int native_flags = _net_os_recv_flags_to_native(flags);
#if OS_WINDOWS
    int received = recv((SOCKET)fd, (char*)buffer, (int)len, native_flags);
    if (received == SOCKET_ERROR) {
        return -1;
    }
    return (isize)received;
#else
    ssize_t received = recv((int)fd, buffer, len, native_flags);
    return (isize)received;
#endif
}

isize net_os_sendto(Net_Fd          fd,
                    const void*     buffer,
                    usize           len,
                    const Net_Addr* addr)
{
    struct sockaddr_in sa;
    _net_os_addr_to_sockaddr(addr, &sa);
#if OS_WINDOWS
    int sent = sendto((SOCKET)fd,
                      (const char*)buffer,
                      (int)len,
                      NET_OS_SEND_FLAGS,
                      (struct sockaddr*)&sa,
                      sizeof(sa));
    if (sent == SOCKET_ERROR) {
        return -1;
    }
    return (isize)sent;
#else
    ssize_t sent = sendto((int)fd,
                          buffer,
                          len,
                          NET_OS_SEND_FLAGS,
                          (struct sockaddr*)&sa,
                          sizeof(sa));
    return (isize)sent;
#endif
}

isize net_os_recvfrom(
    Net_Fd fd, void* buffer, usize len, u32 flags, Net_Addr* out_addr)
{
    struct sockaddr_in sa;
    net_os_socklen_t   sa_len       = sizeof(sa);
    int                native_flags = _net_os_recv_flags_to_native(flags);
    memset(&sa, 0, sizeof(sa));
#if OS_WINDOWS
    int received = recvfrom((SOCKET)fd,
                            (char*)buffer,
                            (int)len,
                            native_flags,
                            (struct sockaddr*)&sa,
                            &sa_len);
    if (received == SOCKET_ERROR) {
        return -1;
    }
#else
    ssize_t received = recvfrom((int)fd,
                                buffer,
                                len,
                                native_flags,
                                (struct sockaddr*)&sa,
                                &sa_len);
    if (received < 0) {
        return -1;
    }
#endif
    if (out_addr) {
        _net_os_addr_from_sockaddr(&sa, out_addr);
    }
    return (isize)received;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
