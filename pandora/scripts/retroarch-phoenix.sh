#!/bin/sh

source "/mnt/utmp/retroarch/scripts/env-vars.sh"

# load the libstdc++ from gcc-4.7 because phoenix doesn't build in anything less
# preload my modified SDL that knows what "pixelperfect" is
exec env LD_PRELOAD=/mnt/utmp/retroarch/lib/libstdc++.so.6:/mnt/utmp/retroarch/lib/libSDL-1.2.so.0.11.3 retroarch-phoenix "${@}"

