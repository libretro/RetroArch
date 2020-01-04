#ifndef __RARCH_FEATURES_H
#define __RARCH_FEATURES_H

#include <stddef.h>
#include <boolean.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_OVERLAY
#define SUPPORTS_OVERLAY true
#else
#define SUPPORTS_OVERLAY false
#endif

#ifdef HAVE_V4L2
#define SUPPORTS_V4L2 true
#else
#define SUPPORTS_V4L2 false
#endif

#ifdef HAVE_COMMAND
#define SUPPORTS_COMMAND true
#else
#define SUPPORTS_COMMAND false
#endif

#ifdef HAVE_NETWORK_CMD
#define SUPPORTS_NETWORK_COMMAND true
#else
#define SUPPORTS_NETWORK_COMMAND false
#endif

#ifdef HAVE_NETWORKGAMEPAD
#define SUPPORTS_NETWORK_GAMEPAD true
#else
#define SUPPORTS_NETWORK_GAMEPAD false
#endif

#ifdef HAVE_FILTERS_BUILTIN
#define SUPPORTS_CPU_FILTERS true
#else
#define SUPPORTS_CPU_FILTERS false
#endif

#ifdef HAVE_LIBUSB
#define SUPPORTS_LIBUSB true
#else
#define SUPPORTS_LIBUSB false
#endif

#if defined(HAVE_SDL)
#define SUPPORTS_SDL true
#else
#define SUPPORTS_SDL false
#endif

#ifdef HAVE_SDL2
#define SUPPORTS_SDL2 true
#else
#define SUPPORTS_SDL2 false
#endif

#ifdef HAVE_THREADS
#define SUPPORTS_THREAD true
#else
#define SUPPORTS_THREAD false
#endif

#ifdef HAVE_OPENGL
#define SUPPORTS_OPENGL true
#else
#define SUPPORTS_OPENGL false
#endif

#ifdef HAVE_VULKAN
#define SUPPORTS_VULKAN true
#else
#define SUPPORTS_VULKAN false
#endif

#ifdef HAVE_METAL
#define SUPPORTS_METAL true
#else
#define SUPPORTS_METAL false
#endif

#if defined(HAVE_OPENGLES) || defined(HAVE_OPENGLES2) || defined(HAVE_OPENGLES3) || defined(HAVE_OPENGLES_3_1) || defined(HAVE_OPENGLES_3_2)
#define SUPPORTS_OPENGLES true
#else
#define SUPPORTS_OPENGLES false
#endif

#ifdef HAVE_KMS
#define SUPPORTS_KMS true
#else
#define SUPPORTS_KMS false
#endif

#ifdef HAVE_UDEV
#define SUPPORTS_UDEV true
#else
#define SUPPORTS_UDEV false
#endif

#ifdef HAVE_VG
#define SUPPORTS_VG true
#else
#define SUPPORTS_VG false
#endif

#ifdef HAVE_EGL
#define SUPPORTS_EGL true
#else
#define SUPPORTS_EGL false
#endif

#ifdef HAVE_X11
#define SUPPORTS_X11 true
#else
#define SUPPORTS_X11 false
#endif

#ifdef HAVE_WAYLAND
#define SUPPORTS_WAYLAND true
#else
#define SUPPORTS_WAYLAND false
#endif

#ifdef HAVE_XVIDEO
#define SUPPORTS_XVIDEO true
#else
#define SUPPORTS_XVIDEO false
#endif

#ifdef HAVE_ALSA
#define SUPPORTS_ALSA true
#else
#define SUPPORTS_ALSA false
#endif

#ifdef HAVE_TINYALSA
#define SUPPORTS_TINYALSA true
#else
#define SUPPORTS_TINYALSA false
#endif

#ifdef HAVE_COREAUDIO
#define SUPPORTS_COREAUDIO true
#else
#define SUPPORTS_COREAUDIO false
#endif

#ifdef HAVE_COREAUDIO3
#define SUPPORTS_COREAUDIO3 true
#else
#define SUPPORTS_COREAUDIO3 false
#endif

#if defined(HAVE_OSS) || defined(HAVE_OSS_BSD)
#define SUPPORTS_OSS true
#else
#define SUPPORTS_OSS false
#endif

#ifdef HAVE_AL
#define SUPPORTS_AL true
#else
#define SUPPORTS_AL false
#endif

#ifdef HAVE_SL
#define SUPPORTS_SL true
#else
#define SUPPORTS_SL false
#endif

#ifdef HAVE_LIBRETRODB
#define SUPPORTS_LIBRETRODB true
#else
#define SUPPORTS_LIBRETRODB false
#endif

#ifdef HAVE_RSOUND
#define SUPPORTS_RSOUND true
#else
#define SUPPORTS_RSOUND false
#endif

#ifdef HAVE_ROAR
#define SUPPORTS_ROAR true
#else
#define SUPPORTS_ROAR false
#endif

#ifdef HAVE_JACK
#define SUPPORTS_JACK true
#else
#define SUPPORTS_JACK false
#endif

#ifdef HAVE_PULSE
#define SUPPORTS_PULSE true
#else
#define SUPPORTS_PULSE false
#endif

#ifdef HAVE_DSOUND
#define SUPPORTS_DSOUND true
#else
#define SUPPORTS_DSOUND false
#endif

#ifdef HAVE_WASAPI
#define SUPPORTS_WASAPI true
#else
#define SUPPORTS_WASAPI false
#endif

#ifdef HAVE_XAUDIO
#define SUPPORTS_XAUDIO true
#else
#define SUPPORTS_XAUDIO false
#endif

#ifdef HAVE_ZLIB
#define SUPPORTS_ZLIB true
#else
#define SUPPORTS_ZLIB false
#endif

#ifdef HAVE_7ZIP
#define SUPPORTS_7ZIP true
#else
#define SUPPORTS_7ZIP false
#endif

#ifdef HAVE_DYLIB
#define SUPPORTS_DYLIB true
#else
#define SUPPORTS_DYLIB false
#endif

#ifdef HAVE_CG
#define SUPPORTS_CG true
#else
#define SUPPORTS_CG false
#endif

#ifdef HAVE_GLSL
#define SUPPORTS_GLSL true
#else
#define SUPPORTS_GLSL false
#endif

#ifdef HAVE_HLSL
#define SUPPORTS_HLSL true
#else
#define SUPPORTS_HLSL false
#endif

#ifdef HAVE_SDL_IMAGE
#define SUPPORTS_SDL_IMAGE true
#else
#define SUPPORTS_SDL_IMAGE false
#endif

#ifdef HAVE_DYNAMIC
#define SUPPORTS_DYNAMIC true
#else
#define SUPPORTS_DYNAMIC false
#endif

#ifdef HAVE_FFMPEG
#define SUPPORTS_FFMPEG true
#else
#define SUPPORTS_FFMPEG false
#endif

#ifdef HAVE_MPV
#define SUPPORTS_MPV true
#else
#define SUPPORTS_MPV false
#endif

#ifdef HAVE_FREETYPE
#define SUPPORTS_FREETYPE true
#else
#define SUPPORTS_FREETYPE false
#endif

#ifdef HAVE_STB_FONT
#define SUPPORTS_STBFONT true
#else
#define SUPPORTS_STBFONT false
#endif

#ifdef HAVE_NETWORKING
#define SUPPORTS_NETPLAY true
#else
#define SUPPORTS_NETPLAY false
#endif

#ifdef HAVE_PYTHON
#define SUPPORTS_PYTHON true
#else
#define SUPPORTS_PYTHON false
#endif

#if defined(HAVE_COCOA) || defined(HAVE_COCOATOUCH) || defined(HAVE_COCOA_METAL)
#define SUPPORTS_COCOA true
#else
#define SUPPORTS_COCOA false
#endif

#ifdef HAVE_QT
#define SUPPORTS_QT true
#else
#define SUPPORTS_QT false
#endif

#ifdef HAVE_RPNG
#define SUPPORTS_RPNG true
#else
#define SUPPORTS_RPNG false
#endif

#ifdef HAVE_RJPEG
#define SUPPORTS_RJPEG true
#else
#define SUPPORTS_RJPEG false
#endif

#ifdef HAVE_RBMP
#define SUPPORTS_RBMP true
#else
#define SUPPORTS_RBMP false
#endif

#ifdef HAVE_RTGA
#define SUPPORTS_RTGA true
#else
#define SUPPORTS_RTGA false
#endif

#ifdef HAVE_CORETEXT
#define SUPPORTS_CORETEXT true
#else
#define SUPPORTS_CORETEXT false
#endif

#if !defined(_WIN32) && !defined(GLOBAL_CONFIG_DIR)
#if defined(__HAIKU__)
#define GLOBAL_CONFIG_DIR "/system/settings"
#else
#define GLOBAL_CONFIG_DIR "/etc"
#endif
#endif

#endif
