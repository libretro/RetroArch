#!/bin/sh

set -eu

cd -- "$(cd -- "${0%/*}/" && pwd -P)"

. ../../../qb/qb.init.sh

PROTOS=''
SCANNER_VERSION=''
SHARE_DIR=''

usage="generate_wayland_protos.sh - Generates wayland protocols.

  Usage: generate_wayland_protos.sh [OPTIONS]
    -c, --codegen version   Sets the wayland scanner compatibility version.
    -h, --help              Shows this message.
    -p, --protos yes|no     Set to 'no' to use the bundled wayland-protocols.
    -s, --share path        Sets the path of the wayland protocols directory."

while [ $# -gt 0 ]; do
   option="$1"
   shift
   case "$option" in
      -- ) break ;;
      -c|--codegen ) SCANNER_VERSION="$1"; shift ;;
      -h|--help ) die 0 "$usage" ;;
      -p|--protos ) PROTOS="$1"; shift ;;
      -s|--share ) SHARE_DIR="$1/wayland-protocols"; shift ;;
      * ) die 1 "Unrecognized option '$option', use -h for help." ;;
   esac
done

WAYSCAN="$(exists wayland-scanner || :)"

[ "${WAYSCAN}" ] || die 1 "Error: No wayscan in ($PATH)"

WAYLAND_PROTOS=''

if [ "$PROTOS" != 'no' ]; then
   for protos in "$SHARE_DIR" /usr/local/share/wayland-protocols /usr/share/wayland-protocols; do
      [ -d "$protos" ] || continue
      WAYLAND_PROTOS="$protos"
      break
   done
fi

if [ -z "${WAYLAND_PROTOS}" ]; then
   WAYLAND_PROTOS='../../../deps/wayland-protocols'
   die : 'Notice: Using the bundled wayland-protocols.'
fi

if [ "$SCANNER_VERSION" = '1.12' ]; then
   CODEGEN=code
else
   CODEGEN=private-code
fi

XDG_SHELL_UNSTABLE='unstable/xdg-shell/xdg-shell-unstable-v6.xml'
XDG_SHELL='stable/xdg-shell/xdg-shell.xml'
XDG_DECORATION_UNSTABLE='unstable/xdg-decoration/xdg-decoration-unstable-v1.xml'
IDLE_INHIBIT_UNSTABLE='unstable/idle-inhibit/idle-inhibit-unstable-v1.xml'

#Generate xdg-shell_v6 header and .c files
"$WAYSCAN" client-header "$WAYLAND_PROTOS/$XDG_SHELL_UNSTABLE" ./xdg-shell-unstable-v6.h
"$WAYSCAN" $CODEGEN "$WAYLAND_PROTOS/$XDG_SHELL_UNSTABLE" ./xdg-shell-unstable-v6.c

#Generate xdg-shell header and .c files
"$WAYSCAN" client-header "$WAYLAND_PROTOS/$XDG_SHELL" ./xdg-shell.h
"$WAYSCAN" $CODEGEN "$WAYLAND_PROTOS/$XDG_SHELL" ./xdg-shell.c

#Generate idle-inhibit header and .c files
"$WAYSCAN" client-header "$WAYLAND_PROTOS/$IDLE_INHIBIT_UNSTABLE" ./idle-inhibit-unstable-v1.h
"$WAYSCAN" $CODEGEN "$WAYLAND_PROTOS/$IDLE_INHIBIT_UNSTABLE" ./idle-inhibit-unstable-v1.c

#Generate xdg-decoration header and .c files
"$WAYSCAN" client-header "$WAYLAND_PROTOS/$XDG_DECORATION_UNSTABLE" ./xdg-decoration-unstable-v1.h
"$WAYSCAN" $CODEGEN "$WAYLAND_PROTOS/$XDG_DECORATION_UNSTABLE" ./xdg-decoration-unstable-v1.c
