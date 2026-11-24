# RetroArch PyroWave stream protocol, version 1

RPWS is an independently designed, ordered, bidirectional byte-stream
transport for PyroWave video and PCM audio. It is not the Pyrolight,
Polaris or GameStream network protocol. PyroWave bitstream interoperability
does not imply interoperability with those applications' session protocols.
The codec payload follows the published PyroWave bitstream specification.

All integers below are unsigned little-endian. There is no structure padding
on the wire. A connection carries one immutable configuration and one nonzero
64-bit generation. Reconfiguration requires a new connection and generation.

## Record header

Each fragment starts with this 64-byte header:

| Offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 4 | ASCII `RPWS` |
| 4 | 2 | Version, 1 |
| 6 | 1 | Kind: CONFIG=1, READY=2, VIDEO=3, AUDIO=4, END=5, REJECT=6 |
| 7 | 1 | Flags, defined per kind below |
| 8 | 4 | Fragment payload bytes, at most 16384 |
| 12 | 4 | Complete record bytes, at most 16777216 |
| 16 | 4 | Fragment byte offset within the complete record |
| 20 | 4 | Reserved, zero |
| 24 | 8 | Generation, nonzero and identical throughout the connection |
| 32 | 8 | Serial |
| 40 | 8 | Presentation timestamp, microseconds |
| 48 | 8 | Duration, microseconds |
| 56 | 8 | Reserved, zero |

Payload immediately follows the header. Offset plus payload must not exceed
the record size. Timestamp plus duration must fit in 64 bits. Unknown kinds,
versions, flags or nonzero reserved fields are protocol errors.

## Configuration and lifecycle

The sender first sends CONFIG with a 64-byte payload:

| Offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 4 | Width |
| 4 | 4 | Height |
| 8 | 4 | Bit depth, 8 through 16 |
| 12 | 4 | VUI flags |
| 16 | 4 | Maximum compressed video record bytes, 8 through 16777216 |
| 20 | 4 | Frame rate numerator, 1 through 1000000 |
| 24 | 4 | Frame rate denominator, 1 through 1000000 |
| 28 | 4 | PCM sample rate, zero disables audio |
| 32 | 4 | PCM channels |
| 36 | 4 | PCM speaker mask |
| 40 | 4 | PCM format: 1=S16LE; 0 when audio is disabled |
| 44 | 4 | Maximum PCM sample frames per record |
| 48 | 16 | Reserved, zero |

VUI bits are: 0=4:4:4 (otherwise 4:2:0), 1=BT.2020 primaries,
2=PQ transfer, 3=BT.2020 matrix, 4=limited range, 5=left chroma
siting. Remaining bits must be zero. Dimensions must be valid for the codec.
Audio supports 8000 through 384000 samples/second, 1 through 8 channels,
and 1 through 4096 sample frames per record. The largest PCM record must
fit in 16384 bytes. The speaker mask uses the low 18 conventional WAVE
speaker-position bits; its population must equal the channel count.
Disabled audio sets all audio fields to zero.

The receiver validates resource limits and prepares its decoder and audio
sink before sending READY (empty payload), or REJECT (four-byte nonzero
reason). No media may precede READY. CONFIG, READY, REJECT and END have
zero flags, offset, serial, timestamp and duration, and equal fragment and
record sizes. The receiver echoes the sender's generation.

After accepted media drains, the sender sends END with an empty payload.
An EOF before END is a disconnect, including EOF in a partial header or
payload. The caller closes the connection after completion or failure.
There is no reconnect, in-band reconfiguration or implicit generation reuse.

## Video and audio

VIDEO contains raw PyroWave SOF and coefficient packets, without a `PWV1`
file header. Record length is at least eight bytes and no greater than the
negotiated maximum. Lengths and offsets are multiples of four. Serial is
a strictly increasing nonzero frame ID; skipped IDs are allowed. Timestamps
are nondecreasing, and duration is positive.

Video flags are bit 0=first fragment and bit 1=last fragment. These flags
must agree exactly with offset zero and fragment end equal to record size.
All fragments of a video record have identical generation, serial, timestamp,
duration and total size, and contiguous offsets. Video records cannot
interleave each other. The sender currently fragments at 4096 bytes; a
receiver accepts fragments up to the header limit.

AUDIO is an unfragmented interleaved S16LE record. Its serial is the absolute
index of its first sample frame, relative to the session audio origin.
Payload size must be a multiple of channels times two and respect the
negotiated frame bound. Let `end = serial + sample_frames`. Timestamp must
equal `floor(serial * 1000000 / sample_rate)` and duration must equal
`floor(end * 1000000 / sample_rate) - timestamp`, without overflow.
Backwards or overlapping sample ranges are errors. Flag bit 2 must be set
exactly when the serial exceeds the previous audio end, initially zero.
This signals a gap for the playout layer to handle explicitly.

Audio records may occur between video fragments and take priority at the
next fragment boundary. Both media use the same audio-origin timeline.
These timestamps describe synchronization; hardware-clock playout and
buffering policy belong to the integration layer.

## Buffer ownership

The common transport owns no sockets, threads or clocks. One I/O owner
calls its bounded nonblocking pump. A sender lends at most one video and
one audio record until the completion callback retires each lend exactly
once. The receiver acquires one complete video span and writes fragments
directly into it, while audio uses a separate span. Successful commit
transfers ownership downstream; abort or failure cancels each outstanding
reservation exactly once. If a bounded sink has no span, the transport
discards that complete record while retaining framing and audio progress.

`net/rpyrowave_socket.c` adapts an already-connected socket without DNS,
connect or accept inside the stream pump. Its endpoint helper also supports
one nonblocking IPv4/IPv6 connect or single-peer listen. Construction takes
a native resolved address and preallocates the adapter. The caller registers
the endpoint's handle for readable (listen) or writable/exception (connect)
events, then supplies those events to a bounded step. A ready step transfers
the adapter once; freeing the endpoint afterwards preserves that adapter.
Freeing a pending endpoint cancels startup and closes its descriptor.

Native network initialization, resolution, readiness scheduling, admission,
deadlines, peer authentication and connection lifetime remain caller-owned.
No background threads or implicit retries are started. Unregister readiness
before freeing the endpoint; serialize all endpoint calls on its I/O owner.

RetroArch's `record_pyrowave_session` controller supplies the stream callbacks
for GPU packet loans and fixed PCM pools. It uses a separate instance and
generation for each connection. The I/O owner exposes CONFIGURING to the
frontend, which creates compatible resources before accepting. Typed backend
bindings do not record commands or poll fences; those remain render-owner
operations. Session status is also I/O-owned and needs an explicit handoff
before another thread reads it.

For sender EOS, close the GPU and PCM producer queues on their respective
owners, request session finish, and keep pumping through transport END.
Abort cancels transport loans and input reservations. A taken GPU packet
that is still BUSY keeps the session alive until its completion is polled;
session destruction refuses that loan. Cold device-loss recovery can forget
it only before the quiesced/lost backend is explicitly abandoned. Receiver
END closes its input queues; decoded images and audio-device loans still
retire through their consumers. Frontend routing and hardware A/V clocks
are not supplied by this controller.

`record_pyrowave_connection` joins startup and the session controller. It
preallocates both before scheduling, supports connect or single-peer listen
with either protocol role, and exposes the current native handle and readiness
interest. The I/O owner unregisters its previous handle before step, abort or
free, since accepting a peer can replace a descriptor. Re-register the new
handle only when interest is nonzero. Writable interest follows actual queued
protocol work, not whether a TCP socket happens to be writable. Producer/GPU
completion, accept/reject and stop commands must also wake the I/O owner and
call step(0). No resolver, background thread, retry timer or pacing loop is
provided. Frontend scheduling and admission remain caller-owned.
