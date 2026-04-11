//------------------------------------------------------------------------------
// Telnet protocol
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <nexus/internal.h>

#include <errno.h>
#include <poll.h>
#include <sys/socket.h>

#define NET_TELNET_IAC 255u
#define NET_TELNET_DONT 254u
#define NET_TELNET_DO 253u
#define NET_TELNET_WONT 252u
#define NET_TELNET_WILL 251u
#define NET_TELNET_SB 250u
#define NET_TELNET_SE 240u
#define NET_TELNET_OPT_ECHO 1u
#define NET_TELNET_OPT_SUPPRESS_GO_AHEAD 3u
#define NET_TELNET_OPT_NAWS 31u

//------------------------------------------------------------------------------
// _net_telnet_tcp_transport_ops
//
// Transport operations for telnet-over-TCP sockets.
//------------------------------------------------------------------------------

const Net_TransportOps _net_telnet_tcp_transport_ops = {
    .send         = _net_tcp_send_text,
    .recv_message = _net_tcp_recv_telnet_message,
};

//------------------------------------------------------------------------------
// _net_telnet_state_done
//
// Releases any buffers owned by a telnet parser state.
//------------------------------------------------------------------------------

void _net_telnet_state_done(Net_TelnetState* state)
{
    if (!state) {
        return;
    }

    if (state->recv_buffer) {
        state->recv_buffer = mem_free(state->recv_buffer, __FILE__, __LINE__);
    }
    if (state->line_buffer) {
        state->line_buffer = mem_free(state->line_buffer, __FILE__, __LINE__);
    }

    *state = (Net_TelnetState){0};
}

internal void _net_telnet_ensure_recv_capacity(Net_TelnetState* state,
                                               usize            required)
{
    if (required <= state->recv_capacity) {
        return;
    }

    usize new_capacity =
        MAX(required, MAX(state->recv_capacity * 2, (usize)64));
    state->recv_buffer =
        mem_realloc(state->recv_buffer, new_capacity, __FILE__, __LINE__);
    state->recv_capacity = new_capacity;
}

internal void _net_telnet_ensure_line_capacity(Net_TelnetState* state,
                                               usize            required)
{
    if (required <= state->line_capacity) {
        return;
    }

    usize new_capacity =
        MAX(required, MAX(state->line_capacity * 2, (usize)64));
    state->line_buffer =
        mem_realloc(state->line_buffer, new_capacity, __FILE__, __LINE__);
    state->line_capacity = new_capacity;
}

internal void
_net_telnet_append_recv(Net_TelnetState* state, const u8* buffer, usize len)
{
    if (len == 0) {
        return;
    }

    _net_telnet_ensure_recv_capacity(state, state->recv_length + len);
    memcpy(state->recv_buffer + state->recv_length, buffer, len);
    state->recv_length += len;
}

internal void _net_telnet_append_line_byte(Net_TelnetState* state, u8 byte)
{
    _net_telnet_ensure_line_capacity(state, state->line_length + 1);
    state->line_buffer[state->line_length++] = byte;
}

internal void _net_telnet_reset_buffers(Net_TelnetState* state)
{
    state->recv_length           = 0;
    state->line_length           = 0;
    state->parse_state           = NET_TELNET_PARSE_NORMAL;
    state->negotiation_command   = 0;
    state->subnegotiation_option = 0;
    state->subnegotiation_length = 0;
    state->suppress_next_lf      = false;
}

internal void _net_telnet_subnegotiation_append(Net_TelnetState* state, u8 byte)
{
    if (state->subnegotiation_length < sizeof(state->subnegotiation_data)) {
        state->subnegotiation_data[state->subnegotiation_length++] = byte;
    }
}

internal void _net_telnet_finish_subnegotiation(Net_TelnetState* state)
{
    if (state->subnegotiation_option == NET_TELNET_OPT_NAWS &&
        state->subnegotiation_length >= 4) {
        state->width      = (u16)(((u16)state->subnegotiation_data[0] << 8) |
                             (u16)state->subnegotiation_data[1]);
        state->height     = (u16)(((u16)state->subnegotiation_data[2] << 8) |
                              (u16)state->subnegotiation_data[3]);
        state->has_bounds = true;
    }

    state->subnegotiation_option = 0;
    state->subnegotiation_length = 0;
}

internal bool
_net_telnet_negotiation_is_expected(Net_Socket* sock, u8 command, u8 option)
{
    Net_Telnet_Mode mode = _net_telnet_socket_data(sock)->telnet.mode;

    if (command == NET_TELNET_WILL && option == NET_TELNET_OPT_NAWS) {
        return true;
    }

    if (mode == NET_TELNET_CHARACTER_MODE) {
        if (command == NET_TELNET_WILL &&
            option == NET_TELNET_OPT_SUPPRESS_GO_AHEAD) {
            return true;
        }

        if (command == NET_TELNET_DO &&
            (option == NET_TELNET_OPT_ECHO ||
             option == NET_TELNET_OPT_SUPPRESS_GO_AHEAD)) {
            return true;
        }
    }

    return false;
}

internal Net_Result _net_telnet_send_negotiation_response(Net_Socket* sock,
                                                          int         fd,
                                                          u8          command,
                                                          u8          option)
{
    if (_net_telnet_negotiation_is_expected(sock, command, option)) {
        return NET_OK;
    }

    u8 response[3] = {NET_TELNET_IAC, 0, option};
    switch (command) {
    case NET_TELNET_DO:
        response[1] = NET_TELNET_WONT;
        break;
    case NET_TELNET_WILL:
        response[1] = NET_TELNET_DONT;
        break;
    default:
        return NET_OK;
    }

    bool      nonblocking = _net_socket_nonblocking(sock);
    TimePoint deadline =
        nonblocking ? time_now()
                    : _net_timeout_deadline_from_option(
                          _net_socket_data(sock)->options.send_timeout_ms);
    return _net_tcp_send_all_fd(
        fd, response, sizeof(response), deadline, nonblocking);
}

//------------------------------------------------------------------------------
// _net_telnet_request_session_state
//
// Requests the telnet options Nexus currently cares about for this session.
//------------------------------------------------------------------------------

Net_Result _net_telnet_request_session_state(Net_Socket* sock, int fd)
{
    bool      nonblocking = _net_socket_nonblocking(sock);
    TimePoint deadline =
        nonblocking ? time_now()
                    : _net_timeout_deadline_from_option(
                          _net_socket_data(sock)->options.send_timeout_ms);

    static const u8 request_naws[] = {
        NET_TELNET_IAC, NET_TELNET_DO, NET_TELNET_OPT_NAWS};
    Net_Result result = _net_tcp_send_all_fd(
        fd, request_naws, sizeof(request_naws), deadline, nonblocking);
    if (NET_FAILED(result)) {
        return result;
    }

    if (_net_telnet_socket_data(sock)->telnet.mode ==
        NET_TELNET_CHARACTER_MODE) {
        static const u8 request_character_mode[] = {
            NET_TELNET_IAC,
            NET_TELNET_WILL,
            NET_TELNET_OPT_ECHO,
            NET_TELNET_IAC,
            NET_TELNET_WILL,
            NET_TELNET_OPT_SUPPRESS_GO_AHEAD,
            NET_TELNET_IAC,
            NET_TELNET_DO,
            NET_TELNET_OPT_SUPPRESS_GO_AHEAD,
        };

        result = _net_tcp_send_all_fd(fd,
                                      request_character_mode,
                                      sizeof(request_character_mode),
                                      deadline,
                                      nonblocking);
        if (NET_FAILED(result)) {
            return result;
        }
    }

    return NET_OK;
}

internal Net_Result _net_telnet_state_extract_message(Net_Socket*      sock,
                                                      int              fd,
                                                      Net_TelnetState* state,
                                                      bool* out_ready)
{
    *out_ready     = false;
    usize consumed = 0;

    for (usize i = 0; i < state->recv_length; ++i) {
        u8 byte  = state->recv_buffer[i];
        consumed = i + 1;

        switch (state->parse_state) {
        case NET_TELNET_PARSE_NORMAL:
            if (byte == NET_TELNET_IAC) {
                state->parse_state = NET_TELNET_PARSE_IAC;
            } else if (state->mode == NET_TELNET_CHARACTER_MODE) {
                if (byte == '\r') {
                    _net_telnet_append_line_byte(state, '\n');
                    state->suppress_next_lf = true;
                    *out_ready              = true;
                    goto done;
                }

                if (byte == '\n') {
                    if (state->suppress_next_lf) {
                        state->suppress_next_lf = false;
                        break;
                    }

                    _net_telnet_append_line_byte(state, '\n');
                    *out_ready = true;
                    goto done;
                }

                state->suppress_next_lf = false;
                _net_telnet_append_line_byte(state, byte);
                *out_ready = true;
                goto done;
            } else if (byte == '\r') {
                // Ignore CR and wait for the LF terminator.
            } else if (byte == '\n') {
                *out_ready = true;
                goto done;
            } else {
                _net_telnet_append_line_byte(state, byte);
            }
            break;

        case NET_TELNET_PARSE_IAC:
            if (byte == NET_TELNET_IAC) {
                _net_telnet_append_line_byte(state, byte);
                state->parse_state = NET_TELNET_PARSE_NORMAL;
            } else if (byte == NET_TELNET_DO || byte == NET_TELNET_WILL) {
                state->negotiation_command = byte;
                state->parse_state         = NET_TELNET_PARSE_NEGOTIATION;
            } else if (byte == NET_TELNET_DONT || byte == NET_TELNET_WONT) {
                state->parse_state         = NET_TELNET_PARSE_NEGOTIATION;
                state->negotiation_command = byte;
            } else if (byte == NET_TELNET_SB) {
                state->parse_state           = NET_TELNET_PARSE_SUBNEGOTIATION;
                state->subnegotiation_option = 0;
                state->subnegotiation_length = 0;
            } else {
                state->parse_state = NET_TELNET_PARSE_NORMAL;
            }
            break;

        case NET_TELNET_PARSE_NEGOTIATION:
            {
                Net_Result result = _net_telnet_send_negotiation_response(
                    sock, fd, state->negotiation_command, byte);
                if (NET_FAILED(result)) {
                    return result;
                }
                state->parse_state         = NET_TELNET_PARSE_NORMAL;
                state->negotiation_command = 0;
            }
            break;

        case NET_TELNET_PARSE_SUBNEGOTIATION:
            if (state->subnegotiation_option == 0) {
                state->subnegotiation_option = byte;
                break;
            }

            if (byte == NET_TELNET_IAC) {
                state->parse_state = NET_TELNET_PARSE_SUBNEGOTIATION_IAC;
            } else {
                _net_telnet_subnegotiation_append(state, byte);
            }
            break;

        case NET_TELNET_PARSE_SUBNEGOTIATION_IAC:
            if (byte == NET_TELNET_SE) {
                _net_telnet_finish_subnegotiation(state);
                state->parse_state = NET_TELNET_PARSE_NORMAL;
            } else if (byte == NET_TELNET_IAC) {
                _net_telnet_subnegotiation_append(state, byte);
                state->parse_state = NET_TELNET_PARSE_SUBNEGOTIATION;
            } else {
                state->parse_state = NET_TELNET_PARSE_SUBNEGOTIATION;
            }
            break;
        }
    }

done:
    if (consumed > 0 && state->recv_length > consumed) {
        memmove(state->recv_buffer,
                state->recv_buffer + consumed,
                state->recv_length - consumed);
    }
    state->recv_length -= MIN(consumed, state->recv_length);
    return NET_OK;
}

//------------------------------------------------------------------------------
// _net_tcp_send_text_fd
//
// Sends one telnet message via the provided file descriptor.
//------------------------------------------------------------------------------

Net_Result
_net_tcp_send_text_fd(Net_Socket* sock, int fd, const void* buffer, usize len)
{
    if (len > _net_socket_data(sock)->max_message_size) {
        return NET_BAD_MESSAGE;
    }

    bool      nonblocking = _net_socket_nonblocking(sock);
    TimePoint deadline =
        nonblocking ? time_now()
                    : _net_timeout_deadline_from_option(
                          _net_socket_data(sock)->options.send_timeout_ms);

    if (len > 0) {
        Net_Result result =
            _net_tcp_send_all_fd(fd, buffer, len, deadline, nonblocking);
        if (NET_FAILED(result)) {
            return result;
        }
    }

    if (_net_telnet_socket_data(sock)->telnet.mode ==
        NET_TELNET_CHARACTER_MODE) {
        return NET_OK;
    }

    static const u8 line_end[] = {'\r', '\n'};
    return _net_tcp_send_all_fd(
        fd, line_end, sizeof(line_end), deadline, nonblocking);
}

Net_Result _net_tcp_send_text(Net_Socket* sock, const void* buffer, usize len)
{
    return _net_tcp_send_text_fd(sock, sock->fd, buffer, len);
}

internal void _net_telnet_store_pending(Net_Socket*      sock,
                                        Net_TelnetState* state,
                                        Net_Pipe*        pipe)
{
    _net_socket_store_pending(
        sock, state->line_buffer, state->line_length, pipe);
    state->line_length = 0;
}

internal Net_Result _net_tcp_recv_telnet_message_from_fd(Net_Socket*      sock,
                                                         int              fd,
                                                         Net_Pipe*        pipe,
                                                         Net_TelnetState* state,
                                                         TimePoint deadline)
{
    bool line_ready = false;

    Net_Result result =
        _net_telnet_state_extract_message(sock, fd, state, &line_ready);
    if (NET_FAILED(result)) {
        return result;
    }
    if (line_ready) {
        _net_telnet_store_pending(sock, state, pipe);
        return NET_OK;
    }

    bool nonblocking = _net_socket_nonblocking(sock);
    u8   recv_buffer[1024];

    while (true) {
        result = _net_poll_fd(fd, POLLIN, deadline, nonblocking);
        if (NET_FAILED(result)) {
            return result;
        }

        ssize_t recv_result = recv(fd, recv_buffer, sizeof(recv_buffer), 0);
        if (recv_result < 0) {
            if (errno == EINTR) {
                continue;
            }

            switch (errno) {
            case EAGAIN:
#if EWOULDBLOCK != EAGAIN
            case EWOULDBLOCK:
#endif
                return nonblocking ? NET_WOULD_BLOCK : NET_TIMEOUT;
            case ENETDOWN:
                return NET_NO_NETWORK;
            case ECONNRESET:
                return NET_CLOSED;
            default:
                _net_log_error();
                return NET_ERROR;
            }
        }

        if (recv_result == 0) {
            return NET_CLOSED;
        }

        _net_telnet_append_recv(state, recv_buffer, (usize)recv_result);
        result =
            _net_telnet_state_extract_message(sock, fd, state, &line_ready);
        if (NET_FAILED(result)) {
            return result;
        }

        usize max_message_size = _net_socket_data(sock)->max_message_size;
        if (state->line_length > max_message_size ||
            state->recv_length > max_message_size * 2) {
            _net_telnet_reset_buffers(state);
            return NET_BAD_MESSAGE;
        }

        if (line_ready) {
            _net_telnet_store_pending(sock, state, pipe);
            return NET_OK;
        }
    }
}

Net_Result _net_tcp_recv_telnet_message(Net_Socket* sock)
{
    bool      nonblocking = _net_socket_nonblocking(sock);
    TimePoint deadline =
        nonblocking ? time_now()
                    : _net_timeout_deadline_from_option(
                          _net_socket_data(sock)->options.recv_timeout_ms);

    if (sock->state == NET_STATE_CONNECTED) {
        return _net_tcp_recv_telnet_message_from_fd(
            sock,
            sock->fd,
            NULL,
            &_net_telnet_socket_data(sock)->telnet,
            deadline);
    }

    while (true) {
        Net_Pipe*  pipe   = NULL;
        Net_Result result = _net_tcp_poll_ready_pipe(sock, &pipe, deadline);
        if (NET_FAILED(result)) {
            return result;
        }

        if (!pipe) {
            continue;
        }

        result = _net_tcp_recv_telnet_message_from_fd(
            sock, pipe->tcp.fd, pipe, &pipe->tcp.telnet, deadline);
        if (result == NET_CLOSED) {
            _net_pipe_close(pipe);
            continue;
        }

        return result;
    }
}
