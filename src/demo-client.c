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

    cstr   url        = "tcp://127.0.0.1:8080";
    string message    = S("Hello from the client!");

    Net_Socket sock   = net_socket();
    Net_Result result = net_connect(&sock, url);
    if (NET_FAILED(result)) {
        kill("Failed to connect to server: %s", net_result_string(result));
    }

    prn("Connected to %s", url);
    prn("Sending message: " STRINGP, STRINGV(message));

    Net_Message msg = net_message_create(&sock);
    net_message_append_string(&msg, message);

    result = net_send_msg(&msg);
    net_message_done(&msg);
    if (NET_FAILED(result)) {
        kill("Failed to send data: %s", net_result_string(result));
    }

    net_close(&sock);

    return 0;
}
