# PyroWave session and PCM handoff checks

`make check SANITIZER=address,undefined` exercises the shipping fixed PCM
pool through the shipping stream protocol, with seven-byte partial transfers.
`make all CC=x86_64-w64-mingw32-gcc EXE=.exe` checks Windows compilation;
the resulting executable also runs natively on Windows.

The producer writes into a pool span, the transport borrows it until its
completion callback, and the receiver writes directly into another pool.
A receiver-held buffer remains unchanged while later packets flow through
the remaining slot. The check saturates that pool, drops one complete
record, recovers with a sample-index gap, and drains through END. It also
checks reservation cancellation, retained reservations after invalid input,
overlap/overflow rejection and refusal to free borrowed buffers after EOS.
Aborting a second session partway through a PCM payload retires both the
sender's loan and receiver's reservation exactly once without publication.

The byte-stream callbacks emulate nonblocking I/O; actual IPv4/IPv6 TCP
startup and framing are checked by the common codec sample. This test does
not exercise an audio device or establish hardware-clock A/V synchronization.
Those frontend connections remain separate integration work.

The controller check uses the shipping session controller, stream and PCM
pool with a deterministic fake GPU queue. It checks cold configuration and
PCM compatibility, 29/31-byte fragmented I/O, direct 9,000-byte video loans,
audio progress while the GPU is pending, bounded-pool drops, EOS draining,
partial-send cancellation, failed receiver commits and GPU-pending shutdown.
Released source slots are poisoned so a premature release corrupts the
observed payload and fails the test. Cold device-loss recovery explicitly
forgets the pending controller loan before abandoning the backend.

Each real compute adapter exports a typed session binding. The controller
never records GPU work or polls its fences: the render owner does that and
retains decoded images through display completion. This portable check is
not native GPU execution or hardware-clock A/V synchronization.

The controller executable also checks the audio-owner presentation map:
accepted output slices retain their original resampling ratio, cursor gaps
do not extrapolate, backwards cursors invalidate an epoch, and full mapping
capacity applies backpressure. A 441-to-480 frame block split into seven-frame
device writes preserves its exact final sample index. Device-write hooks and
video scheduling still need to supply and consume this provenance.

The connection checks run real IPv4 and IPv6 loopback TCP through the
shipping endpoint, connection and session controllers. They use readiness
events for startup, handshake, video/PCM transfer and EOS, and verify that
idle sessions do not keep requesting writable notifications. Cancelling a
connection closes its socket while retaining a GPU-pending loan. An injected
startup exception closes the endpoint and remains a terminal error. The GPU
queue in these checks is deterministic; native codec execution is tested
separately by the compute samples.

`pyrowave_dispatch_test` adds real I/O workers: TCP video/PCM drain, a finish
request made before negotiation completes, producer wake after pending GPU
completion, cancellation and explicit cold abandonment, and receiver-side
accept/reject commands. Atomic publication protects config and decisions.
The Windows worker rechecks socket readiness after arming its event; without
that check, re-registration can strand a writable socket. Linux uses poll
with a nonblocking producer-wake pipe. Production producer methods never
wait; short delays in the test are collection watchdogs only. The dispatcher
also supports source and receiver listeners. The immutable bound-address
snapshot reports the assigned port before the worker starts accepting.

The desktop OpenGL, Vulkan and Windows D3D12 frontend recorder is registered as `pyrowave` when built
with `--enable-pyrowave`, threads and configuration-file support. Select it
with `record_driver = "pyrowave"`; use glcore, Vulkan or D3D12 video, disable game-only capture,
and supply `record/drivers/pyrowave.cfg.example` with an absolute shader path
as the recording/streaming config. `rpws://127.0.0.1:port` connects to a peer;
`rpws-listen://127.0.0.1:port` accepts one peer. IPv6 addresses use brackets.
Addresses are numeric so cold startup does not perform DNS. The existing
record/stop controls and custom streaming URL select this path. It captures
the final viewport, appending compute to the same frame list. D3D12 borrows
the list's timeline fence. Vulkan uses a preallocated private fence, which
survives renderer frame-fence recycling. Its required copy from an unsampled
swapchain image stays on GPU. Neither path introduces raw pixel readback.

Generate Vulkan modules outside the source tree before starting:

```
python tools/pyrowave_shaders.py --output /path/to/assets/rpyrowave_shaders.h \
  --export-spirv /path/to/assets
```

Point `pyrowave_shader_directory` at that assets directory. D3D12 instead
loads the independent HLSL sources and compiles them during cold setup.
For desktop OpenGL add `--export-glsl /path/to/assets` to the generator and
select `glcore`. It requires GL 4.3 compute and persistent coherent buffer
storage. Context-owned asynchronous jobs compile shaders, create resources,
and retire them after I/O and GPU loans end. The default framebuffer's copy
stays on GPU; source pixels never enter a CPU buffer. The frontend restores
the GL bindings changed by capture and switches shared core contexts only
on their owner. Downlevel GL and GLES frontend capture are not enabled.

`make frontend-fixtures EXE=.exe` builds a synthetic libretro core and CPU
decoder DLL for an independent native application oracle. On Windows, run:

```
python frontend_oracle.py --retroarch /path/to/retroarch.exe \
  --core /path/to/frontend_core.dll --codec /path/to/frontend_codec.dll \
  --output /path/to/test-output
```

Select `--backend vulkan --shader-directory /path/to/assets` for Vulkan.
On Linux use the `.so` fixtures, a Vulkan-capable X11 display, and a native
Linux RetroArch executable; CI provides Xvfb and llvmpipe. Add
`--require-validation` when synchronization validation is configured: the
oracle verifies that the layer confirmed it active and rejects errors.
Repeat with `--threaded`; add `--disconnect-after 12` to check peer-abort
retirement and clean frontend exit. This launches an occluded, task-owned RetroArch,
captures its actual GPU codec output over TCP, and independently checks
decoded center luma, PCM sample values, rational timestamps and END. The
process must also exit cleanly. The fixture's short delay only gives this
offline, occluded-window test progress without relying on display pacing;
production code has no such delay. The test deliberately uses a null audio
device; it does not establish hardware-clock synchronization. Native
device-write provenance and audio-master presentation are covered separately
below.

Select `--backend glcore` with the generated assets to check actual OpenGL
capture. A `make GL_DEBUG=1` build plus `--require-gl-debug` requires an
active debug callback and rejects GL errors. `frontend_cancel.py` takes
the same executable, core, backend, shader directory and output options;
it stops before any peer connects, after two and four frames. Repeat with
`--threaded`; `--missing-shaders` also checks failed asynchronous GL setup.

`frontend_fault.py --build-directory /path/to/configured/build --retroarch
/path/to/configured/build/retroarch --output /path/to/fault/retroarch` builds
an offline failure fixture, then restores and rebuilds the original renderer
in `finally`. It simulates command-buffer finalization failing after 31
captures. Run the oracle on that fixture with `--expect-abort`; it requires
TCP closure and resource retirement instead of END. The source CI job runs
this check with synchronization validation, in addition to normal capture.

The embedded frontend receiver opens a connection file with
`retroarch --pyrowave-receive /path/to/stream.rpws`. It does not load a
PyroWave core. An already selected game core stays loaded and resumes after
stream playback ends; receiving temporarily owns frontend audio/video.
Use `-L /path/to/game_core` and normal content arguments alongside
`--pyrowave-receive` to retain that game session. No PyroWave core is built
or registered. Sending uses the recording driver while the game runs.

A minimal connection file is:

```
pyrowave_url = "rpws://127.0.0.1:12345"
pyrowave_shader_directory = "/absolute/path/to/generated/assets"
```

Use `glcore` with desktop compute support, `vulkan`, or `d3d12` on Windows, plus
threads and configuration-file support. GL loads generated `.comp` assets;
D3D12 uses the independent HLSL directory and compiles it during cold setup.
Vulkan loads generated `.spv` assets on its existing graphics/compute queue.
The current frontend receiver accepts SDR video and zero, one or two PCM
channels. It borrows native decoder textures across repeated draws and retires
the last display use before reusing a slot. Reset reconnects after retiring
the old connection and its GPU resources. Unload/quit cancels negotiation or
playback; EOF drains accepted video and PCM before returning to the frontend.
Sender and receiver cannot run together in one frontend instance.

`receiver_oracle.py` requires Pillow and a `GL_DEBUG=1` application. It acts as
an independent CPU-encoded TCP sender and compares a screenshot of the actual
receiver against CPU-decoded RGB, including orientation and channel order:

```
python receiver_oracle.py --retroarch /path/to/retroarch \
  --codec /path/to/frontend_codec.so --shader-directory /path/to/assets \
  --output /path/to/receiver-test --threaded
```

Use the DLL fixture and `.exe` on Windows. Modes `end`, `abort`, `early`, and
`missing-shaders` require resource retirement; EOF and setup failure must also
request frontend shutdown. `--audio-channels 0|1|2` covers audio admission and
drain. `--mode reconnect` uses the existing command interface bound only to
loopback, requests Reset, requires two ready/retired sessions, and verifies a
changed image from the replacement session. Tests close only their own
application after normal return to the frontend. The default null-audio mode
establishes delivery and lifecycle; select a native driver as described below
for presentation-clock checks.

Select `--backend d3d12` for the native Windows receiver oracle. Its descriptor
views are registered cold in the renderer's existing native-texture cache.
The normal native-input path samples decoder images directly, and makes GPU
copies where shader history requires separate lasting frames. Returning a
held image registers its actual final-use timeline label, including a label
that already completed while the image was held across duplicate draws.

`--history-shader` checks actual shader execution with a visible channel
permutation, source-dimension check and one prior frame. Both desktop GL and
D3D12 pass the RGB oracle with this fixture. The regular input and history
paths remain responsible for preset textures, rather than a CPU pixel path.

For native D3D12 application validation, use a task-owned configured Windows
build (a MinGW cross build on Linux is also suitable):

```
python frontend_d3d12_debug.py --build-directory /path/to/build \
  --retroarch /path/to/build/retroarch.exe --output /path/to/debug/retroarch.exe
```

The script temporarily enables the native debug layer and GPU-based
validation and collects error/corruption messages during cold device teardown.
It restores and rebuilds the original source in `finally`. Run the receiver
oracle on the resulting Windows executable with `--backend d3d12
--require-d3d12-validation`; the test requires confirmed GPU validation and
zero collected errors, in addition to pixels and resource retirement.

`frontend_d3d12_debug.py --omit-receiver-transition` creates a negative fixture
for checking the validation harness. The oracle must reject it for native
resource-state errors; the ordinary fixture must pass. This mutation is
confined to the offline executable and restored configured build.

Select `--backend vulkan --require-validation` for actual Vulkan receiver
checks, with the same layer settings as the source oracle. The ordinary
filter chain samples the decoder's GENERAL-layout image and makes its own
GPU history copies where required. `frontend_fault.py --receiver` records
eight actual decoder packets before failing command-buffer finalization;
run that executable with `--mode gpu-failure`. This requires the fault marker,
resource retirement and clean exit. Removing the unsent decoder discard
leaves a borrowed fence unacknowledged and fails the exit watchdog.

The receiver oracle's `--hdr` option sends PQ/BT.2020 and compares the actual
SDR screenshot against CPU transfer, gamut and peak-normalization math. It
also composes with `--history-shader` and each renderer selection. The
receiver uses the explicit SDR709 decode mode for these streams; native
HDR swapchain presentation remains separate work. Raw decoded pixels stay
on the GPU.

`frontend_fault.py --receiver --retain-unsubmitted` also suppresses the
failed primary's immediate reset. The receiver must retain its native loans
until a cold queue retirement marker completes, then reset its primaries.
The fixture holds seven marker polls; `receiver_oracle.py --mode gpu-failure
--require-retirement-marker` checks acknowledgement before resource teardown.
This is a healthy native GPU cancellation/recovery test, not a physical TDR.

Adding `--report-marker-loss` reports device loss after the real retirement
submission succeeds. The marker must still be acknowledged before teardown;
this exercises the loss-report branch without resetting the physical GPU.

D3D12 recovery fixtures use `frontend_d3d12_fault.py --stage close` or
`--stage signal` with the same configured-build/output arguments as the
other fixtures. They fail after eight recorded decodes, hold actual queue
work behind a test-only fence, and release it after seven retirement polls.
Run their outputs with `receiver_oracle.py --backend d3d12 --mode gpu-failure
--require-d3d12-validation --require-retirement-marker`. The negative
`--skip-retirement-ack` fixture must fail that oracle.

`--stage removed` calls Device5::RemoveDevice on the fixture's own logical
device while reporting an execution error. `--stage removed-queued` removes
it after successful execution and signaling, leaving detection to the
renderer rather than the error callback. Run either with
`--require-d3d12-device-removal` instead of
`--require-retirement-marker`. This checks real runtime removal and driver
reinitialization without inducing a physical GPU reset. Its test-only C ABI
binding also works with the renderer's older SDK headers. With a modern
Windows SDK, `make device5-abi-check EXE=.exe` compares the binding's GUID
and method offsets against that SDK. An older SDK only supports compiling
the binding; the comparison executable returns 77 and says it is unavailable.
Native MSVC x86/x64 and MinGW x64 SDK comparisons pass.


## Native audio and presentation-clock checks

The receiver accepts `--audio-driver wasapi` on Windows or `--audio-driver
alsa` on Linux. It uses the existing frontend resampler and audio worker,
records only frames actually accepted by the device, and retires the binding
after device presentation drains. Unsupported native presentation interfaces
fail admission. Default null-audio checks do not establish this contract.
The tests set a low output volume. Physical Linux audio execution requires
an available ALSA endpoint; the scripted null PCM checks exercise its queue
and recovery contract without claiming physical device validation.

Build `frontend_av_clock.py` with the configured-build and distinct-output
arguments shown above; add `--d3d12-validation` on Windows for native D3D12
GPU validation. Run its output with `receiver_oracle.py --audio-driver wasapi
--mode end --require-av-clock --av-video-lead-us 3000000`, selecting the
appropriate renderer and shader directory. The trace records to bounded
storage on the renderer and prints only after quiescence. It requires a
future-frame hold and checks that every promoted frame is due. The
`--present-future` negative fixture must be rejected. This tests selection
against native audio presentation, not physical display or acoustic latency.

`frontend_gl_fault.py` suppresses the completion marker after eight actual
decodes; `--sender` instead faults the thirty-second capture. Run receiver
outputs with `--mode gpu-failure` and sender outputs with
`frontend_oracle.py --expect-abort --require-gl-debug`. Both paths must prove
GPU retirement before destroying resources, without a GL finish or wait.

`frontend_d3d12_fault.py --sender` applies each native Close, Signal and
logical-removal fixture to source capture. The healthy cases hold real queue
work behind a gate until the ordered retirement marker can complete. Source
recovery also retires renderer loans that already named the never-executed
list's next fence value, without reusing that value. Sender validation rejects
native fence timeouts as well as GPU validation errors. The default
`frontend_fault.py --retain-unsubmitted` exercises the Vulkan sender's
corresponding cold recovery path; adding `--receiver` selects the decoder.

Embedded LAN streaming from the menu
----------------------------------

Build with `--enable-pyrowave` and use glcore (OpenGL 4.3+), Vulkan, or Windows
D3D12. Shader data is embedded in the executable. Both clients work without
an external shader folder, shader download or shader compiler installation.
Normal builds verify and embed the committed precompiled files using Python;
glslang, SPIRV-Cross, FXC and DXC are only needed to regenerate those files.
Vulkan uses SPIR-V; D3D12 selects DXIL on Shader Model 6 devices and DXBC on
older devices. OpenGL uses bundled GLSL compiled by the graphics driver.
The advanced PyroWave Shader Directory setting remains a strict development
override; leave it empty for the built-in bundle. Manual recording configs
can omit pyrowave_shader_directory too. `make pyrowave-assets` only exports
development/sample assets and is not needed to distribute the frontend.

On each device, enable Settings > Recording > PyroWave LAN Discovery (off by
default). Optionally set PyroWave Device Name. In the sender's Main Menu or Quick Menu >
PyroWave Devices, select Send to followed by the receiver's name. The receiver
shows a notification; Settings > Recording (also reached through Quick Menu >
PyroWave Devices) offers Accept Incoming PyroWave Stream and Decline Incoming
PyroWave Stream. The existing game keeps running until acceptance. Stop
PyroWave Streaming restores the loaded game without unloading its core.
Sending and receiving both work without a game loaded. Sending uses PyroWave
regardless of the ordinary recording driver.

Stream Entire RetroArch is enabled by default in Settings > Recording. It captures
the composed output, including menus, notifications and the built-in media player,
and audio after the frontend mixer. A stream survives loading content, returning
to the main menu and rebuilding or resizing the video driver. The receiver accepts
once; these transitions do not start a new connection or require another core.
Wire dimensions and frame rate stay fixed for the session; GPU scaling fits the
new source dimensions into that frame. Changing HDR/output color mode requires a
new stream. With audio disabled or the null audio driver, the stream is video-only.
Disable Stream Entire RetroArch before starting to use the content viewport and
core audio instead. This viewport capture can still include UI drawn over content.
The persisted recording driver, streaming URL and config selection are retained.

Discovery uses one primary IPv4 LAN interface and `_pyrowave._tcp.local.`.
Only opted-in compatible receivers with a supported backend and a free listener are shown.
Allow local UDP 5353 and the advertised ephemeral TCP receiver port through
the firewall. Networks that isolate clients or block multicast need the existing
manual connection-file workflow. Discovery is unauthenticated; accept only an
expected request. A request does not start audio/video playback without consent.

`lan_menu_oracle.py` launches two real frontends and drives their existing MENU
command interface, bound only to loopback. It enables discovery through the
menu, discovers the peer without an address, declines one request, reconnects,
accepts another, then stops it and proves the selected core was never unloaded.
It deliberately configures the ordinary recording driver as null to exercise
the transient sender override (`--record-driver nvenc` also proves an unavailable
ordinary encoder does not interfere). It verifies that disabling discovery
withdraws the receiver from the peer menu. Run it with `--retroarch`, `--core`,
`--embedded-shaders` (or an explicit `--shader-directory`), and a distinct `--output`; optional `--threaded`,
`--backend glcore|vulkan|d3d12`, and `--audio-driver wasapi` cover presentation
variants. Linux CI runs direct and threaded GL menus; Windows can run the same
oracle natively with D3D12. Native Metal remains a separate macOS handover check.

`lan_menu_oracle.py --frontend-only --transition --pixels --resize --media FILE`
checks received menu pixels, core load/unload, fullscreen resizing with fixed wire
dimensions, and moving movie color bars from the built-in player. Use a short
320x180 VP9/AAC MP4 fixture, for example one generated by FFmpeg testsrc2 and sine.
`frontend_oracle.py --full-output --audio-driver alsa --audio-device null` checks
post-mixer PCM and clean manual-stream shutdown on Linux; use WASAPI on Windows.
The video-link test holds an I/O packet across pause, refuses premature backend
replacement and verifies monotonic serials after the backend's counter restarts.

Use `--embedded-shaders` with the LAN/menu or frontend TCP oracle to test the
bundled path. The LAN oracle sets an absent assets directory to prevent an
installed asset folder from hiding a missing executable bundle. Its unit shader
check proves default lookup borrows static aligned bytecode without file I/O,
while explicit overrides still load bounded owned data and fail when missing.
