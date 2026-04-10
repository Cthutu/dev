//
// demo-client.c
//
// Client demo that sends a message to the server demo
//
//> use: core nexus

#include <nexus/nexus.h>

int run(int argc, char* argv[])
{
    UNUSED(argc);
    UNUSED(argv);
    prn("Demo client running...");

    cstr   url      = "tcp://127.0.0.1:8080";
    string message  = S("Hello from the client!");

    Net_Socket sock = net_socket();
    if (NET_FAILED(net_connect(&sock, url))) {
        kill("Failed to connect to server");
    }

    prn("Connected to %s", url);
    prn("Sending message: " STRINGP, STRINGV(message));

    if (NET_FAILED(net_send(&sock, message.data, message.count))) {
        kill("Failed to send data");
    }

    net_close(&sock);

    return 0;
}
