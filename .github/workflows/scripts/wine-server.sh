# Sourced by a step that runs Windows programs under Wine, and needs no
# display: one Wine server for the whole step, ended when the step ends.
# Left to itself each program starts a server that leaves after it, and
# a program started as the last one is leaving is cut off before main()
# ("wine client error:0: recvmsg: Connection reset by peer").
# Wine's services start with a server's first program and hold that
# program's output for as long as they live, which is now the step: a
# step piping a test into tee would wait for them. So they are started
# here, with no output to hold. A server already running for the prefix
# is used as it is, and left running. Sets the step's EXIT trap.
if command -v wineserver >/dev/null 2>&1; then
   mkdir -p "${WINEPREFIX:-$HOME/.wine}"
   if wineserver -p 2>/dev/null; then
      trap 'wineserver -k || true' EXIT
      WINEDEBUG=-all "$(command -v wine64 || command -v wine)" cmd /c exit \
         </dev/null >/dev/null 2>&1 || true
   fi
fi
