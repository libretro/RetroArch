#!/bin/sh
# Run an os/gekko test ELF in Dolphin and print what it reports.
# usage: run-dolphin.sh <test.elf> [seconds] [sw]
# DOLPHIN points at dolphin-emu-nogui.  "sw" renders with the software
# renderer (headless through surfaceless EGL, e.g. Mesa) and writes XFB
# copies to memory, for tests that read back what the GPU drew; frames
# are copied to $DUMP if it is set.  Exits 0 when the test says PASSED.
ELF=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
SECS=${2:-60}
MODE=$3
DOLPHIN=${DOLPHIN:-dolphin-emu-nogui}
USERDIR=$(mktemp -d)
LOG="$USERDIR/out.log"
# A controller on port 1 that counts as plugged in with no input device.
mkdir -p "$USERDIR/Config"
printf '[GCPad1]\nOptions/Always Connected = True\n' > "$USERDIR/Config/GCPadNew.ini"
set -- -u "$USERDIR" -p headless \
   -C Dolphin.Interface.DebugModeEnabled=True \
   -C Logger.Logs.OSREPORT_HLE=True \
   -C Logger.Options.WriteToConsole=True \
   -C Logger.Options.Verbosity=5
if [ "$MODE" = sw ]; then
   export EGL_PLATFORM=surfaceless
   set -- "$@" -v "Software Renderer" \
      -C GFX.Hacks.XFBToTextureEnable=False \
      -C GFX.Hacks.EFBToTextureEnable=False
   if [ -n "$DUMP" ]; then
      set -- "$@" -C Dolphin.Movie.DumpFrames=True \
         -C GFX.Settings.DumpFramesAsImages=True
   fi
else
   set -- "$@" -v Null
fi
timeout "$SECS" "$DOLPHIN" "$@" -e "$ELF" > "$LOG" 2>&1 &
PID=$!
# Stop as soon as the test has reported its verdict.
while kill -0 $PID 2>/dev/null; do
   if grep -aqE "OSREPORT_HLE.*(PASSED|FAILED) \(" "$LOG"; then
      sleep 1
      kill -INT $PID 2>/dev/null
      break
   fi
   sleep 1
done
wait $PID 2>/dev/null
sed -n 's/.*OSREPORT_HLE\]: [0-9a-f]*->[0-9a-f]*| //p' "$LOG"
grep -aq "OSREPORT_HLE.*PASSED (" "$LOG"
RC=$?
if [ -n "$DUMP" ] && [ -d "$USERDIR/Dump/Frames" ]; then
   mkdir -p "$DUMP" && cp "$USERDIR"/Dump/Frames/*.png "$DUMP"/ 2>/dev/null
fi
rm -rf "$USERDIR"
exit $RC
