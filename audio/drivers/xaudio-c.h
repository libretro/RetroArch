/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
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
