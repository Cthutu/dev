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
//      3. Create a message with net_message_create(&socket)
//      4. Send and receive messages over the socket.
//          - net_send(&message)
//          - net_recv(&message)
//      4. When done close socket: net_close(sock)
//
// Server:
//
//      1. Create a socket with net_socket().
//      2. Bind to a socket to recevie messages: net_bind(socket, url)
//      3. Create a message with net_message_create(&socket)
//      4. Send and receive messages over the socket.
//          - net_send(&message)
//          - net_recv(&message)
//      4. When done close socket: net_close(sock)
//------------------------------------------------------------------------------
// Macros

#define NET_FAILED(result) ((result) != NET_OK)
#define NET_MAX_MESSAGE_SIZE (1024 * 1024)
#define NET_WAIT_IMMEDIATE 0ull
#define NET_WAIT_INFINITE 0xffffffffffffffffull

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
    NET_TIMEOUT,          // An operation timed out before it could complete
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

typedef enum {
    NET_OPT_CONNECT_TIMEOUT_MS,
    NET_OPT_RECONNECT_INTERVAL_MS,
    NET_OPT_SEND_TIMEOUT_MS,
    NET_OPT_RECV_TIMEOUT_MS,
    NET_OPT_NONBLOCKING,
} Net_Option;

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

typedef struct {
    Net_Socket* socket;
    u8*         data;
    usize       length;
    usize       capacity;

    // Private implementation state. Callers should treat these fields as
    // unstable and avoid depending on them directly.
    void* internal_data;
} Net_Message;

//------------------------------------------------------------------------------
// Socket API

// Create a socket handle in the disconnected state. The handle itself does not
// allocate network resources until `net_bind` or `net_connect` succeeds.
Net_Socket net_socket(void);

// Close the socket and free any private message-framing state.
void net_close(Net_Socket* sock);

// Convert a `Net_Result` into a short readable string suitable for logging.
cstr net_result_string(Net_Result result);

// Bind a socket to the provided URL. For TCP, the socket becomes a server
// endpoint. For UDP, the socket can receive immediately after bind.
Net_Result net_bind(Net_Socket* sock, cstr url);

// Connect a socket to the provided URL. For TCP this establishes a connection.
// For UDP it sets the default peer for future send/recv calls.
Net_Result net_connect(Net_Socket* sock, cstr url);

// Set or query per-socket options. Timing options use milliseconds.
//
// `NET_OPT_CONNECT_TIMEOUT_MS` controls how long `net_connect` may keep
// retrying before it gives up. `NET_WAIT_IMMEDIATE` means one immediate
// attempt. `NET_WAIT_INFINITE` means retry forever.
//
// `NET_OPT_RECONNECT_INTERVAL_MS` controls the delay between retry attempts
// when the connect timeout allows waiting. A value of `0` means Nexus uses its
// default retry interval.
Net_Result net_set_option(Net_Socket* sock, Net_Option option, u64 value);
Net_Result net_get_option(Net_Socket* sock, Net_Option option, u64* out_value);

//------------------------------------------------------------------------------
// Message API

// Create a reusable message object associated with a specific socket.
Net_Message net_message_create(Net_Socket* sock);

// Release any storage owned by the message.
void net_message_done(Net_Message* msg);

// Clear the message payload while preserving its storage for reuse.
void net_message_clear(Net_Message* msg);

// Append raw or integer data to the message body. Multi-byte integers are
// stored in network byte order.
void net_message_append(Net_Message* msg, const void* buffer, usize len);
void net_message_append_string(Net_Message* msg, string value);
void net_message_append_u8(Net_Message* msg, u8 value);
void net_message_append_u16(Net_Message* msg, u16 value);
void net_message_append_u32(Net_Message* msg, u32 value);
void net_message_append_u64(Net_Message* msg, u64 value);

// Read and remove data from the front of the message body. Multi-byte integers
// are converted from network byte order. These return false when there is not
// enough unread data remaining in the message.
bool net_message_read(Net_Message* msg, void* buffer, usize len);
bool net_message_read_string(Net_Message* msg, string* out_value);
bool net_message_read_u8(Net_Message* msg, u8* out_value);
bool net_message_read_u16(Net_Message* msg, u16* out_value);
bool net_message_read_u32(Net_Message* msg, u32* out_value);
bool net_message_read_u64(Net_Message* msg, u64* out_value);

// Send one full message using the socket associated with the message object.
// For TCP the payload is framed internally with a 4-byte length prefix. For
// UDP the payload is sent as one datagram. Messages larger than
// `NET_MAX_MESSAGE_SIZE` fail with `NET_BAD_MESSAGE`.
Net_Result net_send(Net_Message* msg);

// Receive one full message into the message object, growing its storage as
// required. For TCP, Nexus reads one complete framed message. For UDP, one
// datagram is received. Zero-length messages are valid.
Net_Result net_recv(Net_Message* msg);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
