#!/bin/bash

source "$(dirname $0)/env-vars.sh"

# choose a libretro core.
cd ${HOME}/lib
BACKEND=$(ls -1 libretro*.so | zenity --list --column=Backend)
cd ${HOME}

# if user didn't select a libretro, bail out.
[ -z "${BACKEND}" ] && exit 0

# narrow down the available file formats for the file chooser.
FILTER='All files (*)|*'
case "${BACKEND}" in
	libretro-fceu*.so | libretro-bnes.so)
		FILTER='NES (*.nes)|*.nes'
		;;
	libretro-snes9x*.so | libretro-bsnes*.so)
		FILTER='SNES (*.sfc; *.smc)|*.sfc *.smc'
		;;
	libretro-gambatte.so)
		FILTER='GBC (*.gb; *.gbc; *.sgb)|*.gb *.gbc *.sgb'
		;;
	libretro-meteor.so | libretro-vba.so | libretro-gpsp.so)
		FILTER='GBA (*.gba)|*.gba'
		;;
	libretro-imame4all.so | libretro-fba.so)
		FILTER='Arcade (*.zip)|*.zip'
		;;
	libretro-genplus.so)
		FILTER='Genesis/MegaDrive/SegaCD (*.md; *.gen; *.bin; *.iso)|*.md *.gen *.bin *.iso'
		;;
	libretro-pcsx-rearmed.so | libretro-yabause.so)
		FILTER='Disc image (*.iso; *.bin; *.img; *.cue)|*.iso *.bin *.img *.cue'
		;;
	libretro-prboom.so)
		FILTER='Doom (*.wad)|*.wad'
		;;
esac

# bit hackish, silently adds supported archive formats to file listings.
# worth noting that the pandora itself doesn't have 7z in firmware by default.
if [[ ! "${FILTER}" =~ ^Arcade ]] ; then
	FILTER="${FILTER} *.zip *.rar *.7z"
fi

# try to point the file chooser at the last used path, if there is one.
LASTROM=
if [ -r "${BACKEND}-lastrom.txt" ] ; then
	LASTROM="--filename="$(head -1 "${BACKEND}-lastrom.txt")
fi

ROM=$(zenity --file-selection --file-filter="${FILTER}" "${LASTROM}")

# if user didn't select a ROM, bail out.
[ -z "${ROM}" ] && exit 0

echo "${ROM}" > "${BACKEND}-lastrom.txt"

if [[ "${ROM}" =~ \.(zip|rar|7z)$ ]] && [[ ! "${FILTER}" =~ ^Arcade ]] ; then
	source retroarch-zip "${ROM}" -L "${HOME}/lib/${BACKEND}" "${@}"
else
	exec retroarch "${ROM}" -L "${HOME}/lib/${BACKEND}" "${@}"
fi
