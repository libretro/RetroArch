# VP9 headers and hardware-session sequencing

`make check` exercises the production uncompressed-header parser and the
platform-neutral hardware session with a mock backend. `make stream-check`
requires Python and FFmpeg with libvpx-vp9; it generates two 48-frame IVF
streams (profile 0 and profile 2), inserts a BUSY response before every decode,
and compares logical/physical references and effective filter/segmentation
metadata with the software decoder. These are CPU tests, not GPU decode or
pixel-conformance tests.

Use `SANITIZER=address,undefined` for sanitizer builds. All C tests also build
with MinGW. The session test accepts an IVF path as its optional argument;
this test reader expects individual visible frames, without superframe indices.
The synthetic tests separately cover hidden and show-existing frames.

Hardware backends implement `rvp9_hw_ops_t`. Initialize a caller-owned
`rvp9_hw_session_t` with 10-32 physical DPB slots, and submit individual coded
frames after container superframe splitting. Parse the first keyframe with
`rvp9_parse_frame_header` to query the backend's profile/size/format support
before opening its resources. The session does not register or open a platform
decoder, allocate software frame buffers, or parse entropy-coded tile data.

Drain or discard pending display output before the next packet. A backend
BUSY return must perform no submission: retain the immutable packet until retry
or cancellation. Accepted submissions commit the eight logical reference slots
atomically from the owner's perspective. Physical slots remain distinct from
logical slots, which can alias a single reconstructed picture. Keyframes,
hidden frames and show-existing output follow the same mapping.

`output` exports an independent NV12/P010 GPU-frame lease; it must survive DPB
slot reuse and backend close until the consumer signals done or abandons it.
Backends protect actual GPU reads/writes and leases with their existing fences.
`discard_output` avoids exporting a surface when a display frame is dropped.
No session operation waits, allocates, copies pixels, or reads GPU pixels.

An opaque bitstream factory sets `rvp9_hw_backend_t.packet_decoder` to true;
WebM copies this immutable choice into the session. Direct callers use
`rvp9_hw_session_init_mode`. The default initializer and surface factories keep
the existing output-only repeat path. Packet mode submits every repeated-display
packet through `decode`, retaining its bytes on BUSY and preserving the reference
map, previous picture and coded-header state across retry or cancellation.
Only an accepted repeat publishes display output. Hidden coded pictures still
update references without producing display output. Session and WebM tests cover
both modes, including superframes, lacing, backpressure and held output at close.
This contract does not register or implement a native MediaCodec VP9 backend.

The initial session supports fixed-size profile 0 8-bit and profile 2 10-bit
4:2:0 sequences beginning with a keyframe. Profile, depth or geometry changes
return unsupported without submitting: cancel/retire and reopen resources or
use software fallback. Backend decode/output failures make the session unusable.
D3D11 profile-0/profile-2 submission is registered when compiled in. The WebM
animation stream opts into it through image_transfer and the shared video_player,
including the thumbnail animation path. The primary media_core WebM path
also adopts this stream; its host tests cover GPU sink ownership, transport
and software fallback in
`samples/cores/webm_player`. Vulkan VP9 submission is also registered.

Reference: [Vulkan VP9 reference management and show-existing behavior](https://docs.vulkan.org/features/latest/features/proposals/VK_KHR_video_decode_vp9.html).


`rwebm_hw_stream_test` links the production WebM stream, image_transfer and
video_player with a mock backend. Its synthetic container uses valid parsed
uncompressed headers with opaque dummy entropy bytes, consumed only by the mock.
It covers hidden frames and multiple visible components in a superframe,
BUSY retries at decode and export, packet-floor pinning, fixed laces, partial
input, rewind, backend withdrawal, HDR refusal, untaken/held leases and GPU sink
selection without CPU presentation. Intermediate display components are dropped
without surface export. This test does not establish actual GPU bitstream decoding.

Real-device WebM fixtures and independent pixel oracles live in
samples/gfx/d3d11_video_decode. Software fallback is retained when initial probing
fails; a later backend failure requires playback restart, rather than switching
reference chains mid-stream. GPU frames still incur the backend's existing
DPB-to-shared-output copy. Initial still-image decoding remains software.

`rwebm_video_stream_skip` advances one displayed frame without a GPU surface
export or software color conversion. Retry the same next/skip operation while
it reports waiting. The movie core latches that choice across BUSY and input
retries, so seek/catch-up frames avoid the DPB-to-output copy. Presented frames
still require the backend's independent shared-output lease.

Hardware seeks rewind to a keyframe and discard preroll without exporting
GPU frames. Each seek step submits at most one container packet and retains
pending submission state across BUSY responses. The target packet is kept for
the next displayed frame. The stream test covers forward/backward seeks,
partial input, superframes, show-existing frames, seek replacement while BUSY,
and a held frame surviving decoder reset and close. These mock checks establish
sequencing and ownership, not native GPU pixels or seek performance.
