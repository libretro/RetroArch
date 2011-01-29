#ifndef __SSNES_FEATURES_H
#define __SSNES_FEATURES_H

#include <stddef.h>
#include <stdbool.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SDL
static const bool _sdl_supp = true;
#else
static const bool _sdl_supp = false;
#endif

#ifdef HAVE_ALSA
static const bool _alsa_supp = true;
#else
static const bool _alsa_supp = false;
#endif

#ifdef HAVE_OSS
static const bool _oss_supp = true;
#else
static const bool _oss_supp = false;
#endif

#ifdef HAVE_AL
static const bool _al_supp = true;
#else
static const bool _al_supp = false;
#endif

#ifdef HAVE_RSOUND
static const bool _rsound_supp = true;
#else
static const bool _rsound_supp = false;
#endif

#ifdef HAVE_ROAR
static const bool _roar_supp = true;
#else
static const bool _roar_supp = false;
#endif

#ifdef HAVE_JACK
static const bool _jack_supp = true;
#else
static const bool _jack_supp = false;
#endif

#ifdef HAVE_PULSE
static const bool _pulse_supp = true;
#else
static const bool _pulse_supp = false;
#endif

#ifdef HAVE_XAUDIO
static const bool _xaudio_supp = true;
#else
static const bool _xaudio_supp = false;
#endif

#ifdef HAVE_FILTER
static const bool _filter_supp = true;
#else
static const bool _filter_supp = false;
#endif

#ifdef HAVE_CG
static const bool _cg_supp = true;
#else
static const bool _cg_supp = false;
#endif

#ifdef HAVE_XML
static const bool _xml_supp = true;
#else
static const bool _xml_supp = false;
#endif

#ifdef HAVE_DYNAMIC
static const bool _dynamic_supp = true;
#else
static const bool _dynamic_supp = false;
#endif

#ifdef HAVE_FFMPEG
static const bool _ffmpeg_supp = true;
#else
static const bool _ffmpeg_supp = false;
#endif

#ifdef HAVE_SRC
static const bool _src_supp = true;
#else
static const bool _src_supp = false;
#endif

#ifdef HAVE_CONFIGFILE
static const bool _configfile_supp = true;
#else
static const bool _configfile_supp = false;
#endif

#ifdef HAVE_FREETYPE
static const bool _freetype_supp = true;
#else
static const bool _freetype_supp = false;
#endif

#endif
