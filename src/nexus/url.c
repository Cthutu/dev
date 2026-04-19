//------------------------------------------------------------------------------
// URL parsing
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <nexus/internal.h>

//------------------------------------------------------------------------------
// _net_parse_ip4_address

internal bool _net_process_ip4_address(Net_Endpoint* in_out_ep)
{
    u64   dot_count = 0;
    u64   num_count = 0;
    usize ip_index  = 0;

    string host     = in_out_ep->host;

    enum {
        EXPECT_NUM,
        EXPECT_DOT,
    } state = EXPECT_NUM;

    for (usize i = 0; i < host.count;) {
        switch (state) {
        case EXPECT_NUM:
            {
                u64 num = 0;
                if (host.data[i] < '0' || host.data[i] > '9') {
                    return false; // Expected number
                }
                while (i < host.count && host.data[i] >= '0' &&
                       host.data[i] <= '9') {
                    num = num * 10 + (host.data[i] - '0');
                    i++;
                    if (num > 255) {
                        return false; // Invalid number in IP
                    }
                }
                if (num_count >= 4) {
                    return false; // Too many numbers
                }
                num_count++;
                state                     = EXPECT_DOT;
                in_out_ep->ip[ip_index++] = (u8)num;
            }
            break;

        case EXPECT_DOT:
            if (host.data[i++] != '.') {
                return false; // Expected dot
            }
            dot_count++;
            if (dot_count > 3) {
                return false; // Too many dots
            }
            state = EXPECT_NUM;
            break;
        }
    }

    return true;
}

//------------------------------------------------------------------------------
// _net_parse_url

bool _net_parse_url(cstr url, Net_Endpoint* out_ep)
{
    string url_string = string_from_cstr(url);

    string proto_str;
    string host_str;
    string port_str;

    if (!string_split_once(url_string, "://", &proto_str, &url_string)) {
        return false;
    }
    if (!string_split_once(url_string, ":", &host_str, &port_str)) {
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

    out_ep->host = host_str;

    if (!_net_process_ip4_address(out_ep)) {
        // TODO: Do a DNS look up
        return false;
    }

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
