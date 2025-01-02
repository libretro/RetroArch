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
PKGCONFIG="$(exists pkg-config || :)"

[ "${WAYSCAN}" ] || die 1 "Error: No wayscan in ($PATH)"

WAYLAND_PROTOS=''

if [ "$PROTOS" != 'no' -a "$PKGCONFIG" ]; then
   WAYLAND_PROTOS="$($PKGCONFIG wayland-protocols --variable=pkgdatadir)"
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

generate_source () {
   PROTO_DIR="$1"
   PROTO_NAME="$2"
   PROTO_FILE="$WAYLAND_PROTOS/$PROTO_DIR/$PROTO_NAME.xml"

   "$WAYSCAN" client-header "$PROTO_FILE" "./$PROTO_NAME.h"
   "$WAYSCAN" $CODEGEN "$PROTO_FILE" "./$PROTO_NAME.c"
}

generate_source 'stable/viewporter' 'viewporter'
generate_source 'stable/xdg-shell' 'xdg-shell'
generate_source 'unstable/xdg-decoration' 'xdg-decoration-unstable-v1'
generate_source 'unstable/idle-inhibit' 'idle-inhibit-unstable-v1'
generate_source 'unstable/pointer-constraints' 'pointer-constraints-unstable-v1'
generate_source 'unstable/relative-pointer' 'relative-pointer-unstable-v1'
generate_source 'staging/fractional-scale' 'fractional-scale-v1'
generate_source 'staging/cursor-shape' 'cursor-shape-v1'
# tablet-unstable-v1 is required by cursor-shape-v1
generate_source 'unstable/tablet' 'tablet-unstable-v2'
generate_source 'staging/content-type' 'content-type-v1'
generate_source 'staging/single-pixel-buffer' 'single-pixel-buffer-v1'
