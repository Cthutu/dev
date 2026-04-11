# How to build a line-based telnet server

This guide shows how to use `net_telnet_socket()` in its current line-oriented
mode.

## Goal

Accept a telnet client, receive one line at a time, and echo the line back.

## When should you use a telnet socket?

Use `net_telnet_socket()` when:

- you want to talk to a telnet client over TCP
- line-based text input is enough
- you want Nexus to strip telnet negotiation bytes from the received input

Do not use it when:

- you need binary message framing
- you want UDP
- you need character-at-a-time input already

Line mode is the default telnet mode. You do not need to set a socket option
for it unless you want to switch back from character mode explicitly.

## Steps

1. Create a socket with `net_telnet_socket()`.
2. Bind it to a TCP URL with `net_bind()`.
3. Create a `Net_Message` for that socket.
4. Call `net_recv(&message)` to wait for one line.
5. Treat `message.data`/`message.length` as the line text, without `CRLF`.
6. Clear the message, append the reply text, and call `net_send(&message)`.

## Example

```c
#include <nexus/nexus.h>

int run(int argc, char* argv[])
{
    UNUSED(argc);
    UNUSED(argv);

    Net_Socket sock = net_telnet_socket();

    Net_Result result = net_bind(&sock, "tcp://127.0.0.1:2323");
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

        string line = string_from(msg.data, msg.length);
        prn("Received: " STRINGP, STRINGV(line));

        net_message_clear(&msg);
        net_message_append(&msg, line.data, line.count);

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

## What does Nexus do for you?

- received telnet lines arrive without the trailing `CRLF`
- telnet negotiation bytes are handled internally and do not appear in the
  message payload
- sent telnet lines are written back with `CRLF`
- if the client supplies NAWS information, you can query the current console
  width and height with `net_telnet_bounds(&message, &width, &height)`

## Notes

- Telnet sockets are currently TCP-only.
- The current telnet mode is line-based, not character-based.
- A received telnet line still uses the normal `Net_Message` API, so you can
  inspect sender metadata with `net_message_id()` and `net_message_url()`.
- `net_telnet_bounds()` returns false until the client has negotiated and sent
  its NAWS size information.

## Related files

- [README.md](/home/matt/dev/src/nexus/README.md)
- [demo-telnet.c](/home/matt/dev/src/demo-telnet.c)
- [telnet.c](/home/matt/dev/tests/nexus/telnet.c)
