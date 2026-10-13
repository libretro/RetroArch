#!/bin/bash
# Runs make with its C and C++ compilers wrapped in ccache, for the
# platform Makefiles that set CC and CXX themselves. The compilers are
# read from the same Makefile and variables, so the wrapped build is
# the build CI ran before.
#   bash .github/workflows/scripts/ccache-make.sh -f Makefile.libnx -j4 all
set -euo pipefail
if ! command -v ccache > /dev/null; then
   echo "ccache-make: no ccache, building without it"
   exec make "$@"
fi
args=("$@")
# The options and variable assignments, without the targets.
opts=()
while [ $# -gt 0 ]; do
   case $1 in
      -f|-C|-I|-o|-W) opts+=("$1" "$2"); shift ;;
      -*|*=*) opts+=("$1") ;;
   esac
   shift
done
print() {
   make "${opts[@]}" --no-print-directory \
      --eval "ra-print: ; @echo \$($1)" ra-print | tail -n 1
}
cc=$(print CC)
cxx=$(print CXX)
if [ -z "$cc" ] || [ -z "$cxx" ]; then
   echo "ccache-make: could not read CC and CXX" >&2
   exit 1
fi
echo "ccache-make: CC=$cc CXX=$cxx"
exec make "${args[@]}" CC="ccache $cc" CXX="ccache $cxx"
