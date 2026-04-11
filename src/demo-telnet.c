//
// demo-telnet.c
//
// Line-oriented telnet demo
//
//> use: core nexus

#include <nexus/nexus.h>

int run(int argc, char* argv[])
{
    cstr url = "tcp://127.0.0.1:2323";
    if (argc > 1) {
        url = argv[1];
    }

    prn("Telnet demo running...");

    Net_Socket sock   = net_telnet_socket();
    Net_Result result = net_bind(&sock, url);
    if (NET_FAILED(result)) {
        fatal_error("Failed to bind telnet socket: %s",
                    net_result_string(result));
    }

    prn("Listening for one telnet client on %s", url);
    prn("Type lines to echo them back. Type q to quit.");

    Net_Message msg = net_message_create(&sock);
    while (true) {
        result = net_recv(&msg);
        if (NET_FAILED(result)) {
            fatal_error("Failed to receive telnet line: %s",
                        net_result_string(result));
        }

        string line = string_from(msg.data, msg.length);
        prn("Received: " STRINGP, STRINGV(line));

        net_message_clear(&msg);
        if (string_equals_cstr(line, "q")) {
            net_message_append(&msg, "Goodbye.", 8);
            result = net_send(&msg);
            if (NET_FAILED(result)) {
                fatal_error("Failed to send goodbye line: %s",
                            net_result_string(result));
            }
            break;
        }

        net_message_append(&msg, line.data, line.count);
        result = net_send(&msg);
        if (NET_FAILED(result)) {
            fatal_error("Failed to send telnet echo: %s",
                        net_result_string(result));
        }
    }

    net_message_done(&msg);
    net_close(&sock);
    prn("Telnet demo finished.");
    return 0;
}
