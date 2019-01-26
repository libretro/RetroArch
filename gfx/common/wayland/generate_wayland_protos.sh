#!/bin/sh
WAYSCAN=/usr/bin/wayland-scanner
WAYLAND_PROTOS=/usr/share/wayland-protocols
OUTPUT=gfx/common/wayland

if [ ! -d $WAYLAND_PROTOS ]; then
    WAYSCAN=/usr/local/bin/wayland-scanner
    WAYLAND_PROTOS=/usr/local/share/wayland-protocols
fi

if [ ! -d $OUTPUT ]; then
    mkdir $OUTPUT
fi

if [ "${1:-}" = '1.12' ]; then
   CODEGEN=code
else
   CODEGEN=private-code
fi

#Generate xdg-shell_v6 header and .c files
$WAYSCAN client-header $WAYLAND_PROTOS/unstable/xdg-shell/xdg-shell-unstable-v6.xml $OUTPUT/xdg-shell-unstable-v6.h
$WAYSCAN private-code $WAYLAND_PROTOS/unstable/xdg-shell/xdg-shell-unstable-v6.xml $OUTPUT/xdg-shell-unstable-v6.c

#Generate xdg-shell header and .c files
$WAYSCAN client-header $WAYLAND_PROTOS/stable/xdg-shell/xdg-shell.xml $OUTPUT/xdg-shell.h
$WAYSCAN private-code $WAYLAND_PROTOS/stable/xdg-shell/xdg-shell.xml $OUTPUT/xdg-shell.c

#Generate idle-inhibit header and .c files
$WAYSCAN client-header $WAYLAND_PROTOS/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml $OUTPUT/idle-inhibit-unstable-v1.h
$WAYSCAN private-code $WAYLAND_PROTOS/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml $OUTPUT/idle-inhibit-unstable-v1.c

#Generate xdg-decoration header and .c files
$WAYSCAN client-header $WAYLAND_PROTOS/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml $OUTPUT/xdg-decoration-unstable-v1.h
$WAYSCAN private-code $WAYLAND_PROTOS/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml $OUTPUT/xdg-decoration-unstable-v1.c
