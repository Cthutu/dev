//------------------------------------------------------------------------------
// Socket functions
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <nexus/internal.h>

#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>

//------------------------------------------------------------------------------
// net_socket

Net_Socket net_socket(void) { return (Net_Socket){.fd = -1}; }

internal int _net_create_socket(Net_Endpoint* endpoint)
{
    int sock_type  = 0;
    int proto_type = 0;
    switch (endpoint->proto) {
    case NET_PROTO_TCP:
        sock_type  = SOCK_STREAM;
        proto_type = IPPROTO_TCP;
        break;
    case NET_PROTO_UDP:
        sock_type  = SOCK_DGRAM;
        proto_type = IPPROTO_UDP;
        break;
    default:
        return -1; // Invalid protocol
    }

    int fd = socket(AF_INET, sock_type, proto_type);
    if (fd < 0) {
        return -1; // Socket creation failed
    }

    return fd;
}

Net_Result net_bind(Net_Socket* out_sock, cstr url)
{
    //
    // Process the URL
    //

    Net_Endpoint endpoint;
    if (!_net_parse_url(url, &endpoint)) {
        return NET_INVALID_URL;
    }

    //
    // Create a socket compatible with the URL
    //

    int fd = _net_create_socket(&endpoint);
    if (fd < 0) {
        switch (errno) {
        case EMFILE:
        case ENFILE:
            return NET_OUT_OF_FD;

        case ENETDOWN:
            return NET_NO_NETWORK;

        case EPROTONOSUPPORT:
            return NET_PROTOCOL_NOT_SUPPORTED;

        default:
            return NET_ERROR;
        }
    }

    out_sock->fd = fd;
    return NET_OK;
}
