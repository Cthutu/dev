# How to create a basic client

This guide shows the smallest useful Nexus client.

## Goal

Connect to a server, send one message, and close cleanly.

## Steps

1. Create a socket with `net_socket()`.
2. Connect with `net_connect()`.
3. Create a `Net_Message` bound to that socket.
4. Append the payload you want to send.
5. Call `net_send(&message)`.
6. Clean up with `net_message_done()` and `net_close()`.

## Example

```c
#include <nexus/nexus.h>

int run(int argc, char* argv[])
{
    UNUSED(argc);
    UNUSED(argv);

    Net_Socket sock = net_socket();

    Net_Result result = net_connect(&sock, "tcp://127.0.0.1:8080");
    if (NET_FAILED(result)) {
        prn("Connect failed: %s", net_result_string(result));
        return 1;
    }

    Net_Message msg = net_message_create(&sock);
    net_message_append_string(&msg, S("hello"));
    net_message_append_u32(&msg, 42);

    result = net_send(&msg);
    if (NET_FAILED(result)) {
        prn("Send failed: %s", net_result_string(result));
        net_message_done(&msg);
        net_close(&sock);
        return 1;
    }

    net_message_done(&msg);
    net_close(&sock);
    return 0;
}
```

## Notes

- For TCP, Nexus adds message framing internally.
- For UDP, one send is one datagram.
- `Net_Message` is reusable, so you can clear it and send again instead of
  creating a new one each time.

## Related files

- [README.md](/home/matt/dev/src/nexus/README.md)
- [demo-client.c](/home/matt/dev/src/demo-client.c)
