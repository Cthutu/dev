//
// demo-client.c
//
// Client demo that sends a message to the server demo
//
//> use: core nexus

#include <nexus/nexus.h>

#include <stdio.h>

int run(int argc, char* argv[])
{
    UNUSED(argc);
    UNUSED(argv);
    prn("Demo client running...");

    cstr url = "tcp://127.0.0.1:8080";

    for (int i = 0; i < 10; ++i) {
        char message_buffer[64];
        snprintf(message_buffer,
                 sizeof(message_buffer),
                 "Hello from client connection %d",
                 i + 1);
        string message =
            string_from((u8*)message_buffer, strlen(message_buffer));

        Net_Socket sock = net_socket();
        Net_Result result =
            net_set_option(&sock, NET_OPT_CONNECT_TIMEOUT_MS, 2000);
        if (NET_FAILED(result)) {
            kill("Failed to set connect timeout: %s",
                 net_result_string(result));
        }
        result = net_set_option(&sock, NET_OPT_RECONNECT_INTERVAL_MS, 25);
        if (NET_FAILED(result)) {
            kill("Failed to set reconnect interval: %s",
                 net_result_string(result));
        }
        result = net_connect(&sock, url);
        if (NET_FAILED(result)) {
            kill("Failed to connect to server: %s", net_result_string(result));
        }

        prn("Connected to %s (%d/10)", url, i + 1);
        prn("Sending message: " STRINGP, STRINGV(message));

        Net_Message msg = net_message_create(&sock);
        net_message_append_string(&msg, message);

        result = net_send(&msg);
        net_message_done(&msg);
        net_close(&sock);
        if (NET_FAILED(result)) {
            kill("Failed to send data: %s", net_result_string(result));
        }
    }

    return 0;
}
