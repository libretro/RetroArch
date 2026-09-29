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
run "chain: root -> RSA intermediate -> leaf"      viarsa  ECDHE-RSA-AES128-GCM-SHA256 prime256v1 0 localhost 0 $D/ca.pem
run "chain: root -> P-384 intermediate -> leaf"    viap384 ECDHE-RSA-AES128-GCM-SHA256 prime256v1 0 localhost 0 $D/ca.pem
run "chain: trusting the intermediate directly also works" viarsa ECDHE-RSA-AES128-GCM-SHA256 prime256v1 0 localhost 0 $D/irsa.pem
cp $D/viarsa.pem $D/nointer.pem; cp $D/viarsa.key $D/nointer.key
run "chain: server omitting the intermediate is refused" nointer ECDHE-RSA-AES128-GCM-SHA256 prime256v1 1 localhost 0 $D/ca.pem
# TLS 1.3: the server picks it over 1.2 when offered; RSA certs sign the
# CertificateVerify with RSA-PSS, ECDSA with P-256; both 1.3 suites; the
# chain still has to verify; a 1.3-only suite we do not offer must not
# connect at all rather than silently downgrade.
run13() { # label cert ciphersuite expect_rc host mode ca
   CHAIN=""; [ -f $D/$2.chain.pem ] && CHAIN="-cert_chain $D/$2.chain.pem"
   openssl s_server -accept 44331 -cert $D/$2.pem $CHAIN -key $D/$2.key -tls1_3 -ciphersuites "$3" -www >/dev/null 2>&1 &
   SRV=$!; sleep 0.4
   set +e; $RUN ./tls_fetch$EXE $5 44331 $6 "$7" 1 >/dev/null 2>&1; rc=$?; set -e
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
run13 "TLS 1.3, only AES-256-GCM-SHA384 offered: no connection" rsa TLS_AES_256_GCM_SHA384 1 localhost 0 $D/ca.pem
echo "[pass] tls_retro local server matrix"
