#!/bin/sh
# Every case of x11_monitors_test, each on a fresh Xvfb so monitors set
# in one case do not leak into the next.  Needs Xvfb.  Run from this
# directory after make; exits non-zero on the first failure.
# THREADS_ONLY=1 runs only the threads case.
set -eu
T=$(pwd)/x11_monitors_test
WORK=$(mktemp -d)
XVFB=""

stop_xvfb()
{
   if [ -n "$XVFB" ]; then
      kill "$XVFB" 2>/dev/null || true
      wait "$XVFB" 2>/dev/null || true
      XVFB=""
   fi
}

cleanup()
{
   stop_xvfb
   rm -rf "${WORK:?}"
}
trap cleanup EXIT INT TERM

start_xvfb()
{
   : > "$WORK/display"
   Xvfb -displayfd 3 -nolisten tcp -screen 0 1920x1080x24 \
      3> "$WORK/display" > "$WORK/xvfb.log" 2>&1 &
   XVFB=$!
   for i in $(seq 1 100); do
      [ -s "$WORK/display" ] && break
      sleep 0.05
   done
   [ -s "$WORK/display" ] || { cat "$WORK/xvfb.log"; exit 1; }
   DISPLAY=":$(cat "$WORK/display")"
   export DISPLAY
}

run_case()
{
   rc=0
   start_xvfb
   "$T" "$1" > "$WORK/$1.out" 2>&1 || rc=$?
   cat "$WORK/$1.out"
   stop_xvfb
   [ "$rc" -eq 0 ] || { echo "case $1 failed ($rc)"; exit 1; }
}

if [ -z "${THREADS_ONLY:-}" ]; then
   # No server at all: nothing must wait for one
   [ ! -e /tmp/.X11-unix/X250 ] || { echo "a server is on :250"; exit 1; }
   env -u DISPLAY "$T" nodisplay

   run_case one
   run_case two
   if grep -q "^SKIP two" "$WORK/two.out"; then
      echo "SKIP own: Xvfb shows one Xinerama screen for two monitors"
   else
      run_case own
   fi
   run_case gone
fi

run_case threads

echo "ALL OK"
