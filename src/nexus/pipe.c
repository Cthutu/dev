//------------------------------------------------------------------------------
// Pipe helpers
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <nexus/internal.h>

#include <unistd.h>

//------------------------------------------------------------------------------
// _net_pipe_alloc
//
// Allocates a new pipe record owned by the given socket and assigns it a stable
// identity. Pipe objects stay allocated until the parent socket closes so any
// message holding hidden pipe context does not end up with a dangling pointer.
//------------------------------------------------------------------------------

internal Net_Pipe* _net_pipe_alloc(Net_Socket* sock, Net_Pipe_Kind kind)
{
    Net_SocketData* data = _net_socket_data_ensure(sock);

    Net_Pipe* pipe       = mem_realloc(NULL, sizeof(*pipe), __FILE__, __LINE__);
    *pipe                = (Net_Pipe){
                       .kind  = kind,
                       .owner = sock,
                       .id    = ++data->next_pipe_id,
    };

    array_push(data->pipes, pipe);
    return pipe;
}

//------------------------------------------------------------------------------
// _net_pipe_create_tcp
//
// Creates a TCP pipe record for one accepted client connection.
//------------------------------------------------------------------------------

Net_Pipe* _net_pipe_create_tcp(Net_Socket* sock, int fd)
{
    Net_Pipe* pipe = _net_pipe_alloc(sock, NET_PIPE_TCP);
    pipe->tcp.fd   = fd;
    if (sock->kind == NET_SOCKET_TELNET) {
        pipe->tcp.telnet.mode = _net_telnet_socket_data(sock)->telnet.mode;
    }
    return pipe;
}

//------------------------------------------------------------------------------
// _net_pipe_find_or_create_udp
//
// Reuses an existing UDP pseudo-pipe for a sender address when one already
// exists, otherwise creates a new one.
//------------------------------------------------------------------------------

Net_Pipe* _net_pipe_find_or_create_udp(Net_Socket*               sock,
                                       const struct sockaddr_in* addr)
{
    Net_SocketData* data = _net_socket_data_ensure(sock);
    for (usize i = 0; i < array_count(data->pipes); ++i) {
        Net_Pipe* pipe = data->pipes[i];
        if (!pipe || pipe->kind != NET_PIPE_UDP || pipe->closed) {
            continue;
        }

        if (pipe->udp.addr.sin_family == addr->sin_family &&
            pipe->udp.addr.sin_port == addr->sin_port &&
            pipe->udp.addr.sin_addr.s_addr == addr->sin_addr.s_addr) {
            return pipe;
        }
    }

    Net_Pipe* pipe = _net_pipe_alloc(sock, NET_PIPE_UDP);
    pipe->udp.addr = *addr;
    return pipe;
}

//------------------------------------------------------------------------------
// _net_pipe_close
//
// Marks a pipe closed and releases any operating-system resources it owns. The
// pipe record itself remains allocated until the parent socket is closed.
//------------------------------------------------------------------------------

void _net_pipe_close(Net_Pipe* pipe)
{
    if (!pipe || pipe->closed) {
        return;
    }

    if (pipe->kind == NET_PIPE_TCP && pipe->tcp.fd >= 0) {
        close(pipe->tcp.fd);
        pipe->tcp.fd = -1;
        _net_telnet_state_done(&pipe->tcp.telnet);
    }

    pipe->closed = true;
}

//------------------------------------------------------------------------------
// _net_socket_close_pipes
//
// Closes and frees every pipe owned by a socket.
//------------------------------------------------------------------------------

void _net_socket_close_pipes(Net_Socket* sock)
{
    Net_SocketData* data = _net_socket_data(sock);
    if (!data) {
        return;
    }

    for (usize i = 0; i < array_count(data->pipes); ++i) {
        Net_Pipe* pipe = data->pipes[i];
        _net_pipe_close(pipe);
        if (pipe) {
            mem_free(pipe, __FILE__, __LINE__);
        }
    }

    array_free(data->pipes);
    data->pending_pipe = NULL;
}

//------------------------------------------------------------------------------
// _net_pipe_send
//
// Sends one full message via the communication path represented by the pipe.
//------------------------------------------------------------------------------

Net_Result _net_pipe_send(Net_Pipe* pipe, const void* buffer, usize len)
{
    if (!pipe || pipe->closed) {
        return NET_CLOSED;
    }

    switch (pipe->kind) {
    case NET_PIPE_TCP:
        if (pipe->owner->kind == NET_SOCKET_TELNET) {
            return _net_tcp_send_text_fd(
                pipe->owner, pipe->tcp.fd, buffer, len);
        }
        return _net_tcp_send_framed_fd(pipe->tcp.fd, buffer, len);
    case NET_PIPE_UDP:
        return _net_udp_send_to_addr(
            pipe->owner->fd, &pipe->udp.addr, buffer, len);
    default:
        return NET_ERROR;
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
