//------------------------------------------------------------------------------
// Request/reply protocol
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <nexus/internal.h>

//------------------------------------------------------------------------------
// _net_reqrep_send
//
// Sends one request/reply-protocol message. The socket must currently be in its
// send phase: request sockets start there, and reply sockets enter it after a
// successful receive.
//------------------------------------------------------------------------------

Net_Result _net_reqrep_send(Net_Message* msg)
{
    if (!msg || !msg->socket) {
        return NET_NOT_CONNECTED;
    }

    Net_Socket*           sock = msg->socket;
    Net_SocketData*       data = _net_socket_data(sock);
    Net_ReqRepSocketData* rdat = _net_reqrep_socket_data(sock);
    if (!data || !data->protocol_ops || !data->transport_ops ||
        !data->transport_ops->send) {
        return NET_NOT_CONNECTED;
    }

    if (!rdat->send_next) {
        return NET_WRONG_STATE;
    }

    if (msg->length > data->max_message_size) {
        return NET_BAD_MESSAGE;
    }

    Net_Pipe*        pipe = NULL;
    Net_MessageData* mdat = msg->internal_data;
    if (mdat) {
        pipe = mdat->pipe;
    }

    Net_Result result = NET_NOT_CONNECTED;
    if (pipe) {
        result = _net_pipe_send(pipe, msg->data, msg->length);
    } else {
        if (sock->state != NET_STATE_CONNECTED) {
            return NET_NOT_CONNECTED;
        }
        result = data->transport_ops->send(sock, msg->data, msg->length);
    }
    if (result == NET_OK) {
        rdat->send_next = false;
    }

    return result;
}

//------------------------------------------------------------------------------
// _net_reqrep_recv
//
// Receives one request/reply-protocol message. The socket must currently be in
// its receive phase: reply sockets start there, and request sockets enter it
// after a successful send.
//------------------------------------------------------------------------------

Net_Result _net_reqrep_recv(Net_Socket* sock,
                            void*       buffer,
                            usize       len,
                            usize*      out_recv_len,
                            Net_Pipe**  out_pipe)
{
    Net_SocketData*       data = _net_socket_data(sock);
    Net_ReqRepSocketData* rdat = _net_reqrep_socket_data(sock);

    if (sock->state != NET_STATE_CONNECTED &&
        sock->state != NET_STATE_WAITING_CONNECTION) {
        return NET_NOT_CONNECTED;
    }

    if (!data || !data->transport_ops || !data->transport_ops->recv_message) {
        return NET_NOT_CONNECTED;
    }

    if (rdat->send_next) {
        return NET_WRONG_STATE;
    }

    if (!data->has_pending_message) {
        Net_Result result = data->transport_ops->recv_message(sock);
        if (NET_FAILED(result)) {
            return result;
        }
    }

    Net_Result result =
        _net_socket_consume_pending(sock, buffer, len, out_recv_len, out_pipe);
    if (result == NET_OK) {
        rdat->send_next = true;
    }

    return result;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
