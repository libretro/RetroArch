#!/bin/sh
# Run an os/gekko test ELF in Dolphin and print what it reports.
# usage: run-dolphin.sh <test.elf> [seconds]
# DOLPHIN points at dolphin-emu-nogui.  Exits 0 when the test says
# PASSED.
ELF=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
SECS=${2:-60}
DOLPHIN=${DOLPHIN:-dolphin-emu-nogui}
USERDIR=$(mktemp -d)
LOG="$USERDIR/out.log"
timeout "$SECS" "$DOLPHIN" -u "$USERDIR" -p headless -v Null \
   -C Dolphin.Interface.DebugModeEnabled=True \
   -C Logger.Logs.OSREPORT_HLE=True \
   -C Logger.Options.WriteToConsole=True \
   -C Logger.Options.Verbosity=5 \
   -e "$ELF" > "$LOG" 2>&1 &
PID=$!
# Stop as soon as the test has reported its verdict.
while kill -0 $PID 2>/dev/null; do
   if grep -aqE "OSREPORT_HLE.*(PASSED|FAILED) \(" "$LOG"; then
      sleep 1
      kill $PID 2>/dev/null
      break
   fi
   sleep 1
done
wait $PID 2>/dev/null
sed -n 's/.*OSREPORT_HLE\]: [0-9a-f]*->[0-9a-f]*| //p' "$LOG"
grep -aq "OSREPORT_HLE.*PASSED (" "$LOG"
RC=$?
rm -rf "$USERDIR"
exit $RC
