#!/bin/sh
export PATH="/mnt/utmp/retroarch/bin:${PATH:-"/usr/bin:/bin:/usr/local/bin"}"
export LD_LIBRARY_PATH="/mnt/utmp/retroarch/lib:${LD_LIBRARY_PATH:-"/usr/lib:/lib"}"
export HOME="/mnt/utmp/retroarch" XDG_CONFIG_HOME="/mnt/utmp/retroarch"

if [ -d /mnt/utmp/retroarch/share ];then
	export XDG_DATA_DIRS=/mnt/utmp/retroarch/share:$XDG_DATA_DIRS:/usr/share
fi

cd /mnt/utmp/retroarch/lib
BACKEND=$(ls -1 libretro*.so | zenity --list --column=Backend)
cd $HOME

if [ -z "$BACKEND" ] ; then
	exit 0
fi

FILTER='All files (*)|*'
SDL_OMAP_LAYER_SIZE="640x480"
case "$BACKEND" in
	libretro-fceu*.so)
		FILTER='NES (*.nes)|*.nes'
		SDL_OMAP_LAYER_SIZE="512x448"
		;;
	libretro-pocketsnes.so | libretro-snes9x*.so)
		FILTER='SNES (*.sfc)|*.sfc'
		## This still looks distorted for some reason...
		## Both for 512x448 and 512x478.
		SDL_OMAP_LAYER_SIZE="512x448"
		;;
	libretro-gambatte.so)
		FILTER='GBC (*.gb; *.gbc)|*.gb *.gbc'
		SDL_OMAP_LAYER_SIZE="480x432"
		;;
	libretro-meteor.so | libretro-vba.so )
		FILTER='GBA (*.gba)|*.gba'
		SDL_OMAP_LAYER_SIZE="720x480"
		;;
	libretro-imame4all.so | libretro-fba.so)
		FILTER='Arcade (*.zip)|*.zip'
		;;
	libretro-genplus.so)
		FILTER='Genesis/MegaDrive (*.md; *.gen)|*.md *.gen'
		;;
	libretro-prboom.so)
		FILTER='Doom (*.wad)|*.wad'
		;;
esac

export SDL_OMAP_LAYER_SIZE

export SDL_VIDEODRIVER="omapdss"
export SDL_AUDIODRIVER="alsa"

ROM=$(zenity --file-selection --file-filter="${FILTER}")

exec retroarch "${ROM}" -L "/mnt/utmp/retroarch/lib/${BACKEND}" "${@}"

