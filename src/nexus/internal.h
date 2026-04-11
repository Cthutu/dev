//------------------------------------------------------------------------------
// Internal header
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <nexus/nexus.h>

#include <netinet/in.h>

//------------------------------------------------------------------------------
// Private runtime structures

typedef struct Net_TransportOps Net_TransportOps;
typedef struct Net_PatternOps   Net_PatternOps;
typedef struct Net_MessageData  Net_MessageData;
typedef struct Net_Pipe         Net_Pipe;

typedef enum : u8 {
    NET_PIPE_TCP,
    NET_PIPE_UDP,
} Net_Pipe_Kind;

struct Net_Pipe {
    Net_Pipe_Kind kind;
    Net_Socket*   owner;
    u32           id;
    bool          closed;

    union {
        struct {
            int fd;
        } tcp;

        struct {
            struct sockaddr_in addr;
        } udp;
    };
};

typedef struct {
    u64 connect_timeout_ms;
    u64 reconnect_interval_ms;
    u64 send_timeout_ms;
    u64 recv_timeout_ms;
    u64 nonblocking;
} Net_SocketOptions;

typedef struct {
    const Net_TransportOps* transport_ops;
    const Net_PatternOps*   pattern_ops;
    Net_SocketOptions       options;
    Array(Net_Pipe*) pipes;
    void*     pending_message;
    Net_Pipe* pending_pipe;
    usize     pending_message_len;
    usize     pending_message_capacity;
    usize     max_message_size;
    u32       next_pipe_id;
    bool      has_pending_message;
} Net_SocketData;

struct Net_MessageData {
    u8*       string_storage;
    Net_Pipe* pipe;
};

struct Net_TransportOps {
    Net_Result (*send)(Net_Socket* sock, const void* buffer, usize len);
    Net_Result (*recv_message)(Net_Socket* sock);
};

struct Net_PatternOps {
    Net_Result (*send)(Net_Socket* sock, const void* buffer, usize len);
    Net_Result (*recv)(Net_Socket* sock,
                       void*       buffer,
                       usize       len,
                       usize*      out_recv_len);
};

//------------------------------------------------------------------------------
// URL parsing

bool _net_parse_url(cstr url, Net_Endpoint* out_ep);

//------------------------------------------------------------------------------
// Shared socket helpers

Net_SocketData* _net_socket_data(Net_Socket* sock);
Net_SocketData* _net_socket_data_ensure(Net_Socket* sock);
void            _net_socket_set_ops(Net_Socket*             sock,
                                    const Net_TransportOps* transport_ops,
                                    const Net_PatternOps*   pattern_ops);
void            _net_log_error(void);
int             _net_create_socket(Net_Endpoint* endpoint);
void            _net_socket_clear_pending(Net_Socket* sock);
void            _net_socket_close_pipes(Net_Socket* sock);
void            _net_socket_store_pending(Net_Socket* sock,
                                          const void* buffer,
                                          usize       len,
                                          Net_Pipe*   pipe);
Net_Result _net_socket_send(Net_Socket* sock, const void* buffer, usize len);
Net_Result _net_socket_recv(Net_Socket* sock,
                            void*       buffer,
                            usize       len,
                            usize*      out_recv_len);
Net_Result _net_socket_consume_pending(Net_Socket* sock,
                                       void*       buffer,
                                       usize       len,
                                       usize*      out_recv_len);
void       _net_endpoint_to_addr(Net_Endpoint*       endpoint,
                                 struct sockaddr_in* out_addr);

//------------------------------------------------------------------------------
// Pipe helpers

Net_Pipe*  _net_pipe_create_tcp(Net_Socket* sock, int fd);
Net_Pipe*  _net_pipe_find_or_create_udp(Net_Socket*               sock,
                                        const struct sockaddr_in* addr);
void       _net_pipe_close(Net_Pipe* pipe);
Net_Result _net_pipe_send(Net_Pipe* pipe, const void* buffer, usize len);

//------------------------------------------------------------------------------
// Pattern entry points

Net_Result _net_message_send(Net_Socket* sock, const void* buffer, usize len);
Net_Result _net_message_recv(Net_Socket* sock,
                             void*       buffer,
                             usize       len,
                             usize*      out_recv_len);

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
Net_Result _net_udp_send_to_addr(int                       fd,
                                 const struct sockaddr_in* addr,
                                 const void*               buffer,
                                 usize                     len);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
