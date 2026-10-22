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
# Options: -d DRIVER (default vulkan), -f FRAMES (default 30), -k keep
# the captures in ./out/.  Exit status is the number of presets whose
# captures differ or failed, capped at 1.  No check target: the run
# needs a display and a driver.
set -u

driver=vulkan
frames=30
keep=0
while [ $# -gt 0 ]; do
   case $1 in
      -d) driver=$2; shift 2 ;;
      -f) frames=$2; shift 2 ;;
      -k) keep=1; shift ;;
      *) break ;;
   esac
done
if [ $# -lt 2 ]; then
   echo "usage: $0 [-d driver] [-f frames] [-k] <retroarch> <preset.slangp> ..." >&2
   exit 2
fi
ra=$1; shift
here=$(cd "$(dirname "$0")" && pwd)
core="$here/pattern_core.so"
out="$here/out"
[ -f "$core" ] || { echo "build pattern_core.so first (make)" >&2; exit 2; }
mkdir -p "$out"

capture() {
   # $1 preset, $2 compiler number, $3 output png, $4 log
   cfg="$out/ra_$2.cfg"
   cat > "$cfg" <<EOF
video_driver = "$driver"
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
   # every capture compiles afresh, so the log says which compiler ran
   rm -rf "$out/cache_$2"
   mkdir -p "$out/cache_$2"
   rm -f "$3"
   "$ra" --verbose --config "$cfg" -L "$core" --set-shader "$1" \
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
   if cmp -s "$g" "$r"; then
      echo "same  $name"
   else
      echo "DIFF  $name: $g $r"; bad=$((bad + 1))
   fi
done
echo "$n presets, $bad differ or failed"
[ "$keep" = 1 ] || rm -f "$out"/*.cfg
[ "$bad" = 0 ]
