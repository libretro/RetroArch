#!/bin/sh
# net_smb2.c against a local smbd: every dialect 2.0.2 to 3.1.1 with
# signing mandatory, sealing required on 3.0.2 (AES-CCM) and 3.1.1
# (AES-GCM), and the refusals that must happen. Needs samba and root
# (smbd binds 445 on the loopback only). Skips, and passes, without them.
set -e
cd "$(dirname "$0")"
command -v smbd >/dev/null 2>&1 || { echo "skip: no smbd"; exit 0; }
[ "$(id -u)" = 0 ] || { echo "skip: needs root for port 445"; exit 0; }
make -s smb_test vfs_test
D=$(mktemp -d); chmod 755 $D; mkdir -p $D/share $D/priv $D/run /run/samba
trap 'pkill -f "smbd -s $D/smb.conf" 2>/dev/null || true; rm -rf $D' EXIT
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
EOC
}
conf SMB3_11 default
(echo "Sekret1!"; echo "Sekret1!") | smbpasswd -c $D/smb.conf -s -a rsmbtest >/dev/null 2>&1
run() { # label max_protocol encrypt expect_rc password
   pkill -f "smbd -s $D/smb.conf" 2>/dev/null || true; sleep 0.3
   conf $2 $3
   smbd -s $D/smb.conf -D
   # wait for the listener rather than guessing a delay
   i=0; while [ $i -lt 50 ]; do
      ./smb_test 127.0.0.1 share rsmbtest 'Sekret1!' RETRO > $D/probe 2>&1 && break
      grep -q "connect failed" $D/probe || break
      i=$((i + 1)); sleep 0.2
   done
   rm -f $D/share/rsmb_test.bin
   set +e; ./smb_test 127.0.0.1 share rsmbtest "$5" RETRO >$D/out 2>&1; rc=$?
   # the real VFS backend on top, on the file the run above wrote
   [ $rc -eq 0 ] && { ./vfs_test 127.0.0.1 share rsmbtest "$5" RETRO >>$D/out 2>&1 || rc=3; }
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
run "wrong password is refused"             SMB3_11 default  1 'wrong'
echo "[pass] smb_retro local samba matrix"
