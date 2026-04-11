# How to configure timeouts and non-blocking

This guide shows how to configure socket timing and blocking behaviour with
`net_set_option()`.

## Available timing options

- `NET_OPT_CONNECT_TIMEOUT_MS`
- `NET_OPT_RECONNECT_INTERVAL_MS`
- `NET_OPT_SEND_TIMEOUT_MS`
- `NET_OPT_RECV_TIMEOUT_MS`
- `NET_OPT_NONBLOCKING`

Useful constants:

- `NET_WAIT_IMMEDIATE`
- `NET_WAIT_INFINITE`

## Connect retry example

This is useful when the client may start before the server has bound.

```c
Net_Socket sock = net_socket();

net_set_option(&sock, NET_OPT_CONNECT_TIMEOUT_MS, 2000);
net_set_option(&sock, NET_OPT_RECONNECT_INTERVAL_MS, 25);

Net_Result result = net_connect(&sock, "tcp://127.0.0.1:8080");
if (NET_FAILED(result)) {
    prn("Connect failed: %s", net_result_string(result));
}
```

Behaviour:

- Nexus retries `connect()` until it succeeds or the timeout expires.
- If the timeout is `NET_WAIT_INFINITE`, Nexus keeps retrying until success or
  a non-retryable error occurs.

## Receive timeout example

```c
Net_Socket sock = net_socket();
net_bind(&sock, "tcp://127.0.0.1:8080");

net_set_option(&sock, NET_OPT_RECV_TIMEOUT_MS, 500);

Net_Message msg = net_message_create(&sock);
Net_Result result = net_recv(&msg);
if (result == NET_TIMEOUT) {
    prn("No message arrived within 500 ms");
}
```

## Non-blocking example

```c
Net_Socket sock = net_socket();
net_bind(&sock, "udp://127.0.0.1:9999");

net_set_option(&sock, NET_OPT_NONBLOCKING, 1);

Net_Message msg = net_message_create(&sock);
Net_Result result = net_recv(&msg);
if (result == NET_WOULD_BLOCK) {
    prn("No datagram is ready yet");
}
```

## Return codes to expect

- `NET_TIMEOUT`
  The operation waited up to its configured timeout and still could not finish.
- `NET_WOULD_BLOCK`
  The operation was non-blocking and could not make progress immediately.

## Notes

- By default, send and receive operations wait indefinitely.
- Non-blocking mode is per-socket.
- Request/reply ordering is separate from blocking behaviour. A request socket
  can still return `NET_WRONG_STATE` if you call `net_recv()` before `net_send()`.

## Related files

- [README.md](/home/matt/dev/src/nexus/README.md)
- [message.c](/home/matt/dev/tests/nexus/message.c)
