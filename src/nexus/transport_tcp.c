//------------------------------------------------------------------------------
// TCP transport
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <nexus/internal.h>

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

//------------------------------------------------------------------------------
// _net_tcp_send_all
//
// Writes the full buffer to the TCP socket, retrying partial writes until the
// requested byte count has been sent or an error occurs.
//------------------------------------------------------------------------------

internal Net_Result _net_tcp_send_all(Net_Socket* sock,
                                      const u8*   buffer,
                                      usize       len)
{
    usize sent  = 0;
    int   flags = 0;

#if defined(MSG_NOSIGNAL)
    flags |= MSG_NOSIGNAL;
#endif

    while (sent < len) {
        ssize_t result = send(sock->fd, buffer + sent, len - sent, flags);
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
// _net_tcp_recv_exact
//
// Reads exactly the requested number of bytes from the TCP stream. This is the
// primitive used to assemble framed messages from the stream transport.
//------------------------------------------------------------------------------

internal Net_Result _net_tcp_recv_exact(Net_Socket* sock,
                                        void*       buffer,
                                        usize       len)
{
    u8*   out       = buffer;
    usize recv_size = 0;

    while (recv_size < len) {
        ssize_t result = recv(sock->fd, out + recv_size, len - recv_size, 0);
        if (result < 0) {
            _net_log_error();
            switch (errno) {
            case ENETDOWN:
                return NET_NO_NETWORK;
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
// _net_tcp_discard_exact
//
// Discards a fixed number of bytes from the TCP stream. This is used to keep
// the stream in sync when we detect an oversized or otherwise invalid frame.
//------------------------------------------------------------------------------

internal Net_Result _net_tcp_discard_exact(Net_Socket* sock, usize len)
{
    u8    discard[4096];
    usize remaining = len;

    while (remaining > 0) {
        usize      chunk  = MIN(remaining, sizeof(discard));
        usize      before = remaining;
        // Reuse a small stack buffer because the discarded bytes are not needed
        // after they have been consumed from the stream.
        Net_Result result = _net_tcp_recv_exact(sock, discard, chunk);
        if (NET_FAILED(result)) {
            return result;
        }
        remaining = before - chunk;
    }

    return NET_OK;
}

//------------------------------------------------------------------------------
// _net_tcp_accept_if_needed
//
// Performs the one-time accept step for the simple Phase 1 TCP server model.
// After the first accepted connection, the socket handle is repurposed to refer
// to that connected client stream.
//------------------------------------------------------------------------------

internal Net_Result _net_tcp_accept_if_needed(Net_Socket* sock)
{
    if (sock->state != NET_STATE_WAITING_CONNECTION) {
        return NET_OK;
    }

    int listen_fd = sock->fd;
    int client_fd = accept(sock->fd, NULL, NULL);
    if (client_fd < 0) {
        _net_log_error();
        switch (errno) {
        case EMFILE:
        case ENFILE:
            return NET_OUT_OF_FD;
        case ENETDOWN:
            return NET_NO_NETWORK;
        default:
            return NET_ERROR;
        }
    }

    close(listen_fd);
    // The public API is still one-socket for the simple server flow, so after
    // accept the handle now represents the connected client stream.
    sock->fd    = client_fd;
    sock->state = NET_STATE_CONNECTED;
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

    sock->fd    = fd;
    sock->proto = NET_PROTO_TCP;
    sock->state = NET_STATE_WAITING_CONNECTION;
    return NET_OK;
}

//------------------------------------------------------------------------------
// _net_tcp_connect
//
// Creates and connects a TCP client socket to the provided endpoint.
//------------------------------------------------------------------------------

Net_Result _net_tcp_connect(Net_Socket* sock, Net_Endpoint* endpoint)
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

    struct sockaddr_in addr;
    _net_endpoint_to_addr(endpoint, &addr);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        _net_log_error();
        switch (errno) {
        case ENETDOWN:
            return NET_NO_NETWORK;
        case EACCES:
            return NET_ACCESS_DENIED;
        default:
            return NET_ERROR;
        }
    }

    sock->fd    = fd;
    sock->proto = NET_PROTO_TCP;
    sock->state = NET_STATE_CONNECTED;
    return NET_OK;
}

//------------------------------------------------------------------------------
// _net_tcp_send
//
// Sends one framed TCP message. The frame format for Phase 1 is a 4-byte
// length prefix in network byte order followed by the payload bytes.
//------------------------------------------------------------------------------

Net_Result _net_tcp_send(Net_Socket* sock, const void* buffer, usize len)
{
    u32 frame_len = htonl((u32)len);

    Net_Result result =
        _net_tcp_send_all(sock, (const u8*)&frame_len, sizeof(frame_len));
    if (NET_FAILED(result)) {
        return result;
    }

    if (len == 0) {
        return NET_OK;
    }

    return _net_tcp_send_all(sock, buffer, len);
}

//------------------------------------------------------------------------------
// _net_tcp_recv_message
//
// Receives exactly one framed TCP message and stores it in the socket's pending
// buffer so the public receive API can apply retry/drop semantics consistently.
//------------------------------------------------------------------------------

Net_Result _net_tcp_recv_message(Net_Socket* sock)
{
    Net_Result result = _net_tcp_accept_if_needed(sock);
    if (NET_FAILED(result)) {
        return result;
    }

    u32 frame_len_n = 0;
    result = _net_tcp_recv_exact(sock, &frame_len_n, sizeof(frame_len_n));
    if (NET_FAILED(result)) {
        return result;
    }

    usize frame_len = (usize)ntohl(frame_len_n);
    if (frame_len > NET_MAX_MESSAGE_SIZE) {
        // Consume the full oversized frame so future receives remain aligned to
        // the next frame boundary.
        result = _net_tcp_discard_exact(sock, frame_len);
        if (NET_FAILED(result)) {
            return result;
        }
        return NET_BAD_MESSAGE;
    }

    if (frame_len == 0) {
        _net_socket_store_pending(sock, NULL, 0);
        return NET_OK;
    }

    // Read into a temporary contiguous buffer first, then copy into the socket
    // pending storage used by the higher-level receive semantics.
    void* frame_buffer = mem_realloc(NULL, frame_len, __FILE__, __LINE__);
    result             = _net_tcp_recv_exact(sock, frame_buffer, frame_len);
    if (NET_FAILED(result)) {
        mem_free(frame_buffer, __FILE__, __LINE__);
        return result;
    }

    _net_socket_store_pending(sock, frame_buffer, frame_len);
    mem_free(frame_buffer, __FILE__, __LINE__);
    return NET_OK;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
