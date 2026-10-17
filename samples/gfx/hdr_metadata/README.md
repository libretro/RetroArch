The experimental queries at command IDs 88-92 are retired. Cores must migrate
to GET_HDR_METADATA (101), or GET_DISPLAY_METADATA (102). IDs 93 and 94 already
belong to VFS authorization and multichannel audio and cannot be used for HDR.

Zero the caller-owned structure, set version to 1 and size to sizeof the
structure, then call the environment callback from the emulation thread.
Unsupported versions and short buffers fail without modifying the buffer.
No video-thread command, GPU readback or CPU pixel conversion is performed.

output_mode is the configured output encoding, not the requested setting or
the monitor's HDR capability. Before output initialization it is zero, while
`source_formats` reports a `RETRO_HDR_SOURCE_FORMAT_*` mask for native core input,
including configured capability before initialization. HDR10 packed input and FP16
linear scRGB have distinct bits; FP16 is reserved and remains unadvertised until
its core-input negotiation and submission path exist. Output precision alone
does not imply input support. Consumers must ignore unknown bits and still
check format negotiation.
passthrough is conditional on accepted native HDR10 input; it also permits
PQ/Rec.2020 to linear Rec.709 conversion for scRGB. It does not change existing
shader preset ownership heuristics. Geometry is the display server's desktop
size, cached whenever a display server comes up, so it is known in
retro_load_game before the video driver starts. Unknown content light levels,
geometry, refresh range and display capabilities remain zero. The effective peak target is separate from
display capabilities, including when Use Display Peak supplies it.

make check exercises the bounded ABI. python3 run.py compiles extracted shipping
provider and DXGI code with portable threads, including simultaneous independent
swapchains. CFLAGS may enable ASan/UBSan or TSan. live.py --retroarch PATH
--log-dir DIRECTORY checks actual environment dispatch, direct and threaded,
with a null driver and a deliberately stale HDR request. --driver d3d12 --hdr
also probes native SDR/PQ/scRGB output; inspect the reported mode to distinguish
an available HDR path from an SDR fallback. The live core is a thin wrapper over
the recording fixture and produces no output recording.

GLCore capability tests exercise the shipping publisher and getter while another
thread changes FP16/PQ framebuffer state. A scRGB texture is accepted only when
the selected UI target preserves FP16 highlights. No render-thread handles or
mutable framebuffer flags are read by the capability consumer.
