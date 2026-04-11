//------------------------------------------------------------------------------
// Internal header
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <nexus/nexus.h>

#include <netinet/in.h>

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
            int             fd;     // Accepted TCP client file descriptor
            Net_TelnetState telnet; // Telnet line parser for this TCP peer
        } tcp;

        struct {
            struct sockaddr_in addr; // Remembered UDP peer address
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
void                  _net_log_error(void);
int                   _net_create_socket(Net_Endpoint* endpoint);
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
void                  _net_endpoint_to_addr(Net_Endpoint*       endpoint,
                                            struct sockaddr_in* out_addr);

//------------------------------------------------------------------------------
// Pipe helpers

Net_Pipe*  _net_pipe_create_tcp(Net_Socket* sock, int fd);
Net_Pipe*  _net_pipe_find_or_create_udp(Net_Socket*               sock,
                                        const struct sockaddr_in* addr);
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
Net_Result _net_telnet_request_session_state(Net_Socket* sock, int fd);

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
Net_Result _net_tcp_send_framed_fd(int fd, const void* buffer, usize len);
Net_Result
_net_tcp_send_text_fd(Net_Socket* sock, int fd, const void* buffer, usize len);
Net_Result _net_udp_send_to_addr(int                       fd,
                                 const struct sockaddr_in* addr,
                                 const void*               buffer,
                                 usize                     len);

//------------------------------------------------------------------------------
// Shared TCP helper entry points

TimePoint _net_timeout_deadline_from_option(u64 timeout_ms);
bool      _net_socket_nonblocking(Net_Socket* sock);
int       _net_timeout_poll_ms(TimePoint deadline);
Net_Result
_net_poll_fd(int fd, short events, TimePoint deadline, bool nonblocking);
Net_Result _net_tcp_send_all_fd(
    int fd, const u8* buffer, usize len, TimePoint deadline, bool nonblocking);
Net_Result
_net_tcp_poll_ready_pipe(Net_Socket* sock, Net_Pipe** out, TimePoint deadline);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
