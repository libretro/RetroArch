#!/bin/sh
# Which samples/audio suites compile this driver - and do they still
# build and pass. Run after ANY audio-driver edit, before commit:
#   tools/audio-suite-check.sh audio/drivers/<driver>.c
# Only the matching suites are built, nothing else.
set -u
[ $# -eq 1 ] || { echo "usage: $0 audio/drivers/<driver>.c"; exit 2; }
base=$(basename "$1")
fail=0; found=0
for d in samples/audio/*/; do
   [ -f "$d/Makefile" ] || continue
   if grep -rqs "$base" "$d"Makefile "$d"*.c "$d"*.h 2>/dev/null; then
      found=1
      ( cd "$d" &&
        # A suite ships tsan.supp when its reports come from an
        # uninstrumented dependency rather than from the code under test.
        # The workflow passes it; so does this.
        if [ -f tsan.supp ]; then
           TSAN_OPTIONS=halt_on_error=1:suppressions=$PWD/tsan.supp
           export TSAN_OPTIONS
        fi
        for san in "" address,undefined thread; do
           [ -n "$san" ] && ! grep -q SANITIZER Makefile && continue
           make clean >/dev/null 2>&1
           if ! timeout 240 make SANITIZER=$san >/tmp/asc.log 2>&1; then
              echo "BUILD-FAIL $d (SANITIZER=$san)"
              grep -m2 "error\|undefined" /tmp/asc.log; exit 1
           fi
           t=$(ls *_test 2>/dev/null | head -1)
           if [ -n "$t" ] && ! timeout 180 "./$t" >/tmp/asc_run.log 2>&1; then
              echo "RUN-FAIL $d (SANITIZER=$san)"
              grep -m6 "ThreadSanitizer\|AddressSanitizer\|runtime error" /tmp/asc_run.log || tail -3 /tmp/asc_run.log
              exit 1
           fi
        done
        echo "ok    $d" ) || fail=1
   fi
done
[ $found -eq 0 ] && echo "no suite references $base"
exit $fail
