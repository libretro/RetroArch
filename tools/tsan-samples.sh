#!/bin/sh
# Builds each suite in tools/tsan-samples.list with SANITIZER=thread and
# runs its test binary; any ThreadSanitizer report fails the test. A
# suite that ships tsan.supp has its reports from an uninstrumented
# library suppressed with it, as its own lane does.
#
# Usage: tools/tsan-samples.sh [list]
set -u

root=$(cd "$(dirname "$0")/.." && pwd)
list=${1:-$root/tools/tsan-samples.list}
fail=0
built=""

# The Wayland suites run an in-process compositor, whose socket goes in
# XDG_RUNTIME_DIR.
if [ -z "${XDG_RUNTIME_DIR:-}" ]; then
   XDG_RUNTIME_DIR=$(mktemp -d)
   export XDG_RUNTIME_DIR
fi

cd "$root"
while read -r dir bin args; do
   case "$dir" in ''|'#'*) continue ;; esac
   case " $built " in
      *" $dir "*) ;;
      *)
         make -C "$dir" clean </dev/null >/dev/null 2>&1
         if ! out=$(make -C "$dir" SANITIZER=thread </dev/null 2>&1); then
            echo "FAIL  $dir: build"
            printf '%s\n' "$out" | grep -E 'error' | head -5 | sed 's/^/      /'
            fail=1
            continue
         fi
         built="$built $dir" ;;
   esac
   opts="halt_on_error=1 second_deadlock_stack=1"
   [ -f "$dir/tsan.supp" ] && opts="$opts suppressions=$root/$dir/tsan.supp"
   if out=$(cd "$dir" && TSAN_OPTIONS="$opts" timeout 900 ./"$bin" $args </dev/null 2>&1); then
      echo "ok    $dir/$bin"
   else
      echo "FAIL  $dir/$bin"
      printf '%s\n' "$out" | grep -E -A12 'WARNING: ThreadSanitizer' | head -30 | sed 's/^/      /'
      [ -n "$(printf '%s\n' "$out" | grep 'WARNING: ThreadSanitizer')" ] \
         || printf '%s\n' "$out" | tail -n 8 | sed 's/^/      /'
      fail=1
   fi
done < "$list"

for dir in $built; do
   make -C "$dir" clean </dev/null >/dev/null 2>&1
done
exit $fail
