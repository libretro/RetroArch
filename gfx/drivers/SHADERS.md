# Built-in shaders

GLSL is grouped by the minimum desktop OpenGL generation: `gl2_glsl_shaders`
(GLSL 1.10/1.20), `gl3_glsl_shaders` (GLSL 1.30 and later draw shaders),
and `gl4_glsl_shaders` (OpenGL 4 specific programs). OpenGL 4 contexts reuse
GL2/GL3 draw programs where those suffice. GLES variants stay beside the
corresponding draw programs; `snow_gles2` identifies the GLES2 vertex path.
Cg programs live separately in `gl_cg_shaders`. Each family puts menu effects
in `effects/`. Vulkan and GX2 follow the same effects layout.

D3D programs are grouped by shader model, `d3d_sm2_shaders` through
`d3d_sm6_shaders`. Their `precompiled/` and `effects/precompiled/` directories
contain committed native bytecode: legacy token streams for SM2/SM3, DXBC
for SM4/SM5, and DXIL for SM6. Shared source headers avoid duplicating HLSL;
each model is compiled separately for its actual native target profile.

## Runtime and build

D3D10/11 built-ins use embedded DXBC. D3D12 uses DXIL on SM6 devices and DXBC
on older devices. Blob wrappers borrow immutable executable data; releasing
a wrapper never copies or frees shader bytes. Built-in paths select short program identifiers in the committed catalog;
HLSL source strings and source fingerprints are not embedded for lookup. A
missing identifier fails rather than invoking a shader compiler. User-supplied shader presets keep their runtime compilation path.

Ordinary builds only run Python to verify source and artifact hashes and
embed the committed bytes. FXC and DXC are maintainer tools, not build or
runtime dependencies for built-in D3D10/11/12 shaders. Hash checks normalize
CRLF in sources but never modify binary bytecode. The manifests record target
profiles, source fingerprints, artifact hashes, compiler binary hashes and
compiler options. SM4+ artifacts omit unused reflection and debug chunks;
input/output signatures remain intact. The embedder also verifies native
program profile tokens.

To regenerate on Windows, build `tools/d3d_shader_sources.c` as C with
`libretro-common/include` on the include path, then run:

```
python tools/d3d_shader_precompile.py --sources PATH/TO/sources.exe --fxc PATH/TO/fxc.exe --dxc PATH/TO/dxc.exe
python tools/d3d_shader_embed.py --check
```

Commit the updated source and native blobs together. DXC uses the HLSL 2016
language rules required by the existing HDR expressions.

## Lower shader model fallbacks

SM2/SM3 provide native raster variants for textured quads, meshes, expanded
sprites, stereo views, bloom, ribbon, snow, bokeh, snowflake, SDR tonemapping,
pixel downsampling and stacked planar YUV conversion. SM4 also provides
pixel downsampling and planar conversion alternatives to compute. The
fallback entry points are `VSMain`/`PSMain`, with alpha and stereo entries
where applicable. Effects use simpler math to fit native instruction limits.

Geometry/compute stages are not available in SM2/SM3, and SM4 has no SM5
compute kernels. These fallbacks require CPU-expanded sprite geometry or
raster passes; they do not emulate unsupported stages. HDR transfer functions
in lower-model fallback assets are SDR approximations, not an HDR swapchain
implementation. Existing D3D9 effect paths remain intact; the additional
fallback assets remain in the repository and full maintainer test catalog.
Production builds exclude them until a frontend path uses them, along with
backend-specific compute variants that the enabled drivers cannot use. This
is not a claim that every D3D9 device gains every frontend feature.

`make -C samples/gfx/d3d_builtin RUNNER=wine check` checks all native stages,
source keys, COM lifetime and shared byte storage, and creates all SM4/SM5
stages through a D3D11 device. It can run natively on Windows without RUNNER.
Native SM6 execution is covered by D3D12 frontend tests. The low-model token
streams are compiled by FXC; old SM2 hardware and Wii U/macOS still require
native platform validation.

## SPIR-V optimization

The Vulkan shader Makefile runs `tools/spirv_optimize.py` after compilation.
This is an explicit maintainer regeneration step, not an ordinary frontend
build requirement. The shared modules use an explicit size pass list that
retains reflected names, interface variables, bindings, specialization
constants and unused buffer members. Removing unused members can change
GL buffer flattening/upload contracts even when Vulkan validation succeeds.
`tools/spirv_optimize_test.py` verifies reflected resource layouts and the
frontend's ribbon upload block in CI. Every module is validated before and
after optimization.

PyroWave's Vulkan-only exports use the size pipeline with debug names
stripped. GLSL and Metal are still generated from the original named IR;
their committed assets do not change when Vulkan modules are optimized.

## Embedded GLSL text

The generated GL-core fallback strings and embedded PyroWave GLSL/GLES
compute strings pass through `tools/glsl_minify.py`. Readable shader sources
and committed compute source artifacts remain unchanged. The minifier only
removes comments and unnecessary whitespace, verifies that the GLSL token
stream is identical, and preserves identifiers and preprocessor directives.
Line-sensitive source and escaped line continuations are retained verbatim.
It does not rewrite arithmetic, rename uniforms or add runtime processing.
The C-stringified GL2/GL3 shader headers already remove comments and collapse
whitespace through the C preprocessor; they do not use this generation step.

CI checks tricky token boundaries and directive handling, and verifies that
all six PyroWave GLSL/GLES variants compile to byte-identical OpenGL SPIR-V
before and after minification with the reference compiler. Actual GL driver
rendering and codec tests cover the embedded strings. This establishes
unchanged operations rather than making an unmeasured FPS claim for every
vendor driver.
