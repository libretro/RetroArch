#!/bin/sh
# Run an os/gekko test ELF in Dolphin and print what it reports.
# usage: run-dolphin.sh <test.elf> [seconds] [sw]
# DOLPHIN points at dolphin-emu-nogui.  "sw" renders with the software
# renderer (headless through surfaceless EGL, e.g. Mesa) and writes XFB
# copies to memory, for tests that read back what the GPU drew; frames
# are copied to $DUMP if it is set.  $SDIMG is the Wii's SD card image.
# The clock chip reads 1790000000 (2026-09-21 14:13:20) at the start,
# a Wii has the emulated Wii Speak and Skylander portal on USB, and its emulated remote 1 a
# Nunchuk ($WIIEXT names another extension, e.g. Classic).  $ARGS,
# arguments separated by '|', is passed the way loaders pass them;
# $ATTACH names a file loaded with it, whose address and length follow
# them.
# $PADS names a file of pipe commands (e.g. "PRESS A", "RELEASE A")
# fed to the GameCube pad on port 1, one line per $PADSTEP seconds;
# $WMPADS one for remote 1, whose pointer follows the MAIN axes
# ("SET MAIN 1 0.5" points right, 0.5 0.5 at the centre); X and Y
# are a guitar's green fret and strum down, L its whammy bar and C its
# stick.
# $GECKO puts a USB Gecko in slot B and also wants the verdict from
# it, read the way a computer on its other end would (Dolphin serves it
# on TCP port 55020, or the next one free).
# $KEEP keeps Dolphin's user directory and log, and prints its path.
# Exits 0 when the test says PASSED.
ELF=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
SECS=${2:-60}
MODE=$3
DOLPHIN=${DOLPHIN:-dolphin-emu-nogui}
USERDIR=$(mktemp -d)
LOG="$USERDIR/out.log"
if [ -n "$ARGS" ]; then
   if [ -n "$ATTACH" ]; then
      set -- --attach "$ATTACH"
   else
      set --
   fi
   IFS='|'
   # shellcheck disable=SC2086
   python3 "$(dirname "$0")/elf-args.py" "$ELF" "$USERDIR/args.elf" "$@" \
      $ARGS || exit 1
   unset IFS
   ELF="$USERDIR/args.elf"
fi
# A controller on port 1 that counts as plugged in with no input device.
mkdir -p "$USERDIR/Config"
printf '[GCPad1]\nOptions/Always Connected = True\n' > "$USERDIR/Config/GCPadNew.ini"
if [ -n "$PADS" ]; then
   mkdir -p "$USERDIR/Pipes"
   mkfifo "$USERDIR/Pipes/pad"
   {
      printf 'Device = Pipe/0/pad\n'
      for b in A B X Y Z START; do
         printf 'Buttons/%s = `Button %s`\n' "$b" "$b"
      done
      for d in Up Down Left Right; do
         printf 'D-Pad/%s = `Button D_%s`\n' "$d" "$(echo "$d" | tr a-z A-Z)"
      done
   } >> "$USERDIR/Config/GCPadNew.ini"
fi
printf '[Wiimote1]\nSource = 1\nExtension = %s\n' "${WIIEXT:-Nunchuk}" \
   > "$USERDIR/Config/WiimoteNew.ini"
if [ -n "$WMPADS" ]; then
   mkdir -p "$USERDIR/Pipes"
   mkfifo "$USERDIR/Pipes/wm"
   {
      printf 'Device = Pipe/0/wm\n'
      printf 'Buttons/A = `Button A`\nButtons/B = `Button B`\n'
      printf 'IR/Up = `Axis MAIN Y -`\nIR/Down = `Axis MAIN Y +`\n'
      printf 'IR/Left = `Axis MAIN X -`\nIR/Right = `Axis MAIN X +`\n'
      printf 'Guitar/Frets/Green = `Button X`\nGuitar/Strum/Down = `Button Y`\n'
      printf 'Guitar/Whammy/Bar = `Axis L +`\n'
      printf 'Guitar/Stick/Up = `Axis C Y -`\nGuitar/Stick/Down = `Axis C Y +`\n'
   } >> "$USERDIR/Config/WiimoteNew.ini"
fi
set -- -u "$USERDIR" -p headless \
   -C Dolphin.Interface.DebugModeEnabled=True \
   -C Logger.Logs.OSREPORT_HLE=True \
   -C Logger.Options.WriteToConsole=True \
   -C Logger.Options.Verbosity=5 \
   -C Dolphin.Core.EnableCustomRTC=True \
   -C Dolphin.Core.CustomRTCValue=1790000000 \
   -C Dolphin.EmulatedUSBDevices.EmulateWiiSpeak=True \
   -C Dolphin.EmulatedUSBDevices.EmulateSkylanderPortal=True
if [ -n "$GECKO" ]; then
   set -- "$@" -C Dolphin.Core.SlotB=7
fi
if [ -n "$SDIMG" ]; then
   set -- "$@" -C "Dolphin.General.WiiSDCardPath=$(cd "$(dirname "$SDIMG")" && pwd)/$(basename "$SDIMG")" \
      -C Dolphin.Core.WiiSDCard=True -C Dolphin.Core.WiiSDCardAllowWrites=True
fi
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
# Commands from $1 into the pipe $2, one per $PADSTEP seconds.
feed() {
   (
      exec 3> "$USERDIR/Pipes/$2"
      while IFS= read -r cmd; do
         sleep "${PADSTEP:-1}"
         printf '%s\n' "$cmd" >&3
      done < "$1"
      sleep "$SECS"
   ) &
   FEED="$FEED $!"
}
if [ -n "$GECKO" ]; then
   python3 - "$USERDIR/gecko.log" <<'PY' &
import socket, sys, time
out = open(sys.argv[1], 'wb')
s = None
for _ in range(600):
    for port in range(55020, 55040):
        try:
            s = socket.create_connection(('127.0.0.1', port))
            break
        except OSError:
            pass
    if s:
        break
    time.sleep(0.1)
else:
    sys.exit()
while True:
    d = s.recv(4096)
    if not d:
        break
    out.write(d)
    out.flush()
PY
   FEED="$FEED $!"
fi
[ -n "$PADS" ] && feed "$PADS" pad
[ -n "$WMPADS" ] && feed "$WMPADS" wm
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
# shellcheck disable=SC2086
[ -n "$FEED" ] && kill $FEED 2>/dev/null
sed -n 's/.*OSREPORT_HLE\]: [0-9a-f]*->[0-9a-f]*| //p' "$LOG"
grep -aq "OSREPORT_HLE.*PASSED (" "$LOG"
RC=$?
if [ -n "$GECKO" ] && ! grep -aq "PASSED (" "$USERDIR/gecko.log"; then
   echo "FAIL USB Gecko: no verdict from it"
   RC=1
fi
if [ -n "$DUMP" ] && [ -d "$USERDIR/Dump/Frames" ]; then
   mkdir -p "$DUMP" && cp "$USERDIR"/Dump/Frames/*.png "$DUMP"/ 2>/dev/null
fi
[ -n "$KEEP" ] && echo "$USERDIR" || rm -rf "$USERDIR"
exit $RC
