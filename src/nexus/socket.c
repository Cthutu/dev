//------------------------------------------------------------------------------
// Socket functions
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <nexus/internal.h>

//------------------------------------------------------------------------------
// _net_message_protocol_ops
//
// Protocol operations for the default basic socket kind.
//------------------------------------------------------------------------------

internal const Net_ProtocolOps _net_message_protocol_ops = {
    .send = _net_message_send,
    .recv = _net_message_recv,
};

internal const Net_ProtocolOps _net_reqrep_protocol_ops = {
    .send = _net_reqrep_send,
    .recv = _net_reqrep_recv,
};

#define NET_DEFAULT_CONNECT_TIMEOUT_MS 1000ull
#define NET_DEFAULT_RECONNECT_INTERVAL_MS 25ull

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

internal Net_SocketData* _net_socket_data_alloc(Net_Socket* sock)
{
    Net_SocketData* data = NULL;

    switch (sock->kind) {
    case NET_SOCKET_REQUEST:
    case NET_SOCKET_REPLY:
        data =
            mem_realloc(NULL, sizeof(Net_ReqRepSocketData), __FILE__, __LINE__);
        *(Net_ReqRepSocketData*)data = (Net_ReqRepSocketData){0};
        break;

    case NET_SOCKET_TELNET:
        data =
            mem_realloc(NULL, sizeof(Net_TelnetSocketData), __FILE__, __LINE__);
        *(Net_TelnetSocketData*)data = (Net_TelnetSocketData){0};
        break;

    case NET_SOCKET_BASIC:
    default:
        data  = mem_realloc(NULL, sizeof(Net_SocketData), __FILE__, __LINE__);
        *data = (Net_SocketData){0};
        break;
    }

    data->kind                          = sock->kind;
    data->max_message_size              = NET_MAX_MESSAGE_SIZE;
    data->options.connect_timeout_ms    = NET_DEFAULT_CONNECT_TIMEOUT_MS;
    data->options.reconnect_interval_ms = NET_DEFAULT_RECONNECT_INTERVAL_MS;
    data->options.send_timeout_ms       = NET_WAIT_INFINITE;
    data->options.recv_timeout_ms       = NET_WAIT_INFINITE;
    data->options.nonblocking           = 0;

    if (sock->kind == NET_SOCKET_TELNET) {
        ((Net_TelnetSocketData*)data)->telnet.mode = NET_TELNET_LINE_MODE;
    }

    return data;
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
        data                = _net_socket_data_alloc(sock);
        sock->internal_data = data;
    }
    return data;
}

Net_ReqRepSocketData* _net_reqrep_socket_data(Net_Socket* sock)
{
    Net_SocketData* data = _net_socket_data_ensure(sock);
    ASSERT(data->kind == NET_SOCKET_REQUEST || data->kind == NET_SOCKET_REPLY,
           "Socket runtime kind is not request/reply");
    return (Net_ReqRepSocketData*)data;
}

Net_TelnetSocketData* _net_telnet_socket_data(Net_Socket* sock)
{
    Net_SocketData* data = _net_socket_data_ensure(sock);
    ASSERT(data->kind == NET_SOCKET_TELNET,
           "Socket runtime kind is not telnet");
    return (Net_TelnetSocketData*)data;
}

//------------------------------------------------------------------------------
// _net_socket_set_ops
//
// Updates the transport and protocol operation tables attached to a socket.
//------------------------------------------------------------------------------

void _net_socket_set_ops(Net_Socket*             sock,
                         const Net_TransportOps* transport_ops,
                         const Net_ProtocolOps*  protocol_ops)
{
    Net_SocketData* data = _net_socket_data_ensure(sock);
    data->transport_ops  = transport_ops;
    data->protocol_ops   = protocol_ops;
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
        .fd    = NET_INVALID_FD,
        .kind  = NET_SOCKET_BASIC,
    };

    _net_socket_set_ops(&sock, NULL, &_net_message_protocol_ops);
    return sock;
}

//------------------------------------------------------------------------------
// net_request_socket
//
// Creates a request socket. Request sockets must send first, then receive,
// repeating that request/reply order for each exchange.
//------------------------------------------------------------------------------

Net_Socket net_request_socket(void)
{
    Net_Socket sock = (Net_Socket){
        .state = NET_STATE_DISCONNECTED,
        .fd    = NET_INVALID_FD,
        .kind  = NET_SOCKET_REQUEST,
    };
    _net_socket_set_ops(&sock, NULL, &_net_reqrep_protocol_ops);
    _net_reqrep_socket_data(&sock)->send_next = true;
    return sock;
}

//------------------------------------------------------------------------------
// net_reply_socket
//
// Creates a reply socket. Reply sockets must receive first, then send,
// repeating that receive/reply order for each exchange.
//------------------------------------------------------------------------------

Net_Socket net_reply_socket(void)
{
    Net_Socket sock = (Net_Socket){
        .state = NET_STATE_DISCONNECTED,
        .fd    = NET_INVALID_FD,
        .kind  = NET_SOCKET_REPLY,
    };
    _net_socket_set_ops(&sock, NULL, &_net_reqrep_protocol_ops);
    _net_reqrep_socket_data(&sock)->send_next = false;
    return sock;
}

//------------------------------------------------------------------------------
// net_telnet_socket
//
// Creates a telnet socket. The first telnet mode is line-oriented over TCP,
// with each message representing one received or transmitted line.
//------------------------------------------------------------------------------

Net_Socket net_telnet_socket(void)
{
    Net_Socket sock = (Net_Socket){
        .state = NET_STATE_DISCONNECTED,
        .fd    = NET_INVALID_FD,
        .kind  = NET_SOCKET_TELNET,
    };
    _net_socket_set_ops(&sock, NULL, &_net_message_protocol_ops);
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
    _net_socket_close_pipes(sock);

    if (sock->fd != NET_INVALID_FD) {
        if (sock->kind == NET_SOCKET_TELNET) {
            _net_tcp_close_telnet_fd(sock->fd);
        } else {
            net_os_close(sock->fd);
        }
        sock->fd = NET_INVALID_FD;
    }

    _net_socket_clear_pending(sock);
    if (sock->internal_data) {
        if (_net_socket_data(sock)->kind == NET_SOCKET_TELNET) {
            _net_telnet_state_done(&_net_telnet_socket_data(sock)->telnet);
        }
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
    case NET_TIMEOUT:
        return "timed out";
    case NET_WOULD_BLOCK:
        return "would block";
    case NET_WRONG_STATE:
        return "wrong state";
    case NET_CLOSED:
        return "connection closed";
    case NET_ERROR:
        return "network error";
    default:
        return "unknown network result";
    }
}

//------------------------------------------------------------------------------
// _net_create_socket
//
// Creates a raw operating-system socket matching the endpoint protocol. The
// returned handle is not bound or connected yet.
//------------------------------------------------------------------------------

Net_Fd _net_create_socket(Net_Endpoint* endpoint)
{
    return net_os_socket(endpoint->proto);
}

//------------------------------------------------------------------------------
// _net_result_from_os_error
//
// Maps the canonical lowlevel error to the closest public `Net_Result`. Codes
// without a dedicated public slot fall through to `NET_ERROR`.
//------------------------------------------------------------------------------

Net_Result _net_result_from_os_error(Net_OsError err)
{
    switch (err) {
    case NET_OS_OK:
        return NET_OK;
    case NET_OS_WOULD_BLOCK:
        return NET_WOULD_BLOCK;
    case NET_OS_NET_DOWN:
    case NET_OS_NET_UNREACH:
    case NET_OS_HOST_UNREACH:
        return NET_NO_NETWORK;
    case NET_OS_CONN_REFUSED:
    case NET_OS_CONN_RESET:
    case NET_OS_PIPE_BROKEN:
    case NET_OS_CLOSED:
        return NET_CLOSED;
    case NET_OS_TIMED_OUT:
        return NET_TIMEOUT;
    case NET_OS_ADDR_IN_USE:
        return NET_PORT_IN_USE;
    case NET_OS_ACCESS_DENIED:
        return NET_ACCESS_DENIED;
    case NET_OS_OUT_OF_FD:
        return NET_OUT_OF_FD;
    case NET_OS_PROTO_NOT_SUPPORTED:
        return NET_PROTOCOL_NOT_SUPPORTED;
    case NET_OS_INTR:
    case NET_OS_OTHER:
    default:
        return NET_ERROR;
    }
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
    data->pending_pipe             = NULL;
}

//------------------------------------------------------------------------------
// _net_socket_store_pending
//
// Copies a complete message into the socket's private pending buffer so later
// `net_recv` calls can consume, retry, or drop it.
//------------------------------------------------------------------------------

void _net_socket_store_pending(Net_Socket* sock,
                               const void* buffer,
                               usize       len,
                               Net_Pipe*   pipe)
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
    data->pending_pipe        = pipe;
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
                                       usize*      out_recv_len,
                                       Net_Pipe**  out_pipe)
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
    if (out_pipe) {
        *out_pipe = data->pending_pipe;
    }

    if (!buffer) {
        // A null buffer is the explicit "drop this pending message" signal.
        data->has_pending_message = false;
        data->pending_message_len = 0;
        data->pending_pipe        = NULL;
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
    data->pending_pipe        = NULL;
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

    if (sock->kind == NET_SOCKET_TELNET && endpoint.proto != NET_PROTO_TCP) {
        return NET_PROTOCOL_NOT_SUPPORTED;
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
// so the message protocol can continue to use the same send/receive interface.
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

    if (sock->kind == NET_SOCKET_TELNET && endpoint.proto != NET_PROTO_TCP) {
        return NET_PROTOCOL_NOT_SUPPORTED;
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
// net_set_option
//
// Stores a configuration value in the socket's private runtime state so future
// operations such as `net_connect` can obey it.
//------------------------------------------------------------------------------

Net_Result net_set_option(Net_Socket* sock, Net_Option option, u64 value)
{
    Net_SocketData* data = _net_socket_data_ensure(sock);

    switch (option) {
    case NET_OPT_CONNECT_TIMEOUT_MS:
        data->options.connect_timeout_ms = value;
        return NET_OK;
    case NET_OPT_RECONNECT_INTERVAL_MS:
        data->options.reconnect_interval_ms = value;
        return NET_OK;
    case NET_OPT_SEND_TIMEOUT_MS:
        data->options.send_timeout_ms = value;
        return NET_OK;
    case NET_OPT_RECV_TIMEOUT_MS:
        data->options.recv_timeout_ms = value;
        return NET_OK;
    case NET_OPT_NONBLOCKING:
        data->options.nonblocking = value ? 1 : 0;
        return NET_OK;
    case NET_OPT_TELNET_MODE:
        if (sock->kind != NET_SOCKET_TELNET) {
            return NET_PROTOCOL_NOT_SUPPORTED;
        }
        if (value != NET_TELNET_LINE_MODE &&
            value != NET_TELNET_CHARACTER_MODE) {
            return NET_ERROR;
        }
        _net_telnet_socket_data(sock)->telnet.mode = (u8)value;
        return NET_OK;
    default:
        return NET_ERROR;
    }
}

//------------------------------------------------------------------------------
// net_get_option
//
// Reads a previously configured socket option from the socket's private state.
//------------------------------------------------------------------------------

Net_Result net_get_option(Net_Socket* sock, Net_Option option, u64* out_value)
{
    if (!out_value) {
        return NET_ERROR;
    }

    Net_SocketData* data = _net_socket_data_ensure(sock);

    switch (option) {
    case NET_OPT_CONNECT_TIMEOUT_MS:
        *out_value = data->options.connect_timeout_ms;
        return NET_OK;
    case NET_OPT_RECONNECT_INTERVAL_MS:
        *out_value = data->options.reconnect_interval_ms;
        return NET_OK;
    case NET_OPT_SEND_TIMEOUT_MS:
        *out_value = data->options.send_timeout_ms;
        return NET_OK;
    case NET_OPT_RECV_TIMEOUT_MS:
        *out_value = data->options.recv_timeout_ms;
        return NET_OK;
    case NET_OPT_NONBLOCKING:
        *out_value = data->options.nonblocking;
        return NET_OK;
    case NET_OPT_TELNET_MODE:
        if (sock->kind != NET_SOCKET_TELNET) {
            return NET_PROTOCOL_NOT_SUPPORTED;
        }
        *out_value = _net_telnet_socket_data(sock)->telnet.mode;
        return NET_OK;
    default:
        return NET_ERROR;
    }
}

//------------------------------------------------------------------------------
// _net_socket_send
//
// Sends one raw payload using the active socket protocol. This is an internal
// helper used by the public message API after it has finished preparing the
// payload stored in a `Net_Message`.
//------------------------------------------------------------------------------

Net_Result _net_socket_send(Net_Message* msg)
{
    if (!msg || !msg->socket) {
        return NET_NOT_CONNECTED;
    }

    Net_Socket*     sock = msg->socket;
    Net_SocketData* data = _net_socket_data(sock);
    if (!data || !data->protocol_ops || !data->protocol_ops->send) {
        return NET_NOT_CONNECTED;
    }

    return data->protocol_ops->send(msg);
}

//------------------------------------------------------------------------------
// _net_socket_recv
//
// Receives one raw payload using the active socket protocol. If a previous
// receive left a message pending, this helper consumes that retained message
// before touching the network again.
//------------------------------------------------------------------------------

Net_Result _net_socket_recv(Net_Socket* sock,
                            void*       buffer,
                            usize       len,
                            usize*      out_recv_len,
                            Net_Pipe**  out_pipe)
{
    Net_SocketData* data = _net_socket_data(sock);
    if (!data || !data->protocol_ops || !data->protocol_ops->recv) {
        return NET_NOT_CONNECTED;
    }

    return data->protocol_ops->recv(sock, buffer, len, out_recv_len, out_pipe);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
