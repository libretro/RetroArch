#!/bin/sh
# The builtin glslang and SPIRV-Cross objects must define nothing a
# loaded core could bind to.  A core that bundles its own copy of either
# library resolves its calls through global scope first, so any default
# visibility definition in the executable wins over the core's own as
# soon as some shared library in the link references that symbol.
#
# The check reads the objects' own symbol tables rather than the linked
# executable's .dynsym: what ld may export is fixed at compile time, and
# the result then does not depend on which system libraries happen to be
# in the link.  What libstdc++ headers emit (std:: and __gnu_cxx::
# instantiations and their local classes, inline placement new/delete)
# is allowed; it is identical in every copy.
#
# ELF only: PE and Mach-O resolve per library and are not exposed.
#
# Usage: tools/builtin_deps_export_check.sh [objdir]
#        (default obj-unix/release, after a build with builtin glslang
#        and/or builtin SPIRV-Cross)
# Exits 1 if any object would export a symbol, 2 if nothing was checked.
set -u
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root" || exit 2

objdir=${1:-obj-unix/release}
READELF=${READELF:-readelf}

objs=$(find "$objdir/deps/glslang" "$objdir/deps/SPIRV-Cross" \
          -name '*.o' 2>/dev/null | sort)
if [ -z "$objs" ]; then
   echo "builtin_deps_export_check: no glslang/SPIRV-Cross objects under $objdir" >&2
   exit 2
fi

first=$(printf '%s\n' "$objs" | head -n 1)
if ! "$READELF" -h "$first" 2>/dev/null | grep -q 'ELF'; then
   echo "builtin_deps_export_check: $first is not ELF, nothing to check"
   exit 0
fi

fail=0
count=0
for o in $objs; do
   count=$((count + 1))
   syms=$("$READELF" -sW "$o" | awk '
      $7 != "UND" && $7 != "Ndx" && ($5 == "GLOBAL" || $5 == "WEAK") \
         && $6 == "DEFAULT" { print $8 }' |
      grep -Ev '^_Z(N(K)?)?(St|9__gnu_cxx)|^_ZT[VIS]St|^_ZGVZN?K?St|^_ZZN?K?St|^_Zn[wa][jm]Pv$|^_Zd[la]PvS_$')
   if [ -n "$syms" ]; then
      n=$(printf '%s\n' "$syms" | wc -l)
      echo "FAIL $o: $n exported symbol(s)"
      printf '%s\n' "$syms" | head -n 5 | sed 's/^/   /'
      fail=1
   fi
done

if [ "$fail" -ne 0 ]; then
   exit 1
fi
echo "PASS: $count builtin glslang/SPIRV-Cross objects export nothing"
exit 0
