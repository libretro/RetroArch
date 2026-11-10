/* raac_adts_parse and raac_set_channel_order: ADTS headers read into
 * the AudioSpecificConfig raac_open takes, and the layouts that have a
 * WAVE channel order and those that don't. */

#include <stdio.h>
#include <string.h>

#include <formats/raac.h>

static int fails;

#define CHECK(cond, what) \
   do { if (!(cond)) { printf("FAIL: %s\n", what); fails++; } } while (0)

typedef struct
{
   uint8_t buf[32];
   size_t  pos;
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

/* An ADTS header: LC at 48 kHz, chcfg channels, frame of len bytes,
 * CRC present when crc. */
static void adts_header(uint8_t *h, unsigned chcfg, unsigned len, int crc)
{
   h[0] = 0xFF;
   h[1] = (uint8_t)(0xF0 | (crc ? 0 : 1));
   h[2] = (uint8_t)((1u << 6) | (3u << 2) | (chcfg >> 2));
   h[3] = (uint8_t)(((chcfg & 3) << 6) | ((len >> 11) & 3));
   h[4] = (uint8_t)(len >> 3);
   h[5] = (uint8_t)(((len & 7) << 5) | 0x1F);
   h[6] = 0xFC;
}

/* An AudioSpecificConfig, LC at 48 kHz, with a PCE carrying @nfront
 * front elements (SCE unless the bit in @front_cpe is set) and one
 * LFE when @lfe. */
static size_t pce_asc(uint8_t *out, unsigned nfront, unsigned front_cpe, int lfe)
{
   bw_t     w;
   unsigned i;
   memset(&w, 0, sizeof(w));
   put(&w, 2, 5);        /* AAC LC */
   put(&w, 3, 4);        /* 48 kHz */
   put(&w, 0, 4);        /* channelConfiguration 0: PCE follows */
   put(&w, 0, 3);        /* frameLength, dependsOnCore, extension */
   put(&w, 0, 4);        /* element_instance_tag */
   put(&w, 1, 2);        /* object_type */
   put(&w, 3, 4);        /* sampling_frequency_index */
   put(&w, nfront, 4);
   put(&w, 0, 4);        /* side */
   put(&w, 0, 4);        /* back */
   put(&w, lfe ? 1 : 0, 2);
   put(&w, 0, 3);        /* assoc data */
   put(&w, 0, 4);        /* cc */
   put(&w, 0, 3);        /* no mixdowns */
   for (i = 0; i < nfront; i++)
   {
      put(&w, (front_cpe >> i) & 1, 1);
      put(&w, i, 4);
   }
   if (lfe)
      put(&w, 0, 4);
   w.pos = (w.pos + 7) & ~(size_t)7;
   put(&w, 0, 8);        /* comment_field_bytes */
   memcpy(out, w.buf, (w.pos + 7) >> 3);
   return (w.pos + 7) >> 3;
}

int main(void)
{
   uint8_t     h[16];
   uint8_t     asc[32];
   size_t      n;
   raac_adts_t a;
   raac_t     *dec;
   unsigned    cfg;

   /* headers */
   adts_header(h, 2, 300, 0);
   CHECK(raac_adts_parse(h, 7, &a) == 0, "adts parse");
   CHECK(a.frame_size == 300 && a.header_size == 7 && a.raw_blocks == 1,
         "adts fields");
   CHECK(a.asc[0] == 0x11 && a.asc[1] == 0x90, "adts asc (LC 48k stereo)");
   adts_header(h, 6, 1000, 1);
   CHECK(raac_adts_parse(h, 9, &a) == 0 && a.header_size == 9
         && a.frame_size == 1000, "adts with CRC");
   CHECK(raac_adts_parse(h, 6, &a) < 0, "adts too short");
   h[0] = 0xFE;
   CHECK(raac_adts_parse(h, 9, &a) < 0, "adts without sync");
   adts_header(h, 2, 7, 0);
   CHECK(raac_adts_parse(h, 7, &a) < 0, "adts frame no longer than header");

   /* every standard configuration has a WAVE order */
   for (cfg = 1; cfg <= 7; cfg++)
   {
      char what[64];
      adts_header(h, cfg, 300, 0);
      raac_adts_parse(h, 7, &a);
      dec = raac_open(a.asc, sizeof(a.asc));
      sprintf(what, "open channelConfiguration %u", cfg);
      CHECK(dec != NULL, what);
      sprintf(what, "WAVE order, channelConfiguration %u", cfg);
      CHECK(raac_set_channel_order(dec, RAAC_ORDER_WAVE) == 0, what);
      CHECK(raac_set_channel_order(dec, RAAC_ORDER_STREAM) == 0,
            "back to stream order");
      raac_close(dec);
   }

   /* a PCE of C, L/R, LFE maps; one with two front SCEs doesn't */
   n   = pce_asc(asc, 2, 2, 1);
   dec = raac_open(asc, n);
   CHECK(dec && raac_channels(dec) == 4, "open PCE C L R LFE");
   CHECK(raac_set_channel_order(dec, RAAC_ORDER_WAVE) == 0,
         "WAVE order for PCE C L R LFE");
   raac_close(dec);
   n   = pce_asc(asc, 2, 0, 0);
   dec = raac_open(asc, n);
   CHECK(dec && raac_channels(dec) == 2, "open PCE C C");
   CHECK(raac_set_channel_order(dec, RAAC_ORDER_WAVE) < 0,
         "no WAVE order for two front centres");
   raac_close(dec);

   printf("raac_layout_test: %s (%d failures)\n", fails ? "FAIL" : "PASS",
         fails);
   return fails ? 1 : 0;
}
