//------------------------------------------------------------------------------
// Message objects
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <nexus/internal.h>

#include <arpa/inet.h>
#include <sys/socket.h>

//------------------------------------------------------------------------------
// _net_hton_u64
//
// Converts a 64-bit integer to network byte order.
//------------------------------------------------------------------------------

internal u64 _net_hton_u64(u64 value)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return ((u64)htonl((u32)(value & 0xffffffffull)) << 32) |
           (u64)htonl((u32)(value >> 32));
#else
    return value;
#endif
}

//------------------------------------------------------------------------------
// _net_ntoh_u64
//
// Converts a 64-bit integer from network byte order.
//------------------------------------------------------------------------------

internal u64 _net_ntoh_u64(u64 value)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return ((u64)ntohl((u32)(value & 0xffffffffull)) << 32) |
           (u64)ntohl((u32)(value >> 32));
#else
    return value;
#endif
}

//------------------------------------------------------------------------------
// _net_message_data_ensure
//
// Creates the private runtime block for a message on first use.
//------------------------------------------------------------------------------

internal Net_MessageData* _net_message_data_ensure(Net_Message* msg)
{
    Net_MessageData* data = msg->internal_data;
    if (!data) {
        data  = mem_realloc(NULL, sizeof(*data), __FILE__, __LINE__);
        *data = (Net_MessageData){0};
        msg->internal_data = data;
    }
    return data;
}

//------------------------------------------------------------------------------
// _net_message_clear_pipe
//
// Clears any hidden pipe context retained by the message.
//------------------------------------------------------------------------------

internal void _net_message_clear_pipe(Net_Message* msg)
{
    Net_MessageData* data = msg->internal_data;
    if (!data) {
        return;
    }

    data->pipe = NULL;
}

//------------------------------------------------------------------------------
// _net_message_set_route
//
// Stores hidden pipe metadata inside the message runtime block.
//------------------------------------------------------------------------------

internal void _net_message_set_pipe(Net_Message* msg, Net_Pipe* pipe)
{
    Net_MessageData* data = _net_message_data_ensure(msg);
    data->pipe            = pipe;
}

//------------------------------------------------------------------------------
// _net_message_ensure_capacity
//
// Grows the message storage when required so appends and receives have enough
// contiguous room for the payload.
//------------------------------------------------------------------------------

internal void _net_message_ensure_capacity(Net_Message* msg, usize required)
{
    if (required <= msg->capacity) {
        return;
    }

    usize new_capacity = MAX(required, MAX(msg->capacity * 2, (usize)1));
    msg->data     = mem_realloc(msg->data, new_capacity, __FILE__, __LINE__);
    msg->capacity = new_capacity;
}

//------------------------------------------------------------------------------
// net_message_create
//
// Creates a reusable message associated with a specific socket.
//------------------------------------------------------------------------------

Net_Message net_message_create(Net_Socket* sock)
{
    return (Net_Message){
        .socket = sock,
    };
}

//------------------------------------------------------------------------------
// net_message_done
//
// Releases any storage owned by the message and resets it to an empty state.
//------------------------------------------------------------------------------

void net_message_done(Net_Message* msg)
{
    Net_MessageData* data = msg->internal_data;
    if (data && data->string_storage) {
        data->string_storage =
            mem_free(data->string_storage, __FILE__, __LINE__);
    }
    if (data) {
        msg->internal_data = mem_free(msg->internal_data, __FILE__, __LINE__);
    }
    if (msg->data) {
        msg->data = mem_free(msg->data, __FILE__, __LINE__);
    }
    msg->length        = 0;
    msg->capacity      = 0;
    msg->internal_data = NULL;
}

//------------------------------------------------------------------------------
// net_message_clear
//
// Clears the message payload while preserving the allocated storage for reuse.
//------------------------------------------------------------------------------

void net_message_clear(Net_Message* msg) { msg->length = 0; }

//------------------------------------------------------------------------------
// net_message_append
//
// Appends raw bytes to the end of the message body.
//------------------------------------------------------------------------------

void net_message_append(Net_Message* msg, const void* buffer, usize len)
{
    if (len == 0) {
        return;
    }

    _net_message_ensure_capacity(msg, msg->length + len);
    memcpy(msg->data + msg->length, buffer, len);
    msg->length += len;
}

//------------------------------------------------------------------------------
// net_message_append_u8
//
// Appends a single byte to the message body.
//------------------------------------------------------------------------------

void net_message_append_u8(Net_Message* msg, u8 value)
{
    net_message_append(msg, &value, sizeof(value));
}

//------------------------------------------------------------------------------
// net_message_append_string
//
// Appends a length-prefixed string to the message body. The string length is
// stored as a 32-bit integer in network byte order followed by the raw bytes.
//------------------------------------------------------------------------------

void net_message_append_string(Net_Message* msg, string value)
{
    ASSERT(value.count <= 0xffffffffu, "String too large for message encoding");

    net_message_append_u32(msg, (u32)value.count);
    net_message_append(msg, value.data, value.count);
}

//------------------------------------------------------------------------------
// net_message_append_u16
//
// Appends a 16-bit integer in network byte order.
//------------------------------------------------------------------------------

void net_message_append_u16(Net_Message* msg, u16 value)
{
    u16 network_value = htons(value);
    net_message_append(msg, &network_value, sizeof(network_value));
}

//------------------------------------------------------------------------------
// net_message_append_u32
//
// Appends a 32-bit integer in network byte order.
//------------------------------------------------------------------------------

void net_message_append_u32(Net_Message* msg, u32 value)
{
    u32 network_value = htonl(value);
    net_message_append(msg, &network_value, sizeof(network_value));
}

//------------------------------------------------------------------------------
// net_message_append_u64
//
// Appends a 64-bit integer in network byte order.
//------------------------------------------------------------------------------

void net_message_append_u64(Net_Message* msg, u64 value)
{
    u64 network_value = _net_hton_u64(value);
    net_message_append(msg, &network_value, sizeof(network_value));
}

//------------------------------------------------------------------------------
// net_message_read
//
// Reads bytes from the front of the message body and removes them from the
// message.
//------------------------------------------------------------------------------

bool net_message_read(Net_Message* msg, void* buffer, usize len)
{
    if (len > msg->length) {
        return false;
    }

    if (len > 0) {
        memcpy(buffer, msg->data, len);
        memmove(msg->data, msg->data + len, msg->length - len);
    }

    msg->length -= len;
    return true;
}

//------------------------------------------------------------------------------
// net_message_read_u8
//
// Reads and removes a single byte from the front of the message.
//------------------------------------------------------------------------------

bool net_message_read_u8(Net_Message* msg, u8* out_value)
{
    return net_message_read(msg, out_value, sizeof(*out_value));
}

//------------------------------------------------------------------------------
// net_message_read_string
//
// Reads and removes a length-prefixed string from the front of the message.
// The returned string points into message-owned storage and remains valid until
// the next successful string read or `net_message_done`.
//------------------------------------------------------------------------------

bool net_message_read_string(Net_Message* msg, string* out_value)
{
    Net_MessageData* data   = _net_message_data_ensure(msg);
    u32              length = 0;
    if (!net_message_read_u32(msg, &length)) {
        return false;
    }

    if (length > msg->length) {
        return false;
    }

    if (length > 0) {
        data->string_storage =
            mem_realloc(data->string_storage, length, __FILE__, __LINE__);
        if (!net_message_read(msg, data->string_storage, length)) {
            return false;
        }
    } else if (data->string_storage) {
        data->string_storage =
            mem_free(data->string_storage, __FILE__, __LINE__);
    }

    *out_value = string_from(data->string_storage, length);
    return true;
}

//------------------------------------------------------------------------------
// net_message_read_u16
//
// Reads and removes a 16-bit integer, converting it from network byte order.
//------------------------------------------------------------------------------

bool net_message_read_u16(Net_Message* msg, u16* out_value)
{
    u16 network_value = 0;
    if (!net_message_read(msg, &network_value, sizeof(network_value))) {
        return false;
    }

    *out_value = ntohs(network_value);
    return true;
}

//------------------------------------------------------------------------------
// net_message_read_u32
//
// Reads and removes a 32-bit integer, converting it from network byte order.
//------------------------------------------------------------------------------

bool net_message_read_u32(Net_Message* msg, u32* out_value)
{
    u32 network_value = 0;
    if (!net_message_read(msg, &network_value, sizeof(network_value))) {
        return false;
    }

    *out_value = ntohl(network_value);
    return true;
}

//------------------------------------------------------------------------------
// net_message_read_u64
//
// Reads and removes a 64-bit integer, converting it from network byte order.
//------------------------------------------------------------------------------

bool net_message_read_u64(Net_Message* msg, u64* out_value)
{
    u64 network_value = 0;
    if (!net_message_read(msg, &network_value, sizeof(network_value))) {
        return false;
    }

    *out_value = _net_ntoh_u64(network_value);
    return true;
}

//------------------------------------------------------------------------------
// net_send_msg
//
// Sends the message body over the message's associated socket.
//------------------------------------------------------------------------------

Net_Result net_send_msg(Net_Message* msg)
{
    if (!msg->socket) {
        return NET_NOT_CONNECTED;
    }

    Net_MessageData* data = msg->internal_data;
    if (data && data->pipe) {
        // Reply via the originating pipe when the message carries one.
        return _net_pipe_send(data->pipe, msg->data, msg->length);
    }

    return net_send(msg->socket, msg->data, msg->length);
}

//------------------------------------------------------------------------------
// net_recv_msg
//
// Receives one full message into the message body, growing storage as required.
//------------------------------------------------------------------------------

Net_Result net_recv_msg(Net_Message* msg)
{
    if (!msg->socket) {
        return NET_NOT_CONNECTED;
    }

    Net_SocketData* socket_data = _net_socket_data(msg->socket);
    if (msg->capacity == 0) {
        _net_message_ensure_capacity(msg, 1);
    }

    if (!socket_data || !socket_data->transport_ops ||
        !socket_data->transport_ops->recv_message) {
        return NET_NOT_CONNECTED;
    }

    while (true) {
        if (!socket_data->has_pending_message) {
            Net_Result result =
                socket_data->transport_ops->recv_message(msg->socket);
            if (NET_FAILED(result)) {
                return result;
            }
        }

        if (socket_data->pending_pipe) {
            _net_message_set_pipe(msg, socket_data->pending_pipe);
        } else {
            _net_message_clear_pipe(msg);
        }

        usize      recv_len = 0;
        Net_Result result   = _net_socket_consume_pending(
            msg->socket, msg->data, msg->capacity, &recv_len);
        if (result == NET_BUFFER_TOO_SMALL) {
            _net_message_ensure_capacity(msg, recv_len == 0 ? 1 : recv_len);
            continue;
        }

        if (NET_FAILED(result)) {
            return result;
        }

        msg->length = recv_len;
        return NET_OK;
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
