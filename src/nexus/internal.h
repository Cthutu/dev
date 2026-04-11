//------------------------------------------------------------------------------
// Internal header
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <nexus/nexus.h>

#include <netinet/in.h>

//------------------------------------------------------------------------------
// URL parsing

bool _net_parse_url(cstr url, Net_Endpoint* out_ep);

//------------------------------------------------------------------------------
// Shared socket helpers

void _net_log_error(void);
int  _net_create_socket(Net_Endpoint* endpoint);
void _net_socket_clear_pending(Net_Socket* sock);
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
