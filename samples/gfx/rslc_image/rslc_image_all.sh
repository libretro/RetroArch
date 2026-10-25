#!/bin/sh
# The image gate over every preset of a slang-shaders checkout outside
# bezel/, one driver after another: rslc_image_check.sh on each preset
# in turn, its verdict line kept in out/gate_<driver>.log, which a run
# cut short picks up from.  This is the run that decides whether a
# driver's video_shader_compiler = auto may pick rslc.
#
#   make                    (EXT=dll on Windows)
#   ./rslc_image_all.sh ../../../retroarch ../../../../slang-shaders vulkan glcore
#
# On Windows, from an MSYS2 MINGW64 shell, with a retroarch.exe built
# with both compilers (./configure --enable-builtinglslang
# --enable-builtinspirv_cross, or an MSVC build), for the D3D drivers
# through Microsoft's d3dcompiler_47:
#
#   make EXT=dll
#   ./rslc_image_all.sh ../../../retroarch.exe ../../../../slang-shaders d3d11 d3d12
#
# Options before the paths are passed to rslc_image_check.sh (-f, -D,
# -t, -c, -b, -w).  Each preset gets PRESET_TIMEOUT seconds (default
# 1200).  The summary counts each verdict; the presets that differ or
# fail are listed at the end, with the captures and logs left in out/.
set -u

here=$(cd "$(dirname "$0")" && pwd)
opts=
while [ $# -gt 0 ]; do
   case $1 in
      -f|-b) opts="$opts $1 $2"; shift 2 ;;
      -*) opts="$opts $1"; shift ;;
      *) break ;;
   esac
done
if [ $# -lt 3 ]; then
   echo "usage: $0 [options] <retroarch> <slang-shaders dir> <driver> [driver ...]" >&2
   exit 2
fi
ra=$1
[ -f "$ra" ] || { echo "$ra: no such RetroArch" >&2; exit 2; }
shaders=$(cd "$2" && pwd)
shift 2
list="$here/out/presets.txt"
mkdir -p "$here/out"
find "$shaders" -name '*.slangp' ! -path '*/bezel/*' | LC_ALL=C sort > "$list"
echo "$(wc -l < "$list") presets under $shaders"

# macOS has no timeout(1) unless coreutils is installed
to="timeout ${PRESET_TIMEOUT:-1200}"
command -v timeout > /dev/null 2>&1 || to=
command -v gtimeout > /dev/null 2>&1 && [ -z "$to" ] && to="gtimeout ${PRESET_TIMEOUT:-1200}"

status=0
for driver in "$@"; do
   log="$here/out/gate_$driver.log"
   touch "$log"
   while read -r preset; do
      grep -qF "	$preset" "$log" && continue
      verdict=$($to "$here/rslc_image_check.sh" \
            -d "$driver" $opts "$ra" "$preset" 2>&1 \
         | grep -E "^(same|DIFF|FAIL|vary)" | head -1)
      [ -n "$verdict" ] || verdict="FAIL  $(basename "$preset" .slangp): no verdict (timeout?)"
      printf '%s\t%s\n' "$verdict" "$preset" >> "$log"
   done < "$list"
   echo "== $driver: $(cut -c1-4 "$log" | sort | uniq -c | tr '\n' ' ')"
   if grep -qE "^(DIFF|FAIL)" "$log"; then
      grep -E "^(DIFF|FAIL)" "$log" | cut -f1
      status=1
   fi
done
exit $status
