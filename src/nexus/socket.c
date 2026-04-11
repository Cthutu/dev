//------------------------------------------------------------------------------
// Socket functions
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <nexus/internal.h>

#include <errno.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/socket.h>

//------------------------------------------------------------------------------
// net_socket
//
// This creates a socket ready to be bound or connected to.  Currently, we just
// support basic streaming sockets.
//------------------------------------------------------------------------------

Net_Socket net_socket(void)
{
    return (Net_Socket){.state = NET_STATE_DISCONNECTED, .fd = -1};
}

//------------------------------------------------------------------------------
// net_close
//
// Closes an open socket created previously by `net_socket`.
//------------------------------------------------------------------------------

void net_close(Net_Socket* sock)
{
    if (sock->fd >= 0) {
        close(sock->fd);
        sock->fd    = -1;
        sock->state = NET_STATE_DISCONNECTED;
    }
}

//------------------------------------------------------------------------------
// _net_log_error
//
// Logs the last network error to the console.  This is intended to be called
// after a network function fails to provide more information about the failure.
//------------------------------------------------------------------------------

internal void _net_log_error(void)
{
#if DEBUG
    int err = errno;
    prn("Network error: %s", strerror(err));
#endif
}

//------------------------------------------------------------------------------
// _net_create_socket
//
// Creates a socket file descriptor based on the provided endpoint information.
// The socket is not bound or connected to anything, it's just created and
// returned.
//
// Returns the socket file descriptor on success, or -1 on failure.
//------------------------------------------------------------------------------

internal int _net_create_socket(Net_Endpoint* endpoint)
{
    int sock_type  = 0;
    int proto_type = 0;
    switch (endpoint->proto) {
    case NET_PROTO_TCP:
        sock_type  = SOCK_STREAM;
        proto_type = IPPROTO_TCP;
        break;
    case NET_PROTO_UDP:
        sock_type  = SOCK_DGRAM;
        proto_type = IPPROTO_UDP;
        break;
    default:
        return -1; // Invalid protocol
    }

    int fd = socket(AF_INET, sock_type, proto_type);
    if (fd < 0) {
        return -1; // Socket creation failed
    }

    return fd;
}

//------------------------------------------------------------------------------
// net_bind
//
// Binds a socket to the provided URL.  The URL should be in the format
// `<protocol>://<host>:<port>`, e.g. `tcp://127.0.0.1:8080`.
//
// On success, the socket is bound and ready to receive connections (if TCP) or
// send/receive data (if UDP).  On failure, the socket is left unchanged and an
// appropriate error code is returned.
//------------------------------------------------------------------------------

Net_Result net_bind(Net_Socket* out_sock, cstr url)
{
    //
    // Validate the socket is in a state where it can be bound
    //

    if (out_sock->state != NET_STATE_DISCONNECTED) {
        return NET_SOCKET_BUSY; // Socket is already in use
    }

    //
    // Process the URL
    //

    Net_Endpoint endpoint;
    if (!_net_parse_url(url, &endpoint)) {
        return NET_INVALID_URL;
    }

    //
    // Create a socket compatible with the URL
    //

    int fd = _net_create_socket(&endpoint);
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

    out_sock->fd    = fd;
    out_sock->proto = endpoint.proto;

    //
    // Set up the socket address
    //

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(endpoint.port);
    memcpy(&addr.sin_addr, endpoint.ip, 4); // IPv4-mapped IPv

    //
    // Bind the socket to the IP address
    //

    int result = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
    if (result < 0) {
        close(fd);
        out_sock->fd = -1;
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

    //
    if (endpoint.proto == NET_PROTO_TCP) {
        //
        // Start listening on it
        //

        if (listen(fd, SOMAXCONN) < 0) {
            close(fd);
            out_sock->fd = -1;
            return NET_ERROR;
        }

        // Signify that on the next time we try to receive, we need to accept a
        // connection.
        out_sock->state = NET_STATE_WAITING_CONNECTION;
    } else {
        // UDP is connectionless, so a bound socket can receive immediately.
        out_sock->state = NET_STATE_CONNECTED;
    }
    return NET_OK;
}

//------------------------------------------------------------------------------
// net_send
//
// Sends data over a connected socket. For TCP, this loops until the full
// buffer is written or an error occurs. For UDP, the send is a single datagram
// send to the connected peer.
//------------------------------------------------------------------------------

Net_Result net_send(Net_Socket* sock, const void* buffer, usize len)
{
    if (sock->state != NET_STATE_CONNECTED) {
        return NET_NOT_CONNECTED;
    }

    const u8* data = buffer;
    usize     sent = 0;
    int       flags = 0;

#if defined(MSG_NOSIGNAL)
    flags |= MSG_NOSIGNAL;
#endif

    while (sent < len) {
        ssize_t result = send(sock->fd, data + sent, len - sent, flags);
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

        if (sock->proto == NET_PROTO_UDP) {
            break;
        }
    }

    return sent == len ? NET_OK : NET_ERROR;
}

//------------------------------------------------------------------------------
// net_connect
//
// Connects a socket to the provided URL.  The URL should be in the format
// `<protocol>://<host>:<port>`, e.g. `tcp://127.0.0.1:8080`. On success, the
// socket is connected and ready to send/receive data.  On failure the socket is
// left unchanged and an appropriate error code is returned.
//
// Note that for TCP, this will attempt to connect to the server at the provided
// URL.  For UDP, this will just set the default destination for send/recv calls
// to the provided URL (UDP is connectionless, so this doesn't actually
// establish a connection, it just sets the default peer address).
//------------------------------------------------------------------------------

Net_Result net_connect(Net_Socket* out_sock, cstr url)
{
    //
    // Validate the socket is in a state where it can be connected
    //

    if (out_sock->state != NET_STATE_DISCONNECTED) {
        return NET_SOCKET_BUSY; // Socket is already in use
    }

    //
    // Process the URL
    //

    Net_Endpoint endpoint;
    if (!_net_parse_url(url, &endpoint)) {
        return NET_INVALID_URL;
    }

    //
    // Create a socket compatible with the URL
    //

    int fd = _net_create_socket(&endpoint);
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

    out_sock->fd    = fd;
    out_sock->proto = endpoint.proto;

    //
    // Set up the socket address
    //

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(endpoint.port);
    memcpy(&addr.sin_addr, endpoint.ip, 4); // IPv4-mapped IPv

    //
    // Connect the socket to the server
    //

    int result = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
    if (result < 0) {
        close(fd);
        out_sock->fd = -1;
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

    out_sock->state = NET_STATE_CONNECTED;
    return NET_OK;
}

//------------------------------------------------------------------------------
// net_recv
//
// Receives data from the socket.  If the socket is in the
// `NET_STATE_WAITING_CONNECTION` state, this will first accept an incoming
// connection (if TCP) and then receive data from the accepted connection.  If
// the socket is already connected, it will just receive data from the existing
// connection.
//
// On success, the received data is written to the provided buffer and the
// number of bytes received is written to `out_recv_len`.  On failure, the
// socket is left unchanged and an appropriate error code is returned.
//------------------------------------------------------------------------------

Net_Result
net_recv(Net_Socket* sock, void* buffer, usize buffer_len, usize* out_recv_len)
{
    if (sock->state == NET_STATE_WAITING_CONNECTION) {
        if (sock->proto == NET_PROTO_TCP) {
            // Accept an incoming connection if TCP
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

            // Replace the listening socket with the accepted connection. This
            // keeps the one-socket API simple, but it also means this socket
            // only handles a single TCP peer.
            close(listen_fd);
            sock->fd = client_fd;
        }
        sock->state = NET_STATE_CONNECTED;
    }

    if (sock->state != NET_STATE_CONNECTED) {
        return NET_NOT_CONNECTED;
    }

    ssize_t recv_len = recv(sock->fd, buffer, buffer_len, 0);
    if (recv_len < 0) {
        _net_log_error();
        switch (errno) {
        case ENETDOWN:
            return NET_NO_NETWORK;
        default:
            return NET_ERROR;
        }
    }

    if (recv_len == 0) {
        return NET_CLOSED;
    }

    if (out_recv_len) {
        *out_recv_len = (usize)recv_len;
    }
    return NET_OK;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
