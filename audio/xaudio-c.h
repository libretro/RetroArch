/*
	Simple C interface for XAudio2
	Author: Hans-Kristian Arntzen
	License: Public Domain
*/

#ifndef XAUDIO_C_H
#define XAUDIO_C_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef XAUDIOC_EXPORTS
#define XAUDIOC_API __declspec(dllexport)
#else
#define XAUDIOC_API __declspec(dllimport)
#endif

typedef struct xaudio2 xaudio2_t;

XAUDIOC_API xaudio2_t* xaudio2_new(unsigned samplerate, unsigned channels, unsigned bits, size_t bufsize);
XAUDIOC_API size_t xaudio2_write_avail(xaudio2_t *handle);
XAUDIOC_API size_t xaudio2_write(xaudio2_t *handle, const void *data, size_t bytes);
XAUDIOC_API void xaudio2_free(xaudio2_t *handle);

typedef xaudio2_t* (*xaudio2_new_t)(unsigned, unsigned, unsigned, size_t);
typedef size_t (*xaudio2_write_avail_t)(xaudio2_t*);
typedef size_t (*xaudio2_write_t)(xaudio2_t*, const void*, size_t);
typedef void (*xaudio2_free_t)(xaudio2_t*);

#ifdef __cplusplus
}
#endif

#endif
