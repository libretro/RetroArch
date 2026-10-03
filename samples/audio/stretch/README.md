# Bounded native transport stretcher

## Borrowed source spans for inline owners

`audio_stretch_stream_push_view_limit` reuses the bound stream adapter while
exposing inactive, quiescent source spans without copying. The owner must keep
the accepted source prefix alive and unchanged until `consume` or reset; the
returned view is read-only. Active processing and exit transitions still use
the bound output buffer. No source ring, allocation or format conversion is
added. This API alone does not activate inline frontend time stretching.

`stretch_test` compares copied and borrowed output bit for bit across native
int16/float stereo, 5.1, 7.1 and 11-channel spans. It checks alternating active
and inactive requests, fragmented budgets, partial acknowledgement, stable
pending views, EOF/reset, untouched dry output storage and guarded heap calls.

## Engine

This is an engine foundation, not an enabled frontend transport mode. The existing
WSOLA pitch DSP is unchanged. Normal playback does not call or feed this engine.

`audio_stretch_new` chooses one native sample format and allocates one contiguous
working set. The caller owns its lifetime and must serialize processing/reset.
The search mask uses **channel indices**, not speaker-position bits: the future
frontend adapter must map its layout and exclude LFE. The highest-energy eligible
overlap channel supplies the search reference; all channels use the same selected
offset. This avoids cancellation on anti-phase material. Ties prefer the nominal
position, then the closest candidate (lower index for equal distance).

At 48 kHz, synthesis hop is 128 frames, the overlap window spans 256 source
frames, and search radius is 512. Hop scales as round(rate / 375), with rates
restricted to 8..192 kHz and 1..8 channels. Radius is 4 * hop: a splice can
only land on matching phase if the search reaches a full period, and 10.7 ms
covers fundamentals down to about 47 Hz. The ring holds only 2*hop + 2*radius
frames, irrespective of tempo (0.25..32). High tempos
consume skipped source directly. A shared linear search neighborhood is staged
once per hop; sample storage stays native. Integer correlation uses int32
references and int64 accumulation, with native int16 overlap synthesis using
Q15 weights and nearest rounding, halfway away from zero. Float uses the shared
scalar/SSE2/NEON correlation selected once at creation.

Fractional source advance is Q32 with rounding error at most half a Q32 unit
per hop. Scheduling follows the nominal position, not the correlation offset,
so the search cannot accumulate tempo drift. Each synthesized hop schedules its
next analysis advance using that call's tempo. Large tempo changes should be
slewed by the future transport owner, not by an independent queue controller.

Processing reports actual input consumed and output produced. It emits directly
into caller storage whenever a complete hop fits. Otherwise it retains at most
one hop for partial-output calls. Zero output capacity consumes nothing. Invalid
parameters return false and leave state unchanged (result counts are zeroed).
Input/output storage must not overlap, and its sample format must match creation.

Startup needs two hops of input; later search also needs lookahead. Zero-input
process calls can emit a pending synthesized hop. To exit or end a finite input,
`audio_stretch_drain` returns pending synthesis, the last overlap, and source
lookahead not already represented by that overlap. Nothing is padded, synthesized
or converted by drain. Repeated partial calls preserve order and report complete
only after all available tail data has been emitted. A positive-capacity drain
call latches drain mode; process then rejects requests until reset. Zero-capacity
drain calls are non-mutating queries. Reset discards retained data without freeing
storage, including a partially drained tail.

High-tempo processing may already have skipped source beyond the last overlap.
Drain reports that gap once via `gap_offset`, relative to the current output
buffer. The marker may equal `output_frames`, even on the final call: the gap
then precedes future caller input. Otherwise `(size_t)-1` means no new marker.
The owner must retain enough transition context to reconcile that discontinuity
(e.g. crossfade); the drain API does not invent skipped audio or guarantee a
click-free boundary. Entry/exit crossfades, stream epochs and device short writes
still require frontend integration before exposing a transport mode.

`make check` builds C89 tests and guards heap calls during processing/reset.
`make -B check CC="gcc -DAUDIO_STRETCH_SCALAR"` tests scalar selection.
`SANITIZER=address,undefined` is supported by the Linux CI target. Tests cover
fragmentation, canaries, fractional consumption, invalid requests, backpressure,
reset, partial native tail drains, gap markers, full-scale integer samples,
anti-phase/coherent channels, LFE exclusion
from the reference, duration bounds and a coarse 440 Hz pitch check. They do not
constitute listening or device-latency acceptance.

The tests print allocated bytes (including state) for 2/6/8 channels at several
rates. On x86-64, 48 kHz uses 11,656/23,944/30,088 bytes for int16 and
18,056/42,632/54,920 bytes for float. The largest supported eight-channel float
instance uses 219,272 bytes at 192 kHz. ABI padding can change these figures.

## Native transition spans

`audio_stretch_crossfade` blends caller-owned outgoing/incoming spans into caller
output without retaining state or allocating memory. Use one total frame count
and advance the offset across fragmented calls. The same Q16 linear weight is
shared by all channels; endpoints select the source exactly. Int16 uses convex
int64 accumulation with nearest rounding, ties away from zero. Float remains
float. The weight recurrence avoids per-frame division. Output can alias either
input exactly; partial overlap is unsupported. Length is bounded to 1..65536
frames, and a one-frame transition selects incoming.

The helper does not acquire history, schedule transitions or consume drain gap
markers. The runtime owner must retain suitable outgoing audio and select spans
before calling it. There is no added engine storage or normal-processing work.
Tests cover all 1..8 channels, both lanes, endpoints, full-range integer inputs,
fragmentation, exact aliasing, invalid requests and allocation guards. Playback
integration and listening/device acceptance remain pending.

## Transition owner

`audio_stretch_transition_new` allocates a single native tail ring and metadata.
It is optional: inactive playback must bypass it. The owner retains up to the
chosen tail length before emitting continuous audio. For example, 128 frames
adds 2.67 ms of holdback at 48 kHz. Large blocks copy their middle directly to
caller output; only the bounded tail passes through the ring. Small fragmented
calls can require two ring copies. Process, boundary, flush and reset allocate
nothing and never convert sample formats.

At a drain gap, submit all pre-gap frames to the transition owner, retrying any
unconsumed input. Then call `audio_stretch_transition_boundary` exactly once
before submitting post-gap frames. A gap at the end of a buffer applies before
future caller input. The retained outgoing tail overlaps incoming frames with
the shared native crossfade, shortening combined duration by the overlap length.
No search/alignment is performed at this boundary. If a boundary arrives before
the current overlap finishes, it is rejected without mutation; runtime policy
must handle rapid transitions explicitly. Do not silently drop that boundary.

Flush emits an ordinary retained tail and latches EOF until reset. If EOF occurs
after partial blending, unused outgoing frames are discarded. If no incoming
frames arrived, the original tail is preserved. Zero-capacity calls do not
mutate state. Reset discards retained data for an explicit stream discontinuity.
This owner does not own device writes, stream epochs or the engine itself.

Tests compare bulk and fragmented operation against a two-segment reference,
including short/empty streams, wrapped rings, partial EOF, reset and allocation
failure. Actual high-tempo engine drains are split at their gap markers and
rejoined with future source input in both native lanes. Runtime frontend wiring,
short-write ownership, entry scheduling and listening acceptance remain pending.

## Stream adapter

`audio_stretch_stream` owns the engine, transition owner and one hop of native
exit staging. Construction performs three allocations; processing and reset perform
none. All objects are single-consumer. The runtime must publish control changes
to that consumer and call reset at a stream discontinuity; the adapter does not
provide atomics, epoch publication or device I/O.

Process accepts a desired active flag and tempo with consumed/produced counts.
Raw state copies caller input directly. Activation starts the engine with empty
history. Exit drains pending synthesis/lookahead through the transition owner,
applies the reported gap boundary, joins future raw input if needed, flushes
retained transition audio and returns to raw. A subsequent activation waits for
this exit to finish. EOF drains without future input and latches until reset.
Zero-capacity calls do not mutate state, including zero-capacity EOF queries.

Active processing emits directly from the engine into caller output; its
consumed/produced counts match direct engine calls. The adapter stages at most
one hop during exit, retaining partial progress and gap markers across output
backpressure. Exit handling retains the transition tail, but steady active
processing adds no holdback beyond the engine's own lookahead. The runtime must retain produced audio until
SRC/device consumers accept it; resetting or reprocessing a partially written
output buffer is incorrect. Default inactive frontend paths must bypass the
adapter entirely. No frontend setting is enabled by this patch.

Tests cover repeated raw/stretch transitions, rapid requests, EOF, allocation
failures at all three construction stages, reset during processing, output
canaries and bulk/fragmented equality. Uninterrupted active output is compared
against direct engine processing plus drain at slow, unity and fractional tempos.
Full runtime ownership, SRC capacity integration and hardware acceptance remain.

## Caller-owned pending output

Bind a native output buffer before using a stream, then use push, peek, consume
and finish instead of direct process/flush. Push writes directly into that
buffer. While any frames remain unconsumed, push validates the request but
consumes no input and preserves the pending bytes; an exit request still latches.
Peek returns the remaining span in place, and consume acknowledges only accepted
frames. A zero acknowledgement preserves everything; an oversized one fails.
The buffer may not overlap input and must remain alive until reset/unbind/free.

Finish latches EOF immediately, but reports complete only after downstream
acknowledges all produced frames. Reset deliberately discards pending samples
and preserves the buffer binding. Rebind/unbind is allowed only in empty raw
state. Direct process/flush reject a bound stream to prevent mixed ownership.

The API adds a pointer and three size_t fields to adapter metadata (32 bytes on
x86-64), no sample storage, allocations, copies or format conversions. Exit
staging/holdback costs still apply. The frontend can supply an existing
sized arena slice. A downstream SRC must acknowledge source frames actually
consumed; its produced device samples need their own short-write lifetime.
This API does not itself change driver write behavior or publish stream epochs.

Tests compare 144 combinations of rate, width, native format, tempo and output
capacity against direct adapter output while accepting only 1..11 frames at a
time, including zero acknowledgements and repeated pushes against pending data.
EOF acceptance, reset, output canaries, invalid acknowledgements and forbidden
API mixing are covered. Frontend/device integration remains outstanding.

## Direct active output regression

The active adapter now forwards the caller output span directly to the engine.
A 256-frame startup at 48 kHz produces its first 128 frames immediately, without
waiting for another engine hop to fill transition holdback. Tests compare each
call's consumption, production and native samples against the engine across
small output capacities and changing tempos. Drain/exit still supplies at least
the engine's final overlap before its possible source-gap boundary, so the
transition owner can retain the necessary outgoing tail during exit alone.

## Real SRC chain integration

`stretch_src_test` links the actual stretch adapter and float/int16 sinc
implementations. It compares a batch-reference chain with fragmented bound
output, partial source acknowledgements and short simulated sink accepts.
Coverage includes stereo/eight channels, ratios 0.75/2/4, HQ off/on (including
below-threshold fallback), and ratio changes at fixed stretched-frame positions.
Each stereo pair must produce equal frame counts. A conservative 128-frame
output budget limits source submissions, and canaries check writes beyond the
reported native SRC output. Allocation wrappers cover guarded processing.

The fixture uses native pair staging and does not link conversion routines.
It is not the frontend's channel-routing implementation or a real device test.
The simulated sink fully accepts each prepared output block before upstream
processing resumes. This establishes the intended ownership order, not driver
short-write behavior. Linux's existing stretch ASan/UBSan job runs both suites;
local sanitizer/device acceptance and frontend wiring remain outstanding.

## Per-pass production budgets

`audio_stretch_stream_push_limit` and `audio_stretch_stream_finish_limit` cap new
output to the smaller of the supplied frame limit and bound buffer capacity.
They allocate and copy nothing beyond existing processing. A zero limit is a
non-mutating validation/query: it neither consumes input nor latches exit/EOF.
Positive limits retain the existing exit/EOF behavior. Pending output is never
truncated to a new limit; its remaining span must still be acknowledged.
Existing push/finish calls retain full-capacity behavior.

Frontend preflight can now pass an SRC input-frame budget without rebinding or
resizing its native arena. This limit is in stretched frames, not source frames
or device bytes. Derive it from the downstream ratio and output scratch/device
budget; then separately cap the SRC submission from any pending span. The API
does not calculate that frontend budget or change device writes.

Tests compare varying limits (0, 1, 7, 127 and SIZE_MAX) against direct adapter
calls, including pending-output zero-budget control requests and EOF. The real
SRC chain fixture now varies production budgets from 1..23 and drains EOF in
seven-frame budgets while retaining exact reference output.

## Quiescent handoff and stream reset

The consumer may bypass the adapter only when the desired mode is inactive and
`audio_stretch_stream_quiescent` returns true. The query requires raw state with
no retained or unacknowledged output; binding storage alone does not prevent
quiescence. EOF is not quiescent until reset. NULL denotes an absent adapter.
This is a read-only single-owner query, not a cross-thread synchronization API.

At a discontinuity, the owner must reset both the stretch stream and all native
SRC instances, and discard any old device-format output under its ownership.
The chain fixture now dirties SRC history and leaves bound stretch output
unacknowledged, resets both while allocation guards are active, then checks
reused processing against a fresh chain for all tested ratios/layouts/formats.
Quiescence tests include one-frame output backpressure, exit, EOF and reset.
Frontend epoch publication and the actual bypass hook remain unimplemented.


## Search oracle and throughput benchmark

`make check` also runs `stretch_search_test`: 2,520 search states across both
native formats, seven rates, mono/stereo/5.1/7.1, wrapped rings, clipped search
windows, channel masks, silence, full-scale constants, alternating extrema,
impulses and deterministic noise. The oracle gathers each candidate directly
from the ring and uses the existing correlation kernels, independently of the
production search staging and ranking loop. It checks search decisions, not
an independent mathematical implementation of the correlation kernels.

Build `make stretch_bench`, then run `./stretch_bench 2 200` for seven timed
trials per format/rate/channel combination at tempo 2. The second argument is
the minimum milliseconds per trial (default 50). The CSV reports the median
nanoseconds per input frame; lower is better. Tempo accepts 0.25 through 32.
Each trial warms the real stream adapter, then processes repeated 8,192-frame
native blocks without resetting between blocks. Initialization, warmup, sample
generation and reset are outside timing. Every call verifies complete source
consumption; this benchmark does not measure exit/drain or device work.

Use identical compilers, flags, benchmark source, tempo and CPU affinity for
baseline/candidate builds. Alternate process order and repeat runs on an idle
machine. `clock()` resolution and scheduling differ across hosts: increase the
trial duration for small differences. Do not run performance acceptance under
sanitizers, or treat one median as proof of no regression. The float and int16
rows must both be checked. `make bench` is optional and is not part of CI;
the search oracle runs through the existing scalar/SIMD sanitizer check job.
