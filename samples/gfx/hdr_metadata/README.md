The experimental queries at command IDs 88-92 are retired. Cores must migrate
to GET_HDR_METADATA (101), or GET_DISPLAY_METADATA (102). IDs 93 and 94 already
belong to VFS authorization and multichannel audio and cannot be used for HDR.

Zero the caller-owned structure, set version to 1 and size to sizeof the
structure, then call the environment callback from the emulation thread.
Unsupported versions and short buffers fail without modifying the buffer.
No video-thread command, GPU readback or CPU pixel conversion is performed.

output_mode is the configured output encoding, not the requested setting or
the monitor's HDR capability. Before output initialization it is zero, while
screen_10bpc can report the configured source capability for format negotiation.
passthrough is conditional on accepted native HDR10 input; it also permits
PQ/Rec.2020 to linear Rec.709 conversion for scRGB. It does not change existing
shader preset ownership heuristics. Unknown content light levels, geometry and
display capabilities remain zero. The effective peak target is separate from
display capabilities, including when Use Display Peak supplies it.

make check exercises the bounded ABI. python3 run.py compiles extracted shipping
provider and DXGI code with portable threads, including simultaneous independent
swapchains. CFLAGS may enable ASan/UBSan or TSan. live.py --retroarch PATH
--log-dir DIRECTORY checks actual environment dispatch, direct and threaded,
with a null driver and a deliberately stale HDR request. --driver d3d12 --hdr
also probes native SDR/PQ/scRGB output; inspect the reported mode to distinguish
an available HDR path from an SDR fallback. The live core is a thin wrapper over
the recording fixture and produces no output recording.
