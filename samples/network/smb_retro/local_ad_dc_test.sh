#!/bin/sh
# The built-in SMB client with Kerberos against a Samba Active
# Directory domain controller provisioned on the spot: its KDC issues
# the tickets, its file server takes them in SESSION_SETUP, and NTLM is
# disabled on it so nothing but a ticket can log in. Needs samba (the
# AD DC role, samba-tool, winbindd) and root for the DC's ports (88,
# 445 on the loopback) and a hosts entry for the DC name; skips without
# them. Wine works too (CC=..-mingw.. RUNNER=wine EXE=.exe).
set -eu
cd "$(dirname "$0")"
command -v samba >/dev/null 2>&1 && command -v samba-tool >/dev/null 2>&1 \
   && [ -x /usr/sbin/winbindd ] || { echo "skip: no samba AD DC (samba, samba-tool, winbindd)"; exit 0; }
[ "$(id -u)" -eq 0 ] || { echo "skip: the AD DC needs root for its ports"; exit 0; }
make -s smb_test
RUN=${RUNNER:-}
EXE=${EXE:-}
DCNAME=dc1.ad.retro.test
# Provisioning sets POSIX ACLs on SYSVOL, so the DC has to live on a
# filesystem that takes them: the temp directory first, then the
# repository's own filesystem, then /var/tmp. Probed with setfacl
# (package acl); without any ACL-capable place the matrix is skipped
# rather than failed.
acl_dir() {
   d=$(mktemp -d "$1/ad_dc.XXXXXX" 2>/dev/null) || return 1
   if command -v setfacl >/dev/null 2>&1; then
      if ! setfacl -m u:0:rwx "$d" >/dev/null 2>&1; then rmdir "$d"; return 1; fi
   fi
   echo "$d"
}
D=$(acl_dir "${TMPDIR:-/tmp}" || acl_dir "$(pwd)" || acl_dir /var/tmp || true)
[ -n "$D" ] || { echo "skip: no directory with POSIX ACL support for the AD DC"; exit 0; }
chmod 755 $D; mkdir -p $D/share; chmod 777 $D/share
stop() { pkill -f "samba -F -s $D/etc/smb.conf" 2>/dev/null || true; pkill -f "configfile=$D/etc/smb.conf" 2>/dev/null || true; sleep 0.5; }
cleanup() { stop; rm -rf $D; }
trap cleanup EXIT
# nothing else may hold 445 or 88
systemctl stop smbd nmbd samba-ad-dc winbind 2>/dev/null || true
pkill -x smbd 2>/dev/null || true; pkill -x samba 2>/dev/null || true; pkill -x winbindd 2>/dev/null || true
pkill -f samba-dcerpcd 2>/dev/null || true; pkill -f rpcd_ 2>/dev/null || true
sleep 0.5
grep -q "$DCNAME" /etc/hosts || echo "127.0.0.1 $DCNAME" >> /etc/hosts
samba-tool domain provision --realm=AD.RETRO.TEST --domain=ADRETRO --server-role=dc \
   --dns-backend=NONE --adminpass='Adm1n!!pass' --targetdir=$D --host-name=dc1 --use-rfc2307 > $D/provision.log 2>&1 \
   || { echo "FAIL: provision:"; tail -15 $D/provision.log; exit 1; }
cat >> $D/etc/smb.conf << EOF

[global]
	bind interfaces only = yes
	interfaces = 127.0.0.1
	server services = s3fs, rpc, ldap, cldap, kdc, winbindd
	log file = $D/log.%m
	log level = 1
	smb ports = 445
	server signing = mandatory
	ntlm auth = disabled

[games]
	path = $D/share
	read only = no
	guest ok = no
EOF
samba -F -s $D/etc/smb.conf > $D/samba.out 2>&1 &
i=0; while [ $i -lt 100 ]; do
   samba-tool user create player 'Sekret1!!' -s $D/etc/smb.conf > $D/user.log 2>&1 && break
   i=$((i + 1)); sleep 0.3
done
grep -q "added successfully" $D/user.log || { echo "FAIL: user create: $(cat $D/user.log)"; exit 1; }
export SMB_KRB_REALM=AD.RETRO.TEST SMB_KRB_KDC=127.0.0.1 SMB_KRB_REQUIRE=1
i=0; while [ $i -lt 50 ]; do
   $RUN ./smb_test$EXE $DCNAME games player 'Sekret1!!' > $D/out 2>&1 && break
   grep -q "connect failed" $D/out || break
   i=$((i + 1)); sleep 0.3
done
if grep -q "^ok:" $D/out; then
   echo "ok:   Kerberos login to the AD DC (SMB 3.1.1, signed, NTLM disabled on the server)"
else
   echo "FAIL: $(cat $D/out)"; exit 1
fi
if $RUN ./smb_test$EXE $DCNAME games player wrong > $D/out 2>&1; then
   echo "FAIL: wrong password was accepted"; exit 1
elif grep -q "wrong password" $D/out; then
   echo "ok:   wrong password refused by the KDC"
else
   echo "FAIL: $(cat $D/out)"; exit 1
fi
if $RUN ./smb_test$EXE $DCNAME games nobody x > $D/out 2>&1; then
   echo "FAIL: unknown user was accepted"; exit 1
else
   echo "ok:   unknown user refused"
fi
unset SMB_KRB_REALM SMB_KRB_KDC SMB_KRB_REQUIRE
if $RUN ./smb_test$EXE $DCNAME games player 'Sekret1!!' > $D/out 2>&1; then
   echo "FAIL: NTLM was accepted by a DC with NTLM disabled"; exit 1
else
   echo "ok:   NTLMSSP alone is refused by this DC (so the login above was the ticket)"
fi
rm -f $D/share/*
echo "[pass] smb_retro local AD DC matrix"
