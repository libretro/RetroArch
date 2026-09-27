#!/usr/bin/env bash
# Runs retro_procbarrier_test and retro_asym_eventcount_test once per
# tier and fails unless each run used the tier it asked for.
#
#   run_tiers.sh <procbarrier bin> <asym eventcount bin> <force=name>...
#
# force is the RETRO_PROCBARRIER value ("natural" for none); name is what
# the test prints on its "tier:" line. A forced tier the platform lacks
# silently falls back to the natural one, which is why the name is
# checked rather than just the exit code.
#
# REQUIRE_SMP=1 also fails a run on one CPU, where the fence checks
# cannot catch anything.

set -u

pb=$1
aec=$2
shift 2

fails=0

for pair in "$@"; do
   force=${pair%%=*}
   want=${pair#*=}
   [ "$force" = natural ] && env_force= || env_force=$force

   printf '\n== %s (want tier: %s)\n' "$force" "$want"

   out=$(RETRO_PROCBARRIER=$env_force "$pb" 2>&1)
   rc=$?
   printf '%s\n' "$out"

   if [ $rc -ne 0 ] || ! printf '%s' "$out" | grep -q 'procbarrier: ok'; then
      echo "::error title=procbarrier failed::$force: exit $rc"
      fails=$((fails+1))
   fi
   if ! printf '%s' "$out" | grep -qF "tier: $want"; then
      echo "::error title=procbarrier wrong tier::$force: expected tier $want"
      fails=$((fails+1))
   fi
   if [ "${REQUIRE_SMP:-0}" = 1 ] && printf '%s' "$out" | grep -q 'cpus: 1 '; then
      echo "::error title=procbarrier on one CPU::$force: runner has one CPU"
      fails=$((fails+1))
   fi

   out=$(RETRO_PROCBARRIER=$env_force "$aec" 2>&1)
   rc=$?
   printf '%s\n' "$out"
   if [ $rc -ne 0 ] || ! printf '%s' "$out" | grep -q 'asym_eventcount: ok'; then
      echo "::error title=asym_eventcount failed::$force: exit $rc"
      fails=$((fails+1))
   fi
done

printf '\n%d failure(s)\n' "$fails"
[ $fails -eq 0 ]
