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
frames. Multichannel inline sources retain ordinary playback. Inline device
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
