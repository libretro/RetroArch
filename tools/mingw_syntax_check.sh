#!/bin/sh
# Syntax-check every C translation unit a Win32 MSYS2 build compiles,
# with the defines that build actually uses, on a mingw cross compiler.
#
# Why this exists: a header edit checked by compiling only the file that
# was edited will pass while another TU that includes the same header
# fails. The include order differs per TU, so a typedef visible in one
# is absent in another (vulkan_win32.h behind an include guard set
# before VK_USE_PLATFORM_WIN32_KHR was defined, for instance). The only
# check that catches that is compiling all of them.
#
# Usage:
#   tools/mingw_syntax_check.sh --consumers-of gfx/common/foo.h [...]
#       compile every .c that directly includes any of the named headers.
#       THIS is the check to run after editing a header.
#   tools/mingw_syntax_check.sh file.c [...]
#       just those files
#   tools/mingw_syntax_check.sh
#       every .c under gfx/ audio/ input/ menu/ and the top level
#
# Prints one line per failing TU and exits non-zero if any failed.

CC="${CC:-x86_64-w64-mingw32-gcc}"
command -v "$CC" >/dev/null 2>&1 || { echo "no $CC" >&2; exit 2; }

FLAGS="-fsyntax-only -std=gnu99 -I. -Ilibretro-common/include -Ideps \
 -Ideps/stb -Igfx/include \
 -DRARCH_INTERNAL -DHAVE_THREADS -DHAVE_CONFIGFILE -DHAVE_MENU \
 -DHAVE_VULKAN -DHAVE_SLANG -DHAVE_SPIRV_CROSS -DHAVE_D3D11 -DHAVE_D3D12 \
 -DHAVE_D3DKMT -DHAVE_DINPUT -DHAVE_XINPUT -DHAVE_WASAPI -DHAVE_XAUDIO \
 -DHAVE_NETWORKING -DHAVE_CHEEVOS -DHAVE_RUNAHEAD -DHAVE_REWIND \
 -DHAVE_OVERLAY -DHAVE_RGUI -DHAVE_XMB -DHAVE_OZONE -DHAVE_MATERIALUI \
 -DHAVE_GFX_WIDGETS -DHAVE_SHADERPIPELINE -DHAVE_CG -DHAVE_GLSL \
 -DHAVE_OPENGL -DHAVE_OPENGL_CORE -DHAVE_DSOUND -DHAVE_AUDIOMIXER \
 -DHAVE_TRANSLATE -DHAVE_SCREENSHOTS -DHAVE_PATCH -DHAVE_BSV_MOVIE"

if [ "$1" = "--consumers-of" ]; then
   shift
   FILES=""
   for h in "$@"; do
      base=$(basename "$h")
      # Linux-only test harnesses under */test/ are not Win32 / C89
      # consumers (they have their own scripts under tools/).
      FILES="$FILES $(grep -rl --include='*.c' "#include.*[\"/]$base\"" . \
         | grep -v '^./deps/' | grep -v '/test/' | sed 's#^\./##')"
   done
   FILES=$(echo $FILES | tr ' ' '\n' | sort -u)
   [ -z "$FILES" ] && { echo "no consumers found for: $*" >&2; exit 2; }
   echo "checking $(echo "$FILES" | wc -l) consumers of: $*"
elif [ $# -gt 0 ]; then
   FILES="$*"
else
   FILES=$(find gfx audio input menu -name '*.c' \
      -not -path '*/deps/*' -not -path '*/include/*' 2>/dev/null; \
      ls *.c 2>/dev/null)
fi

# Pass 2: the linux-c89 CI job's flags, verbatim from the Makefile's
# C89_BUILD block, on the native compiler. -Werror=pedantic and
# -Werror=declaration-after-statement are what MSVC-era C89 compliance
# actually means here, and neither is implied by -std=gnu99 above. A
# file that passes pass 1 and fails this one is exactly what has broken
# the build before: a statement placed between declarations.
C89CC="${C89CC:-gcc}"
C89FLAGS="-fsyntax-only -std=c89 -ansi -pedantic -Werror=pedantic \
 -Wno-long-long -Werror=declaration-after-statement -Wno-variadic-macros \
 -D_GNU_SOURCE -I. -Ilibretro-common/include -Ideps -Ideps/stb -Igfx/include \
 -DRARCH_INTERNAL -DHAVE_THREADS -DHAVE_CONFIGFILE -DHAVE_MENU \
 -DHAVE_NETWORKING -DHAVE_CHEEVOS -DHAVE_RUNAHEAD -DHAVE_REWIND \
 -DHAVE_AUDIOMIXER -DHAVE_OVERLAY -DHAVE_RGUI -DHAVE_XMB -DHAVE_OZONE"

# A Win32-only translation unit cannot be C89-checked with the host gcc:
# <windows.h> is not there, the pass dies on the include and the
# missing-header filter below forgives it - so ui_win32_companion.c was
# silently never checked, and MSVC 2005 found the C89 violations instead.
# Use the 32-bit mingw compiler for those when it is installed (the width
# MSVC 2005 builds, where a shift by 32 is undefined too).
C89CC_WIN32="${C89CC_WIN32:-i686-w64-mingw32-gcc}"
command -v "$C89CC_WIN32" >/dev/null 2>&1 || C89CC_WIN32=""

fail=0; n=0
for f in $FILES; do
   n=$((n+1))
   # Only real errors, not warnings. A missing header named without a
   # path (d3dkmthk.h: an optional platform header this box lacks) is
   # forgiven; a missing header with a path component (../companion/x.h:
   # a project header) is not - that once let a stale include of a
   # deleted header through as "ok".
   err=$($CC $FLAGS "$f" 2>&1 | grep -E ' error: ' \
         | grep -vE 'error: [A-Za-z0-9_.-]+: No such file|error: [A-Za-z0-9_.-]+: file not found' | head -3)
   if [ -n "$err" ]; then
      echo "FAIL [win32] $f"; echo "$err" | sed 's/^/     /'; fail=1
   fi
   # Windows-only translation units cannot take pass 2 with the host
   # gcc (<windows.h> is not there, and -ansi breaks those headers
   # anyway). They still have to satisfy C89 - MSVC 2005 builds them -
   # so use the 32-bit mingw compiler when it is installed: same width
   # as that build, so a shift by 32 shows up too. Without it, say so
   # rather than pass silently, which is how declarations after
   # statements reached master in ui_win32_companion.c.
   cc89="$C89CC"
   case "$f" in
      *win32*|*dinput*|*xinput*|*wasapi*|*xaudio*|*asio*|*dsound*|*d3d*|*dxgi*|*wgl*|*uwp*|*winraw*|*_w.c|*/w_*)
         if [ -n "$C89CC_WIN32" ]; then
            cc89="$C89CC_WIN32"
         else
            echo "skip [c89]  $f (install gcc-mingw-w64-i686 to check it)"
            continue
         fi
         ;;
      *)
         if grep -q '#include <windows\.h>' "$f"; then
            [ -n "$C89CC_WIN32" ] || { echo "skip [c89]  $f (install gcc-mingw-w64-i686)"; continue; }
            cc89="$C89CC_WIN32"
         fi
         ;;
   esac
   err=$($cc89 $C89FLAGS -Wno-overlength-strings "$f" 2>&1 | grep -E ' error: ' \
         | grep -vE 'error: [A-Za-z0-9_.-]+: No such file|error: [A-Za-z0-9_.-]+: file not found' | head -3)
   if [ -n "$err" ]; then
      echo "FAIL [c89]   $f"; echo "$err" | sed 's/^/     /'; fail=1
   fi
done
[ $fail = 0 ] && echo "ok: $n translation units clean (win32 gnu99 + linux c89 pedantic)"
exit $fail
