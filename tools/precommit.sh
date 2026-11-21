#!/bin/sh
# The tree-wide checks CI runs on every push, the fast ones, for the
# files a commit changes.  tools/hooks/pre-commit runs this on the
# staged files; run it by hand with file names, or with none for all.
#
#   git config core.hooksPath tools/hooks     (once per clone)
#   sh tools/precommit.sh [file...]
#
# Each check is the one its workflow runs, so a pass here is a pass
# there.  It does not run the samples and builds a change touches:
# tools/ci_local.py which, printed at the end, lists those.

cd "$(git rev-parse --show-toplevel)" || exit 1

if command -v python3 >/dev/null 2>&1; then PY=python3
elif command -v python >/dev/null 2>&1; then PY=python
else echo "precommit: no python on the PATH" >&2; exit 1; fi

if [ "$#" -gt 0 ]; then
   files=$(printf '%s\n' "$@")
elif [ -n "$PRECOMMIT_STAGED" ]; then
   files=$(git diff --cached --name-only --diff-filter=ACMR)
else
   files=$(git ls-files)
fi
[ -n "$files" ] || exit 0

has() { printf '%s\n' "$files" | grep -Eq "$1"; }

failed=""
run() {
   name=$1; shift
   printf 'precommit: %s\n' "$name"
   if ! out=$("$@" 2>&1); then
      printf '%s\n' "$out" | tail -n 40
      failed="$failed $name"
   fi
}

src='\.(c|h|m|mm|cpp|cc|cxx|hpp|inl)$'

if has "$src"; then
   run "Windows SDK macro names" $PY tools/win_macro_names_check.py
   run "overlapping copies" tools/check.sh tools/overlap_copy_check.c
   run "extern C prototypes" tools/check.sh tools/extern_c_prototype_check.c
fi

if has '^(gfx|libretro-common/vulkan)/'; then
   run "context presentable" tools/check.sh tools/ctx_presentable_check.c
   run "Vulkan idle waits" tools/check.sh tools/vulkan_idle_wait_check.c
   run "Vulkan NULL submits" tools/check.sh tools/vulkan_null_submit_check.c
   run "Vulkan teardown order" tools/check.sh tools/vulkan_teardown_order_check.c
   run "D3D12 waits" tools/check.sh tools/d3d12_idle_wait_check.c
fi

if has '^(menu/|intl/|msg_hash|settings/)'; then
   run "msg_hash packed table" tools/check.sh tools/msg_hash_packed_check.c
   run "msg_hash formats" tools/check.sh tools/msg_hash_format_check.c
   run "sublabel map order" $PY tools/sublabel_map_check.py
   lbl_before=$(git hash-object msg_hash_lbl_str.h)
   $PY tools/gen_lbl_str.py >/dev/null 2>&1
   if [ "$(git hash-object msg_hash_lbl_str.h)" != "$lbl_before" ]; then
      echo "msg_hash_lbl_str.h was stale and is regenerated: stage it"
      failed="$failed msg_hash_lbl_str.h"
   fi
fi

if has '^(settings/|menu/menu_setting\.c|configuration\.|config\.def\.h|tools/settings_)'; then
   run "settings table" $PY tools/settings_check.py table --check tools/settings_table.reference
   run "settings unity" $PY tools/settings_check.py unity
fi

if has '(^|/)Makefile[^/]*$|^samples/|^libretro-common/samples/'; then
   run "sample object directories" tools/check.sh tools/sample_objdir_check.c
fi

if has '^\.github/workflows/|(^|/)Makefile[^/]*$'; then
   run "workflow paths filters" $PY tools/ci_paths.py --check
fi

if [ -n "$failed" ]; then
   echo
   echo "precommit: failed:$failed"
   echo "Fix these before committing; CI fails on each of them."
   exit 1
fi

if [ -n "$PRECOMMIT_STAGED" ] || [ "$#" -gt 0 ]; then
   echo
   echo "precommit: passed. The CI jobs these files reach, to run with"
   echo "tools/ci_local.py run <workflow> --job <job> before pushing:"
   # shellcheck disable=SC2086
   $PY tools/ci_local.py which $files 2>/dev/null | sed -n '1,60p'
fi
exit 0
