# How to get screen size on a telnet connection

This guide shows how to query the current telnet console size reported by a
client.

## Goal

Receive telnet input and, when available, read the client's current width and
height.

## When does this work?

`net_telnet_bounds()` works when:

- you are using `net_telnet_socket()`
- the telnet client supports NAWS
- the client has already sent its current size

It returns `false` when:

- the message did not come from a telnet socket
- the telnet client has not supplied NAWS size information yet

## Intended usage

Call `net_telnet_bounds()` after `net_recv(&message)`, using the same received
message object.

It is message metadata, not a replacement for `net_recv()`.

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

        u16 width  = 0;
        u16 height = 0;
        if (net_telnet_bounds(&msg, &width, &height)) {
            prn("Client size: %ux%u", width, height);
        }

        string line = string_from(msg.data, msg.length);
        prn("Received: " STRINGP, STRINGV(line));
    }

    net_message_done(&msg);
    net_close(&sock);
    return 0;
}
```

## Important behaviour

- Nexus caches the latest known bounds for that telnet client.
- The client does not need to resend its bounds with every line.
- Once NAWS has been received, later messages from the same telnet client may
  still return the same width and height until the client reports a new size.

## Notes

- This is a cheap query on cached telnet state.
- Change detection is left to higher-level code.
- Telnet bounds are currently only available through received telnet messages.

## Related files

- [README.md](/home/matt/dev/src/nexus/README.md)
- [005-how-to-build-a-line-based-telnet-server.md](/home/matt/dev/docs/how-to/nexus/005-how-to-build-a-line-based-telnet-server.md)
- [telnet.c](/home/matt/dev/tests/nexus/telnet.c)
