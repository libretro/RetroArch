# Embedded RSX shaders

These are committed native RSX vertex (`.vpo`) and fragment (`.fpo`) programs
compiled from the adjacent Cg sources. The four programs used by the PSL1GHT
frontend total 1,088 bytes. Their parameter tables retain the names used by
`rsx_gfx.c`, including `modelViewProj`, vertex attributes and texture samplers.

`Makefile.psl1ght` verifies the bundle and embeds each binary using the SDK's
existing `bin2o` rule, preserving the `modern_*_vpo`/`modern_*_fpo` symbols and
64-byte alignment. The resulting objects are linked into RetroArch. Normal
builds need Python 3 and the PS3 toolchain, but do not need cgcomp, NVIDIA Cg,
Cg DLLs/shared libraries or external runtime shader files. Clean preserves
these committed artifacts. Source edits fail verification until regenerated.

The manifest records source/artifact hashes, program metadata, the cgcomp
upstream revision and executable hash, the Cg library hash, profiles and
compiler options. These artifacts were generated with NVIDIA Cg 3.1.0013
and PSL1GHT cgcomp from revision `dd4e8c9be744e3273d665c393fa6d97a87402e2e`.

The local 64-bit host tool build corrected this upstream argv initialization:

```c
memset(argv, 0, numArgs * sizeof(*argv));
```

Upstream initialized only `numArgs` bytes, leaving the argument pointer array
unterminated on this host. The correction is in the maintainer tool build;
no compiler binary or proprietary Cg library is distributed here.

Explicit maintainer regeneration, with the Cg library discoverable by cgcomp:

```
python3 tools/rsx_shaders.py --regenerate --compiler PATH_TO_CGCOMP --cg-library PATH_TO_CG_LIBRARY --cgcomp-revision TOOL_SOURCE_REVISION --compiler-note HOST_BUILD_NOTES
python3 tools/rsx_shaders.py --check
python3 tools/rsx_shaders_test.py
```

Review and commit sources, binaries and manifest together. The unused
`vpshader_basic.vcg`/`fpshader_basic.fcg` sources are not part of the frontend's
shader object list and are not included in this bundle.

Artifact checks and PowerPC64 embedding were tested without cgcomp available
to the normal build. Native PS3 execution still requires hardware validation.

These containers use PSL1GHT's `rsxVertexProgram`/`rsxFragmentProgram`
structures. They are not advertised as PS3 SDK/PSGL program binaries. A
separate SDK path would need SDK compilation or a loader for this container
and its parameter tables, followed by SDK/hardware validation. The native
instruction payload and Cg sources are distinct from that container ABI.
