#!/bin/sh
# net_smb2.c against a local smbd: every dialect 2.0.2 to 3.1.1 with
# signing mandatory, sealing required on 3.0.2 (AES-CCM) and 3.1.1
# (AES-GCM), and the refusals that must happen. Needs samba and root
# (smbd binds 445 on the loopback only). Skips, and passes, without them.
set -e
cd "$(dirname "$0")"
command -v smbd >/dev/null 2>&1 || { echo "skip: no smbd"; exit 0; }
[ "$(id -u)" = 0 ] || { echo "skip: needs root for port 445"; exit 0; }
make -s smb_test vfs_test vfs_threads_test
RUN=${RUNNER:-}
EXE=${EXE:-}
D=$(mktemp -d); chmod 755 $D; mkdir -p $D/share $D/priv $D/run /run/samba
stop() { pkill -f "smbd -s $D/smb.conf" 2>/dev/null || true; pkill -f "configfile=$D/smb.conf" 2>/dev/null || true; }
# Port 445 must be ours: a distribution smbd started by the package
# (the CI runner's samba comes up as a service) answers every case
# instead of the one this script configured - it maps the unknown
# user to guest and knows no [share].
port_busy() { ss -ltn 2>/dev/null | grep -q ':445 '; }
if port_busy; then
   systemctl stop smbd nmbd samba-ad-dc 2>/dev/null || true
   pkill -x smbd 2>/dev/null || true
   i=0; while port_busy && [ $i -lt 50 ]; do i=$((i + 1)); sleep 0.2; done
   port_busy && { echo "FAIL: port 445 is held by another smbd"; exit 1; }
fi
trap 'stop; rm -rf $D' EXIT
id rsmbtest >/dev/null 2>&1 || useradd -M -s /usr/sbin/nologin rsmbtest
chown rsmbtest $D/share
conf() { # max_protocol encrypt
cat > $D/smb.conf << EOC
[global]
   workgroup = RETRO
   server role = standalone server
   security = user
   smb ports = 445
   interfaces = lo
   bind interfaces only = yes
   pid directory = $D/run
   lock directory = $D/run
   state directory = $D/run
   cache directory = $D/run
   private dir = $D/priv
   log file = $D/log
   log level = 0
   server min protocol = SMB2_02
   server max protocol = $1
   server signing = mandatory
   smb encrypt = $2
[share]
   path = $D/share
   read only = no
   valid users = rsmbtest
[hidden$]
   path = $D/share
   read only = yes
   valid users = rsmbtest
EOC
}
conf SMB3_11 default
(echo "Sekret1!"; echo "Sekret1!") | smbpasswd -c $D/smb.conf -s -a rsmbtest >/dev/null 2>&1
run() { # label max_protocol encrypt expect_rc password
   # smbd and the samba-dcerpcd it spawned for this config; a stale
   # rpc daemon from a previous run answers the srvsvc pipe for nobody
   stop; sleep 0.3
   conf $2 $3
   smbd -s $D/smb.conf -D
   # wait for the listener rather than guessing a delay
   i=0; while [ $i -lt 50 ]; do
      $RUN ./smb_test$EXE 127.0.0.1 share rsmbtest 'Sekret1!' RETRO > $D/probe 2>&1 && break
      grep -q "connect failed" $D/probe || break
      i=$((i + 1)); sleep 0.2
   done
   rm -f $D/share/rsmb_test.bin
   set +e; $RUN ./smb_test$EXE 127.0.0.1 share rsmbtest "$5" RETRO >$D/out 2>&1; rc=$?
   # the real VFS backend on top, on the file the run above wrote,
   # then four threads on a two-slot pool through it
   [ $rc -eq 0 ] && { $RUN ./vfs_test$EXE 127.0.0.1 share rsmbtest "$5" RETRO >>$D/out 2>&1 || rc=3; }
   [ $rc -eq 0 ] && { $RUN ./vfs_threads_test$EXE 127.0.0.1 share rsmbtest "$5" RETRO >>$D/out 2>&1 || rc=4; }
   set -e
   if [ $rc -eq $4 ]; then echo "ok:   $1"; else echo "FAIL: $1 (rc=$rc, want $4): $(cat $D/out)"; exit 1; fi
   rm -f $D/share/rsmb_test.bin
}
run "SMB 2.0.2, HMAC-SHA256 signing"        SMB2_02 default  0 'Sekret1!'
run "SMB 2.1, HMAC-SHA256 signing"          SMB2_10 default  0 'Sekret1!'
run "SMB 3.0, AES-CMAC signing"             SMB3_00 default  0 'Sekret1!'
run "SMB 3.0.2, AES-CMAC signing"           SMB3_02 default  0 'Sekret1!'
run "SMB 3.1.1, preauth + AES-CMAC signing" SMB3_11 default  0 'Sekret1!'
run "SMB 3.0.2, AES-CCM sealing required"   SMB3_02 required 0 'Sekret1!'
run "SMB 3.1.1, AES-GCM sealing required"   SMB3_11 required 0 'Sekret1!'
# A guest / anonymous session is returned unsigned even under mandatory
# signing; the client must drop signing for it, not read the unsigned
# SUCCESS as a bad signature.
guestconf() {
cat > $D/smb.conf << EOC
[global]
   workgroup = RETRO
   server role = standalone server
   security = user
   map to guest = Bad User
   guest account = nobody
   smb ports = 445
   interfaces = lo
   bind interfaces only = yes
   pid directory = $D/run
   lock directory = $D/run
   state directory = $D/run
   cache directory = $D/run
   private dir = $D/priv
   log file = $D/log
   log level = 0
   server min protocol = SMB2_02
   server max protocol = SMB2_02
   server signing = mandatory
[public]
   path = $D/share
   read only = no
   guest ok = yes
   guest only = yes
   force user = root
EOC
}
stop; sleep 0.3; guestconf; smbd -s $D/smb.conf -D
i=0; while [ $i -lt 50 ] && ! $RUN ./smb_test$EXE 127.0.0.1 public nobody x RETRO >$D/out 2>&1; do
   grep -q "bad signature" $D/out && break
   i=$((i + 1)); sleep 0.2
done
if $RUN ./smb_test$EXE 127.0.0.1 public nobody x RETRO >$D/out 2>&1; then
   echo "ok:   guest session (unsigned under mandatory signing)"
else
   echo "FAIL: guest session: $(cat $D/out)"; exit 1
fi

run "wrong password is refused"             SMB3_11 default  1 'wrong'
echo "[pass] smb_retro local samba matrix"
