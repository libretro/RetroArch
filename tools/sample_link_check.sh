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
#   tools/sample_link_check.sh --all builds every sample there is, for
#   when a change is wide enough that guessing which ones matter is
#   the wrong move.
set -eu

rev=${1:-HEAD}
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"


# Build one sample and, where it has a check target, run it the way its
# CI job does: under the address and undefined sanitizers with leak
# detection on. Linking alone proved too little - a sample that builds
# and passes plain can still hold a leak or a use-after-free that only
# the sanitized run reports.
build_one() {
   dir=$1
   name=$2
   printf '== %s (%s)\n' "$dir" "$name"
   log=$(mktemp)
   if ! (cd "$root/$dir" && make -f "$name" clean >/dev/null 2>&1 \
         && make -f "$name" >"$log" 2>&1); then
      tail -20 "$log"
      echo "FAIL $dir"
      rm -f "$log"
      return 1
   fi
   # A sample that cross-builds a Windows target cannot be run here;
   # building it is the whole check this host can do.
   # make itself says whether the target exists, which a grep for
   # "check:" does not - a comment mentioning it is not a rule, and a
   # sample whose runnable target is called something else has none.
   # A sample that cross-builds a Windows target is built and not
   # run, since this host cannot run it.
   if     (cd "$root/$dir" && make -f "$name" -n check >/dev/null 2>&1) \
      && ! grep -qE '\.exe|mingw|MINGW' "$root/$dir/$name"; then
      if (cd "$root/$dir" && make -f "$name" clean >/dev/null 2>&1 \
          && ASAN_OPTIONS=detect_leaks=1:allocator_may_return_null=1 \
             UBSAN_OPTIONS=print_stacktrace=1 \
             timeout 300 make -f "$name" SANITIZER=address,undefined check \
             >>"$log" 2>&1); then
         echo "   ok (checked)"
      else
         tail -25 "$log"
         echo "FAIL $dir (check)"
         rm -f "$log"
         return 1
      fi
   else
      echo "   ok"
   fi
   rm -f "$log"
   return 0
}

if [ "${1:-}" = "--all" ]; then
   rev=""
   changed="--all"
else
   changed=$(git diff --name-only "$rev" ; git diff --cached --name-only "$rev")
fi
if [ "$changed" = "--all" ]; then
   todo=""
   for mk in $(find samples libretro-common/samples -name 'Makefile*' 2>/dev/null | sort); do
      todo="$todo $(dirname "$mk"):$(basename "$mk")"
   done
   rc=0
   for entry in $todo; do
      dir=${entry%%:*}
      name=${entry#*:}
      build_one "$dir" "$name" || rc=1
   done
   exit $rc
fi
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
   build_one "$dir" "$(basename "$mk")" || rc=1
done
exit $rc
