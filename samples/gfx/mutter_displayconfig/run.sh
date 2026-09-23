#!/bin/sh
# Every case of mutter_displayconfig_test, each with exactly the servers
# it needs.  Needs dbus-daemon, python3-dbus + python3-gi, Xorg with the
# dummy driver, weston, Xwayland and mutter.  Run from this directory after
# make; exits non-zero on the first failure.  XORG="sudo Xorg" where
# Xorg cannot run as the user (the GitHub runners).
set -eu
HERE=$(pwd)
T=$HERE/mutter_displayconfig_test
WORK=$(mktemp -d)
PIDS=""

cleanup()
{
   for p in $PIDS; do kill "$p" 2>/dev/null || sudo -n kill "$p" 2>/dev/null || true; done
   rm -rf "$WORK"
}
trap cleanup EXIT INT TERM

start_mock()
{
   python3 "$HERE/mock_mutter.py" > "$WORK/mock.log" 2>&1 &
   MOCK=$!
   PIDS="$PIDS $MOCK"
   for i in $(seq 1 50); do
      grep -q ready "$WORK/mock.log" 2>/dev/null && return 0
      sleep 0.1
   done
   cat "$WORK/mock.log"; exit 1
}

stop_mock()
{
   kill "$MOCK" 2>/dev/null || true
   wait "$MOCK" 2>/dev/null || true
}

# No session bus at all: nothing must be autolaunched
env -u DBUS_SESSION_BUS_ADDRESS XDG_RUNTIME_DIR="$WORK/nort" "$T" nobus

# A private session bus for everything else
export XDG_RUNTIME_DIR="$WORK/rt"
mkdir -p -m 700 "$XDG_RUNTIME_DIR"
dbus-daemon --session --fork --print-address=3 --print-pid=4 \
   3> "$WORK/bus.addr" 4> "$WORK/bus.pid"
DBUS_SESSION_BUS_ADDRESS=$(cat "$WORK/bus.addr")
export DBUS_SESSION_BUS_ADDRESS
PIDS="$PIDS $(cat "$WORK/bus.pid")"

"$T" nomutter

start_mock
"$T" mutter
stop_mock

# A real X server with Mutter on the bus: Mutter must not be asked
${XORG:-Xorg} :91 -config "$HERE/../display_servers_x11_live/xorg-dummy.conf" \
   -logfile "$WORK/xorg.log" -noreset -nolisten tcp -ac > "$WORK/xorg.out" 2>&1 &
PIDS="$PIDS $!"
for i in $(seq 1 60); do DISPLAY=:91 xrandr > /dev/null 2>&1 && break; sleep 0.25; done
start_mock
DISPLAY=:91 "$T" x11-xorg
stop_mock

# Headless Weston with Xwayland: GNOME's shape without GNOME
weston --backend=headless --socket=mdc-wl --idle-time=0 \
   > "$WORK/weston.log" 2>&1 &
PIDS="$PIDS $!"
for i in $(seq 1 60); do [ -S "$XDG_RUNTIME_DIR/mdc-wl" ] && break; sleep 0.25; done
WAYLAND_DISPLAY=mdc-wl Xwayland :92 -noreset > "$WORK/xwayland.log" 2>&1 &
PIDS="$PIDS $!"
for i in $(seq 1 60); do DISPLAY=:92 xrandr > /dev/null 2>&1 && break; sleep 0.25; done

DISPLAY=:92 "$T" x11-xwayland-nomutter
WAYLAND_DISPLAY=mdc-wl "$T" wl-nomutter

start_mock
DISPLAY=:92 "$T" x11-xwayland
stop_mock
start_mock
WAYLAND_DISPLAY=mdc-wl "$T" wl
stop_mock

# A real headless Mutter and the XWayland it starts: the outputs carry
# Mutter's connector names, as on a GNOME desktop
MLOG="$WORK/mutter.log"
mutter --headless --virtual-monitor 2560x1440@60 --wayland > "$MLOG" 2>&1 &
PIDS="$PIDS $!"
for i in $(seq 1 120); do
   grep -q "Using public X11 display" "$MLOG" 2>/dev/null && break
   sleep 0.25
done
MDPY=$(sed -n 's/.*Using public X11 display \(:[0-9]*\).*/\1/p' "$MLOG" | head -1)
MAUTH=$(ls "$XDG_RUNTIME_DIR"/.mutter-Xwaylandauth.* 2>/dev/null | head -1)
[ -n "$MDPY" ] && [ -n "$MAUTH" ] || { cat "$MLOG"; exit 1; }
for i in $(seq 1 60); do
   DISPLAY=$MDPY XAUTHORITY=$MAUTH xrandr > /dev/null 2>&1 && break
   sleep 0.25
done
DISPLAY=$MDPY XAUTHORITY=$MAUTH "$T" x11-real-mutter

echo "ALL OK"
