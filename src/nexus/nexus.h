//------------------------------------------------------------------------------
// Nexus module
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <core/core.h>

//------------------------------------------------------------------------------
// Macros

#define NET_FAILED(result) ((result) != NET_OK)

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
    NET_ERROR,         // A general error occurred - dev needs to investigate
} Net_Result;

typedef struct {
    int fd; // Socket file descriptor
} Net_Socket;

typedef enum : u8 {
    NET_PROTO_TCP,
    NET_PROTO_UDP,
} Net_Protocol;

typedef struct {
    u8           ip[16]; // IPv6 address (or IPv4-mapped IPv6)
    string       host;
    u16          port;
    Net_Protocol proto;
} Net_Endpoint;

//------------------------------------------------------------------------------
// Socket API

Net_Socket net_socket(void);
void       net_close(Net_Socket* sock);

Net_Result net_bind(Net_Socket* sock, cstr url);
Net_Result net_connect(Net_Socket* sock, cstr url);

Net_Result net_send(Net_Socket* sock, const void* buffer, usize len);
Net_Result
net_recv(Net_Socket* sock, void* buffer, usize len, usize* out_recv_len);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
