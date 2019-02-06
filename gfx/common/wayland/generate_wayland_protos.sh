#!/bin/sh

set -eu

cd -- "$(cd -- "${0%/*}/" && pwd -P)"

. ../../../qb/qb.init.sh

SCANNER_VERSION=''
SHARE_DIR=''

usage="generate_wayland_protos.sh - Generates wayland protocols.

  Usage: generate_wayland_protos.sh [OPTIONS]
    -c, --codegen version   Sets the wayland scanner compatibility version.
    -h, --help              Shows this message.
    -s, --share path        Sets the path of the wayland protocols directory."

while [ $# -gt 0 ]; do
   option="$1"
   shift
   case "$option" in
      -- ) break ;;
      -c|--codegen ) SCANNER_VERSION="$1"; shift ;;
      -h|--help ) die 0 "$usage" ;;
      -s|--share ) SHARE_DIR="$1/wayland-protocols"; shift ;;
      * ) die 1 "Unrecognized option '$option', use -h for help." ;;
   esac
done

WAYSCAN="$(exists wayland-scanner || :)"

[ "${WAYSCAN}" ] || die 1 "Error: No wayscan in ($PATH)"

WAYLAND_PROTOS=''

for protos in "$SHARE_DIR" /usr/local/share/wayland-protocols /usr/share/wayland-protocols; do
   [ -d "$protos" ] || continue
   WAYLAND_PROTOS="$protos"
   break
done

[ "${WAYLAND_PROTOS}" ] || die 1 'Error: No wayland-protocols directory found.'

if [ "$SCANNER_VERSION" = '1.12' ]; then
   CODEGEN=code
else
   CODEGEN=private-code
fi

#Generate xdg-shell_v6 header and .c files
"$WAYSCAN" client-header "$WAYLAND_PROTOS/unstable/xdg-shell/xdg-shell-unstable-v6.xml" ./xdg-shell-unstable-v6.h
"$WAYSCAN" $CODEGEN "$WAYLAND_PROTOS/unstable/xdg-shell/xdg-shell-unstable-v6.xml" ./xdg-shell-unstable-v6.c

#Generate xdg-shell header and .c files
"$WAYSCAN" client-header "$WAYLAND_PROTOS/stable/xdg-shell/xdg-shell.xml" ./xdg-shell.h
"$WAYSCAN" $CODEGEN "$WAYLAND_PROTOS/stable/xdg-shell/xdg-shell.xml" ./xdg-shell.c

#Generate idle-inhibit header and .c files
"$WAYSCAN" client-header "$WAYLAND_PROTOS/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml" ./idle-inhibit-unstable-v1.h
"$WAYSCAN" $CODEGEN "$WAYLAND_PROTOS/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml" ./idle-inhibit-unstable-v1.c

#Generate xdg-decoration header and .c files
"$WAYSCAN" client-header "$WAYLAND_PROTOS/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml" ./xdg-decoration-unstable-v1.h
"$WAYSCAN" $CODEGEN "$WAYLAND_PROTOS/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml" ./xdg-decoration-unstable-v1.c
