# How to use request/reply sockets

This guide shows how to use the request/reply socket kinds added on top of the
basic message transport.

## Goal

Use `net_request_socket()` and `net_reply_socket()` to enforce a strict
request/reply exchange.

## When should you use request/reply?

Choose request/reply sockets when:

- one side is clearly issuing requests
- the other side is clearly sending one reply per request
- you want Nexus to enforce the turn-taking rules for you

Choose a basic socket instead when:

- either side may send unsolicited messages
- you need a more conversational or symmetric protocol
- you want to define your own ordering rules in the application

The difference is behavioural, not transport-level. Both socket kinds still use
the same underlying Nexus message transport:

- TCP with internal framing
- UDP with datagram semantics

## Request/reply rules

Request sockets must do this:

1. send a request
2. receive a reply
3. repeat

Reply sockets must do this:

1. receive a request
2. send a reply
3. repeat

If you break that order, Nexus returns `NET_WRONG_STATE`.

That is the main reason to choose request/reply over a basic socket: the socket
kind itself protects the intended protocol flow.

## Client example

```c
Net_Socket client = net_request_socket();

Net_Result result = net_connect(&client, "tcp://127.0.0.1:8081");
if (NET_FAILED(result)) {
    prn("Connect failed: %s", net_result_string(result));
    return 1;
}

Net_Message msg = net_message_create(&client);
net_message_append_string(&msg, S("ping"));
net_message_append_u32(&msg, 1);

result = net_send(&msg);
if (NET_FAILED(result)) {
    prn("Request send failed: %s", net_result_string(result));
    return 1;
}

result = net_recv(&msg);
if (NET_FAILED(result)) {
    prn("Reply receive failed: %s", net_result_string(result));
    return 1;
}
```

## Server example

```c
Net_Socket server = net_reply_socket();

Net_Result result = net_bind(&server, "tcp://127.0.0.1:8081");
if (NET_FAILED(result)) {
    prn("Bind failed: %s", net_result_string(result));
    return 1;
}

Net_Message msg = net_message_create(&server);
result = net_recv(&msg);
if (NET_FAILED(result)) {
    prn("Request receive failed: %s", net_result_string(result));
    return 1;
}

string request_text;
u32    request_id = 0;

if (!net_message_read_string(&msg, &request_text) ||
    !net_message_read_u32(&msg, &request_id)) {
    prn("Failed to decode request");
    return 1;
}

net_message_clear(&msg);
net_message_append_string(&msg, S("pong"));
net_message_append_u32(&msg, request_id + 1);

result = net_send(&msg);
if (NET_FAILED(result)) {
    prn("Reply send failed: %s", net_result_string(result));
    return 1;
}
```

## Why reuse the same message?

When a server receives into a `Net_Message`, Nexus stores hidden routing
information inside that message. Reusing the same message for the reply lets
`net_send(&message)` route the reply back to the correct originating peer.

If you want to confirm which request you are replying to, you can also inspect:

- `net_message_id(&message, &id)`
- `net_message_url(&message, &url)`

These values remain available after `net_message_clear(&message)`.

## Demo

For a complete in-process example, see:

- [demo-repreq.c](/home/matt/dev/src/demo-repreq.c)

## Related files

- [README.md](/home/matt/dev/src/nexus/README.md)
- [reqrep.c](/home/matt/dev/tests/nexus/reqrep.c)
