//------------------------------------------------------------------------------
// Nexus module
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <core/core.h>

//------------------------------------------------------------------------------
// Client:
//
//      1. Create a socket with net_socket().
//      2. Connect to a URL via the socket: net_connect(socket, url)
//      3. Send and receive messages over the socket.
//          - net_send(socket, buffer, len)
//          - net_recv(socket, buffer, len, recv_len)
//      4. When done close socket: net_close(sock)
//
// Server:
//
//      1. Create a socket with net_socket().
//      2. Bind to a socket to recevie messages: net_bind(socket, url)
//      3. Send and receive messages over the socket.
//          - net_send(socket, buffer, len)
//          - net_recv(socket, buffer, len, recv_len)
//      4. When done close socket: net_close(sock)
//------------------------------------------------------------------------------
// Macros

#define NET_FAILED(result) ((result) != NET_OK)
#define NET_MAX_MESSAGE_SIZE (1024 * 1024)

//------------------------------------------------------------------------------
// Types

typedef enum {
    NET_OK = 0,
    NET_INVALID_URL, // The URL is invalid (e.g. missing port, host, etc)
    NET_NO_NETWORK,  // No network connection
    NET_OUT_OF_FD,   // No file descriptors available
    NET_PROTOCOL_NOT_SUPPORTED, // The protocol doesn't exist (e.g. TCP, UDP
    // etc).
    NET_PORT_IN_USE,   // The port is already in use by another socket
    NET_ACCESS_DENIED, // Permission denied when trying to bind to the port
    NET_SOCKET_BUSY,   // The socket is already in use (e.g. trying to bind or
                       // connect a socket that's already connected or bound)
    NET_NOT_CONNECTED, // The socket is not ready for send/recv yet
    NET_BUFFER_TOO_SMALL, // Receive buffer cannot hold the full pending message
    NET_BAD_MESSAGE,      // The remote side sent an invalid message
    NET_CLOSED,           // The peer closed the connection
    NET_ERROR,            // A general error occurred - dev needs to investigate
} Net_Result;

typedef enum {
    NET_STATE_DISCONNECTED,
    NET_STATE_WAITING_CONNECTION,
    NET_STATE_CONNECTED,
} Net_State;

typedef enum : u8 {
    NET_PROTO_TCP,
    NET_PROTO_UDP,
} Net_Protocol;

typedef enum : u8 {
    NET_SOCKET_BASIC,
} Net_Socket_Kind;

typedef struct {
    Net_State       state;
    int             fd; // Socket file descriptor
    Net_Protocol    proto;
    Net_Socket_Kind kind;

    // Private implementation state. Callers should treat these fields as
    // unstable and avoid depending on them directly.
    void* internal_data;
} Net_Socket;

typedef struct {
    u8           ip[16]; // IPv6 address (or IPv4-mapped IPv6)
    string       host;
    u16          port;
    Net_Protocol proto;
} Net_Endpoint;

//------------------------------------------------------------------------------
// Socket API

// Create a socket handle in the disconnected state. The handle itself does not
// allocate network resources until `net_bind` or `net_connect` succeeds.
Net_Socket net_socket(void);

// Close the socket and free any private message-framing state.
void net_close(Net_Socket* sock);

// Convert a `Net_Result` into a short readable string suitable for logging.
cstr net_result_string(Net_Result result);

// Bind a socket to the provided URL. For TCP, the socket becomes a simple
// single-peer server endpoint and will accept an incoming connection during the
// first `net_recv`. For UDP, the socket can receive immediately after bind.
Net_Result net_bind(Net_Socket* sock, cstr url);

// Connect a socket to the provided URL. For TCP this establishes a connection.
// For UDP it sets the default peer for future send/recv calls.
Net_Result net_connect(Net_Socket* sock, cstr url);

// Send one message over the socket. For TCP the message is framed internally
// with a 4-byte length prefix. For UDP the message is sent as one datagram.
// Messages larger than `NET_MAX_MESSAGE_SIZE` fail with `NET_BAD_MESSAGE`.
Net_Result net_send(Net_Socket* sock, const void* buffer, usize len);

// Receive one message from the socket.
//
// For TCP, Nexus reads a full framed message and only returns once one complete
// message is available. For UDP, one datagram is received.
//
// If the caller buffer is too small, this returns `NET_BUFFER_TOO_SMALL`,
// leaves the message pending inside the socket, and writes the required payload
// size to `out_recv_len` when provided.
//
// If `buffer == NULL`, the pending message is dropped instead of copied out. If
// `out_recv_len` is provided, it receives the dropped message size.
//
// Zero-length messages are valid.
Net_Result
net_recv(Net_Socket* sock, void* buffer, usize len, usize* out_recv_len);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
