//
// demo-server.c
//
// Server demo
//
//> desc: Server part of the Nexus demo
//> use: core nexus

#include <nexus/nexus.h>

int run(int argc, char* argv[])
{
    UNUSED(argc);
    UNUSED(argv);
    prn("Demo server running...");

    cstr url          = "tcp://127.0.0.1:8080";

    Net_Socket sock   = net_socket();
    Net_Result result = net_bind(&sock, url);
    if (NET_FAILED(result)) {
        fatal_error("Failed to create binding: %s", net_result_string(result));
    }

    prn("Bound to %s", url);
    prn("Waiting for 10 incoming messages...");

    Net_Message msg = net_message_create(&sock);
    for (int i = 0; i < 10; ++i) {
        result = net_recv(&msg);
        if (NET_FAILED(result)) {
            fatal_error("Failed to receive data: %s",
                        net_result_string(result));
        }

        string text_msg;
        if (!net_message_read_string(&msg, &text_msg)) {
            fatal_error("Failed to read message");
        }

        prn("Received message %d/10: " STRINGP, i + 1, STRINGV(text_msg));
    }
    net_message_done(&msg);

    net_close(&sock);

    return 0;
}
