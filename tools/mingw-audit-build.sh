#!/bin/sh
# Cross-builds retroarch.exe with mingw-w64 solely so the thread-read
# audit can walk the Windows-only thread entries and the D3D11/D3D12
# frame contexts, which are compiled out of the Linux binary. The
# result is an audit subject, not a shippable build: freetype, Cg and
# XAudio are off (their import libraries are not in the cross
# toolchain), tools/mingw_audit_stubs/ papers over headers missing
# from older mingw-w64 (d3dkmthk.h; the unversioned d3d12 serialize
# PFN; three D3D interface names mapped to IUnknown - layout is
# irrelevant, only call edges are read), and imm32 is appended by
# (Makefile.win now links imm32 and sets HAVE_OVERLAY/HAVE_RPNG
# itself, so this script no longer compensates for either.)
# Usage: tools/mingw-audit-build.sh   (then:)
#   python3 tools/thread_read_audit.py --binary retroarch.exe \
#     --objdump x86_64-w64-mingw32-objdump
set -eu
STUBS=$(cd "$(dirname "$0")"/mingw_audit_stubs && pwd)
export CFLAGS="-I$STUBS -include $STUBS/d3d12_fixup.h \
-DID3D11TracingDevice=IUnknown -DID3D12PipelineLibrary=IUnknown \
-DID3D12InfoQueue=IUnknown -DD3D12_MAX_TEXTURE_DIMENSION_2_TO_EXP=17"
MK="make -f Makefile.win HOST_PREFIX=x86_64-w64-mingw32- \
CXX=x86_64-w64-mingw32-gcc HAVE_D3D11=1 HAVE_D3D12=1 HAVE_WDMKS=1 HAVE_FREETYPE=0 \
HAVE_CG=0 HAVE_XAUDIO=1"
$MK -j"$(nproc)"
ls -la retroarch.exe
