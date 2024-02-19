#!/bin/bash

WD=$(realpath $(dirname $0))

include_cheats=""
include_overlays=""
include_shaders=""
assets_zip="$WD/assets.zip"

args=`getopt achmos $*`
set -- $args
while :; do
    case "$1" in
        -a)
            include_cheats=1
            include_overlays=1
            include_shaders=1
            shift
            ;;
        -c)
            include_cheats=1
            shift
            ;;
        -h)
            echo "$(basename $0) -- Rebuild assets.zip"
            echo "Meant to be used when building RetroArch yourself. The buildbot does not use this."
            echo
            echo " -c    Include cheats"
            echo " -o    Include overlays"
            echo " -s    Include shaders"
            echo " -a    Include cheats, overlays, and shaders"
            echo " -m    Build for macOS (places in OSX directory"
            exit 0
            ;;
        -m)
            assets_zip="$WD/OSX/assets.zip"
            shift
            ;;
        -o)
            include_overlays=1
            shift
            ;;
        -s)
            include_shaders=1
            shift
            ;;
        --)
            shift
            break
            ;;
    esac
done

function fetch_zip()
{
    echo "Fetching $1..."
    curl -s -o tmp.zip "https://git.libretro.com/api/v4/projects/libretro-assets%2F$1/jobs/artifacts/master/download?job=libretro-package-any"
    echo "  Unzipping..."
    unzip -q tmp.zip
    rm -f tmp.zip
}

pushd "$WD" &>/dev/null

rm -rf .media
fetch_zip retroarch-assets
fetch_zip retroarch-joypad-autoconfig
fetch_zip libretro-database
fetch_zip libretro-super
if [ -n "$include_overlays" ] ; then
    fetch_zip common-overlays
fi
if [ -n "$include_shaders" ] ; then
    fetch_zip glsl-shaders
    fetch_zip slang-shaders
fi

pushd .media &>/dev/null

echo "Packaging assets"
mkdir assets ; mv retroarch-assets/{COPYING,glui,menu_widgets,ozone,pkg,rgui,sounds,xmb} assets ; rm -rf retroarch-assets
rm -rf assets/pkg/wiiu

echo "Packaging autoconfig"
mv retroarch-joypad-autoconfig autoconfig
rm -rf autoconfig/{android,dinput,linuxraw,parport,qnx,sdl2,udev,x,xinput}

if [ -n "$include_cheats" ] ; then
    echo "Packaging cheats"
    mv libretro-database/cht cht
fi
echo "Packaging database"
mkdir database ; mv libretro-database/{cursors,rdb} database ; rm -rf libretro-database

echo "Packaging info"
mv libretro-super/info info ; rmdir libretro-super

if [ -n "$include_overlays" ] ; then
    echo "Packaging overlays"
    mv common-overlays overlays
    rm -rf overlays/{ctr,wii}
fi

if [ -n "$include_shaders" ] ; then
    echo "Packaging shaders"
    mkdir shaders ; mv glsl-shaders shaders/shaders_glsl ; mv slang-shaders shaders/shaders_slang
fi

rm -f ../assets.zip
echo "Zipping final assets bundle..."
zip -qr ../assets.zip *

popd &>/dev/null

rm -rf .media

echo "Done!"
