#!/bin/sh
# Builds every sample and tool harness in the tree that compiles a given
# libretro-common translation unit, so a new dependency inside that unit
# (a call into another libretro-common TU) is caught at link time here,
# before a mailbox is delivered.
#
# The harnesses under samples/, libretro-common/samples/, tests-other/
# and tools/ each
# carry a hand-picked list of libretro-common sources, and a TU that
# starts calling into a unit those lists do not name links in the main
# build and fails in every one of them.  Nothing short of linking them
# shows it: -fsyntax-only passes, and grepping build files for the TU's
# name is exactly the step that gets truncated or misses a shell script.
#
# Usage:
#   tools/libretro-common-link-check.sh streams/rzip_stream.c
#   tools/libretro-common-link-check.sh rthreads/rthreads.c --scripts
#
# Without --scripts the tools/*.sh harnesses that name the unit are
# listed for running by hand (several need a display, wine or GNUstep);
# with it they are run too, in a build-only way where the script
# supports one.  Exit status is non-zero if any build fails.
set -u
cd "$(dirname "$0")/.."

tu="${1:?usage: $0 <libretro-common/relative/unit.c> [--scripts]}"
run_scripts=0
[ "${2:-}" = "--scripts" ] && run_scripts=1

name=$(basename "$tu" .c)
fail=0
built=0

# Every Makefile that names the unit, by the source list the build reads
for mk in $(grep -rl --include='Makefile*' "$name" samples libretro-common/samples tests-other 2>/dev/null | sort); do
   dir=$(dirname "$mk")
   file=$(basename "$mk")
   if [ "$file" = "Makefile" ]; then
      arg=""
   else
      arg="-f $file"
   fi
   built=$((built + 1))
   if out=$(make -C "$dir" $arg -s clean all 2>&1); then
      echo "ok    $mk"
   else
      echo "FAIL  $mk"
      echo "$out" | grep -E "undefined reference|Undefined symbols|error:" | sed 's/^/      /' | head -6
      fail=1
   fi
done

# Shell harnesses that compile the unit directly
for sh in $(grep -l "$name" tools/*.sh 2>/dev/null | grep -v "$(basename "$0")" | sort); do
   if [ $run_scripts = 1 ]; then
      built=$((built + 1))
      if out=$("$sh" 2>&1); then
         echo "ok    $sh"
      else
         echo "FAIL  $sh"
         echo "$out" | grep -E "undefined reference|Undefined symbols|error:|not found" | sed 's/^/      /' | head -6
         fail=1
      fi
   else
      echo "run   $sh"
   fi
done

# Where the main builds take the unit from, for the platforms that do
# not use Makefile.common
for f in Makefile.common griffin/griffin.c; do
   grep -q "$name" "$f" && echo "in    $f"
done
for f in Makefile.*salamander; do
   grep -q "$name" "$f" && echo "in    $f  (salamander: a hand-picked TU subset, check its object list)"
done

echo "$built built, $( [ $fail = 0 ] && echo all linked || echo FAILURES )"
exit $fail
