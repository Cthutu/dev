# Nexus Plan

## Goal

Keep `nexus` as a small message-oriented networking layer with a simple public
API:

- `net_bind`
- `net_connect`
- `net_send`
- `net_recv`
- specialised constructors where the behaviour is materially different:
  - `net_socket`
  - `net_request_socket`
  - `net_reply_socket`
  - `net_telnet_socket`

The design target is still nanomsg-inspired simplicity, but with enough
runtime structure to support TCP, UDP, request/reply, and telnet cleanly.

## Current Status

The following work is already in place:

- TCP message framing
- UDP datagram transport
- message-only public send/receive API
- reusable `Net_Message` objects
- hidden pipe-based reply routing
- sender inspection via:
  - `net_message_id`
  - `net_message_url`
- multi-client TCP server support
- request/reply socket kinds
- telnet socket kind with:
  - line-oriented TCP behaviour
  - character mode via socket option
  - telnet negotiation filtering
  - NAWS-based bounds query via `net_telnet_bounds`
- socket options for:
  - connect retry
  - connect timeout
  - send timeout
  - receive timeout
  - non-blocking mode
- demos
- Nexus-specific tests in the shared test framework
- overview and how-to documentation

## Code Organisation Rules

These rules remain active:

- Put shared private declarations in `internal.h`.
- Keep public API types and declarations in `nexus.h`.
- Split implementation concerns into separate `.c` files inside `src/nexus`.
- Keep transport logic separate from protocol logic.
- Progress on Nexus must include tests in the existing test framework.

This means:

- `internal.h` is for shared private structures, helpers, and ops tables
- `nexus.h` is for the supported public surface
- new behaviour or bug fixes should be covered in `tests/nexus`

## Current Architecture

### Public model

Nexus currently has four public socket directions:

- `net_socket`
  - flexible message transport without enforced turn-taking
- `net_request_socket`
  - send, then receive, repeating
- `net_reply_socket`
  - receive, then send, repeating
- `net_telnet_socket`
  - line-oriented telnet text over TCP

### Internal model

The implementation currently separates:

- transport:
  - TCP
  - UDP
- protocol / socket behaviour:
  - basic message
  - request/reply
  - telnet

It also uses:

- `Net_Pipe`
  - one replyable communication path
  - accepted TCP clients and remembered UDP peers both use this model
- `Net_Message`
  - payload plus hidden pipe metadata
- specialised private socket runtime structs
  - `Net_SocketData`
  - `Net_ReqRepSocketData`
  - `Net_TelnetSocketData`

## Telnet: Current Behaviour

The first telnet milestone is done.

Current telnet support provides:

- TCP-only telnet sockets
- line-oriented receive/send
- outgoing lines terminated with `CRLF`
- telnet negotiation bytes removed from message payloads
- NAWS negotiation request on connection
- `net_telnet_bounds` for querying current client width/height from a received
  telnet message

Current telnet limits:

- no character mode yet
- no explicit change-notification API for bounds updates
- no richer telnet option-state query surface beyond bounds

## Next Work

### 1. Richer telnet session state

Only add the telnet state that helps real interactive applications.

The most likely useful pieces are:

- whether NAWS bounds are known
- whether the peer supports echo negotiation
- whether the peer supports suppress-go-ahead
- whether the session is currently in line mode or character mode

This should stay query-oriented and cached. Higher layers can decide whether
changes matter.

### 2. Telnet-specific documentation

Current telnet docs cover:

- line-based telnet servers
- screen size querying

Still useful later:

- how to build a character-mode telnet server
- how to use telnet bounds in a layout-driven application

### 3. Possible public wait/poll API

This is optional, not the current priority.

If needed later, a public wait/poll API should reflect message readiness rather
than raw file-descriptor readiness.

## Telnet Features Worth Having For A MUD

The telnet features most likely to matter for a MUD engine are:

- suppress-go-ahead negotiation
  - useful for more interactive telnet behaviour
- echo negotiation awareness
  - useful for password entry and input-mode decisions
- NAWS bounds
  - already implemented
  - useful for paging, prompts, status bars, and wrapping
- terminal type negotiation
  - useful if you later care about ANSI behaviour quirks or client capability

The features that are probably not worth over-investing in early are:

- a large fully-general telnet state machine surface in the public API
- change-detection events for every telnet property
- exposing low-level telnet command traffic directly to callers

The better Nexus boundary is:

- Nexus negotiates and caches useful telnet session state
- Nexus exposes small query functions
- the MUD layer decides how to react

## Testing And Verification

These remain required:

- new behaviour should come with Nexus tests
- demos are useful, but they are not a substitute for tests
- debug and release test runs should both remain green

## Immediate Next Step

The next concrete implementation step should be:

- richer telnet negotiation state, especially:
  - suppress-go-ahead
  - echo negotiation awareness
  - terminal type negotiation

That is the main missing telnet capability between the current implementation
and a more useful foundation for a MUD engine.
