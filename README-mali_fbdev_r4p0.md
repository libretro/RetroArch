USAGE NOTES
===========

This driver is meant for devices with Allwinner SoCs with Mali400 3D block and a
good fbdev implementation. It is derived from the old Android GLES driver.

It was meant to be used on Cubieboard/Cubieboard2/Cubietruck, but it should not
be used on an Odroid X2/U2/U3 where a superior solution (RetroArch exynos video driver) is available.
Fbdev implementation on Odroid harware is missing WAITFORVSYNC ioctl, so use Exynos driver there.

This driver requires MALI r4p0 binary blobs for fbdev, and a kernel compatible with r4p0 binaries.

So we will use
-This kernel          : https://github.com/mireq/linux-sunxi
-This small patch     : https://gist.github.com/ssvb/8088519
-Files in this thread : http://forum.odroid.com/viewtopic.php?f=52&t=4956

First we will clone and build the kernel:
git clone https://github.com/mireq/linux-sunxi.git -b sunxi-3.4 --depth 1

Now we edit drivers/video/sunxi/disp/dev_fb.c, and uncomment the line 1074:
// Fb_wait_for_vsync(info);

It is assumed you have a cross-compiler installed, so we configure and build the kernel and modules:

make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- sun7i_defconfig

(This is for Cubieboard2, for other Sunxi boards look here: http://linux-sunxi.org/Linux_Kernel#Compilation)

make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- menuconfig

(Just in case we want to customize kernel options. It is OK to build with default config)

make -j4 ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- uImage modules
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- INSTALL_MOD_PATH=<path_to_rootfs_mountpoint> modules_install

cp arch/arm/boot/uImage /<path_to_sd_rootfs_mountpoint>/boot/

Now we should download and extract the EGL/GLES/GLES2 MALI FBDEV blobs from this thread:

http://forum.odroid.com/viewtopic.php?f=52&t=4956

This is the exact link:

http://builder.mdrjr.net/tools/r4p0-mp400-fbdev.tar

Copy these libraries to /usr/lib.

Now we need the headers. We can get them from here:

http://malideveloper.arm.com/develop-for-mali/sdks/opengl-es-sdk-for-linux/#opengl-es-sdk-for-linux-download

Download whatever version you want. We just get the headers from here, not machine-dependant compiled code.

Extract the files and copy the directories inside inc to /usr/include .

Also, copy simple_framework/inc/mali/EGL/fbdev_window.h to /usr/include/EGL .

In the end you should have this on your system:

   /usr/include/EGL/
      eglext.h
      egl.h
      eglplatform.h
      fbdev_window.h
   /usr/include/GLES/
      glext.h
      gl.h
      glplatform.h
   /usr/include/GLES2
      gl2ext.h
      gl2.h
      gl2platform.h
   /usr/include/GLES3
      gl3ext.h
      gl3.h
      gl3platform.h

To enable mali_fbdev you must configure RetroArch with --enable-gles and --enable-mali_fbdev.

This is an example of what you would use on a CubieBoard2 for a lightweight RetroArch:

./configure --enable-gles --enable-mali_fbdev --disable-x11 --disable-sdl2 --enable-floathard --disable-ffmpeg --disable-netplay --enable-udev --disable-sdl --disable-pulse --disable-oss --disable-freetype --disable-7zip

NOTE: A TTY hack is used to auto-clean the console on exit, and the fbdev ioctls are used to retrieve
current video mode. Both things work good, but they are not exactly ideal solutions.

If you come up with something better, feel free to improve the driver.
