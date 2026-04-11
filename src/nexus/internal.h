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

typedef struct {
    const Net_TransportOps* transport_ops;
    const Net_PatternOps*   pattern_ops;
    void*                   pending_message;
    usize                   pending_message_len;
    usize                   pending_message_capacity;
    usize                   max_message_size;
    bool                    has_pending_message;
} Net_SocketData;

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
void _net_socket_store_pending(Net_Socket* sock, const void* buffer, usize len);
Net_Result _net_socket_consume_pending(Net_Socket* sock,
                                       void*       buffer,
                                       usize       len,
                                       usize*      out_recv_len);
void       _net_endpoint_to_addr(Net_Endpoint*       endpoint,
                                 struct sockaddr_in* out_addr);

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

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
