#!/bin/sh

source "/mnt/utmp/retroarch/scripts/env-vars.sh"

# load the libstdc++ from gcc-4.7 because phoenix doesn't build in anything less
exec env LD_PRELOAD=/mnt/utmp/retroarch/lib/libstdc++.so.6 retroarch-phoenix "${@}"

