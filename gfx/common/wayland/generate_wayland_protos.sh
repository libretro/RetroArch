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

if [ -n "${CROSS_COMPILE:-}" ] && echo "${CROSS_COMPILE:-}" | grep -q "webos"; then
   if [ -z "${STAGING_DIR:-}" ]; then
      die 1 "Error: WEBOS=1 but STAGING_DIR not set"
   fi
   WAYSCAN="$STAGING_DIR/../../bin/wayland-scanner"
fi

PKGCONFIG="$(exists pkg-config || :)"

[ "${WAYSCAN}" ] || die 1 "Error: No wayscan in ($PATH)"

WAYLAND_PROTOS=''

if [ "$PROTOS" != 'no' -a "$PKGCONFIG" ]; then
   WAYLAND_PROTOS="$($PKGCONFIG wayland-protocols --variable=pkgdatadir 2>/dev/null || true)"
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

# A scanner older than the protocol files validates them against a DTD
# without the newer attributes and warns on every file using them. It
# ignores them all the same, so they are stripped before it sees them:
# the event type attribute (wayland 1.20) and deprecated-since (1.23).
SCANNER_MINOR="$("$WAYSCAN" --version 2>&1 |
   sed -n 's/.*wayland-scanner 1\.\([0-9][0-9]*\).*/\1/p' | head -n 1)"
STRIP_SED=''
if [ -n "$SCANNER_MINOR" ]; then
   if [ "$SCANNER_MINOR" -lt 20 ]; then
      STRIP_SED="$STRIP_SED -e '/<event /s/ type=\"[^\"]*\"//'"
   fi
   if [ "$SCANNER_MINOR" -lt 23 ]; then
      STRIP_SED="$STRIP_SED -e 's/ deprecated-since=\"[^\"]*\"//'"
   fi
fi
STRIP_FILE=''
if [ -n "$STRIP_SED" ]; then
   STRIP_FILE="$(mktemp "${TMPDIR:-/tmp}/wlproto.XXXXXX")"
   trap 'rm -f "$STRIP_FILE"' EXIT
fi

generate_source () {
   PROTO_DIR="$1"
   PROTO_NAME="$2"
   PROTO_FILE="$WAYLAND_PROTOS/$PROTO_DIR/$PROTO_NAME.xml"

   # A protocol newer than the installed wayland-protocols comes from
   # the bundled copy rather than failing the build.
   if [ ! -f "$PROTO_FILE" ]; then
      PROTO_FILE="../../../deps/wayland-protocols/$PROTO_DIR/$PROTO_NAME.xml"
   fi

   if [ -n "$STRIP_FILE" ]; then
      eval "sed $STRIP_SED" < "$PROTO_FILE" > "$STRIP_FILE"
      PROTO_FILE="$STRIP_FILE"
   fi

   "$WAYSCAN" client-header "$PROTO_FILE" "./$PROTO_NAME.h"
   "$WAYSCAN" $CODEGEN "$PROTO_FILE" "./$PROTO_NAME.c"
}

generate_source 'stable/presentation-time' 'presentation-time'
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
generate_source 'staging/tearing-control' 'tearing-control-v1'
generate_source 'staging/color-management' 'color-management-v1'
# KWin's output protocols, from plasma-wayland-protocols (MIT-CMU); no
# system wayland-protocols carries them, so the bundled copy is used
generate_source 'kde' 'kde-output-device-v2'
generate_source 'kde' 'kde-output-management-v2'
# wlroots' output management (sway, Hyprland, river), from wlr-protocols
generate_source 'wlr' 'wlr-output-management-unstable-v1'
generate_source 'staging/xdg-toplevel-icon' 'xdg-toplevel-icon-v1'
generate_source 'staging/xdg-toplevel-tag' 'xdg-toplevel-tag-v1'
generate_source 'staging/drm-lease' 'drm-lease-v1'

if [ -n "${CROSS_COMPILE:-}" ] && echo "${CROSS_COMPILE:-}" | grep -q "webos"; then
   if [ -z "${STAGING_DIR:-}" ]; then
      die 1 "Error: WEBOS=1 but STAGING_DIR not set"
   fi
   WAYLAND_PROTOS="$STAGING_DIR/usr/share"

   generate_source 'wayland-webos' 'webos-shell'
   generate_source 'wayland-webos' 'webos-foreign'
   generate_source 'wayland-webos' 'webos-surface-group'
   generate_source 'wayland-webos' 'webos-input-manager'
fi
