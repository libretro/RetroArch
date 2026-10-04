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
# It runs a core, smoke_core.c, built here as a DLL, which needs no
# content and writes to the log what reaches it: port 1's B button
# changing, and every key event its keyboard callback is given.
#
# Each scenario is: press J (bound to nothing) and Z (the default bind
# for B), toggle fullscreen with F (which, with GDI, restarts the video
# driver), press J and Z again, close the content with C (bound to
# that here; the drivers are freed and started again for the menu),
# quit with Escape. What is checked is what the log says happened. In
# every scenario: RetroArch quit on the Escape pressed after the
# content was closed; the core saw B pressed and released before the
# restart and after it; it was given J's key-down and key-up, in that
# order, both times; and every key event it was given came on the
# thread it runs on - with threaded video the window's messages are
# handled on the video thread, and DirectInput's keys used to reach
# the core's keyboard callback from there. And by scenario:
#
#   read by the poll, kept      the default. The input driver starts
#                               once and is restarted neither by the
#                               toggle nor by closing the content; the
#                               keyboard still works after both
#                               (Escape quits);
#                               reports were read in bulk, none taken
#                               by the thread's pump as messages.
#   RETROARCH_INPUT_KEEP=0      the input driver is restarted with the
#                               video driver, and works after it.
#   video not threaded          as the first, with the window, the pump
#                               and the poll all on one thread.
#   RETROARCH_RAWINPUT_POLL=0   the driver as it was before any of
#                               this: no bulk reads, restarted with
#                               the video driver, still works - and
#                               says, each time it is freed, how old
#                               what the poll read was (the wait that
#                               reading by the poll removes).
#   DirectInput                 the other input driver a Windows window
#                               can get, with its joypad driver:
#                               started by the same code, restarted
#                               with the video driver, works.
#
# In all of them the input driver is started by the frontend for the
# window, once the video driver is up, and not by the video driver.
#
# The scenarios above use the GDI video driver, whose fullscreen toggle
# restarts it. Two more use OpenGL, on Wine's software rasteriser:
#
#   OpenGL                      the toggle restyles the window in
#                               place and restarts nothing; closing
#                               the content restarts the driver, and
#                               the window is left up and taken back
#                               with the pixel format it had.
#   OpenGL, toggle by restart   RETROARCH_FULLSCREEN_IN_PLACE=0: the
#                               toggle restarts the driver as it used
#                               to, on the kept window.
#
# In every scenario a controller is also "plugged in": the window is
# sent the timer message that follows a device notification, which is
# what makes the frontend restart its joypad driver. With threaded
# video the window procedure that gets it is on the video thread, and
# the restart must be left for the poll on the frontend's thread to
# do - the log says so - where without threaded video it is done on
# the spot. Either way the joypad driver starts once more than it
# otherwise would, and the pad still works afterwards.
#
# One scenario gives the first port a keyboard of its own (its Keyboard
# Index names the first keyboard raw input lists, which under Wine is
# the only one): the port's key bind is then read from that keyboard's
# own state and must work as before. Telling two keyboards apart is
# samples/input/winraw_keyboards' to hold.
#
# The OpenGL scenarios also count how often the window's menu bar was
# built. It is taken off the window while it is fullscreen and put
# back when it is windowed again, and goes with a kept window through
# a driver restart: built once, where it used to be built again each
# time.
#
# ONLY=<part of a scenario's name> runs just the scenarios that match.
#
# And the window. In every scenario above it is left up across both
# restarts and taken back by the driver that comes next - with
# threaded video, where the video thread is held for it, and without.
# Two more:
#
#   RETROARCH_WINDOW_KEEP=0     the window is destroyed with the driver
#                               and a new one made, as it used to be.
#   RETROARCH_WINDOW_KEEP=2     the window is left up and then not
#                               taken: it is destroyed, a new one is
#                               made, and RetroArch does not take the
#                               destroying of it for being closed.
#
# (The core is given only the key-up of Z and F, never the key-down:
# the frontend keeps the key-down of a key that is bound to something
# from the core's keyboard callback. That is how it was before the poll
# read anything, and is the same in every scenario here; J is there
# because it shows both.)
#
# Keys are held for a few frames, as a finger holds one: the frontend
# samples the keyboard's state once a frame, and a press and release
# inside one frame is a state it never sees.
#
# Usage: run.sh [directory with retroarch.exe]   (default: the repo root)
# Needs: wine, Xvfb, xdotool, and mingw-w64's gcc for the core.
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

${MINGW_CC:-x86_64-w64-mingw32-gcc} -shared -O1 -Wall \
   -I"$here/../../../libretro-common/include" \
   -o "$work/smoke_core.dll" "$here/smoke_core.c" \
   || { echo "the core did not build" >&2; exit 1; }

# What a controller being plugged in ends in: the hotplug timer's
# message at RetroArch's window. The timer's id is the frontend's own.
timer_id=$(sed -n 's/^#define WIN32_HOTPLUG_TIMER_ID *\(0x[0-9A-Fa-f]*\).*/\1/p' \
   "$here/../../../gfx/common/win32_common.h")
[ -n "$timer_id" ] || { echo "no WIN32_HOTPLUG_TIMER_ID in win32_common.h" >&2; exit 1; }
cat > "$work/plug.c" <<PLUG
#include <windows.h>
int main(void)
{
   HWND w = FindWindowA("RetroArch", NULL);
   if (!w)
      return 1;
   return PostMessageA(w, WM_TIMER, $timer_id, 0) ? 0 : 2;
}
PLUG
${MINGW_CC:-x86_64-w64-mingw32-gcc} -O1 -o "$work/plug.exe" "$work/plug.c" \
   || { echo "the plug helper did not build" >&2; exit 1; }

export DISPLAY=:98
Xvfb :98 -screen 0 1280x720x24 > "$work/xvfb.log" 2>&1 &
XVFB=$!
sleep 1

write_cfg() {  # $1: video_threaded  $2: input driver  $3: joypad driver
   cat > "$work/retroarch.cfg" <<CFG
video_driver = "${VIDEO_DRIVER:-gdi}"
video_vsync = "false"
${EXTRA_CFG:-}
input_driver = "$2"
input_joypad_driver = "$3"
menu_driver = "rgui"
audio_driver = "null"
video_threaded = "$1"
video_fullscreen = "false"
pause_nonactive = "false"
config_save_on_exit = "false"
frontend_log_level = "0"
confirm_quit = "false"
input_close_content = "c"
quit_on_close_content = "0"
confirm_close = "false"
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
        -c "Z:$(echo "$work/retroarch.cfg" | sed 's|/|\\|g')" \
        -L "Z:$(echo "$work/smoke_core.dll" | sed 's|/|\\|g')" ) > "$log" 2>&1 &
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
   hold j
   hold z
   # a controller is plugged in
   ( cd "$work" && WINEDEBUG=-all $WINE ./plug.exe ) >/dev/null 2>&1
   sleep 1.5
   hold f
   sleep 4
   hold j
   hold z
   hold c
   sleep 4
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
# $4 want joypad driver starts, $5 "nomsg" if no report may come as a
# message, $6 input driver, $7 joypad driver; environment for RetroArch
# passes through
scenario() {
   local name=$1 threaded=$2 want_bulk=$3 want_joy=$4 nomsg=$5
   local log="$work/$name.log" try rc video bulk joy msg ok b_down b_up j_seq stale win off byname deferred kbd menus
   local want_video=${WANT_VIDEO:-3}
   if [ -n "${ONLY:-}" ]; then
      case "$name" in
         *"$ONLY"*) ;;
         *) return ;;
      esac
   fi
   write_cfg "$threaded" "${6:-raw}" "${7:-winraw_joypad}"
   # key delivery on a virtual display with no window manager is not
   # exact: a scenario gets a second go before it counts as failed
   for try in 1 2; do
      play "$log"; rc=$?
      video=$(count "$log" "${VIDEO_STARTED:-GDI\] Init complete}")
      bulk=$(count "$log" 'read in bulk by the poll')
      joy=$(count "$log" 'Found joypad driver')
      b_down=$(count "$log" 'smoke core\] joypad B pressed')
      b_up=$(count "$log" 'smoke core\] joypad B released')
      # J's events, in the order the core was given them
      j_seq=$(sed 's/\x1b\[[0-9;]*m//g' "$log" | tr -d '\r' \
            | grep -ao 'smoke core\] key event: [a-z]* keycode 106' \
            | awk '{printf "%s ", $5}')
      msg=$(sed 's/\x1b\[[0-9;]*m//g' "$log" | tr -d '\r' \
            | grep -a 'Read by the poll: [0-9]* keyboard' \
            | grep -avc ', 0 as messages')
      ok=yes
      [ "$rc" = 0 ] || ok=no
      [ "$video" = "$want_video" ] || ok=no
      [ "$bulk" = "$want_bulk" ] || ok=no
      [ "$joy" = "$want_joy" ] || ok=no
      [ "$b_down" = 2 ] && [ "$b_up" = 2 ] || ok=no
      [ "$j_seq" = "down up down up " ] || ok=no
      off=$(count "$log" "not the core's")
      [ "$off" = 0 ] || ok=no
      # the frontend starts the input driver for the window; it never
      # has to fall back to picking one by the setting's name
      byname=$(count "$log" 'did not initialize an input driver')
      [ "$byname" = 0 ] || ok=no
      # the joypad restart the plugged-in controller asks for: left to
      # the poll when the window procedure is on another thread
      # the raw input driver lists its keyboards, by name, for the
      # menu's Input Information; Wine has one
      kbd=$(count "$log" 'WinRaw\] Found keyboard #1: "')
      if [ "${6:-raw}" = raw ]; then
         [ "$kbd" -ge 1 ] || ok=no
      fi
      # the menu bar: built this many times, where it is asked
      menus=$(count "$log" 'Win32\] Menu bar built')
      [ -z "${WANT_MENUS:-}" ] || [ "$menus" = "$WANT_MENUS" ] || ok=no
      # a line the log has to have, where one is asked for
      [ -z "${WANT_LOG:-}" ] || [ "$(count "$log" "$WANT_LOG")" -ge 1 ] || ok=no
      deferred=$(count "$log" 'joypad driver is restarted by the poll')
      if [ "$threaded" = true ]; then
         [ "$deferred" = 1 ] || ok=no
      else
         [ "$deferred" = 0 ] || ok=no
      fi
      [ "$nomsg" != nomsg ] || [ "$msg" = 0 ] || ok=no
      # the old path measures the wait the new one removes; the new
      # one has nothing to measure
      win="$(count "$log" 'window is left up') $(count "$log" 'Took the window left up') $(count "$log" 'not one this driver takes')"
      [ "$win" = "${WANT_WINDOW:-2 2 0}" ] || ok=no
      stale=$(count "$log" "Taken by the window's thread: at the poll")
      if [ "$name" = "RETROARCH_RAWINPUT_POLL=0" ]; then
         [ "$stale" = 3 ] || ok=no
      else
         [ "$stale" = 0 ] || ok=no
      fi
      [ "$ok" = yes ] && break
   done
   if [ "$ok" = yes ]; then
      echo "[pass] $name: video driver started $video times, input driver $bulk by the poll," \
           "joypad driver $joy; the core saw B twice and J's down and up twice; quit on Escape"
      sed 's/\x1b\[[0-9;]*m//g' "$log" | tr -d '\r' \
         | grep -a "Taken by the window's thread" | sed 's/.*\[WinRaw\] /       /'
   else
      echo "[FAIL] $name: quit by itself: $([ "$rc" = 0 ] && echo yes || echo no);" \
           "video driver started $video (want $want_video), input driver by the poll $bulk (want $want_bulk)," \
           "joypad driver started $joy (want $want_joy);" \
           "lines on how old the state was at the poll: $stale;" \
           "window left up, taken, not taken: $win (want ${WANT_WINDOW:-2 2 0});" \
           "the core saw B pressed $b_down and released $b_up (want 2 and 2), J: ${j_seq:-nothing}(want down up down up)," \
           "key events on a thread that is not the core's: $off (want 0)," \
           "input driver picked by the setting's name: $byname (want 0)," \
           "keyboards named by the raw input driver: $kbd (want at least 1 with raw input)," \
           "menu bars built: $menus (want ${WANT_MENUS:-any})," \
           "log line asked for: ${WANT_LOG:-none}," \
           "joypad restarts left to the poll: $deferred (want $([ "$threaded" = true ] && echo 1 || echo 0))" \
           "$([ "$nomsg" = nomsg ] && echo ", driver instances with reports taken as messages: $msg (want 0)")"
      sed 's/\x1b\[[0-9;]*m//g' "$log" | tr -d '\r' | grep -av "ALSA lib\|Playlist\]" | tail -25
      failures=$((failures + 1))
   fi
}

# (joypad driver starts: what the restarts make, and one for the
# controller plugged in)
scenario "read by the poll, kept" true 1 2 nomsg
RETROARCH_INPUT_KEEP=0 scenario "RETROARCH_INPUT_KEEP=0" true 3 4 nomsg
scenario "video not threaded" false 1 2 any
RETROARCH_RAWINPUT_POLL=0 scenario "RETROARCH_RAWINPUT_POLL=0" true 0 4 any
scenario "DirectInput" true 0 4 any dinput dinput
WANT_WINDOW="0 0 0" RETROARCH_WINDOW_KEEP=0 scenario "RETROARCH_WINDOW_KEEP=0" true 1 2 nomsg
WANT_WINDOW="2 0 2" RETROARCH_WINDOW_KEEP=2 scenario "window left up and not taken" true 1 2 nomsg
EXTRA_CFG='input_player1_keyboard_index = "1"' \
   WANT_LOG='keyboards named by\|Found keyboard #1' \
   scenario "the first port given a keyboard" true 1 2 nomsg
# A port's keyboard is kept by what the keyboard is. Here the first
# port is pinned to one that is not plugged in. No other port has a
# keyboard of its own, so the port reads every keyboard - the core
# still sees the keys - and the log says so.
EXTRA_CFG='input_player1_keyboard_index = "1"
input_player1_keyboard_device = "dead:beef"' \
   WANT_LOG="Port 1's keyboard \"dead:beef\" is not there: the port reads every keyboard" \
   scenario "the first port pinned to a keyboard that is away" true 1 2 nomsg
# The same for the port's mouse: pinned to one that is not plugged in,
# with no other port having a mouse of its own, the port reads the
# mouse its Mouse Index names, and the log says so.
EXTRA_CFG='input_player1_mouse_device = "dead:beef"' \
   WANT_LOG="Port 1's mouse \"dead:beef\" is not there: the port reads the mouse its Mouse Index names" \
   scenario "the first port pinned to a mouse that is away" true 1 2 nomsg
# "XInput for Xbox Controllers" on: XInput is loaded and asked for its
# pads when the joypad driver starts. With none there, nothing changes.
EXTRA_CFG='input_winraw_xinput_enable = "true"' \
   scenario "XInput for Xbox controllers on" true 1 2 nomsg
# "Assign Ports on First Button Press" on: the first key gives the
# first user its core port, and the core still sees both presses.
EXTRA_CFG='input_assign_ports_on_button_press = "true"' \
   WANT_LOG='Keyboard assigned to core port 1' \
   scenario "ports assigned on first press" true 1 2 nomsg
# SOCD cleaning on: a single key at a time goes through unchanged.
EXTRA_CFG='input_socd_horizontal = "2"
input_socd_vertical = "4"' \
   scenario "SOCD cleaning on" true 1 2 nomsg
# Input rotation on: buttons that are not directions are unchanged.
EXTRA_CFG='input_rotation = "1"' \
   scenario "input rotation on" true 1 2 nomsg
# Automatic Mouse Grab: the pointer is captured, for that reason, when
# the window gains focus; everything else as before.
EXTRA_CFG='input_auto_mouse_grab = "true"
frontend_log_level = "0"' WANT_LOG='Grab mouse state => ON (reasons 0x02)' \
   scenario "automatic mouse grab" true 1 2 nomsg
# Background Keyboard Input on: the keyboard is registered as a sink;
# with the window active the keys work as before.
EXTRA_CFG='input_keyboard_background = "true"' \
   scenario "background keyboard input on" true 1 2 nomsg
VIDEO_DRIVER=gl VIDEO_STARTED='Found GL context' WANT_VIDEO=2 WANT_WINDOW="1 1 0" \
   WANT_MENUS=1 EXTRA_CFG='ui_menubar_enable = "true"' \
   scenario "OpenGL" true 1 2 nomsg
VIDEO_DRIVER=gl VIDEO_STARTED='Found GL context' WANT_VIDEO=3 WANT_WINDOW="2 2 0" \
   WANT_MENUS=1 EXTRA_CFG='ui_menubar_enable = "true"' \
   RETROARCH_FULLSCREEN_IN_PLACE=0 scenario "OpenGL, toggle by restart" true 1 2 nomsg

if [ "$failures" != 0 ]; then
   echo "FAIL windows_wine_smoke: $failures"
   exit 1
fi
echo "PASS windows_wine_smoke"
