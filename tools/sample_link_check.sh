#!/bin/sh
# Build every sample that links a source file this working tree changed.
#
# The samples link subsets of the tree against their own stubs, so a
# call added to a file they link - task_image.c, gfx_thumbnail.c,
# input_driver.c and friends - is a link error there long before it is
# one anywhere else, and compiling the file alone never shows it. This
# is the step that catches that class, and it is meant to be run
# before a patch goes out, not after CI says so.
#
# Usage: tools/sample_link_check.sh [rev]
#   rev defaults to HEAD, so an uncommitted tree checks its own diff.
set -eu

rev=${1:-HEAD}
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"

changed=$(git diff --name-only "$rev" ; git diff --cached --name-only "$rev")
changed=$(printf '%s\n' $changed | sort -u | grep -E '\.(c|m|cpp)$' || true)
if [ -z "$changed" ]; then
   echo "no changed sources"
   exit 0
fi

makefiles=$(find samples libretro-common/samples -name 'Makefile*' 2>/dev/null | sort)
todo=""
for mk in $makefiles; do
   dir=$(dirname "$mk")
   for src in $changed; do
      base=$(basename "$src")
      if grep -q "$base" "$mk" 2>/dev/null; then
         case " $todo " in
            *" $dir:$mk "*) ;;
            *) todo="$todo $dir:$mk" ;;
         esac
      fi
   done
done

if [ -z "$todo" ]; then
   echo "no sample links any changed source"
   exit 0
fi

rc=0
for entry in $todo; do
   dir=${entry%%:*}
   mk=${entry#*:}
   name=$(basename "$mk")
   printf '== %s (%s)\n' "$dir" "$name"
   log=$(mktemp)
   if (cd "$root/$dir" && make -f "$name" clean >/dev/null 2>&1 \
       && make -f "$name" >"$log" 2>&1); then
      echo "   ok"
   else
      tail -20 "$log"
      echo "FAIL $dir"
      rc=1
   fi
   rm -f "$log"

done
exit $rc
