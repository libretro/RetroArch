# Committed PyroWave shader artifacts

These files are deliberately committed at the user's request so RetroArch
builds and distributions do not require a shader compiler or external runtime
shader assets. They derive from the independently authored MIT-licensed HLSL
in the parent directory; that license also applies to these artifacts.

Each color/DWT/packet stage has:

- `.spv`: size-optimized, validated Vulkan 1.0 SPIR-V with debug names stripped.
- `.dxil`: DXIL compute shader model 6.0, used by capable D3D12 devices.
- `.dxbc`: DXBC compute shader model 5.0, used by older D3D12 devices and
  suitable for a future D3D11 compute binding. This does not add a D3D11 backend.
- `.comp` / `.es.comp`: translated GLSL 4.30 / GLES 3.10 source. Portable
  OpenGL program binaries are vendor/driver specific, so the driver compiles
  this bundled source at runtime.
- `.metal`: translated Metal 2.1 source. A native metallib must be compiled
  and validated on a Mac; no precompiled Metal library is claimed here.

`manifest.json` records source hashes, artifact sizes/hashes and compiler
versions/profiles. `tools/pyrowave_embed_shaders.py` verifies the bundle and
embeds them as C89 arrays for the executable. Binary artifacts remain byte
identical; GLSL/GLES text is conservatively minified during embedding while
retaining tokens, identifiers and directives. Readable source files remain
unchanged. It needs Python,
not glslang, SPIRV-Cross, spirv-opt, FXC or DXC. A source change fails verification until
its artifacts are regenerated; corrupted or wrong-format blobs fail too.
Only formats required by enabled video backends become executable data.

Regenerate explicitly on a maintainer Windows environment with the shader
tools on PATH and Windows SDK compilers available:

```
python tools/pyrowave_precompile.py --dxc PATH/TO/dxc.exe --fxc PATH/TO/fxc.exe
python tools/pyrowave_embed_shaders.py --check
python tools/pyrowave_embed_shaders_test.py
```

Review and commit the artifacts and manifest together with any source change.
Run actual GPU tests for each affected backend; hashes establish provenance
and integrity, not shader correctness. Preserve the existing native Metal
compilation test when carrying changes to a Mac.

Vulkan exports are optimized with spirv-opt while preserving descriptor
bindings, interface variables and specialization constants. Validation runs
before and after the pass. Cross-compilation for GLSL/Metal uses the original
named intermediate module, independently of the stripped Vulkan export.
The optimizer version and pass options are recorded in the manifest.
