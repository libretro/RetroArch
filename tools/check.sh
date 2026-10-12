#!/bin/sh
# Runs a tree checker written in C, building it first when its source
# or check_common.h is newer than the binary:
#
#   tools/check.sh overlap_copy_check [--selftest]
#
# The checker is named as that, or by its source file
# (tools/overlap_copy_check.c).
#
# CC picks the compiler (default cc, then gcc), CHECK_OBJDIR where the
# binaries are kept (default: a directory of this checkout's under
# $TMPDIR).
set -eu

if [ $# -lt 1 ]; then
   echo "usage: tools/check.sh <checker> [arguments]" >&2
   exit 2
fi

here=$(cd "$(dirname "$0")" && pwd)
name=$(basename "${1%.c}")
shift
src="$here/$name.c"
if [ ! -f "$src" ]; then
   echo "check.sh: there is no tools/$name.c" >&2
   exit 2
fi

exe=
case "$(uname -s 2>/dev/null || echo unknown)" in
   MINGW*|MSYS*|CYGWIN*) exe=.exe ;;
esac

if [ -z "${CHECK_OBJDIR:-}" ]; then
   CHECK_OBJDIR="${TMPDIR:-/tmp}/retroarch-checks-$(printf '%s' "$here" | cksum | cut -d' ' -f1)"
fi
bin="$CHECK_OBJDIR/$name$exe"

if [ ! -x "$bin" ] || [ "$src" -nt "$bin" ] \
      || [ "$here/check_common.h" -nt "$bin" ]; then
   if [ -z "${CC:-}" ]; then
      if command -v cc >/dev/null 2>&1; then CC=cc; else CC=gcc; fi
   fi
   mkdir -p "$CHECK_OBJDIR"
   tmp="$CHECK_OBJDIR/$name.$$$exe"
   $CC -std=c89 -O2 -Wall -Wextra -pedantic \
      -Werror=declaration-after-statement -I"$here" -o "$tmp" "$src"
   mv -f "$tmp" "$bin"
fi

RA_TREE_ROOT="$(dirname "$here")"
export RA_TREE_ROOT
exec "$bin" "$@"
