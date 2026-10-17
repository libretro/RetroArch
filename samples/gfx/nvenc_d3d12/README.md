# Native D3D12/NVENC encoder

`gfx/common/nvenc_d3d12.c` is an encoder-worker adapter for H.264, HEVC and
AV1. It retains a fixed pool of D3D12 textures, registers/maps them once,
waits on producer fences on the GPU, and writes encoded packets to its
own bitstream buffers. Packet retrieval uses completion-fence status and
nonblocking NVENC locking. Busy calls do not consume input; packet release
returns the corresponding input slot. EOS is retryable and idempotent.
Free refuses outstanding packets/EOS; a removed device permits teardown.

Input textures must be single-layer, single-mip, single-sample resources
of the configured size/format, on the encoder device, in COMMON state
when their ready fence signals. Keep a slot immutable until packet release.
The codec, format, async mode, 10-bit support and actual resource registration
are capability gates. RGBA may be refused even when generic input-format
enumeration advertises it. This host accepts BGRA, NV12 and P010.

Startup and teardown belong on the encoder worker. There are no added
frame-path sleeps, locks, CPU fence waits or pixel readbacks. Encoded bytes
are returned to CPU memory for muxing; that is distinct from reading pixels.
Frontend capture attachment, core audio callbacks and HDR render-format
conversion remain unconnected. Building the adapter does not enable a new
user-facing recording mode. Automatic HDR signaling is not implemented.

## Validation

Run `make check`, or `make check CC=x86_64-w64-mingw32-gcc EXE=.exe RUNNER=wine`.
Unsupported platforms/runtimes skip explicitly. Set `NVENC_TEST_REQUIRE_HW=1`
to turn a hardware skip into failure. Native Windows needs FFmpeg CLI for
independent software decoding; production does not use FFmpeg.

The sample builds with both the system and vendored D3D12 headers to check
COM aggregate-return ABI compatibility.

The test exercises a deliberately unsignalled producer fence, all four
slots full, duplicate-slot refusal, retained textures, injected encoder/lock
busy returns, packet lifetime, timestamps, forced keyframes and EOS while
frames remain queued. Eight codec/format cases cover BGRA/NV12 on all three
codecs and P010 on HEVC/AV1. All 192 independently decoded frames pass with each header variant on
the RTX 5090. Pixel comparisons check content/order, geometry and 10-bit
output; they do not establish HDR display acceptance, muxing or A/V sync.

Explicit input color descriptions cover BT.709 and P010 BT.2020 PQ/HLG,
with limited or full range. The harness independently checks decoded stream
tags for H.264, HEVC and AV1. These tags describe the supplied pixels; the
adapter does not convert transfer functions. Optional HDR mastering and light
metadata use the shared video_color.h units. Complete mastering data and
MaxCLL/MaxFALL are serialized into explicit HEVC SEI or AV1 metadata payloads;
the adapter copies them at open and retains them through encoder retries.
Absent metadata stays absent. Display HDR and frontend recording acceptance
remain pending.

The renderer-side producer now reserves slots through record_gpu_queue and
hands timestamp/duration/keyframe/fence-value descriptors to the encoder
worker. A BUSY encode retains its descriptor; only packet release returns
the slot. Producer close reaches EOS before outstanding GPU output drains.
The native harness uses separate producer and encoder threads; frontend
capture and core audio callback routing remain pending.

`record/record_gpu_capture_d3d12.c` supplies the GPU capture pool for that
handoff. It allocates textures and per-slot command resources at startup,
copies a compatible source into an available slot and publishes its GPU
completion fence/value. RGB sources can be cropped to a viewport; RGBA is
converted to BGRA on the GPU for NVENC's D3D12 input format. The private draw
pipeline and descriptor heaps are allocated at startup. Matching formats
use texture copies. NV12/P010
currently require a full texture. It restores source state and copies both
planar subresources. A busy pool consumes nothing and never resets a command
allocator still in use. Capture retains submitted source textures and optional
source fences through copy completion; later pushes, nonblocking reap or free
release completed references. The caller keeps source contents
unchanged through the returned completion value. Reap releases completed source
references even while the encoder holds its destination slot; use it before
swapchain resizing. There are no pixel uploads/readbacks or CPU waits in capture.

The native harness additionally compares eight captured codec/format cases
and captures two HDR streams through requested and periodic keyframes. A
queued GPU dependency holds all four capture/encoder slots in flight. A
failed completion signal closes publication and retains the command/texture
pool until a recovery signal completes; this is injected against a real
GPU copy/draw. Captured frames must decode identically to direct input,
including RGBA-to-BGRA viewport drawing and a separate BGRA viewport copy.
Direct input tests remain separate. Shader source/blob digests are checked
portably; regenerate with `python3 record/shaders/generate_rgb.py` on Windows.
Frontend attachment, HDR render-format conversion and core audio routing
remain pending.

HDR cases independently parse every mastering coordinate and luminance/light
value through FFmpeg's bitstream tracer, including AV1's different fixed-point
units. They cover PQ/HLG, mastering-only/light-only/both, zero values and upper
bounds, and decode a second stream beginning at the later forced keyframe.
Invalid/incomplete metadata is refused before opening an encoder session.

`record_gpu_worker_d3d12` now services the queue and encoder in bounded steps.
It keeps a descriptor across submit BUSY/error, retains packet bytes until
explicit downstream acknowledgement, and returns the texture slot only after
NVENC unlock. Queue close reaches EOS only after every descriptor is accepted;
END requires output and EOS completion. All native cases use this production
pump, including encoder/lock/EOS BUSY and packet backpressure. `worker_test`
checks retryable failures, empty close, timestamp gaps and ownership teardown
with a deterministic portable encoder. Abandon is reserved for a stopped
producer after successful encoder destruction; capture GPU retirement remains
the capture owner's responsibility. SDR frontend acceptance lives in samples/record/nvenc_frontend.

The native harness now obtains startup sequence headers through the adapter
and writes an MP4 counterpart through `record_mux_mp4` for every session.
Each encoded packet stays worker-owned across mux backpressure; serialized
output is separately acknowledged. Independent decoding checks MP4 pixel,
timestamp and keyframe equality with the elementary stream. The pure muxer
also supports native AC-3 A/V (see samples/record/mp4_mux). The SDR frontend uses this pipeline; see samples/record/nvenc_frontend.

Three additional native sessions exercise GPU capture/encode plus concurrent
PCM submission through the production A/V output pump and overlapped writer.
The shared runner compares video pixels/keyframes and exact 1536-sample audio
clocks against raw video and independently encoded AC-3 references at 48 kHz
stereo, 44.1 kHz mono and 32 kHz 5.1. SDR frontend acceptance lives in samples/record/nvenc_frontend.

Three dispatcher sessions run the same video/audio oracles through
`record_gpu_dispatch_d3d12` and its owned session. Separate video and PCM
producers submit while the dedicated worker opens, drains and retires the
encoder, output pump and file. The source fence remains unsignalled until
four capture slots are published. The controller only observes status and
collects after both producers stop; it never services the session itself.
These cases cover H.264/BGRA with 48 kHz stereo, HEVC/P010 with 44.1 kHz mono
and AV1/P010 with 32 kHz 5.1. They establish native combined pipeline
acceptance; HDR frontend metadata and full application acceptance
remain pending.

Source-lifetime fixtures queue four GPU copies behind an unsignalled fence,
release the caller's source/fence references, refuse a fifth push without
retaining anything, and reap after completion while destination leases remain
held. Counted COM calls verify exactly one retained/released pair per submitted
slot. Replacement checks retain the new source/fence before releasing old
references to the same objects. Separate fixtures refuse a queue wait before
submission and retain the
source through a failed completion signal and recovery. Both header variants
run these cases on native Windows as part of every capture session.

`swapchain_test` verifies BGRA copies and RGBA draws from real DXGI backbuffers.
After GPU completion, ResizeBuffers still refuses the retained source references;
explicit reap permits resize while all four destination slots remain leased.
The same capture pool then crops the replacement backbuffer and permits another
resize after producer closure. This test needs D3D12/DXGI, but no NVENC runtime.
Both system and bundled D3D headers build it; other platforms explicitly skip.

Owned-dispatcher sessions transfer their fresh capture to the dispatcher. The render
producer reaps through the dispatcher while open; after closure, the I/O worker
retires the capture and GPU source references before terminal status. The
controller joins only the test producers and nonblockingly collects the job;
it performs no capture destruction. Borrowed-dispatcher cases remain separate. These cases retain the independent video
and PCM oracles for H.264, HEVC and AV1.

`hdr_capture_test` converts PQ/Rec.2020 RGB10A2 and linear scRGB FP16 to
PQ/Rec.2020 P010 on the GPU. It tests limited/full range, an odd-origin crop,
non-workgroup-aligned dimensions, 80/1000/10000-nit source levels, negative
scRGB components, chroma edges and pool reuse. Every read-back plane value is
checked against a double-precision CPU reference (one 10-bit code tolerance,
zero low six bits). This readback belongs only to the test. The output uses
left/vertical-centre chroma with a three-tap horizontal filter.
When P010 typed UAV stores are supported, conversion writes directly into
the encoder texture's two plane views. Those slots allocate no intermediate
planes and issue no conversion copies. Failed capability queries, missing
store support or rejected UAV textures retain the intermediate-plane path,
independently for each slot. The test intercepts only the capture's API calls
to exercise these failures and a mixed pool, and checks allocation/copy counts
alongside the real GPU results. `NVENC_TEST_REQUIRE_DIRECT_HDR=1` requires the
unmodified hardware case to use four direct textures.
Twelve frames per case are then encoded as HEVC/AV1 and independently decoded;
the runner checks pixels and PQ/Rec.2020/range signaling. Native D3D12 debug
layer errors fail the test when the validation layer is available. Both header
variants build on Linux and MinGW; hardware absence is an explicit skip unless
`NVENC_TEST_REQUIRE_HW=1`. The swapchain test also checks RGB10A2/FP16 conversion
and source retirement across ResizeBuffers. HDR frontend admission remains
disabled pending backend metadata and full frontend acceptance.
