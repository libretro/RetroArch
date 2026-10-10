#!/bin/bash
# The exact cases on d3d11 under Wine: software and D3D11 frames, the
# stock chain, a preset that reads the frame where it is (pass.slangp)
# and one that keeps history (history.slangp), threaded and not. Every
# pixel of each view must be its source pixel, as in run.py.
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
export WINEDEBUG=-all WINEDLLOVERRIDES="d3dcompiler_47=n"

failed=0
for threaded in false true; do
   for hw in off d3d11; do
      for preset in none pass history; do
         d="$out/$threaded-$hw-$preset"
         rm -rf "$d"; mkdir -p "$d"
         printf 'video_views_test_map = "none"\nvideo_views_test_hw = "%s"\nvideo_views_test_pattern = "noise"\n' \
            "$hw" > "$d/opts.cfg"
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
            [ "$preset" = none ] || echo 'video_shader_enable = "true"'
         } > "$d/ra.cfg"
         set -- --config "$(winpath "$d/ra.cfg")" \
            -L "$(winpath "$out/video_views_libretro.dll")" --verbose \
            --max-frames=180 --max-frames-ss \
            --max-frames-ss-path="$(winpath "$d/shot.png")"
         [ "$preset" = none ] || set -- "$@" \
            --set-shader="$(winpath "$here/$preset.slangp")"
         timeout 180 wine "$exe_dir/retroarch.exe" "$@" >"$d/run.log" 2>&1 || true
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
echo "$failed failed"
[ "$failed" = 0 ]
