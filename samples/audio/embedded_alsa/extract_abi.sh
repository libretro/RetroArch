#!/bin/sh
# Re-extracts the driver's ABI block into ealsa_abi.h, so abi_check.c
# tests what the driver actually uses rather than a stale copy. Run by
# the Makefile before every build.
set -eu
src="../../../audio/drivers/alsa.c"
out="ealsa_abi.h"
{
   printf '/* Generated from audio/drivers/alsa.c by extract_abi.sh - do not edit.\n'
   printf " * The driver's copy of the kernel PCM ABI, for abi_check.c. */\n"
   printf '#ifndef EALSA_ABI_H\n#define EALSA_ABI_H\n#include <stdint.h>\n#include <sys/ioctl.h>\n\n'
   sed -n '/^typedef unsigned long ealsa_uframes_t;/,/^#define EALSA_IOCTL_WRITEI_FRAMES/p' "$src"
   printf '\n#endif\n'
} > "$out"
