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

    Net_Socket sock;
    if (NET_FAILED(net_socket(&sock, "tcp://localhost:8080"))) {
        kill("Failed to create socket");
    }

    return 0;
}
