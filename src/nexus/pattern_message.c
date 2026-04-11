//------------------------------------------------------------------------------
// Message pattern
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <nexus/internal.h>

//------------------------------------------------------------------------------
// _net_message_send
//
// Implements the default message-pattern send path. This layer is responsible
// for message-sized validation and dispatching to the active transport.
//------------------------------------------------------------------------------

Net_Result _net_message_send(Net_Socket* sock, const void* buffer, usize len)
{
    if (sock->state != NET_STATE_CONNECTED) {
        return NET_NOT_CONNECTED;
    }

    if (len > NET_MAX_MESSAGE_SIZE) {
        return NET_BAD_MESSAGE;
    }

    switch (sock->proto) {
    case NET_PROTO_TCP:
        return _net_tcp_send(sock, buffer, len);
    case NET_PROTO_UDP:
        return _net_udp_send(sock, buffer, len);
    default:
        return NET_PROTOCOL_NOT_SUPPORTED;
    }
}

//------------------------------------------------------------------------------
// _net_message_recv
//
// Implements the default message-pattern receive path. It first ensures that a
// complete message is available, then hands that message to the shared pending
// buffer logic so retry/drop semantics are consistent across transports.
//------------------------------------------------------------------------------

Net_Result _net_message_recv(Net_Socket* sock,
                             void*       buffer,
                             usize       len,
                             usize*      out_recv_len)
{
    if (sock->state != NET_STATE_CONNECTED &&
        sock->state != NET_STATE_WAITING_CONNECTION) {
        return NET_NOT_CONNECTED;
    }

    if (!sock->has_pending_message) {
        // Only touch the network when there is no retained message waiting to
        // be consumed by the caller.
        Net_Result result = NET_ERROR;
        switch (sock->proto) {
        case NET_PROTO_TCP:
            result = _net_tcp_recv_message(sock);
            break;
        case NET_PROTO_UDP:
            result = _net_udp_recv_message(sock);
            break;
        default:
            result = NET_PROTOCOL_NOT_SUPPORTED;
            break;
        }

        if (NET_FAILED(result)) {
            return result;
        }
    }

    return _net_socket_consume_pending(sock, buffer, len, out_recv_len);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
