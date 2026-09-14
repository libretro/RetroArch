# Shipping frontend audio checks

`make check` exercises `audio_driver.c` with scripted audio devices, including
the native transport quality checks. Run just those checks with
`DM_ONLY=transportquality ./discrete_multichannel_test`.

`DM_ONLY=inline ./discrete_multichannel_test` exercises configured inline
stereo transport through the int16 and float batch entries. Ten cases cover
tempo 0.25..32, pitch/duration, native SRC lanes, allocation failure,
stop/restart, unsupported-speed/layout/format fallback, recovery and the LPF.
Each callback must release every borrowed source view before returning.
Storage is prepared before playback; the output block holds 1024 native stereo
frames. Negotiated multichannel cores use canonical native storage. Inline device
writes retain their existing blocking/nonblocking and short-write behavior.

`DM_ONLY=canonicalreserve ./discrete_multichannel_test` checks the producer's
canonical staging reservation: initial/growth allocation failure preserves
state, and repeated reservation plus varying 5.1/7.1 native batches do not
reallocate. Pipeline initialization reserves the bounded maximum before the
worker starts (1024 canonical float frames, 44 KiB). This moves the existing
maximum staging cost to startup; the stereo pipeline does not reserve it.

The 48 quality cases cover int16/float, stereo/5.1, normal sinc at 48 → 44.1 kHz
and HQ sinc at 48 → 96 kHz, and tempos 0.25, 0.5, 1, 2, 8 and 32. They activate
transport through the configured startup helper, publish through the frontend,
and use bounded consumer steps with zero/partial device writes and EOF drain.
Known achieved-speed estimates avoid depending on the test host's wall clock.

The device captures a 440 Hz signal with alternating channel polarity. Checks
require pitch within 2%, non-silent bounded samples, channel coherence within
0.002, and exact source publication/drain accounting. Duration must be within
two output frames at unity; active transport allows
`512 * (1 + 1 / tempo) * output_rate / input_rate` output frames for finite-stream
window/overlap edges. Pitch and energy omit the first and last eighth of the
capture to exclude startup/end transients. These are independent output
oracles, not comparisons against another execution of the same renderer.

The existing samples/audio CI runs this target plain and under ASan/UBSan.
These tests do not measure CPU throughput or physical-device latency and do
not establish perceptual quality for arbitrary content. Live transition,
LPF, routing, and threaded-wrapper stress have separate fixtures.

`DM_ONLY=inlinewide ./discrete_multichannel_test` adds 32 native quad/5.1/
side-5.1/7.1 cases through discrete and downmix sinks, normal and HQ SRC,
slow and accelerated tempo, channel gain/polarity coherence, pitch/duration,
layout handoff, fallback/recovery and teardown. Slot-order checks cover the
shared 7.1 packer: side channels occupy slots 9 and 10, not 6 and 7.

Multichannel transport prepares one 48 KiB int16 or 96 KiB float arena for
canonical source/output and stereo scratch. It does not allocate transport
storage on layout changes. The existing extra-channel SRC path still prepares
layout-dependent state on first use or layout changes; its input reservation
covers the full bounded output block to avoid growth as batch sizes vary.
Layout changes discard retained transport history and reset SRC; they are not
seamless tail-preserving handoffs. Stereo-only cores retain their smaller arena.

`DM_ONLY=inlineformat ./discrete_multichannel_test` covers late multichannel
negotiation and both native format directions with normal/HQ SRC. It checks
repeated negotiation, retained stereo/wide storage, stale-tail isolation,
slow-motion duration, accumulator flushing, allocation failure, stop and
negotiation after teardown. Negotiation runs
between source callbacks; a changed format rebuilds optional transport storage
and resets retained DSP history and cadence. Allocation failure uses ordinary
playback until reinitialization. No transport allocation is added to sample
processing. Returning to stereo retains the canonical arena.

`DM_ONLY=callbackcontinuity ./discrete_multichannel_test` compares uninterrupted
batch playback against callback delivery interrupted by pause and suspension.
Two native cases cover float batches with HQ SRC and the int16 single-sample
accumulator with native SRC, through prepared slow-motion transport. Audible
output must match byte for byte; paused callbacks cannot invoke the source,
and suspended callbacks cannot retain speculative samples or advance output.
The stage and its arena remain attached without new transport allocations.
A persistent worker invokes callbacks. A condition-variable rendezvous holds
each source callback open while the main thread ends a frame; pending int16
samples must remain owned by the worker until the callback returns. Completion
synchronizes capture reads and buffer reuse. Pause and suspension use the same
worker, without added cases or sleep-based scheduling. Rendezvous waits have a
30-second failure bound. This checks a controlled two-thread interleaving,
not arbitrary real-core scheduling or whole-runloop acceptance.

The callback continuity cases also insert a source callback that emits no samples.
It must report no device progress, allowing the wrapper to park, without changing
subsequent audible output. Normal callback results are checked against captured
device output rather than assuming every source call produces an immediate write.
