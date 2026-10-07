#!/bin/sh
# Packaged data directory defaults (frontend/drivers/platform_unix.c).
#
# Distribution packages install the libretro data sets under
# <prefix>/share/libretro/<name> (FreeBSD ports, Debian).  A fresh
# RetroArch with no retroarch.cfg must default assets_directory,
# libretro_info_path, video_shader_dir and joypad_autoconfig_dir to
# those directories when they exist, and to the per-user directory
# when they do not.
#
# Runs the built ./retroarch headless (null drivers, a few frames of
# menu) with a throwaway HOME and config_save_on_exit, then reads the
# config it wrote.  Creating /usr/local/share/libretro needs root; the
# "present" lane is skipped when that is not possible and only the
# "absent" lane runs.  From the repo root:
#
#   samples/frontend/packaged_dirs/run.sh
set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
bin="$root/retroarch"
pkgdir=/usr/local/share/libretro

if [ ! -x "$bin" ]; then
   echo "skip: no built retroarch in $root (./configure && make first)"
   exit 0
fi

work=$(mktemp -d)
created=""
cleanup()
{
   for d in $created; do
      rmdir "$d" 2>/dev/null || :
   done
   rm -rf "$work"
}
trap cleanup EXIT

fail=0

# run_lane <name>: writes $work/<name>/retroarch.cfg
run_lane()
{
   lane=$1
   home="$work/$lane"
   mkdir -p "$home"
   cat > "$home/seed.cfg" <<CFG
video_driver = "null"
audio_driver = "null"
input_driver = "null"
input_joypad_driver = "null"
menu_driver = "rgui"
config_save_on_exit = "true"
CFG
   # --config points at a file that carries only driver overrides;
   # every directory is left to platform defaults and written back.
   HOME="$home" XDG_CONFIG_HOME="$home" \
      timeout 60 "$bin" --config="$home/seed.cfg" --menu --max-frames=5 \
      > "$home/out.log" 2>&1 || {
         echo "FAIL $lane: retroarch exited with $?" >&2
         cat "$home/out.log" >&2
         fail=1
         return 1
      }
}

# expect <lane> <key> <value>
expect()
{
   got=$(sed -n "s/^$2 = \"\(.*\)\"$/\1/p" "$work/$1/seed.cfg")
   if [ "$got" = "$3" ]; then
      echo "PASS $1: $2 = $3"
   else
      echo "FAIL $1: $2 = '$got', expected '$3'" >&2
      fail=1
   fi
}

# Lane 1: no packaged directories -> per-user defaults.
# (If a real package is installed on this box its directories exist
# and this lane cannot run; the "present" lane still covers them.)
absent_ok=1
for name in assets info shaders autoconfig; do
   [ -d "$pkgdir/$name" ] && absent_ok=0
done
if [ $absent_ok -eq 1 ]; then
   if run_lane absent; then
      # Paths under $HOME are written back home-relative.
      expect absent assets_directory        "~/retroarch/assets"
      expect absent libretro_info_path      "~/retroarch/cores"
      expect absent video_shader_dir        "~/retroarch/shaders"
      expect absent joypad_autoconfig_dir   "~/retroarch/autoconfig"
   fi
else
   echo "skip absent: $pkgdir already populated on this machine"
fi

# Lane 2: packaged directories present -> they win over per-user.
[ -d "$pkgdir" ] || created="$pkgdir"   # removed last, after the leaves
for name in assets info shaders autoconfig; do
   d="$pkgdir/$name"
   if [ ! -d "$d" ]; then
      if mkdir -p "$d" 2>/dev/null; then
         created="$d $created"
      else
         echo "skip present: cannot create $d (not root)"
         exit $fail
      fi
   fi
done

if run_lane present; then
   expect present assets_directory        "$pkgdir/assets"
   expect present libretro_info_path      "$pkgdir/info"
   expect present video_shader_dir        "$pkgdir/shaders"
   expect present joypad_autoconfig_dir   "$pkgdir/autoconfig"
fi

exit $fail
