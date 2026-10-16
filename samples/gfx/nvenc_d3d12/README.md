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
The frontend recording/export queue, GPU format conversion, muxer and A/V
handoff are not connected yet. Building the adapter does not enable a new
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
adapter does not convert transfer functions or add mastering-display/content
light metadata. Display HDR and frontend recording acceptance remain pending.

The renderer-side producer now reserves slots through record_gpu_queue and
hands timestamp/duration/keyframe/fence-value descriptors to the encoder
worker. A BUSY encode retains its descriptor; only packet release returns
the slot. Producer close reaches EOS before outstanding GPU output drains.
The native harness uses separate producer and encoder threads; frontend
capture, worker scheduling and recording/mux/audio hookup remain pending.
