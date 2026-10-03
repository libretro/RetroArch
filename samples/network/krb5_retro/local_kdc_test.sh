#!/bin/sh
# The built-in Kerberos client against a local MIT KDC: realm
# RETRO.TEST on a fixed port, one user with preauthentication
# required, one cifs/ service whose key is read back from its keytab
# to prove the issued ticket is real. Skips when krb5kdc is not
# installed. Wine works too (CC=..-mingw.. RUNNER=wine EXE=.exe).
set -eu
cd "$(dirname "$0")"
KDC=$(command -v krb5kdc || true)
[ -n "$KDC" ] && command -v kadmin.local >/dev/null && command -v kdb5_util >/dev/null \
   && command -v klist >/dev/null || { echo "skip: krb5-kdc / krb5-admin-server / krb5-user not installed"; exit 0; }
make -s krb5_test
RUN=${RUNNER:-}
EXE=${EXE:-}
PORT=20088
D=$(mktemp -d)
export KRB5_CONFIG=$D/krb5.conf KRB5_KDC_PROFILE=$D/kdc.conf
cat > $D/krb5.conf << EOF
[libdefaults]
    default_realm = RETRO.TEST
    dns_lookup_kdc = false
    dns_lookup_realm = false
[realms]
    RETRO.TEST = { kdc = 127.0.0.1:$PORT  database_name = $D/principal  key_stash_file = $D/stash  acl_file = $D/kadm5.acl }
[logging]
    kdc = FILE:$D/kdc.log
EOF
cat > $D/kdc.conf << EOF
[kdcdefaults]
    kdc_listen = 127.0.0.1:$PORT
    kdc_tcp_listen = 127.0.0.1:$PORT
[realms]
    RETRO.TEST = {
        database_name = $D/principal
        key_stash_file = $D/stash
        acl_file = $D/kadm5.acl
        kdc_listen = 127.0.0.1:$PORT
        kdc_tcp_listen = 127.0.0.1:$PORT
        supported_enctypes = aes256-cts-hmac-sha1-96:normal aes128-cts-hmac-sha1-96:normal
    }
EOF
: > $D/kadm5.acl
kdb5_util create -r RETRO.TEST -s -P masterpw > $D/setup.log 2>&1
kadmin.local -q "addprinc -pw Sekret1! +requires_preauth player" >> $D/setup.log 2>&1
kadmin.local -q "addprinc -pw Open1234 -requires_preauth guest" >> $D/setup.log 2>&1
kadmin.local -q "addprinc -randkey cifs/fileserver.retro.test" >> $D/setup.log 2>&1
kadmin.local -q "ktadd -k $D/cifs.keytab cifs/fileserver.retro.test" >> $D/setup.log 2>&1
KEY=$(klist -k -K -e $D/cifs.keytab | grep aes256 | sed 's/.*(0x\([0-9a-f]*\)).*/\1/' | head -1)
$KDC -n -P $D/kdc.pid > $D/kdc.out 2>&1 &
SRV=$!
cleanup() { kill $SRV 2>/dev/null || true; sleep 0.5; rm -rf $D; }
trap cleanup EXIT
i=0; while [ $i -lt 50 ]; do
   $RUN ./krb5_test$EXE RETRO.TEST 127.0.0.1 $PORT player 'Sekret1!' cifs/fileserver.retro.test $KEY > $D/res 2>&1 && break
   grep -q "connect failed" $D/res || break
   i=$((i + 1)); sleep 0.2
done
if grep -q "^ok:" $D/res && grep -q NEEDED_PREAUTH $D/kdc.log; then
   echo "ok:   AS with PA-ENC-TIMESTAMP, TGS for cifs/, ticket verified with the service key"
else
   echo "FAIL: $(cat $D/res)"; exit 1
fi
if $RUN ./krb5_test$EXE RETRO.TEST 127.0.0.1 $PORT guest Open1234 cifs/fileserver.retro.test $KEY > $D/res 2>&1; then
   echo "ok:   AS without preauthentication"
else
   echo "FAIL: $(cat $D/res)"; exit 1
fi
if $RUN ./krb5_test$EXE RETRO.TEST 127.0.0.1 $PORT player wrong cifs/fileserver.retro.test > $D/res 2>&1; then
   echo "FAIL: wrong password was accepted"; exit 1
elif grep -q "24" $D/res; then
   echo "ok:   wrong password is refused (KDC error 24)"
else
   echo "FAIL: $(cat $D/res)"; exit 1
fi
if $RUN ./krb5_test$EXE RETRO.TEST 127.0.0.1 $PORT guest wrongpw cifs/fileserver.retro.test > $D/res 2>&1; then
   echo "FAIL: wrong password without preauth was accepted"; exit 1
else
   echo "ok:   wrong password without preauth: AS-REP does not decrypt"
fi
if $RUN ./krb5_test$EXE RETRO.TEST 127.0.0.1 $PORT player 'Sekret1!' cifs/nowhere.retro.test > $D/res 2>&1; then
   echo "FAIL: unknown service was issued"; exit 1
else
   echo "ok:   unknown service principal is refused"
fi
echo "[pass] krb5_retro local KDC matrix"
