#!/bin/sh
# Builds gfx/common/win_dxgi_present.c as Visual C++ before 2015 sees it.
#
# The MSVC 2005 and 2010 griffin builds compile the DXGI bridge with an
# SDK that has no dxgi1_5.h or d3d12.h. Visual C++ cannot run here, so
# this takes the file's _MSC_VER < 1900 branch, compiles it with those
# headers poisoned under old C at NT 4.0's API, and links it with the
# WGL presenter that calls it.
#
# Needs: gcc-mingw-w64-i686.
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
CC=${CC:-i686-w64-mingw32-gcc}
command -v "$CC" >/dev/null 2>&1 || { echo "no $CC" >&2; exit 2; }

src=gfx/common/win_dxgi_present.c
cond='#if defined(_MSC_VER) && _MSC_VER < 1900'
grep -qxF "$cond" "$src" || { echo "FAIL dxgi_bridge_old_sdk_check: $src has no old-SDK branch" >&2; exit 1; }

out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
mkdir "$out/sdk"
for h in dxgi.h dxgi1_2.h dxgi1_3.h dxgi1_4.h dxgi1_5.h dxgi1_6.h d3d12.h; do
   echo "#error $h is not in the old SDK" > "$out/sdk/$h"
done
sed "s/^#if defined(_MSC_VER) \&\& _MSC_VER < 1900\$/#if 1/" "$src" > "$out/bridge.c"
cat > "$out/main.c" <<'EOC'
#include <windows.h>
#include "win_wgl_dxgi.h"
int main(void)
{
   return ra_wgl_dxgi_create(NULL, 640, 480, 0, 0) != NULL;
}
EOC

flags="-std=c89 -ansi -pedantic -Werror=pedantic -Wno-long-long
   -Wno-variadic-macros -Werror=declaration-after-statement
   -Werror=implicit-function-declaration -D_WIN32_WINNT=0x0400
   -I $out/sdk -I gfx/common -I libretro-common/include"

$CC $flags -c "$out/bridge.c" -o "$out/bridge.o"
$CC $flags -c gfx/common/win_wgl_dxgi.c -o "$out/wgl.o"
$CC $flags -c "$out/main.c" -o "$out/main.o"
$CC "$out/main.o" "$out/wgl.o" "$out/bridge.o" -lopengl32 -o "$out/t.exe"
echo "PASS dxgi_bridge_old_sdk_check"
