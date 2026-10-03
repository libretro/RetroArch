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
KRB5_CONFIG=$D/private/krb5.conf samba -F -s $D/etc/smb.conf > $D/samba.out 2>&1 &
i=0; while [ $i -lt 100 ]; do
   samba-tool user create player 'Sekret1!!' -s $D/etc/smb.conf > $D/user.log 2>&1 && break
   i=$((i + 1)); sleep 0.3
done
grep -q "added successfully" $D/user.log || { echo "FAIL: user create: $(cat $D/user.log)"; exit 1; }
# Samba's own client as the oracle for the environment: when it
# cannot log in to this DC with a ticket either, the DC is not usable
# here and the lane skips with the reason; when it can and ours
# cannot, that is a real failure.
oracle() {
   command -v smbclient >/dev/null 2>&1 && command -v kinit >/dev/null 2>&1 || return 2
   cat > $D/krb5-cli.conf << EOF
[libdefaults]
    default_realm = AD.RETRO.TEST
    dns_lookup_kdc = false
    dns_lookup_realm = false
[realms]
    AD.RETRO.TEST = {
        kdc = 127.0.0.1:88
    }
EOF
   KRB5_CONFIG=$D/krb5-cli.conf KRB5CCNAME=$D/cc sh -c "echo 'Sekret1!!' | kinit player >/dev/null 2>&1" || return 1
   KRB5_CONFIG=$D/krb5-cli.conf KRB5CCNAME=$D/cc smbclient --use-kerberos=required -N //$DCNAME/games -c ls > $D/oracle.out 2>&1
}
# (an if, not a bare call: under set -e a failing oracle would end the
# script before it could say why)
orc=1
i=0; while [ $i -lt 100 ]; do
   if oracle; then orc=0; else orc=$?; fi
   [ $orc -eq 0 ] && break
   [ $orc -eq 2 ] && break
   i=$((i + 1)); sleep 0.3
done
# Everything the failure needs to be understood, printed by a helper
# that set -e cannot cut short (a log that does not exist yet must not
# end the report).
dc_report() {
   set +e
   echo "--- smbclient:"; [ -f $D/oracle.out ] && tail -5 $D/oracle.out
   echo "--- klist:"; KRB5_CONFIG=$D/krb5-cli.conf KRB5CCNAME=$D/cc klist 2>&1 | head -5
   echo "--- samba.out:"; [ -f $D/samba.out ] && tail -10 $D/samba.out
   for l in $D/log.samba $D/log.smbd $D/log.winbindd $D/log.kdc; do
      [ -f $l ] && { echo "--- $(basename $l):"; tail -25 $l; }
   done
   return 0
}
if [ $orc -eq 1 ]; then
   echo "skip: Samba's own client cannot log in to the DC with a ticket here:"
   dc_report
   exit 0
fi
export SMB_KRB_REALM=AD.RETRO.TEST SMB_KRB_KDC=127.0.0.1 SMB_KRB_REQUIRE=1
# the DC's KDC and file server come up in their own time after the
# user exists: retry a refused connect or session setup for a while
i=0; while [ $i -lt 100 ]; do
   $RUN ./smb_test$EXE $DCNAME games player 'Sekret1!!' > $D/out 2>&1 && break
   grep -qE "connect failed|session setup failed|logon refused|logon failure" $D/out || break
   i=$((i + 1)); sleep 0.3
done
if grep -q "^ok:" $D/out; then
   echo "ok:   Kerberos login to the AD DC (SMB 3.1.1, signed, NTLM disabled on the server)"
else
   echo "FAIL: $(cat $D/out)"
   dc_report
   exit 1
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
