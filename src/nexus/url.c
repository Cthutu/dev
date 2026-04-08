//------------------------------------------------------------------------------
// URL parsing
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <nexus/internal.h>

//------------------------------------------------------------------------------
// _net_parse_url

bool _net_parse_url(cstr url, Net_Endpoint* out_ep)
{
    string url_string = string_from_cstr(url);

    string proto_str;
    string host_str;
    string port_str;

    if (!string_split(url_string, "://", &proto_str, &url_string)) {
        return false;
    }
    if (!string_split(url_string, ":", &host_str, &port_str)) {
        return false;
    }

    //
    // Protocol parsing
    //

    if (string_equals_cstr(proto_str, "tcp")) {
        out_ep->proto = NET_PROTO_TCP;
    } else if (string_equals_cstr(proto_str, "udp")) {
        out_ep->proto = NET_PROTO_UDP;
    } else {
        return false;
    }

    //
    // Host parsing
    //

    out_ep->host = (cstr)host_str.data;

    //
    // Port parsing
    //

    u64 port;
    if (!string_to_u64(port_str, &port) || port > 65535) {
        return false;
    }
    out_ep->port = (u16)port;

    return true;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
