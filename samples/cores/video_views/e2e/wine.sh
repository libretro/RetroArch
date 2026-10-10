#!/bin/bash
# The exact cases on d3d11 under Wine: software and D3D11 frames, the
# stock chain, a preset that reads the frame where it is (pass.slangp)
# and one that keeps history (history.slangp), threaded and not. Every
# pixel of each view must be its source pixel, as in run.py. Then the
# rect presets on a D3D11 frame in the corner of a larger texture.
#
# Usage: wine.sh <directory holding a mingw retroarch.exe> <output dir>
#
# Needs wine, Xvfb and x86_64-w64-mingw32-gcc. Wine's own d3dcompiler
# can't compile the d3d11 driver's sprite geometry shader, so a native
# d3dcompiler_47.dll goes next to retroarch.exe. Not run in CI.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../../.." && pwd)
exe_dir=$(cd "$1" && pwd)
out=$(mkdir -p "$2" && cd "$2" && pwd)
winpath() { echo "Z:$(echo "$1" | tr / '\\')"; }

x86_64-w64-mingw32-gcc -O2 -Wall -std=gnu99 \
   -I"$root/libretro-common/include" -I"$root/gfx/include" \
   -o "$out/video_views_libretro.dll" "$here/../video_views_core.c" \
   -shared -static-libgcc -Wl,-Bstatic -lpthread

if [ -z "${DISPLAY:-}" ]; then
   Xvfb :87 -screen 0 1920x1200x24 -nolisten tcp >/dev/null 2>&1 &
   xpid=$!
   trap 'kill $xpid 2>/dev/null || true; wineserver -k 2>/dev/null || true' EXIT
   export DISPLAY=:87
   sleep 2
fi
export WINEDEBUG=-all WINEDLLOVERRIDES="d3dcompiler_47=n" RUN_PY="$here/run.py"

# run_ra <dir> <threaded> <preset path or empty>: RetroArch on the core
# with <dir>/opts.cfg; its screenshot at the last frame in <dir>/shot.png.
run_ra() {
   local d=$1 threaded=$2 preset=$3
   {
      echo 'video_driver = "d3d11"'
      echo 'input_joypad_driver = "null"'
      echo 'audio_driver = "null"'
      echo 'video_font_enable = "false"'
      echo 'menu_enable_widgets = "false"'
      echo 'video_windowed_position_width = "1600"'
      echo 'video_windowed_position_height = "960"'
      echo 'video_scale_integer = "true"'
      echo 'video_smooth = "false"'
      echo 'video_gpu_screenshot = "true"'
      echo 'video_waitable_swapchains = "false"'
      echo 'config_save_on_exit = "false"'
      echo 'global_core_options = "true"'
      echo "core_options_path = \"$(winpath "$d/opts.cfg")\""
      echo "log_dir = \"$(winpath "$d")\""
      echo 'log_to_file = "true"'
      echo "video_threaded = \"$threaded\""
      [ -z "$preset" ] || echo 'video_shader_enable = "true"'
   } > "$d/ra.cfg"
   set -- --config "$(winpath "$d/ra.cfg")" \
      -L "$(winpath "$out/video_views_libretro.dll")" --verbose \
      --max-frames=180 --max-frames-ss \
      --max-frames-ss-path="$(winpath "$d/shot.png")"
   [ -z "$preset" ] || set -- "$@" --set-shader="$(winpath "$preset")"
   timeout 180 wine "$exe_dir/retroarch.exe" "$@" >"$d/run.log" 2>&1 || true
}

failed=0
for threaded in false true; do
   for hw in off d3d11; do
      for preset in none pass history; do
         d="$out/$threaded-$hw-$preset"
         rm -rf "$d"; mkdir -p "$d"
         printf 'video_views_test_map = "none"\nvideo_views_test_hw = "%s"\nvideo_views_test_pattern = "noise"\n' \
            "$hw" > "$d/opts.cfg"
         p=""
         [ "$preset" = none ] || p="$here/$preset.slangp"
         run_ra "$d" "$threaded" "$p"
         if python3 - "$here/run.py" "$d/shot.png" <<'PY'
import importlib.util, sys
spec = importlib.util.spec_from_file_location('run', sys.argv[1])
r = importlib.util.module_from_spec(spec)
spec.loader.exec_module(r)
try:
    w, h, bpp, rows = r.read_png(sys.argv[2])
except OSError:
    print('    no screenshot')
    sys.exit(1)
r.W, r.H = w, h
rows = [bytes(r.rgb_row(rows, bpp, y, 0, w)) for y in range(h)]
errors = r.exact_errors(rows, 3, r.EXACT_VIEWS['3ds'])
for e in errors:
    print('    ' + e)
sys.exit(1 if errors else 0)
PY
         then echo "pass d3d11/$threaded-$hw-$preset"
         else echo "FAIL d3d11/$threaded-$hw-$preset"; failed=$((failed + 1))
         fi
      done
   done
done
# A preset reads a hardware frame in the corner of a larger texture
# where it lies: the core draws its DS frame into a texture of its
# regular maximum. Each preset runs on that and on the software frame,
# which comes on its own; the screenshots must match. Linear filtering
# with border wrap, and a filter the preset leaves to the smooth
# setting, have the frame copied out instead.
for case in rect_nearest:1 rect_nearest_edge:1 rect_nearest_repeat:1 \
            rect_nearest_mirror:1 rect_linear:1 rect_linear_border:0 \
            rect_full:1 rect_two:1 rect_unspec:0; do
   preset=${case%:*}; want=${case#*:}
   for hw in off d3d11; do
      d="$out/rect-$hw-$preset"
      rm -rf "$d"; mkdir -p "$d"
      printf 'video_views_test_map = "ds"\nvideo_views_test_hw = "%s"\nvideo_views_test_pattern = "noise"\nvideo_views_test_max = "large"\n' \
         "$hw" > "$d/opts.cfg"
      run_ra "$d" false "$here/$preset.slangp"
   done
   if python3 - "$out/rect-off-$preset" "$out/rect-d3d11-$preset" "$want" <<'PY'
import importlib.util, os, sys
def png_rows(path):
    spec = importlib.util.spec_from_file_location('run', os.environ['RUN_PY'])
    r = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(r)
    w, h, bpp, rows = r.read_png(path)
    return [bytes(r.rgb_row(rows, bpp, y, 0, w)) for y in range(h)]
try:
    sw = png_rows(sys.argv[1] + '/shot.png')
    hw = png_rows(sys.argv[2] + '/shot.png')
except OSError:
    print('    no screenshot')
    sys.exit(1)
logs = ''
for name in os.listdir(sys.argv[2]):
    if name.endswith('.log'):
        with open(os.path.join(sys.argv[2], name), errors='replace') as f:
            logs += f.read()
in_place = 'Preset reads frames where the core leaves them' in logs
errors = []
if sw != hw:
    errors.append('the hardware frame draws other than the software one')
if in_place != (sys.argv[3] == '1'):
    errors.append('frame %s where it lies' % ('read' if in_place else 'not read'))
for e in errors:
    print('    ' + e)
sys.exit(1 if errors else 0)
PY
   then echo "pass d3d11/rect-$preset"
   else echo "FAIL d3d11/rect-$preset"; failed=$((failed + 1))
   fi
done

echo "$failed failed"
[ "$failed" = 0 ]
