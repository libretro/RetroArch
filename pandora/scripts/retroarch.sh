#!/bin/sh
export PATH="/mnt/utmp/retroarch/bin:${PATH:-"/usr/bin:/bin:/usr/local/bin"}"
export LD_LIBRARY_PATH="/mnt/utmp/retroarch/lib:${LD_LIBRARY_PATH:-"/usr/lib:/lib"}"
export HOME="/mnt/utmp/retroarch" XDG_CONFIG_HOME="/mnt/utmp/retroarch"

if [ -d /mnt/utmp/retroarch/share ];then
	export XDG_DATA_DIRS=/mnt/utmp/retroarch/share:$XDG_DATA_DIRS:/usr/share
fi
export SDL_OMAP_LAYER_SIZE="640x480"
export SDL_VIDEODRIVER="omapdss"
export SDL_AUDIODRIVER="alsa"

cd /mnt/utmp/retroarch/lib
BACKEND=$(ls -1 libretro*.so | zenity --list --column=Backend)
cd $HOME

if [ -z "$BACKEND" ] ; then
	exit 0
fi

FILTER='All files (*)|*'
case "$BACKEND" in
	libretro-fceu*.so)
		FILTER='NES (*.nes)|*.nes'
		;;
	libretro-pocketsnes.so | libretro-snes9x*.so)
		FILTER='SNES (*.sfc)|*.sfc'
		;;
	libretro-meteor.so | libretro-vba.so )
		FILTER='GBA (*.gba)|*.gba'
		export SDL_OMAP_LAYER_SIZE="720x480"
		;;
	libretro-gambatte.so)
		FILTER='GBC (*.gb; *.gbc)|*.gb *.gbc'
		export SDL_OMAP_LAYER_SIZE="534x480"
		;;
	libretro-genplus.so)
		FILTER='Genesis/MegaDrive (*.md; *.gen)|*.md *.gen'
		;;
	libretro-prboom.so)
		FILTER='Doom (*.wad)|*.wad'
		;;
esac

ROM=$(zenity --file-selection --file-filter="${FILTER}")

exec retroarch "${ROM}" -L "/mnt/utmp/retroarch/lib/${BACKEND}" "${@}"

