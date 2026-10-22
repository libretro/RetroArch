#!/bin/sh
# Builds Monado with only what samples/openxr/e2e/run.py uses - the null
# compositor, the simulated headset and monado-service - and installs it
# into a prefix (Linux-openxr-e2e.yml).
#
# Usage: build-monado.sh <tag> <prefix> [<work dir>]
#
# The work dir, $RUNNER_TEMP/monado-build by default, takes the clone and
# the build tree and must not exist yet. The manifest,
# <prefix>/share/openxr/1/openxr_monado.json, names its library relative
# to itself, so a prefix restored from a cache works; nothing is made the
# system's active runtime.
set -eu

tag=${1:?usage: build-monado.sh <tag> <prefix> [<work dir>]}
prefix=${2:?usage: build-monado.sh <tag> <prefix> [<work dir>]}
work=${3:-${RUNNER_TEMP:-/tmp}/monado-build}

mkdir -p "$(dirname "$work")"
mkdir "$work"
git clone -q --depth 1 --branch "$tag" \
   https://gitlab.freedesktop.org/monado/monado.git "$work/src"

# Every driver but the simulated one, so a library that happens to be
# installed does not pull one in.
set --
for d in ANDROID ARDUINO BLUBUR_S1 DAYDREAM DEPTHAI EUROC HANDTRACKING HDK \
      HYDRA ILLIXR NS OHMD OPENGLOVES PSMV PSSENSE PSVR QWERTY REALSENSE \
      REMOTE RIFT RIFT_S ROKID SIMULAVR SOLARXR STEAMVR_LIGHTHOUSE SURVIVE \
      TWRAP ULV2 ULV5 VF VIVE WMR XREAL_AIR; do
   set -- "$@" "-DXRT_BUILD_DRIVER_$d=OFF"
done

# ON fails the configure when a dependency is missing; OFF keeps what
# the host has installed out.
cmake -S "$work/src" -B "$work/build" -G Ninja \
   -DCMAKE_BUILD_TYPE=RelWithDebInfo \
   -DCMAKE_INSTALL_PREFIX="$prefix" \
   -DBUILD_TESTING=OFF \
   -DXRT_BUILD_SAMPLES=OFF \
   -DXRT_MODULE_COMPOSITOR_MAIN=OFF \
   -DXRT_MODULE_COMPOSITOR_NULL=ON \
   -DXRT_FEATURE_SERVICE=ON \
   -DXRT_BUILD_DRIVER_SIMULATED=ON \
   -DXRT_HAVE_VULKAN=ON \
   -DXRT_FEATURE_STEAMVR_PLUGIN=OFF \
   -DXRT_MODULE_MONADO_CLI=OFF \
   -DXRT_MODULE_MONADO_GUI=OFF \
   -DXRT_MODULE_AUX_VIVE=OFF \
   -DXRT_MODULE_MERCURY_HANDTRACKING=OFF \
   -DXRT_FEATURE_SLAM=OFF \
   -DXRT_FEATURE_TRACING=OFF \
   -DXRT_HAVE_OPENGL=OFF \
   -DXRT_HAVE_OPENGL_GLX=OFF \
   -DXRT_HAVE_XLIB=OFF \
   -DXRT_HAVE_SDL2=OFF \
   -DXRT_HAVE_OPENCV=OFF \
   -DXRT_HAVE_GST=OFF \
   -DXRT_HAVE_SYSTEMD=OFF \
   -DXRT_HAVE_DBUS=OFF \
   -DXRT_HAVE_WAYLAND=OFF \
   -DXRT_HAVE_OPENGLES=OFF \
   -DXRT_HAVE_LIBUSB=OFF \
   -DXRT_HAVE_LIBUVC=OFF \
   -DXRT_HAVE_HIDAPI=OFF \
   -DXRT_HAVE_REALSENSE=OFF \
   -DXRT_HAVE_BLUETOOTH=OFF \
   -DXRT_HAVE_OPENVR=OFF \
   -DXRT_HAVE_ONNXRUNTIME=OFF \
   -DXRT_HAVE_PERCETTO=OFF \
   -DXRT_HAVE_JPEG=OFF \
   -DXRT_HAVE_LIBBSD=OFF \
   -DXRT_HAVE_SYSTEM_CJSON=OFF \
   "$@"
cmake --build "$work/build"
cmake --install "$work/build"
test -x "$prefix/bin/monado-service"
test -f "$prefix/share/openxr/1/openxr_monado.json"
