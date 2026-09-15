# Native low-pass core

Run `make check` (C89); `make check SANITIZER=address,undefined` enables
sanitizers on supported toolchains. The 62 cases cover both native formats,
all 1..11 channel widths at 8/44.1/48/96/192 kHz, chunk invariance with changing
targets and reset, dry bit identity, invalid arguments, full-scale DC,
silence decay, channel isolation, native-lane agreement and measured response.
Allocator wrappers assert that initialization, controls, processing and reset
make no heap calls.

The bounded transport stage owns this component after WSOLA and before SRC.
Ordered metadata supplies core-rate cutoff Hz; zero disables filtering. Wet
direct input is filtered into the existing stage output buffer in one pass;
WSOLA output is filtered in place. Partial acknowledgements do not filter
retained samples again. Dry inactive stages still return direct ring views.
Playback activation, settings and speed-to-cutoff policy remain separate work.

`audio_speed_lpf_process_into` also accepts disjoint source/output buffers.
It preserves the source, rejects partial overlap, and matches in-place output
including when a fade becomes dry within the block.

Two cascaded real poles give a combined -3 dB cutoff. Each pole's coefficient
stays between zero and one throughout interpolation. Float uses float history;
int16 uses Q16 history and Q30 coefficients with signed 64-bit products.
Integer convex updates cannot exceed native input extrema. Rounding can leave
sub-sample internal residue at silence, but tests require zero native output.
The float lane flushes magnitudes below 1e-20 while wet to avoid denormals.

Engagement primes history from the first actual sample and fades over 50 ms.
Cutoff changes interpolate at a 64-source-frame cadence, reaching the exact
endpoint after 50 ms; callback splitting does not change this clock. Repeated
targets do not restart ramps. Disengagement stops touching samples/history
when dry. Reset clears history and restarts engagement for an enabled target.
Control and process calls belong to one owner; no internal locks are provided.
