#!/bin/sh
# Compile the translation units whose contents depend on which codecs or
# which platform a build selects, across the combinations real builds
# actually use.
#
# The ordinary CI jobs each build one configuration, so a file that
# compiles for the configuration they build is green even when it cannot
# compile for a console's. This has broken the tree: giving WAV a
# streaming mixer type made it a user of machinery guarded on the
# compressed codecs, which every desktop build has and Wii U - WAV and
# nothing else - does not. Nothing caught it until the console job ran.
#
# Nothing here links or runs; it is a syntax and declaration check, which
# is the class of breakage this is for.

set -e
cd "$(dirname "$0")/../../.."

CC=${CC:-gcc}
INC="-I. -Ilibretro-common/include -Ideps -Ideps/rcheevos/include -Iinput/include"
WARN="-Wall -Wno-unused-function -Werror=implicit-function-declaration"
BASE="-DRARCH_INTERNAL -DHAVE_AUDIOMIXER -DHAVE_THREADS -DHAVE_CONFIGFILE -DHAVE_MENU"

fail=0

check() {
   name="$1"; shift
   defs="$1"; shift
   for tu in "$@"; do
      if ! out=$($CC $WARN $INC $BASE $defs -fsyntax-only "$tu" 2>&1); then
         echo "FAIL  $name"
         echo "      $tu"
         echo "$out" | sed 's/^/      /' | head -12
         fail=1
      fi
   done
   [ "$fail" = 1 ] || echo "ok    $name"
}

AUDIO="libretro-common/formats/audio_transfer.c libretro-common/audio/audio_mixer.c"

echo "== audio codec combinations =="
# the sets real platform makefiles select, plus each codec alone and none
check "wav only (ctr/psp/wiiu)"   "-DHAVE_RWAV"                                   $AUDIO
check "wav+mod (gx/ngc/wii)"      "-DHAVE_RWAV -DHAVE_RMODTRACKER"                $AUDIO
check "mod+vorbis (emscripten)"   "-DHAVE_RMODTRACKER -DHAVE_RVORBIS"             $AUDIO
check "wav+vorbis (vita)"         "-DHAVE_RWAV -DHAVE_RVORBIS"                    $AUDIO
check "wav+vorbis+mp3+mod (nx)"   "-DHAVE_RWAV -DHAVE_RVORBIS -DHAVE_RMP3 -DHAVE_RMODTRACKER" $AUDIO
check "mod+mp3+vorbis (dos)"      "-DHAVE_RMODTRACKER -DHAVE_RMP3 -DHAVE_RVORBIS" $AUDIO
check "vorbis only"               "-DHAVE_RVORBIS"                                $AUDIO
check "flac only"                 "-DHAVE_RFLAC"                                  $AUDIO
check "mp3 only"                  "-DHAVE_RMP3"                                   $AUDIO
check "mod only"                  "-DHAVE_RMODTRACKER"                            $AUDIO
check "aac only"                  "-DHAVE_RAAC"                                   $AUDIO
check "aac+mp4"                   "-DHAVE_RAAC -DHAVE_RMP4"                       $AUDIO
check "opus only"                 "-DHAVE_ROPUS"                                  $AUDIO
check "opus+webm"                 "-DHAVE_ROPUS -DHAVE_RWEBM"                     $AUDIO
check "no codecs"                 ""                                              $AUDIO
check "everything" \
  "-DHAVE_RWAV -DHAVE_RVORBIS -DHAVE_RFLAC -DHAVE_RMP3 -DHAVE_RMODTRACKER -DHAVE_RAAC -DHAVE_ROPUS -DHAVE_RMP4 -DHAVE_RWEBM" \
  $AUDIO


# mem_stats selects one platform backend out of a dozen, and its includes
# and its bodies must both follow that one choice.  When they were two
# separate chains, a platform matching an early arm while still defining
# __unix__ - Orbis and Emscripten do, and DJGPP does - got its own arm's
# includes and a different arm's code, and three console jobs failed on
# symbols the arm had never included a header for.
#
# The SDK headers are not here, so an arm whose header is missing cannot
# be compiled.  What can be checked anywhere is that the only thing it
# complains about is that header: a diagnostic naming a symbol that
# belongs to some other arm means the chains have drifted apart again.
echo
echo "== mem_stats platform arms =="

# Only symbols that belong to no console arm: each arm's own symbols go
# undeclared here simply because its SDK header is absent, which is
# expected.  These six are the /proc and sysconf paths, and their
# appearing in a console arm's diagnostics is the drift being looked for.
CONTAMINANT='O_RDONLY|ssize_t|_SC_PHYS_PAGES|_SC_PAGE_SIZE|MemAvailable|implicit declaration of function .(open|read|close).'

arm() {
   name="$1"; defs="$2"
   out=$($CC $WARN $INC $defs -fsyntax-only \
      libretro-common/memory/mem_stats.c 2>&1 || true)
   bad=$(printf '%s\n' "$out" | grep -Ei "$CONTAMINANT" || true)
   if [ -n "$bad" ]; then
      echo "FAIL  $name got another arm's code"
      printf '%s\n' "$bad" | sed 's/^/      /' | head -6
      fail=1
   elif printf '%s\n' "$out" | grep -q "error:"; then
      echo "ok    $name (SDK header absent, no foreign symbols)"
   else
      echo "ok    $name"
   fi
}

arm "3ds"        "-D_3DS"
arm "gamecube"   "-DGEKKO"
arm "wii"        "-DGEKKO -DHW_RVL"
arm "vita"       "-DVITA"
arm "switch"     "-DHAVE_LIBNX"
arm "orbis"      "-DORBIS"
arm "ps3"        "-D__PSL1GHT__ -DHAVE_MEMINFO"
arm "ps2"        "-DPS2"
arm "emscripten" "-D__EMSCRIPTEN__"
arm "dos/djgpp"  "-D__DJGPP__ -D__unix__"
arm "linux"      ""

exit $fail
