# How to build a character-based telnet server

This guide shows how to switch a telnet socket into character mode.

## Goal

Receive telnet input one character at a time instead of waiting for a full
line.

## When should you use character mode?

Use character mode when:

- your application needs immediate key input
- line buffering is too coarse
- you are building an interactive telnet application such as a MUD

Use line mode instead when:

- newline-delimited commands are enough
- you want the simplest telnet server behaviour

## Steps

1. Create a socket with `net_telnet_socket()`.
2. Set `NET_OPT_TELNET_MODE` to `NET_TELNET_CHARACTER_MODE`.
3. Bind or connect the socket as usual.
4. Call `net_recv(&message)` to receive one character-sized message at a time.
5. Call `net_send(&message)` to send raw bytes back without `CRLF`.

## Example

```c
#include <nexus/nexus.h>

int run(int argc, char* argv[])
{
    UNUSED(argc);
    UNUSED(argv);

    Net_Socket sock = net_telnet_socket();

    Net_Result result =
        net_set_option(&sock, NET_OPT_TELNET_MODE, NET_TELNET_CHARACTER_MODE);
    if (NET_FAILED(result)) {
        prn("Failed to set telnet mode: %s", net_result_string(result));
        return 1;
    }

    result = net_bind(&sock, "tcp://127.0.0.1:2324");
    if (NET_FAILED(result)) {
        prn("Bind failed: %s", net_result_string(result));
        return 1;
    }

    Net_Message msg = net_message_create(&sock);

    while (true) {
        result = net_recv(&msg);
        if (NET_FAILED(result)) {
            prn("Receive failed: %s", net_result_string(result));
            break;
        }

        if (msg.length == 0) {
            continue;
        }

        char ch = (char)msg.data[0];
        prn("Received character: %c", ch);

        net_message_clear(&msg);
        net_message_append(&msg, &ch, 1);

        result = net_send(&msg);
        if (NET_FAILED(result)) {
            prn("Send failed: %s", net_result_string(result));
            break;
        }
    }

    net_message_done(&msg);
    net_close(&sock);
    return 0;
}
```

## Important behaviour

- character mode affects both receive and send
- receive returns one character-sized message at a time
- send writes raw bytes and does not append `CRLF`
- carriage return is normalised to a single newline character

## Notes

- `NET_OPT_TELNET_MODE` only applies to telnet sockets.
- Line mode remains the default.
- This mode is intended for more interactive telnet applications.

## Related files

- [README.md](/home/matt/dev/src/nexus/README.md)
- [demo-telnet2.c](/home/matt/dev/src/demo-telnet2.c)
- [telnet.c](/home/matt/dev/tests/nexus/telnet.c)
