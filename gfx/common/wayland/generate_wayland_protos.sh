#!/bin/sh
WAYSCAN=/usr/bin/wayland-scanner
WAYLAND_PROTOS=/usr/share/wayland-protocols
OUTPUT=gfx/common/wayland

if [ ! -d $OUTPUT ]; then
    mkdir $OUTPUT
fi

#Generate xdg-shell header and .c files
$WAYSCAN client-header $WAYLAND_PROTOS/stable/xdg-shell/xdg-shell.xml $OUTPUT/xdg-shell.h
$WAYSCAN code $WAYLAND_PROTOS/stable/xdg-shell/xdg-shell.xml $OUTPUT/xdg-shell.c

#Generate xdg-decoration header and .c files
$WAYSCAN client-header $WAYLAND_PROTOS/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml $OUTPUT/xdg-decoration-unstable-v1.h
$WAYSCAN code $WAYLAND_PROTOS/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml $OUTPUT/xdg-decoration-unstable-v1.c

#Generate idle-inhibitor header and .c files
$WAYSCAN client-header $WAYLAND_PROTOS/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml $OUTPUT/idle-inhibit-unstable-v1.h
$WAYSCAN code $WAYLAND_PROTOS/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml $OUTPUT/idle-inhibit-unstable-v1.c 
