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
Active stretch uses tempo 1x to retain nominal device duration. Speed-policy,
settings activation, multichannel and non-1x wrapper validation remain separate.
