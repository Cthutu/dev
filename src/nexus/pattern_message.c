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
    Net_SocketData* data = _net_socket_data(sock);

    if (sock->state != NET_STATE_CONNECTED) {
        return NET_NOT_CONNECTED;
    }

    if (!data || !data->transport_ops || !data->transport_ops->send) {
        return NET_NOT_CONNECTED;
    }

    if (len > data->max_message_size) {
        return NET_BAD_MESSAGE;
    }

    return data->transport_ops->send(sock, buffer, len);
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
    Net_SocketData* data = _net_socket_data(sock);

    if (sock->state != NET_STATE_CONNECTED &&
        sock->state != NET_STATE_WAITING_CONNECTION) {
        return NET_NOT_CONNECTED;
    }

    if (!data || !data->transport_ops || !data->transport_ops->recv_message) {
        return NET_NOT_CONNECTED;
    }

    if (!data->has_pending_message) {
        // Only touch the network when there is no retained message waiting to
        // be consumed by the caller.
        Net_Result result = data->transport_ops->recv_message(sock);
        if (NET_FAILED(result)) {
            return result;
        }
    }

    return _net_socket_consume_pending(sock, buffer, len, out_recv_len);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
