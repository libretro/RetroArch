#!/bin/sh

DEBUG=

if [ -z "$DEBUG" ] ; then
    CURL_DEBUG=-sf
    UNZIP_DEBUG=-q
else
    printf "Turning on debug\n"
    CURL_DEBUG=
    UNZIP_DEBUG=
fi

function debug() {
    if [ -n "$DEBUG" ] ; then
        printf "$@\n"
    fi
}

if [ -z "$PROJECT_DIR" ] ; then
    APPLE_DIR=$(dirname $0)
else
    APPLE_DIR="${PROJECT_DIR}"
fi

if [ "$1" = "-n" -o "$1" = "--dry-run" ] ; then
    DRY_RUN=1
    shift
else
    DRY_RUN=
fi

URL_BASE="https://buildbot.libretro.com/nightly/apple"

if [ "$PLATFORM_FAMILY_NAME" = "tvOS" -o "$1" = "tvos" -o "$1" = "--tvos" ] ; then
    CORES_DIR="${APPLE_DIR}/tvOS/modules"
    PLATFORM=tvos
    URL_PLATFORMS=( tvos-arm64 )
    if [ "$1" = "tvos" -o "$1" = "--tvos" ] ; then
        shift
    fi
elif [ "$PLATFORM_FAMILY_NAME" = "macOS" -o "$1" = "macos" -o "$1" = "--macos" ] ; then
    CORES_DIR="${APPLE_DIR}/OSX/modules"
    PLATFORM=osx
    URL_PLATFORMS=( osx/arm64 osx/x86_64 )
    if [ "$1" = "macos" -o "$1" = "--macos" ] ; then
        shift
    fi
else
    CORES_DIR="${APPLE_DIR}/iOS/modules"
    PLATFORM=ios
    URL_PLATFORMS=( ios-arm64 )
    if [ "$1" = "ios" -o "$1" = "--ios" ] ; then
        shift
    fi
fi
debug CORES_DIR is $CORES_DIR
cd "$CORES_DIR"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

function update_dylib() {
    dylib=$1
    printf "Updating ${YELLOW}$dylib${NC}...\n"
    if [ -n "$DRY_RUN" ] ; then
        return
    fi

    (
        mkdir "$dylib".tmp
        pushd "$dylib".tmp >/dev/null
        for urlp in "${URL_PLATFORMS[@]}" ; do
            debug curl $CURL_DEBUG -o "$dylib".zip "$URL_BASE"/"$urlp"/latest/"$dylib".zip
            curl $CURL_DEBUG -o "$dylib".zip "$URL_BASE"/"$urlp"/latest/"$dylib".zip
            if [ ! -f "$dylib".zip ] ; then
                printf "${RED}Download ${dylib} failed${NC}\n"
                break
            fi

            debug unzip $UNZIP_DEBUG "$dylib".zip
            unzip $UNZIP_DEBUG "$dylib".zip
            if [ ! -f "$dylib" ] ; then
                printf "${RED}Unzip ${dylib} failed${NC}\n"
                break
            fi
            rm -f "$dylib".zip

            # macos app store needs universal binaries
            if [ "${#URL_PLATFORMS[@]}" != 1 ] ; then
                mv "$dylib" "$dylib".$(basename "$urlp")
            fi
        done
        if [ "${#URL_PLATFORMS[@]}" != 1 ] ; then
            lipo -create -output "$dylib" "$dylib".*
        fi
        popd >/dev/null
        if [ -f "$dylib".tmp/"$dylib" ] ; then
            printf "${GREEN}Download ${dylib} success!${NC}\n"
            mv "$dylib".tmp/"$dylib" "$dylib"
        fi
        rm -rf "$dylib".tmp
    ) &
}

allcores=
function get_all_cores() {
    if [ -z "$allcores" ] ; then
        allcores=($(curl $CURL_DEBUG "$URL_BASE/${URL_PLATFORMS[0]}/latest/" | sed -e 's/></\n/g' | grep '>[^<]\+\.dylib.zip<' | sed -e 's/.*>\(.*\)\.zip<.*/\1/'))
    fi
}

dylibs=()
function add_dylib() {
    if ! [[ "${dylibs[*]}" =~ "${1}" ]] ; then
        dylibs+=("$1")
    fi
}
function find_dylib() {
    if [[ "${allcores[*]}" =~ "${1}_libretro_${PLATFORM}.dylib" ]] ; then
        add_dylib "${1}_libretro_${PLATFORM}.dylib"
    elif [[ "${allcores[*]}" =~ "${1}_libretro.dylib" ]] ; then
        add_dylib "${1}_libretro.dylib"
    elif [[ "${allcores[*]}" =~ "${1}" ]] ; then
        add_dylib "${1}"
    else
        echo "Don't know how to handle '$1'."
    fi
}

get_all_cores

if [ -z "$1" ] ; then
    if find . -maxdepth 1 -iname \*_libretro\*.dylib | grep -q ^. ; then
        dylibs=( *_libretro*.dylib )
    fi
else
    while [ -n "$1" ] ; do
        if [ "$1" = "all" ] ; then
            dylibs=(${allcores[*]})
        elif [ "$1" = "appstore" ] ; then
            exports=(
                2048
                a5200
                anarch
                ardens
                atari800
                #blastem
                bluemsx
                bsnes
                bsnes_hd_beta
                cap32
                crocods
                desmume
                dinothawr
                dirksimple
                dosbox_pure
                DoubleCherryGB
                easyrpg
                ep128emu_core
                fbneo
                fceumm
                #flycast
                freechaf
                freeintv
                fuse
                gambatte
                gearboy
                gearsystem
                genesis_plus_gx
                genesis_plus_gx_wide
                geolith
                gme
                gpsp
                gw
                handy
                kronos
                mednafen_ngp
                mednafen_pce
                mednafen_pce_fast
                mednafen_pcfx
                mednafen_psx
                mednafen_psx_hw
                mednafen_saturn
                mednafen_supergrafx
                mednafen_vb
                mednafen_wswan
                melondsds
                mesen
                mesen-s
                mgba
                mojozork
                mu
                mupen64plus_next
                neocd
                nestopia
                np2kai
                numero
                nxengine
                opera
                pcsx_rearmed
                picodrive
                #play
                pocketcdg
                pokemini
                potator
                ppsspp
                prboom
                prosystem
                puae
                px68k
                quicknes
                race
                sameboy
                scummvm
                smsplus
                snes9x
                snes9x2005
                snes9x2010
                stella
                stella2014
                stella2023
                tgbdual
                theodore
                tic80
                tyrquake
                vba_next
                vbam
                vecx
                vice_x128
                vice_x64
                vice_x64sc
                vice_xcbm2
                vice_xcbm5x0
                vice_xpet
                vice_xplus4
                vice_xscpu64
                vice_xvic
                vircon32
                virtualxt
                wasm4
                xrick
            )
            if [ "$PLATFORM" = "osx" ] ; then
                exports+=(
                    flycast
                    play
                )
            fi
            for dylib in "${exports[@]}" ; do
                find_dylib $dylib
            done
        else
            find_dylib "$1"
        fi
        shift
    done
fi

if [[ -z "${dylibs[*]}" ]] ; then
    echo Available cores:
    for i in "${allcores[@]}" ; do
        echo $i
    done
fi

# TODO: command line arg to indicate which cores to update
for dylib in "${dylibs[@]}" ; do
    update_dylib "$dylib"
done
wait
