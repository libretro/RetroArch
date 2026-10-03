#!/bin/bash
# RetroArch's input on a Wayland surface, end to end.
#
# The Wayland counterpart of ../x11_smoke: a retroarch built with
# Wayland and OpenGL is run under a compositor (Weston, itself a window
# on a virtual X display, drawing in software), keys are pressed and
# the pointer is moved in it, and its log is read. It runs the same
# small core, ../windows_wine_smoke/smoke_core.c, which needs no
# content and writes to the log what reaches it.
#
# What is checked, with threaded video and without:
#
# - every keyboard and pointer event was handled on the frontend's
#   thread. The seat's events are on an event queue of their own that
#   only the input driver's poll dispatches. On the one queue there
#   used to be, the video side dispatched them too, and with threaded
#   video an event was handled on whichever thread got to it;
# - the core saw B pressed, and J's key-down and key-up in that order,
#   before a fullscreen toggle and after it;
# - with "pause when not in focus" on, after the keyboard focus was
#   taken away and given back the core saw B again: the focus events
#   are on the input queue too, and RetroArch has to hear of the focus
#   coming back;
# - nothing was left on the input queue when it was destroyed;
# - RetroArch quit on Escape.
#
# A last scenario turns the queue off (RETROARCH_WAYLAND_INPUT_QUEUE=0)
# to hold that way of running together too; where its events are
# handled is not checked.
#
# Usage: run.sh [directory with retroarch]   (default: the repo root)
# Needs: weston, Xvfb, xdotool, xclock, a C compiler.
set -u

here=$(cd "$(dirname "$0")" && pwd)
root=${1:-$(cd "$here/../../.." && pwd)}
work=$(mktemp -d)
trap 'kill $WESTON $XVFB 2>/dev/null; rm -rf "$work"' EXIT
XVFB= WESTON=

for t in Xvfb xdotool weston xclock; do
   command -v $t >/dev/null 2>&1 || { echo "no $t" >&2; exit 1; }
done
[ -x "$root/retroarch" ] || { echo "no retroarch in $root" >&2; exit 1; }

${CC:-cc} -shared -fPIC -O1 -Wall \
   -I"$here/../../../libretro-common/include" \
   -o "$work/smoke_core.so" "$here/../windows_wine_smoke/smoke_core.c" -lpthread \
   || { echo "the core did not build" >&2; exit 1; }

export XDG_RUNTIME_DIR="$work/xdg"
mkdir -p "$XDG_RUNTIME_DIR" && chmod 700 "$XDG_RUNTIME_DIR"
export DISPLAY=:93
Xvfb :93 -screen 0 1280x720x24 > "$work/xvfb.log" 2>&1 &
XVFB=$!
sleep 1
weston --backend=x11 --renderer=pixman --width=1024 --height=640 \
   --socket=wl-smoke --no-config > "$work/weston.log" 2>&1 &
WESTON=$!
for i in $(seq 1 60); do
   [ -S "$XDG_RUNTIME_DIR/wl-smoke" ] && break
   sleep 0.25
done
[ -S "$XDG_RUNTIME_DIR/wl-smoke" ] \
   || { echo "the compositor did not start" >&2; tail -5 "$work/weston.log" >&2; exit 1; }
sleep 1
# the compositor's window on the X display: what the keys are sent to
comp=$(xdotool search --onlyvisible --name "" 2>/dev/null | tail -1)
xclock -geometry 100x100+1150+600 > /dev/null 2>&1 &
sleep 1
other=$(xdotool search --class xclock 2>/dev/null | head -1)
[ -n "$comp" ] && [ -n "$other" ] || { echo "no windows to press keys in" >&2; exit 1; }

write_cfg() {  # $1: video_threaded
   cat > "$work/retroarch.cfg" <<CFG
video_driver = "gl"
input_driver = "wayland"
input_joypad_driver = "udev"
menu_driver = "rgui"
audio_driver = "null"
video_threaded = "$1"
video_fullscreen = "false"
video_vsync = "false"
pause_nonactive = "true"
config_save_on_exit = "false"
frontend_log_level = "0"
confirm_quit = "false"
video_window_save_positions = "false"
CFG
}

hold() {  # a key, held for a few frames
   xdotool keydown "$1"; sleep 0.15; xdotool keyup "$1"
   sleep 0.7
}

sweep() {  # the pointer, across the surface
   local i
   for i in $(seq 1 30); do
      xdotool mousemove $((300 + i * 5)) $((250 + i * 3))
      sleep 0.03
   done
}

play() {  # $1: log file. Returns 0 if RetroArch quit by itself.
   local log=$1 app i
   ( cd "$root" && WAYLAND_DISPLAY=wl-smoke LIBGL_ALWAYS_SOFTWARE=1 DISPLAY= \
        exec ./retroarch --verbose -c "$work/retroarch.cfg" \
        -L "$work/smoke_core.so" ) > "$log" 2>&1 &
   app=$!
   for i in $(seq 1 60); do
      grep -aq 'smoke core\] running' "$log" 2>/dev/null && break
      sleep 0.5
   done
   sleep 3
   xdotool windowfocus "$comp" 2>/dev/null
   xdotool mousemove 400 300; sleep 0.5; xdotool click 1; sleep 0.5
   sweep
   hold j
   hold z
   hold f
   sleep 4
   sweep
   hold j
   hold z
   # the keyboard focus goes to another window, and comes back
   xdotool windowfocus "$other" 2>/dev/null; sleep 2.5
   xdotool windowfocus "$comp" 2>/dev/null; sleep 2.5
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
scenario() {  # $1 name, $2 threaded, $3: queue (1 on, 0 off)
   local name=$1 threaded=$2 queue=$3
   local log="$work/$name.log" try rc b_down j_seq line events elsewhere left ok
   write_cfg "$threaded"
   # key delivery through two display servers is not exact: a scenario
   # gets a second go before it counts as failed
   for try in 1 2; do
      RETROARCH_WAYLAND_INPUT_QUEUE=$queue play "$log"; rc=$?
      b_down=$(count "$log" 'smoke core\] joypad B pressed')
      j_seq=$(sed 's/\x1b\[[0-9;]*m//g' "$log" \
            | grep -ao 'smoke core\] key event: [a-z]* keycode 106' \
            | awk '{printf "%s ", $5}')
      line=$(sed 's/\x1b\[[0-9;]*m//g' "$log" | grep -a 'Wayland\] Input events handled' | tail -1)
      events=$(echo "$line" | sed -n 's/.*handled: \([0-9]*\),.*/\1/p')
      elsewhere=$(echo "$line" | sed -n "s/.*frontend's: \([0-9]*\)\..*/\1/p")
      left=$(count "$log" 'still attached')
      ok=yes
      [ "$rc" = 0 ] || ok=no
      [ "$b_down" = 3 ] || ok=no
      [ "$j_seq" = "down up down up " ] || ok=no
      [ "${events:-0}" -gt 20 ] || ok=no
      [ "$left" = 0 ] || ok=no
      if [ "$queue" = 1 ]; then
         [ "${elsewhere:-x}" = 0 ] || ok=no
      fi
      [ "$ok" = yes ] && break
   done
   if [ "$ok" = yes ]; then
      if [ "$queue" = 1 ]; then
         echo "[pass] $name: $events input events, all handled on the frontend's thread;" \
              "the core saw B three times - the last after the focus left and came back -" \
              "and J's down and up twice; quit on Escape"
      else
         echo "[pass] $name: $events input events (${elsewhere:-?} handled on another thread);" \
              "the core saw B three times and J's down and up twice; quit on Escape"
      fi
   else
      echo "[FAIL] $name: quit by itself: $([ "$rc" = 0 ] && echo yes || echo no)," \
           "B pressed $b_down (want 3), J events '$j_seq' (want 'down up down up ')," \
           "input events ${events:-none} (want more than 20)," \
           "handled off the frontend's thread: ${elsewhere:-?} (want 0 with the queue)," \
           "objects left on the input queue: $left (want 0)"
      sed 's/\x1b\[[0-9;]*m//g' "$log" | grep -a 'smoke core\|Wayland\]\|ERROR\|attached' | tail -30
      failures=$((failures + 1))
   fi
}

scenario "threaded video" true 1
scenario "video not threaded" false 1
scenario "threaded video, RETROARCH_WAYLAND_INPUT_QUEUE=0" true 0

if [ "$failures" != 0 ]; then
   echo "FAIL wayland_smoke: $failures scenario(s)"
   exit 1
fi
echo "PASS wayland_smoke"
