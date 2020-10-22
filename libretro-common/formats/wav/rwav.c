/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rwav.c).
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

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h> /* ptrdiff_t on osx */
#include <stdlib.h>
#include <string.h>

#include <formats/rwav.h>

enum
{
   ITER_BEGIN,
   ITER_COPY_SAMPLES,
   ITER_COPY_SAMPLES_8,
   ITER_COPY_SAMPLES_16
};

struct rwav_iterator
{
   rwav_t *out;
   const uint8_t *data;
   size_t size;
   size_t i, j;
   int step;
};

void rwav_init(rwav_iterator_t* iter, rwav_t* out, const void* buf, size_t size)
{
   iter->out    = out;
   iter->data   = (const uint8_t*)buf;
   iter->size   = size;
   iter->step   = ITER_BEGIN;

   out->samples = NULL;
}

enum rwav_state rwav_iterate(rwav_iterator_t *iter)
{
   size_t s;
   uint16_t *u16       = NULL;
   void *samples       = NULL;
   rwav_t *rwav        = iter->out;
   const uint8_t *data = iter->data;

   switch (iter->step)
   {
      case ITER_BEGIN:
         if (iter->size < 44)
            return RWAV_ITERATE_ERROR; /* buffer is smaller than an empty wave file */

         if (data[0] != 'R' || data[1] != 'I' || data[2] != 'F' || data[3] != 'F')
            return RWAV_ITERATE_ERROR;

         if (data[8] != 'W' || data[9] != 'A' || data[10] != 'V' || data[11] != 'E')
            return RWAV_ITERATE_ERROR;

         if (data[12] != 'f' || data[13] != 'm' || data[14] != 't' || data[15] != ' ')
            return RWAV_ITERATE_ERROR; /* we don't support non-PCM or compressed data */

         if (data[16] != 16 || data[17] != 0 || data[18] != 0 || data[19] != 0)
            return RWAV_ITERATE_ERROR;

         if (data[20] != 1 || data[21] != 0)
            return RWAV_ITERATE_ERROR; /* we don't support non-PCM or compressed data */

         if (data[36] != 'd' || data[37] != 'a' || data[38] != 't' || data[39] != 'a')
            return RWAV_ITERATE_ERROR;

         rwav->bitspersample = data[34] | data[35] << 8;

         if (rwav->bitspersample != 8 && rwav->bitspersample != 16)
            return RWAV_ITERATE_ERROR; /* we only support 8 and 16 bps */

         rwav->subchunk2size = data[40] | data[41] << 8 | data[42] << 16 | data[43] << 24;

         if ((rwav->subchunk2size < 1) ||
             (rwav->subchunk2size > iter->size - 44))
            return RWAV_ITERATE_ERROR; /* too few bytes in buffer */

         samples = malloc(rwav->subchunk2size);

         if (!samples)
            return RWAV_ITERATE_ERROR;

         rwav->numchannels = data[22] | data[23] << 8;
         rwav->numsamples  = rwav->subchunk2size * 8 / rwav->bitspersample / rwav->numchannels;
         rwav->samplerate  = data[24] | data[25] << 8 | data[26] << 16 | data[27] << 24;
         rwav->samples     = samples;

         iter->step = ITER_COPY_SAMPLES;
         return RWAV_ITERATE_MORE;

      case ITER_COPY_SAMPLES:
         iter->i = 0;

         if (rwav->bitspersample == 8)
         {
            iter->step = ITER_COPY_SAMPLES_8;

            /* TODO/FIXME - what is going on here? */
            case ITER_COPY_SAMPLES_8:
            s = rwav->subchunk2size - iter->i;

            if (s > RWAV_ITERATE_BUF_SIZE)
               s = RWAV_ITERATE_BUF_SIZE;

            memcpy((void*)((uint8_t*)rwav->samples + iter->i), (void *)(iter->data + 44 + iter->i), s);
            iter->i += s;
         }
         else
         {
            iter->step = ITER_COPY_SAMPLES_16;
            iter->j    = 0;

            /* TODO/FIXME - what is going on here? */
            case ITER_COPY_SAMPLES_16:
            s = rwav->subchunk2size - iter->i;

            if (s > RWAV_ITERATE_BUF_SIZE)
               s = RWAV_ITERATE_BUF_SIZE;

            u16 = (uint16_t *)rwav->samples;

            while (s != 0)
            {
               u16[iter->j++] = iter->data[44 + iter->i] | iter->data[45 + iter->i] << 8;
               iter->i += 2;
               s -= 2;
            }
         }

         if (iter->i < rwav->subchunk2size)
            return RWAV_ITERATE_MORE;
         return RWAV_ITERATE_DONE;
   }

   return RWAV_ITERATE_ERROR;
}

enum rwav_state rwav_load(rwav_t* out, const void* buf, size_t size)
{
   enum rwav_state res;
   rwav_iterator_t iter;

   iter.out             = NULL;
   iter.data            = NULL;
   iter.size            = 0;
   iter.i               = 0;
   iter.j               = 0;
   iter.step            = 0;

   rwav_init(&iter, out, buf, size);

   do
   {
      res = rwav_iterate(&iter);
   }while (res == RWAV_ITERATE_MORE);

   return res;
}

void rwav_free(rwav_t *rwav)
{
   free((void*)rwav->samples);
}
