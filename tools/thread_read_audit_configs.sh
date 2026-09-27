#!/bin/bash
# Run tools/thread_read_audit.py across the build configurations one
# Linux machine can produce, so "zero findings" is a statement about
# every worker entry these builds contain rather than about whichever
# config was last built. Hand-run, like the audit itself:
#
#   bash tools/thread_read_audit_configs.sh linux
#   bash tools/thread_read_audit_configs.sh windows
#
# Each config reconfigures the tree, builds, audits, and prints the
# entry/finding counts plus --list-unaudited for what that binary
# still cannot see. Coverage as of this script's writing:
#
#   linux    51 entries (wifi, bluetooth, libusb HID, the ffmpeg
#            decode/record/camera workers and the Vulkan swapchain
#            mailbox included - packages below)
#   windows  52 entries (dinput, xinput, winraw, wasapi, mmdevice,
#            the modeline resync thread and the companion workers)
#
# Entries neither build contains - Android, emscripten, GX, PSP,
# sunxi/hub75, btstack, Steam - need their platforms' toolchains;
# --list-unaudited names them so nobody mistakes silence for
# coverage.
#
# Debian/Ubuntu packages for the full Linux config:
#   libusb-1.0-0-dev libavcodec-dev libavformat-dev libavutil-dev
#   libswscale-dev libswresample-dev libavdevice-dev libvulkan-dev
# For the Windows config: gcc-mingw-w64-x86-64 (host objdump reads
# the PE natively; no wine needed - the audit is static).

set -eu
cd "$(dirname "$0")/.."

audit()
{
   python3 tools/thread_read_audit.py --binary "$1" --list-unaudited
}

case "${1:-}" in
   linux)
      ./configure --enable-wifi --enable-bluetooth --enable-hid \
                  --enable-ffmpeg --enable-vulkan
      make -j"$(nproc)" retroarch
      audit retroarch
      ;;
   windows)
      # This mingw distribution has no xaudio2 import library; the
      # driver has no thread entry, so nothing hides behind the
      # disable.
      ./configure --host=x86_64-w64-mingw32 --disable-xaudio
      make -j"$(nproc)"
      audit retroarch.exe
      echo "note: the tree is now configured for Windows;" \
           "re-run ./configure for native work."
      ;;
   *)
      echo "usage: $0 linux|windows" >&2
      exit 2
      ;;
esac
