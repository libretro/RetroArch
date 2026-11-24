# Clean-room provenance

Current platform scope (20 November 2026): Linux/Windows sender and receiver
integration uses desktop GL compute, Vulkan and D3D12, with ALSA/WASAPI native
presentation clocks. Reusable Metal compute adapters and their C89 runtime
bridge are included; native Apple execution and frontend/CoreAudio routing
are the explicitly deferred Mac handover. See `BACKENDS.md` and
`samples/gfx/pyrowave_metal/README.md` for current limits and next steps.
The entries below are a chronological engineering record. Earlier statements
that a feature remains unfinished are superseded by later execution evidence.


The rpyrowave implementation is independently authored C89. No PyroWave
implementation source or shader has been read, copied, translated or used
as an implementation template.

Permitted sources consulted on 19 November 2026:

- The author's descriptive article:
  https://themaister.net/blog/2025/06/16/i-designed-my-own-ridiculously-fast-game-streaming-video-codec-pyrowave/
- The formal v1 bitstream and PWV1 file specification, last modified by
  commit `03391df4be12528426e205ac90c5911ff172e5c3`:
  https://github.com/Themaister/pyrowave/blob/03391df4be12528426e205ac90c5911ff172e5c3/bitstream/bitstream.md
  SHA-256 of the downloaded Markdown:
  `d8a53b090c6e8745f71035c09f5860190eefbe0b69d965d31f36ecfa1c713b12`.
- The project's descriptive README, including documented standalone CLI
  usage, not the referenced test or build implementation files:
  https://github.com/Themaister/pyrowave/blob/master/README.md
- The Polaris/Nova authors' implementer documentation (transport research):
  https://papi-ux.com/docs/pyrowave-reference/
- ITU-T T.800 (06/2019), Table F.4, equations F-7 and F-11:
  https://www.itu.int/rec/T-REC-T.800-201906-S
  PDF SHA-256:
  `9932e967462cd56a193db87117917e74734fb385913bf51d1f52fb0428924e1c`.
  The inverse transform uses the specified 1.230174104914001 normalization
  and whole-sample symmetric extension. Edge and padded-image behavior
  was independently checked against the packaged GPU reference binary.
- The public C ABI declarations and documentation in the project's
  `pyrowave.h`, for the isolated reference harness only. The header was
  checked for function bodies before display. No function definitions or
  implementation files were consulted. Interface declarations are used
  as an ABI specification; the shipped codec does not include this header
  or link to the reference DLL.

The formal specification contains normative C/GLSL layout and decoding
notation. These are format specifications, not separately consulted
implementation files. The C code here uses explicit byte access rather
than native bitfield layouts. Tests are independently constructed from
the formal wire definition; self-consistency does not establish reference
implementation interoperability.

The v1 document describes its freeze as 3 October 2026, and says that the
reference implementation is authoritative if the document disagrees.
Such disagreement must be investigated with independent reference vectors
or black-box binary observations; reading implementation source is outside
this task's clean-room boundary.

## Evidence status

Packet, coefficient encoder/decoder, file-header and layout vectors passed
on native MSVC 19.44, native MinGW GCC 16.2 and Linux GCC with ASan/UBSan.
The measured optimized MinGW decoder frame is 240 bytes; the coefficient
encoder is 352 bytes, against the 2 KiB libretro-common ceiling.
Reference GPU encoder to independent CPU decoder: verified with the
black-box vectors below. Independent CPU encoder to reference GPU decoder:
verified below.
Independent GPU kernels and asynchronous adapters: native Windows GL,
Vulkan and D3D12 verified below, plus Linux software-device execution.
Portable transport/session ownership checks pass. Metal has contract and
forced-native-branch compile coverage; native Apple validation is deferred
until a Mac is available. Frontend controls and hardware-clock A/V remain.

The codec specification leaves presentation timing and session protocols
to external mechanisms. PWV1 is a file interchange format and does not
define an interoperable streaming session, audio transport or negotiation.

## Isolated reference binary

The Pyrolight v6.2.11 Windows portable release was downloaded from
https://github.com/karsyboy/Pyrolight/releases/tag/v6.2.11
for future black-box testing. Its ZIP SHA-256 matches the release's
SHA256SUMS:
`7366e0582dcf1e7b2109ea52512392d84001b96ae862957cc83cfad51c31e9c1`.
The bundled `libpyrowave-shared-1.dll` SHA-256 is
`17b0160e84898fbad056b314afac9bee37d91122dc0e9f21c456481fb62e00d1`.
The reference reports C ABI 1.1.0. It was loaded through its public ABI,
created its default Vulkan device on the native RTX 5090, and encoded and
decoded six synthetic patterns (zero, mid gray, white, ramp, corner impulse
and checkerboard), each at 128x128 4:4:4, 130x98 4:2:0, 1x1 4:4:4,
2x2 4:2:0 and 321x181 4:4:4. Each encode used a 262144-byte budget,
8192-byte packet boundary and planar 8-bit YCbCr input. Reference decode
used planar 16-bit output and the compute path. This binary is not a
runtime dependency or part of the shipped implementation.

Every output sample of the independent CPU decoder was compared with
the same-bitstream reference decode, over all 30 vectors. The largest
normalized absolute difference was 0.001069244; the acceptance bound was
0.003. This is a floating-point reconstruction comparison, not a bit-exact
claim. The specification permits different floating-point precision.

Four reference bitstreams and their reference-decoded little-endian
16-bit Y/Cb/Cr planes were retained outside the repository for optional
reference checks. Their capture manifest records exact byte hashes.
They are generated data, not implementation code, and are not shipped.
The ramp is `(3*x + 5*y + 37*component) mod 256`.
The corner impulse is 255 at (0,0) and 128 elsewhere in every component.
The six-frame generation order is zero, mid, white, ramp, impulse, checker.
The optional `reference_vectors.py` harness regenerates the inputs and
calls only the reference DLL's public ABI. For example, supply the DLL
with `--dll`, an isolated directory with `--output`, and
`--width 130 --height 98 --chroma 420`. Repeated native encodes produced
identical used bits and identical reference-decoded output. Unused sign
bits and alignment bytes can differ between runs; the fixture manifest
hashes identify the particular retained byte sequences, not an assertion
that the reference initializes padding deterministically.
The sample also checks source-image error with a separate, looser bound
because the reference codec is lossy; reference decoder agreement uses
the tighter 0.003 bound.

The image decoder and saved vectors passed native MSVC `/TC /W4 /WX`,
MinGW strict C89 with SDK macro collision defines, and Linux ASan/UBSan.
The standalone MSVC harness uses `_CRT_SECURE_NO_WARNINGS` for standard
C file I/O. Its optimized MinGW image-decode frame is 272 bytes.
As a sensitivity check, a temporary build with every inverse lifting
subtraction replaced by addition failed 382181 checks; the unmodified
source passed. The mutated source is not part of the implementation.

## Independent image encoder

The CPU encoder uses the forward lifting equations from T.800 F-11,
clamps padding to the right/bottom edge, and independently chooses a
global deadzone quantizer by exact packet-size search. The output writes
straight into the caller's compressed buffer. It does not reproduce the
reference encoder's rate-distortion policy. Both directions use the
formal PyroWave v1 bitstream.

An initial reverse test failed at quantizer code 233. A separate black-box
matrix tested 15 quantizer codes and nine coefficient magnitudes, including
18-bit magnitudes. At very small dequantization factors, the packaged
reference produced large discrepancies. The independent encoder therefore
limits its finest coefficient step to 2^-21, where all eight mantissas
are exactly representable as FP16 subnormal values. The independent
decoder continues to accept all quantizer codes in the formal format.
This encoder limit is an interoperability choice, not a format change.

After that change, 150 independent frames passed native reference decode:
the same five image layouts and six patterns as above, at budgets of 8,
512, 4096, 32768 and 262144 bytes. Every frame stayed within its budget.
The native MSVC build and native MinGW build were each used as the
independent codec DLL. The maximum normalized difference between the
independent decoder and reference decoder was 0.001106952, below 0.003.
The optional `reference_reverse.py` harness records every case and hash.
It takes `--reference-dll`, `--codec-dll` and `--output` arguments.

Native MSVC, MinGW strict C89 and Linux ASan/UBSan also passed the image
encoder's regression tests for byte budgets, short-buffer transactionality,
metadata, high-budget image error and invalid input. Decoding now writes
coefficients directly into the wavelet atlas, removing the 4 KiB staging
block and its per-block copy. The largest measured optimized MinGW frame
among the image codec functions is 304 bytes.

## Independent compute programs

`shaders/dwt.hlsl` is independently authored from the same formal lifting
equations. It uses ordinary FP32 operations and buffer accesses, with no
subgroup or 8-bit-storage requirement. It has compiled to Shader Model 5.0
DXBC with FXC, and to Vulkan SPIR-V with glslang; SPIR-V validation passed.
SPIRV-Cross generated desktop GLSL 4.30 and ES GLSL 3.10 from this independently
authored program; ES GLSL validation passed. Native DWT execution is tested
as described below. Complete GPU codec execution is tested below.

The shared C89 DWT recorder emits 50 dependent dispatches without allocating,
waiting or reading pixels. Callers own resource lifetime and completion.
Two GPU atlas buffers alternate, with buffer 0 retaining the final result.
The offline validation programs use bounded fence waits and raw readback;
these operations are not part of the production codec/frame path.

All four retained reference vectors passed the native MSVC OpenGL 4.3,
D3D12 and Vulkan harnesses on the RTX 5090, NVIDIA driver 621.01. The maximum
normalized error against reference inverse reconstruction was 0.001069065.
The maximum GPU forward/inverse round-trip error was 0.000000626.
D3D12 debug validation was enabled. Vulkan ran with the Khronos validation
layer explicitly required, with no reported errors. The task-local layer
DLL SHA-256 is
`e7bfc5abeccec3db932212ec13be2c873556d62e969a771d8a71067c7559eb01`.
No system loader or layer settings were changed. SDK headers are treated
as external headers for warning purposes; independently authored C89 source
is compiled with warnings as errors.

A deliberately incorrect inverse-lifting shader was rejected on all three
native backends, then the correct shader passed all 12 backend/vector cases.
Linux strict C89 builds also exercised GL through EGL and Vulkan through
the llvmpipe software device, with Vulkan validation required. These are
software API/portability checks and are not counted as physical GPU tests.

## GPU packet coding

`shaders/packet.hlsl` is independently authored from the formal v1 wire
definition and the independently authored C codec. Its 128-thread groups
perform rate analysis, exact byte-budget quantizer search, packet prefix
layout, bit-plane/sign packing, and bounds-checked unpacking. It uses four
storage-buffer bindings, one uniform block, FP32 and 32-bit atomics; no
subgroup or 8-bit storage is required. Shader Model 5 DXBC and Vulkan SPIR-V
compile, SPIR-V validation and ES GLSL 3.10 translation/validation pass.
The packet programs execute natively on OpenGL, D3D12 and Vulkan.

The C89 helpers construct immutable atlas block descriptors at setup and
record packet operations without allocation or GPU waits. Large dispatches
use a two-dimensional grid bounded to 65535 groups per axis. The receiver
indexes compressed headers in the caller-owned upload span in place;
internal magnitude/sign bounds are checked on GPU. Resource completion and
borrow retirement remain the backend owner's responsibility.

The MSVC C89 OpenGL codec harness ran on the native RTX 5090. It first
overwrote the coefficient output with poison, then GPU-unpacked all four
saved reference bitstreams and reconstructed their reference images.
For each image it kept the source on GPU, performed forward DWT and GPU
packet coding at the five budgets above, and tested a fixed quantizer too.
All 24 frames stayed within budget. GPU unpack/reconstruction agreed with
the independent CPU decoder within 0.000000864 normalized units. The
external reference DLL decoded every generated frame; its largest
difference from independent CPU reconstruction was 0.001098119, below 0.003.
`reference_decode_files.py` reproduces the external decoder comparison.

Each image also exercised malformed magnitude and missing-sign payloads
that pass header indexing. The GPU rejected all eight corrupted cases.
Linux EGL/llvmpipe exercised the same complete kernel sequence and fault
cases as software API coverage. Raw atlas readback and bounded waits in
these offline harnesses are numerical oracles, not production frame-path
operations. The command API leaves compressed output in caller-owned
storage; production adapters must retain it until asynchronous completion
and consumer release. Those adapters are not yet connected to RetroArch.

MSVC, MinGW strict C89 and Linux ASan/UBSan passed the shared layout,
header-indexing, duplicate/loss, command-abort, budget-bound and large-grid
regression tests. The largest optimized MinGW packet-control stack frame
measured 240 bytes.

The MSVC D3D12 and Vulkan offline harnesses also passed all four reference
vectors, six encoding profiles per vector and eight malformed-body checks
per backend. The external decoder accepted all 24 generated frames per
backend, with maximum normalized difference 0.001098119. D3D12 debug and
required Vulkan validation reported no errors. MinGW strict C89 builds
passed a native six-profile/fault run on each backend. Linux Vulkan ran the
complete sequence with validation on llvmpipe, as software coverage only.

All three native MSVC backends additionally passed a 1920x1080 4:2:0 ramp,
six encoding profiles and two malformed bodies. The external decoder
accepted all 18 generated frames, with maximum normalized difference
0.001164575. These tests keep the source and rate-control work on GPU;
their final raw readbacks and fence waits remain offline validation only.

## GPU RGB conversion

`shaders/color.hlsl` is independently authored from the non-constant-
luminance equations and quantization definitions in
[ITU-R BT.709-6](https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.709-6-201506-I%21%21PDF-E.pdf)
and the BT.2020 matrix constants in the
[Khronos Data Format specification](https://registry.khronos.org/DataFormat/specs/1.4/dataformat.1.4.inline.html).
It converts RGB texture values to the shifted codec atlas and back. The
default mode retains nonlinear source gamut/transfer, including PQ code
values; scRGB mode fuses gamut/transfer conversion into the same stage.
The C89 setup helper
checks dimensions and texture crop bounds without allocating resources.

The native OpenGL MSVC and MinGW harnesses cover 256 code-value configurations:
four geometries including 1x1 and 2x2, 4:4:4/4:2:0, 8/10/12/16-bit
reference scaling, BT.709/BT.2020 matrices, full/limited range, centred/left
chroma, vertical reversal and an offset crop. An independent double-
precision calculation checks the padded atlas and reconstructed RGB;
sentinels check that output stays inside its crop. A deliberately wrong
matrix fails the numerical check. Linux EGL/llvmpipe also passes as software
coverage. FXC cs_5_0, Vulkan SPIR-V, SPIR-V validation and ES GLSL 3.10
compilation pass. Native D3D12/Vulkan texture conversion also passes the
adapter tests below. Production renderer connections remain unfinished.

## Asynchronous OpenGL encoder ownership

`record/record_pyrowave_gl.c` owns a bounded compressed-output pool and
records color conversion, forward DWT, GPU packet coding and compressed
staging on the caller's GL queue. Atlas buffers and rate-control work stay
on GPU. Constants, buffers and persistent coherent read mappings are
created at cold setup. The producer queries each outstanding fence once
with zero timeout and publishes completion through `retro_atomic.h`;
`record_gpu_queue` transfers descriptors and recycles slots after consumer
release. The consumer reads the mapped compressed span directly, without
calling GL or copying the packet. Source retirement follows GPU completion.

The native MSVC and MinGW harnesses passed 40 RGB-texture encodes each,
including source server dependencies, sequence wrap, byte-budget and image
checks, packet BUSY before publication, full-pool backpressure, retained
borrow integrity across subsequent encodes, double-release refusal, EOS
while GPU work is pending, and teardown refusal with outstanding ownership.
An injected fence-creation failure exercises destruction of the GL context
and cancellation of queued/borrowed packets. A premature-publication
mutation fails the ownership check. API spies reject frame-path buffer
allocation/remapping and nonzero fence waits. Bounded waits and raw reads
remain confined to the numerical test oracle.

The Linux EGL/llvmpipe run also passes with ASan/UBSan and leak detection
enabled. This is software coverage. The GLES branch compiles as strict C89
against GLES 3 headers; its three ES 3.1 compute functions are resolved at
runtime. Receiver and session bindings are described below. The adapter is
not yet called by RetroArch's renderers.

One 130x98 4:2:0 frame from the asynchronous RGB-texture capture path also
passed external native reference decoding. Its maximum normalized
difference from the independent CPU reconstruction was 0.000741754.

## Build-time shader assets

`tools/pyrowave_shaders.py` compiles the independent HLSL into validated
Vulkan 1.0 SPIR-V, generates desktop GL 4.30/ES 3.10 sources and validates
both GLSL dialects. It also translates all three kernels to Metal 2.1 MSL
using explicit SPIR-V resource binding numbers. Generated C89 headers
contain source/SPIR-V arrays behind per-language include switches; MSVC
and strict C89 MinGW compile a probe using all five asset variants.
Repeated generation passes the tool's byte-for-byte `--check` mode.
Generated headers, compiled shaders and test captures are build output
and must not be committed.

The optional `--metal-sdk` switch invokes Apple's Metal compiler through
`xcrun`; it has not run in this Windows environment. MSL generation alone
is not Metal compilation or execution evidence. Metal is a required backend;
its renderer/session integration and native validation remain unfinished.

## Metal C89 host and runtime adapter

`record/record_pyrowave_metal.c` records full GPU encoding and decoding with
immutable parameter banks. Private wavelet atlases and decoded textures stay
on GPU. The I/O producer receives directly into a lent compressed upload
span; header indexing leaves that payload in place. Encoder output uses
compressed-only blits. The receiver publishes an image only after checking
completed GPU status and the packet error flag, and retains its slot through
the final display command buffer. Unsubmitted command buffers have an
explicit discard path. Source retirement never advances past an earlier
unfinished ticket, including when a later buffer is discarded.

`record_pyrowave_metal_runtime.c` is C89 as well. It resolves the Objective-C
runtime's C functions at cold setup, caches selectors and calls Metal through
typed message signatures. It creates no Objective-C classes or shipping
Objective-C source. All objects have explicit retains/releases. Managed
buffers use `didModifyRange:` and GPU `synchronizeResource:`; unified-memory
devices use coherent shared buffers. Serial compute passes bind tracked
resources directly, relying on Metal's documented automatic ordering rather
than requiring optional intrapass barrier support. These choices follow
[Apple's resource synchronization contract](https://developer.apple.com/documentation/metal/resource-synchronization),
[storage-mode guidance](https://developer.apple.com/documentation/metal/setting-resource-storage-modes),
and [runtime calling-convention requirements](https://developer.apple.com/documentation/uikit/managing-functions-and-function-pointers).
The C ABI is a portability choice, not a measured performance improvement.

The portable test uses a typed Metal stand-in, with separate CPU/GPU copies
for managed buffers. MSVC `/TC /W4 /WX`, strict C89 MinGW and Linux ASan/UBSan
pass. Tests check both complete dispatch/binding sequences, immutable borrowed
packets, source retirement, full-pool backpressure, display-held textures,
queue closure, cold construction failures, unretained/foreign command-buffer
rejection, failed encoders/blits and GPU-error handling. Broken source
retirement and premature display-slot release mutations both fail.
The native runtime branch compiles with Linux headers and with an MSVC
`dlfcn` declaration stand-in; neither is Apple SDK or GPU execution evidence.

An Apple-only GPU oracle creates RGB textures, records full encode/decode
work through the shipping C89 adapter, compares compressed reconstruction
to the source and decoded RGB to an independent CPU calculation, and tests
a malformed sign payload. It covers 130x98 and 1920x1080. GPU readback and
bounded waits exist only in this offline oracle. Its source has passed a
Clang Objective-C syntax check with declaration stand-ins, and its smooth
source fixtures have passed an independent CPU quality sanity check.
The macOS workflow invokes the actual Metal compiler and GPU oracle;
these have not run in this Windows environment. Missing hardware reports
`NOT VERIFIED`, never a GPU pass. Actual renderer/streaming integration
and native Apple validation remain.

## Independent session framing

`rpyrowave_stream.c` and `net/rpyrowave_socket.c` implement the independently
designed RPWS protocol described in [STREAM.md](STREAM.md). No upstream
session implementation was consulted. This transport does not claim
Pyrolight or Polaris network compatibility. It lends compressed video and
PCM buffers directly, interleaves audio between video fragments and uses
exact rational audio timestamps. It does not yet provide frontend controls
or hardware-clock audio/video playout.

Native Windows MSVC C89 and Linux ASan/UBSan tests pass an actual TCP
loopback session. Portable tests cover partial transfers, would-block,
an independent literal configuration vector, audio gaps, fragment priority,
full sink pools, cancellation, malformed headers and generation isolation.
Removing generation validation or disabling audio priority makes the
portable tests fail. The default codec tests generate their own inputs;
external captured reconstructions are checked only when an explicit
reference directory is supplied and are not shipped as source artifacts.

The socket endpoint helper starts nonblocking IPv4/IPv6 connections and
single-peer listeners using caller-supplied resolved addresses and readiness
events. Its steps allocate no storage and perform no waits or clock reads.
Native MSVC and Linux sanitizer tests exercise both address families through
the full video/PCM session, plus refusal, occupied ports, cancellation and
failed adoption ownership. Removing the connect-error check or retaining
the transferred descriptor in the endpoint makes the native TCP test fail.

`record_pyrowave_pcm.c` reuses RetroArch's SPSC slot queue for fixed S16LE
buffers. A transport receiver writes into the producer reservation itself;
the downstream reader borrows that same span until release. No PCM payload
staging or copies are added by the pool. Source sample-frame indices remain
64-bit and permit explicit forward gaps. MSVC C89, strict MinGW and Linux
ASan/UBSan pass a partial-transfer protocol test, including saturation,
held-buffer preservation, cancellation, overlap/overflow checks and EOS
ownership. Premature release and accepted-overlap mutations are rejected.
The test has no physical audio device; hardware-clock playout and frontend
audio routing remain unimplemented.

## Asynchronous OpenGL receiver

The GL adapter now has a receive mode: I/O writes compressed bytes directly
into a fixed coherent mapped workspace, indexes headers there and publishes
the slot through the existing SPSC queue. The context owner records unpack,
inverse DWT and RGB conversion with immutable constants. A completion fence
guards publication; an image retains its decode fence for a shared-context
server wait. A final display fence guards slot reuse. Malformed GPU bodies
release their slots without publishing images. No CPU pixel readback,
per-frame buffer allocation, remapping, clock reads or blocking waits occur
in the adapter.

MSVC C89 and MinGW run this receive path on an NVIDIA RTX 5090. At 130x98,
GPU RGB differs from independently computed CPU-decoded RGB by at most
0.000000792. Linux llvmpipe ASan/UBSan passes with maximum 0.000000329;
this is software execution, not another physical GPU. The oracle checks
direct spans, header/VUI rejection, full pools, repeated polling with held
images, borrowed sync handles, display retirement, queued-frame dropping,
EOS and a missing-sign payload rejected on the GPU, followed by recovery.
All eight sequence constant banks and sequence wrap execute. A failed-fence
case preserves decoding, queued and reserved slots until context destruction,
then abandons them without GL calls.
Premature display-slot release and ignored GPU-error mutations both fail
on the native GPU. Offline waits and pixel readback are confined to the
test. The GLES receiver branch compiles, but has no native GLES execution
evidence. RGBA32F output carries code values or scRGB as selected by
texture_mode; frontend integration is still required.

## scRGB texture mode

The color kernel now optionally accepts and produces linear BT.709 scRGB,
using 80 nits per unit, while preserving the default nonlinear code-value
mode. Encoding converts primaries before clipping to the legal sRGB/PQ
code range; decoding applies the EOTF and gamut conversion without clamping
extended scRGB. Conversion is fused into the existing color pass, adding no
pixel staging or per-frame host allocation. The constant layout remains
80 bytes. Both C89 GL and Metal adapters propagate the selected texture mode.

The definitions follow [Microsoft's scRGB interpretation](https://learn.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range),
[ITU-R BT.2100 PQ equations](https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.2100-0-201607-S!!PDF-E.pdf)
and [Khronos's colorimetry and sRGB definitions](https://registry.khronos.org/DataFormat/specs/1.4/dataformat.1.4.html).
No upstream PyroWave implementation was consulted.

Native MSVC/MinGW and Linux sanitizer color tests pass 1,024 configurations
against independently derived double-precision matrices. They include
negative scRGB, HDR values, transfer boundaries and independent VUI flags.
Disabling linear mode, clipping its output, clamping negative input before
gamut conversion or changing the luminance normalization each fails on the
native GPU. The asynchronous GL encode/receive oracle preserves a signed
HDR fixture through actual packets: maximum error is 0.000240326 scRGB on
RTX 5090 and 0.000963211 on llvmpipe. Metal portable binding tests cover the
mode in both constant banks; native Metal numerical execution is unverified.
HLSL Shader Model 5 compilation, Vulkan 1.0 SPIR-V validation and GLSL/GLES/MSL
generation pass. Frontend routing and physical display validation remain.

The independent Vulkan resource adapter uses caller-owned command buffers
and borrowed completion fences; it does not submit or wait. Its encoder
publishes compressed slots and retires source tickets in order. Its receiver
retains decoded RGBA32F images through display completion. Cached mappings
are lent directly; the fallback copies compressed data and metadata only.
Native RTX 5090 MSVC and MinGW tests and Linux llvmpipe ASan/UBSan tests
exercise code-value/scRGB conversion, crop, unsubmitted discard, held packets,
display retirement, allocation rollback, cache fallbacks and 1080p.
Explicit synchronization validation passes on the native GPU. A mutation
removing buffer barrier access masks fails with
SYNC-HAZARD-READ-AFTER-WRITE. Raw pixel readback is confined to the offline
numerical oracle. Frontend Vulkan routing remains unfinished.

The independently authored D3D12 adapter borrows recording command lists
and timeline fence values without executing or waiting. Encoder packets are
lent from cached readback memory. Decoder input is indexed in a cached CPU
pool and copied as compressed bytes plus metadata into upload memory;
pixels remain in GPU resources. Internal resources return to fixed states
within each list, so a never-executed list can be reset and discarded
without a speculative resource-state rollback.

Native RTX 5090 MSVC and MinGW tests pass code-value and signed scRGB
send/receive, cropped input, sequence/ring wrap, immutable packet loans,
source retirement across unsubmitted discards, display fences, header
rejection and 1080p. CPU-decoded YCbCr and GPU RGB are compared against
independent constant fixtures. MSVC GPU-based validation passes. Injecting
the device-removal fence sentinel retains pending borrows until explicit
quiescent abandonment; removing that guard fails the source-retirement
assertion. Linux cross-compilation passes, and native execution built with
Windows SDK 10.0.18362.0 passes. This is adapter validation, not completed
frontend routing or native Linux D3D12 execution.

RetroArch's C89 session controller connects the stream protocol to borrowed
GPU packet/image-upload queues and fixed PCM pools. Typed bindings cover GL,
Vulkan, D3D12 and Metal without SDK objects in the transport owner. Receiver
configuration is accepted only after compatible resources are attached.
The bounded I/O pump allocates nothing, reads no clock and performs no GPU
recording or fence waits. Sender EOS drains closed producer queues before
sending END. Cancellation retains a GPU-pending loan until completion or
explicit cold backend abandonment; failed reservation cancellation also
retains ownership instead of allowing destruction.

Portable MSVC/MinGW and Linux sanitizer tests use a deterministic GPU queue
to check fragmented direct packet loans, PCM progress during GPU backpressure,
pool exhaustion, EOS, partial-send cancellation, failed commits and invalid
receive spans. Source-slot poisoning detects premature retirement, and a
mutation that releases GPU-pending packets fails. Native RTX 5090 D3D12 tests
also run the real session controller and typed binding through handshake,
GPU encode, fragmented in-memory transport, GPU decode/display and EOS for
code-value, signed scRGB and 1080p. MSVC and MinGW GPU-based validation pass.
This does not establish hardware audio-clock synchronization, frontend
controls, or native Metal session execution; those remain unfinished.

The C89 audio-owner presentation map records accepted device-frame slices
against original resampling blocks. Keeping block offsets avoids cumulative
rounding across partial writes. Its bounded preallocated queue refuses to
overwrite unplayed provenance; polling does not extrapolate through unknown
silence or beyond accepted output. A backwards cursor invalidates the epoch
until reset after the old device queue retires. MSVC and Linux sanitizer
checks cover gaps, boundaries, fractional slices, resets, capacity and large
integer products. Removing the slice offset fails the test. Device-write
provenance hooks and video scheduling are still required; this arithmetic
test is not evidence of physical audio/video synchronization.

The C89 connection controller preallocates endpoint and session storage,
then advances startup and streaming on one readiness-driven I/O owner.
Writable interest is requested only for queued protocol work. A completed
lend is followed by a bounded offer of the next queued packet before
returning, so an idle socket does not strand producer backlog. MSVC, MinGW
and Linux sanitizer checks pass real IPv4/IPv6 loopback video/PCM transfers,
EOS, startup exceptions and GPU-pending cancellation. Removing the post-I/O
offer causes the readiness-only test to stall and fail. These checks use a
deterministic GPU queue; frontend controls, native device-write provenance
and physical audio/video synchronization remain unfinished.

The C89 dispatcher owns a dedicated readiness-driven I/O worker while
borrowing caller-owned GPU/PCM resources. Producers notify only after
publication or completion. Config is immutable after release publication;
receiver accept/reject uses a single atomic handoff. Cancellation closes
network I/O but keeps a GPU-pending loan until completion or explicit cold
abandonment. Collection is nonblocking, with a final-access publication for
detached POSIX workers and a zero-time native thread check on Windows.

Native MSVC/MinGW and Linux sanitizer checks exercise worker TCP video/PCM
drain, pre-negotiation finish, producer wake, pending-GPU cancellation,
abandonment and receiver decisions. Linux ASan/UBSan and ThreadSanitizer pass.
The initial Windows worker stalled at
its completion assertion; rechecking level readiness after event arming
corrects the missed notification. Source frame ownership remains separate
from I/O retirement. Connector and listener workers both pass source and
receiver tests, including accept/reject and GPU-pending cancellation.
Device-write provenance and physical A/V remain unfinished.

The first frontend source connection is the Windows D3D12 `pyrowave`
recorder. It uses existing record/custom-stream controls, compiles the
independent kernels during cold setup, appends encoding to the renderer's
frame list and borrows its completion fence. Raw pixels stay on GPU. A
synthetic libretro core in an actual RetroArch process sent 178 frames and
178 PCM records over TCP to an independent receiver. It checked decoded
center luma, PCM values, timestamps and END. A threaded-video run sent 142
frames and 177 PCM records and exited cleanly. Before the retired-snapshot
fix, that threaded run drained END but failed its process-exit watchdog;
the same oracle passes after retiring the render snapshot lend.

The native source constructor compiles as MSVC C89 with warnings as errors.
The full Windows MinGW frontend links. A configured Griffin before/after
syntax comparison reports the same 138 pre-existing errors and no new error
lines; this is a comparison, not a successful full Griffin build.

Vulkan frontend capture now passes actual RetroArch/TCP runs on Windows
RTX 5090 and Linux llvmpipe. Both direct runs sent 179 video frames and 179
PCM records. Threaded Windows and Linux runs, and Linux peer-abort retirement,
also pass. The independent receiver checks Y/Cb/Cr fixture values and exact
PCM samples/timestamps. Synchronization validation was confirmed active in
the application logs, with no reported errors. The dedicated Linux frontend
source CI job builds the application and runs all three cases with Xvfb.
A build with the Vulkan capture hook disabled fails the oracle with zero
video frames, while the restored source passes.

Vulkan retains a GPU-only sampled copy where the swapchain is not sampleable;
no raw pixels are transferred to CPU. Private preallocated fences cover the
primary command buffer before the renderer signals its ordinary frame fence,
avoiding borrow-after-recycle. Output-format metadata uses atomic publication
for cold setup from the controller. Shared source controls remain C89; both
native factories pass MSVC warnings-as-errors and strict MinGW compilation.
The shader generator can export validated SPIR-V without committing binaries.

Fault checks inject a failed, never-submitted primary after 31 valid frames.
The source retires its resources and exits under synchronization validation;
removing the explicit unsubmitted discard fails the exit watchdog. Completion
acknowledgments carry that frame's fence, so later frames cannot acknowledge
an unsubmitted predecessor. A second fault check makes completion become
visible between polls: resetting the fence independently of the adapter's
retired-ticket acknowledgment fails the oracle; waiting for that acknowledgment
passes. The wrapper does not query an encoded frame's fence a second time.

Command-buffer finalization failure and a failed pre-primary VAAPI flush also
close capture admission. A successfully reset, never-submitted primary cancels
its loans; reset failure or device loss retains them for cold abandonment.
A native Windows fault after 31 valid frames at command-buffer finalization
retires resources and exits with synchronization validation enabled. Removing
the cancellation leaves the stream stuck and fails the peer's watchdog.

Desktop OpenGL frontend capture now passes actual native Windows RetroArch
direct and threaded runs over TCP, including exact PCM and independent
Y/Cb/Cr checks. A threaded run with the GL debug callback confirmed active
sent 177 video frames and 178 PCM records with no API errors and clean
resource retirement. Its default-framebuffer copy remains on GPU. Cold
context-owned jobs compile the independently generated GLSL, create the
encoder, and collect its resources without a threaded-video round trip.
Capture restores changed GL bindings; shared core contexts are restored
after cold jobs. The new factory and controller compile as MSVC C89 with
warnings as errors and in a strict MinGW aggregate with SDK macro collisions.

Stopping a listener before READY originally hung the actual application.
Cancelling negotiation on early stop passes direct and threaded checks after
two and four frames; missing-shader setup also retires cleanly. These are
application lifecycle checks, not hardware A/V synchronization evidence.
Desktop GL below the required compute/storage capabilities and GLES frontend
capture are rejected; the reusable native GLES adapter remains separate.

The OpenGL player factory now owns cold shader loading and decoder creation
from negotiated metadata, lends compressed spans to the I/O owner, restores
renderer bindings after decode, and keeps decoded textures borrowed through
final presentation. Its native Windows RTX 5090 and Linux llvmpipe checks
compare RGB against the independent CPU decoder and verify image retirement;
Linux ASan/UBSan also passes. A decoder-dispatch-disabled build fails the
factory oracle. The factory remains C89 under MSVC warnings-as-errors; its
test links existing VFS helpers using their normal compiler dialect.

The builtin desktop GL receiver now passes actual native Windows RTX 5090
and Linux llvmpipe application tests in direct and threaded modes. Independent
CPU packets feed the TCP receiver; a screenshot compared at 25 RGB positions
has maximum error below 0.0021. The initial decoder texture without a
level-zero mip limit displayed black with the frontend's mipmapped sampler;
the pixel oracle rejected it. Limiting the decoder texture to its owned mip
level fixes the actual frontend image, without a CPU pixel fallback.
Stereo, mono and no-audio EOF, interrupted video payload, startup cancellation
and missing shader assets retire resources. Reset retires the old connection,
accepts a new generation and displays its horizontally reversed image; the
oracle requires two ready and two retired sessions. The test command endpoint
binds only loopback. Null audio proves PCM delivery and drain, not hardware
presentation timing. The oracle waits for the frontend's dummy/menu driver
reinitialization before closing its task-owned process, so an exit signal is
not cleared during that transition.

The D3D12 cold receiver factory now loads bounded HLSL assets and creates
native decoder resources from negotiated metadata. The public standalone
factory target repeats the native code-value/scRGB, display retirement, EOS
and 1080p checks on a direct queue. Native RTX 5090 execution passes with
GPU-based validation required; the updated compute CI also cross-compiles the
factory target. This is factory/adapter evidence, not frontend playback.

Actual D3D12 receiver application playback now passes native Windows RTX
5090 direct/threaded pixel checks, mono/stereo/no-audio EOF, peer abort,
early stop, missing shaders, and reset/reconnect. The offline application
fixture confirms D3D12 GPU-based validation and collects zero errors through
resource retirement, including shader-history playback and reconnect. Its
constructor registers descriptor views cold; the renderer's ordinary native
input path samples decoder resources directly, making GPU history copies only
where a preset needs older frames to outlive the decoder's reusable slots.
Both GL and D3D12 pass a visible channel-permuting history shader with checked
source dimensions. Removing the receiver's NON_PIXEL_SHADER_RESOURCE to
COPY_SOURCE transition from the offline debug fixture produces 48 native
state errors and is rejected by the same application oracle. Full Linux
threaded/threadless frontend links and a Windows D3D12-only frontend link pass.
The D3D12-only executable also passes the native receiver RGB oracle without
OpenGL or Vulkan compiled in. Native sender regressions pass on GL, D3D12 and
Vulkan after the receiver integration. These tests use null audio and do not
prove physical A/V.

The Vulkan receiver factory and native adapters pass Linux llvmpipe
ASan/UBSan with synchronization validation, and native RTX 5090 validation.
Tests cover shared decode/display fences, late display return, acknowledgement
before reset, and discard of every loan sharing an unsubmitted fence.
Actual Windows Vulkan receiver direct/threaded RGB, shader history, EOF,
mono/stereo/no-audio drain, peer abort, missing shaders, early stop and
reconnect pass with synchronization validation. Shader history exposed a
write-after-read hazard in the renderer's history-copy destination barrier:
UNDEFINED discards contents but does not remove the execution dependency on
previous shader reads. Adding the graphics/transfer source stages removes
that hazard in the same actual application oracle.

The first finalization fixture could fail before any decode and was too weak
to establish decoder cancellation. The receiver-specific fixture now fails
after eight recorded decodes and requires that marker. The normal application
retires and exits; omitting the native unsubmitted discard fails its exit
watchdog with no resource-retirement marker. Native image/pool ownership is
retained until submission acknowledgement and final display completion.
The updated full Linux source/receiver CI passes the same recorded-decode
fault, and full frontend links pass with GL+Vulkan, threadless GL, and
Vulkan without GL. The renderer's existing configure rules require threads
for Vulkan; the feature matrix preserves that constraint.

PQ/BT.2020 receiver admission now uses the explicit decode-only SDR709 color
mode for SDR presentation. The GPU converts the signalled transfer and gamut
to linear BT.709, peak-normalizes highlights above 80 nits, and encodes sRGB.
The unchanged scRGB mode retains signed HDR for native HDR sinks. The color
oracle passes 1,536 configurations under Linux ASan/UBSan and native MSVC on
RTX 5090. Native actual GL/D3D12/Vulkan direct and threaded shader-history
playback pass an independent CPU PQ/BT.2020 screenshot oracle. D3D12 also
passes shader history, reconnect and EOF with native GPU-based validation
required and zero errors. Bypassing the conversion is rejected by the same screenshot oracle after normal retirement.

Vulkan receiver unacknowledged submissions now retire through a cold queue
marker. Frame submissions pause immediately on the retained failure; after
I/O and frame snapshots quiesce, the marker completes and the renderer resets
its primaries before native abandonment. Native RTX 5090 direct and threaded
reset-failure fixtures pass with synchronization validation, as do Linux
llvmpipe direct/threaded cases. Seven held polls
exercise retention; accepting the first pending result instead produces
VUID-vkDestroyFence-fence-01120 and fails the oracle. This verifies recovery
on a healthy GPU, not a physical device reset or TDR. An offline loss report
after the real marker submit also passes native Windows while still requiring
completion acknowledgement. The first Linux threaded fixture exposed snapshot
starvation: I/O was quiesced, but newly lent frame snapshots kept the context
perpetually in flight. Recovery now stops lending those snapshots before the
cold owner handoff.
The full Linux sender/receiver CI now passes ordinary cancellation, retained
submission and post-submit loss-report cases. GL+Vulkan, threadless GL and
Vulkan-only full frontend links pass. MSVC C89 and the combined host/core
translation-unit probe pass; native Vulkan sender regression still drains
176 video frames and 179 PCM records with synchronization validation.

The [Vulkan lost-device lifetime rules](https://docs.vulkan.org/spec/latest/chapters/devsandqueues.html#devsandqueues-lost-device)
treat a submission's loss result as a submitted operation for lifetime
purposes. The marker path still checks its completion after that result;
it does not equate a failed submit report with completed GPU work.

D3D12 receiver execution now checks Close and the post-execution Signal.
Failures stop new frame loans and pause submissions. Cold setup preallocates
a retirement fence and a replacement command-list/allocator pair. After the
queue marker completes, cleanup repairs any missing frame-fence labels and
replaces the failed list, detaches borrowed descriptors while the codec is
still valid, and abandons native loans before resuming. A failed Close list
is discarded rather than reset, as required by the
[Microsoft command-list contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-reset).

Native RTX 5090 close/signal recovery passes direct and threaded execution
with GPU-based validation required and zero errors. These fixtures hold real
GPU work behind a test fence until seven retirement polls; premature
abandonment fails the same oracle before acknowledgement. Device5::RemoveDevice
also passes direct/threaded native execution, resource retirement and driver
reinitialization with zero validation errors. This removes the test process's
logical device, not the physical GPU. Removal exposed a threaded stale-device
frame: submissions now remain paused through device rebuild. Both Vulkan and
D3D12 keep their submission pause through native teardown, including retries.
Removal after a successful execution and frame signal also passes both native
modes with zero GPU-validation errors. The renderer notifies its borrowed
receiver context when removal is detected and stops before further resource
access if waiting for a previous slot discovers removal. Detach clears that
context borrow before freeing the receiver. Fresh native Vulkan retained-work
and post-submit loss-report fixtures pass both modes after these changes.

The Device5 fixture binding uses the public COM ABI with old SDK headers.
Its GUID and method offsets match native MSVC Win32/Win64 and MinGW Win64 SDK
comparisons, and the binding executes against the native Windows runtime.
The Linux cross SDK lacks Device5 declarations: its build checks compatibility,
while the comparison executable explicitly returns 77 rather than claiming
an SDK comparison. New D3D12 renderer routines compile from their shipping
source in an MSVC C89 /W4 /WX probe; core and aggregate C89 probes also pass.

The audio presentation interface now reports accepted PCM frames and actual
device presentation separately, with queue-loss epochs. Native Windows
shared/exclusive WASAPI tests pass with MinGW and MSVC Win32/Win64 C builds, each
accepting and retiring 105600 frames of zero-valued PCM through write and
direct-lend paths, explicit silence and stop/start. The tests use the shipping
driver and MMDevice implementation against the actual default endpoint.
Scripted COM-clock checks also exercise frequency conversion, known prefill,
silence, S_FALSE, backwards positions and unaccounted missed periods. ALSA
partial-write, invalid-delay, XRUN/suspend epoch, drain and removal contracts
pass under ASan/UBSan against a real null PCM with scripted reports; this is
not physical Linux hardware validation. Audio thread-handshake and ALSA
lifecycle regressions pass under the same sanitizers. Frontend resampling
provenance and receiver clock binding remain to be integrated.
The full Linux compute job, including the new presentation checks and Windows
cross build, passes. All three frontend matrix variants link from clean
objects, including threadless GL. The native Windows frontend also links.
The fixed-capacity resampling provenance tracker now preserves source-block
ratios across short writes and across native writes spanning several output
fragments. Sanitized Linux and strict MSVC C89 controller checks cover clock
gaps, zero-output source slices, native epoch changes, backwards positions,
mapping-capacity backpressure and end-of-stream drain without extrapolation.
It still needs the frontend resampler/write hooks and receiver binding.

Native HDR receiver output, remaining backend/source device-loss integration
and hardware-clock audio/video remain unfinished.
Native Apple execution is deferred to the requested Mac handover.


The frontend resampler/write hooks and receiver audio binding are now
integrated. Eight sanitized output-path comparisons cover mono/stereo,
44.1/48 kHz conversion, complete native lends and forced 23-frame writes.
They compare the actual output bytes against the existing float resampler,
overwrite the source PCM loan before pending writes finish, and verify
that hardware presentation cannot advance through unmapped silence.
Native MSVC C89 /W4 /WX probes compile the complete receiver and source
adapters with OpenGL, Vulkan and D3D12 enabled.

The actual Windows receiver runs PCM on the existing audio worker and
waits for WASAPI presentation to drain before releasing its binding.
Mono/stereo and direct/threaded receiver checks pass against the native
endpoint. Frame selection holds future native images and drops expired
compressed frames before decode. A bounded offline trace records candidate
and promoted timestamps on the GPU owner and prints only after quiescence.
D3D12 GPU validation, OpenGL debug and Vulkan synchronization validation
all pass direct/threaded with a three-second video lead, proving that the
hold is exercised and every promotion is at or below its proven timeline
ceiling. A negative fixture bypassing that ceiling is rejected. The tests
measure software selection and native audio presentation; they do not
measure display scanout or acoustic end-to-end latency.

This exposed and fixed a video-tail anchor being initialized during the
metadata-only call before its frame time existed. Only timed submissions
now start the tail clock. Audio mode changes also invalidate kept-device
and session-carry reuse, so presentation tracking does not leak into the
next ordinary core. The full Linux compute job, including audio sanitizer
checks and Windows cross builds, passes with these audio-path changes.


On 20 November, both GL sender and receiver recover from a missing native
completion marker using a separate ordered retirement marker. The native
fixture leaves actual dispatches untracked, exercises direct/threaded paths,
and requires clean retirement and GL debug output without errors. Explicit
context-loss recovery skips native calls only after a native context-loss error.

D3D12 and Vulkan source construction, polling and destruction now run on the
renderer owner through cold asynchronous jobs. GPU quiescence precedes the
I/O worker forgetting its compressed loan; I/O and snapshot quiescence precede
native abandonment. An abandoned primary may leave upload or swapchain state
uncommitted, so these sources stay paused until the frontend rebuilds the
device. Native D3D12 Close/Signal gate fixtures and both logical-removal
stages pass direct/threaded with zero GPU validation errors. Vulkan retained
unsubmitted-source fixtures pass both modes with synchronization validation.
D3D12 recovery also retires the next label already named by renderer loans
before a failed Close; the label is skipped before any future submission.
The source oracle explicitly rejects native fence timeouts.

The refreshed master builds pass strict MSVC C89 checks on Win32 and Win64,
including full GL/Vulkan/D3D12 source/receiver code and threadless stubs.
Portable Metal contracts and forced-native C-bridge compilation pass; this
is still not evidence of Apple execution. The full Linux compute sanitizer
job and all three supported frontend link variants pass. Vulkan requires
threads in configure; an unsupported threadless Vulkan configuration is
rejected rather than claimed as a link target.

Linux GL and Vulkan frontend receiver checks also run through the shipping
ALSA driver using its actual virtual null PCM, covering direct/threaded EOF
and threaded reset/reconnect. This caught prepared-PCMs accepting data without
automatic startup: the presentation path now starts the native PCM after its
accepted start threshold and still requires native state/delay before claiming
presentation. The added real-null startup/drain regression and existing
partial-write, XRUN/suspend and delay contracts pass ASan/UBSan. The virtual
endpoint does not establish physical Linux device latency or acoustic A/V.
