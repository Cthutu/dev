//------------------------------------------------------------------------------
// Socket functions
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <nexus/internal.h>

#include <errno.h>
#include <unistd.h>

//------------------------------------------------------------------------------
// _net_message_pattern_ops
//
// Pattern operations for the default basic socket kind.
//------------------------------------------------------------------------------

internal const Net_PatternOps _net_message_pattern_ops = {
    .send = _net_message_send,
    .recv = _net_message_recv,
};

//------------------------------------------------------------------------------
// _net_socket_data
//
// Fetches the private runtime block for a socket, or null when one has not yet
// been created.
//------------------------------------------------------------------------------

Net_SocketData* _net_socket_data(Net_Socket* sock)
{
    return sock->internal_data;
}

//------------------------------------------------------------------------------
// _net_socket_data_ensure
//
// Creates the private runtime block for a socket on first use.
//------------------------------------------------------------------------------

Net_SocketData* _net_socket_data_ensure(Net_Socket* sock)
{
    Net_SocketData* data = _net_socket_data(sock);
    if (!data) {
        data  = mem_realloc(NULL, sizeof(*data), __FILE__, __LINE__);
        *data = (Net_SocketData){0};
        data->max_message_size = NET_MAX_MESSAGE_SIZE;
        sock->internal_data    = data;
    }
    return data;
}

//------------------------------------------------------------------------------
// _net_socket_set_ops
//
// Updates the transport and pattern operation tables attached to a socket.
//------------------------------------------------------------------------------

void _net_socket_set_ops(Net_Socket*             sock,
                         const Net_TransportOps* transport_ops,
                         const Net_PatternOps*   pattern_ops)
{
    Net_SocketData* data = _net_socket_data_ensure(sock);
    data->transport_ops  = transport_ops;
    data->pattern_ops    = pattern_ops;
}

//------------------------------------------------------------------------------
// net_socket
//
// Creates a new socket handle in the disconnected state. The handle itself is
// just a small piece of local state until `net_bind` or `net_connect` attaches
// it to an operating-system socket.
//------------------------------------------------------------------------------

Net_Socket net_socket(void)
{
    Net_Socket sock = (Net_Socket){
        .state = NET_STATE_DISCONNECTED,
        .fd    = -1,
        .kind  = NET_SOCKET_BASIC,
    };

    _net_socket_set_ops(&sock, NULL, &_net_message_pattern_ops);
    return sock;
}

//------------------------------------------------------------------------------
// net_close
//
// Closes any open operating-system socket and releases any pending message data
// retained internally by Nexus.
//------------------------------------------------------------------------------

void net_close(Net_Socket* sock)
{
    if (sock->fd >= 0) {
        close(sock->fd);
        sock->fd = -1;
    }

    _net_socket_clear_pending(sock);
    if (sock->internal_data) {
        sock->internal_data = mem_free(sock->internal_data, __FILE__, __LINE__);
    }
    sock->state = NET_STATE_DISCONNECTED;
}

//------------------------------------------------------------------------------
// net_result_string
//
// Converts a result code into a short readable string suitable for logging and
// diagnostics.
//------------------------------------------------------------------------------

cstr net_result_string(Net_Result result)
{
    switch (result) {
    case NET_OK:
        return "ok";
    case NET_INVALID_URL:
        return "invalid url";
    case NET_NO_NETWORK:
        return "no network";
    case NET_OUT_OF_FD:
        return "out of file descriptors";
    case NET_PROTOCOL_NOT_SUPPORTED:
        return "protocol not supported";
    case NET_PORT_IN_USE:
        return "port in use";
    case NET_ACCESS_DENIED:
        return "access denied";
    case NET_SOCKET_BUSY:
        return "socket busy";
    case NET_NOT_CONNECTED:
        return "not connected";
    case NET_BUFFER_TOO_SMALL:
        return "buffer too small";
    case NET_BAD_MESSAGE:
        return "bad message";
    case NET_CLOSED:
        return "connection closed";
    case NET_ERROR:
        return "network error";
    default:
        return "unknown network result";
    }
}

//------------------------------------------------------------------------------
// _net_log_error
//
// Logs the current network error in debug builds. Public APIs translate these
// platform errors into the smaller `Net_Result` space.
//------------------------------------------------------------------------------

void _net_log_error(void)
{
#if DEBUG
    int err = errno;
    prn("Network error: %s", strerror(err));
#endif
}

//------------------------------------------------------------------------------
// _net_create_socket
//
// Creates a raw operating-system socket matching the endpoint protocol. The
// returned file descriptor is not bound or connected yet.
//------------------------------------------------------------------------------

int _net_create_socket(Net_Endpoint* endpoint)
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
        return -1;
    }

    return socket(AF_INET, sock_type, proto_type);
}

//------------------------------------------------------------------------------
// _net_endpoint_to_addr
//
// Converts the parsed Nexus endpoint into a `sockaddr_in` ready for bind or
// connect.
//------------------------------------------------------------------------------

void _net_endpoint_to_addr(Net_Endpoint* endpoint, struct sockaddr_in* out_addr)
{
    out_addr->sin_family = AF_INET;
    out_addr->sin_port   = htons(endpoint->port);
    memcpy(&out_addr->sin_addr, endpoint->ip, 4);
}

//------------------------------------------------------------------------------
// _net_socket_clear_pending
//
// Releases any pending message retained by the socket. We keep pending messages
// when the caller's receive buffer is too small and the message needs to remain
// available for a retry.
//------------------------------------------------------------------------------

void _net_socket_clear_pending(Net_Socket* sock)
{
    Net_SocketData* data = _net_socket_data(sock);
    if (!data) {
        return;
    }

    if (data->pending_message) {
        data->pending_message =
            mem_free(data->pending_message, __FILE__, __LINE__);
    }
    data->pending_message_len      = 0;
    data->pending_message_capacity = 0;
    data->has_pending_message      = false;
}

//------------------------------------------------------------------------------
// _net_socket_store_pending
//
// Copies a complete message into the socket's private pending buffer so later
// `net_recv` calls can consume, retry, or drop it.
//------------------------------------------------------------------------------

void _net_socket_store_pending(Net_Socket* sock, const void* buffer, usize len)
{
    Net_SocketData* data = _net_socket_data_ensure(sock);

    if (len > data->pending_message_capacity) {
        // Grow the retained buffer only when needed so repeated receives can
        // reuse the same allocation.
        data->pending_message =
            mem_realloc(data->pending_message, len, __FILE__, __LINE__);
        data->pending_message_capacity = len;
    }

    if (len > 0) {
        memcpy(data->pending_message, buffer, len);
    }

    data->pending_message_len = len;
    data->has_pending_message = true;
}

//------------------------------------------------------------------------------
// _net_socket_consume_pending
//
// Copies the retained pending message into the caller buffer, or drops it when
// the caller passes a null buffer. If the caller buffer is too small, the
// message is left untouched for a later retry.
//------------------------------------------------------------------------------

Net_Result _net_socket_consume_pending(Net_Socket* sock,
                                       void*       buffer,
                                       usize       len,
                                       usize*      out_recv_len)
{
    Net_SocketData* data = _net_socket_data(sock);
    if (!data || !data->has_pending_message) {
        return NET_NOT_CONNECTED;
    }

    if (out_recv_len) {
        // Always report the full pending size so the caller can size a retry
        // buffer correctly.
        *out_recv_len = data->pending_message_len;
    }

    if (!buffer) {
        // A null buffer is the explicit "drop this pending message" signal.
        data->has_pending_message = false;
        data->pending_message_len = 0;
        return NET_OK;
    }

    if (len < data->pending_message_len) {
        return NET_BUFFER_TOO_SMALL;
    }

    if (data->pending_message_len > 0) {
        memcpy(buffer, data->pending_message, data->pending_message_len);
    }

    data->has_pending_message = false;
    data->pending_message_len = 0;
    return NET_OK;
}

//------------------------------------------------------------------------------
// net_bind
//
// Binds a socket to the provided URL. TCP sockets become simple server
// endpoints that accept on the first receive; UDP sockets can receive
// immediately after a successful bind.
//------------------------------------------------------------------------------

Net_Result net_bind(Net_Socket* sock, cstr url)
{
    if (sock->state != NET_STATE_DISCONNECTED) {
        return NET_SOCKET_BUSY;
    }

    Net_Endpoint endpoint;
    if (!_net_parse_url(url, &endpoint)) {
        return NET_INVALID_URL;
    }

    switch (endpoint.proto) {
    case NET_PROTO_TCP:
        return _net_tcp_bind(sock, &endpoint);
    case NET_PROTO_UDP:
        return _net_udp_bind(sock, &endpoint);
    default:
        return NET_PROTOCOL_NOT_SUPPORTED;
    }
}

//------------------------------------------------------------------------------
// net_connect
//
// Connects a socket to the provided URL. For UDP this records the default peer
// so the message pattern can continue to use the same send/receive interface.
//------------------------------------------------------------------------------

Net_Result net_connect(Net_Socket* sock, cstr url)
{
    if (sock->state != NET_STATE_DISCONNECTED) {
        return NET_SOCKET_BUSY;
    }

    Net_Endpoint endpoint;
    if (!_net_parse_url(url, &endpoint)) {
        return NET_INVALID_URL;
    }

    switch (endpoint.proto) {
    case NET_PROTO_TCP:
        return _net_tcp_connect(sock, &endpoint);
    case NET_PROTO_UDP:
        return _net_udp_connect(sock, &endpoint);
    default:
        return NET_PROTOCOL_NOT_SUPPORTED;
    }
}

//------------------------------------------------------------------------------
// net_send
//
// Sends one message using the default message pattern for the socket. Transport
// details such as TCP framing are handled below this API layer.
//------------------------------------------------------------------------------

Net_Result net_send(Net_Socket* sock, const void* buffer, usize len)
{
    Net_SocketData* data = _net_socket_data(sock);
    if (!data || !data->pattern_ops || !data->pattern_ops->send) {
        return NET_NOT_CONNECTED;
    }

    return data->pattern_ops->send(sock, buffer, len);
}

//------------------------------------------------------------------------------
// net_recv
//
// Receives one message using the default message pattern for the socket. If a
// previous receive left a message pending, this call consumes or drops that
// retained message before touching the network again.
//------------------------------------------------------------------------------

Net_Result
net_recv(Net_Socket* sock, void* buffer, usize len, usize* out_recv_len)
{
    Net_SocketData* data = _net_socket_data(sock);
    if (!data || !data->pattern_ops || !data->pattern_ops->recv) {
        return NET_NOT_CONNECTED;
    }

    return data->pattern_ops->recv(sock, buffer, len, out_recv_len);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
