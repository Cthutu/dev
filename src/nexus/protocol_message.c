//------------------------------------------------------------------------------
// Message protocol
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <nexus/internal.h>

//------------------------------------------------------------------------------
// _net_message_pipe
//
// Fetches any hidden reply pipe associated with a message.
//------------------------------------------------------------------------------

internal Net_Pipe* _net_message_pipe(Net_Message* msg)
{
    Net_MessageData* data = msg->internal_data;
    return data ? data->pipe : NULL;
}

//------------------------------------------------------------------------------
// _net_message_send
//
// Implements the default message-protocol send path. This layer is responsible
// for message-sized validation and dispatching to the active transport.
//------------------------------------------------------------------------------

Net_Result _net_message_send(Net_Message* msg)
{
    Net_Socket* sock = msg->socket;
    if (!sock) {
        return NET_NOT_CONNECTED;
    }

    Net_SocketData* data = _net_socket_data(sock);

    if (!data || !data->transport_ops || !data->transport_ops->send) {
        return NET_NOT_CONNECTED;
    }

    if (msg->length > data->max_message_size) {
        return NET_BAD_MESSAGE;
    }

    Net_Pipe* pipe = _net_message_pipe(msg);
    if (pipe) {
        return _net_pipe_send(pipe, msg->data, msg->length);
    }

    if (sock->state != NET_STATE_CONNECTED) {
        return NET_NOT_CONNECTED;
    }

    return data->transport_ops->send(sock, msg->data, msg->length);
}

//------------------------------------------------------------------------------
// _net_message_recv
//
// Implements the default message-protocol receive path. It first ensures that a
// complete message is available, then hands that message to the shared pending
// buffer logic so retry/drop semantics are consistent across transports.
//------------------------------------------------------------------------------

Net_Result _net_message_recv(Net_Socket* sock,
                             void*       buffer,
                             usize       len,
                             usize*      out_recv_len,
                             Net_Pipe**  out_pipe)
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

    return _net_socket_consume_pending(
        sock, buffer, len, out_recv_len, out_pipe);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
