/* raac_last_size: the bytes one access unit takes, for callers handed
 * buffers that hold more than one. */

#include <stdio.h>
#include <string.h>

#include <formats/raac.h>

typedef struct
{
   uint8_t *buf;
   size_t   pos;
} bw_t;

static void put(bw_t *w, unsigned v, int n)
{
   while (n--)
   {
      if ((v >> n) & 1)
         w->buf[w->pos >> 3] |= (uint8_t)(0x80 >> (w->pos & 7));
      w->pos++;
   }
}

/* A silent mono frame: an SCE with no bands, then END. With @fil, a
 * fill element of @fil bytes before the END. */
static size_t silent_frame(uint8_t *buf, unsigned fil)
{
   bw_t w;
   unsigned i;
   w.buf = buf;
   w.pos = 0;
   put(&w, 0, 3);    /* SCE */
   put(&w, 0, 4);    /* tag */
   put(&w, 100, 8);  /* global_gain */
   put(&w, 0, 1);    /* ics_reserved_bit */
   put(&w, 0, 2);    /* ONLY_LONG */
   put(&w, 0, 1);    /* window_shape */
   put(&w, 0, 6);    /* max_sfb */
   put(&w, 0, 1);    /* predictor_data_present */
   put(&w, 0, 3);    /* pulse, tns, gain control */
   if (fil)
   {
      put(&w, 6, 3); /* FIL */
      put(&w, fil, 4);
      for (i = 0; i < fil; i++)
         put(&w, 0xA5, 8);
   }
   put(&w, 7, 3);    /* END */
   return (w.pos + 7) >> 3;
}

int main(void)
{
   static const uint8_t asc[2] = { 0x12, 0x08 }; /* LC, 44.1 kHz, mono */
   static int16_t pcm[RAAC_MAX_FRAME * 8];
   uint8_t  buf[256];
   size_t   a, b, i;
   int      fails = 0;
   raac_t  *dec = raac_open(asc, sizeof(asc));

   if (!dec)
   {
      printf("FAIL: open\n");
      return 1;
   }

   memset(buf, 0, sizeof(buf));
   a = silent_frame(buf, 0);
   b = silent_frame(buf + a, 9);
   memset(buf + a + b, 0x5A, 32);

   /* Each unit alone, then with what follows it. */
   if (raac_decode_s16(dec, buf, a, pcm) != 1024 || raac_last_size(dec) != a)
   {
      printf("FAIL: lone unit took %u of %u\n", (unsigned)raac_last_size(dec), (unsigned)a);
      fails++;
   }
   if (raac_decode_s16(dec, buf, sizeof(buf), pcm) != 1024 || raac_last_size(dec) != a)
   {
      printf("FAIL: first unit took %u, want %u\n", (unsigned)raac_last_size(dec), (unsigned)a);
      fails++;
   }
   if (raac_decode_s16(dec, buf + a, sizeof(buf) - a, pcm) != 1024 || raac_last_size(dec) != b)
   {
      printf("FAIL: second unit took %u, want %u\n", (unsigned)raac_last_size(dec), (unsigned)b);
      fails++;
   }
   for (i = 0; i < 1024; i++)
      if (pcm[i])
      {
         printf("FAIL: silent frame not silent at %u\n", (unsigned)i);
         fails++;
         break;
      }

   /* Cut short: no END, so a failure, and nothing taken. */
   if (raac_decode_s16(dec, buf + a, b - 1, pcm) >= 0 || raac_last_size(dec) != 0)
   {
      printf("FAIL: truncated unit decoded or took %u\n", (unsigned)raac_last_size(dec));
      fails++;
   }

   raac_close(dec);
   if (fails)
      return 1;
   printf("raac_size_test: OK (%u and %u byte units)\n", (unsigned)a, (unsigned)b);
   return 0;
}
