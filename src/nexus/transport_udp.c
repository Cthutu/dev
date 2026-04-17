//------------------------------------------------------------------------------
// UDP transport
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <nexus/internal.h>

#include <limits.h>

//------------------------------------------------------------------------------
// _net_udp_transport_ops
//
// Transport operations for UDP sockets.
//------------------------------------------------------------------------------

internal const Net_TransportOps _net_udp_transport_ops = {
    .send         = _net_udp_send,
    .recv_message = _net_udp_recv_message,
};

//------------------------------------------------------------------------------
// _net_udp_wait_fd
//
// Waits for UDP socket readiness according to the configured timeout option.
// A timeout of `NET_WAIT_INFINITE` blocks forever.
//------------------------------------------------------------------------------

internal Net_Result _net_udp_wait_fd(Net_Fd fd,
                                     u16    events,
                                     u64    timeout_ms,
                                     bool   nonblocking)
{
    int timeout = -1;
    if (nonblocking) {
        timeout = 0;
    } else if (timeout_ms != NET_WAIT_INFINITE) {
        timeout = (int)MIN(timeout_ms, (u64)INT_MAX);
    }

    Net_PollFd poll_fd = {
        .fd     = fd,
        .events = events,
    };

    while (true) {
        int poll_result = net_os_poll(&poll_fd, 1, timeout);
        if (poll_result == 0) {
            return nonblocking ? NET_WOULD_BLOCK : NET_TIMEOUT;
        }

        if (poll_result < 0) {
            Net_OsError err = net_os_last_error();
            if (err == NET_OS_INTR) {
                continue;
            }

            net_os_log_error();
            return _net_result_from_os_error(err);
        }

        if (poll_fd.revents & (NET_POLL_ERR | NET_POLL_NVAL)) {
            return NET_ERROR;
        }

        if (poll_fd.revents & events) {
            return NET_OK;
        }
    }
}

//------------------------------------------------------------------------------
// _net_udp_send_to_addr
//
// Sends one UDP datagram to the provided destination address.
//------------------------------------------------------------------------------

Net_Result _net_udp_send_to_addr(Net_Fd          fd,
                                 const Net_Addr* addr,
                                 const void*     buffer,
                                 usize           len)
{
    Net_Result wait_result =
        _net_udp_wait_fd(fd, NET_POLL_OUT, NET_WAIT_INFINITE, false);
    if (NET_FAILED(wait_result)) {
        return wait_result;
    }

    isize sent = net_os_sendto(fd, buffer, len, addr);
    if (sent < 0) {
        Net_OsError err = net_os_last_error();
        net_os_log_error();
        return _net_result_from_os_error(err);
    }

    return (usize)sent == len ? NET_OK : NET_ERROR;
}

//------------------------------------------------------------------------------
// _net_udp_bind
//
// Creates and binds a UDP socket. Because UDP is connectionless, a bound socket
// can immediately receive datagrams.
//------------------------------------------------------------------------------

Net_Result _net_udp_bind(Net_Socket* sock, Net_Endpoint* endpoint)
{
    Net_Fd fd = _net_create_socket(endpoint);
    if (fd == NET_INVALID_FD) {
        Net_OsError err = net_os_last_error();
        net_os_log_error();
        return _net_result_from_os_error(err);
    }

    Net_Addr addr;
    net_os_addr_from_endpoint(endpoint, &addr);

    Net_OsError err = net_os_bind(fd, &addr);
    if (err != NET_OS_OK) {
        net_os_close(fd);
        net_os_log_error();
        return _net_result_from_os_error(err);
    }

    sock->fd    = fd;
    sock->proto = NET_PROTO_UDP;
    sock->state = NET_STATE_CONNECTED;
    _net_socket_set_ops(sock,
                        &_net_udp_transport_ops,
                        _net_socket_data_ensure(sock)->protocol_ops);
    return NET_OK;
}

//------------------------------------------------------------------------------
// _net_udp_connect
//
// Connects a UDP socket to a default peer so the public API can use plain
// send/receive calls without separate peer parameters.
//------------------------------------------------------------------------------

Net_Result _net_udp_connect(Net_Socket* sock, Net_Endpoint* endpoint)
{
    Net_Fd fd = _net_create_socket(endpoint);
    if (fd == NET_INVALID_FD) {
        Net_OsError err = net_os_last_error();
        net_os_log_error();
        return _net_result_from_os_error(err);
    }

    Net_Addr addr;
    net_os_addr_from_endpoint(endpoint, &addr);

    Net_OsError err = net_os_connect(fd, &addr);
    if (err != NET_OS_OK) {
        net_os_close(fd);
        net_os_log_error();
        return _net_result_from_os_error(err);
    }

    sock->fd    = fd;
    sock->proto = NET_PROTO_UDP;
    sock->state = NET_STATE_CONNECTED;
    _net_socket_set_ops(sock,
                        &_net_udp_transport_ops,
                        _net_socket_data_ensure(sock)->protocol_ops);
    return NET_OK;
}

//------------------------------------------------------------------------------
// _net_udp_send
//
// Sends one UDP datagram to the socket's connected peer.
//------------------------------------------------------------------------------

Net_Result _net_udp_send(Net_Socket* sock, const void* buffer, usize len)
{
    Net_Result wait_result =
        _net_udp_wait_fd(sock->fd,
                         NET_POLL_OUT,
                         _net_socket_data(sock)->options.send_timeout_ms,
                         _net_socket_data(sock)->options.nonblocking != 0);
    if (NET_FAILED(wait_result)) {
        return wait_result;
    }

    isize sent = net_os_send(sock->fd, buffer, len);
    if (sent < 0) {
        Net_OsError err = net_os_last_error();
        net_os_log_error();
        return _net_result_from_os_error(err);
    }

    return (usize)sent == len ? NET_OK : NET_ERROR;
}

//------------------------------------------------------------------------------
// _net_udp_recv_message
//
// Receives one UDP datagram and stores it in the socket's pending buffer so the
// message protocol can apply the same retry/drop semantics used for TCP.
//------------------------------------------------------------------------------

Net_Result _net_udp_recv_message(Net_Socket* sock)
{
    Net_Result wait_result =
        _net_udp_wait_fd(sock->fd,
                         NET_POLL_IN,
                         _net_socket_data(sock)->options.recv_timeout_ms,
                         _net_socket_data(sock)->options.nonblocking != 0);
    if (NET_FAILED(wait_result)) {
        return wait_result;
    }

    usize packet_len = 0;
    Net_OsError available_err = net_os_available_bytes(sock->fd, &packet_len);
    if (available_err != NET_OS_OK) {
        net_os_log_error();
        return _net_result_from_os_error(available_err);
    }

    Net_Addr route_addr;
    usize    max_message_size = _net_socket_data(sock)->max_message_size;
    if (packet_len > max_message_size) {
        // Receiving with a short buffer discards the rest of the datagram.
        u8 discard = 0;
        (void)net_os_recvfrom(sock->fd, &discard, sizeof(discard), 0, &route_addr);
        return NET_BAD_MESSAGE;
    }

    if (packet_len == 0) {
        u8 discard = 0;
        isize recv_len =
            net_os_recvfrom(sock->fd, &discard, sizeof(discard), 0, &route_addr);
        if (recv_len < 0) {
            Net_OsError err = net_os_last_error();
            net_os_log_error();
            return _net_result_from_os_error(err);
        }

        Net_Pipe* pipe = _net_pipe_find_or_create_udp(sock, &route_addr);
        _net_socket_store_pending(sock, NULL, 0, pipe);
        return NET_OK;
    }

    // UDP preserves message boundaries, so one receive gives us exactly one
    // complete message to retain for the public receive API.
    void* packet_buffer = mem_realloc(NULL, packet_len, __FILE__, __LINE__);
    isize recv_len =
        net_os_recvfrom(sock->fd, packet_buffer, packet_len, 0, &route_addr);
    if (recv_len < 0) {
        Net_OsError err = net_os_last_error();
        mem_free(packet_buffer, __FILE__, __LINE__);
        net_os_log_error();
        return _net_result_from_os_error(err);
    }

    Net_Pipe* pipe = _net_pipe_find_or_create_udp(sock, &route_addr);

    _net_socket_store_pending(sock, packet_buffer, (usize)recv_len, pipe);
    mem_free(packet_buffer, __FILE__, __LINE__);
    return NET_OK;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
