#!/bin/sh
# Image gate for the slang compilers: the same frame of the pattern
# core, through the same preset, captured from one RetroArch build
# with video_shader_compiler forced to glslang and then to rslc, must
# be the same picture.  Needs a RetroArch built with both compilers
# (HAVE_GLSLANG and HAVE_RSLC) and a Vulkan device; on a headless box
# lavapipe under Xvfb is enough:
#
#   Xvfb :99 -screen 0 1280x720x24 & export DISPLAY=:99
#   make                                     builds pattern_core.so
#   ./rslc_image_check.sh ../../../retroarch ../../../../slang-shaders/bilinear.slangp ...
#
# A .slang given in place of a .slangp is wrapped in a one-pass preset.
# Options: -d DRIVER (vulkan, the default, glcore, d3d10, d3d11 or
# d3d12), -f FRAMES (default 30), -k keep the captures in ./out/, -c
# capture the rslc run a second time on its warm SPIR-V cache (the
# words must pass rslc's ingest and the picture must not change), -w
# run a Windows RetroArch under Wine with pattern_core.dll (make
# CC=x86_64-w64-mingw32-gcc EXT=dll), which is how the D3D drivers run
# on a Linux box.  With glcore the glslang run crosses the SPIR-V and
# the rslc run links the GLSL rslc printed; GL_ARB_gl_spirv is kept
# out of the way so the printer, not the driver's SPIR-V path, is what
# is compared.  With D3D both runs go through D3DCompile, the glslang
# one on SPIRV-Cross's HLSL and the rslc one on rslc's.  Exit status is
# the number of presets whose captures differ or failed, capped at 1.
# No check target: the run needs a display and a driver.
set -u

driver=vulkan
frames=30
keep=0
cached=0
runner=
ext=so
while [ $# -gt 0 ]; do
   case $1 in
      -d) driver=$2; shift 2 ;;
      -f) frames=$2; shift 2 ;;
      -k) keep=1; shift ;;
      -c) cached=1; shift ;;
      -w) runner=wine; ext=dll; shift ;;
      *) break ;;
   esac
done
if [ $# -lt 2 ]; then
   echo "usage: $0 [-d driver] [-f frames] [-k] [-c] [-w] <retroarch> <preset.slangp> ..." >&2
   exit 2
fi
ra=$1; shift
case $driver in
   vulkan) ctx=vk_x ;;
   glcore) ctx=x; export RETROARCH_GL3_DISABLE_SPIRV=1 ;;
   *) ctx= ;;
esac
here=$(cd "$(dirname "$0")" && pwd)
core="$here/pattern_core.$ext"
out="$here/out"
[ -f "$core" ] || { echo "build $core first (make)" >&2; exit 2; }
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
screenshot_directory = "$out"
cache_directory = "$out/cache_$2"
video_shader_dir = "$(dirname "$1")"
config_save_on_exit = "false"
EOF
   # every capture compiles afresh, so the log says which compiler ran,
   # unless the warm cache is the thing under test
   if [ "${5:-}" != warm ]; then
      rm -rf "$out/cache_$2"
      mkdir -p "$out/cache_$2"
   fi
   rm -f "$3"
   $runner "$ra" --verbose --config "$cfg" -L "$core" --set-shader "$1" \
      --max-frames="$frames" --max-frames-ss --max-frames-ss-path="$3" \
      > "$4" 2>&1
   [ -s "$3" ]
}

bad=0; n=0
for preset in "$@"; do
   n=$((n + 1))
   case $preset in
      *.slang)
         name=$(basename "$preset" .slang)
         printf 'shaders = "1"\nshader0 = "%s"\n' "$(cd "$(dirname "$preset")" && pwd)/$(basename "$preset")" > "$out/$name.slangp"
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
   esac
   if ! cmp -s "$g" "$r"; then
      echo "DIFF  $name: $g $r"; bad=$((bad + 1)); continue
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
echo "$n presets, $bad differ or failed"
[ "$keep" = 1 ] || rm -f "$out"/*.cfg
[ "$bad" = 0 ]
