# mDNS / DNS-SD

`net/net_mdns.c` is an optional, socket-neutral C89 discovery engine. Compile
it with `HAVE_MDNS` and include `net/net_mdns.h`. It has no RetroArch dependency,
worker thread, resolver call, lock, sleep, or internal clock read.

The module browses one service type and can publish one service instance per
context. It implements PTR/SRV/TXT/A/AAAA records, compressed-name decoding,
probing and simultaneous-probe tie-breaking, established-name defense,
announcements, known-answer suppression, multicast and QU replies, legacy
unicast queries, service-type enumeration, cache-flush grace, TTL refresh,
and goodbye expiry. Discovery events carry the resolved port, TXT bytes,
and one IPv4 and/or IPv6 address. Discovery is not authentication.

## Ownership and scheduling

Create one context **per network link**, with that link's IPv4/IPv6 addresses.
The application owns interface enumeration, socket lifetime, multicast
membership, and IPv6 scope IDs. It must receive/send UDP 5353 multicast on
`224.0.0.251` / `ff02::fb`, set IP TTL / IPv6 hop limit 255 for multicast and
unicast replies, and pass the received TTL/hop limit to `net_mdns_receive()`.
Do not substitute 255 for missing ancillary metadata. Off-link traffic must
not be admitted. A `NULL` destination in the send callback means multicast;
a non-NULL destination is a reply to that peer on the context's link.

Enable discovery explicitly with `net_mdns_browse()`. Publication is separate:
`net_mdns_publish()` copies its configuration and probes before advertising.
Use a descriptive instance label and a unique ASCII host label. On
`NET_MDNS_CONFLICT`, publication stops: choose a new instance/host label and
publish again. Browsing can continue during a publication conflict.

Supply monotonic milliseconds from the application's existing event loop.
Call `net_mdns_poll()` after readable events or when `net_mdns_timeout()`
expires. Backpressure schedules a bounded retry; never poll in a busy loop.
Callbacks borrow packet/service storage only for their duration and must not
reenter or destroy the context. Service getters borrow a snapshot until the
next receive/poll/browse call. Unpublish/free emits a best-effort goodbye
while the transport is still alive.

Limits are deliberate: 32 resolved services, 128 cached records, 64 questions,
128 records per packet, 9000 received bytes, 1400 transmitted bytes, and 512
TXT bytes. Oversized/invalid TXT does not become a discovered service.
Resolution tolerates out-of-order and split answers. Addresses unrelated to
the browsed service's SRV targets do not occupy the cache. Each service view
selects one current address of each family; applications needing complete
multi-address RRsets require an extended result API. Truncated packets and
multi-packet known-answer continuations are rejected. No DNS-over-TCP
fallback, DNS Push, DNSSEC, or routed/unicast DNS browsing is provided.

## Build and test

```
make clean check SANITIZER=address,undefined
make clean check CC=clang SANITIZER=address,undefined
make clean all demo CC=x86_64-w64-mingw32-gcc EXE=.exe
```

The default test is self-contained: no network interface, multicast routing,
daemon, Python package, or sleep is required. It covers packet validation,
compression, conflict resolution, cache replacement/expiry, deadline-driven
retries, and a malformed-input corpus. CI also compiles/runs with MSVC `/TC`
on x86 and x64.

## PyroWave discovery example

`make demo` builds `obj/mdns_discovery` (`.exe` on Windows). This **desktop
IPv4 transport example** advertises/browses `_pyrowave._tcp.local.` and
demonstrates native ancillary TTL validation, nonblocking sockets, and an
event-driven `select()` loop. The engine itself also handles IPv6 addresses;
the example socket transport does not join IPv6 multicast groups.

On one machine, advertise a hypothetical ready receiver on its own interface:

```
obj/mdns_discovery publish "Living Room" 192.168.1.50 12345 30
```

On another, browse on its own interface:

```
obj/mdns_discovery browse 192.168.1.60 30
```

It prints `ADD`, `CHANGE`, and `REMOVE` events. The example does not start a
stream or open the advertised TCP port. It is a discovery harness, not an
enabled-by-default advertisement in RetroArch. The frontend device picker,
capability negotiation, and receive/accept controls are separate integration
work; no PyroWave core is introduced.

Optional real multicast interoperability test, using an independent
[python-zeroconf](https://github.com/python-zeroconf/python-zeroconf) installation:

```
python3 interop.py --demo obj/mdns_discovery --interface 127.0.0.1
```

This checks C publication/resolution and goodbye handling in both directions.
It is separate from the deterministic default checks because multicast
availability is a host/network property.

Protocol references: [RFC 6762](https://www.rfc-editor.org/rfc/rfc6762) and
[RFC 6763](https://www.rfc-editor.org/rfc/rfc6763).

The `discovery` example now uses the reusable `net_mdns_socket` adapter, so
`make demo` compile-checks the same nonblocking transport that RetroArch uses.
The adapter selects one primary IPv4 interface, validates received TTL 255,
and handles at most eight received datagrams per poll. The protocol engine
remains socket-neutral and resolves both A and AAAA records. Applications
that require multiple interfaces can create one adapter per explicit address;
IPv6 multicast transport is not provided by this adapter.
