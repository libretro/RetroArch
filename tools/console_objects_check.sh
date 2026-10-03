#!/bin/sh
# Checks what the console Makefiles build, without their toolchains.
#
# Each Makefile's final object list is read from make's own database
# (make -pn), so the answer is what that platform really compiles, not
# what the defines in one file suggest. It fails when a console builds
# an object it must not:
#
#   - network/mcp_server.o anywhere: the MCP server is a desktop
#     option (HAVE_MCP, set by ./configure only). Listed once with the
#     networking objects, it was compiled for the 3DS, Vita, Switch,
#     Wii U, OpenDingux, RG350 and Emscripten, and broke the 3DS build.
#   - the crypto library on GameCube, Wii and PS2: 24 and 32 MiB of RAM
#     leave no room for it, so HAVE_CRYPTO is never set there.
#
# The SDK variables point at an empty directory: they must be set for
# the Makefiles to parse, and nothing is compiled.
#
# Usage: tools/console_objects_check.sh
set -eu

cd "$(dirname "$0")/.."

fake=$(mktemp -d)
trap 'rm -rf "$fake"' EXIT

fail=0
checked=0

objs()
{
   DEVKITPRO="$fake" DEVKITARM="$fake/devkitARM" DEVKITPPC="$fake/devkitPPC" \
   VITASDK="$fake" PS2SDK="$fake" PSPSDK="$fake" PS2DEV="$fake" \
      make -s -f "$1" -pn 2>/dev/null | sed -n 's/^OBJ :*= //p' | head -1
}

# refuse <makefile> <what> <pattern>
refuse()
{
   list=$(objs "$1")
   if [ -z "$list" ]; then
      echo "skip  $1: no object list"
      return
   fi
   checked=$((checked + 1))
   if echo "$list" | tr ' ' '\n' | grep -E -q "$3"; then
      echo "FAIL  $1 builds $2: $(echo "$list" | tr ' ' '\n' | grep -E "$3" | head -3 | tr '\n' ' ')"
      fail=1
   else
      echo "ok    $1 builds no $2"
   fi
}

for mk in Makefile.ctr Makefile.vita Makefile.libnx Makefile.wiiu \
          Makefile.psp1 Makefile.ps2 Makefile.ngc Makefile.wii \
          Makefile.dingux Makefile.rg350 Makefile.rg350_odbeta \
          Makefile.retrofw Makefile.miyoo Makefile.rs90 Makefile.lfx000 \
          Makefile.webos Makefile.emscripten Makefile.orbis \
          Makefile.psl1ght Makefile.dos; do
   [ -f "$mk" ] || continue
   refuse "$mk" "MCP server" '(^|/)network/mcp_server\.o$'
done

for mk in Makefile.ngc Makefile.wii Makefile.ps2; do
   [ -f "$mk" ] || continue
   refuse "$mk" "crypto library" '(^|/)libretro-common/crypto/[a-z0-9_]+\.o$'
done

if [ "$checked" -eq 0 ]; then
   echo "FAIL  no Makefile gave an object list"
   exit 1
fi
exit $fail
