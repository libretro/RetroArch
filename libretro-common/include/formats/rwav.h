/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rwav.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_FORMAT_RWAV_H__
#define __LIBRETRO_SDK_FORMAT_RWAV_H__

#include <retro_common_api.h>
#include <stdint.h>

RETRO_BEGIN_DECLS

typedef struct
{
   /* bits per sample */
   unsigned int bitspersample;

   /* number of channels */
   unsigned int numchannels;

   /* sample rate */
   unsigned int samplerate;

   /* number of *samples* */
   size_t numsamples;

   /* number of *bytes* in the pointer below, i.e. numsamples * numchannels * bitspersample/8 */
   size_t subchunk2size;

   /* PCM data */
   const void* samples;
} rwav_t;

enum rwav_state
{
   RWAV_ITERATE_ERROR    = -1,
   RWAV_ITERATE_MORE     = 0,
   RWAV_ITERATE_DONE     = 1,
   RWAV_ITERATE_BUF_SIZE = 4096
};

typedef struct rwav_iterator rwav_iterator_t;

/**
 * Initializes the iterator to fill the out structure with data parsed from buf.
 */
void rwav_init(rwav_iterator_t* iter, rwav_t* out, const void* buf, size_t size);

/**
 * Parses a piece of the data. Continue calling as long as it returns RWAV_ITERATE_MORE.
 * Stop calling otherwise, and check for errors. If RWAV_ITERATE_DONE is returned,
 * the rwav_t structure passed to rwav_init is ready to be used. The iterator does not
 * have to be freed.
 */
enum rwav_state rwav_iterate(rwav_iterator_t *iter);

/**
 * Loads the entire data in one go.
 */
enum rwav_state rwav_load(rwav_t* out, const void* buf, size_t size);

/**
 * Frees parsed wave data.
 */
void rwav_free(rwav_t *rwav);

RETRO_END_DECLS

#endif
