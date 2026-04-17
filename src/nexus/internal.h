//------------------------------------------------------------------------------
// Internal header
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <nexus/nexus.h>

//------------------------------------------------------------------------------
// Private runtime structures

typedef struct Net_TransportOps     Net_TransportOps;
typedef struct Net_ProtocolOps      Net_ProtocolOps;
typedef struct Net_MessageData      Net_MessageData;
typedef struct Net_Pipe             Net_Pipe;
typedef struct Net_TelnetState      Net_TelnetState;
typedef struct Net_SocketData       Net_SocketData;
typedef struct Net_ReqRepSocketData Net_ReqRepSocketData;
typedef struct Net_TelnetSocketData Net_TelnetSocketData;

//------------------------------------------------------------------------------
// Low-level socket abstraction
//
// Only `lowlevel.c` includes the native socket headers (BSD sockets on POSIX,
// Winsock on Windows). Every other file in the nexus module must reach the
// operating system through the `net_os_*` API declared below.

#define NET_OS_IP4_BYTES 4

// IPv4 endpoint in a form the lowlevel layer can translate to/from the native
// `sockaddr_in`. The ip bytes are stored in network/wire order; the port is
// stored in host byte order.
typedef struct {
    u8  ip[NET_OS_IP4_BYTES];
    u16 port;
} Net_Addr;

// Cross-platform mirror of the POSIX `poll` event bits. The lowlevel layer
// converts these to and from the native `struct pollfd` / `WSAPOLLFD` fields.
typedef enum : u16 {
    NET_POLL_IN   = 1u << 0,
    NET_POLL_OUT  = 1u << 1,
    NET_POLL_ERR  = 1u << 2,
    NET_POLL_HUP  = 1u << 3,
    NET_POLL_NVAL = 1u << 4,
} Net_PollEvents;

typedef struct {
    Net_Fd fd;      // Socket to watch
    u16    events;  // Requested Net_PollEvents mask
    u16    revents; // Returned Net_PollEvents mask
} Net_PollFd;

// Canonical error enum used by every `net_os_*` call. Each native platform
// error is mapped to the closest canonical code; the callers then convert to
// `Net_Result` for public APIs.
typedef enum : u8 {
    NET_OS_OK,
    NET_OS_WOULD_BLOCK,
    NET_OS_INTR,
    NET_OS_NET_DOWN,
    NET_OS_CONN_RESET,
    NET_OS_CONN_REFUSED,
    NET_OS_TIMED_OUT,
    NET_OS_HOST_UNREACH,
    NET_OS_NET_UNREACH,
    NET_OS_ADDR_IN_USE,
    NET_OS_ACCESS_DENIED,
    NET_OS_OUT_OF_FD,
    NET_OS_PIPE_BROKEN,
    NET_OS_PROTO_NOT_SUPPORTED,
    NET_OS_CLOSED,
    NET_OS_OTHER,
} Net_OsError;

typedef enum : u32 {
    NET_OS_RECV_PEEK     = 1u << 0,
    NET_OS_RECV_DONTWAIT = 1u << 1,
    NET_OS_RECV_TRUNC    = 1u << 2,
} Net_OsRecvFlags;

// Startup/teardown the OS networking stack (Winsock needs WSAStartup; no-op on
// POSIX).
void net_os_init(void);
void net_os_done(void);

// Canonical error for the most recent failed `net_os_*` call.
Net_OsError net_os_last_error(void);
void        net_os_log_error(void);

// Socket lifetime.
Net_Fd      net_os_socket(Net_Protocol proto);
void        net_os_close(Net_Fd fd);
void        net_os_shutdown_send(Net_Fd fd);
Net_OsError net_os_set_nonblocking(Net_Fd fd, bool nonblocking);
Net_OsError net_os_set_reuseaddr(Net_Fd fd);
Net_OsError net_os_available_bytes(Net_Fd fd, usize* out_available);

// Binding / connecting / accepting. IPv4 only for now.
Net_OsError net_os_bind(Net_Fd fd, const Net_Addr* addr);
Net_OsError net_os_listen(Net_Fd fd);
Net_OsError net_os_connect(Net_Fd fd, const Net_Addr* addr);
Net_OsError net_os_accept(Net_Fd fd, Net_Fd* out_client);
Net_OsError net_os_getpeername(Net_Fd fd, Net_Addr* out_addr);

// I/O. Return value: byte count on success, -1 on error (call
// `net_os_last_error()` for the canonical code).
isize net_os_send(Net_Fd fd, const void* buffer, usize len);
isize net_os_recv(Net_Fd fd, void* buffer, usize len, u32 flags);
isize net_os_sendto(Net_Fd fd, const void* buffer, usize len, const Net_Addr* addr);
isize net_os_recvfrom(
    Net_Fd fd, void* buffer, usize len, u32 flags, Net_Addr* out_addr);

// Poll. Returns number of ready descriptors (>= 0) or -1 on error.
// `timeout_ms` of -1 means wait forever; 0 means return immediately.
int net_os_poll(Net_PollFd* fds, usize n, int timeout_ms);

// Byte-order helpers that hide `htons`/`htonl`/`ntohs`/`ntohl`.
u16 net_os_hton16(u16 value);
u32 net_os_hton32(u32 value);
u16 net_os_ntoh16(u16 value);
u32 net_os_ntoh32(u32 value);

// Address helpers.
bool net_os_addr_equal(const Net_Addr* a, const Net_Addr* b);
void net_os_addr_from_endpoint(const Net_Endpoint* endpoint, Net_Addr* out_addr);
bool net_os_addr_format_ip(const Net_Addr* addr, char* out_buf, usize buf_size);

//------------------------------------------------------------------------------

typedef enum : u8 {
    NET_TELNET_PARSE_NORMAL,
    NET_TELNET_PARSE_IAC,
    NET_TELNET_PARSE_NEGOTIATION,
    NET_TELNET_PARSE_SUBNEGOTIATION,
    NET_TELNET_PARSE_SUBNEGOTIATION_IAC,
} Net_Telnet_Parse_State;

struct Net_TelnetState {
    u8*   recv_buffer;           // Raw bytes accumulated from the telnet stream
    u8*   line_buffer;           // Current decoded line without CRLF
    usize recv_length;           // Number of unread bytes in recv_buffer
    usize recv_capacity;         // Allocated capacity for recv_buffer
    usize line_length;           // Number of decoded line bytes ready so far
    usize line_capacity;         // Allocated capacity for line_buffer
    u16   width;                 // Most recently negotiated console width
    u16   height;                // Most recently negotiated console height
    u8    mode;                  // Current telnet input/output mode
    u8    parse_state;           // Current telnet parser state machine state
    u8    negotiation_command;   // Pending DO/DONT/WILL/WONT command byte
    u8    subnegotiation_option; // Current SB option byte
    u8    subnegotiation_data[8]; // Small SB payload buffer for NAWS parsing
    u8    subnegotiation_length;  // Number of bytes stored in SB payload
    bool  has_bounds;             // True once NAWS has produced valid bounds
    bool  suppress_next_lf;       // True when a CR already emitted a newline
};

typedef enum : u8 {
    NET_PIPE_TCP,
    NET_PIPE_UDP,
} Net_Pipe_Kind;

struct Net_Pipe {
    Net_Pipe_Kind kind;   // Underlying flow kind: TCP or UDP peer
    Net_Socket*   owner;  // Socket that owns this pipe
    u32           id;     // Stable pipe id for the lifetime of this pipe
    bool          closed; // True once the pipe has been closed locally

    union {
        struct {
            Net_Fd          fd;     // Accepted TCP client socket handle
            Net_TelnetState telnet; // Telnet line parser for this TCP peer
        } tcp;

        struct {
            Net_Addr addr; // Remembered UDP peer address
        } udp;
    };
};

typedef struct {
    u64 connect_timeout_ms;    // Total wait budget for connect retries
    u64 reconnect_interval_ms; // Delay between connect retry attempts
    u64 send_timeout_ms;       // Send wait budget before timing out
    u64 recv_timeout_ms;       // Receive wait budget before timing out
    u64 nonblocking;           // Non-zero when operations should not wait
} Net_SocketOptions;

struct Net_SocketData {
    Net_Socket_Kind         kind;          // Runtime-checked socket kind
    const Net_TransportOps* transport_ops; // Active transport behaviour
    const Net_ProtocolOps*  protocol_ops;  // Active socket protocol behaviour
    Net_SocketOptions       options;       // Configured per-socket options
    Array(Net_Pipe*) pipes;                // Managed TCP/UDP reply paths
    void*     pending_message;             // Retained full message payload
    Net_Pipe* pending_pipe;             // Pipe associated with pending_message
    usize     pending_message_len;      // Length of pending_message
    usize     pending_message_capacity; // Allocated size of pending_message
    usize     max_message_size;         // Maximum accepted/sent payload size
    u32       next_pipe_id;             // Monotonic pipe id source
    bool      has_pending_message;      // True when pending_message is valid
};

struct Net_ReqRepSocketData {
    Net_SocketData base;      // Shared socket runtime header
    bool           send_next; // True when the next req/rep operation must send
};

struct Net_TelnetSocketData {
    Net_SocketData  base;   // Shared socket runtime header
    Net_TelnetState telnet; // Connected-socket telnet parser state
};

struct Net_MessageData {
    u8*       string_storage;     // Scratch storage for decoded strings
    u8*       sender_url_storage; // Scratch storage for formatted sender URL
    Net_Pipe* pipe;               // Hidden reply route attached by receive
};

struct Net_TransportOps {
    Net_Result (*send)(Net_Socket* sock,
                       const void* buffer,
                       usize       len);                // Transport-level send
    Net_Result (*recv_message)(Net_Socket* sock); // Transport-level receive
};

struct Net_ProtocolOps {
    Net_Result (*send)(Net_Message* msg); // Protocol-aware send entry point
    Net_Result (*recv)(
        Net_Socket* sock,
        void*       buffer,
        usize       len,
        usize*      out_recv_len,
        Net_Pipe**  out_pipe); // Protocol-aware receive entry point
};

//------------------------------------------------------------------------------
// URL parsing

bool _net_parse_url(cstr url, Net_Endpoint* out_ep);

//------------------------------------------------------------------------------
// Shared socket helpers

Net_SocketData*       _net_socket_data(Net_Socket* sock);
Net_SocketData*       _net_socket_data_ensure(Net_Socket* sock);
Net_ReqRepSocketData* _net_reqrep_socket_data(Net_Socket* sock);
Net_TelnetSocketData* _net_telnet_socket_data(Net_Socket* sock);
void                  _net_socket_set_ops(Net_Socket*             sock,
                                          const Net_TransportOps* transport_ops,
                                          const Net_ProtocolOps*  protocol_ops);
Net_Fd                _net_create_socket(Net_Endpoint* endpoint);
void                  _net_socket_clear_pending(Net_Socket* sock);
void                  _net_socket_close_pipes(Net_Socket* sock);
void                  _net_socket_store_pending(Net_Socket* sock,
                                                const void* buffer,
                                                usize       len,
                                                Net_Pipe*   pipe);
Net_Result            _net_socket_send(Net_Message* msg);
Net_Result            _net_socket_recv(Net_Socket* sock,
                                       void*       buffer,
                                       usize       len,
                                       usize*      out_recv_len,
                                       Net_Pipe**  out_pipe);
Net_Result            _net_socket_consume_pending(Net_Socket* sock,
                                                  void*       buffer,
                                                  usize       len,
                                                  usize*      out_recv_len,
                                                  Net_Pipe**  out_pipe);

// Translate a canonical lowlevel error into a public `Net_Result` using the
// typical mapping (e.g. `NET_OS_NET_DOWN` → `NET_NO_NETWORK`). Returns
// `NET_ERROR` for errors that don't have a dedicated public code.
Net_Result _net_result_from_os_error(Net_OsError err);

//------------------------------------------------------------------------------
// Pipe helpers

Net_Pipe*  _net_pipe_create_tcp(Net_Socket* sock, Net_Fd fd);
Net_Pipe*  _net_pipe_find_or_create_udp(Net_Socket*     sock,
                                        const Net_Addr* addr);
void       _net_pipe_close(Net_Pipe* pipe);
Net_Result _net_pipe_send(Net_Pipe* pipe, const void* buffer, usize len);

//------------------------------------------------------------------------------
// Protocol entry points

Net_Result _net_message_send(Net_Message* msg);
Net_Result _net_message_recv(Net_Socket* sock,
                             void*       buffer,
                             usize       len,
                             usize*      out_recv_len,
                             Net_Pipe**  out_pipe);
Net_Result _net_reqrep_send(Net_Message* msg);
Net_Result _net_reqrep_recv(Net_Socket* sock,
                            void*       buffer,
                            usize       len,
                            usize*      out_recv_len,
                            Net_Pipe**  out_pipe);
Net_Result _net_tcp_send_text(Net_Socket* sock, const void* buffer, usize len);
Net_Result _net_tcp_recv_telnet_message(Net_Socket* sock);
void       _net_telnet_state_done(Net_TelnetState* state);
Net_Result _net_telnet_request_session_state(Net_Socket* sock, Net_Fd fd);

extern const Net_TransportOps _net_telnet_tcp_transport_ops;

//------------------------------------------------------------------------------
// Transport entry points

Net_Result _net_tcp_bind(Net_Socket* sock, Net_Endpoint* endpoint);
Net_Result _net_udp_bind(Net_Socket* sock, Net_Endpoint* endpoint);
Net_Result _net_tcp_connect(Net_Socket* sock, Net_Endpoint* endpoint);
Net_Result _net_udp_connect(Net_Socket* sock, Net_Endpoint* endpoint);
Net_Result _net_tcp_send(Net_Socket* sock, const void* buffer, usize len);
Net_Result _net_udp_send(Net_Socket* sock, const void* buffer, usize len);
Net_Result _net_tcp_recv_message(Net_Socket* sock);
Net_Result _net_udp_recv_message(Net_Socket* sock);
Net_Result _net_tcp_send_framed_fd(Net_Fd fd, const void* buffer, usize len);
Net_Result _net_tcp_send_text_fd(Net_Socket* sock,
                                 Net_Fd      fd,
                                 const void* buffer,
                                 usize       len);
Net_Result _net_udp_send_to_addr(Net_Fd          fd,
                                 const Net_Addr* addr,
                                 const void*     buffer,
                                 usize           len);

//------------------------------------------------------------------------------
// Shared TCP helper entry points

TimePoint _net_timeout_deadline_from_option(u64 timeout_ms);
bool      _net_socket_nonblocking(Net_Socket* sock);
int       _net_timeout_poll_ms(TimePoint deadline);
Net_Result
_net_poll_fd(Net_Fd fd, u16 events, TimePoint deadline, bool nonblocking);
Net_Result _net_tcp_send_all_fd(Net_Fd    fd,
                                const u8* buffer,
                                usize     len,
                                TimePoint deadline,
                                bool      nonblocking);
Net_Result
_net_tcp_poll_ready_pipe(Net_Socket* sock, Net_Pipe** out, TimePoint deadline);
void _net_tcp_close_telnet_fd(Net_Fd fd);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
