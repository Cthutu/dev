# Nexus

Nexus is a small message-oriented networking layer.

It is inspired by nanomsg in spirit: the public API is centred around sockets
and messages rather than raw BSD stream operations. The current implementation
supports:

- TCP message transport with internal length-prefixed framing
- UDP datagram transport
- reusable message objects
- multi-client TCP servers behind a single bound socket
- per-socket options for connect retry, timeouts, and non-blocking behaviour

This document is an overview of the module as it exists today. More focused
guides can be added later under `docs/how-to/nexus`.

## Core ideas

### Messages, not byte streams

The public API deals in complete messages:

- `net_send(&message)` sends one message
- `net_recv(&message)` receives one message

For TCP, Nexus adds its own framing so one send corresponds to one receive.
For UDP, one datagram is one message.

### Reusable message objects

`Net_Message` owns a growable buffer and can be reused across many send and
receive operations.

- `net_message_clear` clears the payload but keeps allocated capacity
- append helpers write values into the message body
- read helpers consume values from the front of the message body

When a message is received from a bound socket, Nexus also keeps hidden routing
information inside the message so `net_send(&message)` can reply back to the
originating peer.

### Simple socket lifecycle

Typical usage is:

1. create a socket with `net_socket()`
2. `net_connect()` for a client or `net_bind()` for a server
3. create a `Net_Message` with `net_message_create(&socket)`
4. send and receive messages
5. clean up with `net_message_done()` and `net_close()`

## URLs

Nexus currently accepts URLs of the form:

- `tcp://127.0.0.1:8080`
- `udp://127.0.0.1:9999`

Current parsing is intentionally simple:

- IPv4 literals only
- no host name lookup yet
- no IPv6 URL support yet

## Basic client example

```c
#include <nexus/nexus.h>

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
}

net_message_done(&msg);
net_close(&sock);
```

## Basic server example

```c
#include <nexus/nexus.h>

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
    return 1;
}

string text;
u32    number;

if (!net_message_read_string(&msg, &text) ||
    !net_message_read_u32(&msg, &number)) {
    prn("Bad message payload");
    return 1;
}

prn("Received " STRINGP " and %u", STRINGV(text), number);

net_message_done(&msg);
net_close(&sock);
```

## Message API

### Create and reuse

- `net_message_create(&socket)`
- `net_message_clear(&message)`
- `net_message_done(&message)`

### Append helpers

- `net_message_append`
- `net_message_append_string`
- `net_message_append_u8`
- `net_message_append_u16`
- `net_message_append_u32`
- `net_message_append_u64`

Strings are encoded as:

- `u32` length in network byte order
- followed by raw string bytes

### Read helpers

- `net_message_read`
- `net_message_read_string`
- `net_message_read_u8`
- `net_message_read_u16`
- `net_message_read_u32`
- `net_message_read_u64`

Reads are destructive: they consume bytes from the front of the message.

## Socket options

Use `net_set_option()` and `net_get_option()` to configure per-socket
behaviour.

### Connect timing

- `NET_OPT_CONNECT_TIMEOUT_MS`
- `NET_OPT_RECONNECT_INTERVAL_MS`

These options let a client start before a server has bound. Nexus retries
`connect()` internally until either:

- the connection succeeds
- the timeout expires

If the connect timeout is `NET_WAIT_INFINITE`, Nexus keeps retrying until the
connection succeeds or another non-retryable error occurs.

### Send and receive timing

- `NET_OPT_SEND_TIMEOUT_MS`
- `NET_OPT_RECV_TIMEOUT_MS`

By default, send and receive operations wait indefinitely.

### Non-blocking mode

- `NET_OPT_NONBLOCKING`

When enabled, operations that cannot make progress immediately return
`NET_WOULD_BLOCK` instead of waiting.

In practice this affects:

- `net_connect`
- `net_recv`
- `net_send` when the underlying transport would block

## Result codes

| Result | Description |
| --- | --- |
| `NET_OK` | The operation completed successfully. |
| `NET_INVALID_URL` | The URL could not be parsed into a valid Nexus endpoint. |
| `NET_NO_NETWORK` | The local network stack is unavailable. |
| `NET_OUT_OF_FD` | The process or system ran out of file descriptors. |
| `NET_PROTOCOL_NOT_SUPPORTED` | The requested protocol is not supported by Nexus or the platform. |
| `NET_PORT_IN_USE` | The requested bind port is already in use. |
| `NET_ACCESS_DENIED` | The operation was denied by the platform, for example due to bind permissions. |
| `NET_SOCKET_BUSY` | The socket is already in use and cannot be rebound or reconnected. |
| `NET_NOT_CONNECTED` | The socket or message is not ready for the requested operation. |
| `NET_BUFFER_TOO_SMALL` | An internal receive buffer was too small for the pending message. This is mainly an internal transport result now. |
| `NET_BAD_MESSAGE` | The remote side sent a message Nexus considers invalid. |
| `NET_TIMEOUT` | The operation did not complete before its configured timeout expired. |
| `NET_WOULD_BLOCK` | A non-blocking operation could not make progress immediately. |
| `NET_CLOSED` | The peer closed the connection. |
| `NET_ERROR` | A general network error occurred. |

Use `net_result_string(result)` for readable diagnostics.

## Transport behaviour

### TCP

- one message per send/receive
- uses a 4-byte length prefix internally
- bound sockets can receive from multiple connected clients
- replies can be sent using the same received `Net_Message`

### UDP

- one datagram per send/receive
- received messages retain sender routing information
- replies can be sent using the same received `Net_Message`

## Current limitations

This module is still intentionally small. Some things are not there yet:

- no host name lookup
- no IPv6 URL handling
- no public request/reply socket constructors yet
- no telnet socket kind yet
- no dedicated how-to guides yet

## Related files

- [nexus.h](/home/matt/dev/src/nexus/nexus.h)
- [PLAN.md](/home/matt/dev/src/nexus/PLAN.md)
- [demo-client.c](/home/matt/dev/src/demo-client.c)
- [demo-server.c](/home/matt/dev/src/demo-server.c)
