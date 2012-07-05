#!/bin/sh

export PATH="/mnt/utmp/retroarch/bin:${PATH:-"/usr/bin:/bin:/usr/local/bin"}"
export LD_LIBRARY_PATH="/mnt/utmp/retroarch/lib:${LD_LIBRARY_PATH:-"/usr/lib:/lib"}"
export HOME="/mnt/utmp/retroarch" XDG_CONFIG_HOME="/mnt/utmp/retroarch"

if [ -d /mnt/utmp/retroarch/share ] ; then
	export XDG_DATA_DIRS=/mnt/utmp/retroarch/share:$XDG_DATA_DIRS:/usr/share
fi

# use notaz's optimized driver
export SDL_VIDEODRIVER="omapdss"
export SDL_AUDIODRIVER="alsa"

# integral scaling
export SDL_OMAP_LAYER_SIZE="pixelperfect"

