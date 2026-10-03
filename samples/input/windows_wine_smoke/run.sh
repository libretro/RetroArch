#!/bin/bash
# The Windows build's raw input path, end to end, under Wine.
#
# The other winraw tests take the drivers apart: each includes one
# driver file and stands in for the frontend around it. This one runs
# the frontend itself - retroarch.exe, cross-built with mingw-w64 - on
# a virtual display, with the GDI video driver, the raw input driver
# and the raw input joypad driver, presses keys in its window, and
# reads its log.
#
# Each scenario is: start in the menu, press a key, toggle fullscreen
# (which, with GDI, restarts the video driver), press a key, quit with
# Escape. What is checked is what the log says happened:
#
#   read by the poll, kept      the default. The input driver starts
#                               once and is not restarted with the
#                               video driver; the keyboard still works
#                               after the restart (Escape quits);
#                               reports were read in bulk, none taken
#                               by the thread's pump as messages.
#   RETROARCH_INPUT_KEEP=0      the input driver is restarted with the
#                               video driver, and works after it.
#   video not threaded          as the first, with the window, the pump
#                               and the poll all on one thread.
#   RETROARCH_RAWINPUT_POLL=0   the driver as it was before any of
#                               this: no bulk reads, restarted with
#                               the video driver, still works.
#
# Keys are held for a few frames, as a finger holds one: the frontend
# samples the keyboard's state once a frame, and a press and release
# inside one frame is a state it never sees.
#
# Usage: run.sh [directory with retroarch.exe]   (default: the repo root)
# Needs: wine, Xvfb, xdotool.
set -u

here=$(cd "$(dirname "$0")" && pwd)
root=${1:-$(cd "$here/../../.." && pwd)}
work=$(mktemp -d)
trap 'kill $XVFB 2>/dev/null; timeout 10 wineserver -k 2>/dev/null; rm -rf "$work"' EXIT

WINE=
for w in wine wine64 /usr/lib/wine/wine64; do
   if command -v "$w" >/dev/null 2>&1; then WINE=$w; break; fi
done
[ -n "$WINE" ] || { echo "no wine" >&2; exit 1; }
for t in Xvfb xdotool; do
   command -v $t >/dev/null 2>&1 || { echo "no $t" >&2; exit 1; }
done
[ -f "$root/retroarch.exe" ] || { echo "no retroarch.exe in $root" >&2; exit 1; }

export DISPLAY=:98
Xvfb :98 -screen 0 1280x720x24 > "$work/xvfb.log" 2>&1 &
XVFB=$!
sleep 1

write_cfg() {  # $1: video_threaded
   cat > "$work/retroarch.cfg" <<CFG
video_driver = "gdi"
input_driver = "raw"
input_joypad_driver = "winraw_joypad"
menu_driver = "rgui"
audio_driver = "null"
video_threaded = "$1"
video_fullscreen = "false"
pause_nonactive = "false"
config_save_on_exit = "false"
frontend_log_level = "0"
confirm_quit = "false"
video_window_save_positions = "false"
ui_menubar_enable = "false"
CFG
}

hold() {  # a key, held for a few frames, in whichever window is RetroArch's now
   local now
   now=$(xdotool search --name "RetroArch" 2>/dev/null | tail -1)
   [ -n "$now" ] && xdotool windowfocus "$now" 2>/dev/null
   xdotool keydown "$1"; sleep 0.12; xdotool keyup "$1"
   sleep 0.7
}

# $1: log file. The scenario: key, fullscreen toggle, key, quit.
# Returns 0 if RetroArch quit by itself.
play() {
   local log=$1 wid= app i
   ( cd "$root" && WINEDEBUG=-all exec $WINE ./retroarch.exe --verbose \
        -c "Z:$(echo "$work/retroarch.cfg" | sed 's|/|\\|g')" ) > "$log" 2>&1 &
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
   xdotool mousemove 300 300 click 1 2>/dev/null
   sleep 1
   hold Down
   hold f
   sleep 4
   hold Down
   hold Escape
   for i in $(seq 1 30); do kill -0 $app 2>/dev/null || break; sleep 0.5; done
   if kill -0 $app 2>/dev/null; then
      kill $app 2>/dev/null
      timeout 10 wineserver -k 2>/dev/null
      return 1
   fi
   timeout 10 wineserver -w 2>/dev/null
   return 0
}

count() { sed 's/\x1b\[[0-9;]*m//g' "$1" | tr -d '\r' | grep -ac "$2"; }

failures=0
# $1 name, $2 threaded, $3 want input driver starts by the poll,
# $4 want joypad driver destroyed, $5 "nomsg" if no report may come as
# a message; environment for RetroArch passes through
scenario() {
   local name=$1 threaded=$2 want_bulk=$3 want_destroyed=$4 nomsg=$5
   local log="$work/$name.log" try rc video bulk destroyed msg ok
   write_cfg "$threaded"
   # key delivery on a virtual display with no window manager is not
   # exact: a scenario gets a second go before it counts as failed
   for try in 1 2; do
      play "$log"; rc=$?
      video=$(count "$log" 'GDI\] Init complete')
      bulk=$(count "$log" 'read in bulk by the poll')
      destroyed=$(count "$log" 'RawInput Joypad\] Destroyed')
      msg=$(sed 's/\x1b\[[0-9;]*m//g' "$log" | tr -d '\r' \
            | grep -a 'Read by the poll: [0-9]* keyboard' \
            | grep -avc ', 0 as messages')
      ok=yes
      [ "$rc" = 0 ] || ok=no
      [ "$video" = 2 ] || ok=no
      [ "$bulk" = "$want_bulk" ] || ok=no
      [ "$destroyed" = "$want_destroyed" ] || ok=no
      [ "$nomsg" != nomsg ] || [ "$msg" = 0 ] || ok=no
      [ "$ok" = yes ] && break
   done
   if [ "$ok" = yes ]; then
      echo "[pass] $name: video driver started $video times, input driver $bulk by the poll," \
           "joypad driver destroyed $destroyed, quit on Escape after the restart"
   else
      echo "[FAIL] $name: quit by itself: $([ "$rc" = 0 ] && echo yes || echo no);" \
           "video driver started $video (want 2), input driver by the poll $bulk (want $want_bulk)," \
           "joypad driver destroyed $destroyed (want $want_destroyed)" \
           "$([ "$nomsg" = nomsg ] && echo ", driver instances with reports taken as messages: $msg (want 0)")"
      sed 's/\x1b\[[0-9;]*m//g' "$log" | tr -d '\r' | grep -av "ALSA lib\|Playlist\]" | tail -25
      failures=$((failures + 1))
   fi
}

scenario "read by the poll, kept" true 1 1 nomsg
RETROARCH_INPUT_KEEP=0 scenario "RETROARCH_INPUT_KEEP=0" true 2 2 nomsg
scenario "video not threaded" false 1 1 any
RETROARCH_RAWINPUT_POLL=0 scenario "RETROARCH_RAWINPUT_POLL=0" true 0 2 any

if [ "$failures" != 0 ]; then
   echo "FAIL windows_wine_smoke: $failures"
   exit 1
fi
echo "PASS windows_wine_smoke"
