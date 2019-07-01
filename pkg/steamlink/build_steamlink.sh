#!/bin/bash
#

TOP=$(cd `dirname "${BASH_SOURCE[0]}"` && pwd)
if [ "${MARVELL_SDK_PATH}" = "" ]; then
	MARVELL_SDK_PATH="$(cd "${TOP}/../.." && pwd)"
fi
if [ "${MARVELL_ROOTFS}" = "" ]; then
	source "${MARVELL_SDK_PATH}/setenv.sh" || exit 1
fi
BUILD="${PWD}"
SRC="${PWD}/retroarch-src"

# Extra environment variables for this build
export OS=Linux
export CC="${CROSS}gcc"
export CPP="${CROSS}cpp"
export CXX="${CROSS}g++"
export CFLAGS="--sysroot=$MARVELL_ROOTFS -DLINUX=1 -DEGL_API_FB=1"
export LDFLAGS="--sysroot=$MARVELL_ROOTFS -static-libgcc -static-libstdc++ -lEGL"
export INCLUDE_DIRS="-I$MARVELL_ROOTFS/usr/include -I$MARVELL_ROOTFS/usr/include/EGL -I$MARVELL_ROOTFS/usr/include/SDL2 -I${MARVELL_ROOTFS}/include/GLES2 -I$MARVELL_ROOTFS/usr/include/freetype2"
export LIBRARY_DIRS="-L$MARVELL_ROOTFS/usr/lib -L$MARVELL_ROOTFS/lib"
export PKG_CONF_PATH=pkg-config

#
# Download the source
#
if [ ! -d "${SRC}" ]; then
	git clone https://github.com/libretro/RetroArch.git "${SRC}"
fi

#
# Build it
#
pushd "${SRC}"
./configure --host=$SOC_BUILD --disable-threads --disable-alsa --disable-pulse --enable-neon --disable-shaderpipeline --enable-opengl --enable-opengles
make $MAKE_J || exit 2
popd

#
# Install it
#
export DESTDIR="${BUILD}/steamlink/apps/retroarch"

# Copy the files to the app directory
mkdir -p "${DESTDIR}"
mkdir -p "${DESTDIR}/content"
mkdir -p "${DESTDIR}/cores"
mkdir -p "${DESTDIR}/.home/.config/retroarch"

cp -v "${SRC}/retroarch" "${DESTDIR}"
$STRIP "${DESTDIR}/retroarch"
cp -v "${TOP}/retroarch-steamlink.cfg" "${DESTDIR}/.home/.config/retroarch/retroarch-steamlink.cfg"

# You can add some content for testing
if [ -d "content" ]; then
	cp -v -r content/* "${DESTDIR}/content"
fi
# Put the compiled cores to cores folder to add them
if [ -d "cores" ]; then
	cp -v -r cores/* "${DESTDIR}/cores"
fi

# Create the table of contents and icon
cat >"${DESTDIR}/toc.txt" <<__EOF__
name=Retroarch
icon=icon.png
run=retroarch
__EOF__

base64 -d >"${DESTDIR}/icon.png" <<__EOF__
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
__EOF__

# Pack it up
name=$(basename ${DESTDIR})
pushd "$(dirname ${DESTDIR})"
tar zcvf $name.tgz $name || exit 3
rm -rf $name
popd

# All done!
echo "Build completed!"
echo
echo "Download cores at RetroArch website and put it into Cores folder."
echo
echo "Put the steamlink folder onto a USB drive, insert it into your Steam Link, and cycle the power to install."