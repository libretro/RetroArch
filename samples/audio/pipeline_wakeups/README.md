# Audio pipeline wakeups and wrapper lifecycle

`make check` runs the direct callback timing fixture. `make check-transport`
selects prepared dry, LPF and active WSOLA stages. `SOURCE_FLOAT=1` selects
native float source; the default is int16. `DEVICE_INT16=1` selects an int16
device; float output is the default. Int16 source/output selects native int16
sinc SRC; the other combinations use float SRC.

`make check-wrapper` links and runs the shipping audio thread wrapper with the
shipping frontend callback. It covers all four source/device format combinations and all three
transport modes, eight publication counts, and both device pacing regimes.
Each setting performs eight lifecycle cycles after its source workload:

- Apply a discard through a control transaction while running.
- Stop and verify that control/release/rebuild cannot advance the callback.
- Recreate the transport while parked and publish its processing request.
- Restart, submit fresh source and require a new device write within one second.

Frontend conversion calls are counted during wrapper runs. Matching-format
lanes must process zero converted samples; mixed lanes must convert samples
only toward the device format. Mixed lanes also provide positive controls for
the counters. Int16 source/output must actually process input through native
int16 SRC. These checks cover the frontend's conversion entry points, not
arbitrary arithmetic in separately compiled DSP code.

The wrapper owns its worker; there is no second fixture consumer in this mode.
Transport release precedes wrapper destruction, which joins the worker before
ring/device storage is freed. SRC instances are freed between settings too.

Wrapper results are correctness counts, not steady-state timing: lifecycle
cycles submit additional audio and intentionally discard retained history.
Run this target under an external timeout to detect a broken stop handshake.
The test does not compare rendered samples or test physical hardware. Existing
native frontend sample-oracle and thread-handshake suites remain complementary.

For manual runs: `WRAPPER=1 SOURCE_FLOAT=1 TRANSPORT=stretch ./pipeline_wakeups_test 1`.
Stretch defaults to 1x. `TEMPO` accepts 0.25, 0.5, 1, 2, 4, 8, 16 or 32 with
`TRANSPORT=stretch`. Source frames per video frame scale with tempo; the source
ring and processing budgets stay fixed. Large publishes therefore exercise
space waits before frame end. Restart stress submits enough extra source to
prime the fixed ring at slow tempos too.

`make check-tempo` runs both matching native lanes at all eight tempos, with
0.25 seconds of source per setting followed by the restart checks. It verifies
progress and conversion invariants, not exact duration, sound quality or
steady-state throughput. Put it under an external timeout to catch lost wakes.
This target explicitly publishes a fixed tempo before its source.

`make check-live` tests all four source/device combinations with
`LIVE_CONTROLS=1 WRAPPER=1 TRANSPORT=stretch`. Before the existing restart stress,
each setting publishes eight live processing requests: 0.25x, bypass, 32x,
0.5x, bypass, 16x, 2x and 4x, alternating dry and 1 kHz LPF targets. No request
sets the reset bit. Source is published after each request; the test requires
its boundary to retire, its source to drain and device writes to advance within
two seconds. Consumer metadata is inspected only in a parked control transaction,
requiring the requested control/cutoff and an unchanged reset serial.

The ring and processing budgets remain fixed, and conversion invariants still
apply. This checks live control progress and ordering, not sample continuity,
pitch accuracy or waveform identity. Parking for observation preserves stream
history, but this is not a physical-device uninterrupted-playback test.

`make check-auto` enters through the settings activation helper used during
audio initialization. It enables `audio_time_stretch` and
`audio_time_stretch_lowpass`, then exercises producer speed updates, fallback,
automatic recovery and explicit restarts in all four source/device formats and with
live source layouts. Both options default off and request audio reinitialization
when changed through Settings > Audio > Synchronization (advanced settings).
Threaded Pipeline must be enabled and supported by the driver. Core-owned audio
callbacks keep their inline path. Unsupported speeds suspend transport;
supported speeds resume it after source and device output drain, reusing its
allocated storage. A continuously nonempty queue defers recovery until a
natural drain or pause; recovery does not force a source drop.
