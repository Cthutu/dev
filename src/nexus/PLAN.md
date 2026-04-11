# Nexus Plan

## Goal

Evolve `nexus` from a thin socket wrapper into a small nanomsg-inspired
messaging layer while keeping the public API simple:

- `net_bind`
- `net_connect`
- `net_send`
- `net_recv`
- future specialized constructors such as:
  - `net_request_socket`
  - `net_reply_socket`
  - `net_telnet_socket`

The immediate objective is to make TCP behave like a message transport via
explicit framing, while preserving a small API surface and not overcommitting
to a large runtime design too early.

## Current Status

The following work is now in place:

- TCP message framing
- UDP datagram support
- message-only public send/receive API
- reusable `Net_Message` objects
- hidden pipe-based reply routing
- sender inspection via `Net_Message` metadata
- multi-client TCP server support
- request/reply socket kinds
- telnet socket kind with line-oriented TCP behaviour
- socket options for:
  - connect retry
  - connect timeout
  - send timeout
  - receive timeout
  - non-blocking mode
- tests for Nexus behaviour in the shared test framework
- overview and how-to documentation
- telnet demo covering line echo and `q` shutdown

The active remaining product work is now:

- richer telnet behaviour
- documentation beyond the overview README, especially how-to guides under
  `docs/how-to/nexus`

## Next Milestone: Richer Telnet Support

The first telnet milestone is now in place:

- `net_telnet_socket`
- `demo-telnet.c`
- line-oriented telnet messages over TCP
- simple telnet option rejection so line-mode clients can connect cleanly

The next telnet work should build on that:

- character-oriented mode for interactive applications and MUD-style input
- telnet state query helpers such as `net_telnet_size()`
- terminal width and height tracking from telnet option negotiation
- a dedicated telnet how-to document

## Current API Direction

Nexus now has three public socket directions:

- `net_socket`
  - flexible message transport without enforced ordering
- `net_request_socket`
  - send, then receive, repeating
- `net_reply_socket`
  - receive, then send, repeating

The next public constructor should be:

- `net_telnet_socket`

## Notes On This Plan

The rest of this document records the design path that led to the current
implementation. Some older sections still describe work that is already done.
Those sections are kept as design history and rationale, but the sections
above are the authoritative view of the current roadmap.

Older sections below record the design path that got the module here. Where
they disagree with the current implementation, the implementation wins.

## Design Direction

### 0. Code organization rules

Keep the module split clean as implementation complexity grows.

- Put shared private declarations in `internal.h`.
- Keep public API types and declarations in `nexus.h`.
- Move distinct implementation concerns into separate `.c` files inside
  `src/nexus`.
- Split transport implementations from pattern implementations.
- Progress on the module must include tests in the existing test framework.

This means `internal.h` should contain things such as:

- private enums
- internal structs
- shared helper declarations
- internal ops tables

and should not force those details into the public API unless users actually
need them.

New behaviour and bug fixes should be covered by tests under `tests/`, with
`tests/nexus/` used for Nexus-specific coverage where appropriate.

### 1. Separate transport from pattern

Internally, sockets should distinguish between:

- transport:
  - TCP
  - UDP
- pattern:
  - raw message
  - request/reply
  - telnet
  - future patterns

This should be an internal design boundary first, not necessarily a public API
explosion. Public constructors can stay minimal while the implementation grows.

### 2. Keep the public API message-oriented

The API should present message semantics, not BSD stream semantics.

- `net_send` means "send one message"
- `net_recv` means "receive one message"

That implies:

- UDP can map naturally to a single datagram
- TCP must add framing internally so one send corresponds to one recv

### 3. Avoid exposing transport setup details

The current instinct to hide `accept()` is correct for this direction.

- `net_bind` creates a server endpoint
- `net_recv` may internally accept a TCP connection when needed
- callers should not need to care about `listen/accept`

This does mean the first TCP server shape remains intentionally simple:

- one bound socket
- one accepted peer at a time
- no multi-client server API yet

That is acceptable for the first phase.

## Phase 1: TCP Message Framing

### Scope

Implement explicit framing for TCP so the current API gains real message
semantics.

### Framing format

Use a fixed-size length prefix before each TCP message.

Proposed first version:

- 4-byte unsigned message length
- network byte order
- payload follows immediately after the header

Reasons:

- simple to debug
- easy to implement on both send and recv
- stable enough to build higher-level patterns on top of

The maximum allowed message size should be exposed as a `#define` in
`nexus.h`. For the first implementation, use a conservative default such as
1 MiB.

### Send behaviour

For TCP:

- `net_send` writes:
  - 4-byte length prefix
  - payload bytes
- partial writes are handled internally until the full framed message is sent

For UDP:

- `net_send` remains one datagram send

### Receive behaviour

For TCP:

- `net_recv` first reads exactly 4 bytes for the frame length
- then reads exactly that many payload bytes
- if the caller buffer is too small, return a specific result and leave the
  full message pending
- on `NET_BUFFER_TOO_SMALL`, `out_recv_len` reports the required payload size
- if the caller passes `NULL` for the buffer, the pending message is explicitly
  dropped

This behaviour should be documented clearly in `nexus.h`.

For UDP:

- `net_recv` remains one datagram receive

### New result codes likely needed

Keep the result space small and shaped by API behaviour rather than raw syscall
errors.

Likely useful:

- `NET_BUFFER_TOO_SMALL`
- `NET_BAD_MESSAGE`
- keep:
  - `NET_NOT_CONNECTED`
  - `NET_CLOSED`
  - `NET_NO_NETWORK`
  - `NET_INVALID_URL`
  - `NET_SOCKET_BUSY`
  - `NET_ERROR`

Likely avoid:

- mirroring every errno value
- transport-specific results that the caller cannot act on

## Phase 2: Internal Socket Runtime Shape

### Internal state

`Net_State` is allowed to change or disappear if it stops helping.

Current state is useful for basic lifecycle tracking, but it may become too
coarse once sockets have transport and pattern behaviour.

Possible direction:

- keep a small lifecycle state:
  - disconnected
  - waiting connection
  - connected
  - closed
- move behaviour decisions out of `Net_State` and into transport/pattern tables

### Internal structures

Introduce internal ops tables once framing lands.

Possible shape:

```c
typedef struct Net_TransportOps {
    Net_Result (*send)(Net_Socket* sock, const void* buffer, usize len);
    Net_Result (*recv)(Net_Socket* sock, void* buffer, usize len, usize* out_len);
    Net_Result (*bind)(Net_Socket* sock, Net_Endpoint* endpoint);
    Net_Result (*connect)(Net_Socket* sock, Net_Endpoint* endpoint);
    void (*close)(Net_Socket* sock);
} Net_TransportOps;

typedef struct Net_PatternOps {
    Net_Result (*send)(Net_Socket* sock, const void* buffer, usize len);
    Net_Result (*recv)(Net_Socket* sock, void* buffer, usize len, usize* out_len);
} Net_PatternOps;
```

This does not need to be fully implemented in one pass. The first useful step
is to structure TCP and UDP code so the split is obvious.

### Source file layout

The implementation should move away from a single `socket.c` as concepts
expand.

Target direction:

- `socket.c`
  - public API entry points
  - thin dispatch and high-level lifecycle glue
- `transport_tcp.c`
  - TCP creation, connect/bind behaviour, framing send/recv helpers
- `transport_udp.c`
  - UDP creation, send/recv behaviour, peer-address handling
- `pattern_message.c`
  - default message socket behaviour
- `pattern_reqrep.c`
  - future request/reply rules
- `pattern_telnet.c`
  - future telnet-oriented behaviour
- `url.c`
  - URL parsing
- `internal.h`
  - private shared declarations used across these files

Exact filenames can change, but the split between:

- public API glue
- transport logic
- pattern logic

should be maintained.

### Socket data

`Net_Socket` will likely need more internal fields, for example:

- transport kind
- pattern kind
- framing scratch state
- max message size
- future peer/pipe metadata

The public struct can remain exposed for now if needed, but long term it would
be cleaner to hide implementation details behind an opaque type.

## Phase 3: Request/Reply and Other Socket Kinds

After the message and pipe foundations are stable, add higher-level socket
constructors.

Initial candidates:

- `net_request_socket`
- `net_reply_socket`
- `net_telnet_socket`

Planned behaviour:

- request/reply constrains send/recv ordering
- telnet uses stream-oriented text behaviour instead of framed binary messages
- basic sockets use:
  - TCP with framing
  - UDP with datagrams

Request/reply is now the next active implementation step.

Telnet should be the next specialised socket kind after request/reply.
The first useful telnet milestone should be:

- line-based input from a telnet client
- line echo / response behaviour
- a small `demo-telnet.c` proving the socket kind end to end

Later telnet milestones should include:

- character-based mode
- telnet size negotiation support
- a query such as `net_telnet_size()` returning the current console width and
  height based on telnet option negotiation

The important architectural point is that these constructors choose pattern
behaviour first, and transport support second.

## Multi-Client Server Model

### Core issue

For a server handling multiple clients, one socket cannot cleanly represent both:

- the listening endpoint
- every active client conversation

The current one-socket server behaviour is acceptable for the first simple demo,
but it should not be treated as the long-term model for TCP servers.

### Internal concepts we should separate

Even if the public API stays simple, the implementation should distinguish:

- listener:
  - owns the bound TCP port
  - accepts new clients
- pipe:
  - represents one replyable communication path
  - for TCP this is one accepted client connection
  - for UDP this is one remembered peer address on a bound socket

This separation is useful for both transport types:

- TCP multi-client servers need many pipes under one listener
- UDP multi-peer servers need one bound socket plus many remembered peer pipes

Revised direction:

- use one internal concept, `Net_Pipe`, for any replyable communication path
- for TCP, a pipe represents one accepted client connection
- for UDP, a pipe represents one remembered peer address on a socket

This keeps reply routing unified across transports and avoids introducing a
separate route abstraction when the real concept is "a path where data can flow
back to the sender".

### Recommended internal model

Introduce hidden internal records instead of overloading `Net_Socket` with every
role at once.

Preferred shape:

```c
typedef enum : u8 {
    NET_PIPE_TCP,
    NET_PIPE_UDP,
} Net_Pipe_Kind;

typedef struct Net_Pipe {
    Net_Pipe_Kind kind;
    Net_Socket*   owner;
    u32           id;
    bool          closed;

    union {
        struct {
            int fd;
        } tcp;

        struct {
            sockaddr_in addr;
        } udp;
    };
} Net_Pipe;
```

The exact fields can change. The important part is:

- `Net_Socket` remains the public endpoint handle
- `Net_Pipe` becomes the internal per-flow reply path
- `Net_Message` can carry a hidden `Net_Pipe*`

### API shape options

There are three realistic directions.

#### Option 1: Explicit accepted clients

This is the clearest low-level shape.

Example:

```c
Net_Socket listener = net_socket();
net_bind(&listener, "tcp://127.0.0.1:8080");

Net_Socket client = net_accept(&listener);
net_recv(&client, buffer, sizeof(buffer), &recv_len);
net_send(&client, reply.data, reply.count);
```

Pros:

- maps directly to TCP reality
- easy to reason about
- good for low-level transport APIs

Cons:

- exposes `accept`
- less aligned with the nanomsg-style direction
- does not unify well with UDP

#### Option 2: Message envelopes

This is the most flexible long-term direction for a nanomsg-like API.

Example:

```c
Net_Message msg;
net_recv(&msg);
prn("Received %zu bytes", msg.len);
net_message_append(&msg, reply.data, reply.count);
net_send(&msg);
```

Internally, the message identifies where it came from via one pipe:

- TCP: a server-managed accepted pipe
- UDP: a pseudo-pipe that remembers the source address

That reply context does not need to be passed as a separate public parameter if
it is stored inside the message object itself.

Pros:

- unifies TCP and UDP server-side behaviour
- matches message-oriented semantics
- good fit for future req/rep sockets

Cons:

- requires extra API types
- more runtime machinery internally
- more design work up front

#### Option 3: Request/reply sockets

This is a specialization on top of message envelopes.

Example:

```c
Net_Socket sock = net_reply_socket();
net_bind(&sock, "tcp://127.0.0.1:8080");

Net_Request req;
net_recv_request(&sock, &req);
net_reply(&sock, &req, reply.data, reply.count);
```

Pros:

- strongest alignment with nanomsg-style patterns
- caller does not manage accepted clients directly
- natural request/reply flow

Cons:

- pattern-specific, not a general base transport API
- best built after message identity is already solved

### Recommendation

Use a staged approach.

#### Near term

Keep the current simple API for single-peer flows:

- `net_bind`
- `net_connect`
- `net_send`
- `net_recv`

This keeps demos and framing work simple.

#### Internal preparation

While implementing TCP multi-client support, add hidden pipe-aware structure so
the code does not assume "one bound TCP socket becomes one client forever" as a
permanent design.

That means:

- do not rely on `Net_Socket` alone as the future server model
- keep room for listener/pipe split internally
- keep per-message pipe identity as a first-class future concept

#### Medium term

Build multi-client transport support on top of the existing message-envelope
API:

- `net_recv`
- `net_message_append`
- `net_send`
- optional future pipe inspection helpers if needed

These primitives already support and should be extended to handle:

- multi-client TCP servers
- multi-peer UDP servers
- future request/reply patterns

#### Higher level

Build request/reply sockets on top of message envelopes rather than directly on
top of raw TCP accept/send/recv.

That gives one consistent mental model:

- receive a message
- optionally reset or append to its body
- send the same message object back out

### What to avoid

Avoid making the long-term API depend on this behaviour:

- a bound TCP socket accepts one client during `net_recv`
- the socket permanently stops being a listener
- other clients are impossible to represent

That is acceptable only as a temporary single-client simplification.

## Threads and Channels

The `thread` module and channels may be useful later, but they should not be a
hard requirement for Phase 1.

Use cases for later:

- background reader/writer threads
- decoupling network I/O from application threads
- request/reply correlation machinery
- multi-client dispatch

For now:

- implement synchronous send/recv first
- avoid introducing background concurrency until message semantics are stable

## Socket Options Direction

Rather than adding many specialised functions for small behaviour changes,
`nexus` should grow a socket-options model.

This is the preferred way to configure:

- connection timing behaviour
- I/O timing behaviour
- future blocking mode
- transport and pattern tuning

That keeps the public API smaller and lets the existing socket operations obey
configured behaviour instead of introducing one function per variation.

### Why use socket options

Options scale better than adding functions such as:

- `net_connect_wait`
- `net_connect_retry`
- `net_recv_timeout`
- `net_set_nonblocking`

Those behaviours are configuration of the same underlying socket rather than
fundamentally different operations.

The preferred direction is:

- keep `net_connect` as the main connect entry point
- keep `net_send` and `net_recv` as the main message I/O entry points
- have those operations respect configured socket options

### Initial option groups

The first useful option groups are:

- connection options
  - connect timeout
  - reconnect retry interval
  - retry-until-available behaviour
- I/O options
  - send timeout
  - receive timeout
  - future non-blocking mode
- transport and pattern options
  - maximum message size
  - future request/reply-specific tuning

### Connect-before-bind behaviour

If a client starts before a TCP server has bound and begun listening, the
kernel will reject a plain `connect()` call with `ECONNREFUSED`.

So the desired later behaviour:

- client starts first
- server binds later
- connection still succeeds

must be implemented as Nexus-managed retry behaviour on top of normal socket
connect semantics.

This should be configured through socket options rather than by introducing a
separate family of connect functions.

### Likely public shape

A likely future shape is:

```c
typedef enum {
    NET_OPT_CONNECT_TIMEOUT_MS,
    NET_OPT_RECONNECT_INTERVAL_MS,
    NET_OPT_SEND_TIMEOUT_MS,
    NET_OPT_RECV_TIMEOUT_MS,
    NET_OPT_NONBLOCKING,
} Net_Option;

Net_Result net_set_option(Net_Socket* sock, Net_Option option, u64 value);
Net_Result net_get_option(Net_Socket* sock, Net_Option option, u64* out_value);
```

Exact names can change, but the direction should be:

- per-socket configuration
- small, predictable option set
- behaviour driven by configuration, not API explosion

### Timeout conventions

Timeout-bearing options should support at least:

- immediate / disabled behaviour
- finite timeout in milliseconds
- infinite wait

Use an explicit sentinel for infinite wait rather than overloading `0` to mean
both immediate and infinite behaviour. That keeps the semantics clear.

### Scope for later work

The first socket-options work should likely focus on:

- connect timeout
- reconnect retry interval
- infinite retry / wait-forever configuration

Later phases can extend this to:

- send and receive timeouts
- non-blocking sockets
- pattern-specific options

## Memory Management Strategy

`core.h` already provides several memory-management techniques, including:

- arena allocation
- heap allocation
- dynamic arrays

`nexus` should use those intentionally based on lifetime and access patterns
rather than picking one mechanism for the whole module.

### Recommended rule

Do not make arena allocation foundational to the transport layer.

The transport layer needs stable, reusable state for:

- partial TCP frame reads
- partial TCP frame writes
- per-socket scratch buffers
- per-pipe framing progress

Those are usually better handled with explicit owned buffers and counters than
with arena-only allocation.

### Best fit by layer

#### Transport layer

Prefer fixed or reusable owned storage:

- heap-backed buffers when size is not known at compile time
- dynamic arrays when a resizable buffer is genuinely useful
- explicit per-socket/per-pipe scratch state in internal structs

This is the right place for:

- receive scratch buffers
- send staging buffers if needed
- framing header state
- pending message size and read/write offsets

#### Pattern layer

Arena allocation can be very useful for short-lived higher-level objects:

- decoded messages
- request envelopes
- temporary routing or dispatch metadata
- batch processing of multiple received messages

This is especially useful when the lifetime is scope-shaped:

- receive message(s)
- inspect/process them
- discard the whole working set together

#### Long-lived server state

Prefer heap allocation or dynamic arrays for structures such as:

- pipe tables
- peer maps
- listener-owned client lists
- transport/pattern runtime objects

These structures usually need independent growth and selective cleanup, which is
not a strong fit for arenas.

### Practical guidance for Phase 1

For TCP framing:

- keep per-socket state explicit
- do not require caller-provided arenas
- use heap or dynamic-array-backed scratch storage if fixed-size buffers are not
  sufficient

For future higher-level APIs:

- consider optional arena-assisted receive helpers for convenience
- keep the base `net_send` and `net_recv` API allocator-agnostic

## Message Object Direction

The preferred long-term server-side abstraction is now a reusable message
object carrying hidden pipe context.

### Public behaviour

A future `Net_Message` should provide:

- creation for a specific socket
- body storage
- current read point
- current write point
- capacity
- append support
- destructive read support
- reuse across receive and send
- hidden pipe association for replies

For now, only a body is required publicly. Head/body segmentation can be added
later if it proves useful.

### Message actions

The intended message API should support these operations:

- create for a specific socket
- append data to the body
- read data from the message body while removing it
- clear the message for reuse

Supported data forms should include:

- `u8`
- `u16`
- `u32`
- `u64`
- arbitrary buffer data

This gives a compact binary-message API without forcing callers to manually
manage offsets for common cases.

### Socket association

Messages should be created for a specific socket so the implementation can bind
them to the correct transport/pattern behaviour and attach hidden routing
context.

That supports:

- sending through the correct socket family
- retaining reply pipe metadata internally
- future pattern-specific validation

### Receive/send lifecycle

The intended model is:

- caller owns or reuses a `Net_Message`
- receive clears the message, resets read/write state, and fills the body
- caller may append more data to the same message
- send transmits the same message object

This keeps the API message-oriented and avoids a separate peer argument in the
common reply flow.

### Read/write semantics

Appends grow the message body.

Reads consume from the message body, advancing the read side and removing data
from the logical front of the message.

`clear` resets the message to an empty reusable state while preserving any owned
capacity for efficient reuse. `clear` only resets payload state; it does not
remove hidden routing context.

### Hidden reply context

The message object should carry hidden pipe metadata internally.

That means:

- TCP messages can remember which accepted pipe they came from
- UDP messages can remember which pseudo-pipe they came from

This preserves the simple public API while still allowing multi-client and
multi-peer reply semantics later.

That hidden metadata should also be inspectable through the public API so a
server can identify the current sender while handling a request. The useful
surface is:

- `net_message_id`
- `net_message_url`

Those values should:

- remain available after `net_message_clear`
- stay stable for the lifetime of the originating pipe
- be replaced on the next successful receive into that message

## Phase 4: Unified Pipe Model For Multi-Client TCP

### Scope

Add one internal concept, `Net_Pipe`, for any replyable flow of data:

- TCP accepted clients become TCP pipes
- UDP senders become UDP pseudo-pipes

This keeps the public API message-oriented while giving the runtime a real
representation for where a message came from.

### Why this is needed

The current implementation is enough for:

- single-peer TCP
- multi-peer UDP replies via hidden message metadata

It is not enough for a bound TCP socket to talk to multiple clients, because
the socket still cannot remain a listener and also represent many active client
streams at once.

### Core runtime model

The internal model should become:

- `Net_Socket`
  - public endpoint handle
  - owns the listening socket or default connected socket
  - owns any server-side TCP pipe table
- `Net_Pipe`
  - one replyable communication path
  - TCP pipe: one accepted client file descriptor plus framing state
  - UDP pipe: one remembered sender address associated with a bound socket
- `Net_Message`
  - payload plus hidden `Net_Pipe*`

This deliberately replaces a separate route abstraction. The real reusable
concept is a path where data can flow back to the sender.

### Proposed internal shape

Exact fields can change, but the direction should look like:

```c
typedef enum : u8 {
    NET_PIPE_TCP,
    NET_PIPE_UDP,
} Net_Pipe_Kind;

typedef struct Net_Pipe {
    Net_Pipe_Kind kind;
    Net_Socket*   owner;
    u32           id;
    bool          closed;

    union {
        struct {
            int fd;
        } tcp;

        struct {
            sockaddr_in addr;
        } udp;
    };
} Net_Pipe;
```

The first TCP version will likely need more fields than this for framed receive
progress and any pending message state. That is expected.

### Message behaviour with pipes

Messages should carry hidden `Net_Pipe*` metadata internally.

That means:

- `net_recv` on a TCP server can attach the originating accepted pipe
- `net_recv` on a UDP socket can attach the remembered sender pipe
- `net_send` can reply via that pipe without any explicit peer parameter

`net_message_clear` should continue to preserve this hidden pipe association so
the same message object can be reused for replies.

### TCP server behaviour

For a bound TCP socket:

- the listening file descriptor remains a listener
- accepted clients become managed TCP pipes
- each pipe owns its own framing progress and closure state
- receive logic finds one complete framed message from one pipe
- the received `Net_Message` remembers which pipe it came from

This removes the current temporary behaviour where the first receive mutates a
listener into a single connected client.

### UDP behaviour

For a bound UDP socket:

- a sender can be represented by a UDP pseudo-pipe
- the pseudo-pipe remembers the sender address
- `net_send` can reply via that stored address

This unifies TCP and UDP reply semantics around the same hidden message
metadata.

### Polling and readiness

The first practical TCP multi-client implementation should use a readiness API
such as `poll()` for the listener and all active TCP pipes.

That gives one synchronous server-side receive flow:

- accept any pending clients
- wait for readability
- read one framed message from one ready pipe
- attach that pipe to the message

This remains compatible with the current synchronous design and does not
require background threads.

### Testing expectations

This phase should include tests proving that:

- one TCP server socket can receive from multiple connected clients
- replies sent via `net_send` go back to the correct originating TCP pipe
- UDP pseudo-pipe behaviour still works
- pipe closure and removal behave correctly

The demos can remain simple while the tests exercise the multi-client server
behaviour.

### Summary

Recommended default:

- transport internals:
  - heap and/or dynamic arrays
- temporary high-level message objects:
  - arena-friendly
- long-lived runtime state:
  - heap-backed

This keeps the low-level networking path predictable while still making use of
the richer memory tools already available in `core.h`.

## Initial Implementation Steps

1. Move shared private declarations into `internal.h`.
2. Split transport and pattern code out of `socket.c` into separate `.c` files.
3. Add TCP framing helpers in the TCP transport implementation.
4. Teach `net_send` to send framed TCP messages.
5. Teach `net_recv` to read one full framed TCP message.
6. Add a small result for buffer-too-small cases.
7. Tighten result handling with `net_result_string`.
8. Revisit `Net_State` after framing code exists.

## Non-Goals For This Pass

- multi-client TCP server support
- async runtime
- background network threads
- hostname resolution
- IPv6
- full nanomsg pattern set
- public opaque-handle refactor

## Open Questions

These have now been resolved for the first implementation:

1. Maximum message size:
   - yes, define one in `nexus.h`
   - use a conservative default such as 1 MiB
2. Oversized receive behaviour:
   - return `NET_BUFFER_TOO_SMALL`
   - leave the message pending so the caller can retry with a larger buffer
   - report the required payload size via `out_recv_len`
   - if the caller passes `NULL` for the buffer, drop the pending message
3. Zero-length messages:
   - allowed
   - useful for pings
4. Framing extensibility:
   - not needed
   - keep the header as a plain 4-byte length field
5. First real multi-client server direction:
   - message envelopes
   - reusable message objects with append support
   - hidden `Net_Pipe` context carried inside the message object
6. Internal multi-client transport model:
   - one `Net_Pipe` abstraction for TCP accepted clients and UDP remembered
     peers
7. Future connection management:
   - prefer socket options for connect timeout, retry interval, and infinite
     wait behaviour rather than adding specialised connect functions

## Recommended Answers

The implementation should follow the resolved decisions above, and `nexus.h`
should gain documentation comments that explain:

- framed TCP message semantics
- maximum message size behaviour
- `NET_BUFFER_TOO_SMALL` retry behaviour
- `out_recv_len` semantics on `NET_BUFFER_TOO_SMALL`
- the `NULL` buffer convention for dropping a pending message
- zero-length message support
