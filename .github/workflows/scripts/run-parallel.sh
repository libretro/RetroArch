# Sourced by a step: runs commands at the same time, for tests that wait
# on a real clock and use next to no CPU, so the step takes as long as
# the longest of them rather than all of them in a row.
#
#   par "name" command args...   start one, its output kept apart
#   par_wait                     wait for all; print each one's output in
#                                the order started, and fail the step if
#                                any of them failed
par_names=""
par_pids=""
par_dir=$(mktemp -d)

par() {
   name="$1"; shift
   "$@" > "$par_dir/$(echo "$name" | tr ' /' '__').log" 2>&1 &
   par_pids="$par_pids $!"
   par_names="$par_names|$name"
}

par_wait() {
   bad=0
   set -- $par_pids
   old_ifs=$IFS; IFS='|'; names=${par_names#|}; IFS=$old_ifs
   while [ $# -gt 0 ]; do
      name=${names%%|*}; names=${names#*|}
      if wait "$1"; then status=pass; else status=FAIL; bad=1; fi
      echo "== $name"
      cat "$par_dir/$(echo "$name" | tr ' /' '__').log"
      echo "[$status] $name"
      shift
   done
   rm -rf "$par_dir"
   par_names=""; par_pids=""; par_dir=$(mktemp -d)
   return $bad
}
