#!/bin/sh
# Image gate for the slang compilers: the same frame of the pattern
# core, through the same preset, captured from one RetroArch build
# with video_shader_compiler forced to glslang and then to rslc, must
# be the same picture.  Needs a RetroArch built with both compilers
# (HAVE_GLSLANG and HAVE_RSLC: ./configure --enable-glslang
# --enable-spirv_cross, with the system's glslang and SPIRV-Cross) and a
# Vulkan device; on a headless box lavapipe under Xvfb is enough:
#
#   Xvfb :99 -screen 0 1280x720x24 & export DISPLAY=:99
#   make                                     builds pattern_core.so
#   ./rslc_image_check.sh ../../../retroarch ../../../../slang-shaders/bilinear.slangp ...
#
# A .slang given in place of a .slangp is wrapped in a one-pass preset.
# Options: -d DRIVER (vulkan, the default, glcore, d3d10, d3d11 or
# d3d12), -f FRAMES (default 30, 300 with -D or -t), -k keep the captures in ./out/, -c
# capture the rslc run a second time on its warm SPIR-V cache (the
# words must pass rslc's ingest and the picture must not change), -w
# run a Windows RetroArch under Wine with pattern_core.dll (make
# CC=x86_64-w64-mingw32-gcc EXT=dll), which is how the D3D drivers run
# on a Linux box - on Windows itself, from an MSYS2 MINGW64 shell, the
# script runs retroarch.exe directly with pattern_core.dll (make
# EXT=dll) and Microsoft's d3dcompiler_47, which is the run of record
# for d3d11 and d3d12, -D apply the preset after the first frames instead
# of at start, which takes the deferred path (one pass per frame, the
# stock chain on screen until the new one is live), -t the same under
# threaded video, where the passes are built on the video thread, -b
# RETROARCH take the rslc runs from that build instead - one configured
# with rslc alone (--disable-glslang --disable-spirv_cross), so the
# glslang build's picture is the reference for it;
# both capture at frame 300 unless -f says otherwise, since the load
# starts 50 ms in and a frame is not a vblank on a headless box.  With glcore the glslang run crosses the SPIR-V and
# the rslc run links the GLSL rslc printed; GL_ARB_gl_spirv is kept
# out of the way so the printer, not the driver's SPIR-V path, is what
# is compared.  With D3D both runs go through D3DCompile, the glslang
# one on SPIRV-Cross's HLSL and the rslc one on rslc's.  A preset whose
# frame depends on the clock (two glslang runs differ as well) is
# reported as time-dependent, not as a difference.  Exit status is the
# number of presets whose captures differ or failed, capped at 1.
# No check target: the run needs a display and a driver.
set -u

driver=vulkan
frames=
keep=0
late=0
threaded=false
cached=0
runner=
ext=so
ra_rslc=
native_win=0
case $(uname -s) in
   MINGW*|MSYS*|CYGWIN*) native_win=1; ext=dll ;;
   Darwin) ext=dylib ;;
esac
# A path as the RetroArch being run reads it: a Windows one from an
# MSYS2 shell, the path itself on Linux and under Wine (whose Z: drive
# is /).
wp() {
   if [ $native_win = 1 ]; then cygpath -m "$1"; else printf '%s' "$1"; fi
}
while [ $# -gt 0 ]; do
   case $1 in
      -d) driver=$2; shift 2 ;;
      -f) frames=$2; shift 2 ;;
      -k) keep=1; shift ;;
      -c) cached=1; shift ;;
      -w) runner=wine; ext=dll; shift ;;
      -D) late=1; shift ;;
      -t) late=1; threaded=true; shift ;;
      -b) ra_rslc=$2; shift 2 ;;
      *) break ;;
   esac
done
if [ $# -lt 2 ]; then
   echo "usage: $0 [-d driver] [-f frames] [-k] [-c] [-w] [-D|-t] [-b retroarch] <retroarch> <preset.slangp> ..." >&2
   exit 2
fi
[ -n "$frames" ] || { [ $late = 1 ] && frames=300 || frames=30; }
ra=$1; shift
[ -n "$ra_rslc" ] || ra_rslc=$ra
case $driver in
   vulkan) ctx=vk_x ;;
   glcore) ctx=x; export RETROARCH_GL3_DISABLE_SPIRV=1 ;;
   *) ctx= ;;
esac
here=$(cd "$(dirname "$0")" && pwd)
core="$here/pattern_core.$ext"
out="$here/out"
[ -f "$core" ] || { echo "build $core first (make)" >&2; exit 2; }
# a RetroArch that is missing, or that Windows cannot start (a DLL not
# found beside it or on PATH), would fail every preset with no log
for b in "$ra" "$ra_rslc"; do
   $runner "$b" --version > /dev/null 2>&1 \
      || { echo "$b does not start (missing, or a DLL it needs is not found)" >&2; exit 2; }
done
mkdir -p "$out"

capture() {
   # $1 preset, $2 compiler number, $3 output png, $4 log, $5 "warm"
   # to keep the SPIR-V cache of the previous capture
   cfg="$out/ra_$2.cfg"
   cat > "$cfg" <<EOF
video_driver = "$driver"
video_context_driver = "$ctx"
video_shader_compiler = "$2"
video_shader_enable = "true"
video_gpu_screenshot = "true"
video_fullscreen = "false"
video_windowed_fullscreen = "false"
video_window_custom_size_enable = "true"
video_window_custom_size_x = "640"
video_window_custom_size_y = "480"
video_scale_integer = "false"
video_vsync = "false"
video_smooth = "false"
video_font_enable = "false"
video_notch_write_over_enable = "false"
audio_enable = "false"
menu_show_load_content_animation = "false"
video_msg_bgcolor_enable = "false"
notification_show_autoconfig = "false"
notification_show_screenshot = "false"
screenshot_directory = "$(wp "$out")"
cache_directory = "$(wp "$out/cache_$2")"
video_shader_dir = "$(wp "$(dirname "$1")")"
video_threaded = "$threaded"
video_shader_delay = "$([ $late = 1 ] && echo 50 || echo 0)"
video_shader_deferred_loading = "true"
config_save_on_exit = "false"
EOF
   # retroarch.exe sends --verbose to the console it attaches to, not to
   # a redirected stdout: on Windows the log goes to a file instead
   if [ $native_win = 1 ]; then
      rm -rf "$out/log_$2"
      mkdir -p "$out/log_$2"
      printf 'log_to_file = "true"\nlog_to_file_timestamp = "false"\nlog_dir = "%s"\n' \
         "$(wp "$out/log_$2")" >> "$cfg"
   fi
   # every capture compiles afresh, so the log says which compiler ran,
   # unless the warm cache is the thing under test
   if [ "${5:-}" != warm ]; then
      rm -rf "$out/cache_$2"
      mkdir -p "$out/cache_$2"
   fi
   rm -f "$3"
   bin=$ra
   [ "$2" = 2 ] && bin=$ra_rslc
   $runner "$bin" --verbose --config "$(wp "$cfg")" -L "$(wp "$core")" \
      --set-shader "$(wp "$1")" \
      --max-frames="$frames" --max-frames-ss --max-frames-ss-path="$(wp "$3")" \
      > "$4" 2>&1
   [ $native_win = 1 ] && cat "$out/log_$2/retroarch.log" >> "$4" 2>/dev/null
   [ -s "$3" ]
}

bad=0; n=0; vary=0
for preset in "$@"; do
   n=$((n + 1))
   case $preset in
      *.slang)
         name=$(basename "$preset" .slang)
         printf 'shaders = "1"\nshader0 = "%s"\n' "$(wp "$(cd "$(dirname "$preset")" && pwd)/$(basename "$preset")")" > "$out/$name.slangp"
         preset="$out/$name.slangp" ;;
      *) name=$(basename "$preset" .slangp) ;;
   esac
   g="$out/${name}_glslang.png"
   r="$out/${name}_rslc.png"
   lg="$out/${name}_glslang.log"
   lr="$out/${name}_rslc.log"
   if ! capture "$preset" 1 "$g" "$lg" || grep -q "Failed to create preset" "$lg"; then
      echo "FAIL  $name: no capture with glslang (see $lg)"; bad=$((bad + 1)); continue
   fi
   if ! capture "$preset" 2 "$r" "$lr"; then
      echo "FAIL  $name: no capture with rslc (see $lr)"; bad=$((bad + 1)); continue
   fi
   if ! grep -q "with rslc" "$lr" || grep -q "rslc rejected" "$lr"; then
      echo "FAIL  $name: the rslc run did not compile with rslc (see $lr)"; bad=$((bad + 1)); continue
   fi
   if [ "$driver" = glcore ] && ! grep -q "Linked the compiler's GLSL" "$lr"; then
      echo "FAIL  $name: the rslc run did not link rslc's GLSL (see $lr)"; bad=$((bad + 1)); continue
   fi
   case $driver in d3d1*)
      if ! grep -q "Using the compiler's HLSL" "$lr"; then
         echo "FAIL  $name: the rslc run did not compile rslc's HLSL (see $lr)"; bad=$((bad + 1)); continue
      fi ;;
   metal)
      if ! grep -q "Using the compiler's MSL" "$lr"; then
         echo "FAIL  $name: the rslc run did not compile rslc's MSL (see $lr)"; bad=$((bad + 1)); continue
      fi ;;
   esac
   if ! cmp -s "$g" "$r"; then
      # a preset that reads the clock (FrameTimeDelta, flicker, noise)
      # paints one of a few frames per run: glslang captured again, up
      # to three times, either lands on rslc's frame or on another of
      # its own, and either tells it apart from a compiler difference
      verdict=
      for k in 2 3 4; do
         g2="$out/${name}_glslang$k.png"
         capture "$preset" 1 "$g2" "$out/${name}_glslang$k.log" || break
         if cmp -s "$g2" "$r"; then
            verdict="same  $name (time-dependent: glslang run $k painted rslc's frame)"; break
         fi
         if ! cmp -s "$g" "$g2"; then
            verdict="vary  $name: differs between glslang runs as well (time-dependent)"; vary=$((vary + 1)); break
         fi
      done
      # and the other way round: rslc captured again may paint glslang's
      # frame, or another of its own
      [ -n "$verdict" ] || for k in 2 3 4; do
         r2="$out/${name}_rslc$k.png"
         capture "$preset" 2 "$r2" "$out/${name}_rslc$k.log" || break
         if cmp -s "$r2" "$g"; then
            verdict="same  $name (time-dependent: rslc run $k painted glslang's frame)"; break
         fi
         if ! cmp -s "$r" "$r2"; then
            verdict="vary  $name: differs between rslc runs as well (time-dependent)"; vary=$((vary + 1)); break
         fi
      done
      if [ -z "$verdict" ]; then
         echo "DIFF  $name: $g $r"; bad=$((bad + 1)); continue
      fi
      case $verdict in vary*) echo "$verdict"; continue ;; esac
      if [ "$cached" != 1 ]; then
         echo "$verdict"; continue
      fi
   fi
   if [ "$cached" = 1 ]; then
      c="$out/${name}_rslc_cached.png"
      lc="$out/${name}_rslc_cached.log"
      if ! capture "$preset" 2 "$c" "$lc" warm; then
         echo "FAIL  $name: no capture from the warm cache (see $lc)"; bad=$((bad + 1)); continue
      fi
      if ! grep -q "Loaded shader from cache" "$lc" || grep -q "Cached SPIR-V refused" "$lc"; then
         echo "FAIL  $name: the warm run did not serve the cache (see $lc)"; bad=$((bad + 1)); continue
      fi
      if ! cmp -s "$r" "$c"; then
         echo "DIFF  $name: cache $r $c"; bad=$((bad + 1)); continue
      fi
   fi
   echo "same  $name"
done
echo "$n presets, $bad differ or failed, $vary time-dependent"
[ "$keep" = 1 ] || rm -f "$out"/*.cfg
[ "$bad" = 0 ]
