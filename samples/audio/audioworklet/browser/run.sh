#!/bin/sh
# Builds aw_browser.c for each of the audioworklet driver's build
# shapes and runs it in headless Chrome.  Needs emcc on PATH (emsdk)
# and a Chrome; CHROME overrides which, PORT the local server's port.
# Run from anywhere; builds go to a temporary directory.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../../.." && pwd)
out=$(mktemp -d)
port=${PORT:-8765}
chrome=${CHROME:-google-chrome}
status=0

# The park's watchdog is longer than the ring here, so a writer the
# worklet's pulls fail to wake starves the context and the run fails on
# its throughput, instead of being carried by a watchdog about as short
# as a quantum.
common="-O2 -std=gnu99 -pthread -sAUDIO_WORKLET -sWASM_WORKERS \
 -DHAVE_THREADS -DHAVE_AUDIOWORKLET -DRARCH_INTERNAL \
 -DAUDIOWORKLET_PARK_WAIT_NS=500000000 \
 -I$root -I$root/libretro-common/include -I$root/deps \
 $here/aw_browser.c $root/libretro-common/queues/retro_spsc.c \
 --pre-js $here/pre.js -sEXIT_RUNTIME=0 -sINITIAL_MEMORY=64MB -lm"

build() {
   name=$1; shift
   mkdir -p "$out/$name"
   emcc $common "$@" -o "$out/$name/index.html" || { echo "BUILD FAIL $name"; status=1; }
}

build proxy    -sPROXY_TO_PTHREAD -DPROXY_TO_PTHREAD
build async    -sASYNCIFY -DEMSCRIPTEN_ASYNCIFY -DEMSCRIPTEN_FULL_ASYNCIFY
build jspi     -sJSPI -DEMSCRIPTEN_ASYNCIFY -DEMSCRIPTEN_FULL_ASYNCIFY -DEMSCRIPTEN_JSPI
# Makefile.emscripten's own asyncify flags for MIN_ASYNC, with the
# harness's main in the list where the frontend has emscripten_mainloop.
build minasync -sASYNCIFY -sASYNCIFY_STACK_SIZE=8192 -sASYNCIFY_IGNORE_INDIRECT=1 \
               "-sASYNCIFY_ADD=dynCall_*,main" \
               -DEMSCRIPTEN_ASYNCIFY -DEMSCRIPTEN_MIN_ASYNCIFY \
               -DEMSCRIPTEN_AUDIO_EXTERNAL_BLOCK -DEMSCRIPTEN_AUDIO_ASYNC_BLOCK

python3 "$here/serve.py" "$out" "$port" &
server=$!
sleep 1

for name in proxy async jspi minasync; do
   [ -f "$out/$name/index.html" ] || continue
   rm -f "$out/log.txt"
   "$chrome" --headless=new --no-sandbox --disable-gpu --no-proxy-server \
      --autoplay-policy=no-user-gesture-required \
      --user-data-dir="$out/profile-$name" \
      "http://127.0.0.1:$port/$name/index.html" >/dev/null 2>&1 &
   browser=$!
   i=0
   while [ $i -lt 60 ] && ! grep -q '^RESULT' "$out/log.txt" 2>/dev/null; do
      sleep 1; i=$((i + 1))
   done
   kill $browser 2>/dev/null; wait $browser 2>/dev/null
   echo "== $name"
   grep -v 'Buffer size\|Device rate\|Reusing\|frames dropped' "$out/log.txt" 2>/dev/null
   grep -q '^RESULT PASS' "$out/log.txt" 2>/dev/null || status=1
done

kill $server 2>/dev/null
rm -rf "$out"
exit $status
