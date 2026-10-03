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
make -s vfs_threads_test nfs_idle_test nfs_sparse_test >/dev/null 2>&1 || true
stop() {
   kill $(cat $D/run/g.pid 2>/dev/null) 2>/dev/null || pkill -x ganesha.nfsd 2>/dev/null || true
   i=0; while pgrep -x ganesha.nfsd >/dev/null 2>&1 && [ $i -lt 30 ]; do sleep 0.2; i=$((i + 1)); done
   rm -rf $D/run/* 2>/dev/null || true
}
cleanup() {
   stop
   rm -rf $D 2>/dev/null || true
}
trap cleanup EXIT

# One server per round, speaking the minor versions given; the client
# must settle on the newest of them it knows (2, 1, then 0).
round() {
   minors="$1"; want="$2"
   cat > $D/ganesha.conf << EOF
NFS_Core_Param { NFS_Port = $PORT; Enable_UDP = false; Bind_addr = 127.0.0.1; Protocols = 4; Enable_NLM = false; Enable_RQUOTA = false; }
NFSv4 { Minor_Versions = $minors; Grace_Period = 2; Lease_Lifetime = 20; RecoveryBackend = fs_ng; RecoveryRoot = $D/run; }
EXPORT_DEFAULTS { Protocols = 4; Transports = TCP; SecType = sys; Access_Type = RW; Squash = No_Root_Squash; }
EXPORT { Export_Id = 77; Path = $D/export; Pseudo = /export; FSAL { Name = VFS; } }
LOG { Default_Log_Level = EVENT; }
EOF
   # each server starts on an empty export, as a fresh install would:
   # files left by the round before and removed behind a running
   # server's back would test its cache, not this client
   rm -rf $D/export/* 2>/dev/null || true
   # ganesha dies if it is not fully detached from this shell
   setsid nohup $GANESHA -F -L $D/ganesha.log -f $D/ganesha.conf -p $D/run/g.pid -N NIV_EVENT > $D/out 2>&1 < /dev/null &
   # wait for the port, then for the grace period to pass (the client
   # retries through it, but the negative case below should not wait)
   i=0; while [ $i -lt 50 ]; do
      $RUN ./nfs_test$EXE 127.0.0.1 /export $PORT 0 4 > $D/res 2>&1 && break
      i=$((i + 1)); sleep 0.3
   done
   rm -rf $D/export/*
   if $RUN ./nfs_test$EXE 127.0.0.1 /export $PORT 0 4 > $D/res 2>&1; then
      echo "ok:   NFSv4 against nfs-ganesha offering 4.[$minors] (no portmapper, pseudo path)"
   else
      echo "FAIL: 4.[$minors]: $(cat $D/res)"; exit 1
   fi
   if grep -q "minor version $want" $D/res; then
      echo "ok:   settled on NFSv4.$want"
   else
      echo "FAIL: 4.[$minors] should settle on 4.$want: $(grep 'minor version' $D/res)"; exit 1
   fi
   # the real VFS on top, version 4
   if $RUN ./vfs_test$EXE 127.0.0.1 /export $PORT 0 4 > $D/res 2>&1; then
      echo "ok:   nfs:// (v4.$want) through vfs_implementation.c"
   else
      echo "FAIL: 4.[$minors]: $(cat $D/res)"; exit 1
   fi
   # pipelined reads with read-ahead on: one session slot per READ
   if [ -x ./vfs_threads_test$EXE ] && $RUN ./vfs_threads_test$EXE 127.0.0.1 /export $PORT 0 4 > $D/res 2>&1; then
      echo "ok:   concurrent threads through the nfs:// (v4.$want) backend"
   else
      echo "FAIL: 4.[$minors]: $(cat $D/res)"; exit 1
   fi
   # the export as a v3 client knows it - the directory's own path on the
   # server - above the v4 namespace root: its leading components are
   # dropped until the namespace has it (Linux fsid=0 roots it the same)
   rm -rf $D/export/*
   if $RUN ./nfs_test$EXE 127.0.0.1 $D/export $PORT 0 4 > $D/res 2>&1; then
      echo "ok:   NFSv4.$want export given by its server path, above the namespace root"
   else
      echo "FAIL: 4.[$minors]: $(cat $D/res)"; exit 1
   fi
   # a path under an export the namespace does have is not guessed at
   if $RUN ./nfs_test$EXE 127.0.0.1 /export/nonexistent $PORT 0 4 > $D/res 2>&1; then
      echo "FAIL: 4.[$minors]: a wrong path under an existing export was accepted"; exit 1
   else
      echo "ok:   wrong path under an export is refused"
   fi
   # nfs-ganesha's READ_PLUS answers every range with one empty data
   # run: a 4.2 client must see that no READ could have said so, go
   # back to READ and read the file right
   if [ "$want" = 2 ]; then
      truncate -s 4M $D/export/sparse.img
      head -c 262144 /dev/urandom | dd of=$D/export/sparse.img bs=65536 seek=16 conv=notrunc status=none
      local_path=$D/export/sparse.img
      [ -n "$RUN" ] && local_path="Z:$D/export/sparse.img"
      if [ -x ./nfs_sparse_test$EXE ] && $RUN ./nfs_sparse_test$EXE 127.0.0.1 /export $PORT sparse.img "$local_path" 262144 fallback > $D/res 2>&1; then
         echo "ok:   NFSv4.2: a server's broken READ_PLUS is left for READ, the file read right"
      else
         echo "FAIL: 4.[$minors] READ_PLUS fallback: $(cat $D/res)"; exit 1
      fi
   fi
   stop
}

round "0"       0
round "1"       1
round "2"       2
round "0, 1, 2" 2

# Files kept open across a pause - the game paused, a long cutscene -
# must still read and write afterwards: past a short lease on a server
# that reclaims, and across a server restart (a NAS rebooting), where
# the write is the call that finds the connection gone.
serve() {
   minors="$1"; lease="$2"
   rm -rf $D/export/* 2>/dev/null || true
   cat > $D/ganesha.conf << EOF
NFS_Core_Param { NFS_Port = $PORT; Enable_UDP = false; Bind_addr = 127.0.0.1; Protocols = 4; Enable_NLM = false; Enable_RQUOTA = false; }
NFSv4 { Minor_Versions = $minors; Grace_Period = $lease; Lease_Lifetime = $lease; RecoveryBackend = fs_ng; RecoveryRoot = $D/run; }
EXPORT_DEFAULTS { Protocols = 4; Transports = TCP; SecType = sys; Access_Type = RW; Squash = No_Root_Squash; }
EXPORT { Export_Id = 77; Path = $D/export; Pseudo = /export; FSAL { Name = VFS; } }
LOG { Default_Log_Level = EVENT; }
EOF
   setsid nohup $GANESHA -F -L $D/ganesha.log -f $D/ganesha.conf -p $D/run/g.pid -N NIV_EVENT > $D/out 2>&1 < /dev/null &
   i=0; while [ $i -lt 50 ]; do
      $RUN ./nfs_test$EXE 127.0.0.1 /export $PORT 0 4 > $D/res 2>&1 && break
      i=$((i + 1)); sleep 0.3
   done
   rm -rf $D/export/*
}
for m in 0 1; do
   serve $m 5
   if $RUN ./nfs_idle_test$EXE 127.0.0.1 /export $PORT 0 4 12 > $D/res 2>&1; then
      echo "ok:   NFSv4.$m: a file read and written after idling past the lease"
   else
      echo "FAIL: NFSv4.$m idle past the lease: $(cat $D/res)"; exit 1
   fi
   stop
   serve $m 5
   $RUN ./nfs_idle_test$EXE 127.0.0.1 /export $PORT 0 4 12 w > $D/res 2>&1 &
   idler=$!
   sleep 4
   stop_keep() {
      kill $(cat $D/run/g.pid 2>/dev/null) 2>/dev/null || true
      i=0; while pgrep -x ganesha.nfsd >/dev/null 2>&1 && [ $i -lt 30 ]; do sleep 0.2; i=$((i + 1)); done
   }
   stop_keep   # the server goes, its state with it; the files stay
   # back with a grace period longer than any retry could wait out, as
   # Linux's 90 seconds is: the files' opens must be claimed back
   sed -i 's/Grace_Period = [0-9]*;/Grace_Period = 60;/' $D/ganesha.conf
   setsid nohup $GANESHA -F -L $D/ganesha.log -f $D/ganesha.conf -p $D/run/g.pid -N NIV_EVENT > $D/out 2>&1 < /dev/null &
   if wait $idler; then
      echo "ok:   NFSv4.$m: a file read and written across a server restart"
   else
      echo "FAIL: NFSv4.$m across a server restart: $(cat $D/res)"; exit 1
   fi
   stop
done
echo "[pass] nfs_retro local ganesha matrix"
