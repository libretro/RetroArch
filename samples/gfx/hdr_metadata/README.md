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
linear scRGB have distinct bits; FP16 has the explicit
`RETRO_PIXEL_FORMAT_SCRGB_RGBA16F` value (binary16 RGBA, native-endian samples,
8 bytes/pixel, linear Rec.709, 1.0 = 80 nits, alpha ignored). Native submission
is available with scRGB output on D3D11, D3D12, Vulkan and GLCore. Other output modes/backends reject negotiation. Output precision alone
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

D3D11/D3D12 fixtures cover published HDR texture support with HDR compiled in
and out, planar failure/feature-level admission, and concurrent renderer flag
changes. D3D11 publishes its lazy planar initialization failure atomically.

Native FP16 acceptance (requires an scRGB-capable output):

    python3 run_fp16.py --retroarch /path/to/retroarch --output /tmp/fp16-results
    python3 run_fp16.py --retroarch /path/to/retroarch --output /tmp/fp16-stock --stock
    python3 run_fp16.py --retroarch /path/to/retroarch --output /tmp/fp16-overlay --overlay --frames 120
    python3 run_fp16.py --retroarch /path/to/retroarch --output /tmp/fp16-deferred --deferred --threaded false --frames 180
    python3 run_fp16.py --retroarch /path/to/retroarch --output /tmp/fp16-resize --resize --frames 30
    python3 run_fp16.py --retroarch /path/to/retroarch --output /tmp/fp16-hw --hardware

The fixture submits padded 64-bit rows, requests native framebuffer lending,
and checks negative and above-white half-float samples in a Slang shader,
direct and threaded. D3D12 lending retains the native mapped upload buffers.
The wrapper forwards native HDR screenshot readback after the claimed frame.
CPU softfilters and recording do not support this source format; recording
rejects it rather than interpreting its rows as RGB565.

The hardware fixture checks a floating-point GLCore framebuffer. Both threaded
settings are exercised; hardware contexts use the existing unthreaded fallback.

Slang HDR uniform acceptance:

    python3 run_fp16.py --retroarch /path/to/retroarch --output /tmp/hdr-ubo --hdr-uniforms ubo
    python3 run_fp16.py --retroarch /path/to/retroarch --output /tmp/hdr-push --hdr-uniforms push --paper-white 100 --expand-gamut 2
    python3 run_sdr_uniforms.py --retroarch /path/to/retroarch --output /tmp/hdr-sdr
    python3 run_sdr_uniforms.py --retroarch /path/to/retroarch --output /tmp/hdr-d3d-sdr --drivers d3d11,d3d12

HDRMode (uint), BrightnessNits (float) and ExpandGamut (uint) use the
renderer's actual output and the settings carried with the frame. SDR output
supplies zero for all three, including when the context refused HDR. On a
machine without HDR output, --requested-hdr 2 checks that fallback. The SDR
runner covers both storage types, direct/threaded and immediate/deferred.
These bindings add no texture copies, readbacks or synchronization.
D3D11/12 take HDRMode from the active swapchain and clear HDR output metadata on
SDR fallback. ../slang_hdr_bindings/output_state.py exercises the shipping
state updates through PQ/scRGB-to-SDR transitions and compiled-out HDR.

Optional Slang final-pass ownership:

    hdr_mode = "0"   # gamma SDR
    hdr_mode = "1"   # PQ / Rec.2020
    hdr_mode = "2"   # linear scRGB, 1.0 = 80 nits
    hdr_mode = "auto" # restore legacy inference, including in a reference

The declaration applies only to the final pass. Missing, invalid and auto
values retain legacy format, scale and source heuristics. A shader parameter
named hdr_mode takes precedence over this optional metadata. GLSL presets
retain their existing behavior. Full saves and reference overrides preserve
ownership. Truncating an appended chain restores legacy inference.

Explicit PQ/scRGB declarations require a matching active output encoding;
a mismatch refuses the new preset. SDR output is converted by the frontend,
even when its storage is floating point or its core input is HDR. Explicit
HDR selects a suitable final target precision; intermediate passes retain
their own formats. This adds no CPU pixel conversion or GPU readback.
GLCore may retain a separate FP16 UI target for correct HDR composition.

    python3 ownership.py
    python3 run_fp16.py --retroarch /path/to/retroarch --output /tmp/owner-sdr --preset-hdr-mode 0 --shader-sdr --scaled
    python3 run_fp16.py --retroarch /path/to/retroarch --output /tmp/owner-hdr --preset-hdr-mode 2 --source-sdr --no-format --reference --deferred --frames 180
    python3 run_fp16.py --retroarch /path/to/retroarch --output /tmp/owner-refuse --preset-hdr-mode 1 --source-sdr --expect-rejection

ownership.py checks the shipping parser, precision/admission helpers and
Metal ownership policy. The shader_copy shipping-object harness also checks
full preset saves and reference overrides, including clearing an inherited
HDR declaration with auto. Native Metal rendering requires an Apple runner.
