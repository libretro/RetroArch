/* Copyright (C) 2026 The RetroArch team. MIT license. */
#include "../../../formats/wma/rwma_internal.h"
#include "../../../formats/wma/rwma_mdct.h"
#include <formats/rwma.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct writer
{
   unsigned char *data;
   size_t bit;
} writer;

static void put(writer *w, unsigned n, unsigned v)
{
   while (n)
   {
      unsigned mask = 1u << (7 - (w->bit & 7));
      n--;
      w->data[w->bit / 8] &= (unsigned char)~mask;
      if ((v >> n) & 1)
         w->data[w->bit / 8] |= (unsigned char)mask;
      w->bit++;
   }
}

static void check(int condition, const char *name)
{
   if (!condition)
   {
      fprintf(stderr, "FAIL: %s\n", name);
      exit(1);
   }
}

static void put_span(writer *w, const unsigned char *p, size_t at, size_t count)
{
   while (count--)
   {
      put(w, 1, (p[at / 8] >> (7 - (at & 7))) & 1);
      at++;
   }
}

/* A uniform, single-channel, uncoded frame has an all-zero spectrum. */
static void silent_frame(writer *w, unsigned width, unsigned bits, unsigned more,
                         int raw)
{
   size_t start = w->bit;
   put(w, width, bits);
   if (raw)
   {
      put(w, 1, 1);
      put(w, 1, 0);
      put(w, 8, 0); /* Raw-XMA gain field. */
   }
   put(w, 1, 0); /* No trim. */
   put(w, 1, 0); /* No fill. */
   put(w, 1, 0); /* No extended transform. */
   put(w, 1, 0); /* No coded channel. */
   while (w->bit < start + bits - 1)
      put(w, 1, 0);
   put(w, 1, more);
}

static void zero_frame(const rwma_frame *f, unsigned length)
{
   unsigned i;
   check(f->channels == 1 && f->samples == length, "frame shape");
   for (i = 0; i < length; i++)
      check(f->planes[0][i] == 0.0f, "silent spectrum");
}

static void raw_test(void)
{
   rwma_config cfg;
   rwma_t *d = NULL;
   rwma_frame f;
   unsigned char data[16];
   writer w;
   unsigned offset;
   memset(&cfg, 0, sizeof(cfg));
   cfg.codec = RWMA_XMA_RAW;
   cfg.sample_rate = 48000;
   cfg.channels = 1;
   check(rwma_create(&d, &cfg) == RWMA_OK, "raw create");
   for (offset = 0; offset < 8; offset++)
   {
      memset(data, 0, sizeof(data));
      w.data = data;
      w.bit = offset;
      silent_frame(&w, 15, 40, 0, 1);
      check(rwma_decode_xma(d, data, sizeof(data), offset, 40, &f) == 0,
            "unaligned raw frame");
      zero_frame(&f, 512);
   }
   check(!rwma_drain_xma(d, &f), "raw overlap drain");
   zero_frame(&f, 256);
   check(!rwma_drain_xma(d, &f) && !f.samples, "drain once");
   check(rwma_decode_xma(d,data,sizeof(data),7,40,&f)==RWMA_BAD_DATA,
         "reset required after drain");
   rwma_reset(d);
   check(!rwma_decode_xma(d,data,sizeof(data),7,40,&f), "decode after drain reset");
   check(rwma_decode_xma(d, data, 1, 0, 40, &f) == RWMA_BAD_DATA, "truncated raw span");
   check(!f.samples && !f.planes[0], "clear failed raw view");
   rwma_reset(d);
   rwma_free(d);
}

static void packet_test(void)
{
   rwma_config cfg;
   rwma_t *d = NULL;
   rwma_frame f;
   unsigned char extra[18], first[64], second[64], frame[64];
   writer w;
   size_t used;
   memset(&cfg, 0, sizeof(cfg));
   memset(extra, 0, sizeof(extra));
   memset(first, 0, sizeof(first));
   memset(second, 0, sizeof(second));
   memset(frame, 0, sizeof(frame));
   extra[0] = 16;
   extra[14] = 64;
   cfg.codec = RWMA_PRO;
   cfg.sample_rate = 48000;
   cfg.channels = 1;
   cfg.block_align = sizeof(first);
   cfg.extra = extra;
   cfg.extra_size = sizeof(extra);
   check(rwma_create(&d, &cfg) == RWMA_OK, "pro create");
   w.data = frame;
   w.bit = 0;
   silent_frame(&w, 10, 500, 0, 0);
   w.data = first;
   w.bit = 0;
   put(&w, 6, 0);
   put(&w, 10, 0);
   silent_frame(&w, 10, 20, 1, 0); /* Priming frame. */
   put_span(&w, frame, 0, 476);
   w.data = second;
   w.bit = 0;
   put(&w, 6, 4);
   put(&w, 10, 24);
   put_span(&w, frame, 476, 24);
   silent_frame(&w, 10, 20, 0, 0);
   check(rwma_decode_packet(d, first, sizeof(first), &used, &f) == 0,
         "partial packet frame");
   check(used == sizeof(first) && !f.samples, "retain partial frame");
   check(rwma_decode_packet(d, second, sizeof(second), &used, &f) == 0,
         "continued frame");
   zero_frame(&f, 2048);
   check(!used, "continued trailer preserves current packet");
   check(rwma_decode_packet(d, second, sizeof(second), &used, &f) == 0,
         "frame after continuation");
   zero_frame(&f, 2048);
   check(used == sizeof(second), "packet released");
   check(rwma_decode_packet(d, NULL, 0, &used, &f) == 0 && !f.samples, "empty drain");
   rwma_free(d);
}

static void transform_test(void)
{
   rwma_mdct m;
   unsigned n, i, j;
   float *storage = (float *)calloc(11 * 8192, sizeof(float));
   float *input, *output, *alias;
   check(storage != NULL, "transform storage");
   input = storage + 8 * 8192;
   output = input + 8192;
   alias = output + 8192;
   rwma_mdct_init(&m, storage, 8192);
   for (n = 64; n <= 8192; n *= 2)
   {
      for (i = 0; i < n; i++)
         input[i] = (float)(sin(i * 0.71) * 0.2);
      rwma_mdct_run(&m, output, input, n, 0.01f);
      memcpy(alias, input, n * sizeof(float));
      rwma_mdct_run(&m, alias, alias, n, 0.01f);
      check(!memcmp(alias, output, n * sizeof(float)), "in-place transform");
      for (i = 0; i < n; i += n / 8)
      {
         double expected = 0.0;
         for (j = 0; j < n; j++)
            expected += input[j] * cos(RWMA_PI / n * (j + 0.5) * (n - 1 - i + 0.5));
         expected *= 0.01;
         check(fabs(output[i] - expected) < 0.00003, "direct cosine reference");
      }
   }
   free(storage);
}

static void damaged_test(void)
{
   rwma_config cfg;
   rwma_t *d = NULL;
   rwma_frame f;
   uint8_t data[256];
   uint32_t random = 17;
   unsigned trial, i;
   memset(&cfg, 0, sizeof(cfg));
   cfg.codec = RWMA_XMA_RAW;
   cfg.channels = 2;
   cfg.sample_rate = 48000;
   check(!rwma_create(&d, &cfg), "damaged test create");
   for (trial = 0; trial < 4000; trial++)
   {
      unsigned bits = 32 + trial % (sizeof(data) * 8 - 32);
      int status;
      for (i = 0; i < sizeof(data); i++)
      {
         random = random * 1664525u + 1013904223u;
         data[i] = (uint8_t)(random >> 24);
      }
      /* Keep the declared extent valid to exercise the spectral walk. */
      data[0] = (uint8_t)(bits >> 7);
      data[1] = (uint8_t)((data[1] & 1) | ((bits & 127) << 1));
      rwma_reset(d);
      status = rwma_decode_xma(d, data, sizeof(data), 0, bits, &f);
      if (status)
         check(!f.samples && !f.planes[0], "damaged frame clears view");
      else
         check(f.samples == 512 && f.channels == 2, "damaged frame shape");
   }
   rwma_free(d);
}

static void standard_test(void)
{
   rwma_config cfg;
   rwma_t *d = NULL;
   rwma_frame f;
   unsigned char extra[10], packet[32];
   size_t used;
   writer w;
   unsigned version;
   memset(&cfg, 0, sizeof(cfg));
   memset(extra, 0, sizeof(extra));
   memset(packet, 0, sizeof(packet));
   cfg.extra = extra;
   cfg.extra_size = sizeof(extra);
   cfg.channels = 1;
   cfg.bit_rate = 128000;
   cfg.block_align = sizeof(packet);
   for (version = RWMA_V1; version <= RWMA_V2; version++)
   {
      unsigned n = version == RWMA_V1 ? 1024 : 2048;
      cfg.codec = version;
      cfg.sample_rate = version == RWMA_V1 ? 22050 : 48000;
      check(!rwma_create(&d, &cfg), "standard create");
      check(!rwma_decode_packet(d, packet, sizeof(packet), &used, &f),
            "uncoded standard frame");
      zero_frame(&f, n);
      check(used == sizeof(packet), "standard packet released");
      check(!rwma_decode_packet(d, NULL, 0, &used, &f) && !f.samples,
            "standard drain adds no invented tail");
      rwma_free(d);
   }
   cfg.codec = RWMA_V2;
   cfg.sample_rate = 48000;
   extra[4] = 5;
   w.data = packet;
   w.bit = 0;
   put(&w, 2, 0);
   put(&w, 2, 1);
   put(&w, 2, 1);
   put(&w, 1, 0);
   put(&w, 2, 0);
   put(&w, 1, 0);
   check(!rwma_create(&d, &cfg), "variable block create");
   check(!rwma_decode_packet(d, packet, sizeof(packet), &used, &f),
         "two variable blocks");
   zero_frame(&f, 2048);
   rwma_free(d);
   extra[4] = 3;
   memset(packet, 0, sizeof(packet));
   w.bit = 0;
   put(&w, 4, 0);
   put(&w, 4, 2);
   put(&w, 14, 0);
   put(&w, 1, 0);
   check(!rwma_create(&d, &cfg), "reservoir create");
   check(!rwma_decode_packet(d, packet, sizeof(packet), &used, &f), "reservoir packet");
   zero_frame(&f, 2048);
   check(used == sizeof(packet), "reservoir released");
   rwma_free(d);
}

static void wave_test(void)
{
   uint8_t bytes[28];
   rwma_config cfg;
   rwma_t *d = NULL;
   memset(bytes, 0, sizeof(bytes));
   bytes[0] = 0x61;
   bytes[1] = 1;
   bytes[2] = 2;
   bytes[4] = 0x80;
   bytes[5] = 0xbb;
   bytes[8] = 0x80;
   bytes[9] = 0x3e;
   bytes[12] = 32;
   bytes[16] = 10;
   bytes[22] = 1;
   check(!rwma_parse_wave(&cfg, bytes, sizeof(bytes)), "WAVEFORMATEX parse");
   check(cfg.codec == RWMA_V2 && cfg.sample_rate == 48000 &&
         cfg.channels == 2 && cfg.bit_rate == 128000 && cfg.extra == bytes + 18 &&
         cfg.extra_size == 10, "borrowed WAVEFORMATEX setup");
   check(!rwma_create(&d, &cfg), "parsed format create");
   rwma_free(d);
   check(rwma_parse_wave(&cfg, bytes, 17) == RWMA_BAD_DATA && !cfg.codec,
         "short WAVEFORMATEX");
   bytes[8] = bytes[9] = bytes[10] = bytes[11] = 0xff;
   check(rwma_parse_wave(&cfg, bytes, sizeof(bytes)) == RWMA_BAD_DATA &&
         !cfg.codec, "byte-rate overflow");
}

int main(void)
{
   raw_test();
   packet_test();
   transform_test();
   damaged_test();
   standard_test();
   wave_test();
   puts("PASS rwma raw, packet, transform and damaged-input tests");
   return 0;
}
