#!/bin/sh
# GameMode lifecycle check.
#
# Runs the built ./retroarch headless (null drivers, a few frames of
# menu) against a stub libgamemode.so.0 that logs every call, and checks:
#
#   disabled  gamemode_enable = false: libgamemode is never called,
#             neither at startup nor at shutdown.
#   enabled   gamemode_enable = true: GameMode is entered at startup and
#             left at shutdown.  This lane also proves the stub is the
#             library actually loaded, so a clean 'disabled' run means
#             something.
#
# Requires a completed build in the repo root (./configure && make),
# with GameMode support compiled in.  From the repo root:
#
#   samples/frontend/gamemode/run.sh
set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
bin="$root/retroarch"
cc=${CC:-cc}

# A job that runs every sample without building the frontend (the
# unclaimed-samples workflow) has nothing to check this against.
if [ ! -x "$bin" ]; then
   echo "skip: no built retroarch in $root (./configure && make first)"
   exit 0
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

"$cc" -std=c89 -pedantic -Wall -Werror -fPIC -shared \
   -Wl,-soname,libgamemode.so.0 \
   -o "$work/libgamemode.so.0" "$here/gamemode_stub.c"

fail=0

run_lane()
{
   lane=$1
   enable=$2
   log="$work/$lane.log"
   cfg="$work/$lane.cfg"

   : > "$log"
   cat > "$cfg" <<EOF
video_driver = "null"
audio_driver = "null"
input_driver = "null"
input_joypad_driver = "null"
menu_driver = "rgui"
config_save_on_exit = "false"
gamemode_enable = "$enable"
EOF

   HOME="$work" XDG_CONFIG_HOME="$work" \
   GAMEMODE_STUB_LOG="$log" LD_LIBRARY_PATH="$work" \
      timeout 60 "$bin" --config="$cfg" --menu --max-frames=5 \
      > "$work/$lane.out" 2>&1 || {
         echo "FAIL $lane: retroarch exited with $?" >&2
         cat "$work/$lane.out" >&2
         fail=1
         return
      }
}

run_lane disabled false
if [ -s "$work/disabled.log" ]; then
   echo "FAIL disabled: libgamemode called with gamemode_enable = false:" >&2
   sed 's/^/   /' "$work/disabled.log" >&2
   fail=1
else
   echo "PASS disabled"
fi

run_lane enabled true
if grep -qx request_start "$work/enabled.log" \
      && grep -qx request_end "$work/enabled.log"; then
   echo "PASS enabled"
else
   echo "FAIL enabled: expected request_start and request_end, got:" >&2
   sed 's/^/   /' "$work/enabled.log" >&2
   fail=1
fi

exit $fail
