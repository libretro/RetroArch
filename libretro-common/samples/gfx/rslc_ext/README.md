# Extended shader validation

`make check` runs the compiler and builder contracts, then independently
validates emitted SPIR-V and GLSL when those tools are installed. The dedicated
`Linux-shader-validation.yml` job requires both validators; its SPIRV-Tools
checkout and matching headers are pinned and cached separately from system tools.
No compiler or validator is downloaded by `make check` itself.

The unsized uniform-block fixture needs
`VK_EXT_shader_uniform_buffer_unsized_array` and standard uniform-buffer layout
for its declared `std430` members. Older standalone validators reject the
runtime array before checking layout. Current Khronos validation accepts the
uniform-block array and leaves the device feature requirement to runtime
validation. See the [extension specification](https://docs.vulkan.org/features/latest/features/proposals/VK_EXT_shader_uniform_buffer_unsized_array.html)
and [upstream validator](https://github.com/KhronosGroup/SPIRV-Tools/blob/3e73ae769e684f17b283250f803a231a830d45e3/source/val/validate_memory.cpp).

`validate_spirv.py` applies Vulkan 1.1 and standard UBO layout only to that
fixture. The subgroup fixture retains its Vulkan 1.1 profile, and other fixtures
retain Vulkan 1.0 validation. A deliberately misaligned member in the emitted
uniform block must be rejected, proving that enabling the required features
does not bypass layout checks.

For an independent local run, use a current SDK validator, or build the pinned
one explicitly:

```
python3 ../../../../.github/workflows/scripts/setup-spirv-validator.py --cache-dir /tmp/retroarch-spirv-validator
```

Add the printed validator directory to `PATH`, then run `make check`. A missing
validator is reported as skipped validation, and an incompatible validator
remains a failure; neither is independent acceptance.
