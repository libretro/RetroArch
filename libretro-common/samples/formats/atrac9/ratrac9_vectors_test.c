/* ratrac9 against a reference decoder's output: generated streams covering every
 * channel layout, low and high sample rates and every superframe size, decoded to
 * 16-bit, 32-bit, float and planar 16-bit and compared by CRC-32 (of the samples as
 * little-endian bytes). Also: a reset and a savestate continue exactly. */

#include <stdio.h>
#include <string.h>

#include <formats/ratrac9.h>

struct at9_vector
{
   const char    *name;
   const uint8_t *data;
   size_t         size;
   unsigned       frames;
   unsigned       used;
   uint32_t       crc_s16, crc_s32, crc_f32, crc_planar;
};

#include "ratrac9_vectors.h"

#define MAX_PCM (8 * 256)

static int fails;

static void check(int ok, const char *name, const char *what)
{
   if (!ok)
   {
      printf("FAIL: %s: %s\n", name, what);
      fails++;
   }
}

/* CRC-32 (zlib's), bit by bit */
static uint32_t crc32_bytes(uint32_t crc, const uint8_t *p, size_t len)
{
   crc = ~crc;
   while (len--)
   {
      int k;
      crc ^= *p++;
      for (k = 0; k < 8; k++)
         crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1)));
   }
   return ~crc;
}

static uint32_t crc_le(uint32_t crc, const void *pcm, unsigned count,
      unsigned width)
{
   const uint8_t *p = (const uint8_t*)pcm;
   uint8_t        le[4];
   unsigned       i, k;
   for (i = 0; i < count; i++, p += width)
   {
      uint32_t v = 0;
      /* the value in host order, written out little-endian */
      if (width == 2)
      {
         uint16_t s;
         memcpy(&s, p, 2);
         v = s;
      }
      else
         memcpy(&v, p, 4);
      for (k = 0; k < width; k++)
         le[k] = (uint8_t)(v >> (8 * k));
      crc = crc32_bytes(crc, le, width);
   }
   return crc;
}

/* Decodes frames [from, to) of v with d in the given output form. */
static uint32_t run(ratrac9_t *d, const struct at9_vector *v, int form,
      unsigned from, unsigned to, uint32_t crc, unsigned *used_sum)
{
   static int16_t s16[MAX_PCM];
   static int32_t s32[MAX_PCM];
   static float   f32[MAX_PCM];
   unsigned fb  = ratrac9_frame_bytes(d);
   unsigned cnt = ratrac9_channels(d) * ratrac9_frame_samples(d);
   unsigned f;
   for (f = from; f < to; f++)
   {
      const uint8_t *in = v->data + 4 + (size_t)f * fb;
      size_t used       = 0;
      int    ret;
      switch (form)
      {
         case 1:
            ret = ratrac9_decode_s32(d, in, fb, s32, false, &used);
            crc = crc_le(crc, s32, cnt, 4);
            break;
         case 2:
            ret = ratrac9_decode_f32(d, in, fb, f32, false, &used);
            crc = crc_le(crc, f32, cnt, 4);
            break;
         case 3:
            ret = ratrac9_decode_s16(d, in, fb, s16, true, &used);
            crc = crc_le(crc, s16, cnt, 2);
            break;
         default:
            ret = ratrac9_decode_s16(d, in, fb, s16, false, &used);
            crc = crc_le(crc, s16, cnt, 2);
            break;
      }
      if (ret != 0)
         return 0;
      if (used_sum)
         *used_sum += (unsigned)used;
   }
   return crc;
}

int main(void)
{
   unsigned i;
   for (i = 0; i < sizeof(at9_vectors) / sizeof(at9_vectors[0]); i++)
   {
      const struct at9_vector *v = &at9_vectors[i];
      uint32_t  want[4];
      unsigned  half             = v->frames / 2;
      unsigned  used             = 0;
      int       form;
      ratrac9_t *d               = ratrac9_open(v->data);
      ratrac9_t *e;

      want[0] = v->crc_s16;
      want[1] = v->crc_s32;
      want[2] = v->crc_f32;
      want[3] = v->crc_planar;
      check(d != NULL, v->name, "open");
      if (!d)
         continue;
      check(4 + (size_t)ratrac9_frame_bytes(d) * v->frames == v->size,
            v->name, "frame size");

      for (form = 0; form < 4; form++)
      {
         ratrac9_reset(d);
         check(run(d, v, form, 0, v->frames, 0, form ? NULL : &used)
               == want[form], v->name,
               form == 0 ? "16-bit" : form == 1 ? "32-bit"
               : form == 2 ? "float" : "planar");
      }
      check(used == v->used, v->name, "bytes used");

      /* a reset mid-stream starts over */
      ratrac9_reset(d);
      run(d, v, 0, 0, half, 0, NULL);
      ratrac9_reset(d);
      check(run(d, v, 0, 0, v->frames, 0, NULL) == want[0], v->name,
            "after a reset");

      /* a savestate mid-stream, loaded into another decoder, goes on */
      e = ratrac9_open(v->data);
      if (e)
      {
         size_t   size = ratrac9_state_size(d);
         static uint8_t state[65536];
         uint32_t crc;
         ratrac9_reset(d);
         crc = run(d, v, 0, 0, half, 0, NULL);
         check(size <= sizeof(state), v->name, "state size");
         if (size <= sizeof(state))
         {
            ratrac9_save_state(d, state);
            check(ratrac9_load_state(e, state, size), v->name, "state load");
            check(!ratrac9_load_state(e, state, size - 1), v->name,
                  "state of the wrong size");
            check(run(e, v, 0, half, v->frames, crc, NULL) == want[0],
                  v->name, "continued from a savestate");
         }
         ratrac9_close(e);
      }
      ratrac9_close(d);
   }
   printf("ratrac9_vectors_test: %s (%d failures)\n", fails ? "FAIL" : "PASS",
         fails);
   return fails ? 1 : 0;
}
