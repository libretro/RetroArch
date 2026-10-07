#!/bin/sh
# Compile the translation units whose contents depend on which codecs or
# which platform a build selects, across the combinations real builds
# actually use.
#
# The ordinary CI jobs each build one configuration, so a file that
# compiles for the configuration they build is green even when it cannot
# compile for a console's. This has broken the tree: giving WAV a
# streaming mixer type made it a user of machinery guarded on the
# compressed codecs, which every desktop build has and Wii U - WAV and
# nothing else - does not. Nothing caught it until the console job ran.
#
# Nothing here links or runs; it is a syntax and declaration check, which
# is the class of breakage this is for.

set -e
cd "$(dirname "$0")/../../.."

CC=${CC:-gcc}
INC="-I. -Ilibretro-common/include -Ideps -Ideps/rcheevos/include -Iinput/include"
WARN="-Wall -Wno-unused-function -Werror=implicit-function-declaration"
# gcc 14 rejects these outright and older gcc only warns; a lane on
# an older compiler has to reject them too, or a mismatched callback
# passes here and fails on the toolchains that build the platform.
WARN="$WARN -Werror=incompatible-pointer-types -Werror=int-conversion"
BASE="-DRARCH_INTERNAL -DHAVE_AUDIOMIXER -DHAVE_THREADS -DHAVE_CONFIGFILE -DHAVE_MENU"
# A console lane that keeps the host identity takes the Linux branch
# of every #if ladder and proves nothing, so this sits above the
# lanes rather than among them.
HOSTOFF="-U__linux__ -U__gnu_linux__ -Ulinux -U__unix__ -U__unix -Uunix"

fail=0

# A failing compile's output, errors first: a file with a page of
# warnings ahead of its one error pushed the error past the cut, which
# left a failing lane reporting nothing to act on.
show_out() {
   if echo "$1" | grep -q ": error: "; then
      echo "$1" | grep ": error: " | sed 's/^/      /' | head -8
   else
      echo "$1" | sed 's/^/      /' | head -12
   fi
}

check() {
   name="$1"; shift
   defs="$1"; shift
   bad=0
   for tu in "$@"; do
      if ! out=$($CC $WARN $INC $BASE $defs -fsyntax-only "$tu" 2>&1); then
         echo "FAIL  $name"
         echo "      $tu"
         show_out "$out"
         fail=1; bad=1
      fi
   done
   [ "$bad" = 1 ] || echo "ok    $name"
}

AUDIO="libretro-common/formats/audio_transfer.c libretro-common/audio/audio_mixer.c"

echo "== audio codec combinations =="
# the sets real platform makefiles select, plus each codec alone and none
check "wav only (ctr/psp/wiiu)"   "-DHAVE_RWAV"                                   $AUDIO
check "wav+mod (gx/ngc/wii)"      "-DHAVE_RWAV -DHAVE_RMODTRACKER"                $AUDIO
check "mod+vorbis (emscripten)"   "-DHAVE_RMODTRACKER -DHAVE_RVORBIS"             $AUDIO
check "wav+vorbis (vita)"         "-DHAVE_RWAV -DHAVE_RVORBIS"                    $AUDIO
check "wav+vorbis+mp3+mod (nx)"   "-DHAVE_RWAV -DHAVE_RVORBIS -DHAVE_RMP3 -DHAVE_RMODTRACKER" $AUDIO
check "mod+mp3+vorbis (dos)"      "-DHAVE_RMODTRACKER -DHAVE_RMP3 -DHAVE_RVORBIS" $AUDIO
check "vorbis only"               "-DHAVE_RVORBIS"                                $AUDIO
check "flac only"                 "-DHAVE_RFLAC"                                  $AUDIO
check "mp3 only"                  "-DHAVE_RMP3"                                   $AUDIO
check "mod only"                  "-DHAVE_RMODTRACKER"                            $AUDIO
check "aac only"                  "-DHAVE_RAAC"                                   $AUDIO
check "aac+mp4"                   "-DHAVE_RAAC -DHAVE_RMP4"                       $AUDIO
check "opus only"                 "-DHAVE_ROPUS"                                  $AUDIO
check "opus+webm"                 "-DHAVE_ROPUS -DHAVE_RWEBM"                     $AUDIO
check "no codecs"                 ""                                              $AUDIO
check "everything" \
  "-DHAVE_RWAV -DHAVE_RVORBIS -DHAVE_RFLAC -DHAVE_RMP3 -DHAVE_RMODTRACKER -DHAVE_RAAC -DHAVE_ROPUS -DHAVE_RMP4 -DHAVE_RWEBM" \
  $AUDIO


# mem_stats selects one platform backend out of a dozen, and its includes
# and its bodies must both follow that one choice.  When they were two
# separate chains, a platform matching an early arm while still defining
# __unix__ - Orbis and Emscripten do, and DJGPP does - got its own arm's
# includes and a different arm's code, and three console jobs failed on
# symbols the arm had never included a header for.
#
# The SDK headers are not here, so an arm whose header is missing cannot
# be compiled.  What can be checked anywhere is that the only thing it
# complains about is that header: a diagnostic naming a symbol that
# belongs to some other arm means the chains have drifted apart again.
echo
echo "== mem_stats platform arms =="

# The SDKs are not on a runner, so each arm is compiled against a stub
# header supplying only the handful of symbols that arm names. That does
# not prove the real SDK matches the stub - it proves the arm's own code
# is well formed and that every symbol it uses is one it has arranged to
# have. Three console failures reached CI as missing or wrong headers -
# 3ds.h absent, gccore.h guessed, SYSMEM1_SIZE taken from the SDK when
# it is RetroArch's own - and each would have shown up here.
STUBS="$(dirname "$0")/stubs"
# host headers are the real ones and deprecate things the target's do not
ARMWARN="$WARN -Wno-deprecated-declarations"

# A platform's own video driver, which the arm lanes above do not reach:
# they check that one file's headers resolve, not that the driver a
# console actually draws with still compiles. Settings a driver reads
# while answering the frontend have moved into the frame descriptor more
# than once, and each time these drivers were the ones left behind -
# nothing in CI compiles them.
#
# @extra carries whatever headers the driver needs that the runner has;
# a lane whose headers are absent says so and is not a failure.
platform_video() {
   name="$1"; defs="$2"; extra="$3"; tu="$4"; probe="$5"
   for pr in $probe; do
      if [ ! -f "$pr" ]; then
         echo "skip  $name (no $pr)"
         return
      fi
   done
   if ! out=$($CC $WARN $extra $INC $BASE $defs -fsyntax-only "$tu" 2>&1); then
      echo "FAIL  $name"
      echo "      $tu"
      show_out "$out"
      fail=1
   else
      echo "ok    $name"
   fi
}

arm() {
   name="$1"; stub="$2"; defs="$3"
   inc="$INC"
   [ -n "$stub" ] && inc="-I$STUBS/$stub $INC"
   if ! out=$($CC $ARMWARN $inc $defs -fsyntax-only \
         libretro-common/memory/mem_stats.c 2>&1); then
      echo "FAIL  $name"
      printf '%s\n' "$out" | sed 's/^/      /' | head -8
      fail=1
   elif [ -n "$out" ]; then
      echo "FAIL  $name (warnings)"
      printf '%s\n' "$out" | sed 's/^/      /' | head -6
      fail=1
   else
      echo "ok    $name"
   fi
}

arm "3ds"        ctr        "-D_3DS"
arm "gamecube"   gx         "-DGEKKO"
arm "wii"        gx         "-DGEKKO -DHW_RVL"
arm "vita"       ""         "-DVITA"
arm "switch"     libnx      "-DHAVE_LIBNX"
arm "orbis"      orbis      "-DORBIS"
arm "ps3"        ps3        "-D__PSL1GHT__ -DHAVE_MEMINFO"
arm "ps2"        ""         "-DPS2"
arm "emscripten" emscripten "-D__EMSCRIPTEN__"

# The consoles that carry the cleanroom crypto, keychain and TLS client
# (3DS, Vita, Switch), each in its own shape with the SDK stand-ins the
# random source and the socket layer need. Vita's and the 3DS's socket
# layers are the SDKs' own, so the TLS client itself is compiled only
# in the Switch shape here; those two builds are their workflows' to
# link.
CRYPTO_SET="libretro-common/crypto/crypto.c libretro-common/crypto/kdf.c libretro-common/crypto/pk.c libretro-common/crypto/x509.c libretro-common/file/keychain.c"
TLS_SET="$CRYPTO_SET libretro-common/net/net_socket_ssl_retro.c network/tls_log.c"
CONSOLE_NET="-DHAVE_CRYPTO -DHAVE_KEYCHAIN -DHAVE_SSL -DHAVE_RETROSSL -DHAVE_NETWORKING -DHAVE_THREADS -DHAVE_CONFIGFILE -DRARCH_INTERNAL"
console_set() {
   cs_name="$1"; cs_inc="$2"; cs_defs="$3"; shift 3
   for cs_tu in "$@"; do
      check "$cs_name: $(basename $cs_tu)" "$HOSTOFF $cs_inc $CONSOLE_NET $cs_defs $CDECL" "$cs_tu"
   done
}
console_set "3ds crypto"    "-Itools/platform_stubs/ctr"  "-D_3DS -DARM11"                    $CRYPTO_SET
console_set "vita crypto"   "-Itools/platform_stubs/vita" "-DVITA"                            $CRYPTO_SET
console_set "switch tls"    "-I$STUBS/libnx"              "-DHAVE_LIBNX -DSWITCH -D__SWITCH__" $TLS_SET
# The built-in NFS client, which the 3DS, Vita, Switch and Wii U
# build, under the defines their Makefiles set (RARCH_CONSOLE for the
# console sleep and the 3DS transfer cap, Vita's legacy socket layer):
# Wii U against its own wiiu/include, Vita against stubs of vitasdk's
# socket headers.
NFS_SET="libretro-common/net/net_nfs3.c libretro-common/vfs/vfs_implementation_nfs.c libretro-common/vfs/vfs_prefetch.c"
NFSD="-DHAVE_RETRONFS -DHAVE_NFSCLIENT"
console_set "3ds nfs"       "-Itools/platform_stubs/ctr $NFSD"  "-D_3DS -DARM11 -DRARCH_CONSOLE" $NFS_SET
console_set "vita nfs"      "-Itools/platform_stubs/vita $NFSD" "-DVITA -DRARCH_CONSOLE -DHAVE_SOCKET_LEGACY" $NFS_SET
console_set "switch nfs"    "-I$STUBS/libnx $NFSD"              "-DHAVE_LIBNX -DSWITCH -D__SWITCH__ -DRARCH_CONSOLE" $NFS_SET
console_set "wiiu nfs"      "-Iwiiu/include -Itools/platform_stubs/wiiu $NFSD" "-DWIIU -D__WUT__ -DHW_WUP -D__wiiu__ -DRARCH_CONSOLE" $NFS_SET
# The built-in SMB and Kerberos clients, which the 3DS, Vita and Switch
# build on their crypto (the Wii U has no kernel RNG for it to draw on).
SMB_SET="libretro-common/net/net_smb2.c libretro-common/net/net_krb5.c libretro-common/vfs/vfs_implementation_smb.c"
SMBD="-DHAVE_RETROSMB -DHAVE_SMBCLIENT"
console_set "3ds smb"       "-Itools/platform_stubs/ctr $SMBD"  "-D_3DS -DARM11 -DRARCH_CONSOLE" $SMB_SET
console_set "vita smb"      "-Itools/platform_stubs/vita $SMBD" "-DVITA -DRARCH_CONSOLE -DHAVE_SOCKET_LEGACY" $SMB_SET
console_set "switch smb"    "-I$STUBS/libnx $SMBD"              "-DHAVE_LIBNX -DSWITCH -D__SWITCH__ -DRARCH_CONSOLE" $SMB_SET

# The network stack in the old MSVC projects (2005 - 2017), which no job
# here builds: 32-bit Windows, C89, at the Windows floors those projects
# target - VS2005's _WIN32_WINNT 0x0410 and XP's 0x0501. The stack keeps
# to what those compilers and SDKs have: C89, the LL/ULL suffixes code
# they already build uses, Windows entry points resolved at run time,
# and a CA bundle split into parts under MSVC's 64 KiB string limit.
MSVC_OLD_CC=${MSVC_OLD_CC:-i686-w64-mingw32-gcc}
if command -v "$MSVC_OLD_CC" > /dev/null 2>&1; then
   MSVC_OLD_STACK="$CRYPTO_SET libretro-common/net/net_socket_ssl_retro.c network/tls_log.c libretro-common/net/net_smb2.c libretro-common/net/net_krb5.c libretro-common/net/net_nfs3.c libretro-common/vfs/vfs_implementation_smb.c libretro-common/vfs/vfs_implementation_nfs.c libretro-common/vfs/vfs_prefetch.c"
   MSVC_OLD_DEFS="-DRARCH_INTERNAL -DHAVE_NETWORKING -DHAVE_THREADS -DHAVE_CONFIGFILE -DHAVE_CRYPTO -DHAVE_KEYCHAIN -DHAVE_SSL -DHAVE_RETROSSL -DHAVE_SMBCLIENT -DHAVE_RETROSMB -DHAVE_NFSCLIENT -DHAVE_RETRONFS -DLEGACY_WIN32_NO_MIGRATION_NOTE"
   for winnt in 0x0410 0x0501; do
      mo_bad=0
      for tu in $MSVC_OLD_STACK; do
         if ! out=$($MSVC_OLD_CC -std=c89 -ansi -pedantic -Werror=pedantic -Werror=declaration-after-statement -Wno-long-long -Wno-variadic-macros -Werror=implicit-function-declaration -Wall -Wno-unused-function -Wno-overlength-strings \
               -I. -Ilibretro-common/include -D_WIN32_WINNT=$winnt $MSVC_OLD_DEFS -fsyntax-only "$tu" 2>&1) \
               || echo "$out" | grep -q "warning:"; then
            echo "FAIL  msvc-old network stack, _WIN32_WINNT=$winnt"
            echo "      $tu"
            show_out "$out"
            fail=1; mo_bad=1
         fi
      done
      [ "$mo_bad" = 1 ] || echo "ok    msvc-old network stack, _WIN32_WINNT=$winnt"
   done
   # and no single string literal in the CA bundle reaches MSVC's limit
   if python3 - << 'PYEOF'
import re, sys
t = open("libretro-common/net/cacert.h", encoding="latin-1").read()
for body in re.findall(r"\[\] = \{(.*?)\};", t, re.S):
    lits = re.findall(r'"((?:[^"\\]|\\.)*)"', body)
    size = sum(len(l.encode("latin-1").decode("unicode_escape")) for l in lits) + len(lits)
    if size >= 65535:
        sys.exit(1)
PYEOF
   then
      echo "ok    cacert.h parts under MSVC's 64 KiB string limit"
   else
      echo "FAIL  cacert.h has a part at or over MSVC's 64 KiB string limit"; fail=1
   fi
else
   echo "skip  msvc-old network stack (no $MSVC_OLD_CC)"
fi

# Every platform makefile on Makefile.common, without its toolchain: the
# graph's own DEFINES checked for consistency (TLS, SMB, NFS need the
# networking they are built on) and the frontend compiled under exactly
# them. Makefile.retrofw asked for the TLS client with no networking
# and only its own workflow saw the undefined reference.
pm_out=$(tools/platform_makefile_check.sh 2>&1); pm_rc=$?
printf '%s\n' "$pm_out" | sed 's/^ok    /ok    platform makefile: /'
[ $pm_rc -eq 0 ] || { echo "FAIL  platform makefile check"; fail=1; }

# What each console Makefile actually builds, from its own object list:
# no MCP server on any console (a desktop HAVE_MCP option; listed once
# with the networking objects it broke the 3DS), and no crypto library
# on the 24/32 MiB machines.
co_out=$(tools/console_objects_check.sh 2>&1); co_rc=$?
printf '%s\n' "$co_out" | sed 's/^ok    /ok    console objects: /'
[ $co_rc -eq 0 ] || { echo "FAIL  console object check"; fail=1; }

# The OpenDingux family builds with a MIPS toolchain; the crypto and the
# TLS client compile with a MIPS cross compiler when one is installed.
MIPSEL_CC=${MIPSEL_CC:-mipsel-linux-gnu-gcc}
if command -v "$MIPSEL_CC" >/dev/null 2>&1; then
   for tu in $TLS_SET; do
      if out=$($MIPSEL_CC -march=mips32r2 $INC $CONSOLE_NET -DDINGUX -D_GNU_SOURCE -Wall -Werror -Wno-unused-function -fsyntax-only "$tu" 2>&1); then
         echo "ok    dingux mips: $(basename $tu)"
      else
         echo "FAIL  dingux mips: $(basename $tu)"; show_out "$out"; fail=1
      fi
   done
else
   echo "skip  dingux mips (no $MIPSEL_CC)"
fi

platform_video "odroidgo2 video" \
   "-DHAVE_ODROIDGO2 -DHAVE_OPENGL -DHAVE_GLSL" "" \
   gfx/drivers/gl2.c ""
platform_video "gx video" "-DGEKKO -DHW_RVL" "-I$STUBS/gx" \
   gfx/drivers/gx_gfx_libogc.c ""
# The GameCube build takes its display offset from SRAM instead of the
# Wii's configuration; nothing compiled that branch.
platform_video "gx video (GameCube)" "-DGEKKO -DHW_DOL" "-I$STUBS/gx" \
   gfx/drivers/gx_gfx_libogc.c ""
platform_video "switch video" \
   "-DHAVE_LIBNX -DSWITCH -D__SWITCH__" "-I$STUBS/libnx" \
   gfx/drivers/switch_nx_gfx.c ""
# The PSP driver folds its VRAM and cache-alias addresses into pointers,
# which a 64-bit host narrows; the arithmetic is 32-bit on the target.
platform_video "psp1 video" "-DPSP" \
   "-Itools/platform_stubs/psp -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
    -Wdeclaration-after-statement -Werror=declaration-after-statement" \
   gfx/drivers/psp1_gfx.c ""
# The Vita driver, against psp2 stubs carrying the SceGxm declarations it
# names in the shapes the real headers give them. Same narrowing as the
# PSP driver above: its pool addresses are 32-bit on the target.
platform_video "gxm video" "-DVITA -DRARCH_CONSOLE $HOSTOFF" \
   "-Itools/platform_stubs/vita -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast" \
   gfx/drivers/gxm_gfx.c ""
# gl1 on Vita, against a vitaGL stub over the host GL headers. The
# driver's Vita branches are compiled nowhere else, and one of them
# carried a declaration after a statement that only the Vita job saw.
platform_video "vita gl1 video" \
   "-DVITA -DRARCH_CONSOLE -DHAVE_OPENGL1 -DHAVE_OVERLAY -DHAVE_GFX_WIDGETS $HOSTOFF" \
   "-Itools/platform_stubs/vita -Wdeclaration-after-statement \
    -Werror=declaration-after-statement" \
   gfx/drivers/gl1.c /usr/include/GL/gl.h
# The PS2 driver, against gsKit and PS2SDK stubs carrying what it
# names in the shapes the real headers give them. Makefile.ps2 turns on
# the window offset, which the driver reads. This lane is also the only
# one that compiles retro_atomic.h's EE interrupt-mask backend, which
# no other platform selects.
platform_video "ps2 video" \
   "-DPS2 -DRARCH_CONSOLE -DHAVE_WINDOW_OFFSET -DHAVE_RGUI $HOSTOFF" \
   "-Itools/platform_stubs/ps2" gfx/drivers/ps2_gfx.c ""
# The DOS VGA driver, against DJGPP stubs for the DPMI and port I/O it
# uses. Only the DJGPP job compiled it before; DJGPP defines __unix__.
platform_video "dos vga video" \
   "-D__DJGPP__ -DDJGPP -DRARCH_CONSOLE $HOSTOFF -D__unix__" \
   "-Itools/platform_stubs/dos -Wdeclaration-after-statement \
    -Werror=declaration-after-statement" \
   gfx/drivers/vga_gfx.c ""
# The two SDL video drivers. Nothing else here compiled them, which is
# how a field they read through video_info_t went on being read after it
# had been packed away. Each skips where its headers are absent, as the
# dingux lanes below do.
platform_video "sdl2 video" "-DHAVE_SDL2" "-I/usr/include/SDL2" \
   gfx/drivers/sdl2_gfx.c /usr/include/SDL2/SDL.h
platform_video "sdl3 video" "-DHAVE_SDL3" "-I/usr/include/SDL3" \
   gfx/drivers/sdl3_gfx.c /usr/include/SDL3/SDL.h
platform_video "dingux video"   "-DDINGUX" "-I/usr/include/SDL" \
   gfx/drivers/sdl_dingux_gfx.c /usr/include/SDL/SDL.h
platform_video "rs90 video"     "-DDINGUX -DRS90" "-I/usr/include/SDL" \
   gfx/drivers/sdl_rs90_gfx.c /usr/include/SDL/SDL.h
# libdrm's exynos module and its G2D headers, which distros ship only
# where the hardware exists. A runner building libdrm with -Dexynos=true
# turns this on.
platform_video "wiiu gx2 video" "-DWIIU $HOSTOFF" \
   "-Iwiiu/include -Iwiiu" gfx/drivers/gx2_gfx.c ""
platform_video "hub75 video" "" "" gfx/drivers/hub75_gfx.c ""
platform_video "openvg video" "-DHAVE_VG -DHAVE_EGL" \
   "-Itools/platform_stubs/openvg" gfx/drivers/vg.c ""
platform_video "exynos video"   "-DHAVE_EXYNOS" "-I/usr/include/libdrm" \
   gfx/drivers/exynos_gfx.c /usr/include/exynos/exynos_fimg2d.h

# The Direct3D drivers, against the DirectX headers the tree vendors in
# gfx/include/dxsdk. Six drivers no other lane compiles, and they read
# the same frame and draw descriptors every other video driver does.
#
# A mingw-w64 cross compiler is all these need beyond the tree; where
# there is none the lanes say so rather than fail.
MINGW_CC=${MINGW_CC:-x86_64-w64-mingw32-gcc}
d3d_video() {
   name="$1"; defs="$2"; tu="$3"
   if ! command -v "$MINGW_CC" > /dev/null 2>&1; then
      echo "skip  $name (no $MINGW_CC)"
      return
   fi
   if ! out=$($MINGW_CC $WARN -isystemgfx/include/dxsdk $INC $BASE $defs \
         -fsyntax-only "$tu" 2>&1); then
      echo "FAIL  $name"
      echo "      $tu"
      show_out "$out"
      fail=1
   else
      echo "ok    $name"
   fi
}

# The window and message pump files as the MSVC 2005 job sees them: it
# builds for _WIN32_WINNT 0x0400 against an SDK that declares what came
# with Windows XP only from 0x0501 up. mingw-w64 declares some of that
# regardless - WM_INPUT is one - so a use outside a _WIN32_WINNT check
# passes every other lane and breaks that job, which is not GitHub's.
# tools/platform_stubs/win32_old/old_sdk.h takes those names away again.
win32_old() {
   name="$1"; tu="$2"
   if ! command -v "$MINGW_CC" > /dev/null 2>&1; then
      echo "skip  $name (no $MINGW_CC)"
      return
   fi
   if ! out=$($MINGW_CC $WARN $INC -Igfx/include $BASE -D_WIN32 \
         -D_WIN32_WINNT=0x0400 -DHAVE_DINPUT \
         -include tools/platform_stubs/win32_old/old_sdk.h \
         -fsyntax-only "$tu" 2>&1); then
      echo "FAIL  $name"
      echo "      $tu"
      show_out "$out"
      fail=1
   else
      echo "ok    $name"
   fi
}
win32_old "win32-old: ui_win32"      ui/drivers/ui_win32.c
win32_old "win32-old: win32_common"  gfx/common/win32_common.c

D3DDEFS="-DHAVE_D3D -DHAVE_RGUI -DHAVE_OVERLAY"
d3d_video "d3d8 video"    "$D3DDEFS -DHAVE_D3D8"  gfx/drivers/d3d8.c
d3d_video "d3d9 video, Cg"   "$D3DDEFS -DHAVE_D3D9"  gfx/drivers/d3d9cg.c
d3d_video "d3d9 video, HLSL" "$D3DDEFS -DHAVE_D3D9"  gfx/drivers/d3d9hlsl.c
d3d_video "d3d10 video"   "$D3DDEFS -DHAVE_D3D10" gfx/drivers/d3d10.c
d3d_video "d3d11 video"   "$D3DDEFS -DHAVE_D3D11" gfx/drivers/d3d11.c
d3d_video "d3d12 video"   "$D3DDEFS -DHAVE_D3D12" gfx/drivers/d3d12.c
d3d_video "gdi video"     "-DHAVE_RGUI -DHAVE_OVERLAY -DHAVE_GDI" \
   gfx/drivers/gdi_gfx.c
# The same drivers without threads, as a --disable-threads build has
# them: a call into the video thread wrapper outside HAVE_THREADS has no
# definition there, so it compiles as an implicit declaration and the
# link fails. BASE defines HAVE_THREADS; the -U after it wins.
d3d_video "d3d8 video, no threads"   "$D3DDEFS -DHAVE_D3D8 -UHAVE_THREADS" \
   gfx/drivers/d3d8.c
d3d_video "d3d9 video, Cg, no threads"   "$D3DDEFS -DHAVE_D3D9 -UHAVE_THREADS" \
   gfx/drivers/d3d9cg.c
d3d_video "d3d9 video, HLSL, no threads" "$D3DDEFS -DHAVE_D3D9 -UHAVE_THREADS" \
   gfx/drivers/d3d9hlsl.c
d3d_video "d3d10 video, no threads"  "$D3DDEFS -DHAVE_D3D10 -UHAVE_THREADS" \
   gfx/drivers/d3d10.c
d3d_video "d3d11 video, no threads"  "$D3DDEFS -DHAVE_D3D11 -UHAVE_THREADS" \
   gfx/drivers/d3d11.c
d3d_video "d3d12 video, no threads"  "$D3DDEFS -DHAVE_D3D12 -UHAVE_THREADS" \
   gfx/drivers/d3d12.c

# The context drivers, which no job here compiles either. Each answers
# the frontend's window and size questions, so a change to what those
# hand back reaches all of them at once. Every lane names the headers it
# wants and says so when the runner has none.
platform_video "ctx: null" "" "" gfx/drivers_context/gfx_null_ctx.c ""
platform_video "ctx: x11 gl" "-DHAVE_X11 -DHAVE_OPENGL -DHAVE_EGL" "" \
   gfx/drivers_context/x_ctx.c /usr/include/X11/Xlib.h
platform_video "ctx: x11 egl" "-DHAVE_X11 -DHAVE_EGL -DHAVE_OPENGLES" "" \
   gfx/drivers_context/xegl_ctx.c /usr/include/X11/Xlib.h
platform_video "ctx: x11 vulkan" "-DHAVE_VULKAN -DHAVE_X11 -DHAVE_XCB" "" \
   gfx/drivers_context/x_vk_ctx.c \
   "/usr/include/vulkan/vulkan.h /usr/include/X11/Xlib-xcb.h"
platform_video "ctx: khr display" "-DHAVE_VULKAN" "" \
   gfx/drivers_context/khr_display_ctx.c /usr/include/vulkan/vulkan.h
platform_video "ctx: kms/gbm" "-DHAVE_EGL -DHAVE_OPENGL -DHAVE_KMS -DHAVE_GBM" \
   "-I/usr/include/libdrm" gfx/drivers_context/drm_ctx.c /usr/include/gbm.h
# vivante_fbdev is left out: it calls Vivante's own fbCreateWindow and
# fbGetDisplayByIndex, which only that vendor's EGL headers declare, and
# a lane that let those go implicit would pass what the SDK rejects.
platform_video "ctx: mali fbdev" "-DHAVE_EGL -DHAVE_OPENGLES" "" \
   gfx/drivers_context/mali_fbdev_ctx.c /usr/include/EGL/egl.h
platform_video "ctx: opendingux fbdev" "-DHAVE_EGL -DHAVE_OPENGLES -DDINGUX" "" \
   gfx/drivers_context/opendingux_fbdev_ctx.c /usr/include/EGL/egl.h
platform_video "ctx: sdl1 gl" "-DHAVE_SDL -DHAVE_OPENGL" "-I/usr/include/SDL" \
   gfx/drivers_context/sdl1_gl_ctx.c /usr/include/SDL/SDL.h
platform_video "ctx: sdl2 gl" "-DHAVE_SDL2 -DHAVE_OPENGL" "-I/usr/include/SDL2" \
   gfx/drivers_context/sdl2_gl_ctx.c /usr/include/SDL2/SDL.h
# osmesa is the software context a headless build selects, and
# qb/config.libs.sh can enable it from a pkg-config probe alone.
platform_video "ctx: osmesa" "-DHAVE_OSMESA -DHAVE_OPENGL" "" \
   gfx/drivers_context/osmesa_ctx.c /usr/include/GL/osmesa.h
# The Wayland protocol headers are generated into gfx/common/wayland by
# configure, so these lanes run only in a tree that has been configured
# with Wayland.
WL_H="/usr/include/wayland-client.h gfx/common/wayland/content-type-v1.h"
platform_video "ctx: wayland gl" "-DHAVE_WAYLAND -DHAVE_EGL -DHAVE_OPENGL" "" \
   gfx/drivers_context/wayland_ctx.c "$WL_H"
platform_video "ctx: wayland vulkan" "-DHAVE_WAYLAND -DHAVE_VULKAN" "" \
   gfx/drivers_context/wayland_vk_ctx.c "$WL_H /usr/include/vulkan/vulkan.h"
# The shared Wayland code the two contexts above call into. It was the
# only Wayland unit no lane compiled, which is how four declarations
# after a statement sat in it. Its libdecor half is a second lane: the
# function pointers that half calls through are declared under
# HAVE_LIBDECOR_H *and* HAVE_DYLIB, so a lane naming only the first
# proves nothing, and the header libdecor.h itself comes from a
# libdecor-0 include directory the probe checks for.
WLC89="-Wdeclaration-after-statement -Werror=declaration-after-statement"
platform_video "wayland common" "-DHAVE_WAYLAND -DHAVE_EGL -DHAVE_OPENGL" \
   "$WLC89" gfx/common/wayland_common.c "$WL_H"
platform_video "wayland common: libdecor" \
   "-DHAVE_WAYLAND -DHAVE_EGL -DHAVE_OPENGL -DHAVE_LIBDECOR_H -DHAVE_DYLIB" \
   "-I/usr/include/libdecor-0 $WLC89" \
   gfx/common/wayland_common.c "$WL_H /usr/include/libdecor-0/libdecor.h"
platform_video "wayland common: backport" \
   "-DHAVE_WAYLAND -DHAVE_EGL -DHAVE_OPENGL -DHAVE_WAYLAND_BACKPORT=1" \
   "$WLC89" gfx/common/wayland_common.c "$WL_H"
# webOS swaps its own shim in for half of the common Wayland functions,
# and the Wayland context reaches it through prototypes of its own, so
# the shim and the context each get a lane. The shim's webOS protocol
# headers are wayland-scanner output of LG's webos-wayland-extensions
# (Apache-2.0), which the SDK generates at build time; stubs/webos keeps
# a copy laid out so the shim's ../../gfx/common/wayland includes find it.
WEBOS="-DWEBOS -DWEBOS_APP_ID=\"com.retroarch\" -DHAVE_WAYLAND -DHAVE_EGL"
WEBOS="$WEBOS -DHAVE_OPENGLES"
platform_video "ctx: wayland gl (webos)" "$WEBOS" "" \
   gfx/drivers_context/wayland_ctx.c "$WL_H"
platform_video "webos wayland shim" "$WEBOS" \
   "-I$STUBS/webos/gfx/common" \
   input/common/wayland_common_webos.c "$WL_H"

arm "win32"      win32      "-D_WIN32 -D_WIN32_WINNT=0x0600"
arm "win32-old"  win32      "-D_WIN32 -D_WIN32_WINNT=0x0400"
arm "macos"      apple      "-D__APPLE__"
arm "ios"        apple      "-D__APPLE__ -DTARGET_OS_IPHONE=1"

# The GL core driver without slang: its HDR encode is slang's, and state
# for it once sat outside the slang guards, which only the C89 lane -
# built without slang - ever compiled.
platform_video "glcore without slang" "-DHAVE_OPENGL -DHAVE_OPENGL_CORE" \
   "" gfx/drivers/gl3.c "/usr/include/GL/gl.h"
platform_video "glcore with slang" \
   "-DHAVE_OPENGL -DHAVE_OPENGL_CORE -DHAVE_SLANG -DHAVE_SHADERPIPELINE" \
   "" gfx/drivers/gl3.c "/usr/include/GL/gl.h"

# The CoreText rasterizer and the font driver's Apple branch, against
# stand-ins for the SDK headers it includes: no runner builds it but the
# Apple jobs, so a change to the rasterizer interface reached them first.
CORETEXT="$HOSTOFF -D__APPLE__ -D__MACH__ -DHAVE_CORETEXT"
platform_video "macos coretext rasterizer" "$CORETEXT" "-I$STUBS/apple" \
   gfx/drivers_font_renderer/coretext.c ""
platform_video "ios coretext rasterizer" "$CORETEXT -DTARGET_OS_IPHONE=1" \
   "-I$STUBS/apple" gfx/drivers_font_renderer/coretext.c ""
platform_video "macos font driver" "$CORETEXT" "-I$STUBS/apple" \
   gfx/font_driver.c ""
arm "dos/djgpp"  ""         "-D__DJGPP__ -D__unix__"
arm "linux"      ""         ""


# The video drivers and the threaded wrapper compile under more than
# one API and one threading model, and the desktop job builds one of
# each. The OpenGL ring's sync objects went in under a guard the
# desktop build satisfied and the GLES builds did not, and every GLES
# target - Android, Emscripten, Vita, PS4 - went red at once; a
# threadless build had been broken for longer than anyone noticed, on
# calls that compile against a prototype and fail only at link.
# These lanes are the ones a desktop build does not exercise.
echo
echo "== video drivers: API and threading combinations =="

GLINC="-Igfx/include"
GLDEFS="-DHAVE_OPENGL -DHAVE_GLSL -DHAVE_FBO -DHAVE_REWIND -DHAVE_OVERLAY"
check "gl2: GLES2 (android/emscripten/vita/ps4)" \
   "$GLDEFS -DHAVE_OPENGLES -DHAVE_OPENGLES2 -DHAVE_EGL $GLINC" gfx/drivers/gl2.c
check "gl2: GLES3" \
   "$GLDEFS -DHAVE_OPENGLES -DHAVE_OPENGLES3 -DHAVE_EGL $GLINC" gfx/drivers/gl2.c
check "gl2: desktop" \
   "$GLDEFS $GLINC" gfx/drivers/gl2.c
GL3DEFS="-DHAVE_OPENGL -DHAVE_OPENGL_CORE -DHAVE_SLANG -DHAVE_GLSLANG -DHAVE_SPIRV_CROSS -DHAVE_REWIND -DHAVE_OVERLAY -Ideps/SPIRV-Cross"
check "gl3: desktop" \
   "$GL3DEFS $GLINC" gfx/drivers/gl3.c
check "gl3: GLES3" \
   "$GL3DEFS -DHAVE_OPENGLES -DHAVE_OPENGLES3 -DHAVE_EGL $GLINC" gfx/drivers/gl3.c
# gl3 is not in the C89 job's configuration, so its C89 lane is here.
# The flags the Makefile's C89_BUILD lane uses, _GNU_SOURCE included:
# -ansi hides the C99 math names otherwise.
C89="-std=c89 -ansi -pedantic -Werror=pedantic -Werror=declaration-after-statement -Wno-long-long -Wno-variadic-macros -D_GNU_SOURCE -DC89_BUILD"
check "gl3: desktop, C89" \
   "$GL3DEFS $GLINC $C89" gfx/drivers/gl3.c

# Without threads, in C89: audio_driver.h and rthreads.h both reach the
# tasks and the audio driver, and a typedef each declared on its own is
# one C89 refuses twice. make C89_BUILD=1 --disable-threads stopped at
# task_save.c on it; no job builds that shape. check() always adds
# HAVE_THREADS, so this lane spells its defines out.
NOTHREADS_BASE=$(echo "$BASE" | sed 's/-DHAVE_THREADS//')
nt_bad=0
for tu in tasks/task_save.c tasks/task_audio_mixer.c \
          audio/audio_driver.c audio/audio_thread_wrapper.c; do
   if ! out=$($CC $WARN $INC $NOTHREADS_BASE $C89 -fsyntax-only "$tu" 2>&1); then
      echo "FAIL  audio + tasks: no threads, C89"
      echo "      $tu"
      show_out "$out"
      fail=1; nt_bad=1
   fi
done
[ "$nt_bad" = 1 ] || echo "ok    audio + tasks: no threads, C89"

# WASAPI is in the Windows C89_BUILD lane, which no job here builds, and
# its error-string helper serves only the microphone path, so both halves
# are compiled: C89 flags, with and without HAVE_MICROPHONE, unused
# functions as errors. A real compile rather than -fsyntax-only, which
# stops before GCC looks for unused static functions.
win32_audio() {
   name="$1"; defs="$2"; tu="$3"
   if ! command -v "$MINGW_CC" > /dev/null 2>&1; then
      echo "skip  $name (no $MINGW_CC)"
      return
   fi
   if ! out=$($MINGW_CC $WARN -Wunused-function -Werror=unused-function \
         $INC $BASE $C89 \
         $defs -c -o /dev/null "$tu" 2>&1); then
      echo "FAIL  $name"
      echo "      $tu"
      show_out "$out"
      fail=1
   else
      echo "ok    $name"
   fi
}
win32_audio "wasapi: C89, microphone" "-DHAVE_WASAPI -DHAVE_MICROPHONE" \
   audio/drivers/wasapi.c
win32_audio "wasapi: C89, no microphone" "-DHAVE_WASAPI" \
   audio/drivers/wasapi.c
check "hw ring: OpenGL only" \
   "-DHAVE_OPENGL $GLINC" gfx/video_thread_hw.c
check "hw ring: GLES only" \
   "-DHAVE_OPENGLES -DHAVE_OPENGLES2 $GLINC" gfx/video_thread_hw.c
check "hw ring: Vulkan only" \
   "-DHAVE_VULKAN $GLINC" gfx/video_thread_hw.c
check "hw ring: no hardware API" \
   "$GLINC" gfx/video_thread_hw.c

# Widget state lock: the worker draws widgets while the main thread
# writes them, and the wrapper yields the lock around its waits.
check "widgets: state lock" "$GLINC -DHAVE_GFX_WIDGETS" \
   gfx/gfx_widgets.c gfx/video_thread_wrapper.c gfx/video_driver.c runloop.c

# Without threads: the wrapper is not built, and callers must compile
# against the macro stand-ins, not the wrapper's prototypes. Syntax
# only here; the link is the threadless job's.
NOTHREADS=$(echo "$BASE" | sed 's/-DHAVE_THREADS//')
check_nothreads() {
   name="$1"; shift
   defs="$1"; shift
   bad=0
   for tu in "$@"; do
      if ! out=$($CC $WARN $INC $NOTHREADS $defs -fsyntax-only "$tu" 2>&1); then
         echo "FAIL  $name"
         echo "      $tu"
         show_out "$out"
         fail=1; bad=1
      fi
   done
   [ "$bad" = 1 ] || echo "ok    $name"
}
# Console rgui: the menu driver's GEKKO and DINGUX shapes have their
# own framebuffer dimensions, aspect handling and pixel converters;
# a syntax pass keeps refactors honest for both. GEKKO needs one
# tiny libogc stub header (tools/platform_stubs/gekko).
# The Android OpenSL driver compiles nowhere else; a stub SLES header
# set (tools/platform_stubs/android) keeps its lock-free write path
# and hardened teardown under a syntax gate.
# OpenAL: not in the audit build's configure. Compile-only AL stubs,
# first in the include order, so the lane needs no system
# libopenal-dev and behaves the same on every runner. Covers the
# eventcount park path (threads on).
check "audio: openal" "-Itools/platform_stubs/openal -DHAVE_AL -DHAVE_THREADS -Wdeclaration-after-statement -Werror=declaration-after-statement" audio/drivers/openal.c

# psp/psp2 audio: index-pair SPSC + eventcount park, one driver per
# platform; compile-only sce stubs.
check "psp: psp_audio" "-Itools/platform_stubs/psp -DPSP -DHAVE_THREADS -Wdeclaration-after-statement -Werror=declaration-after-statement" audio/drivers/psp_audio.c
check "vita: psp2_audio" "-Itools/platform_stubs/vita -DVITA -DHAVE_THREADS -Wdeclaration-after-statement -Werror=declaration-after-statement" audio/drivers/psp2_audio.c

# features_cpu.c on every statically linked platform. It owns
# cpu_features_get_time_usec() and retro_sleep_until_us(); the wait's
# generic loop calls retro_sleep_us, a per-platform macro from
# retro_timers.h everywhere but Windows and Darwin, so the branch that
# only the console toolchains compile is compiled here, against
# hermetic stubs of each SDK header the file pulls. The host compiler
# predefines __linux__ and friends; a lane that keeps them takes the
# Linux branch of every #if ladder and proves nothing, so these shed
# them. PSP gets _POSIX_C_SOURCE for the host libc's nanosleep, which
# pspsdk's newlib declares unconditionally.
FCPU_TU=libretro-common/features/features_cpu.c
check "features_cpu: 3ds"        "$HOSTOFF -Itools/platform_stubs/ctr -D_3DS -D__3DS__ -DARM11 -DRARCH_CONSOLE" $FCPU_TU
check "features_cpu: gekko"      "$HOSTOFF -Itools/platform_stubs/gekko -DGEKKO -DHW_RVL -DRARCH_CONSOLE" $FCPU_TU
check "features_cpu: wiiu"       "$HOSTOFF -Itools/platform_stubs/wiiu -DWIIU -DRARCH_CONSOLE" $FCPU_TU
check "features_cpu: psp"        "$HOSTOFF -Itools/platform_stubs/psp -DPSP -D_POSIX_C_SOURCE=199309L -DRARCH_CONSOLE" $FCPU_TU
check "features_cpu: vita"       "$HOSTOFF -Itools/platform_stubs/vita -DVITA -DRARCH_CONSOLE" $FCPU_TU
check "features_cpu: ps3"        "$HOSTOFF -Itools/platform_stubs/ps3 -D__PS3__ -DRARCH_CONSOLE" $FCPU_TU
check "features_cpu: psl1ght"    "$HOSTOFF -Itools/platform_stubs/psl1ght -D__PS3__ -D__PSL1GHT__ -DRARCH_CONSOLE" $FCPU_TU
check "features_cpu: emscripten" "$HOSTOFF -Itools/platform_stubs/emscripten -D__EMSCRIPTEN__ -DEMSCRIPTEN" $FCPU_TU

# Shared menu and video code under the console platform macros. These
# files are compiled by every desktop job, but the blocks inside them
# that a console selects are not, and a change to a struct those blocks
# touch is green everywhere until a console job runs. The frame and
# draw descriptors they read need no SDK header, so these lanes take
# the stubs already here and shed the host identity like the ones
# above.
MENUGFX="gfx/video_driver.c gfx/gfx_display.c gfx/gfx_widgets.c \
 gfx/gfx_thumbnail.c gfx/gfx_surface.c gfx/video_thread_wrapper.c \
 menu/drivers/xmb.c menu/drivers/ozone.c menu/drivers/materialui.c"
MG_BASE="-DHAVE_XMB -DHAVE_OZONE -DHAVE_MATERIALUI -DHAVE_RGUI \
 -DHAVE_GFX_WIDGETS -DHAVE_OVERLAY -DHAVE_CONFIGFILE -DRARCH_CONSOLE \
 -DRARCH_INTERNAL -Iinput/include"
check "console menu+gfx: vita"    "$HOSTOFF $MG_BASE -Itools/platform_stubs/vita -DVITA" $MENUGFX
check "console menu+gfx: wiiu"    "$HOSTOFF $MG_BASE -Itools/platform_stubs/wiiu -DWIIU" $MENUGFX
check "console menu+gfx: psl1ght" "$HOSTOFF $MG_BASE -Itools/platform_stubs/psl1ght -D__PS3__ -D__PSL1GHT__" $MENUGFX
# video_thread_wrapper.c reaches for libctru's own allocator on 3DS,
# and a guessed linearMemAlign() would let this lane pass what the real
# SDK rejects, so the 3DS lane takes the rest.
check "console menu+gfx: 3ds"     "$HOSTOFF $MG_BASE -Itools/platform_stubs/ctr -D_3DS -D__3DS__" \
   $(echo "$MENUGFX" | tr ' ' '\n' | grep -v video_thread_wrapper)
check "console menu+gfx: gekko"   "$HOSTOFF $MG_BASE -Itools/platform_stubs/gekko -DGEKKO -DHW_RVL" $MENUGFX

# Platform drivers no other job compiles. Each of these reaches a
# compiler only inside its own console toolchain - griffin includes it
# behind that platform's #ifdef and the desktop makefiles never build
# it - so a C89 slip or a missing declaration in one sits until the
# console job runs, which is how a mixed declaration lived in
# gx_joypad_libogc.c. These are the ones the stubs already in the tree can
# reach, with one stub added for the pair of PSP input drivers - the
# 3ds audio drivers want fifteen more symbols whose libctru signatures
# cannot be checked from here, and a stub that guessed one would let a
# lane pass what the real SDK rejects, so they are left out.
# The rest need headers no stub here supplies. psp1_gfx and
# dispserv_android already have lanes of their own - the latter gains
# the C89 declaration check below rather than a second lane.
#
# Same warnings as the driver lanes above, C89 declarations included,
# because that is the rule these files are furthest from anyone
# checking. GEKKO takes libogc's headers before its stub, which
# carries only what libogc does not: devkitPro's libogc at a fixed
# release, fetched once (LIBOGC_DIR names a checkout to use instead).
CDECL="-Wdeclaration-after-statement -Werror=declaration-after-statement"
LIBOGC_TAG=v3.1.0
LIBOGC_DIR=${LIBOGC_DIR:-${TMPDIR:-/tmp}/libogc-$LIBOGC_TAG}
if [ ! -f "$LIBOGC_DIR/gc/gccore.h" ]; then
   rm -rf "$LIBOGC_DIR"
   git -c advice.detachedHead=false clone -q --depth 1 \
      --branch "$LIBOGC_TAG" https://github.com/devkitPro/libogc \
      "$LIBOGC_DIR" || { echo "FAIL  libogc $LIBOGC_TAG could not be fetched"; fail=1; }
fi
GEKKO_INC="-I$LIBOGC_DIR/gc -Itools/platform_stubs/gekko -DHAVE_LIBOGC"
check "gekko: gx_input"        "$HOSTOFF $GEKKO_INC -DGEKKO -DHW_RVL -DRARCH_CONSOLE $CDECL" input/drivers/gx_input.c
check "gekko: gx_joypad"       "$HOSTOFF $GEKKO_INC -DGEKKO -DHW_RVL -DRARCH_CONSOLE $CDECL" input/drivers_joypad/gx_joypad_libogc.c
check "gekko: mem2_manager"    "$HOSTOFF $GEKKO_INC -DGEKKO -DHW_RVL -DRARCH_CONSOLE $CDECL" libretro-common/memory/mem2_manager_libogc.c
check "gekko: dispserv_gx"     "$HOSTOFF $GEKKO_INC -DGEKKO -DHW_RVL -DRARCH_CONSOLE $CDECL" gfx/display_servers/dispserv_gx_libogc.c
check "gamecube: dispserv_gx"  "$HOSTOFF $GEKKO_INC -DGEKKO -DHW_DOL -DRARCH_CONSOLE $CDECL" gfx/display_servers/dispserv_gx_libogc.c
check "gekko: dispserv_gx_modes" "$HOSTOFF $GEKKO_INC -DGEKKO -DHW_RVL -DRARCH_CONSOLE $CDECL" gfx/display_servers/dispserv_gx_modes.c
check "psl1ght: dispserv_ps3"  "$HOSTOFF -Itools/platform_stubs/ps3 -D__PS3__ -D__PSL1GHT__ -DRARCH_CONSOLE $CDECL" gfx/display_servers/dispserv_ps3.c gfx/display_servers/dispserv_ps3_modes.c
check "3ds: ctr_input"         "$HOSTOFF -Itools/platform_stubs/ctr -D_3DS -D__3DS__ -DARM11 -DRARCH_CONSOLE $CDECL" input/drivers/ctr_input.c
PSP_DEFS="$HOSTOFF -Itools/platform_stubs/psp -DPSP -D_POSIX_C_SOURCE=199309L -DRARCH_CONSOLE"
check "psp: psp_input"         "$PSP_DEFS $CDECL" input/drivers/psp_input.c
check "psp: psp_joypad"        "$PSP_DEFS $CDECL" input/drivers_joypad/psp_joypad.c
check "orbis: ps4_audio"       "$HOSTOFF -Itools/platform_stubs/orbis -DORBIS $CDECL" audio/drivers/ps4_audio.c
check "qnx: alsa_qsa"          "$HOSTOFF -Itools/platform_stubs/qnx -D__QNX__ $CDECL" audio/drivers/alsa_qsa.c
check "android: vfs saf"       "$HOSTOFF -Itools/platform_stubs/android -DANDROID $CDECL" libretro-common/vfs/vfs_implementation_saf.c
check "android: play delivery" "$HOSTOFF -Itools/platform_stubs/android -DANDROID $CDECL" play_feature_delivery/play_feature_delivery.c
# the Android build carries the built-in NFS client and its backend
# (HAVE_RETRONFS in pkg/android/phoenix-common/jni/Android.mk) and the
# prefetcher both network backends read through
check "android: nfs client"    "$HOSTOFF -Itools/platform_stubs/android -DANDROID -DHAVE_NFSCLIENT -DHAVE_RETRONFS -DHAVE_THREADS -DHAVE_NETWORKING $CDECL" libretro-common/net/net_nfs3.c
check "android: nfs backend"   "$HOSTOFF -Itools/platform_stubs/android -DANDROID -DHAVE_NFSCLIENT -DHAVE_RETRONFS -DHAVE_THREADS -DHAVE_NETWORKING -DRARCH_INTERNAL $CDECL" libretro-common/vfs/vfs_implementation_nfs.c
check "android: vfs prefetch"  "$HOSTOFF -Itools/platform_stubs/android -DANDROID -DHAVE_THREADS $CDECL" libretro-common/vfs/vfs_prefetch.c

# The Android build carries the cleanroom network stack (Android.mk:
# HAVE_CRYPTO, keychain, RETROSSL, RETROSMB, RETRONFS) and is compiled
# by the NDK's clang for aarch64. The same compiler family and target
# here, with the aarch64 cross headers standing in for the NDK's, over
# every translation unit of the stack: the AES/PMULL path takes the
# +crypto target attribute this way, which gcc lanes never exercise.
ANDROID_CLANG=${ANDROID_CLANG:-clang}
ANDROID_SYSINC=${ANDROID_SYSINC:-/usr/aarch64-linux-gnu/include}
android_clang() {
   name="$1"; defs="$2"; tu="$3"
   if ! command -v "$ANDROID_CLANG" > /dev/null 2>&1 || [ ! -d "$ANDROID_SYSINC" ]; then
      echo "skip  $name (no $ANDROID_CLANG or $ANDROID_SYSINC)"
      return
   fi
   if ! out=$($ANDROID_CLANG --target=aarch64-linux-gnu -isystem "$ANDROID_SYSINC" \
         -Wall -Werror -Wno-unused-function -D__ANDROID__ -DANDROID $INC $defs \
         -fsyntax-only "$tu" 2>&1); then
      echo "FAIL  $name"
      echo "      $tu"
      show_out "$out"
      fail=1
   else
      echo "ok    $name"
   fi
}
ANDROID_NET="-DHAVE_CRYPTO -DHAVE_KEYCHAIN -DHAVE_SSL -DHAVE_RETROSSL -DHAVE_SMBCLIENT -DHAVE_RETROSMB -DHAVE_NFSCLIENT -DHAVE_RETRONFS -DHAVE_NETWORKING -DHAVE_THREADS -DHAVE_CONFIGFILE -DRARCH_INTERNAL -D_GNU_SOURCE"
for tu in libretro-common/crypto/crypto.c libretro-common/crypto/kdf.c libretro-common/crypto/pk.c \
      libretro-common/crypto/x509.c libretro-common/file/keychain.c libretro-common/net/net_socket_ssl_retro.c \
      libretro-common/net/net_smb2.c libretro-common/net/net_krb5.c libretro-common/net/net_nfs3.c \
      libretro-common/vfs/vfs_implementation_smb.c libretro-common/vfs/vfs_implementation_nfs.c \
      libretro-common/vfs/vfs_prefetch.c network/tls_log.c; do
   android_clang "android clang/aarch64: $(basename $tu)" "$ANDROID_NET" "$tu"
done
# rwebaudio is its own translation unit in the emscripten build, not
# part of griffin's, and nothing else compiles it at all.
check "emscripten: rwebaudio"  "$HOSTOFF -Itools/platform_stubs/emscripten -D__EMSCRIPTEN__ -DEMSCRIPTEN -DHAVE_RWEBAUDIO $CDECL" audio/drivers/rwebaudio.c
check "emscripten: rwebcam"    "$HOSTOFF -Itools/platform_stubs/emscripten -D__EMSCRIPTEN__ -DEMSCRIPTEN $CDECL" camera/drivers/rwebcam.c

# Two more that no job here compiles: the S3 cloud-sync backend, which
# only griffin includes and which nothing defines HAVE_S3 for, and the
# Lakka wifi driver, which needs HAVE_LAKKA. Both had a mixed
# declaration. The C89 build cannot see either, because neither is in
# its object list, so S3 is held to the full C89 lane here.
NETDEFS="-DHAVE_NETWORKING -DHAVE_CONFIGFILE -DHAVE_OVERLAY -DHAVE_CHEATS"
check "cloudsync: s3, C89"     "$NETDEFS -DHAVE_CLOUDSYNC -DHAVE_S3 $C89" network/cloud_sync/s3.c
check "lakka: connmanctl"      "$NETDEFS -DHAVE_LAKKA -DHAVE_WIFI $CDECL" network/drivers_wifi/connmanctl.c

# The salamander launchers link a hand-picked subset of libretro-common
# with -DIS_SALAMANDER: rtime.c for rtime_localtime, no features_cpu.c.
# A syntax pass cannot see the link error that a symbol from an
# unlinked TU produces there, so this lane links exactly that object
# set with a host main, under the Vita salamander's defines.
salamander_link() {
   name="$1"; shift
   defs="$1"; shift
   if ! out=$($CC $WARN $INC $BASE $defs -o /tmp/salamander_link_check \
         tools/platform_stubs/salamander_main.c "$@" 2>&1); then
      echo "FAIL  $name"
      show_out "$out"
      fail=1
   else
      echo "ok    $name"
   fi
   rm -f /tmp/salamander_link_check
}
salamander_link "salamander link: rtime.c" \
   "$HOSTOFF -UHAVE_THREADS -Itools/platform_stubs/vita -DVITA -DIS_SALAMANDER -DRARCH_CONSOLE" \
   libretro-common/time/rtime.c

# runloop.c under Android: runloop_idle_wait's looper branch, and every
# other ANDROID block in the file, compiled against hermetic NDK stubs
# (jni, looper, native_activity, sensor, configuration, window) with
# the host identity shed.
check "android: runloop" "$HOSTOFF -DANDROID -Itools/platform_stubs/android" runloop.c
check "android: dispserv" "$HOSTOFF -DANDROID -Itools/platform_stubs/android $CDECL" gfx/display_servers/dispserv_android.c

# The Android arms of rthreads: thread affinity goes to the kernel
# directly there (bionic keeps cpu_set_t behind _GNU_SOURCE), and the
# API 21 arm names pthread_gettid_np, which only bionic declares.
check "android: rthreads (API 21)" "-DHAVE_THREADS -D__ANDROID__ -D__ANDROID_API__=21 -include tools/platform_stubs/android/bionic_pthread_stub.h -Itools/platform_stubs/android" libretro-common/rthreads/rthreads.c
check "android: rthreads (API 19)" "-DHAVE_THREADS -D__ANDROID__ -D__ANDROID_API__=19 -Itools/platform_stubs/android" libretro-common/rthreads/rthreads.c

# The D-Bus, Mutter and RealtimeKit units, and the elevation chain,
# are built on Linux and BSD only, but griffin and other build systems
# may still see the files elsewhere: as a target with no POSIX headers
# at all (MSVC, consoles), each must reduce to its gate and include
# none of them.
check "no-posix: D-Bus/Mutter/RealtimeKit units" "$HOSTOFF -DHAVE_DYLIB -Itools/platform_stubs/no_posix" \
   gfx/common/dbus_runtime.c gfx/common/dbus_common.c \
   gfx/common/mutter_displayconfig.c \
   frontend/thread_elevation.c frontend/thread_elevation/rtkit.c \
   frontend/thread_elevation/eevdf.c

check "android: opensl" "-DANDROID -DHAVE_OPENSL -Itools/platform_stubs/android -Wdeclaration-after-statement -Werror=declaration-after-statement" audio/drivers/opensl.c

check "gekko: rgui"  "-DGEKKO -DHAVE_MENU -DHAVE_RGUI -Itools/platform_stubs/gekko" menu/drivers/rgui.c
check "dingux: rgui" "-DDINGUX -DHAVE_MENU -DHAVE_RGUI" menu/drivers/rgui.c

# The streaming surface embeds a wrapper node under HAVE_THREADS and
# collapses to direct driver calls without; both shapes compile here,
# along with the thumbnail code that produces into it.
# The VP9 decoder's tile-column threading and the video streams' pooled
# colour conversion sit behind HAVE_THREADS; both shapes compile.
check "vp9/h265/video streams: threads" "-DHAVE_RVP9 -DHAVE_RVP8 -DHAVE_RWEBM -DHAVE_RMP4 -DHAVE_RH264 -DHAVE_RH265" libretro-common/formats/vp9/rvp9.c libretro-common/formats/h265/rh265.c libretro-common/formats/webm/rwebm_video.c libretro-common/formats/mp4/rmp4_video.c libretro-common/formats/image/image_blit_bands.c
check_nothreads "no threads: vp9/h265/video streams" "-DHAVE_RVP9 -DHAVE_RVP8 -DHAVE_RWEBM -DHAVE_RMP4 -DHAVE_RH264 -DHAVE_RH265" libretro-common/formats/vp9/rvp9.c libretro-common/formats/h265/rh265.c libretro-common/formats/webm/rwebm_video.c libretro-common/formats/mp4/rmp4_video.c libretro-common/formats/image/image_blit_bands.c
check "gfx_surface: threads" "$GLINC" gfx/gfx_surface.c gfx/gfx_thumbnail.c
check_nothreads "no threads: gfx_surface" "$GLINC" gfx/gfx_surface.c gfx/gfx_thumbnail.c
check_nothreads "no threads: gl2"          "$GLDEFS $GLINC"       gfx/drivers/gl2.c
check_nothreads "no threads: video_driver" "$GLINC"               gfx/video_driver.c
check_nothreads "no threads: retroarch"    "$GLINC -DHAVE_COMMAND -DHAVE_STDIN_CMD" retroarch.c
check_nothreads "no threads: audio_driver" "$GLINC"               audio/audio_driver.c
check_nothreads "no threads: linux input"  "$GLINC"               input/common/linux_common.c
# input_driver.c's keyboard lane is HAVE_THREADS only, and what it shares
# with the rest of the file has to be declared outside it: a forward
# declaration left inside broke the Emscripten build, the one job that
# compiles this file without threads.
check_nothreads "no threads: input_driver" "$GLINC"               input/input_driver.c
check_nothreads "no threads: widget state lock stand-ins" \
   "$GLINC -DHAVE_GFX_WIDGETS" \
   gfx/gfx_widgets.c gfx/widgets/gfx_widget_volume.c gfx/video_driver.c runloop.c

# The networking files carry threaded machinery of their own - the HTTP
# DNS cache's lock and condition, the task queue's - beside code that is
# compiled either way, so a broadcast or a wait written outside the
# guard is green in every job here and breaks a threadless build. That
# has happened: a DNS cache signal added to net_http_resolve(), which is
# not itself guarded, was caught by a sample rather than by this matrix.
NETDEFS="-DHAVE_NETWORKING $GLINC"
check_nothreads "no threads: net_http"   "$NETDEFS"                 libretro-common/net/net_http.c
check_nothreads "no threads: net_socket" "$NETDEFS"                 libretro-common/net/net_socket.c
check_nothreads "no threads: task_http"  "$NETDEFS"                 tasks/task_http.c
check_nothreads "no threads: cloud sync" "$NETDEFS -DHAVE_CLOUDSYNC" \
   tasks/task_cloudsync.c network/cloud_sync/webdav.c

# The menu and the on-screen widgets are switched separately, so the
# frontend translation units have to hold for all four combinations of
# the two. Every lane above builds with HAVE_MENU, and the shipping jobs
# all enable one or the other, so a declaration that reaches a caller
# only through menu_driver.h or gfx_widgets.h is green everywhere and
# absent from a build with both off.
echo
echo "== menu and widget gates =="
NOMENU=$(echo "$BASE" | sed 's/-DHAVE_MENU//')
check_gates() {
   name="$1"; shift
   defs="$1"; shift
   bad=0
   for tu in "$@"; do
      if ! out=$($CC $WARN $INC $NOMENU $defs -fsyntax-only "$tu" 2>&1); then
         echo "FAIL  $name"
         echo "      $tu"
         show_out "$out"
         fail=1; bad=1
      fi
   done
   [ "$bad" = 1 ] || echo "ok    $name"
}
UITU="retroarch.c runloop.c gfx/video_driver.c gfx/gfx_display.c"
UIDEFS="$GLINC -DHAVE_COMMAND -DHAVE_STDIN_CMD"
check_gates "gates: menu + widgets" "$UIDEFS -DHAVE_MENU -DHAVE_GFX_WIDGETS" $UITU
check_gates "gates: menu only"      "$UIDEFS -DHAVE_MENU"                    $UITU
# The keychain without networking: its menu entry and task must be
# declared outside the networking-only parts of tasks_internal.h.
check_gates "gates: keychain menu, no networking" \
   "$UIDEFS -DHAVE_MENU -DHAVE_KEYCHAIN -DHAVE_CRYPTO -DHAVE_CONFIGFILE" \
   menu/menu_setting.c tasks/task_keychain.c
check_gates "gates: widgets only"   "$UIDEFS -DHAVE_GFX_WIDGETS"             $UITU
check_gates "gates: neither"        "$UIDEFS"                                $UITU

# The optional subsystems switch independently of the menu and of each
# other, and the units that call into them are built either way. Each
# lane drops one subsystem from a fully featured build; the last drops
# all of them, which is what a minimal frontend compiles as.
ALLGATES="-DHAVE_MENU -DHAVE_GFX_WIDGETS -DHAVE_OVERLAY -DHAVE_CHEEVOS -DRC_CLIENT_SUPPORTS_HASH -DHAVE_NETWORKING -DHAVE_RUNAHEAD -DHAVE_DYNAMIC -DHAVE_DYLIB"
FETU="retroarch.c runloop.c command.c gfx/video_driver.c input/input_driver.c configuration.c tasks/task_content.c"
without() { echo "$ALLGATES" | sed "s/$1//g"; }
check_gates "gates: every subsystem" "$UIDEFS $ALLGATES"                            $FETU
check_gates "gates: no overlay"      "$UIDEFS $(without -DHAVE_OVERLAY)"            $FETU
check_gates "gates: no cheevos"      "$UIDEFS $(without '-DHAVE_CHEEVOS -DRC_CLIENT_SUPPORTS_HASH')" $FETU
check_gates "gates: no networking"   "$UIDEFS $(without -DHAVE_NETWORKING)"         $FETU
check_gates "gates: no run-ahead"    "$UIDEFS $(without -DHAVE_RUNAHEAD)"           $FETU
check_gates "gates: no subsystems"   "$UIDEFS"                                      $FETU

# Each video API owns a GPU index setting and its default, under that
# API's own gate, and a build has any mix of APIs: a default defined
# under another API's gate builds only where both are on. Each API that
# a Linux build can have alone is checked alone.
check_gates "gates: GPU index, EGL only"    "$UIDEFS -DHAVE_EGL -DHAVE_OPENGL" configuration.c
check_gates "gates: GPU index, Vulkan only" "$UIDEFS -DHAVE_VULKAN"            configuration.c

# griffin is one translation unit, so a file-local name that is fine on
# its own collides there: p256's fmul against gfx's did, and only the
# MSVC job builds griffin. Both crypto shapes, on Linux, for free.
check_gates "griffin: HAVE_CRYPTO + keychain"  "$UIDEFS -DRARCH_INTERNAL -DHAVE_GRIFFIN=1 -DHAVE_MENU -DHAVE_RGUI -DHAVE_CONFIGFILE -DHAVE_CRYPTO -DHAVE_KEYCHAIN -D_GNU_SOURCE" griffin/griffin.c
check_gates "griffin: HAVE_RETROSSL"            "$UIDEFS -DRARCH_INTERNAL -DHAVE_GRIFFIN=1 -DHAVE_MENU -DHAVE_RGUI -DHAVE_CONFIGFILE -DHAVE_CRYPTO -DHAVE_KEYCHAIN -DHAVE_NETWORKING -DHAVE_SSL -DHAVE_RETROSSL -D_GNU_SOURCE" griffin/griffin.c
check_gates "griffin: HAVE_RETROSMB + cloud sync"  "$UIDEFS -DRARCH_INTERNAL -DHAVE_GRIFFIN=1 -DHAVE_MENU -DHAVE_RGUI -DHAVE_CONFIGFILE -DHAVE_CRYPTO -DHAVE_NETWORKING -DHAVE_SMBCLIENT -DHAVE_RETROSMB -DHAVE_CLOUDSYNC -D_GNU_SOURCE" griffin/griffin.c
# The unity build the Android, Apple, webOS and MSVC targets ship: the
# whole network stack beside every codec, image reader and subsystem
# griffin can carry. A static helper in one translation unit that
# another one also defines (get32 in net_smb2.c and rvorbis.c) only
# collides here, so this lane carries as much of griffin as a host can.
GRIFFIN_FULL="-DRARCH_INTERNAL -DHAVE_GRIFFIN=1 -DHAVE_MENU -DHAVE_RGUI -DHAVE_XMB -DHAVE_OZONE -DHAVE_MATERIALUI -DHAVE_GFX_WIDGETS -DHAVE_OVERLAY -DHAVE_CONFIGFILE -DHAVE_CHEEVOS -DRC_DISABLE_LUA -DHAVE_RUNAHEAD -DHAVE_REWIND -DHAVE_PATCH -DHAVE_XDELTA -DHAVE_CHEATS -DHAVE_SCREENSHOTS -DHAVE_TRANSLATE -DHAVE_LIBRETRODB -DHAVE_DYNAMIC -DHAVE_DYLIB -DHAVE_THREADS -DHAVE_AUDIOMIXER -DHAVE_DSP_FILTER -DHAVE_VIDEO_FILTER -DHAVE_RWAV -DHAVE_RFLAC -DHAVE_RVORBIS -DHAVE_RMP3 -DHAVE_RAC3 -DHAVE_RPNG -DHAVE_RJPEG -DHAVE_RBMP -DHAVE_RTGA -DHAVE_RWEBP -DHAVE_RDDS -DHAVE_RZSTD -DHAVE_7ZIP -DHAVE_CHD -DHAVE_RCHD -DHAVE_IMAGEVIEWER -DHAVE_STB_FONT -DHAVE_NETWORKING -DHAVE_IFINFO -DHAVE_NETWORK_CMD -DHAVE_NETPLAYDISCOVERY -DHAVE_ONLINE_UPDATER -DHAVE_UPDATE_ASSETS -DHAVE_UPDATE_CORES -DHAVE_UPDATE_CORE_INFO -DHAVE_CORE_INFO_CACHE -DHAVE_CLOUDSYNC -DHAVE_S3 -DHAVE_CRYPTO -DHAVE_KEYCHAIN -DHAVE_SSL -DHAVE_RETROSSL -DHAVE_SMBCLIENT -DHAVE_RETROSMB -DHAVE_NFSCLIENT -DHAVE_RETRONFS -DHAVE_LANGEXTRA -DHAVE_SHADERPIPELINE -DHAVE_GLSL -DHAVE_OPENGL -D_GNU_SOURCE"
check_gates "griffin: full unity (every codec + network stack)" "$UIDEFS $GRIFFIN_FULL" griffin/griffin.c
# The SMB show-settings row read a HAVE_MENU-only default under
# HAVE_SMBCLIENT alone; with the built-in client on by default that
# broke every --disable-menu build. configuration.c with SMB and no
# menu, and the same with no menu at all.
check_gates "gates: SMB client, no menu"        "$UIDEFS -DHAVE_CONFIGFILE -DHAVE_NETWORKING -DHAVE_CRYPTO -DHAVE_SMBCLIENT -DHAVE_RETROSMB -D_GNU_SOURCE" configuration.c
check_gates "gates: NFS client, no menu"        "$UIDEFS -DHAVE_CONFIGFILE -DHAVE_NETWORKING -DHAVE_NFSCLIENT -DHAVE_RETRONFS -D_GNU_SOURCE" configuration.c
check_gates "gates: NFS client + menu"          "$UIDEFS -DHAVE_CONFIGFILE -DHAVE_NETWORKING -DHAVE_NFSCLIENT -DHAVE_RETRONFS -DHAVE_MENU -DHAVE_RGUI -D_GNU_SOURCE" configuration.c menu/menu_setting.c menu/menu_displaylist.c menu/cbs/menu_cbs_ok.c menu/cbs/menu_cbs_deferred_push.c menu/cbs/menu_cbs_sublabel.c menu/cbs/menu_cbs_title.c
check_gates "gates: no menu, no SMB"            "$UIDEFS -DHAVE_CONFIGFILE -DHAVE_NETWORKING -D_GNU_SOURCE" configuration.c
check_gates "griffin: HAVE_RETRONFS"            "$UIDEFS -DRARCH_INTERNAL -DHAVE_GRIFFIN=1 -DHAVE_MENU -DHAVE_RGUI -DHAVE_CONFIGFILE -DHAVE_NETWORKING -DHAVE_NFSCLIENT -DHAVE_RETRONFS -D_GNU_SOURCE" griffin/griffin.c
check_gates "griffin: no crypto (console shape)" "$UIDEFS -DRARCH_INTERNAL -DHAVE_GRIFFIN=1 -DHAVE_MENU -DHAVE_RGUI -DHAVE_CONFIGFILE -DHAVE_KEYCHAIN -D_GNU_SOURCE" griffin/griffin.c

# A subsystem's own unit is built only when its gate is on, so each is
# checked with that gate on and the user interface off: the achievement
# and netplay widgets, and the menu entries either drives, are the edges
# where a declaration goes missing.
check_gates "gates: netplay, no UI" \
   "$UIDEFS -DHAVE_NETWORKING" network/netplay/netplay_frontend.c
check_gates "gates: cheevos, no UI" \
   "$UIDEFS -DHAVE_CHEEVOS -DRC_CLIENT_SUPPORTS_HASH" cheevos/cheevos.c
check_gates "gates: run-ahead, no UI" \
   "$UIDEFS -DHAVE_RUNAHEAD -DHAVE_DYNAMIC -DHAVE_DYLIB" runahead.c

# The builtin DSP filters are compiled only by console builds, through
# griffin, and no other job compiles them as C. Each is checked on its
# own, as the consoles build it, in the C89 lane: once with the C99 math
# names glibc declares under _GNU_SOURCE, and once without them, as on
# MSVC, so an undeclared sqrtf() or M_PI fails here.
echo
echo "== dsp filters: C89 =="
DSP=$(ls libretro-common/audio/dsp_filters/*.c)
check "dsp filters: C89" "$C89 -DHAVE_FILTERS_BUILTIN" $DSP
check "dsp filters: C89, no C99 math declarations" \
   "$(echo "$C89" | sed 's/ -D_GNU_SOURCE//') -DHAVE_FILTERS_BUILTIN" $DSP

# The UI companion's shared core, which only a Qt or Cocoa build puts
# in OBJ. A tree configured --disable-qt on a Linux host - which is what
# a desktop job builds - compiles none of it, so a change to a struct it
# reads is green everywhere until one of those jobs runs.
echo "== ui companion: the shared core =="
COMPANION="ui/companion/companion_core.c ui/companion/companion_thumbs.c \
 ui/companion/companion_dock.c"
check "ui companion" "-DHAVE_RGUI -DHAVE_CONFIGFILE" $COMPANION
check "ui companion: no threads" \
   "$(echo "$BASE" | sed 's/ -DHAVE_THREADS//') -DHAVE_RGUI -DHAVE_CONFIGFILE" \
   $COMPANION
# Its own tests, which link the same objects and read the same structs.
# They live outside ui/companion and a change that stops at the three
# files above leaves them behind.
check "ui companion: tests" "-DHAVE_RGUI -DHAVE_CONFIGFILE -Iui/companion" \
   ui/companion/test/companion_core_test.c \
   ui/companion/test/companion_thumbs_test.c \
   ui/companion/test/companion_core_stubs.c

# Video and shader back ends a Linux desktop job does not select. Each
# reads the frame descriptor and the shader parameter block every other
# driver does, and nothing else here compiles them: this configuration
# has no Vulkan, no Cg and none of the framebuffer drivers, so a change
# to those structs is green until a job that does have them runs.
echo "== video back ends this configuration does not build =="
GFXDEFS="-DHAVE_RGUI -DHAVE_OVERLAY -DHAVE_GFX_WIDGETS"
VK_H=/usr/include/vulkan/vulkan.h
platform_video "vulkan back end" "$GFXDEFS -DHAVE_VULKAN" "" \
   gfx/drivers/vulkan.c "$VK_H"
platform_video "vulkan shader back end" "$GFXDEFS -DHAVE_VULKAN" "" \
   gfx/drivers_shader/shader_vulkan.c "$VK_H"
check "cg shader back end" "$GFXDEFS -DHAVE_CG -DHAVE_OPENGL" \
   gfx/drivers_shader/shader_gl_cg.c
check "framebuffer back ends" "$GFXDEFS" \
   gfx/drivers/fpga_gfx.c gfx/drivers/hub75_gfx.c gfx/drivers/sunxi_gfx.c
X11_H=/usr/include/X11/Xlib.h
platform_video "x11 shm back end" "$GFXDEFS -DHAVE_X11" "" \
   gfx/drivers/xshm_gfx.c "$X11_H"
platform_video "xvideo back end" "$GFXDEFS -DHAVE_X11 -DHAVE_XVIDEO" "" \
   gfx/drivers/xvideo.c "$X11_H"

echo "== run-ahead: the dynamic-library gates =="
# The secondary instance exists only with HAVE_DYNAMIC; a build that
# can load libraries but links its core statically (HAVE_DYLIB alone)
# compiles the secondary path out and keeps the callers linking.
RADEFS="-DHAVE_REWIND -DHAVE_RUNAHEAD -DHAVE_DYNAMIC_EXTENSIONS"
check "runahead: neither"          "$RADEFS"                            runahead.c
check "runahead: HAVE_DYLIB only"  "$RADEFS -DHAVE_DYLIB"               runahead.c
check "runahead: HAVE_DYNAMIC"     "$RADEFS -DHAVE_DYNAMIC"             runahead.c
check "runahead: both"             "$RADEFS -DHAVE_DYNAMIC -DHAVE_DYLIB" runahead.c

# The Metal driver, as Objective-C against stand-ins for the parts of the
# macOS SDK it uses (tools/platform_stubs/metal). No runner here has the
# SDK, so the Apple jobs used to be the first to compile it. A selector
# the stand-ins do not declare is an error: a new Metal call needs its
# declaration added there.
if command -v clang > /dev/null 2>&1; then
   if ! out=$(clang -x objective-c -fobjc-runtime=macosx-10.13 -fblocks \
         -fsyntax-only -Wall -Wno-unused-function -Werror=objc-method-access \
         -Werror=unused-but-set-variable $HOSTOFF -D__APPLE__ -D__MACH__ \
         -DHAVE_METAL -Itools/platform_stubs/metal $INC $BASE \
         gfx/drivers/metal.m 2>&1); then
      echo "FAIL  metal video (stubs)"
      echo "      gfx/drivers/metal.m"
      show_out "$out"
      fail=1
   else
      echo "ok    metal video (stubs)"
   fi
else
   echo "skip  metal video (stubs) (no clang)"
fi

# gfx/font_layout.h expands inside each driver's render function, and a
# hook that reads a value before declaring its own locals is C99 the C89
# lane rejects, so these lanes reject it too (the PS2 and Wii U drivers,
# built only by C99 toolchains, opt out). And a
# layout counter a driver has no use for is set but never read. GCC 15
# and clang warn about that; the runner's GCC does not, so these lanes
# ask clang, with the warning made an error.
font_layout() {
   name="$1"; target="$2"; defs="$3"; tu="$4"
   if ! command -v clang > /dev/null 2>&1; then
      echo "skip  $name (no clang)"
      return
   fi
   if ! out=$(clang $target -Werror=unused-but-set-variable \
         -Werror=declaration-after-statement \
         $INC $BASE $defs -fsyntax-only "$tu" 2>&1); then
      echo "FAIL  $name"
      echo "      $tu"
      show_out "$out"
      fail=1
   else
      echo "ok    $name"
   fi
}
# Vulkan from the Khronos headers the tree carries (gfx/include), as the
# real builds use, so the lane runs on a runner without the SDK headers
font_layout "font layout: vulkan" "" "-DHAVE_VULKAN -Igfx/include" gfx/drivers/vulkan.c
font_layout "font layout: gl" "" "-DHAVE_OPENGL" gfx/drivers/gl2.c
font_layout "font layout: gl1" "" "-DHAVE_OPENGL -DHAVE_OPENGL1" \
   gfx/drivers/gl1.c
if command -v sdl2-config > /dev/null 2>&1; then
   font_layout "font layout: sdl2" "" "-DHAVE_SDL2 $(sdl2-config --cflags)" \
      gfx/drivers/sdl2_gfx.c
else
   echo "skip  font layout: sdl2 (no sdl2-config)"
fi
if [ -f /usr/include/SDL3/SDL.h ]; then
   font_layout "font layout: sdl3" "" "-DHAVE_SDL3 -I/usr/include/SDL3" \
      gfx/drivers/sdl3_gfx.c
else
   echo "skip  font layout: sdl3 (no SDL3 headers)"
fi
# The software OSD blitters, where their headers are installed
if [ -f /usr/include/X11/extensions/Xvlib.h ]; then
   font_layout "font layout: xvideo" "" "-DHAVE_X11 -DHAVE_XVIDEO" \
      gfx/drivers/xvideo.c
fi
if [ -f /usr/include/SDL/SDL.h ]; then
   font_layout "font layout: sdl" "" "-DHAVE_SDL -I/usr/include/SDL" \
      gfx/drivers/sdl_gfx.c
fi
if [ -f /usr/include/linux/omapfb.h ]; then
   font_layout "font layout: omap" "" "-DHAVE_OMAP" gfx/drivers/omap_gfx.c
fi
font_layout "font layout: switch" "" \
   "-DHAVE_LIBNX -DSWITCH -D__SWITCH__ -I$STUBS/libnx" \
   gfx/drivers/switch_nx_gfx.c
font_layout "font layout: gxm" "" \
   "-DVITA -DRARCH_CONSOLE $HOSTOFF -Itools/platform_stubs/vita" \
   gfx/drivers/gxm_gfx.c
font_layout "font layout: ps2" "" \
   "-DPS2 -DRARCH_CONSOLE -DHAVE_WINDOW_OFFSET -DHAVE_RGUI $HOSTOFF -Itools/platform_stubs/ps2 \
    -Wno-declaration-after-statement" \
   gfx/drivers/ps2_gfx.c
font_layout "font layout: gx2" "" \
   "-DWIIU $HOSTOFF -Iwiiu/include -Iwiiu -Wno-declaration-after-statement" \
   gfx/drivers/gx2_gfx.c
font_layout "font layout: glcore" "" \
   "-DHAVE_OPENGL -DHAVE_OPENGL_CORE -DHAVE_SLANG" gfx/drivers/gl3.c
if [ -d /usr/x86_64-w64-mingw32/include ]; then
   MINGW_CLANG="--target=x86_64-w64-mingw32 -isystemgfx/include/dxsdk -isystem /usr/x86_64-w64-mingw32/include"
   font_layout "font layout: gdi" "$MINGW_CLANG" \
      "-DHAVE_RGUI -DHAVE_OVERLAY -DHAVE_GDI -D_WIN32_WINNT=0x0601" \
      gfx/drivers/gdi_gfx.c
   font_layout "font layout: d3d8" "$MINGW_CLANG" \
      "-DHAVE_D3D -DHAVE_RGUI -DHAVE_OVERLAY -DHAVE_D3D8" gfx/drivers/d3d8.c
   font_layout "font layout: d3d9 hlsl" "$MINGW_CLANG" \
      "-DHAVE_D3D -DHAVE_RGUI -DHAVE_OVERLAY -DHAVE_D3D9" gfx/drivers/d3d9hlsl.c
   font_layout "font layout: d3d9 cg" "$MINGW_CLANG" \
      "-DHAVE_D3D -DHAVE_RGUI -DHAVE_OVERLAY -DHAVE_D3D9" gfx/drivers/d3d9cg.c
   font_layout "font layout: d3d10" "$MINGW_CLANG" \
      "-DHAVE_D3D -DHAVE_RGUI -DHAVE_OVERLAY -DHAVE_D3D10" gfx/drivers/d3d10.c
   font_layout "font layout: d3d11" "$MINGW_CLANG" \
      "-DHAVE_D3D -DHAVE_RGUI -DHAVE_OVERLAY -DHAVE_D3D11" gfx/drivers/d3d11.c
   font_layout "font layout: d3d12" "$MINGW_CLANG" \
      "-DHAVE_D3D -DHAVE_RGUI -DHAVE_OVERLAY -DHAVE_D3D12" gfx/drivers/d3d12.c
else
   echo "skip  font layout: gdi, d3d8/9/10/11/12 (no MinGW headers)"
fi

echo "== unity build: drivers griffin compiles together =="
# griffin (the MSVC and Xcode builds) includes every driver in one
# translation unit, where two drivers' file-scope names are
# redefinitions; the Makefile build compiles them apart and never sees
# it. GL, GL core and Vulkan share their effect SPIR-V and stock shaders.
UNITY_DIR=$(mktemp -d)
printf '#include "gfx/drivers/gl2.c"\n#include "gfx/drivers/gl3.c"\n#include "gfx/drivers/vulkan.c"\n' \
   > "$UNITY_DIR/unity_gfx.c"
check "unity: gl2 + gl3 + vulkan in one translation unit" \
   "-Igfx/include -DHAVE_OPENGL -DHAVE_OPENGL_CORE -DHAVE_GLSL -DHAVE_VULKAN -DHAVE_SLANG -DHAVE_SHADERPIPELINE -DHAVE_SPIRV_CROSS -DHAVE_GFX_WIDGETS -DHAVE_OVERLAY -DHAVE_X11" \
   "$UNITY_DIR/unity_gfx.c"
rm -rf "$UNITY_DIR"

exit $fail
