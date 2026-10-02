#!/bin/sh
# Handshakes against a local openssl s_server: RSA / P-256 / P-384
# certificates, AES-GCM and ChaCha20-Poly1305, secp256r1 and secp384r1
# key exchange, verify required against a CA made here, plus the
# failures that must fail (wrong host, untrusted CA). Needs openssl.
set -e
command -v openssl >/dev/null 2>&1 || { echo "skip: no openssl"; exit 0; }
D=$(mktemp -d); trap 'rm -rf $D; kill $SRV 2>/dev/null || true' EXIT
cd "$(dirname "$0")"
make -s tls_fetch
# RUNNER prefixes every tool invocation: empty natively, "wine" for a
# MinGW build of the tools
RUN=${RUNNER:-}
EXE=${EXE:-}
openssl req -x509 -newkey rsa:2048 -nodes -keyout $D/ca.key -out $D/ca.pem -days 2 -subj "/CN=tls_retro test CA" 2>/dev/null
mkcert() { # name keyalg [pkeyopt]
   openssl req -newkey "$2" $3 -nodes -keyout $D/$1.key -out $D/$1.csr -subj "/CN=$1" 2>/dev/null
   printf 'subjectAltName=DNS:localhost\nbasicConstraints=CA:FALSE\n' > $D/$1.ext
   openssl x509 -req -in $D/$1.csr -CA $D/ca.pem -CAkey $D/ca.key -CAcreateserial -out $D/$1.pem -days 2 -extfile $D/$1.ext 2>/dev/null
}
mkcert rsa rsa:2048
mkcert p256 ec "-pkeyopt ec_paramgen_curve:prime256v1"
mkcert p384 ec "-pkeyopt ec_paramgen_curve:secp384r1"
# leaf policy: certificates a correctly chaining, in-date, host-matching
# server may still not present - an unknown critical extension, an EKU
# without serverAuth (client-auth only, code-signing only), a keyUsage
# that cannot sign - all refused; and the same shapes with a serverAuth
# EKU or an unknown *non-critical* extension accepted
mkleafx() { # name extfile-lines
   openssl req -newkey rsa:2048 -nodes -keyout $D/$1.key -out $D/$1.csr -subj "/CN=localhost" 2>/dev/null
   printf 'subjectAltName=DNS:localhost\nbasicConstraints=CA:FALSE\n%b' "$2" > $D/$1.ext
   openssl x509 -req -in $D/$1.csr -CA $D/ca.pem -CAkey $D/ca.key -CAcreateserial -out $D/$1.pem -days 2 -extfile $D/$1.ext 2>/dev/null
}
mkleafx critunknown 'extendedKeyUsage=serverAuth\n1.2.3.4.5=critical,DER:05:00\n'
mkleafx noncritunknown 'extendedKeyUsage=serverAuth\n1.2.3.4.5=DER:05:00\n'
mkleafx clientonly 'extendedKeyUsage=clientAuth\n'
mkleafx codesign 'extendedKeyUsage=codeSigning\n'
mkleafx serverauth 'extendedKeyUsage=serverAuth,clientAuth\nkeyUsage=digitalSignature,keyEncipherment\n'
mkleafx nosign 'keyUsage=keyEncipherment,dataEncipherment\n'
# IP addresses: matched against iPAddress SAN entries only. One leaf
# carries IP:127.0.0.1, the other the same address as a DNS name, which
# must not vouch for the address.
mkleafip() { # name san
   openssl req -newkey rsa:2048 -nodes -keyout $D/$1.key -out $D/$1.csr -subj "/CN=127.0.0.1" 2>/dev/null
   printf 'subjectAltName=%s\nbasicConstraints=CA:FALSE\n' "$2" > $D/$1.ext
   openssl x509 -req -in $D/$1.csr -CA $D/ca.pem -CAkey $D/ca.key -CAcreateserial -out $D/$1.pem -days 2 -extfile $D/$1.ext 2>/dev/null
}
mkleafip ipsan "IP:127.0.0.1"
mkleafip ipdns "DNS:127.0.0.1"
# a real-world shape: root -> intermediate CA -> leaf, the server sending
# leaf + intermediate and the client trusting only the root; one with an
# RSA intermediate, one with a P-384 intermediate under the RSA root
mkinter() { # name keyalg [pkeyopt]
   openssl req -newkey "$2" $3 -nodes -keyout $D/$1.key -out $D/$1.csr -subj "/CN=$1 intermediate" 2>/dev/null
   printf 'basicConstraints=critical,CA:TRUE,pathlen:0\nkeyUsage=critical,keyCertSign,cRLSign\n' > $D/$1.ext
   openssl x509 -req -in $D/$1.csr -CA $D/ca.pem -CAkey $D/ca.key -CAcreateserial -out $D/$1.pem -days 2 -extfile $D/$1.ext 2>/dev/null
}
mkleaf() { # name inter
   openssl req -newkey rsa:2048 -nodes -keyout $D/$1.key -out $D/$1.csr -subj "/CN=$1" 2>/dev/null
   printf 'subjectAltName=DNS:localhost\nbasicConstraints=CA:FALSE\n' > $D/$1.ext
   openssl x509 -req -in $D/$1.csr -CA $D/$2.pem -CAkey $D/$2.key -CAcreateserial -out $D/$1.pem -days 2 -extfile $D/$1.ext 2>/dev/null
   cp $D/$2.pem $D/$1.chain.pem   # what the server sends along with the leaf
}
mkinter irsa rsa:2048
mkinter ip384 ec "-pkeyopt ec_paramgen_curve:secp384r1"
mkleaf viarsa irsa
mkleaf viap384 ip384
run() { # label cert cipher curve expect_rc host mode ca [rounds] [server opts]
   CHAIN=""; [ -f $D/$2.chain.pem ] && CHAIN="-cert_chain $D/$2.chain.pem"
   openssl s_server -accept 44331 -cert $D/$2.pem $CHAIN -key $D/$2.key -tls1_2 -cipher "$3" -named_curve $4 -www ${10:-} >/dev/null 2>&1 &
   SRV=$!; sleep 0.4
   set +e; $RUN ./tls_fetch$EXE $6 44331 $7 "$8" ${9:-1} >/dev/null 2>&1; rc=$?; set -e
   kill $SRV 2>/dev/null; wait $SRV 2>/dev/null || true
   if [ $rc -eq $5 ]; then echo "ok:   $1"; else echo "FAIL: $1 (rc=$rc, want $5)"; exit 1; fi
}
run "RSA, AES-128-GCM, P-256, verify required"      rsa  ECDHE-RSA-AES128-GCM-SHA256      prime256v1 0 localhost 0 $D/ca.pem
run "RSA, ChaCha20-Poly1305, P-384 kex"              rsa  ECDHE-RSA-CHACHA20-POLY1305      secp384r1  0 localhost 0 $D/ca.pem
run "ECDSA P-256 cert, AES-128-GCM"                 p256 ECDHE-ECDSA-AES128-GCM-SHA256    prime256v1 0 localhost 0 $D/ca.pem
run "ECDSA P-256 cert, ChaCha20-Poly1305"           p256 ECDHE-ECDSA-CHACHA20-POLY1305    prime256v1 0 localhost 0 $D/ca.pem
run "ECDSA P-384 cert (SHA-384 sig), AES-128-GCM"   p384 ECDHE-ECDSA-AES128-GCM-SHA256    secp384r1  0 localhost 0 $D/ca.pem
run "wrong hostname is refused"                     rsa  ECDHE-RSA-AES128-GCM-SHA256      prime256v1 1 127.0.0.1 0 $D/ca.pem
run "untrusted CA is refused"                       rsa  ECDHE-RSA-AES128-GCM-SHA256      prime256v1 1 localhost 0
run "untrusted CA passes with verify optional"      rsa  ECDHE-RSA-AES128-GCM-SHA256      prime256v1 0 localhost 1
run "RSA, AES-256-GCM-SHA384 (SHA-384 PRF)"          rsa  ECDHE-RSA-AES256-GCM-SHA384      prime256v1 0 localhost 0 $D/ca.pem
run "ECDSA P-256, AES-256-GCM-SHA384"                p256 ECDHE-ECDSA-AES256-GCM-SHA384    prime256v1 0 localhost 0 $D/ca.pem
run "server offering only a CBC suite fails"        rsa  ECDHE-RSA-AES128-SHA256          prime256v1 1 localhost 2
run "session resumption by ticket (3 rounds)"     rsa  ECDHE-RSA-AES128-GCM-SHA256      prime256v1 0 localhost 0 $D/ca.pem 3
run "session resumption by id, no tickets"         rsa  ECDHE-RSA-AES128-GCM-SHA256      prime256v1 0 localhost 0 $D/ca.pem 3 -no_ticket
run "ECDSA + ChaCha20 resumes too"                 p256 ECDHE-ECDSA-CHACHA20-POLY1305    prime256v1 0 localhost 0 $D/ca.pem 2
run "policy: unknown critical extension is refused"      critunknown ECDHE-RSA-AES128-GCM-SHA256 prime256v1 1 localhost 0 $D/ca.pem
run "policy: unknown non-critical extension is fine"      noncritunknown ECDHE-RSA-AES128-GCM-SHA256 prime256v1 0 localhost 0 $D/ca.pem
run "policy: clientAuth-only EKU is refused"              clientonly ECDHE-RSA-AES128-GCM-SHA256 prime256v1 1 localhost 0 $D/ca.pem
run "policy: codeSigning-only EKU is refused"             codesign   ECDHE-RSA-AES128-GCM-SHA256 prime256v1 1 localhost 0 $D/ca.pem
run "policy: serverAuth EKU with digitalSignature is fine" serverauth ECDHE-RSA-AES128-GCM-SHA256 prime256v1 0 localhost 0 $D/ca.pem
run "policy: keyUsage without digitalSignature is refused" nosign    ECDHE-RSA-AES128-GCM-SHA256 prime256v1 1 localhost 0 $D/ca.pem
run "IP address matches an iPAddress entry"            ipsan ECDHE-RSA-AES128-GCM-SHA256 prime256v1 0 127.0.0.1 0 $D/ca.pem
run "IP address written as a DNS name is refused"      ipdns ECDHE-RSA-AES128-GCM-SHA256 prime256v1 1 127.0.0.1 0 $D/ca.pem
run "chain: root -> RSA intermediate -> leaf"      viarsa  ECDHE-RSA-AES128-GCM-SHA256 prime256v1 0 localhost 0 $D/ca.pem
run "chain: root -> P-384 intermediate -> leaf"    viap384 ECDHE-RSA-AES128-GCM-SHA256 prime256v1 0 localhost 0 $D/ca.pem
run "chain: trusting the intermediate directly also works" viarsa ECDHE-RSA-AES128-GCM-SHA256 prime256v1 0 localhost 0 $D/irsa.pem
cp $D/viarsa.pem $D/nointer.pem; cp $D/viarsa.key $D/nointer.key
run "chain: server omitting the intermediate is refused" nointer ECDHE-RSA-AES128-GCM-SHA256 prime256v1 1 localhost 0 $D/ca.pem
# TLS 1.3: the server picks it over 1.2 when offered; RSA certs sign the
# CertificateVerify with RSA-PSS, ECDSA with P-256; all three suites,
# the SHA-384 one on its own key schedule; the chain still has to
# verify.
run13() { # label cert ciphersuite expect_rc host mode ca [rounds] [server opts]
   CHAIN=""; [ -f $D/$2.chain.pem ] && CHAIN="-cert_chain $D/$2.chain.pem"
   openssl s_server -accept 44331 -cert $D/$2.pem $CHAIN -key $D/$2.key -tls1_3 -ciphersuites "$3" -www ${9:-} >/dev/null 2>&1 &
   SRV=$!; sleep 0.4
   set +e; $RUN ./tls_fetch$EXE $5 44331 $6 "$7" ${8:-1} >/dev/null 2>&1; rc=$?; set -e
   kill $SRV 2>/dev/null; wait $SRV 2>/dev/null || true
   if [ $rc -eq $4 ]; then echo "ok:   $1"; else echo "FAIL: $1 (rc=$rc, want $4)"; exit 1; fi
}
run13 "TLS 1.3, RSA cert (PSS), AES-128-GCM"           rsa  TLS_AES_128_GCM_SHA256       0 localhost 0 $D/ca.pem
run13 "TLS 1.3, RSA cert (PSS), ChaCha20-Poly1305"     rsa  TLS_CHACHA20_POLY1305_SHA256 0 localhost 0 $D/ca.pem
run13 "TLS 1.3, ECDSA P-256 cert, AES-128-GCM"         p256 TLS_AES_128_GCM_SHA256       0 localhost 0 $D/ca.pem
run13 "TLS 1.3, ECDSA P-256 cert, ChaCha20-Poly1305"   p256 TLS_CHACHA20_POLY1305_SHA256 0 localhost 0 $D/ca.pem
run13 "TLS 1.3, ECDSA P-384 cert (sha384 sig)"         p384 TLS_AES_128_GCM_SHA256       0 localhost 0 $D/ca.pem
run13 "TLS 1.3, chain through an intermediate"         viarsa TLS_AES_128_GCM_SHA256     0 localhost 0 $D/ca.pem
run13 "TLS 1.3, untrusted CA is refused"               rsa  TLS_AES_128_GCM_SHA256       1 localhost 0
run13 "TLS 1.3, wrong hostname is refused"             rsa  TLS_AES_128_GCM_SHA256       1 127.0.0.1 0 $D/ca.pem
run13 "TLS 1.3, AES-256-GCM-SHA384 (SHA-384 schedule)"   rsa  TLS_AES_256_GCM_SHA384       0 localhost 0 $D/ca.pem
run13 "TLS 1.3, AES-256-GCM-SHA384, ECDSA, PSK resumes"   p256 TLS_AES_256_GCM_SHA384       0 localhost 0 $D/ca.pem 3
# Groups: X25519 is the group most servers prefer; both handshakes
# take it, and a server allowing only one of the three still connects
run13 "TLS 1.3, X25519 only"                          rsa  TLS_AES_128_GCM_SHA256       0 localhost 0 $D/ca.pem 1 "-groups X25519"
run13 "TLS 1.3, P-256 only"                           rsa  TLS_AES_128_GCM_SHA256       0 localhost 0 $D/ca.pem 1 "-groups P-256"
run "TLS 1.2, X25519 key exchange"                    rsa  ECDHE-RSA-AES128-GCM-SHA256  X25519     0 localhost 0 $D/ca.pem
run "TLS 1.2, ECDSA cert, X25519 + ChaCha20"          p256 ECDHE-ECDSA-CHACHA20-POLY1305 X25519   0 localhost 0 $D/ca.pem
# PSK resumption: the ticket from the first connection resumes the next
# ones, on both suites; a server that issues no tickets means every
# connection is a full handshake, which the tool then reports as failure
run13 "TLS 1.3, PSK resumption (3 rounds)"           rsa  TLS_AES_128_GCM_SHA256       0 localhost 0 $D/ca.pem 3
run13 "TLS 1.3, PSK resumption, ChaCha20 (2 rounds)" p256 TLS_CHACHA20_POLY1305_SHA256 0 localhost 0 $D/ca.pem 2
run13 "TLS 1.3, no tickets issued: no resumption"    rsa  TLS_AES_128_GCM_SHA256       1 localhost 0 $D/ca.pem 2 "-num_tickets 0"
# KeyUpdate, both kinds: s_server's console sends one with update
# requested (K) and one without (k) between lines; the client must keep
# reading across each and the server must decrypt what we send after
# rotating our own keys on the requested one.
make -s tls_keyupdate
(sleep 1.5; echo hello; sleep 0.7; echo K; sleep 0.7; echo after-K; sleep 0.7; echo k; sleep 0.7; echo after-k; sleep 2) \
   | openssl s_server -accept 44331 -cert $D/rsa.pem -key $D/rsa.key -tls1_3 > $D/ku.out 2>&1 &
SRV=$!; sleep 0.6
set +e; $RUN ./tls_keyupdate$EXE localhost 44331 $D/ca.pem > $D/ku.client 2>&1; rc=$?; set -e
sleep 2.5; kill $SRV 2>/dev/null || true; wait $SRV 2>/dev/null || true
if [ $rc -eq 0 ] && grep -q "^ack-after-K" $D/ku.out && grep -q "^ack-after-k" $D/ku.out; then
   echo "ok:   TLS 1.3, KeyUpdate requested and unrequested, both directions"
else
   echo "FAIL: KeyUpdate: $(cat $D/ku.client) / server saw: $(grep '^ack' $D/ku.out | tr '\n' ' ')"; exit 1
fi
# concurrent first use: six threads verify against a store none of them
# has built yet, and with an ECDSA server compute the curve constants
# none of them has computed yet; under TSan when the tools are built
# with it
make -s tls_threads
for key in rsa p256; do
   openssl s_server -accept 44331 -cert $D/$key.pem -key $D/$key.key -www >/dev/null 2>&1 &
   SRV=$!; sleep 0.4
   set +e; $RUN ./tls_threads$EXE localhost 44331 $D/ca.pem > $D/thr.out 2>&1; rc=$?; set -e
   kill $SRV 2>/dev/null; wait $SRV 2>/dev/null || true
   if [ $rc -eq 0 ]; then echo "ok:   concurrent first-use verification from six threads ($key)"; else echo "FAIL: threads ($key): $(cat $D/thr.out)"; exit 1; fi
done
# Without AES instructions the client offers ChaCha20-Poly1305 first,
# which s_server, taking the client's order, then picks on both versions
make -s tls_fetch_noaes
noaes() { # label version-option expected-suite
   openssl s_server -accept 44331 -cert $D/rsa.pem -key $D/rsa.key $2 -www >/dev/null 2>&1 &
   SRV=$!; sleep 0.4
   set +e; TLS_FETCH_SUITE=1 $RUN ./tls_fetch_noaes$EXE localhost 44331 0 $D/ca.pem > $D/noaes.out 2>&1; rc=$?; set -e
   kill $SRV 2>/dev/null; wait $SRV 2>/dev/null || true
   if [ $rc -eq 0 ] && grep -q "^suite $3" $D/noaes.out; then echo "ok:   $1"
   else echo "FAIL: $1 (rc=$rc): $(cat $D/noaes.out)"; exit 1; fi
}
noaes "no AES instructions: TLS 1.3 negotiates ChaCha20-Poly1305" -tls1_3 1303
noaes "no AES instructions: TLS 1.2 negotiates ChaCha20-Poly1305" -tls1_2 cca8
# RetroArch's HTTP client over the built-in TLS: net_http fetches a file
# of 3 MiB and change, whose records straddle its read windows at odd
# offsets, on both versions and both AEADs; the body must hash right.
make -s tls_http
mkdir -p $D/www
head -c 3158073 /dev/urandom > $D/www/body.bin
SUM=$(openssl dgst -sha256 -r $D/www/body.bin | cut -d' ' -f1)
http() { # label version cipher-option cipher
   (cd $D/www && exec openssl s_server -accept 44331 -cert $D/rsa.pem -key $D/rsa.key $2 $3 "$4" -WWW) >/dev/null 2>&1 &
   SRV=$!; sleep 0.4
   set +e; $RUN ./tls_http$EXE 44331 body.bin $SUM $D/ca.pem > $D/http.out 2>&1; rc=$?; set -e
   kill $SRV 2>/dev/null; wait $SRV 2>/dev/null || true
   if [ $rc -eq 0 ]; then echo "ok:   $1"; else echo "FAIL: $1: $(cat $D/http.out)"; exit 1; fi
}
http "net_http over TLS 1.2, AES-128-GCM"        -tls1_2 -cipher       ECDHE-RSA-AES128-GCM-SHA256
http "net_http over TLS 1.2, ChaCha20-Poly1305"  -tls1_2 -cipher       ECDHE-RSA-CHACHA20-POLY1305
http "net_http over TLS 1.3, AES-128-GCM"        -tls1_3 -ciphersuites TLS_AES_128_GCM_SHA256
http "net_http over TLS 1.3, ChaCha20-Poly1305"  -tls1_3 -ciphersuites TLS_CHACHA20_POLY1305_SHA256
echo "[pass] tls_retro local server matrix"
