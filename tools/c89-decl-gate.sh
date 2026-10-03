#!/bin/sh
# Recompiles the given source files with the EXACT flags the current
# build uses - their compile lines taken from `make -n` after
# touching them, so every HAVE_ define, include path and gate
# matches what the release compiler sees - plus
# -Werror=declaration-after-statement. Exists because a per-file
# syntax gate with a hand-picked define set checks a different
# preprocessed file than the build compiles: a declaration inside a
# HAVE_ guard the gate does not define is deleted before the check
# runs, which is exactly how a mixed-declaration slipped past the
# old gate while breaking the real build. Requires a configured
# tree; touches the files, so the next `make` recompiles them.
# Usage: tools/c89-decl-gate.sh file.c [file.c ...]
set -u
fail=0
for f in "$@"; do touch -- "$f"; done
make -n 2>/dev/null > /tmp/c89gate_make_n.$$ || {
   echo "c89-decl-gate: make -n failed (configure first)"; exit 2; }
for f in "$@"; do
   line=$(grep -F -- "$f" /tmp/c89gate_make_n.$$ | grep -F -- " -c " | head -1)
   if [ -z "$line" ]; then
      echo "SKIP  $f (no compile line found)"; continue
   fi
   cmd=$(printf '%s' "$line" | sed 's/ -o [^ ]*//')
   if sh -c "$cmd -Wdeclaration-after-statement -Werror=declaration-after-statement -fsyntax-only" 2>/tmp/c89gate_err.$$; then
      echo "ok    $f"
   else
      echo "FAIL  $f"
      head -4 /tmp/c89gate_err.$$ | sed 's/^/      /'
      fail=1
   fi
done
rm -f /tmp/c89gate_make_n.$$ /tmp/c89gate_err.$$
exit $fail
