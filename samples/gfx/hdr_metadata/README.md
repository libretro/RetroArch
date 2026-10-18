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
EnableHDR is an optional scalar float: 1.0 for active HDR output, otherwise
0.0, including refused HDR. An explicit pragma parameter keeps its value.
Use --enable-hdr builtin or --enable-hdr parameter in either image runner.
MinNits is an optional scalar float matching the unknown minimum in the
metadata query: 0.0 on HDR and SDR output. It uses immutable fallback storage
on D3D and Metal, and declared-only constant writes on GLCore and Vulkan.
Explicit pragma parameters retain their value. Use --min-nits builtin or
--min-nits parameter in either image runner. No panel minimum is inferred.
D3D11/12 take HDRMode from the active swapchain and clear HDR output metadata on
SDR fallback. ../slang_hdr_bindings/output_state.py exercises the shipping
state updates through PQ/scRGB-to-SDR transitions and compiled-out HDR.
PaperWhiteNits is an optional float alias of BrightnessNits on D3D11/12,
Vulkan, GLCore and Metal. Both names may coexist; an explicit pragma parameter
named PaperWhiteNits retains its own value. The alias reuses the current
paper-white snapshot and adds no pixel transfer or frame-path allocation.
PeakNits and MaxNits are optional float spellings of the effective HDR
target peak. The frame carries the selected display override or configured
maximum, and each backend supplies zero without active HDR output. Both
spellings may coexist or retain independent explicit pragma parameters.
../slang_hdr_bindings/peak.py checks override selection and frame snapshots.
Metal uses these same typed semantics with frame-carried content paper white
and gamut. Scanlines and SubpixelLayout use the compositor's render-owned
latches. InverseTonemap and HDR10 use its preset/output conversion policy.
All four become zero on SDR output or without compositor uniforms. Its
negotiated layer encoding supplies HDRMode; failed colorspace setup and
compiled-out HDR supply zero. Per-view chains rebase their private
uniform values. ../slang_hdr_bindings/metal.py checks the shipping bindings,
frame handoff, neutral values and rebasing with portable toolchains. Native
Apple rendering acceptance remains pending.
Slang constants are captured per draw before CPU staging is reused. Blocks
smaller than 4096 bytes use the encoder's byte-copy API; larger blocks use
an immutable command-retained buffer. Large-block allocation failures skip
the affected draw, including view placements. This adds no image copy,
readback or GPU wait. Large constant buffers allocate per draw; reuse needs
completion-aware ownership and native Apple performance validation.
../slang_hdr_bindings/metal_constants.py models queued consumption of the
shipping binding code, including boundary sizes and allocation failures.

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

TonemapOperator is an optional scalar uint identifying the frontend HDR
curve: 0 is the current rational operator. It identifies the curve even
when conversion is bypassed or output is SDR; it does not enable tonemapping
or change source/final-pass ownership. Explicit float pragma parameters of
the same name retain their own value. No operator selection is introduced.
Use --tonemap-operator builtin or --tonemap-operator parameter with the HDR
uniform fixtures to check UBO/push storage and parameter priority.

SourceHDR is an optional scalar uint: 1 means Original contains native
HDR10_2101010 PQ Rec.2020 input. It is 0 for SDR, native linear scRGB FP16,
and imported SDR/scRGB movie textures. It does not describe the swapchain
or final-pass ownership. Explicit float pragma parameters keep their value.
D3D10 and Metal currently admit no native PQ core input and bind zero.
Use --source-hdr builtin or parameter in the uniform fixtures. The FP16
fixture also accepts --source-pq to verify PQ samples and the flag. In the
SDR fixture, --source-pq requires --requested-hdr 1 or 2 on a display that
refuses HDR; an initial PQ request with HDR explicitly off is refused.

Framebuffer lending preserves the admitted source format in its descriptor,
including XRGB2101010 and HDR10_2101010. The threaded wrapper, Vulkan and
desktop GL2 report packed 10-bit storage without relabeling it as XRGB8888.
Descriptors retain the loan's pointer, pitch and synchronization. D3D12
declines mapped wrapper memory for packed 10-bit formats, matching its
direct callback: those uploads require channel repacking. The wrapper
still lends a CPU slot and the existing upload performs that packing. Run
framebuffer_format.py for the shipping descriptor matrix.
