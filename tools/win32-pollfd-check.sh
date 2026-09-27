#!/bin/sh
# struct pollfd must be declared exactly once on every Win32 SDK target.
#
# winsock2.h declares it only when _WIN32_WINNT >= 0x0600. Declaring it
# unconditionally breaks Vista+ targets (UWP, msvc2022) with a
# redefinition; declaring it nowhere breaks msvc2003 (0x0400) and
# msvc2005 (0x0410), which build networking against a pre-Vista SDK.
# Both headers that can supply it must agree, in either include order,
# because griffin puts them in one translation unit.
#
# Run by hand after touching net_compat.h or deps/libsmb2/lib/compat.h:
#   tools/win32-pollfd-check.sh
#
# Prints one line per case and exits non-zero if any failed.

set -u

CC="${CC:-x86_64-w64-mingw32-gcc}"
command -v "$CC" >/dev/null 2>&1 || { echo "no $CC" >&2; exit 2; }

root=$(dirname "$0")/..
tmp=${TMPDIR:-/tmp}/pollfd-check.$$
mkdir -p "$tmp" || exit 2
trap 'rm -rf "$tmp"' EXIT INT TERM

INC="-I$root/libretro-common/include -I$root/deps/libsmb2/lib \
 -I$root/deps/libsmb2/include -I$root"

# Both orders: griffin decides which header lands first per build.
cat > "$tmp/order_net_first.c" <<'EOF'
#include <net/net_compat.h>
#include "compat.h"
struct pollfd probe;
int main(void) { probe.fd = 0; probe.events = 0; return (int)probe.revents; }
EOF

cat > "$tmp/order_smb_first.c" <<'EOF'
#include "compat.h"
#include <net/net_compat.h>
struct pollfd probe;
int main(void) { probe.fd = 0; probe.events = 0; return (int)probe.revents; }
EOF

# Errors first: these headers warn about unrelated things (winsock2.h
# include order) that would otherwise fill the excerpt.
show() {
   grep -m4 'error' "$1" || sed -n '1,4p' "$1"
}

# Without libsmb2 in the picture: msvc2003/msvc2005 build networking
# but not the builtin SMB client, so net_compat.h must stand alone.
fail=0

for w in 0x0400 0x0410 0x0501 0x0600 0x0A00; do
   for c in order_net_first order_smb_first; do
      if $CC -std=c89 -Werror=declaration-after-statement \
            -Werror=implicit-function-declaration \
            -c "$tmp/$c.c" -o /dev/null \
            -D_WIN32_WINNT=$w -DWINVER=$w -DHAVE_NETWORKING \
            $INC > "$tmp/log" 2>&1; then
         echo "ok   $c _WIN32_WINNT=$w"
      else
         echo "FAIL $c _WIN32_WINNT=$w"
         show "$tmp/log"
         fail=1
      fi
   done

   if $CC -fsyntax-only -std=gnu99 \
         -D_WIN32_WINNT=$w -DWINVER=$w \
         -I"$root/libretro-common/include" -I"$root" \
         "$root/libretro-common/net/net_socket.c" > "$tmp/log" 2>&1; then
      echo "ok   net_socket.c _WIN32_WINNT=$w"
   else
      echo "FAIL net_socket.c _WIN32_WINNT=$w"
      show "$tmp/log"
      fail=1
   fi
done

exit $fail
