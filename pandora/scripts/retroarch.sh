#!/bin/sh
export PATH="/mnt/utmp/retroarch/bin:${PATH:-"/usr/bin:/bin:/usr/local/bin"}"
export LD_LIBRARY_PATH="/mnt/utmp/retroarch/lib:${LD_LIBRARY_PATH:-"/usr/lib:/lib"}"
export HOME="/mnt/utmp/retroarch" XDG_CONFIG_HOME="/mnt/utmp/retroarch"

if [ -d /mnt/utmp/retroarch/share ] ; then
	export XDG_DATA_DIRS=/mnt/utmp/retroarch/share:$XDG_DATA_DIRS:/usr/share
fi

# choose a libretro core.
cd /mnt/utmp/retroarch/lib
BACKEND=$(ls -1 libretro*.so | zenity --list --column=Backend)
cd $HOME

# if user didn't select a libretro, bail out.
[ -z "$BACKEND" ] && exit 0

# narrow down the available file formats for the file chooser.
FILTER='All files (*)|*'
case "$BACKEND" in
	libretro-fceu*.so)
		FILTER='NES (*.nes)|*.nes'
		;;
	libretro-pocketsnes.so | libretro-snes9x*.so)
		FILTER='SNES (*.sfc)|*.sfc'
		;;
	libretro-gambatte.so)
		FILTER='GBC (*.gb; *.gbc)|*.gb *.gbc'
		;;
	libretro-meteor.so | libretro-vba.so )
		FILTER='GBA (*.gba)|*.gba'
		;;
	libretro-imame4all.so) # does libretro-fba.so belong here?
		FILTER='Arcade (*.zip)|*.zip'
		;;
	libretro-genplus.so)
		FILTER='Genesis/MegaDrive (*.md; *.gen)|*.md *.gen'
		;;
	libretro-prboom.so)
		FILTER='Doom (*.wad)|*.wad'
		;;
esac

# try to point the file chooser at the last used path, if there is one.
LASTROM=
if [ -r "${BACKEND}-lastrom.txt" ] ; then
	LASTROM="--filename="$(head -1 "${BACKEND}-lastrom.txt")
fi

ROM=$(zenity --file-selection --file-filter="${FILTER}" "${LASTROM}")

# if user didn't select a ROM, bail out.
[ -z "$ROM" ] && exit 0

echo "$ROM" > "${BACKEND}-lastrom.txt"

# use notaz's optimized driver
export SDL_VIDEODRIVER="omapdss"
export SDL_AUDIODRIVER="alsa"

# integral scaling
export SDL_OMAP_LAYER_SIZE="pixelperfect"

# preload my modified SDL that knows what "pixelperfect" is
exec env LD_PRELOAD=/mnt/utmp/retroarch/lib/libSDL-1.2.so.0.11.3 retroarch "${ROM}" -L "/mnt/utmp/retroarch/lib/${BACKEND}" "${@}"

