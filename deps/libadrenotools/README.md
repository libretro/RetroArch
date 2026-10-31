# libadrenotools (subset)

A subset of [bylaws/libadrenotools](https://github.com/bylaws/libadrenotools)
at commit `8fae8ce254dfc1344527e05301e43f37dea2df80` (2024-09-10), with
`lib/linkernsbypass` from
[bylaws/liblinkernsbypass](https://github.com/bylaws/liblinkernsbypass) at
commit `aa3975893d83ef1bc84c321ec60c65fbf1287887`. The files are unmodified.

The Android build uses it to load custom Vulkan driver packages on Adreno
GPUs, in the sideloaded arm64 builds.

Kept:

- `LICENSE`
- `include/adrenotools/driver.h`, `include/adrenotools/priv.h`
- `src/driver.cpp`
- `src/hook/hook_impl.cpp`, `hook_impl.h`, `hook_impl_params.h`, `kgsl.h`,
  `main_hook.c`
- `lib/linkernsbypass/android_linker_ns.cpp`, `android_linker_ns.h`,
  `elf_soname_patcher.cpp`, `elf_soname_patcher.h`, `LICENSE`

Left out: BCn texture support (`bcenabler*`, `gen/`), the GPU mapping
import hook (`gsl_alloc_hook.c`), the file redirect hook
(`file_redirect_hook.c`), `tools/`, the CMake files, `build_asm.sh`,
`adrenotools.pc.in`, `.gitmodules` and the upstream READMEs.

libadrenotools and liblinkernsbypass are BSD-2-Clause (their `LICENSE`
files), which is compatible with RetroArch's GPLv3. `src/hook/kgsl.h` is a
Linux UAPI header, GPL-2.0 WITH Linux-syscall-note as its SPDX line says.
