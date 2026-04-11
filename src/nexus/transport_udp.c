//------------------------------------------------------------------------------
// UDP transport
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <nexus/internal.h>

#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

//------------------------------------------------------------------------------
// _net_udp_bind
//
// Creates and binds a UDP socket. Because UDP is connectionless, a bound socket
// can immediately receive datagrams.
//------------------------------------------------------------------------------

Net_Result _net_udp_bind(Net_Socket* sock, Net_Endpoint* endpoint)
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

    sock->fd    = fd;
    sock->proto = NET_PROTO_UDP;
    sock->state = NET_STATE_CONNECTED;
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
    sock->proto = NET_PROTO_UDP;
    sock->state = NET_STATE_CONNECTED;
    return NET_OK;
}

//------------------------------------------------------------------------------
// _net_udp_send
//
// Sends one UDP datagram to the socket's connected peer.
//------------------------------------------------------------------------------

Net_Result _net_udp_send(Net_Socket* sock, const void* buffer, usize len)
{
    ssize_t sent = send(sock->fd, buffer, len, 0);
    if (sent < 0) {
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

    return (usize)sent == len ? NET_OK : NET_ERROR;
}

//------------------------------------------------------------------------------
// _net_udp_recv_message
//
// Receives one UDP datagram and stores it in the socket's pending buffer so the
// message pattern can apply the same retry/drop semantics used for TCP.
//------------------------------------------------------------------------------

Net_Result _net_udp_recv_message(Net_Socket* sock)
{
    ssize_t packet_len = recv(sock->fd, NULL, 0, MSG_PEEK | MSG_TRUNC);
    if (packet_len < 0) {
        _net_log_error();
        switch (errno) {
        case ENETDOWN:
            return NET_NO_NETWORK;
        default:
            return NET_ERROR;
        }
    }

    if ((usize)packet_len > NET_MAX_MESSAGE_SIZE) {
        // Receiving with a short buffer discards the rest of the datagram.
        u8 discard = 0;
        recv(sock->fd, &discard, sizeof(discard), 0);
        return NET_BAD_MESSAGE;
    }

    if (packet_len == 0) {
        recv(sock->fd, NULL, 0, 0);
        _net_socket_store_pending(sock, NULL, 0);
        return NET_OK;
    }

    // UDP preserves message boundaries, so one receive gives us exactly one
    // complete message to retain for the public receive API.
    void* packet_buffer =
        mem_realloc(NULL, (usize)packet_len, __FILE__, __LINE__);
    ssize_t recv_len = recv(sock->fd, packet_buffer, (usize)packet_len, 0);
    if (recv_len < 0) {
        mem_free(packet_buffer, __FILE__, __LINE__);
        _net_log_error();
        switch (errno) {
        case ENETDOWN:
            return NET_NO_NETWORK;
        default:
            return NET_ERROR;
        }
    }

    _net_socket_store_pending(sock, packet_buffer, (usize)recv_len);
    mem_free(packet_buffer, __FILE__, __LINE__);
    return NET_OK;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
