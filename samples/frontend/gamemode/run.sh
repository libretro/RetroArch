#!/bin/sh
# Game Mode check (frontend/drivers/gamemode_linux.c).
#
# Builds gamemode_test against the real gamemode_linux.c and libdbus
# loaded at run time, starts a private dbus-daemon and points the
# system bus at it, then runs these lanes:
#
#   hold     fake_ppd owns org.freedesktop.UPower.PowerProfiles: the
#            performance profile is held on enter and released on leave
#   legacy   fake_ppd owns only net.hadess.PowerProfiles (ppd < 0.20)
#   refuse   the daemon answers HoldProfile with an error
#   none     no daemon at all: nothing blocks, I/O priority still applies
#   plug     fake_upower on AC: pulling the charger releases the hold,
#            plugging back in holds again
#   unplugged  fake_upower starts on battery: no hold until plugged in
#   nowake   no UPower, under strace: the helper follows the kernel's
#            uevents and never wakes on a timer (skipped without strace)
#
# Every lane also checks the I/O priority on all threads. Needs Linux,
# libdbus-1.so.3 and dbus-daemon; skips without them.
#
#   samples/frontend/gamemode/run.sh
set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
lrc="$root/libretro-common"
cc=${CC:-cc}

if [ "$(uname -s)" != Linux ]; then
   echo "skip: Linux only"; exit 0
fi
if ! command -v dbus-daemon >/dev/null 2>&1; then
   echo "skip: no dbus-daemon"; exit 0
fi

work=$(mktemp -d)
daemon_pid=
ppd_pid=
upower_pid=
cleanup()
{
   [ -n "$upower_pid" ] && kill "$upower_pid" 2>/dev/null || :
   [ -n "$ppd_pid" ] && kill "$ppd_pid" 2>/dev/null || :
   [ -n "$daemon_pid" ] && kill "$daemon_pid" 2>/dev/null || :
   rm -rf "$work"
}
trap cleanup EXIT

if ! "$cc" -std=gnu99 -Wall -o "$work/fake_ppd" "$here/fake_ppd.c" \
      -I"$root" -I"$lrc/include" -DHAVE_DYLIB -l:libdbus-1.so.3 \
      2> "$work/cc.log"; then
   echo "skip: cannot link libdbus-1.so.3"; cat "$work/cc.log"; exit 0
fi
"$cc" -std=gnu99 -Wall -o "$work/fake_upower" "$here/fake_upower.c" \
   -I"$root" -I"$lrc/include" -DHAVE_DYLIB -l:libdbus-1.so.3

# gamemode_linux.c builds as C89, as it does in RetroArch.
c89="-std=c89 -pedantic -Werror=declaration-after-statement -Wno-long-long"
defs="-DHAVE_THREADS -DHAVE_DYLIB -I$root -I$lrc/include"
for src in "$root/frontend/drivers/gamemode_linux.c" \
           "$root/gfx/common/dbus_runtime.c" \
           "$lrc/rthreads/rthreads.c" \
           "$lrc/dynamic/dylib.c" \
           "$lrc/compat/compat_strl.c"; do
   "$cc" $c89 -Wall -Werror -c $defs -o "$work/$(basename "$src" .c).o" "$src"
done
"$cc" -std=gnu99 -Wall -Werror -c $defs -o "$work/gamemode_test.o" "$here/gamemode_test.c"
"$cc" -o "$work/gamemode_test" "$work"/*.o -lpthread -ldl

dbus-daemon --session --nofork --print-address=3 3> "$work/addr" &
daemon_pid=$!
i=0
while [ ! -s "$work/addr" ] && [ $i -lt 100 ]; do sleep 0.05; i=$((i + 1)); done
DBUS_SYSTEM_BUS_ADDRESS=$(head -n 1 "$work/addr")
export DBUS_SYSTEM_BUS_ADDRESS

fail=0

lane()
{
   name=$1 busname=$2 ppdmode=$3 testmode=$4 power=${5:-}
   log="$work/$name.log"
   : > "$log"
   ppd_pid=
   upower_pid=
   extra=
   if [ -n "$power" ]; then
      "$work/fake_upower" "$log" "$power" &
      upower_pid=$!
      extra=$upower_pid
      i=0
      while ! grep -qx "upower ready" "$log" && [ $i -lt 100 ]; do sleep 0.05; i=$((i + 1)); done
   fi
   if [ -n "$busname" ]; then
      "$work/fake_ppd" "$busname" "$log" $ppdmode &
      ppd_pid=$!
      i=0
      while ! grep -qx ready "$log" && [ $i -lt 100 ]; do sleep 0.05; i=$((i + 1)); done
   fi
   if timeout 60 "$work/gamemode_test" "$log" "$testmode" $extra > "$work/$name.out" 2>&1; then
      echo "PASS $name"
   else
      echo "FAIL $name" >&2
      sed 's/^/   /' "$work/$name.out" >&2
      fail=1
   fi
   if [ -n "$ppd_pid" ]; then
      kill "$ppd_pid" 2>/dev/null || :
      wait "$ppd_pid" 2>/dev/null || :
      ppd_pid=
   fi
   if [ -n "$upower_pid" ]; then
      kill "$upower_pid" 2>/dev/null || :
      wait "$upower_pid" 2>/dev/null || :
      upower_pid=
   fi
}

lane hold   org.freedesktop.UPower.PowerProfiles ""     hold
lane legacy net.hadess.PowerProfiles             ""     hold
lane refuse org.freedesktop.UPower.PowerProfiles refuse refuse
lane none   ""                                   ""     none
lane plug      org.freedesktop.UPower.PowerProfiles "" plug      ac
lane unplugged org.freedesktop.UPower.PowerProfiles "" unplugged battery

# Without UPower the helper must wait on the kernel's uevent socket, not
# re-check on a timer: a NETLINK_KOBJECT_UEVENT socket bound, and no
# poll() with the fallback's 5000 ms timeout.
if command -v strace >/dev/null 2>&1 \
      && strace -o /dev/null true >/dev/null 2>&1; then
   log="$work/nowake.log"
   : > "$log"
   "$work/fake_ppd" org.freedesktop.UPower.PowerProfiles "$log" &
   ppd_pid=$!
   i=0
   while ! grep -qx ready "$log" && [ $i -lt 100 ]; do sleep 0.05; i=$((i + 1)); done
   if timeout 60 strace -f -e trace=socket,bind,poll -o "$work/nowake.trace" \
         "$work/gamemode_test" "$log" hold > "$work/nowake.out" 2>&1 \
         && grep -q 'socket(AF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT)' "$work/nowake.trace" \
         && grep -Eq 'bind\(.*nl_groups=0x0*1[,}]' "$work/nowake.trace" \
         && ! grep -q 'poll(.*, 5000)' "$work/nowake.trace"; then
      echo "PASS nowake"
   else
      echo "FAIL nowake: expected a bound uevent socket and no timed re-check" >&2
      grep -E 'NETLINK|nl_groups|5000\)' "$work/nowake.trace" | sed 's/^/   /' >&2 || :
      sed 's/^/   /' "$work/nowake.out" >&2
      fail=1
   fi
   kill "$ppd_pid" 2>/dev/null || :
   wait "$ppd_pid" 2>/dev/null || :
   ppd_pid=
else
   echo "skip nowake: no strace"
fi

exit $fail
