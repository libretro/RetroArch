#!/bin/sh

DEBUG=

if [ -z "$DEBUG" ] ; then
    CURL_DEBUG=-sf
    UNZIP_DEBUG=-q
    NO_RM=
else
    printf "Turning on debug\n"
    CURL_DEBUG=
    UNZIP_DEBUG=
    NO_RM=1
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

if [ "$1" = "tvos" -o "$1" = "--tvos" ] ; then
    CORES_DIR="${APPLE_DIR}/tvOS/modules"
    PLATFORM=tvos
    shift
else
    CORES_DIR="${APPLE_DIR}/iOS/modules"
    PLATFORM=ios
fi
debug CORES_DIR is $CORES_DIR
cd "$CORES_DIR"

URL_BASE="https://buildbot.libretro.com/nightly/apple/${PLATFORM}-arm64/latest"
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
    if [ -f "$dylib" ] ; then
        mv "$dylib" "$dylib".bak
    fi
    debug curl $CURL_DEBUG -o "$dylib".zip "$URL_BASE"/"$dylib".zip
    (
    curl $CURL_DEBUG -o "$dylib".zip "$URL_BASE"/"$dylib".zip
    if [ ! -f "$dylib".zip ] ; then
        printf "${RED}Download ${dylib} failed${NC}\n"
        if [ -f "$dylib".bak ] ; then
            mv "$dylib".bak "$dylib"
        fi
    else
        debug unzip $UNZIP_DEBUG "$dylib".zip
        unzip $UNZIP_DEBUG "$dylib".zip
        if [ ! -f "$dylib" ] ; then
            printf "${RED}Unzip ${dylib} failed${NC}\n"
            mv "$dylib".bak "$dylib"
        else
            printf "${GREEN}Download ${dylib} success!${NC}\n"
            [ -n "$NO_RM" ] || rm -f "$dylib".zip "$dylib".bak
        fi
    fi
    ) &
}

allcores=
function get_all_cores() {
    if [ -z "$allcores" ] ; then
        allcores=($(curl $CURL_DEBUG $URL_BASE/ | sed -e 's/></\n/g' | grep '>[^<]\+\.dylib.zip<' | sed -e 's/.*>\(.*\)\.zip<.*/\1/'))
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
