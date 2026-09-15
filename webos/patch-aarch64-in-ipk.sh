#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <package.ipk>"
  exit 1
fi

IPK="$1"
IPK="$(realpath "$IPK")"
IPK_DIR="$(dirname "$IPK")"
IPK_BASE="$(basename "$IPK")"
OUTNAME="${IPK_BASE%.ipk}-patched.ipk"

WORKDIR=$(mktemp -d)

echo "[*] Working in: $WORKDIR"

cp "$IPK" "$WORKDIR/"
cd "$WORKDIR"

echo "[*] Extracting IPK..."
ar x "$IPK_BASE"

echo "[*] Extracting control..."
mkdir control
tar -xf control.tar.gz -C control

echo "[*] Patching Architecture field..."
sed -i 's/^Architecture: *aarch64/Architecture: arm/' control/control

echo "[*] Rebuilding control..."
tar -czf control.tar.gz -C control control

echo "[*] Repacking..."
ar rcs "$OUTNAME" debian-binary control.tar.gz data.tar.gz

echo "[*] Copying result back..."
mv "$OUTNAME" "$IPK_DIR/"

echo "[+] Done: $IPK_DIR/$OUTNAME"
