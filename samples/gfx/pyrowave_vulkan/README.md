# Native Vulkan asynchronous codec and receiver factory

`make check check-player` runs the native adapter and the frontend's cold
SPIR-V loader/decoder factory. Set `RPY_REQUIRE_VALIDATION=1`,
`RPY_VALIDATE_SYNC=1` and `RPY_VALIDATE_SHADER_ACCESSES=1` to require
synchronization validation; `RPY_ALLOW_SOFTWARE=1` permits llvmpipe for Linux
sanitizer runs. The factory target takes its exported SPIR-V directory as its
only argument. Windows executables use `EXE=.exe` when cross-compiling.

The tests compare code-value and signed scRGB images, exercise 1080p,
noncoherent/cache fallback, immutable packets, unsubmitted discard and device
loss retention. Display returns alternate between a pending final-use fence
and an already-completed one. A two-slot case shares one fence between a
decode and a display loan, checks that physical completion does not itself
acknowledge the loans, and verifies that poll or unsubmitted discard retires
all references. Pixel readback and bounded waits exist only in this offline
oracle. Existing VFS helpers keep their normal GNU dialect; codec, factory
and oracle remain C89.

The builtin receiver appends decode outside the renderer's render pass and
lends its GENERAL-layout image to the existing filter chain. Its private
fence pool retains the last display signal across repeated draws. A signal
is reset only after native acknowledgement, physical completion, and release
of the frontend's own display pin. A new draw replaces that pin only after
its primary submission succeeds. The renderer separately signals its ordinary
frame fence so frame-context recycling cannot erase the decoder's signal.

Actual application pixel, shader-history, EOF, abort, early-stop, missing-asset
and reconnect tests are in `../pyrowave_session/receiver_oracle.py`. Use
`--backend vulkan --require-validation` with a configured synchronization
validation layer. `frontend_fault.py --receiver` fails finalization after the
eighth successfully recorded decode. `--mode gpu-failure` requires the fault
marker, resource retirement and normal frontend exit; it does not count a
failure while merely waiting for a network packet as decoder cancellation.
These application fixtures use null audio and do not prove physical A/V.
