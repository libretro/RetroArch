# libretro-mpv

mpv media player as a libretro core. A proof of concept release is now available.

Aims to use features already established in mpv that are not currently available in Retroarch movieplayer.

I want to be able to use Retroarch as my movie player on my embedded devices (Raspberry Pi) and desktop using hardware acceleration without having to use Kodi or mpv directly. Thus allowing for a more integrated experience, and smaller root filesystem.

## Compiling

Retroarch must be compiled with `--disable-ffmpeg` to stop the integrated movieplayer from playing the input file.

FFmpeg (preferably master branch) must be compiled with `--enable-shared`.

mpv must be compiled with `--enable-libmpv-shared`.

Then run `make` in the mpv-libretro folder.
