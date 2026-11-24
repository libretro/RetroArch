# PyroWave checks

`make check` builds and runs the self-contained codec, GPU dispatch-plan,
stream framing and native TCP loopback checks. `make check
SANITIZER=address,undefined` enables the Linux sanitizer configuration.
`make all CC=x86_64-w64-mingw32-gcc EXE=.exe` checks Windows compilation.
Native Windows TCP tests require Winsock initialization, which the harness
performs itself.
The TCP harness uses the shipping readiness-driven endpoint helper for both
IPv4 and IPv6, including connection refusal, occupied listen ports and
startup cancellation. Bounded readiness waits exist only in the harness;
the shipping endpoint and stream pump do not wait.

The default checks generate their own inputs and do not require an upstream
binary or captured frame. External reference reconstruction checks are
explicit: `make reference-check REFERENCE_DIR=/path/to/captures`. That
directory contains `ramp-130x98-420.pyro`, `ramp-321x181-444.pyro`,
`impulse-2x2-420.pyro`, `ramp-1x1-444.pyro` and a matching `.pyro.u16`
planar little-endian 16-bit reference reconstruction for each frame.
Missing or invalid requested captures fail the test. Captures are not
committed to the repository.

`reference_vectors.py`, `reference_reverse.py` and
`reference_decode_files.py` are optional Windows black-box ABI harnesses.
They take an explicitly supplied reference DLL and write generated results
outside the source tree. They do not import upstream implementation code.
See the codec's `PROVENANCE.md` for the reference binary identity and the
distinction between executed tests and pending native backend validation.

GPU numerical harnesses (`gpu_gl*.c`, `gpu_d3d12.c`, `gpu_vulkan.c`) perform
offline readback and waits for validation. Those operations are not the
shipping asynchronous capture path. The latter has separate ownership
checks under `samples/gfx/pyrowave_gl` and `samples/gfx/pyrowave_metal`.

`make color-check COLOR_SHADER=/path/to/color.comp` checks 1,024 color
configurations against a double-precision oracle. The oracle derives gamut
matrices from primary chromaticities and D65, not shader constants. It tests
code values and scRGB, sRGB/PQ transfer, independent primaries/matrix flags,
range, depth, siting, crop and flip, including negative and HDR samples.
Bounds account for FP32 cancellation before nonlinear transfer and the
conditioning of PQ near white. Crop guards and alpha are checked separately.
