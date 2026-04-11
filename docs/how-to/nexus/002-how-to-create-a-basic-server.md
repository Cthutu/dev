# How to create a basic server

This guide shows the simplest Nexus server that binds, receives one message,
and decodes it.

## Goal

Bind to a URL, receive one message, and read typed values from it.

## Steps

1. Create a socket with `net_socket()`.
2. Bind with `net_bind()`.
3. Create a `Net_Message` for the socket.
4. Call `net_recv(&message)`.
5. Read values out of the message with the `net_message_read_*` helpers.
6. Clean up with `net_message_done()` and `net_close()`.

## Example

```c
#include <nexus/nexus.h>

int run(int argc, char* argv[])
{
    UNUSED(argc);
    UNUSED(argv);

    Net_Socket sock = net_socket();

    Net_Result result = net_bind(&sock, "tcp://127.0.0.1:8080");
    if (NET_FAILED(result)) {
        prn("Bind failed: %s", net_result_string(result));
        return 1;
    }

    Net_Message msg = net_message_create(&sock);
    result = net_recv(&msg);
    if (NET_FAILED(result)) {
        prn("Receive failed: %s", net_result_string(result));
        net_message_done(&msg);
        net_close(&sock);
        return 1;
    }

    string text;
    u32    number = 0;

    if (!net_message_read_string(&msg, &text) ||
        !net_message_read_u32(&msg, &number)) {
    prn("Failed to decode message");
        net_message_done(&msg);
        net_close(&sock);
        return 1;
    }

    prn("Received " STRINGP " and %u", STRINGV(text), number);

    net_message_done(&msg);
    net_close(&sock);
    return 0;
}
```

## Notes

- A bound TCP socket can receive from multiple clients.
- A bound UDP socket can receive from multiple peers.
- When a received message is reused for `net_send(&message)`, Nexus can reply
  back to the originating peer using the message's hidden pipe metadata.
- You can inspect the current sender with:
  - `net_message_id(&msg, &id)`
  - `net_message_url(&msg, &url)`

## Related files

- [README.md](/home/matt/dev/src/nexus/README.md)
- [demo-server.c](/home/matt/dev/src/demo-server.c)
