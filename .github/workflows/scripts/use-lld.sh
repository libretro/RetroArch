#!/bin/sh
# Makes the host gcc link with LLVM's lld for the rest of the job.
#
# lld comes with the runner image, but under a versioned name
# (ld.lld-18 and so on) that changes as the image does, so it is looked
# up rather than named. A small "ld" that runs it is put first on PATH:
# the host gcc runs the first "ld" on PATH, while cross compilers run
# their own prefixed linker (x86_64-w64-mingw32-ld, ...) and clang its
# own, so neither of those changes.
#
# GNU ld still does two kinds of link:
#   --wrap   lld also wraps the calls made inside the object that
#            defines the symbol, GNU ld does not; the samples that wrap
#            are written for GNU ld's rule.
#   gcc -flto objects, which lld cannot read: lld fails on them, and the
#            link is run again with GNU ld.
# No lld found leaves the job on GNU ld; that is never an error.
set -eu

lld=
if command -v ld.lld >/dev/null 2>&1; then
   lld=$(command -v ld.lld)
else
   for c in $(ls /usr/bin/ld.lld-* /usr/lib/llvm-*/bin/ld.lld 2>/dev/null \
         | sort -V -r); do
      if [ -x "$c" ]; then
         lld=$c
         break
      fi
   done
fi
gnu=$(command -v ld.bfd || command -v ld || true)

if [ -z "$lld" ] || [ -z "$gnu" ]; then
   echo "use-lld: no lld on this runner; linking with the default ld"
   exit 0
fi

dir="${RUNNER_TEMP:-/tmp}/lld-bin"
mkdir -p "$dir"
cat > "$dir/ld" <<WRAPPER
#!/bin/sh
for a; do
   case "\$a" in --wrap*|-wrap*) exec "$gnu" "\$@" ;; esac
done
err=\$(mktemp)
if "$lld" "\$@" 2>"\$err"; then
   cat "\$err" >&2
   rm -f "\$err"
   exit 0
fi
rm -f "\$err"
exec "$gnu" "\$@"
WRAPPER
chmod +x "$dir/ld"
echo "use-lld: $lld ($("$lld" --version | head -n 1)), GNU ld $gnu"
if [ -n "${GITHUB_PATH:-}" ]; then
   echo "$dir" >> "$GITHUB_PATH"
fi
