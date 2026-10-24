# Sourced by a step that runs Windows programs under Wine: one Wine
# server for the whole step, ended when the step ends. Left to itself
# each program starts a server that leaves after it, and a program
# started as the last one is leaving is cut off before main()
# ("wine client error:0: recvmsg: Connection reset by peer").
# A server already running for the prefix is used as it is, and left
# running. Sets the step's EXIT trap; a step with one of its own does
# this itself.
if command -v wineserver >/dev/null 2>&1; then
   mkdir -p "${WINEPREFIX:-$HOME/.wine}"
   if wineserver -p 2>/dev/null; then
      trap 'wineserver -k || true' EXIT
   fi
fi
