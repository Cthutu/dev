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
    NET_ERROR, // A general error occurred - dev needs to investigate
} Net_Result;

typedef struct {
    int fd; // Socket file descriptor
} Net_Socket;

typedef enum {
    NET_PROTO_TCP,
    NET_PROTO_UDP,
} Net_Protocol;

typedef struct {
    Net_Protocol proto;
    cstr         host;
    u16          port;
} Net_Endpoint;

//------------------------------------------------------------------------------
// Socket API

Net_Result net_socket(Net_Socket* out_sock, cstr url);
