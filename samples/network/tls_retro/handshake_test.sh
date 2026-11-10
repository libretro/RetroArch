#!/bin/sh
set -eu
cd "$(dirname "$0")"
backend=${SSL_BACKEND:-retro}
port=${TLS_HANDSHAKE_PORT:-44531}
case ${TLS_PROTOCOL:-1.2} in
   1.2) protocol=-tls1_2 ;;
   1.3) protocol=-tls1_3 ;;
   *) exit 2 ;;
esac
D=$(mktemp -d)
SRV=
cleanup() {
   if [ -n "$SRV" ]; then
      kill "$SRV" 2>/dev/null || true
      wait "$SRV" 2>/dev/null || true
   fi
   rm -rf "$D"
}
trap cleanup EXIT
make -s -B tls_handshake SSL_BACKEND="$backend"
openssl req -x509 -newkey rsa:2048 -nodes -keyout "$D/ca.key" -out "$D/ca.pem" \
   -days 2 -subj '/CN=TLS step test CA' 2>/dev/null
openssl req -newkey rsa:2048 -nodes -keyout "$D/server.key" -out "$D/server.csr" \
   -subj '/CN=localhost' 2>/dev/null
printf 'subjectAltName=DNS:localhost\nbasicConstraints=CA:FALSE\n' > "$D/server.ext"
openssl x509 -req -in "$D/server.csr" -CA "$D/ca.pem" -CAkey "$D/ca.key" \
   -CAcreateserial -out "$D/server.pem" -days 2 -extfile "$D/server.ext" 2>/dev/null
openssl s_server -accept "$port" -cert "$D/server.pem" -key "$D/server.key" \
   "$protocol" -www > "$D/server.log" 2>&1 &
SRV=$!
if ! python3 - "$port" <<'PY'
import socket
import sys
import time

deadline = time.monotonic() + 5
while True:
    try:
        with socket.create_connection(('127.0.0.1', int(sys.argv[1])), 0.1):
            break
    except OSError:
        if time.monotonic() >= deadline:
            sys.exit('TLS test server did not start')
        time.sleep(0.02)
PY
then
   cat "$D/server.log"
   exit 1
fi
${RUNNER:-} ./tls_handshake${EXE:-} "$port" "$D/ca.pem"
${RUNNER:-} ./tls_handshake${EXE:-} "$port" "$D/ca.pem" reject
