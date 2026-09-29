#!/bin/sh
# The built-in NFSv4.0 client and the nfs:// VFS backend (version 4)
# against nfs-ganesha, a user-space NFSv4 server, on a fixed port with
# no portmapper. Runs when ganesha.nfsd is installed; otherwise skips.
# Wine works too (CC=..-mingw.. RUNNER=wine EXE=.exe), and the four
# thread test runs under TSan when the tools are built with it. The VFS
# FSAL changes fsuid per request, so ganesha wants root (as smbd does).
set -eu
cd "$(dirname "$0")"
GANESHA=$(command -v ganesha.nfsd || true)
[ -n "$GANESHA" ] || { echo "skip: nfs-ganesha not installed"; exit 0; }
make -s nfs_test vfs_test
RUN=${RUNNER:-}
EXE=${EXE:-}
PORT=20449
D=$(mktemp -d); chmod 755 $D; mkdir -p $D/export $D/run; chmod 777 $D/export
cat > $D/ganesha.conf << EOF
NFS_Core_Param { NFS_Port = $PORT; Enable_UDP = false; Bind_addr = 127.0.0.1; Protocols = 4; Enable_NLM = false; Enable_RQUOTA = false; }
NFSv4 { Minor_Versions = 0; Grace_Period = 2; Lease_Lifetime = 20; RecoveryBackend = fs_ng; RecoveryRoot = $D/run; }
EXPORT_DEFAULTS { Protocols = 4; Transports = TCP; SecType = sys; Access_Type = RW; Squash = No_Root_Squash; }
EXPORT { Export_Id = 77; Path = $D/export; Pseudo = /export; FSAL { Name = VFS; } }
LOG { Default_Log_Level = EVENT; }
EOF
# ganesha dies if it is not fully detached from this shell
setsid nohup $GANESHA -F -L $D/ganesha.log -f $D/ganesha.conf -p $D/run/g.pid -N NIV_EVENT > $D/out 2>&1 < /dev/null &
cleanup() {
   kill $(cat $D/run/g.pid 2>/dev/null) 2>/dev/null || pkill -x ganesha.nfsd 2>/dev/null || true
   sleep 1
   rm -rf $D 2>/dev/null || true
}
trap cleanup EXIT
# wait for the port, then for the grace period to pass (the client
# retries through it, but the negative case below should not wait)
i=0; while [ $i -lt 50 ]; do
   $RUN ./nfs_test$EXE 127.0.0.1 /export $PORT 0 4 > $D/res 2>&1 && break
   i=$((i + 1)); sleep 0.3
done
rm -rf $D/export/*
if $RUN ./nfs_test$EXE 127.0.0.1 /export $PORT 0 4 > $D/res 2>&1; then
   echo "ok:   NFSv4.0 against nfs-ganesha (no portmapper, pseudo path)"
else
   echo "FAIL: $(cat $D/res)"; exit 1
fi
# the real VFS on top, version 4
if $RUN ./vfs_test$EXE 127.0.0.1 /export $PORT 0 4 > $D/res 2>&1; then
   echo "ok:   nfs:// (v4) through vfs_implementation.c"
else
   echo "FAIL: $(cat $D/res)"; exit 1
fi
if make -s vfs_threads_test >/dev/null 2>&1 && $RUN ./vfs_threads_test$EXE 127.0.0.1 /export $PORT 0 4 > $D/res 2>&1; then
   echo "ok:   concurrent threads through the nfs:// (v4) backend"
else
   echo "FAIL: $(cat $D/res)"; exit 1
fi
if $RUN ./nfs_test$EXE 127.0.0.1 /nonexistent $PORT 0 4 > $D/res 2>&1; then
   echo "FAIL: unknown export was accepted"; exit 1
else
   echo "ok:   unknown export is refused"
fi
echo "[pass] nfs_retro local ganesha matrix"
