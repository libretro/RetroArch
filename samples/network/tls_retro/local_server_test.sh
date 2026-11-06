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
# One connection's own settings (ssl_socket_retro_configure): its own
# trust anchor in place of the bundle, a client certificate when the
# server asks for one, and the RSA key exchange and CBC suites, which a
# client offers only when told to. Each GET checks the body's hash, so
# the CBC records' MAC and padding are checked in both directions.
make -s tls_config
openssl x509 -in $D/ca.pem -outform DER -out $D/ca.der
openssl x509 -in $D/p256.pem -outform DER -out $D/other.der
openssl req -x509 -newkey rsa:2048 -nodes -keyout $D/cca.key -out $D/cca.pem -days 2 -subj "/CN=client CA" 2>/dev/null
openssl req -newkey rsa:2048 -nodes -keyout $D/cli.key -out $D/cli.csr -subj "/CN=client" 2>/dev/null
openssl x509 -req -in $D/cli.csr -CA $D/cca.pem -CAkey $D/cca.key -CAcreateserial -out $D/cli.pem -days 2 2>/dev/null
openssl x509 -in $D/cli.pem -outform DER -out $D/cli.der
openssl rsa -in $D/cli.key -traditional -outform DER -out $D/cli.key.der 2>/dev/null
openssl pkcs8 -topk8 -nocrypt -in $D/cli.key -outform DER -out $D/cli.p8.der
mkdir -p $D/cfgwww
head -c 1000003 /dev/urandom > $D/cfgwww/b.bin
CSUM=$(openssl dgst -sha256 -r $D/cfgwww/b.bin | cut -d' ' -f1)
cfg() { # label expect_rc want_output server-options -- client-options
   L=$1; W=$2; O=$3; shift 3
   SOPT=""; while [ "$1" != "--" ]; do SOPT="$SOPT $1"; shift; done; shift
   (cd $D/cfgwww && exec openssl s_server -accept 44331 -cert $D/rsa.pem -key $D/rsa.key $SOPT -WWW) >/dev/null 2>&1 &
   SRV=$!; sleep 0.4
   set +e; $RUN ./tls_config$EXE "$@" > $D/cfg.out 2>&1; rc=$?; set -e
   kill $SRV 2>/dev/null; wait $SRV 2>/dev/null || true
   if [ $rc -eq $W ] && grep -q "$O" $D/cfg.out; then echo "ok:   $L"
   else echo "FAIL: $L (rc=$rc, want $W): $(cat $D/cfg.out)"; exit 1; fi
}
G="-g b.bin $CSUM"
cfg "own trust anchor in place of the bundle"         0 "TLS 1.3" -- localhost 44331 -t $D/ca.der $G
cfg "own trust anchor that signed nothing: refused"   1 "FAILED" -- localhost 44331 -t $D/other.der $G
cfg "no hostname check: an address passes"            0 "TLS 1.3" -- 127.0.0.1 44331 -t $D/ca.der -H $G
cfg "hostname still checked otherwise"                1 "FAILED" -- 127.0.0.1 44331 -t $D/ca.der $G
cfg "no verification"                                 0 "TLS 1.3" -- localhost 44331 -n $G
cfg "a suite list without a known suite: refused"     1 "refused" -- localhost 44331 -t $D/ca.der -s 0005
for s in AES128-SHA:002f AES256-SHA:0035 AES128-SHA256:003c AES256-SHA256:003d \
      AES128-GCM-SHA256:009c AES256-GCM-SHA384:009d ECDHE-RSA-AES128-SHA:c013 \
      ECDHE-RSA-AES256-SHA:c014 ECDHE-RSA-AES128-SHA256:c027 ECDHE-RSA-AES256-SHA384:c028; do
   cfg "TLS 1.2 ${s%%:*}, offered by the client" 0 "suite ${s##*:}" -tls1_2 -cipher ${s%%:*} -- \
      localhost 44331 -t $D/ca.der -s ${s##*:} $G
done
cfg "client certificate the server requires"          0 "TLS 1.2 suite c02f" -Verify 1 -CAfile $D/cca.pem -- \
   localhost 44331 -t $D/ca.der -c $D/cli.der -k $D/cli.key.der $G
cfg "client certificate, PKCS#8 key"                  0 "TLS 1.2" -Verify 1 -CAfile $D/cca.pem -- \
   localhost 44331 -t $D/ca.der -c $D/cli.der -k $D/cli.p8.der $G
cfg "no client certificate where one is required"     1 "failed" -Verify 1 -CAfile $D/cca.pem -- \
   localhost 44331 -t $D/ca.der $G
cfg "client certificate requested, none to give"      0 "TLS 1.2" -tls1_2 -verify 1 -CAfile $D/cca.pem -- \
   localhost 44331 -t $D/ca.der $G
cfg "client certificate signed with SHA-384"          0 "TLS 1.2" -Verify 1 -CAfile $D/cca.pem -client_sigalgs RSA+SHA384 -- \
   localhost 44331 -t $D/ca.der -c $D/cli.der -k $D/cli.key.der $G
cfg "client certificate signed with SHA-1"            0 "TLS 1.2" -Verify 1 -CAfile $D/cca.pem -client_sigalgs RSA+SHA1 -cipher DEFAULT:@SECLEVEL=0 -- \
   localhost 44331 -t $D/ca.der -c $D/cli.der -k $D/cli.key.der $G
cfg "client certificate over RSA key exchange and AES-256-CBC-SHA" 0 "suite 0035" \
   -tls1_2 -cipher AES256-SHA -Verify 1 -CAfile $D/cca.pem -- \
   localhost 44331 -t $D/ca.der -c $D/cli.der -k $D/cli.key.der -s 0035 $G
# net_http with one connection's own settings (net_http_connection_set_tls):
# the 3 MiB body over a client certificate and AES-256-CBC. Such a
# connection is its own, never taken from or handed to the pool: a
# keep-alive server below logs each connection, and a GET with another
# trust anchor or none must fail or pass on its own settings, whatever
# the transfer before it left open.
openssl req -newkey rsa:2048 -nodes -keyout $D/untr.key -out $D/untr.csr -subj "/CN=localhost" 2>/dev/null
printf 'subjectAltName=DNS:localhost\nbasicConstraints=CA:FALSE\n' > $D/untr.ext
openssl x509 -req -in $D/untr.csr -CA $D/cca.pem -CAkey $D/cca.key -CAcreateserial -out $D/untr.pem -days 2 -extfile $D/untr.ext 2>/dev/null
openssl x509 -in $D/cca.pem -outform DER -out $D/cca.der
(cd $D/www && exec openssl s_server -accept 44331 -cert $D/rsa.pem -key $D/rsa.key -tls1_2 -cipher AES256-SHA -Verify 1 -CAfile $D/cca.pem -WWW) >/dev/null 2>&1 &
SRV=$!; sleep 0.4
set +e; $RUN ./tls_http$EXE 44331 body.bin $SUM $D/ca.pem -t $D/ca.der -c $D/cli.der -k $D/cli.key.der -s 0035 -g > $D/http.out 2>&1; rc=$?; set -e
kill $SRV 2>/dev/null; wait $SRV 2>/dev/null || true
if [ $rc -eq 0 ]; then echo "ok:   net_http with its own settings: client certificate, AES-256-CBC"
else echo "FAIL: net_http with its own settings (rc=$rc): $(cat $D/http.out)"; exit 1; fi
cat > $D/ka.py << 'PYEOF'
import http.server, os, ssl, sys
port, cert, key, root, log = sys.argv[1:6]
log = os.path.abspath(log)
class H(http.server.SimpleHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    def log_message(self, *a): pass
class S(http.server.ThreadingHTTPServer):
    allow_reuse_address = True
    def handle_error(self, *a): pass   # a refused handshake
    def get_request(self):
        s, a = super().get_request()
        with open(log, "a") as f: f.write("conn\n")
        return s, a
ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
ctx.load_cert_chain(cert, key)
os.chdir(root)
srv = S(("127.0.0.1", int(port)), H)
srv.socket = ctx.wrap_socket(srv.socket, server_side=True, do_handshake_on_connect=False)
srv.serve_forever()
PYEOF
pool() { # label leaf want-rc want-output want-connections -- steps
   L=$1; LEAF=$2; W=$3; O=$4; C=$5; shift 6
   rm -f $D/ka.log
   python3 $D/ka.py 44331 $D/$LEAF.pem $D/$LEAF.key $D/www $D/ka.log & SRV=$!; sleep 0.8
   set +e; $RUN ./tls_http$EXE 44331 body.bin $SUM $D/ca.pem "$@" > $D/http.out 2>&1; rc=$?; set -e
   kill $SRV 2>/dev/null; wait $SRV 2>/dev/null || true
   N=$(wc -l < $D/ka.log 2>/dev/null || echo 0)
   if [ $rc -eq $W ] && grep -q "$O" $D/http.out && [ $N -eq $C ]; then echo "ok:   $L"
   else echo "FAIL: $L (rc=$rc, want $W; $N connections, want $C): $(cat $D/http.out)"; exit 1; fi
}
pool "net_http keeps one connection for two GETs"         rsa   0 "get 2 ok"     1 -- -u -u
pool "a pooled connection is not lent to other settings"   rsa   1 "get 2 failed" 2 -- -u -t $D/other.der -g
pool "nor one with its own settings to a later GET"        untr  1 "get 2 failed" 2 -- -t $D/cca.der -g -u
pool "settings of its own, a connection each time"         rsa   0 "get 2 ok"     2 -- -t $D/ca.der -g -g
# A CBC record changed in flight fails its MAC: the transfer stops there
# with a record error rather than handing over the altered bytes.
cat > $D/flip.py << 'PYEOF'
import socket, sys, threading
lst = socket.socket(); lst.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
lst.bind(("127.0.0.1", 44332)); lst.listen(1)
c, _ = lst.accept(); s = socket.create_connection(("127.0.0.1", 44331))
def up():
    while True:
        b = c.recv(65536)
        if not b: break
        s.sendall(b)
threading.Thread(target=up, daemon=True).start()
seen = 0
while True:
    b = bytearray(s.recv(65536))
    if not b: break
    if seen <= 300000 < seen + len(b): b[300000 - seen] ^= 1
    seen += len(b); c.sendall(bytes(b))
c.close()
PYEOF
for s in AES256-SHA:0035 AES128-SHA256:003c ECDHE-RSA-AES256-SHA384:c028; do
   (cd $D/cfgwww && exec openssl s_server -accept 44331 -cert $D/rsa.pem -key $D/rsa.key -tls1_2 -cipher ${s%%:*} -WWW) >/dev/null 2>&1 &
   SRV=$!; python3 $D/flip.py & PRX=$!; sleep 0.5
   set +e; $RUN ./tls_config$EXE localhost 44332 -t $D/ca.der -s ${s##*:} $G > $D/cfg.out 2>&1; rc=$?; set -e
   kill $SRV $PRX 2>/dev/null || true; wait $SRV $PRX 2>/dev/null || true
   if [ $rc -eq 1 ] && grep -q "receive error -4" $D/cfg.out; then echo "ok:   ${s%%:*}: a record changed in flight is refused"
   else echo "FAIL: ${s%%:*}: changed record (rc=$rc): $(cat $D/cfg.out)"; exit 1; fi
done
# A server that goes silent mid-handshake: the read bound ends it.
make -s tls_stall
$RUN ./tls_stall$EXE
# A refused certificate reaches the verify hook.
./verify_hook_test.sh
echo "[pass] tls_retro local server matrix"
