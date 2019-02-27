#!/bin/sh

export HOME="$(readlink -f $(dirname $0)/..)"
export XDG_CONFIG_HOME="${HOME}"
export PATH="${HOME}/bin:${PATH:-"/usr/bin:/bin:/usr/local/bin"}"
export LD_LIBRARY_PATH="${HOME}/lib:${LD_LIBRARY_PATH:-"/usr/lib:/lib"}"

if [ -d /mnt/utmp/retroarch/share ] ; then
	export XDG_DATA_DIRS=${HOME}/share:$XDG_DATA_DIRS:/usr/share
fi

# use notaz's optimized driver
export SDL_VIDEODRIVER="omapdss"
export SDL_AUDIODRIVER="alsa"

# integral scaling
export SDL_OMAP_LAYER_SIZE="pixelperfect"

# load the libstdc++ from gcc-4.7 because phoenix and some cores don't build in anything less
# preload latest notaz SDL that knows what "pixelperfect" is
export LD_PRELOAD=${HOME}/lib/libstdc++.so.6:$HOME/lib/libSDL-1.2.so.0.11.3
