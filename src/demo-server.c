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

    cstr url          = "tcp://127.0.0.1:8080";

    Net_Socket sock   = net_socket();
    Net_Result result = net_bind(&sock, url);
    if (NET_FAILED(result)) {
        kill("Failed to create binding: %s", net_result_string(result));
    }

    prn("Bound to %s", url);

    u8    buffer[1024];
    usize recv_len;

    prn("Waiting for incoming messages...");
    Net_Message msg = net_message_create(&sock);

    result          = net_recv_msg(&msg);
    if (NET_FAILED(result)) {
        kill("Failed to receive data: %s", net_result_string(result));
    }

    string text_msg;
    if (!net_message_read_string(&msg, &text_msg)) {
        kill("Failed to read message");
    }

    prn("Received message: " STRINGP, STRINGV(text_msg));
    net_message_done(&msg);

    net_close(&sock);

    return 0;
}
