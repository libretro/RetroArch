#!/bin/sh
# net_nfs3.c against unfs3 (a user-space NFSv3 server) on fixed ports,
# no portmapper: mount, write, read, seek, stat, mkdir, rename,
# truncate, list (unfs3 has no READDIRPLUS, so this covers the plain
# READDIR + LOOKUP path), unlink. Builds unfs3 from its repository when
# no unfsd is on PATH and the toolchain for it is there; otherwise
# skips, and passes.
set -e
cd "$(dirname "$0")"
UNFSD=$(command -v unfsd || true)
if [ -z "$UNFSD" ]; then
   if command -v git >/dev/null 2>&1 && command -v flex >/dev/null 2>&1 && command -v bison >/dev/null 2>&1; then
      B=$(mktemp -d)
      # built with a plain toolchain: the sanitizer flags a caller sets
      # for the client must not reach the server, which is not ours to
      # fix and dies under them
      if git clone -q --depth 1 https://github.com/unfs3/unfs3.git $B/unfs3 \
            && (cd $B/unfs3 && env -u CFLAGS -u LDFLAGS -u CC ./bootstrap >/dev/null 2>&1 \
                && env -u CFLAGS -u LDFLAGS -u CC ./configure >/dev/null 2>&1 \
                && env -u CFLAGS -u LDFLAGS -u CC make -j2 >/dev/null 2>&1); then
         UNFSD=$B/unfs3/unfsd
      fi
   fi
fi
[ -n "$UNFSD" ] || { echo "skip: no unfsd and no way to build one"; exit 0; }
make -s nfs_test vfs_test
RUN=${RUNNER:-}
EXE=${EXE:-}
D=$(mktemp -d); chmod 755 $D; mkdir -p $D/export; chmod 777 $D/export
echo "$D/export 127.0.0.1(rw,no_root_squash,insecure)" > $D/exports
trap 'kill $SRV 2>/dev/null || true; rm -rf $D' EXIT
$UNFSD -p -n 20049 -m 20048 -e $D/exports -d > $D/log 2>&1 &
SRV=$!
i=0; while [ $i -lt 50 ]; do
   $RUN ./nfs_test$EXE 127.0.0.1 $D/export 20049 20048 > $D/out 2>&1 && break
   grep -q "connect failed" $D/out || break
   i=$((i + 1)); sleep 0.2
done
rm -rf $D/export/*
if $RUN ./nfs_test$EXE 127.0.0.1 $D/export 20049 20048 > $D/out 2>&1; then
   echo "ok:   NFSv3 over TCP against unfs3 (fixed ports)"
else
   echo "FAIL: $(cat $D/out)"; exit 1
fi
# the real VFS on top: nfs://server/... through filestream and retro_dirent
if $RUN ./vfs_test$EXE 127.0.0.1 $D/export 20049 20048 > $D/out 2>&1; then
   echo "ok:   nfs:// through vfs_implementation.c"
else
   echo "FAIL: $(cat $D/out)"; exit 1
fi
# four threads on a two-slot pool through the VFS; under TSan when the
# caller built the tools with it (CFLAGS=-fsanitize=thread)
if make -s vfs_threads_test >/dev/null 2>&1 && $RUN ./vfs_threads_test$EXE 127.0.0.1 $D/export 20049 20048 > $D/out 2>&1; then
   echo "ok:   concurrent threads through the nfs:// backend"
else
   echo "FAIL: $(cat $D/out)"; exit 1
fi
rm -rf $D/export/*
if $RUN ./nfs_test$EXE 127.0.0.1 /nonexistent/export 20049 20048 > $D/out 2>&1; then
   echo "FAIL: unknown export was accepted"; exit 1
else
   echo "ok:   unknown export is refused"
fi
echo "[pass] nfs_retro local unfs3 matrix"
