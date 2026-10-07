#!/bin/sh
# NFSv4.2 READ_PLUS against fake_nfs42.py, a minimal server that reports
# the holes the filesystem really holds (SEEK_DATA / SEEK_HOLE): a
# 32 MiB file with 1 MiB of data must read back octet for octet with
# its holes not crossing the network. No server here implements
# READ_PLUS right (nfs-ganesha's answers every range with an empty
# data run; local_ganesha_test.sh checks the client falls back to READ
# on it), hence the fake. Its encoding is checked apart from the client
# when tshark is installed: Wireshark's own NFS dissector decodes the
# replies and their data and holes must add up to the file.
# Runs as a Windows binary under Wine too (CC=..-mingw.. RUNNER=wine
# EXE=.exe); needs python3.
set -eu
cd "$(dirname "$0")"
command -v python3 >/dev/null 2>&1 || { echo "skip: no python3"; exit 0; }
make -s nfs_sparse_test
RUN=${RUNNER:-}
EXE=${EXE:-}
PORT=20460
D=$(mktemp -d); chmod 755 $D
cleanup() {
   [ -n "${SRV:-}" ] && kill $SRV 2>/dev/null || true
   [ -n "${CAP:-}" ] && kill $CAP 2>/dev/null || true
   [ -n "$KEEP_PCAP" ] && cp $D/rp.pcap $KEEP_PCAP 2>/dev/null; rm -rf $D
}
trap cleanup EXIT
KEEP_PCAP=${KEEP_PCAP:-}
truncate -s 32M $D/sparse.img
head -c 262144 /dev/urandom | dd of=$D/sparse.img bs=65536 seek=0   conv=notrunc status=none
head -c 524288 /dev/urandom | dd of=$D/sparse.img bs=65536 seek=160 conv=notrunc status=none
head -c 262144 /dev/urandom | dd of=$D/sparse.img bs=65536 seek=500 conv=notrunc status=none
python3 fake_nfs42.py $PORT $D > $D/fake.log 2>&1 &
SRV=$!
CAP=
if command -v tshark >/dev/null 2>&1 && [ "$(id -u)" = 0 ]; then
   tshark -i lo -f "tcp port $PORT" -w $D/rp.pcap -q 2>/dev/null &
   CAP=$!
fi
sleep 2
local_path=$D/sparse.img
[ -n "$RUN" ] && local_path="Z:$D/sparse.img"
if $RUN ./nfs_sparse_test$EXE 127.0.0.1 /export $PORT sparse.img "$local_path" 1048576 plus > $D/res 2>&1; then
   echo "ok:   READ_PLUS: a 32 MiB file with 1 MiB of data, holes kept off the wire ($(grep replies $D/res | tr -d "\r"))"
else
   echo "FAIL: READ_PLUS: $(cat $D/res)"; exit 1
fi
if [ -n "$CAP" ]; then
   sleep 1; kill $CAP; wait $CAP 2>/dev/null || true; CAP=
   sums=$(tshark -r $D/rp.pcap -d tcp.port==$PORT,rpc -Y "nfs.opcode == 68 && rpc.msgtyp == 1" -V 2>/dev/null | awk '
      /Content Type: Data/ {k="d"; next}
      /Content Type: Hole/ {k="h"; next}
      k=="d" && /Read length:/ {d+=$3; k=""}
      k=="h" && /^[ \t]*length:/ {h+=$2; k=""}
      END {printf "%d %d", d, h}')
   bad=$(tshark -r $D/rp.pcap -d tcp.port==$PORT,rpc -Y "_ws.malformed" 2>/dev/null | wc -l)
   if [ "$sums" = "1048576 32505856" ] && [ "$bad" -eq 0 ]; then
      echo "ok:   Wireshark decodes the replies: 1048576 octets of data and 32505856 of holes"
   else
      echo "FAIL: Wireshark's decode: data and holes $sums, $bad packets it could not decode"; exit 1
   fi
else
   echo "skip: the independent decode needs tshark and root"
fi
echo "[pass] nfs_retro local read_plus"
