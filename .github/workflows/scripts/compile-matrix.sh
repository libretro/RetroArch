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
   bad=0
   for tu in "$@"; do
      if ! out=$($CC $WARN $INC $BASE $defs -fsyntax-only "$tu" 2>&1); then
         echo "FAIL  $name"
         echo "      $tu"
         echo "$out" | sed 's/^/      /' | head -12
         fail=1; bad=1
      fi
   done
   [ "$bad" = 1 ] || echo "ok    $name"
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

# The SDKs are not on a runner, so each arm is compiled against a stub
# header supplying only the handful of symbols that arm names. That does
# not prove the real SDK matches the stub - it proves the arm's own code
# is well formed and that every symbol it uses is one it has arranged to
# have. Three console failures reached CI as missing or wrong headers -
# 3ds.h absent, gccore.h guessed, SYSMEM1_SIZE taken from the SDK when
# it is RetroArch's own - and each would have shown up here.
STUBS="$(dirname "$0")/stubs"
# host headers are the real ones and deprecate things the target's do not
ARMWARN="$WARN -Wno-deprecated-declarations"

# A platform's own video driver, which the arm lanes above do not reach:
# they check that one file's headers resolve, not that the driver a
# console actually draws with still compiles. Settings a driver reads
# while answering the frontend have moved into the frame descriptor more
# than once, and each time these drivers were the ones left behind -
# nothing in CI compiles them.
#
# @extra carries whatever headers the driver needs that the runner has;
# a lane whose headers are absent says so and is not a failure.
platform_video() {
   name="$1"; defs="$2"; extra="$3"; tu="$4"; probe="$5"
   if [ -n "$probe" ] && [ ! -f "$probe" ]; then
      echo "skip  $name (no $probe)"
      return
   fi
   if ! out=$($CC $WARN $extra $INC $BASE $defs -fsyntax-only "$tu" 2>&1); then
      echo "FAIL  $name"
      echo "      $tu"
      printf '%s\n' "$out" | sed 's/^/      /' | head -10
      fail=1
   else
      echo "ok    $name"
   fi
}

arm() {
   name="$1"; stub="$2"; defs="$3"
   inc="$INC"
   [ -n "$stub" ] && inc="-I$STUBS/$stub $INC"
   if ! out=$($CC $ARMWARN $inc $defs -fsyntax-only \
         libretro-common/memory/mem_stats.c 2>&1); then
      echo "FAIL  $name"
      printf '%s\n' "$out" | sed 's/^/      /' | head -8
      fail=1
   elif [ -n "$out" ]; then
      echo "FAIL  $name (warnings)"
      printf '%s\n' "$out" | sed 's/^/      /' | head -6
      fail=1
   else
      echo "ok    $name"
   fi
}

arm "3ds"        ctr        "-D_3DS"
arm "gamecube"   gx         "-DGEKKO"
arm "wii"        gx         "-DGEKKO -DHW_RVL"
arm "vita"       ""         "-DVITA"
arm "switch"     libnx      "-DHAVE_LIBNX"
arm "orbis"      orbis      "-DORBIS"
arm "ps3"        ps3        "-D__PSL1GHT__ -DHAVE_MEMINFO"
arm "ps2"        ""         "-DPS2"
arm "emscripten" emscripten "-D__EMSCRIPTEN__"

platform_video "odroidgo2 video" \
   "-DHAVE_ODROIDGO2 -DHAVE_OPENGL -DHAVE_GLSL" "" \
   gfx/drivers/gl2.c ""
platform_video "gx video" "-DGEKKO -DHW_RVL" "-I$STUBS/gx" \
   gfx/drivers/gx_gfx.c ""
platform_video "switch video" \
   "-DHAVE_LIBNX -DSWITCH -D__SWITCH__" "-I$STUBS/libnx" \
   gfx/drivers/switch_nx_gfx.c ""
# The PSP driver folds its VRAM and cache-alias addresses into pointers,
# which a 64-bit host narrows; the arithmetic is 32-bit on the target.
platform_video "psp1 video" "-DPSP" \
   "-Itools/platform_stubs/psp -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
    -Wdeclaration-after-statement -Werror=declaration-after-statement" \
   gfx/drivers/psp1_gfx.c ""
platform_video "dingux video"   "-DDINGUX" "-I/usr/include/SDL" \
   gfx/drivers/sdl_dingux_gfx.c /usr/include/SDL/SDL.h
platform_video "rs90 video"     "-DDINGUX -DRS90" "-I/usr/include/SDL" \
   gfx/drivers/sdl_rs90_gfx.c /usr/include/SDL/SDL.h
arm "win32"      win32      "-D_WIN32 -D_WIN32_WINNT=0x0600"
arm "win32-old"  win32      "-D_WIN32 -D_WIN32_WINNT=0x0400"
arm "macos"      apple      "-D__APPLE__"
arm "ios"        apple      "-D__APPLE__ -DTARGET_OS_IPHONE=1"
arm "dos/djgpp"  ""         "-D__DJGPP__ -D__unix__"
arm "linux"      ""         ""


# The video drivers and the threaded wrapper compile under more than
# one API and one threading model, and the desktop job builds one of
# each. The OpenGL ring's sync objects went in under a guard the
# desktop build satisfied and the GLES builds did not, and every GLES
# target - Android, Emscripten, Vita, PS4 - went red at once; a
# threadless build had been broken for longer than anyone noticed, on
# calls that compile against a prototype and fail only at link.
# These lanes are the ones a desktop build does not exercise.
echo
echo "== video drivers: API and threading combinations =="

GLINC="-Igfx/include"
GLDEFS="-DHAVE_OPENGL -DHAVE_GLSL -DHAVE_FBO -DHAVE_REWIND -DHAVE_OVERLAY"
check "gl2: GLES2 (android/emscripten/vita/ps4)" \
   "$GLDEFS -DHAVE_OPENGLES -DHAVE_OPENGLES2 -DHAVE_EGL $GLINC" gfx/drivers/gl2.c
check "gl2: GLES3" \
   "$GLDEFS -DHAVE_OPENGLES -DHAVE_OPENGLES3 -DHAVE_EGL $GLINC" gfx/drivers/gl2.c
check "gl2: desktop" \
   "$GLDEFS $GLINC" gfx/drivers/gl2.c
GL3DEFS="-DHAVE_OPENGL -DHAVE_OPENGL_CORE -DHAVE_SLANG -DHAVE_GLSLANG -DHAVE_SPIRV_CROSS -DHAVE_REWIND -DHAVE_OVERLAY -Ideps/SPIRV-Cross"
check "gl3: desktop" \
   "$GL3DEFS $GLINC" gfx/drivers/gl3.c
check "gl3: GLES3" \
   "$GL3DEFS -DHAVE_OPENGLES -DHAVE_OPENGLES3 -DHAVE_EGL $GLINC" gfx/drivers/gl3.c
# gl3 is not in the C89 job's configuration, so its C89 lane is here.
# The flags the Makefile's C89_BUILD lane uses, _GNU_SOURCE included:
# -ansi hides the C99 math names otherwise.
C89="-std=c89 -ansi -pedantic -Werror=pedantic -Werror=declaration-after-statement -Wno-long-long -Wno-variadic-macros -D_GNU_SOURCE -DC89_BUILD"
check "gl3: desktop, C89" \
   "$GL3DEFS $GLINC $C89" gfx/drivers/gl3.c
check "hw ring: OpenGL only" \
   "-DHAVE_OPENGL $GLINC" gfx/video_thread_hw.c
check "hw ring: GLES only" \
   "-DHAVE_OPENGLES -DHAVE_OPENGLES2 $GLINC" gfx/video_thread_hw.c
check "hw ring: Vulkan only" \
   "-DHAVE_VULKAN $GLINC" gfx/video_thread_hw.c
check "hw ring: no hardware API" \
   "$GLINC" gfx/video_thread_hw.c

# Widget state lock: the worker draws widgets while the main thread
# writes them, and the wrapper yields the lock around its waits.
check "widgets: state lock" "$GLINC -DHAVE_GFX_WIDGETS" \
   gfx/gfx_widgets.c gfx/video_thread_wrapper.c gfx/video_driver.c runloop.c

# Without threads: the wrapper is not built, and callers must compile
# against the macro stand-ins, not the wrapper's prototypes. Syntax
# only here; the link is the threadless job's.
NOTHREADS=$(echo "$BASE" | sed 's/-DHAVE_THREADS//')
check_nothreads() {
   name="$1"; shift
   defs="$1"; shift
   bad=0
   for tu in "$@"; do
      if ! out=$($CC $WARN $INC $NOTHREADS $defs -fsyntax-only "$tu" 2>&1); then
         echo "FAIL  $name"
         echo "      $tu"
         echo "$out" | sed 's/^/      /' | head -12
         fail=1; bad=1
      fi
   done
   [ "$bad" = 1 ] || echo "ok    $name"
}
# Console rgui: the menu driver's GEKKO and DINGUX shapes have their
# own framebuffer dimensions, aspect handling and pixel converters;
# a syntax pass keeps refactors honest for both. GEKKO needs one
# tiny libogc stub header (tools/platform_stubs/gekko).
# The Android OpenSL driver compiles nowhere else; a stub SLES header
# set (tools/platform_stubs/android) keeps its lock-free write path
# and hardened teardown under a syntax gate.
# OpenAL: not in the audit build's configure. Compile-only AL stubs,
# first in the include order, so the lane needs no system
# libopenal-dev and behaves the same on every runner. Covers the
# eventcount park path (threads on).
check "audio: openal" "-Itools/platform_stubs/openal -DHAVE_AL -DHAVE_THREADS -Wdeclaration-after-statement -Werror=declaration-after-statement" audio/drivers/openal.c

# psp/psp2 audio: index-pair SPSC + eventcount park, one driver per
# platform; compile-only sce stubs.
check "psp: psp_audio" "-Itools/platform_stubs/psp -DPSP -DHAVE_THREADS -Wdeclaration-after-statement -Werror=declaration-after-statement" audio/drivers/psp_audio.c
check "vita: psp2_audio" "-Itools/platform_stubs/vita -DVITA -DHAVE_THREADS -Wdeclaration-after-statement -Werror=declaration-after-statement" audio/drivers/psp2_audio.c

# features_cpu.c on every statically linked platform. It owns
# cpu_features_get_time_usec() and retro_sleep_until_us(); the wait's
# generic loop calls retro_sleep_us, a per-platform macro from
# retro_timers.h everywhere but Windows and Darwin, so the branch that
# only the console toolchains compile is compiled here, against
# hermetic stubs of each SDK header the file pulls. The host compiler
# predefines __linux__ and friends; a lane that keeps them takes the
# Linux branch of every #if ladder and proves nothing, so these shed
# them. PSP gets _POSIX_C_SOURCE for the host libc's nanosleep, which
# pspsdk's newlib declares unconditionally.
FCPU_TU=libretro-common/features/features_cpu.c
HOSTOFF="-U__linux__ -U__gnu_linux__ -Ulinux -U__unix__ -U__unix -Uunix"
check "features_cpu: 3ds"        "$HOSTOFF -Itools/platform_stubs/ctr -D_3DS -D__3DS__ -DARM11 -DRARCH_CONSOLE" $FCPU_TU
check "features_cpu: gekko"      "$HOSTOFF -Itools/platform_stubs/gekko -DGEKKO -DHW_RVL -DRARCH_CONSOLE" $FCPU_TU
check "features_cpu: wiiu"       "$HOSTOFF -Itools/platform_stubs/wiiu -DWIIU -DRARCH_CONSOLE" $FCPU_TU
check "features_cpu: psp"        "$HOSTOFF -Itools/platform_stubs/psp -DPSP -D_POSIX_C_SOURCE=199309L -DRARCH_CONSOLE" $FCPU_TU
check "features_cpu: vita"       "$HOSTOFF -Itools/platform_stubs/vita -DVITA -DRARCH_CONSOLE" $FCPU_TU
check "features_cpu: ps3"        "$HOSTOFF -Itools/platform_stubs/ps3 -D__PS3__ -DRARCH_CONSOLE" $FCPU_TU
check "features_cpu: psl1ght"    "$HOSTOFF -Itools/platform_stubs/psl1ght -D__PS3__ -D__PSL1GHT__ -DRARCH_CONSOLE" $FCPU_TU
check "features_cpu: emscripten" "$HOSTOFF -Itools/platform_stubs/emscripten -D__EMSCRIPTEN__ -DEMSCRIPTEN" $FCPU_TU

# The salamander launchers link a hand-picked subset of libretro-common
# with -DIS_SALAMANDER: rtime.c for rtime_localtime, no features_cpu.c.
# A syntax pass cannot see the link error that a symbol from an
# unlinked TU produces there, so this lane links exactly that object
# set with a host main, under the Vita salamander's defines.
salamander_link() {
   name="$1"; shift
   defs="$1"; shift
   if ! out=$($CC $WARN $INC $BASE $defs -o /tmp/salamander_link_check \
         tools/platform_stubs/salamander_main.c "$@" 2>&1); then
      echo "FAIL  $name"
      echo "$out" | sed 's/^/      /' | head -12
      fail=1
   else
      echo "ok    $name"
   fi
   rm -f /tmp/salamander_link_check
}
salamander_link "salamander link: rtime.c" \
   "$HOSTOFF -UHAVE_THREADS -Itools/platform_stubs/vita -DVITA -DIS_SALAMANDER -DRARCH_CONSOLE" \
   libretro-common/time/rtime.c

# runloop.c under Android: runloop_idle_wait's looper branch, and every
# other ANDROID block in the file, compiled against hermetic NDK stubs
# (jni, looper, native_activity, sensor, configuration, window) with
# the host identity shed.
check "android: runloop" "$HOSTOFF -DANDROID -Itools/platform_stubs/android" runloop.c
check "android: dispserv" "$HOSTOFF -DANDROID -Itools/platform_stubs/android" gfx/display_servers/dispserv_android.c

# The Android arms of rthreads: thread affinity goes to the kernel
# directly there (bionic keeps cpu_set_t behind _GNU_SOURCE), and the
# API 21 arm names pthread_gettid_np, which only bionic declares.
check "android: rthreads (API 21)" "-DHAVE_THREADS -D__ANDROID__ -D__ANDROID_API__=21 -include tools/platform_stubs/android/bionic_pthread_stub.h -Itools/platform_stubs/android" libretro-common/rthreads/rthreads.c
check "android: rthreads (API 19)" "-DHAVE_THREADS -D__ANDROID__ -D__ANDROID_API__=19 -Itools/platform_stubs/android" libretro-common/rthreads/rthreads.c

check "android: opensl" "-DANDROID -DHAVE_OPENSL -Itools/platform_stubs/android -Wdeclaration-after-statement -Werror=declaration-after-statement" audio/drivers/opensl.c

check "gekko: rgui"  "-DGEKKO -DHAVE_MENU -DHAVE_RGUI -Itools/platform_stubs/gekko" menu/drivers/rgui.c
check "dingux: rgui" "-DDINGUX -DHAVE_MENU -DHAVE_RGUI" menu/drivers/rgui.c

# The streaming surface embeds a wrapper node under HAVE_THREADS and
# collapses to direct driver calls without; both shapes compile here,
# along with the thumbnail code that produces into it.
# The VP9 decoder's tile-column threading and the video streams' pooled
# colour conversion sit behind HAVE_THREADS; both shapes compile.
check "vp9/h265/video streams: threads" "-DHAVE_RVP9 -DHAVE_RVP8 -DHAVE_RWEBM -DHAVE_RMP4 -DHAVE_RH264 -DHAVE_RH265" libretro-common/formats/vp9/rvp9.c libretro-common/formats/h265/rh265.c libretro-common/formats/webm/rwebm_video.c libretro-common/formats/mp4/rmp4_video.c libretro-common/formats/image/image_blit_bands.c
check_nothreads "no threads: vp9/h265/video streams" "-DHAVE_RVP9 -DHAVE_RVP8 -DHAVE_RWEBM -DHAVE_RMP4 -DHAVE_RH264 -DHAVE_RH265" libretro-common/formats/vp9/rvp9.c libretro-common/formats/h265/rh265.c libretro-common/formats/webm/rwebm_video.c libretro-common/formats/mp4/rmp4_video.c libretro-common/formats/image/image_blit_bands.c
check "gfx_surface: threads" "$GLINC" gfx/gfx_surface.c gfx/gfx_thumbnail.c
check_nothreads "no threads: gfx_surface" "$GLINC" gfx/gfx_surface.c gfx/gfx_thumbnail.c
check_nothreads "no threads: gl2"          "$GLDEFS $GLINC"       gfx/drivers/gl2.c
check_nothreads "no threads: video_driver" "$GLINC"               gfx/video_driver.c
check_nothreads "no threads: retroarch"    "$GLINC -DHAVE_COMMAND -DHAVE_STDIN_CMD" retroarch.c
check_nothreads "no threads: audio_driver" "$GLINC"               audio/audio_driver.c
check_nothreads "no threads: linux input"  "$GLINC"               input/common/linux_common.c
check_nothreads "no threads: widget state lock stand-ins" \
   "$GLINC -DHAVE_GFX_WIDGETS" \
   gfx/gfx_widgets.c gfx/widgets/gfx_widget_volume.c gfx/video_driver.c runloop.c

# The networking files carry threaded machinery of their own - the HTTP
# DNS cache's lock and condition, the task queue's - beside code that is
# compiled either way, so a broadcast or a wait written outside the
# guard is green in every job here and breaks a threadless build. That
# has happened: a DNS cache signal added to net_http_resolve(), which is
# not itself guarded, was caught by a sample rather than by this matrix.
NETDEFS="-DHAVE_NETWORKING $GLINC"
check_nothreads "no threads: net_http"   "$NETDEFS"                 libretro-common/net/net_http.c
check_nothreads "no threads: net_socket" "$NETDEFS"                 libretro-common/net/net_socket.c
check_nothreads "no threads: task_http"  "$NETDEFS"                 tasks/task_http.c
check_nothreads "no threads: cloud sync" "$NETDEFS -DHAVE_CLOUDSYNC" \
   tasks/task_cloudsync.c network/cloud_sync/webdav.c

# The menu and the on-screen widgets are switched separately, so the
# frontend translation units have to hold for all four combinations of
# the two. Every lane above builds with HAVE_MENU, and the shipping jobs
# all enable one or the other, so a declaration that reaches a caller
# only through menu_driver.h or gfx_widgets.h is green everywhere and
# absent from a build with both off.
echo
echo "== menu and widget gates =="
NOMENU=$(echo "$BASE" | sed 's/-DHAVE_MENU//')
check_gates() {
   name="$1"; shift
   defs="$1"; shift
   bad=0
   for tu in "$@"; do
      if ! out=$($CC $WARN $INC $NOMENU $defs -fsyntax-only "$tu" 2>&1); then
         echo "FAIL  $name"
         echo "      $tu"
         echo "$out" | sed 's/^/      /' | head -12
         fail=1; bad=1
      fi
   done
   [ "$bad" = 1 ] || echo "ok    $name"
}
UITU="retroarch.c runloop.c gfx/video_driver.c gfx/gfx_display.c"
UIDEFS="$GLINC -DHAVE_COMMAND -DHAVE_STDIN_CMD"
check_gates "gates: menu + widgets" "$UIDEFS -DHAVE_MENU -DHAVE_GFX_WIDGETS" $UITU
check_gates "gates: menu only"      "$UIDEFS -DHAVE_MENU"                    $UITU
check_gates "gates: widgets only"   "$UIDEFS -DHAVE_GFX_WIDGETS"             $UITU
check_gates "gates: neither"        "$UIDEFS"                                $UITU

# The optional subsystems switch independently of the menu and of each
# other, and the units that call into them are built either way. Each
# lane drops one subsystem from a fully featured build; the last drops
# all of them, which is what a minimal frontend compiles as.
ALLGATES="-DHAVE_MENU -DHAVE_GFX_WIDGETS -DHAVE_OVERLAY -DHAVE_CHEEVOS -DRC_CLIENT_SUPPORTS_HASH -DHAVE_NETWORKING -DHAVE_RUNAHEAD -DHAVE_DYNAMIC -DHAVE_DYLIB"
FETU="retroarch.c runloop.c command.c gfx/video_driver.c input/input_driver.c configuration.c tasks/task_content.c"
without() { echo "$ALLGATES" | sed "s/$1//g"; }
check_gates "gates: every subsystem" "$UIDEFS $ALLGATES"                            $FETU
check_gates "gates: no overlay"      "$UIDEFS $(without -DHAVE_OVERLAY)"            $FETU
check_gates "gates: no cheevos"      "$UIDEFS $(without '-DHAVE_CHEEVOS -DRC_CLIENT_SUPPORTS_HASH')" $FETU
check_gates "gates: no networking"   "$UIDEFS $(without -DHAVE_NETWORKING)"         $FETU
check_gates "gates: no run-ahead"    "$UIDEFS $(without -DHAVE_RUNAHEAD)"           $FETU
check_gates "gates: no subsystems"   "$UIDEFS"                                      $FETU

# A subsystem's own unit is built only when its gate is on, so each is
# checked with that gate on and the user interface off: the achievement
# and netplay widgets, and the menu entries either drives, are the edges
# where a declaration goes missing.
check_gates "gates: netplay, no UI" \
   "$UIDEFS -DHAVE_NETWORKING" network/netplay/netplay_frontend.c
check_gates "gates: cheevos, no UI" \
   "$UIDEFS -DHAVE_CHEEVOS -DRC_CLIENT_SUPPORTS_HASH" cheevos/cheevos.c
check_gates "gates: run-ahead, no UI" \
   "$UIDEFS -DHAVE_RUNAHEAD -DHAVE_DYNAMIC -DHAVE_DYLIB" runahead.c

# The builtin DSP filters are compiled only by console builds, through
# griffin, and no other job compiles them as C. Each is checked on its
# own, as the consoles build it, in the C89 lane: once with the C99 math
# names glibc declares under _GNU_SOURCE, and once without them, as on
# MSVC, so an undeclared sqrtf() or M_PI fails here.
echo
echo "== dsp filters: C89 =="
DSP=$(ls libretro-common/audio/dsp_filters/*.c)
check "dsp filters: C89" "$C89 -DHAVE_FILTERS_BUILTIN" $DSP
check "dsp filters: C89, no C99 math declarations" \
   "$(echo "$C89" | sed 's/ -D_GNU_SOURCE//') -DHAVE_FILTERS_BUILTIN" $DSP

echo "== run-ahead: the dynamic-library gates =="
# The secondary instance exists only with HAVE_DYNAMIC; a build that
# can load libraries but links its core statically (HAVE_DYLIB alone)
# compiles the secondary path out and keeps the callers linking.
RADEFS="-DHAVE_REWIND -DHAVE_RUNAHEAD -DHAVE_DYNAMIC_EXTENSIONS"
check "runahead: neither"          "$RADEFS"                            runahead.c
check "runahead: HAVE_DYLIB only"  "$RADEFS -DHAVE_DYLIB"               runahead.c
check "runahead: HAVE_DYNAMIC"     "$RADEFS -DHAVE_DYNAMIC"             runahead.c
check "runahead: both"             "$RADEFS -DHAVE_DYNAMIC -DHAVE_DYLIB" runahead.c

exit $fail
