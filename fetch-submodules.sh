#! /usr/bin/env bash
# vim: set ts=3 sw=3 noet ft=sh : bash

# TODO: This entire script _should_ be replaced with git submodules, but
# that cannot be done until we sort out the limitations of that option.  At
# this time, this script is called by libretro-super.  Revisit the whole
# issue at some point.

SCRIPT="${0#./}"
BASE_DIR="${SCRIPT%/*}"
WORKDIR="$PWD"

if [ "$BASE_DIR" = "$SCRIPT" ]; then
	BASE_DIR="$WORKDIR"
else
	if [[ "$0" != /* ]]; then
		# Make the path absolute
		BASE_DIR="$WORKDIR/$BASE_DIR"
	fi
fi

# Inserted here is libretro-super's script-modules/fetch-rules.sh, with a
# couple of features related to core submodules removed from fetch_git.  If
# that file is changed, it should be safe to import it verbatim.

### START OF FETCH-RULES.SH (with mods)

# fetch_git: Clones or pulls updates from a git repository into a local directory
#
# $1	The URI to fetch
# $2	The local directory to fetch to (relative)
#
# NOTE: git _now_ has a -C argument that would replace the cd commands in
#       this rule, but this is a fairly recent addition to git, so we can't
#       use it here.  --iKarith
fetch_git() {
	fetch_dir="$WORKDIR/$2"
	if [ -d "$fetch_dir/.git" ]; then
		echo "cd \"$fetch_dir\""
		cd "$fetch_dir"
		echo "git pull"
		git pull
	else
		clone_type=
		[ -n "$SHALLOW_CLONE" ] && depth="--depth 1"
		echo "git clone $depth \"$1\" \"$WORKDIR/$2\""
		git clone $depth "$1" "$WORKDIR/$2"
	fi
}

# fetch_revision_git: Output the hash of the last commit in a git repository
#
# $1	Local directory to run git in
fetch_revision_git() {
	[ -n "$1" ] && cd "$1"
	git log -n 1 --pretty=format:%H
}

# fetch_revision: Output SCM-dependent revision string of a module
#                 (currently just calls fetch_revision_git)
#
# $1	The directory of the module
fetch_revision() {
	   fetch_revision_git $1
}

### END OF FETCH-RULES.SH

echo "Fetching RetroArch's submodules..."
fetch_git "https://github.com/libretro/common-shaders.git" "media/shaders_cg"
fetch_git "https://github.com/libretro/common-overlays.git" "media/overlays"
fetch_git "https://github.com/libretro/retroarch-assets.git" "media/assets"
fetch_git "https://github.com/libretro/retroarch-joypad-autoconfig.git" "media/autoconfig"
fetch_git "https://github.com/libretro/libretro-database.git" "media/libretrodb"

git submodule update --init --recursive
