//
// demo-telnet2.c
//
// Character-mode telnet demo
//
//> use: core nexus

#include <nexus/nexus.h>

int run(int argc, char* argv[])
{
    cstr url = "tcp://127.0.0.1:2324";
    if (argc > 1) {
        url = argv[1];
    }

    prn("Character-mode telnet demo running...");

    Net_Socket sock = net_telnet_socket();
    Net_Result result =
        net_set_option(&sock, NET_OPT_TELNET_MODE, NET_TELNET_CHARACTER_MODE);
    if (NET_FAILED(result)) {
        fatal_error("Failed to enable telnet character mode: %s",
                    net_result_string(result));
    }

    result = net_bind(&sock, url);
    if (NET_FAILED(result)) {
        fatal_error("Failed to bind telnet socket: %s",
                    net_result_string(result));
    }

    prn("Listening for one telnet client on %s", url);
    prn("Characters echo immediately. Press q to quit.");

    Net_Message msg = net_message_create(&sock);
    while (true) {
        result = net_recv(&msg);
        if (NET_FAILED(result)) {
            fatal_error("Failed to receive telnet character: %s",
                        net_result_string(result));
        }

        if (msg.length == 0) {
            continue;
        }

        char ch = (char)msg.data[0];
        if (ch == 'q') {
            net_message_clear(&msg);
            net_message_append(&msg, "\nGoodbye.\n", 10);
            result = net_send(&msg);
            if (NET_FAILED(result)) {
                fatal_error("Failed to send goodbye text: %s",
                            net_result_string(result));
            }
            break;
        }

        net_message_clear(&msg);
        net_message_append(&msg, &ch, 1);
        result = net_send(&msg);
        if (NET_FAILED(result)) {
            fatal_error("Failed to echo telnet character: %s",
                        net_result_string(result));
        }
    }

    net_message_done(&msg);
    net_close(&sock);
    prn("Character-mode telnet demo finished.");
    return 0;
}
