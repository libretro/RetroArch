#!/bin/sh

source "$(dirname $0)/env-vars.sh"

# load the libstdc++ from gcc-4.7 because phoenix doesn't build in anything less
# preload latest notaz SDL that knows what "pixelperfect" is
exec env LD_PRELOAD=${HOME}/lib/libstdc++.so.6:$HOME/lib/libSDL-1.2.so.0.11.3 retroarch-phoenix "${@}"

