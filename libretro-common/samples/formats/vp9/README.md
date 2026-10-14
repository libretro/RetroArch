# VP9 headers and hardware-session sequencing

`make check` exercises the production uncompressed-header parser and the
platform-neutral hardware session with a mock backend. `make stream-check`
requires Python and FFmpeg with libvpx-vp9; it generates two 48-frame IVF
streams (profile 0 and profile 2), inserts a BUSY response before every decode,
and compares logical/physical references and effective filter/segmentation
metadata with the software decoder. These are CPU tests, not GPU decode or
pixel-conformance tests.

Use `SANITIZER=address,undefined` for sanitizer builds. Both C tests also build
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

The initial session supports fixed-size profile 0 8-bit and profile 2 10-bit
4:2:0 sequences beginning with a keyframe. Profile, depth or geometry changes
return unsupported without submitting: cancel/retire and reopen resources or
use software fallback. Backend decode/output failures make the session unusable.
Platform submissions, automatic player/thumbnail selection, superframe retries,
device-loss handling and real-GPU acceptance are separate integration work.

Reference: [Vulkan VP9 reference management and show-existing behavior](https://docs.vulkan.org/features/latest/features/proposals/VK_KHR_video_decode_vp9.html).
