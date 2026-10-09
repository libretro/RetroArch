#!/bin/sh
# A refused certificate reaches the verify hook: tls_verify_hook against
# an openssl s_server with a self-signed certificate. SSL_BACKEND picks
# the client as in the Makefile. Needs openssl.
set -e
command -v openssl >/dev/null 2>&1 || { echo "skip: no openssl"; exit 0; }
D=$(mktemp -d); SRV=; trap 'rm -rf $D; [ -n "$SRV" ] && kill $SRV 2>/dev/null || true' EXIT
cd "$(dirname "$0")"
RUN=${RUNNER:-}
EXE=${EXE:-}
make -s tls_verify_hook
openssl req -x509 -newkey rsa:2048 -nodes -keyout $D/k.pem -out $D/c.pem -days 2 \
   -subj "/CN=localhost" -addext "subjectAltName=DNS:localhost" 2>/dev/null
openssl s_server -accept 44332 -cert $D/c.pem -key $D/k.pem -tls1_2 -www >/dev/null 2>&1 &
SRV=$!; sleep 0.4
$RUN ./tls_verify_hook$EXE 44332
