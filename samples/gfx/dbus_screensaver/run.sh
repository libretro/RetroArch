#!/bin/sh
# Runs the test on a private session bus of its own.
set -eu
cd "$(dirname "$0")"
info=$(mktemp)
dbus-daemon --session --fork --print-address=1 --print-pid=1 > "$info"
DBUS_SESSION_BUS_ADDRESS=$(sed -n 1p "$info")
bus_pid=$(sed -n 2p "$info")
rm -f "$info"
export DBUS_SESSION_BUS_ADDRESS
trap 'kill "$bus_pid" 2>/dev/null || true' EXIT
./dbus_screensaver_test
