#!/bin/bash
# The Vulkan driver through changes of the swapchain's size, end to end.
#
# A swapchain is replaced whenever the window changes size, and the
# driver used to destroy and rebuild everything it had built on it:
# every pipeline, the render passes, the samplers, the command and
# descriptor pools, the vertex and uniform buffers, the textures the
# core's frames are uploaded into. Only the views of the swapchain's
# images and the framebuffers on them depend on its size, and those are
# now all that is rebuilt when nothing but the size has changed.
#
# A retroarch built with Vulkan and X11 is run on a virtual display
# with Mesa's software Vulkan driver and the small core of
# samples/input/windows_wine_smoke. Its window is resized four times -
# with the menu closed, opened, and closed again - and a screenshot is
# taken after each. Twice: once as it is, and once with
# RETROARCH_VULKAN_REBUILD_ALL=1, which rebuilds everything every time
# as before. What is checked:
#
# - as it is, every one of the four size changes rebuilt the
#   framebuffers only; with the switch, every one rebuilt everything;
# - the core's picture after each resize with the menu closed is, pixel
#   for pixel, the picture the full rebuild draws;
# - the core saw its button before the resizes and after them, and
#   RetroArch quit on Escape;
# - where the Vulkan validation layers are installed, they have
#   nothing to say about either run.
#
# The screenshots with the menu open are not compared: resized under
# the menu, the full rebuild loses the core's last frame (the texture
# it is in is one of the things rebuilt) and shows whatever the new
# one holds; keeping the texture keeps the frame.
#
# Usage: run.sh [directory with retroarch]   (default: the repo root)
# Needs: Xvfb, xdotool, xwd, python3, a C compiler, a software Vulkan
# driver (Mesa's lavapipe).
set -u

here=$(cd "$(dirname "$0")" && pwd)
root=${1:-$(cd "$here/../../.." && pwd)}
work=$(mktemp -d)
XVFB=
trap 'kill $XVFB 2>/dev/null; rm -rf "$work"' EXIT

for t in Xvfb xdotool xwd python3; do
   command -v $t >/dev/null 2>&1 || { echo "no $t" >&2; exit 1; }
done
[ -x "$root/retroarch" ] || { echo "no retroarch in $root" >&2; exit 1; }

${CC:-cc} -shared -fPIC -O1 -Wall \
   -I"$here/../../../libretro-common/include" \
   -o "$work/smoke_core.so" "$here/../../input/windows_wine_smoke/smoke_core.c" -lpthread \
   || { echo "the core did not build" >&2; exit 1; }

export DISPLAY=:90
Xvfb :90 -screen 0 1280x720x24 > "$work/xvfb.log" 2>&1 &
XVFB=$!
sleep 1

cat > "$work/retroarch.cfg" <<CFG
video_driver = "vulkan"
input_driver = "x"
input_joypad_driver = "udev"
menu_driver = "rgui"
audio_driver = "null"
video_threaded = "true"
video_fullscreen = "false"
video_vsync = "false"
pause_nonactive = "false"
config_save_on_exit = "false"
frontend_log_level = "0"
confirm_quit = "false"
video_window_save_positions = "false"
CFG

layers=
if ls /usr/share/vulkan/explicit_layer.d/*khronos_validation* >/dev/null 2>&1; then
   layers=VK_LAYER_KHRONOS_validation
fi

hold() {
   local now
   now=$(xdotool search --name "RetroArch" 2>/dev/null | tail -1)
   [ -n "$now" ] && xdotool windowfocus "$now" 2>/dev/null
   xdotool keydown "$1"; sleep 0.15; xdotool keyup "$1"
   sleep 0.7
}

play() {  # $1: name. Returns 0 if RetroArch quit by itself.
   local name=$1 wid= app i
   ( cd "$root" && VK_INSTANCE_LAYERS=$layers \
        exec ./retroarch --verbose -c "$work/retroarch.cfg" \
        -L "$work/smoke_core.so" ) > "$work/$name.log" 2>&1 &
   app=$!
   for i in $(seq 1 80); do
      wid=$(xdotool search --name "RetroArch" 2>/dev/null | head -1)
      [ -n "$wid" ] && break
      sleep 0.5
   done
   if [ -z "$wid" ]; then
      kill $app 2>/dev/null
      return 2
   fi
   sleep 3
   xdotool mousemove 300 300 2>/dev/null
   hold z
   xdotool windowsize "$wid" 800 500; sleep 2
   xwd -root -silent > "$work/$name.1.xwd" 2>/dev/null
   xdotool windowsize "$wid" 640 480; sleep 2
   hold F1; sleep 1
   xdotool windowsize "$wid" 900 600; sleep 2
   hold F1; sleep 1
   xdotool windowsize "$wid" 700 400; sleep 2
   xwd -root -silent > "$work/$name.2.xwd" 2>/dev/null
   hold z
   hold Escape
   for i in $(seq 1 30); do kill -0 $app 2>/dev/null || break; sleep 0.5; done
   if kill -0 $app 2>/dev/null; then
      kill $app 2>/dev/null; sleep 1; kill -9 $app 2>/dev/null
      return 1
   fi
   return 0
}

count() { sed 's/\x1b\[[0-9;]*m//g' "$1" | grep -ac "$2"; }

# the pixels of two screenshots, without their headers
same_picture() {
   python3 - "$1" "$2" <<'PY'
import struct, sys
def pixels(path):
    d = open(path, 'rb').read()
    h = struct.unpack('>25I', d[:100])
    return d[h[0] + h[19] * 12:]
a, b = pixels(sys.argv[1]), pixels(sys.argv[2])
# and not a black screen: the core's picture is in it
sys.exit(0 if a == b and a.count(b'\x40\x30\x20') > 1000 else 1)
PY
}

failures=0
check() {  # $1 name, $2 how play ended, $3 want size-only, $4 want everything
   local name=$1 rc=$2 only all b errs
   only=$(count "$work/$name.log" 'Swapchain changed in size only')
   all=$(count "$work/$name.log" 'Swapchain changed: everything')
   b=$(count "$work/$name.log" 'smoke core\] joypad B pressed')
   errs=$(grep -ac 'VUID\|Validation Error' "$work/$name.log")
   if [ "$rc" = 0 ] && [ "$only" = "$3" ] && [ "$all" = "$4" ] && [ "$b" = 2 ] && [ "$errs" = 0 ]; then
      echo "[pass] $name: $only size changes rebuilt the framebuffers only, $all rebuilt everything;" \
           "the core saw B twice; quit on Escape;" \
           "validation layers: $([ -n "$layers" ] && echo "no errors" || echo "not installed")"
   else
      echo "[FAIL] $name: quit by itself: $([ "$rc" = 0 ] && echo yes || echo no)," \
           "framebuffers only $only (want $3), everything $all (want $4)," \
           "B pressed $b (want 2), validation errors $errs (want 0)"
      sed 's/\x1b\[[0-9;]*m//g' "$work/$name.log" | grep -a 'Vulkan\]\|ERROR\|VUID' | tail -20
      failures=$((failures + 1))
   fi
}

play "size only"; rc=$?
check "size only" $rc 4 0
RETROARCH_VULKAN_REBUILD_ALL=1 play "everything rebuilt"; rc=$?
check "everything rebuilt" $rc 0 4

for n in 1 2; do
   if same_picture "$work/size only.$n.xwd" "$work/everything rebuilt.$n.xwd"; then
      echo "[pass] screenshot $n: the picture is the one the full rebuild draws"
   else
      echo "[FAIL] screenshot $n: the picture differs from the full rebuild's, or is not there"
      failures=$((failures + 1))
   fi
done

if [ "$failures" != 0 ]; then
   echo "FAIL vulkan_resize: $failures"
   exit 1
fi
echo "PASS vulkan_resize"
