#!/bin/sh
WAYSCAN=/usr/bin/wayland-scanner
WAYLAND_PROTOS=/usr/share/wayland-protocols
OUTPUT=gfx/common/wayland

if [ ! -d $OUTPUT ]; then
    mkdir $OUTPUT
fi

#Generate xdg-shell header and .c files
$WAYSCAN client-header $WAYLAND_PROTOS/stable/xdg-shell/xdg-shell.xml $OUTPUT/xdg-shell.h
$WAYSCAN private-code $WAYLAND_PROTOS/stable/xdg-shell/xdg-shell.xml $OUTPUT/xdg-shell.c

#Generate idle-inhibit header and .c files
$WAYSCAN client-header $WAYLAND_PROTOS/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml $OUTPUT/idle-inhibit-unstable-v1.h
$WAYSCAN private-code $WAYLAND_PROTOS/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml $OUTPUT/idle-inhibit-unstable-v1.c
