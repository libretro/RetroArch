The presentation interface reports frames actually accepted by the native
writer and the corresponding playback cursor. A discarded queue starts a new
epoch. Silence does not produce a valid source position; a separate drain
proof permits EOF after the final accepted sample without extrapolating that
position through silence. Calls belong to the device's write owner.

On Linux, `make check SANITIZER=address,undefined` opens a real ALSA null PCM
and scripts partial writes, hardware delay, failed queries, XRUN, suspend and
removal. This verifies ownership and discontinuity contracts; the null device
does not validate physical hardware timing.

On Windows, `make check CC=gcc` builds the shipping WASAPI implementation and
runs scripted COM clock checks followed by native shared/exclusive endpoint
checks. The endpoint tests submit only zero-valued PCM. They require a device
supporting a 48000 Hz stream and exercise both write and direct buffer lends,
actual presentation, partial-period EOF, explicit silent gaps and stop/start.
`make unit-check` runs the scripted clock checks without opening an endpoint.
Cross-compiling with `CC=x86_64-w64-mingw32-gcc` builds the same Windows test.

From an MSVC Native Tools prompt, run `python build-msvc.py --unit` to build
and run the COM checks as C. Add `--check` for native endpoint validation.
The script also defines the Windows `small` and `hyper` macros when compiling.

WASAPI tracking is requested before device construction. It uses
[IAudioClock's stream position and frequency](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclock-getposition),
including the known initial silent prefill. Shared-mode tracking explicitly
fills unused endpoint space with silence. An empty shared endpoint, a cursor
past submitted data, a backwards clock or a native failure invalidates the
timeline. `S_FALSE` withholds the position rather than retrying in a loop.
Ordinary playback without a tracking request retains its existing behavior.

These tests validate the device interface. Receiver scheduling and the
frontend resampling provenance must also use it before claiming synchronized
PyroWave A/V playback.
