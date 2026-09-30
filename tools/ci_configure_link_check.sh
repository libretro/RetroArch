#!/bin/sh
# Build RetroArch under every distinct ./configure flag set the CI
# workflows use, each in its own git worktree of HEAD.
#
# Why this exists: a function placed inside #ifdef HAVE_MENU and called
# from runloop.c compiles everywhere and fails to link only in the
# --disable-menu lanes.  Appending -UHAVE_MENU to the tree's compile
# line proves nothing - the Makefile's own -D wins - and a syntax pass
# cannot see a link failure at all.  The only check that holds is the
# real configure, followed by a real build.
#
# Usage:
#   tools/ci_configure_link_check.sh            every set, full link
#   tools/ci_configure_link_check.sh --objects  the frontend objects only
#       (retroarch, runloop, command, video_driver, task_content): each
#       set compiles them, and any symbol they call that the default
#       build defines in one of them must be defined under the set too.
#       Minutes instead of an hour; catches the #ifdef mismatch a link
#       would, for those units.  Needs a default `make` done first.
#   tools/ci_configure_link_check.sh --list     print the sets and stop
#
# A set whose configure fails on this host (a library the lane installs
# and this machine lacks) is reported as SKIP, not as a failure, and
# cross-host sets (--host=...) are skipped.  --disable-qt is added to
# each set that does not enable Qt, as the harnesses require.
#
# Prints one line per set and exits non-zero if any set failed.

set -u
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root" || exit 2

mode=full
case "${1:-}" in
   --list)    mode=list ;;
   --objects) mode=objects ;;
   "") ;;
   *) echo "usage: $0 [--list|--objects]" >&2; exit 2 ;;
esac

# Every ./configure invocation, continuation lines joined, reduced to
# its --flags; one set per line, unique.
sets=$(awk '
   /\.\/configure/ { collecting = 1; line = "" }
   collecting {
      line = line " " $0
      if ($0 !~ /\\[ \t]*$/) {
         collecting = 0
         n = split(line, w, /[ \t]+/)
         flags = ""
         for (i = 1; i <= n; i++)
            if (w[i] ~ /^--/) flags = flags " " w[i]
         sub(/^ /, "", flags)
         print flags
      }
   }' .github/workflows/*.yml | sort -u)

if [ "$mode" = list ]; then
   echo "$sets" | sed 's/^$/(default)/'
   exit 0
fi

objs="obj-unix/release/retroarch.o obj-unix/release/runloop.o \
 obj-unix/release/command.o obj-unix/release/gfx/video_driver.o \
 obj-unix/release/tasks/task_content.o"

ref_defs=""
if [ "$mode" = objects ]; then
   for o in $objs; do
      [ -f "$o" ] || { echo "build the default tree first: ./configure --disable-qt && make" >&2; exit 2; }
   done
   # shellcheck disable=SC2086
   ref_defs=$(nm $objs | awk '$2 ~ /^[Tt]$/ { print $3 }' | sort -u)
fi

failed=$(mktemp)
: > "$failed"
jobs=$(nproc 2>/dev/null || echo 2)

echo "$sets" | while IFS= read -r flags; do
   case " $flags " in
      *" --host="*)  echo "SKIP  [$flags] (cross host)"; continue ;;
      *" --enable-qt"*|*" --disable-qt"*) ;;
      *) flags="$flags --disable-qt" ;;
   esac
   wt=$(mktemp -d "${TMPDIR:-/tmp}/ra-cfg-XXXXXX") && rmdir "$wt"
   git worktree add -q "$wt" HEAD 2>/dev/null || { echo "FAIL  worktree for [$flags]"; echo x >> "$failed"; continue; }
   (
      cd "$wt" || exit 1
      # shellcheck disable=SC2086
      if ! ./configure $flags > configure.log 2>&1; then
         echo "SKIP  [$flags]"
         grep -E "cannot locate|Exiting" configure.log | head -1 | sed 's/^/      /'
         exit 0
      fi
      if [ "$mode" = full ]; then
         if make -j"$jobs" > build.log 2>&1; then
            echo "ok    [$flags]"
            exit 0
         fi
         echo "FAIL  [$flags]"
         grep -E "error|undefined reference" build.log | head -5 | sed 's/^/      /'
         exit 1
      fi
      # shellcheck disable=SC2086
      if ! make -j"$jobs" $objs > build.log 2>&1; then
         echo "FAIL  [$flags] (compile)"
         grep -E "error" build.log | head -5 | sed 's/^/      /'
         exit 1
      fi
      # shellcheck disable=SC2086
      missing=$(nm $objs | awk '$1 == "U" { print $2 }' | sort -u | while read -r sym; do
         echo "$ref_defs" | grep -qx "$sym" || continue
         # shellcheck disable=SC2086
         nm $objs | grep -q " [Tt] $sym\$" || echo "$sym"
      done)
      if [ -n "$missing" ]; then
         echo "FAIL  [$flags] (called but compiled out: $(echo "$missing" | tr '\n' ' '))"
         exit 1
      fi
      echo "ok    [$flags] (objects)"
   ) || echo x >> "$failed"
   git worktree remove --force "$wt" 2>/dev/null
done

if [ -s "$failed" ]; then
   rm -f "$failed"
   exit 1
fi
rm -f "$failed"
exit 0
