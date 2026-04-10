//
// demo-server.c
//
// Server demo
//
//> use: core nexus

#include <nexus/nexus.h>

int run(int argc, char* argv[])
{
    UNUSED(argc);
    UNUSED(argv);
    prn("Demo server running...");

    cstr url        = "tcp://127.0.0.1:8080";

    Net_Socket sock = net_socket();
    if (NET_FAILED(net_bind(&sock, url))) {
        kill("Failed to create binding");
    }

    prn("Bound to %s", url);

    u8    buffer[1024];
    usize recv_len;

    prn("Waiting for incoming messages...");
    if (NET_FAILED(net_recv(&sock, buffer, sizeof(buffer), &recv_len))) {
        kill("Failed to receive data");
    }

    string msg = string_from(buffer, recv_len);
    prn("Received message: " STRINGP, STRINGV(msg));

    net_close(&sock);

    return 0;
}
