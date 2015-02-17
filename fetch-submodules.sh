#! /usr/bin/env bash
# vim: set ts=3 sw=3 noet ft=sh : bash

SCRIPT="${0#./}"
BASE_DIR="${SCRIPT%/*}"
WORKDIR=$(pwd)

if [ "$BASE_DIR" = "$SCRIPT" ]; then
	BASE_DIR="$WORKDIR"
else
	BASE_DIR="$WORKDIR/$BASE_DIR"
fi

WORKDIR=$(pwd)


# A stripped-down version of the fetch_git rule from libretro-super
# Clones or pulls updates from a git repository into a local directory
#
# $1	The URI to fetch
# $2	The local directory to fetch to (relative)
fetch_git() {
	fetch_dir="$WORKDIR/$2"
	if [ -d "$fetch_dir/.git" ]; then
		echo "cd \"$fetch_dir\""
		cd "$fetch_dir"
		echo git pull
		git pull
	else
		echo "git clone \"$1\" \"$fetch_dir\""
		git clone "$1" "$fetch_dir"
	fi
}

echo "Fetching RetroArch's submodules..."
fetch_git "https://github.com/libretro/common-shaders.git" "media/shaders_cg"
fetch_git "https://github.com/libretro/common-overlays.git" "media/overlays"
fetch_git "https://github.com/libretro/retroarch-assets.git" "media/assets"
fetch_git "https://github.com/libretro/retroarch-joypad-autoconfig.git" "media/autoconfig"
fetch_git "https://github.com/libretro/libretro-database.git" "media/libretrodb"

# FIXME: This entire script should be unnecessary.  It exists because we don't
# use git submodules in libretro/RetroArch, which introduces one of three
# possible build dependencies:
#
# 1. The user is using libretro-super.  But libretro-super is not supposed to
#    be required because "no dependencies!"
#
# 2. Unreasonable expectations of the user: That they are deeply versed in the
#    changing inner workings of a massively-multi-platform project with dozens
#    of modules and submodules, or that they are somehow psychic.
#
# 3. The user has a script which cannot depend on libretro-super to fetch the
#    submodules for them.  This is it.
#
# The third choice is the path of least resistance, but the correct solution
# is to fix the submodules issue.

