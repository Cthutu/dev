//------------------------------------------------------------------------------
// TCP transport
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <nexus/internal.h>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

//------------------------------------------------------------------------------
// _net_tcp_transport_ops
//
// Transport operations for TCP sockets.
//------------------------------------------------------------------------------

internal const Net_TransportOps _net_tcp_transport_ops = {
    .send         = _net_tcp_send,
    .recv_message = _net_tcp_recv_message,
};

#define NET_DEFAULT_CONNECT_RETRY_INTERVAL_MS 50ull

//------------------------------------------------------------------------------
// _net_timeout_deadline_from_option
//
// Converts a millisecond timeout option into an absolute deadline. A deadline
// of zero means the operation may wait forever.
//------------------------------------------------------------------------------

TimePoint _net_timeout_deadline_from_option(u64 timeout_ms)
{
    if (timeout_ms == NET_WAIT_INFINITE) {
        return 0;
    }

    return time_add_duration(time_now(), time_from_ms(timeout_ms));
}

bool _net_socket_nonblocking(Net_Socket* sock)
{
    return _net_socket_data(sock)->options.nonblocking != 0;
}

//------------------------------------------------------------------------------
// _net_timeout_poll_ms
//
// Converts an absolute deadline into the integer timeout expected by `poll()`.
//------------------------------------------------------------------------------

int _net_timeout_poll_ms(TimePoint deadline)
{
    if (deadline == 0) {
        return -1;
    }

    TimePoint now = time_now();
    if (now >= deadline) {
        return 0;
    }

    u64 remaining_ms = time_duration_to_ms(time_elapsed(now, deadline));
    return (int)MIN(remaining_ms, (u64)INT_MAX);
}

//------------------------------------------------------------------------------
// _net_poll_fd
//
// Waits for the requested readiness on one file descriptor until the deadline
// expires.
//------------------------------------------------------------------------------

Net_Result
_net_poll_fd(int fd, short events, TimePoint deadline, bool nonblocking)
{
    struct pollfd poll_fd = {
        .fd     = fd,
        .events = events,
    };

    while (true) {
        int poll_result = poll(&poll_fd, 1, _net_timeout_poll_ms(deadline));
        if (poll_result == 0) {
            return nonblocking ? NET_WOULD_BLOCK : NET_TIMEOUT;
        }

        if (poll_result < 0) {
            if (errno == EINTR) {
                continue;
            }

            _net_log_error();
            switch (errno) {
            case ENETDOWN:
                return NET_NO_NETWORK;
            default:
                return NET_ERROR;
            }
        }

        if (poll_fd.revents & events) {
            return NET_OK;
        }

        if (poll_fd.revents & POLLHUP) {
            return NET_CLOSED;
        }

        if (poll_fd.revents & (POLLERR | POLLNVAL)) {
            return NET_ERROR;
        }
    }
}

//------------------------------------------------------------------------------
// _net_tcp_send_all_fd
//
// Writes the full buffer to the given TCP file descriptor, retrying partial
// writes until the requested byte count has been sent or an error occurs.
//------------------------------------------------------------------------------

Net_Result _net_tcp_send_all_fd(
    int fd, const u8* buffer, usize len, TimePoint deadline, bool nonblocking)
{
    usize sent  = 0;
    int   flags = 0;

#if defined(MSG_NOSIGNAL)
    flags |= MSG_NOSIGNAL;
#endif

    while (sent < len) {
        Net_Result wait_result =
            _net_poll_fd(fd, POLLOUT, deadline, nonblocking);
        if (NET_FAILED(wait_result)) {
            return wait_result;
        }

        ssize_t result = send(fd, buffer + sent, len - sent, flags);
        if (result < 0) {
            _net_log_error();
            switch (errno) {
            case ENETDOWN:
                return NET_NO_NETWORK;
            case EPIPE:
            case ECONNRESET:
                return NET_CLOSED;
            default:
                return NET_ERROR;
            }
        }

        if (result == 0) {
            return NET_CLOSED;
        }

        sent += (usize)result;
    }

    return NET_OK;
}

//------------------------------------------------------------------------------
// _net_tcp_connect_retry_interval_ms
//
// Chooses the retry delay for waiting connects. A zero-valued reconnect option
// means "use Nexus' default retry interval" rather than busy-spinning.
//------------------------------------------------------------------------------

internal u64 _net_tcp_connect_retry_interval_ms(Net_Socket* sock)
{
    Net_SocketData* data = _net_socket_data_ensure(sock);
    if (data->options.reconnect_interval_ms > 0) {
        return data->options.reconnect_interval_ms;
    }

    return NET_DEFAULT_CONNECT_RETRY_INTERVAL_MS;
}

//------------------------------------------------------------------------------
// _net_tcp_should_retry_connect
//
// Returns true for the connection errors that are expected while waiting for a
// server to bind and begin listening.
//------------------------------------------------------------------------------

internal bool _net_tcp_should_retry_connect(int err)
{
    switch (err) {
    case ECONNREFUSED:
    case ETIMEDOUT:
    case EHOSTUNREACH:
    case ENETUNREACH:
        return true;
    default:
        return false;
    }
}

internal Net_Result _net_tcp_available_bytes(int fd, usize* out_available)
{
    int available = 0;
    if (ioctl(fd, FIONREAD, &available) < 0) {
        _net_log_error();
        return NET_ERROR;
    }

    *out_available = (usize)MAX(available, 0);
    return NET_OK;
}

internal Net_Result _net_tcp_peek_frame_length(int    fd,
                                               usize* out_frame_len,
                                               bool   nonblocking)
{
    u32     header = 0;
    ssize_t peeked = recv(fd, &header, sizeof(header), MSG_PEEK | MSG_DONTWAIT);
    if (peeked == 0) {
        return NET_CLOSED;
    }

    if (peeked < 0) {
        switch (errno) {
        case EAGAIN:
#if EWOULDBLOCK != EAGAIN
        case EWOULDBLOCK:
#endif
            return nonblocking ? NET_WOULD_BLOCK : NET_TIMEOUT;
        case ENETDOWN:
            return NET_NO_NETWORK;
        case ECONNRESET:
            return NET_CLOSED;
        default:
            _net_log_error();
            return NET_ERROR;
        }
    }

    if ((usize)peeked < sizeof(header)) {
        return nonblocking ? NET_WOULD_BLOCK : NET_TIMEOUT;
    }

    *out_frame_len = (usize)ntohl(header);
    return NET_OK;
}

internal Net_Result _net_tcp_wait_for_full_frame(Net_Socket* sock,
                                                 int         fd,
                                                 TimePoint   deadline,
                                                 usize*      out_frame_len)
{
    bool nonblocking = _net_socket_nonblocking(sock);

    while (true) {
        Net_Result wait_result =
            _net_poll_fd(fd, POLLIN, deadline, nonblocking);
        if (NET_FAILED(wait_result)) {
            return wait_result;
        }

        usize      frame_len = 0;
        Net_Result result =
            _net_tcp_peek_frame_length(fd, &frame_len, nonblocking);
        if (result == NET_WOULD_BLOCK || result == NET_TIMEOUT) {
            if (nonblocking) {
                return NET_WOULD_BLOCK;
            }
            continue;
        }
        if (NET_FAILED(result)) {
            return result;
        }

        usize available = 0;
        result          = _net_tcp_available_bytes(fd, &available);
        if (NET_FAILED(result)) {
            return result;
        }

        usize needed = sizeof(u32) + frame_len;
        if (available < needed) {
            if (nonblocking) {
                return NET_WOULD_BLOCK;
            }
            continue;
        }

        *out_frame_len = frame_len;
        return NET_OK;
    }
}

//------------------------------------------------------------------------------
// _net_tcp_recv_exact_fd
//
// Reads exactly the requested number of bytes from a TCP file descriptor. This
// is the primitive used to assemble framed messages from the stream transport.
//------------------------------------------------------------------------------

internal Net_Result _net_tcp_recv_exact_fd(int       fd,
                                           void*     buffer,
                                           usize     len,
                                           TimePoint deadline)
{
    u8*   out       = buffer;
    usize recv_size = 0;

    while (recv_size < len) {
        Net_Result wait_result = _net_poll_fd(fd, POLLIN, deadline, false);
        if (NET_FAILED(wait_result)) {
            return wait_result;
        }

        ssize_t result = recv(fd, out + recv_size, len - recv_size, 0);
        if (result < 0) {
            _net_log_error();
            switch (errno) {
            case ENETDOWN:
                return NET_NO_NETWORK;
            case ECONNRESET:
                return NET_CLOSED;
            default:
                return NET_ERROR;
            }
        }

        if (result == 0) {
            return NET_CLOSED;
        }

        recv_size += (usize)result;
    }

    return NET_OK;
}

//------------------------------------------------------------------------------
// _net_tcp_discard_exact_fd
//
// Discards a fixed number of bytes from the TCP stream. This is used to keep
// the stream in sync when we detect an oversized or otherwise invalid frame.
//------------------------------------------------------------------------------

internal Net_Result _net_tcp_discard_exact_fd(int       fd,
                                              usize     len,
                                              TimePoint deadline)
{
    u8    discard[4096];
    usize remaining = len;

    while (remaining > 0) {
        usize      chunk  = MIN(remaining, sizeof(discard));
        usize      before = remaining;
        // Reuse a small stack buffer because the discarded bytes are not needed
        // after they have been consumed from the stream.
        Net_Result result =
            _net_tcp_recv_exact_fd(fd, discard, chunk, deadline);
        if (NET_FAILED(result)) {
            return result;
        }
        remaining = before - chunk;
    }

    return NET_OK;
}

//------------------------------------------------------------------------------
// _net_tcp_send_framed_fd
//
// Sends one framed TCP message via the provided file descriptor.
//------------------------------------------------------------------------------

Net_Result _net_tcp_send_framed_fd(int fd, const void* buffer, usize len)
{
    TimePoint deadline  = 0;
    u32       frame_len = htonl((u32)len);

    Net_Result result   = _net_tcp_send_all_fd(
        fd, (const u8*)&frame_len, sizeof(frame_len), deadline, false);
    if (NET_FAILED(result)) {
        return result;
    }

    if (len == 0) {
        return NET_OK;
    }

    return _net_tcp_send_all_fd(fd, buffer, len, deadline, false);
}

//------------------------------------------------------------------------------
// _net_tcp_set_nonblocking
//
// Puts the file descriptor into non-blocking mode. This is only used for the
// listening socket so the server can drain pending accepts without blocking.
//------------------------------------------------------------------------------

internal Net_Result _net_tcp_set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        _net_log_error();
        return NET_ERROR;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        _net_log_error();
        return NET_ERROR;
    }

    return NET_OK;
}

//------------------------------------------------------------------------------
// _net_tcp_accept_pending
//
// Accepts every currently queued client connection and converts each one into
// a managed TCP pipe attached to the listening socket.
//------------------------------------------------------------------------------

internal Net_Result _net_tcp_accept_pending(Net_Socket* sock)
{
    while (true) {
        int client_fd = accept(sock->fd, NULL, NULL);
        if (client_fd < 0) {
            switch (errno) {
            case EAGAIN:
#if EWOULDBLOCK != EAGAIN
            case EWOULDBLOCK:
#endif
                return NET_OK;
            case EMFILE:
            case ENFILE:
                return NET_OUT_OF_FD;
            case ENETDOWN:
                return NET_NO_NETWORK;
            default:
                _net_log_error();
                return NET_ERROR;
            }
        }

        Net_Pipe* pipe = _net_pipe_create_tcp(sock, client_fd);
        if (sock->kind == NET_SOCKET_TELNET) {
            Net_Result result =
                _net_telnet_request_session_state(sock, client_fd);
            if (NET_FAILED(result)) {
                _net_pipe_close(pipe);
                return result;
            }
        }
    }
}

//------------------------------------------------------------------------------
// _net_tcp_poll_ready_pipe
//
// Waits until either the listener has pending accepts or one managed TCP pipe
// becomes readable. The caller receives the ready pipe, or null when only new
// accepts were handled and the poll must be repeated.
//------------------------------------------------------------------------------

Net_Result
_net_tcp_poll_ready_pipe(Net_Socket* sock, Net_Pipe** out, TimePoint deadline)
{
    bool            nonblocking  = _net_socket_nonblocking(sock);
    Net_SocketData* data         = _net_socket_data(sock);
    Array(struct pollfd) pollfds = 0;
    Array(Net_Pipe*) pipes       = 0;

    struct pollfd listener       = {
              .fd     = sock->fd,
              .events = POLLIN,
    };
    array_push(pollfds, listener);
    array_push(pipes, NULL);

    for (usize i = 0; i < array_count(data->pipes); ++i) {
        Net_Pipe* pipe = data->pipes[i];
        if (!pipe || pipe->kind != NET_PIPE_TCP || pipe->closed ||
            pipe->tcp.fd < 0) {
            continue;
        }

        struct pollfd client = {
            .fd     = pipe->tcp.fd,
            .events = POLLIN,
        };
        array_push(pollfds, client);
        array_push(pipes, pipe);
    }

    int poll_result =
        poll(pollfds, array_count(pollfds), _net_timeout_poll_ms(deadline));
    if (poll_result == 0) {
        array_free(pollfds);
        array_free(pipes);
        return nonblocking ? NET_WOULD_BLOCK : NET_TIMEOUT;
    }
    if (poll_result < 0) {
        array_free(pollfds);
        array_free(pipes);
        _net_log_error();
        switch (errno) {
        case ENETDOWN:
            return NET_NO_NETWORK;
        default:
            return NET_ERROR;
        }
    }

    if (pollfds[0].revents & POLLIN) {
        Net_Result result = _net_tcp_accept_pending(sock);
        if (NET_FAILED(result)) {
            array_free(pollfds);
            array_free(pipes);
            return result;
        }
    }

    *out = NULL;
    for (usize i = 1; i < array_count(pollfds); ++i) {
        if (pollfds[i].revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL)) {
            *out = pipes[i];
            break;
        }
    }

    array_free(pollfds);
    array_free(pipes);
    return NET_OK;
}

//------------------------------------------------------------------------------
// _net_tcp_open_pipe_count
//
// Counts the number of currently open TCP pipes owned by the server socket.
//------------------------------------------------------------------------------

internal usize _net_tcp_open_pipe_count(Net_Socket* sock)
{
    Net_SocketData* data  = _net_socket_data(sock);
    usize           count = 0;

    for (usize i = 0; i < array_count(data->pipes); ++i) {
        Net_Pipe* pipe = data->pipes[i];
        if (!pipe || pipe->kind != NET_PIPE_TCP || pipe->closed ||
            pipe->tcp.fd < 0) {
            continue;
        }

        count++;
    }

    return count;
}

//------------------------------------------------------------------------------
// _net_tcp_recv_message_from_fd
//
// Receives one framed TCP message from the given file descriptor and stores it
// as the socket's pending message. When a pipe is provided, the pending message
// keeps that pipe as hidden reply context.
//------------------------------------------------------------------------------

internal Net_Result _net_tcp_recv_message_from_fd(Net_Socket* sock,
                                                  int         fd,
                                                  Net_Pipe*   pipe,
                                                  TimePoint   deadline)
{
    usize      frame_len = 0;
    Net_Result result =
        _net_tcp_wait_for_full_frame(sock, fd, deadline, &frame_len);
    if (NET_FAILED(result)) {
        return result;
    }

    usize max_message_size = _net_socket_data(sock)->max_message_size;
    if (frame_len > max_message_size) {
        // Consume the full oversized frame so future receives remain aligned to
        // the next frame boundary.
        u32 frame_len_n = 0;
        result =
            _net_tcp_recv_exact_fd(fd, &frame_len_n, sizeof(frame_len_n), 0);
        if (NET_FAILED(result)) {
            return result;
        }
        result = _net_tcp_discard_exact_fd(fd, frame_len, deadline);
        if (NET_FAILED(result)) {
            return result;
        }
        return NET_BAD_MESSAGE;
    }

    u32 frame_len_n = 0;
    result = _net_tcp_recv_exact_fd(fd, &frame_len_n, sizeof(frame_len_n), 0);
    if (NET_FAILED(result)) {
        return result;
    }

    if (frame_len == 0) {
        _net_socket_store_pending(sock, NULL, 0, pipe);
        return NET_OK;
    }

    void* frame_buffer = mem_realloc(NULL, frame_len, __FILE__, __LINE__);
    result             = _net_tcp_recv_exact_fd(fd, frame_buffer, frame_len, 0);
    if (NET_FAILED(result)) {
        mem_free(frame_buffer, __FILE__, __LINE__);
        return result;
    }

    _net_socket_store_pending(sock, frame_buffer, frame_len, pipe);
    mem_free(frame_buffer, __FILE__, __LINE__);
    return NET_OK;
}

//------------------------------------------------------------------------------
// _net_tcp_bind
//
// Creates, binds, and starts listening on a TCP socket for the provided
// endpoint. The resulting socket enters the waiting-for-connection state.
//------------------------------------------------------------------------------

Net_Result _net_tcp_bind(Net_Socket* sock, Net_Endpoint* endpoint)
{
    int fd = _net_create_socket(endpoint);
    if (fd < 0) {
        _net_log_error();
        switch (errno) {
        case EMFILE:
        case ENFILE:
            return NET_OUT_OF_FD;
        case ENETDOWN:
            return NET_NO_NETWORK;
        case EPROTONOSUPPORT:
            return NET_PROTOCOL_NOT_SUPPORTED;
        default:
            return NET_ERROR;
        }
    }

    int reuse_addr = 1;
    (void)setsockopt(
        fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));

    struct sockaddr_in addr;
    _net_endpoint_to_addr(endpoint, &addr);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        _net_log_error();
        switch (errno) {
        case ENETDOWN:
            return NET_NO_NETWORK;
        case EACCES:
            return NET_ACCESS_DENIED;
        case EADDRINUSE:
            return NET_PORT_IN_USE;
        default:
            return NET_ERROR;
        }
    }

    if (listen(fd, SOMAXCONN) < 0) {
        close(fd);
        _net_log_error();
        return NET_ERROR;
    }

    Net_Result nonblocking_result = _net_tcp_set_nonblocking(fd);
    if (NET_FAILED(nonblocking_result)) {
        close(fd);
        return nonblocking_result;
    }

    sock->fd    = fd;
    sock->proto = NET_PROTO_TCP;
    sock->state = NET_STATE_WAITING_CONNECTION;
    _net_socket_set_ops(sock,
                        sock->kind == NET_SOCKET_TELNET
                            ? &_net_telnet_tcp_transport_ops
                            : &_net_tcp_transport_ops,
                        _net_socket_data_ensure(sock)->protocol_ops);
    return NET_OK;
}

//------------------------------------------------------------------------------
// _net_tcp_connect
//
// Creates and connects a TCP client socket to the provided endpoint.
//------------------------------------------------------------------------------

Net_Result _net_tcp_connect(Net_Socket* sock, Net_Endpoint* endpoint)
{
    struct sockaddr_in addr;
    _net_endpoint_to_addr(endpoint, &addr);
    Net_SocketData* data        = _net_socket_data_ensure(sock);
    bool            nonblocking = _net_socket_nonblocking(sock);
    u64             connect_timeout_ms =
        nonblocking ? NET_WAIT_IMMEDIATE : data->options.connect_timeout_ms;
    TimePoint start_time = time_now();

    while (true) {
        int fd = _net_create_socket(endpoint);
        if (fd < 0) {
            _net_log_error();
            switch (errno) {
            case EMFILE:
            case ENFILE:
                return NET_OUT_OF_FD;
            case ENETDOWN:
                return NET_NO_NETWORK;
            case EPROTONOSUPPORT:
                return NET_PROTOCOL_NOT_SUPPORTED;
            default:
                return NET_ERROR;
            }
        }

        if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            sock->fd    = fd;
            sock->proto = NET_PROTO_TCP;
            sock->state = NET_STATE_CONNECTED;
            _net_socket_set_ops(sock,
                                sock->kind == NET_SOCKET_TELNET
                                    ? &_net_telnet_tcp_transport_ops
                                    : &_net_tcp_transport_ops,
                                _net_socket_data_ensure(sock)->protocol_ops);
            return NET_OK;
        }

        int connect_error = errno;
        close(fd);

        switch (connect_error) {
        case ENETDOWN:
            _net_log_error();
            return NET_NO_NETWORK;
        case EACCES:
            _net_log_error();
            return NET_ACCESS_DENIED;
        default:
            break;
        }

        if (!_net_tcp_should_retry_connect(connect_error)) {
            _net_log_error();
            return NET_ERROR;
        }

        if (connect_timeout_ms == NET_WAIT_IMMEDIATE) {
            return _net_socket_nonblocking(sock) ? NET_WOULD_BLOCK
                                                 : NET_TIMEOUT;
        }

        if (connect_timeout_ms != NET_WAIT_INFINITE) {
            TimeDuration elapsed = time_elapsed(start_time, time_now());
            if (time_duration_to_ms(elapsed) >= connect_timeout_ms) {
                return NET_TIMEOUT;
            }
        }

        u64 retry_interval_ms = _net_tcp_connect_retry_interval_ms(sock);
        if (connect_timeout_ms != NET_WAIT_INFINITE) {
            TimeDuration elapsed = time_elapsed(start_time, time_now());
            u64          remaining_ms =
                connect_timeout_ms -
                MIN(connect_timeout_ms, time_duration_to_ms(elapsed));
            retry_interval_ms = MIN(retry_interval_ms, remaining_ms);
        }

        time_sleep_ms((u32)retry_interval_ms);
    }
}

//------------------------------------------------------------------------------
// _net_tcp_send
//
// Sends one framed TCP message. The frame format for Phase 1 is a 4-byte
// length prefix in network byte order followed by the payload bytes.
//------------------------------------------------------------------------------

Net_Result _net_tcp_send(Net_Socket* sock, const void* buffer, usize len)
{
    bool      nonblocking = _net_socket_nonblocking(sock);
    TimePoint deadline =
        nonblocking ? time_now()
                    : _net_timeout_deadline_from_option(
                          _net_socket_data(sock)->options.send_timeout_ms);
    u32 frame_len     = htonl((u32)len);

    Net_Result result = _net_tcp_send_all_fd(sock->fd,
                                             (const u8*)&frame_len,
                                             sizeof(frame_len),
                                             deadline,
                                             nonblocking);
    if (NET_FAILED(result)) {
        return result;
    }

    if (len == 0) {
        return NET_OK;
    }

    return _net_tcp_send_all_fd(sock->fd, buffer, len, deadline, nonblocking);
}

//------------------------------------------------------------------------------
// _net_tcp_recv_message
//
// Receives exactly one framed TCP message and stores it in the socket's pending
// buffer so the public receive API can apply retry/drop semantics consistently.
//------------------------------------------------------------------------------

Net_Result _net_tcp_recv_message(Net_Socket* sock)
{
    bool      nonblocking = _net_socket_nonblocking(sock);
    TimePoint deadline =
        nonblocking ? time_now()
                    : _net_timeout_deadline_from_option(
                          _net_socket_data(sock)->options.recv_timeout_ms);

    if (sock->state == NET_STATE_CONNECTED) {
        return _net_tcp_recv_message_from_fd(sock, sock->fd, NULL, deadline);
    }

    while (true) {
        Net_Pipe*  pipe   = NULL;
        Net_Result result = _net_tcp_poll_ready_pipe(sock, &pipe, deadline);
        if (NET_FAILED(result)) {
            return result;
        }

        if (!pipe) {
            continue;
        }

        result =
            _net_tcp_recv_message_from_fd(sock, pipe->tcp.fd, pipe, deadline);
        if (result == NET_CLOSED) {
            // A closed client should not tear down the whole listener. Mark the
            // pipe closed and keep waiting for another client message.
            _net_pipe_close(pipe);
            continue;
        }

        return result;
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
