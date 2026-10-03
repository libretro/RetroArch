#!/bin/bash
# RetroArch's input on an X11 window, end to end.
#
# The Linux counterpart of samples/input/windows_wine_smoke: a retroarch
# built with X11 and OpenGL is run on a virtual display (Xvfb, Mesa's
# software rasteriser) with the X11 input driver, keys are pressed in
# its window, and its log is read. It runs the same small core,
# ../windows_wine_smoke/smoke_core.c, which needs no content and writes
# to the log what reaches it.
#
# Each scenario presses J (bound to nothing) and Z (the default bind for
# B), toggles fullscreen with F, which restarts the video driver,
# presses both again and quits with Escape. What is checked:
#
# - RetroArch quit on the Escape pressed after the restart;
# - the core saw B pressed before the restart and after it, and was
#   given J's key-down and key-up, in that order, both times;
# - every key event it was given came on the thread it runs on: X11's
#   key events are read where the window's events are handled, which
#   with threaded video is the video thread, and they are handed to
#   the frontend's thread before anything acts on them;
# - the input driver was started by the frontend for the X11 window,
#   once the video driver was up - the X11 contexts used to start it
#   themselves - and the frontend never had to fall back to the driver
#   the setting names.
#
# With threaded video, and without.
#
# Usage: run.sh [directory with retroarch]   (default: the repo root)
# Needs: Xvfb, xdotool, a C compiler.
set -u

here=$(cd "$(dirname "$0")" && pwd)
root=${1:-$(cd "$here/../../.." && pwd)}
work=$(mktemp -d)
trap 'kill $XVFB 2>/dev/null; rm -rf "$work"' EXIT

for t in Xvfb xdotool; do
   command -v $t >/dev/null 2>&1 || { echo "no $t" >&2; exit 1; }
done
[ -x "$root/retroarch" ] || { echo "no retroarch in $root" >&2; exit 1; }

${CC:-cc} -shared -fPIC -O1 -Wall \
   -I"$here/../../../libretro-common/include" \
   -o "$work/smoke_core.so" "$here/../windows_wine_smoke/smoke_core.c" -lpthread \
   || { echo "the core did not build" >&2; exit 1; }

export DISPLAY=:95
export LIBGL_ALWAYS_SOFTWARE=1
Xvfb :95 -screen 0 1280x720x24 > "$work/xvfb.log" 2>&1 &
XVFB=$!
sleep 1

write_cfg() {  # $1: video_threaded
   cat > "$work/retroarch.cfg" <<CFG
video_driver = "gl"
input_driver = "x"
input_joypad_driver = "udev"
menu_driver = "rgui"
audio_driver = "null"
video_threaded = "$1"
video_fullscreen = "false"
video_vsync = "false"
pause_nonactive = "false"
config_save_on_exit = "false"
frontend_log_level = "0"
confirm_quit = "false"
video_window_save_positions = "false"
CFG
}

hold() {  # a key, held for a few frames, in whichever window is RetroArch's now
   local now
   now=$(xdotool search --name "RetroArch" 2>/dev/null | tail -1)
   [ -n "$now" ] && xdotool windowfocus "$now" 2>/dev/null
   xdotool keydown "$1"; sleep 0.15; xdotool keyup "$1"
   sleep 0.7
}

play() {  # $1: log file. Returns 0 if RetroArch quit by itself.
   local log=$1 wid= app i
   ( cd "$root" && exec ./retroarch --verbose -c "$work/retroarch.cfg" \
        -L "$work/smoke_core.so" ) > "$log" 2>&1 &
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
   xdotool windowfocus "$wid" 2>/dev/null
   xdotool mousemove 300 300 2>/dev/null
   sleep 1
   hold j
   hold z
   hold f
   sleep 4
   hold j
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

failures=0
scenario() {  # $1 name, $2 threaded
   local name=$1 threaded=$2
   local log="$work/$name.log" try rc b_down j_seq off byname ok
   write_cfg "$threaded"
   # key delivery on a virtual display with no window manager is not
   # exact: a scenario gets a second go before it counts as failed
   for try in 1 2; do
      play "$log"; rc=$?
      b_down=$(count "$log" 'smoke core\] joypad B pressed')
      j_seq=$(sed 's/\x1b\[[0-9;]*m//g' "$log" \
            | grep -ao 'smoke core\] key event: [a-z]* keycode 106' \
            | awk '{printf "%s ", $5}')
      off=$(count "$log" "not the core's")
      byname=$(count "$log" 'did not initialize an input driver')
      ok=yes
      [ "$rc" = 0 ] || ok=no
      [ "$b_down" = 2 ] || ok=no
      [ "$j_seq" = "down up down up " ] || ok=no
      [ "$off" = 0 ] || ok=no
      [ "$byname" = 0 ] || ok=no
      [ "$ok" = yes ] && break
   done
   if [ "$ok" = yes ]; then
      echo "[pass] $name: the core saw B twice and J's down and up twice, on its own thread;" \
           "the frontend started the input driver; quit on Escape"
   else
      echo "[FAIL] $name: quit by itself: $([ "$rc" = 0 ] && echo yes || echo no);" \
           "the core saw B pressed $b_down (want 2), J: ${j_seq:-nothing}(want down up down up)," \
           "key events on a thread that is not the core's: $off (want 0)," \
           "input driver picked by the setting's name: $byname (want 0)"
      sed 's/\x1b\[[0-9;]*m//g' "$log" | grep -av "Playlist\]" | tail -25
      failures=$((failures + 1))
   fi
}

scenario "threaded video" true
scenario "video not threaded" false

if [ "$failures" != 0 ]; then
   echo "FAIL x11_smoke: $failures"
   exit 1
fi
echo "PASS x11_smoke"
