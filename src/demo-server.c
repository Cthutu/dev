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

    return 0;
}
