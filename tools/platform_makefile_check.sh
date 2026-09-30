#!/bin/sh
# Every platform makefile that builds on Makefile.common, checked without
# its toolchain: the graph is generated with a stand-in compiler, the
# DEFINES it hands the compiler are read back, the gates are checked for
# consistency (TLS and SMB and NFS need networking, SMB needs the
# crypto), and the frontend units are compiled by the host compiler
# with exactly those defines and implicit declarations as errors -
# the shape in which a call gated one way and its definition another
# fails.
#
# Why this exists: Makefile.retrofw asked for HAVE_RETROSSL with no
# HAVE_NETWORKING; retroarch.c called ssl_socket_set_verify_mode()
# under HAVE_SSL and the object holding it was never built, and the
# only place that failed was the RetroFW workflow.
#
# Usage: tools/platform_makefile_check.sh [Makefile.x ...]
# Exits non-zero if any makefile fails.
set -u
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root" || exit 2

CC=${CC:-gcc}
TUS="retroarch.c runloop.c command.c gfx/video_driver.c configuration.c tasks/task_content.c"

stand=$(mktemp -d)
trap 'rm -rf "$stand"' EXIT INT TERM
# a stand-in toolchain: the makefiles only need paths that exist and an
# sdl-config that answers
mkdir -p "$stand/usr/bin"
printf '#!/bin/sh\ncase "$1" in --cflags) echo "";; --libs) echo "-lSDL";; esac\n' > "$stand/usr/bin/sdl-config"
chmod +x "$stand/usr/bin/sdl-config"
for t in gcc g++ strip ar ld ranlib; do
   ln -sf "$(command -v true)" "$stand/usr/bin/$t"
done

if [ $# -gt 0 ]; then
   makefiles="$*"
else
   makefiles="Makefile.retrofw Makefile.dingux Makefile.rg350 Makefile.rg350_odbeta Makefile.miyoo Makefile.rs90 Makefile.lfx000 Makefile.webos"
fi

fail=0
for mf in $makefiles; do
   [ -f "$mf" ] || { echo "skip  $mf (absent)"; continue; }
   graph="$stand/graph.txt"
   case "$mf" in
      Makefile.webos)
         mkdir -p "$stand/wtc" "$stand/wstage/usr/bin"
         for t in gcc g++ ar ld strip; do ln -sf "$(command -v true)" "$stand/wtc/arm-webos-linux-gnueabi-$t"; done
         cp "$stand/usr/bin/sdl-config" "$stand/wstage/usr/bin/freetype-config"
         if ! PATH="$stand/wtc:$PATH" CROSS_COMPILE=arm-webos-linux-gnueabi- STAGING_DIR="$stand/wstage" \
               make -f "$mf" -n > "$graph" 2>"$stand/err"; then
            echo "FAIL  $mf (make -n)"; head -5 "$stand/err" | sed 's/^/      /'; fail=1; continue
         fi ;;
      *)
         if ! GCW0_CC=true GCW0_CXX=true GCW0_STRIP=true HOST_DIR="$stand" TOOLCHAIN_DIR="$stand" \
               GCW0_SDL_CONFIG="$stand/usr/bin/sdl-config" \
               make -f "$mf" -n > "$graph" 2>"$stand/err"; then
            echo "FAIL  $mf (make -n)"; head -5 "$stand/err" | sed 's/^/      /'; fail=1; continue
         fi ;;
   esac

   # the defines the graph hands the compiler, once
   defs=$(grep -oE ' -D[A-Za-z_0-9]+(=[A-Za-z_0-9.]*)?' "$graph" | sort -u | sed 's/=$/="x"/' | tr '\n' ' ')
   has() { printf '%s' "$defs" | grep -q -- " -D$1\( \|=\)"; }

   bad=""
   if has HAVE_SSL      && ! has HAVE_NETWORKING; then bad="$bad HAVE_SSL-without-networking"; fi
   if has HAVE_RETROSSL && ! has HAVE_NETWORKING; then bad="$bad HAVE_RETROSSL-without-networking"; fi
   if has HAVE_RETROSSL && ! has HAVE_CRYPTO;     then bad="$bad HAVE_RETROSSL-without-crypto"; fi
   if has HAVE_SMBCLIENT && ! has HAVE_NETWORKING; then bad="$bad HAVE_SMBCLIENT-without-networking"; fi
   if has HAVE_RETROSMB && ! has HAVE_CRYPTO;     then bad="$bad HAVE_RETROSMB-without-crypto"; fi
   if has HAVE_NFSCLIENT && ! has HAVE_NETWORKING; then bad="$bad HAVE_NFSCLIENT-without-networking"; fi
   if has HAVE_KEYCHAIN && ! has HAVE_CRYPTO;     then bad="$bad HAVE_KEYCHAIN-without-crypto"; fi
   if has HAVE_KEYCHAIN && ! has HAVE_CONFIGFILE; then bad="$bad HAVE_KEYCHAIN-without-configfile"; fi
   if has HAVE_SSL && ! grep -q 'net_socket_ssl_' "$graph"; then bad="$bad HAVE_SSL-with-no-ssl-object"; fi
   if [ -n "$bad" ]; then
      echo "FAIL  $mf: inconsistent gates:$bad"; fail=1; continue
   fi

   # the frontend units under exactly those defines, host compiler; the
   # platform's own -m flags and sysroot paths are not carried, the
   # code paths behind the defines are
   hostdefs=$(printf ' %s ' "$defs" | sed 's/ -D\(DINGUX\|RETROFW\|MIYOO\|RS90\|LFX000\|WEBOS\)\(=[A-Za-z_0-9.]*\)\{0,1\} / /g')
   # SDL 1.2's headers, the one thing these frontends include from their
   # sysroot (libsdl1.2-dev on the host)
   sdlinc=""
   [ -d /usr/include/SDL ] && sdlinc="-I/usr/include/SDL"
   err=0
   for tu in $TUS; do
      if ! out=$($CC -Wall -Werror=implicit-function-declaration -I. -Ilibretro-common/include -Ideps $sdlinc \
            $hostdefs -fsyntax-only "$tu" 2>&1); then
         echo "FAIL  $mf: $tu"; printf '%s\n' "$out" | grep -E "error" | head -4 | sed 's/^/      /'; err=1
      fi
   done
   if [ $err -ne 0 ]; then fail=1; continue; fi
   echo "ok    $mf ($(printf '%s' "$defs" | wc -w | tr -d ' ') defines)"
done
exit $fail
