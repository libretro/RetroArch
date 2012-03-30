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

typedef struct xaudio2 xaudio2_t;

xaudio2_t* xaudio2_new(unsigned samplerate, unsigned channels, size_t bufsize, unsigned device);
size_t xaudio2_write_avail(xaudio2_t *handle);
size_t xaudio2_write(xaudio2_t *handle, const void *data, size_t bytes);
void xaudio2_free(xaudio2_t *handle);
void xaudio2_enumerate_devices(xaudio2_t *handle);

#ifdef __cplusplus
}
#endif

#endif
