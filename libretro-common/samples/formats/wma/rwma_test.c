/* Copyright (C) 2026 The RetroArch team. MIT license. */
#include "../../../formats/wma/rwma_internal.h"
#include "../../../formats/wma/rwma_mdct.h"
#include <formats/rwma.h>
#include "../../../formats/wma/rwma_pro_data.h"
#include "../../../formats/wma/rwma_pro_prefix.h"
#include "../../../formats/wma/rwma_pro_gain.h"
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

static void long_packet_test(void)
{
   rwma_config cfg;
   rwma_t *d = NULL;
   rwma_frame f;
   unsigned char extra[18], packet[6][8], frame[32];
   writer w;
   size_t used;
   unsigned i, at = 34;
   memset(&cfg, 0, sizeof(cfg));
   memset(extra, 0, sizeof(extra));
   memset(packet, 0, sizeof(packet));
   memset(frame, 0, sizeof(frame));
   extra[0] = 16;
   extra[14] = 64 + (5 << 3);
   cfg.codec = RWMA_PRO;
   cfg.sample_rate = 48000;
   cfg.channels = 1;
   cfg.block_align = 8;
   cfg.extra = extra;
   cfg.extra_size = sizeof(extra);
   check(!rwma_create(&d, &cfg), "clamped frame context");
   w.data = frame;
   w.bit = 0;
   put(&w, 7, 64);
   put(&w, 1, 1);
   for (i = 0; i < 31; i++) put(&w, 3, 5);
   put(&w, 1, 0);
   for (i = 0; i < 32; i++) put(&w, 3, 0);
   put(&w, 2, 0);
   check(w.bit == 200, "procedural long-frame extent");
   w.data = packet[0]; w.bit = 0;
   put(&w, 6, 0); put(&w, 7, 0);
   put(&w, 7, 17); put(&w, 1, 1); put(&w, 3, 0);
   put(&w, 6, 1);
   put_span(&w, frame, 0, at);
   for (i = 1; i < 5; i++)
   {
      unsigned count = i == 4 ? 13 : 51;
      w.data = packet[i]; w.bit = 0;
      put(&w, 6, i << 2); put(&w, 7, 64);
      put_span(&w, frame, at, count); at += count;
   }
   w.data = packet[5]; w.bit = 0;
   put(&w, 6, 5 << 2); put(&w, 7, 0);
   put(&w, 7, 17); put(&w, 1, 1); put(&w, 3, 0); put(&w, 6, 0);
   for (i = 0; i < 4; i++)
   {
      check(!rwma_decode_packet(d, packet[i], 8, &used, &f),
            "clamped frame accumulates full continuation packets");
      check(used == 8 && !f.samples, "incomplete frame has no published PCM");
   }
   check(!rwma_decode_packet(d, packet[4], 8, &used, &f) && !used,
         "complete clamped frame after rollback retries");
   zero_frame(&f, 2048);
   check(!rwma_decode_packet(d, packet[4], 8, &used, &f) && used == 8 && !f.samples,
         "release whole continuation packet");
   check(!rwma_decode_packet(d, packet[5], 8, &used, &f) && used == 8,
         "frame after clamped continuation");
   zero_frame(&f, 2048);
   rwma_free(d);
}

static void long_packet_damage_test(void)
{
   rwma_config cfg;
   rwma_t *d = NULL;
   rwma_frame frame;
   unsigned char extra[18], packet[8];
   writer w;
   unsigned i;
   size_t used;
   int status = 0;
   memset(&cfg, 0, sizeof(cfg));
   memset(extra, 0, sizeof(extra));
   memset(packet, 0, sizeof(packet));
   extra[0] = 16;
   extra[14] = 64 + (5 << 3);
   cfg.codec = RWMA_PRO; cfg.sample_rate = 48000; cfg.channels = 1;
   cfg.block_align = 8; cfg.extra = extra; cfg.extra_size = sizeof(extra);
   check(!rwma_create(&d, &cfg), "bounded continuation context");
   w.data = packet; w.bit = 0;
   put(&w, 6, 0); put(&w, 7, 0); put(&w, 7, 64);
   put(&w, 1, 1); put(&w, 3, 0); put(&w, 1, 0);
   put(&w, 1, 1); put(&w, 2, 0); put(&w, 4, 15); put(&w, 15, 32766);
   check(!rwma_decode_packet(d, packet, 8, &used, &frame) && used == 8 && !frame.samples,
         "retain maliciously oversized frame prefix");
   for (i = 1; i <= 32 && !status; i++)
   {
      memset(packet, 0, sizeof(packet)); w.bit = 0;
      put(&w, 6, (i & 15) << 2); put(&w, 7, 64);
      status = rwma_decode_packet(d, packet, 8, &used, &frame);
      check(!frame.samples && !frame.planes[0], "oversized continuation publishes no PCM");
   }
   check(status == RWMA_BAD_DATA && i <= 22, "bounded carry rejects oversized frame");
   check(rwma_decode_packet(d,packet,8,&used,&frame)==RWMA_BAD_DATA,
         "oversized frame requires reset");
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

static void reject_checkpoint(rwma_t *d, const rwma_xma_view *bad)
{
   static float saved[2][256];
   rwma_xma_view before, after;
   unsigned c;
   check(!rwma_xma_capture(d, &before), "capture before rejected checkpoint");
   for (c = 0; c < before.state.channels; c++)
      if (before.history_samples[c])
         memcpy(saved[c], before.history[c], sizeof(saved[c]));
   check(rwma_xma_restore(d, bad) == RWMA_BAD_DATA, "reject invalid checkpoint");
   check(!rwma_xma_capture(d, &after) &&
             !memcmp(&before.state, &after.state, sizeof(before.state)),
         "rejected checkpoint preserves metadata");
   for (c = 0; c < before.state.channels; c++)
      check(before.history[c] == after.history[c] &&
                before.history_samples[c] == after.history_samples[c] &&
                (!before.history_samples[c] ||
                 !memcmp(saved[c], after.history[c], sizeof(saved[c]))),
            "rejected checkpoint preserves overlap");
}

static void checkpoint_test(void)
{
   static float history[2][256];
   rwma_config cfg;
   rwma_t *a = NULL, *b = NULL;
   rwma_xma_view cold, view, bad;
   rwma_frame first, second;
   unsigned char data[8];
   static const uint32_t bad_bits[4] = {0x7f800000u, 0xff800000u, 0x7fc00000u, 0x7f800001u};
   writer w;
   unsigned i, c;
   memset(&cfg, 0, sizeof(cfg));
   cfg.codec = RWMA_XMA_RAW;
   cfg.channels = 2;
   cfg.sample_rate = 48000;
   check(!rwma_create(&a, &cfg) && !rwma_create(&b, &cfg),
         "checkpoint decoder creation");
   check(!rwma_xma_capture(a, &cold) && !cold.state.pending && !cold.history[0],
         "cold checkpoint has no history payload");
   view = cold;
   view.state.pending = 1;
   for (c = 0; c < 2; c++)
   {
      for (i = 0; i < 256; i++)
         history[c][i] = (float)((int)i - 128 + c * 37) / 1024;
      view.state.previous[c] = c ? 256 : 128;
      view.history[c] = history[c];
      view.history_samples[c] = 256;
   }
   check(!rwma_xma_restore(a, &view), "restore nonzero overlap");
   check(!rwma_xma_capture(a, &view), "borrow nonzero overlap");
   for (c = 0; c < 2; c++)
      check(view.history[c] != history[c] &&
                !memcmp(view.history[c], history[c], sizeof(history[c])),
            "history in final decoder storage");
   bad = view;
   bad.state.version++;
   reject_checkpoint(a, &bad);
   bad = view;
   bad.state.sample_rate++;
   reject_checkpoint(a, &bad);
   bad = view;
   bad.state.channels = 1;
   reject_checkpoint(a, &bad);
   bad = view;
   bad.state.pending = 2;
   reject_checkpoint(a, &bad);
   bad = view;
   bad.state.drained = 2;
   reject_checkpoint(a, &bad);
   bad = view;
   bad.state.failed = 1;
   reject_checkpoint(a, &bad);
   bad = view;
   bad.state.previous[1] = 64;
   reject_checkpoint(a, &bad);
   bad = view;
   bad.history_samples[1]--;
   reject_checkpoint(a, &bad);
   bad = view;
   bad.history[1] = NULL;
   reject_checkpoint(a, &bad);
   bad = view;
   bad.history[1] = view.history[0];
   reject_checkpoint(a, &bad);
   bad = view;
   bad.history[0] = view.history[0] + 1;
   reject_checkpoint(a, &bad);
   bad = view;
   bad.history[1] = (const float *)(UINTPTR_MAX & ~(uintptr_t)3);
   reject_checkpoint(a, &bad);
   bad = view;
   bad.history[1] = (const float *)((const uint8_t *)history[1] + 1);
   reject_checkpoint(a, &bad);
   bad = view;
   bad.history[0] = history[0];
   bad.history[1] = history[1];
   history[0][0] = 0.75f;
   for (i = 0; i < 4; i++)
   {
      memcpy(&history[1][0], &bad_bits[i], sizeof(bad_bits[i]));
      reject_checkpoint(a, &bad);
   }
   check(!rwma_xma_restore(a, &view), "self restore borrowed overlap");
   rwma_reset(b);
   check(!rwma_xma_restore(b, &view), "restore into reset decoder");
   memset(data, 0, sizeof(data));
   w.data = data;
   w.bit = 0;
   put(&w, 15, 40);
   put(&w, 1, 1);
   put(&w, 1, 0);
   put(&w, 1, 0);
   put(&w, 8, 0);
   put(&w, 1, 0);
   put(&w, 1, 0);
   put(&w, 1, 0);
   put(&w, 1, 0);
   put(&w, 1, 1);
   put(&w, 1, 0);
   put(&w, 1, 0);
   put(&w, 1, 0);
   check(!rwma_decode_xma(a, data, sizeof(data), 0, 40, &first) &&
             !rwma_decode_xma(b, data, sizeof(data), 0, 40, &second),
         "decode after checkpoint restore");
   for (c = 0; c < 2; c++)
      check(!memcmp(first.planes[c], second.planes[c], 512 * sizeof(float)),
            "resumed PCM is bit identical");
   check(!rwma_xma_capture(a, &view) && !rwma_xma_restore(b, &view),
         "checkpoint before tail");
   check(!rwma_drain_xma(a, &first) && !rwma_drain_xma(b, &second),
         "drain restored tail");
   for (c = 0; c < 2; c++)
      check(!memcmp(first.planes[c], second.planes[c], 256 * sizeof(float)),
            "restored tail is bit identical");
   check(!rwma_xma_capture(a, &view) && view.state.drained, "capture drained state");
   check(!rwma_xma_restore(b, &view) && !rwma_drain_xma(b, &second) && !second.samples,
         "drained checkpoint cannot duplicate tail");
   check(!rwma_xma_restore(b, &cold) &&
             !rwma_decode_xma(b, data, sizeof(data), 0, 40, &second),
         "restore cold state clears old overlap");
   for (c = 0; c < 2; c++)
      for (i = 0; i < 512; i++)
         check(second.planes[c][i] == 0, "cold state PCM");
   data[1] = 0;
   check(rwma_decode_xma(b, data, sizeof(data), 0, 40, &second) == RWMA_BAD_DATA,
         "poison malformed frame");
   check(!rwma_xma_capture(b, &view) && view.state.failed && !view.state.pending &&
             !view.history[0],
         "failed state is canonical without stale PCM");
   check(!rwma_xma_restore(a, &view), "restore failed state");
   check(rwma_decode_xma(a, data, sizeof(data), 0, 40, &first) == RWMA_BAD_DATA,
         "failed checkpoint requires reset");
   rwma_free(a);
   rwma_free(b);
}

static void configure_test(void)
{
   static const unsigned rates[4] = {24000, 32000, 44100, 48000};
   static float history[2][256];
   rwma_config cfg;
   rwma_t *d = NULL, *mono = NULL;
   rwma_xma_view view, after;
   unsigned r, c, i;
   memset(&cfg, 0, sizeof(cfg));
   cfg.codec = RWMA_XMA_RAW; cfg.sample_rate = 48000; cfg.channels = 2;
   check(!rwma_create(&d, &cfg), "reserved stereo context");
   cfg.channels = 1;
   check(!rwma_create(&mono, &cfg), "reserved mono context");
   check(rwma_xma_configure(mono, 48000, 2) == RWMA_UNSUPPORTED,
         "configuration cannot grow its reserved channels");
   for (r = 0; r < 4; r++) for (c = 1; c <= 2; c++)
   {
      check(!rwma_xma_configure(d, rates[r], c) && !rwma_xma_capture(d, &view),
            "configure reserved profile");
      check(view.state.sample_rate == rates[r] && view.state.channels == c &&
            !view.state.pending && !view.state.failed && !view.history[0],
            "profile change resets history and status");
      view.state.pending = 1;
      for (i = 0; i < c; i++)
      {
         unsigned sample;
         for (sample = 0; sample < 256; sample++) history[i][sample] = (float)sample / 1024;
         view.history[i] = history[i]; view.history_samples[i] = 256;
      }
      check(!rwma_xma_restore(d, &view), "seed reusable profile history");
      check(!rwma_xma_configure(d, rates[r], c) && !rwma_xma_capture(d, &after),
            "unchanged profile preserves history");
      check(!memcmp(&view.state, &after.state, sizeof(view.state)), "same profile metadata");
      for (i = 0; i < c; i++)
         check(!memcmp(after.history[i],history[i],sizeof(history[i])), "same profile overlap");
      check(rwma_xma_configure(d, 12345, c) == RWMA_UNSUPPORTED &&
            rwma_xma_configure(d, rates[r], 3) == RWMA_UNSUPPORTED &&
            !rwma_xma_capture(d, &after), "invalid profile does not mutate decoder");
      check(!memcmp(&view.state, &after.state, sizeof(view.state)), "invalid profile metadata");
      for (i = 0; i < c; i++)
         check(!memcmp(after.history[i],history[i],sizeof(history[i])), "invalid profile overlap");
   }
   rwma_free(mono); rwma_free(d);
}

static void reader_test(void)
{
   static const unsigned char bytes[] = {0xd6, 0x39, 0xa5, 0x78,
                                         0x2f, 0xc1, 0x43, 0xeb};
   unsigned start, end, count, i;
   for (start = 0; start < 16; start++)
      for (end = start; end <= 64; end++)
         for (count = 0; count <= 33; count++)
         {
            rwma_reader r;
            unsigned expected = 0;
            r.p = bytes;
            r.pos = start;
            r.end = end;
            r.error = 0;
            if (count <= 32 && count <= end - start)
            {
               for (i = 0; i < count; i++)
                  expected = (expected << 1) |
                             ((bytes[(start + i) / 8] >> (7 - ((start + i) & 7))) & 1);
               check(rwma_read(&r, count) == expected && !r.error &&
                         r.pos == start + count,
                     "bounded word reader bit oracle");
            }
            else
               check(!rwma_read(&r, count) && r.error && r.pos == start,
                     "short word reader preserves cursor");
         }
}
static void prefix_test(void)
{
   static const rwma_book *books[] = {scaleDelta, scaleRunLevel, vec4, vec2,
                                      vec1,       coef0,         coef1};
   static const uint32_t *prefixes[] = {
       scaleDelta_prefix, scaleRunLevel_prefix, vec4_prefix, vec2_prefix,
       vec1_prefix,       coef0_prefix,         coef1_prefix};
   unsigned book, value, offset, end, pattern;
   unsigned char bytes[8];
   for (book = 0; book < 7; book++)
      for (value = 0; value < 256; value++)
      {
         int node = 0;
         unsigned depth;
         uint32_t entry;
         for (depth = 0; depth < 8;)
         {
            node = books[book][node].edge[(value >> (7 - depth)) & 1];
            depth++;
            if (node <= 0)
               break;
         }
         entry = node < 0 ? (0x80000000u | ((unsigned)(-1 - node) << 5) | depth)
                          : ((unsigned)node << 5) | depth;
         check(entry == prefixes[book][value], "prefix table matches code tree");
         for (offset = 0; offset < 8; offset++)
            for (pattern = 0; pattern < 4; pattern++)
            {
               writer w;
               memset(bytes,
                      pattern == 0   ? 0
                      : pattern == 1 ? 255
                      : pattern == 2 ? 0x55
                                     : 0xa3,
                      sizeof(bytes));
               w.data = bytes;
               w.bit = offset;
               put(&w, 8, value);
               for (end = 0; end <= 32; end++)
               {
                  rwma_reader a, b;
                  int x, y;
                  a.p = bytes;
                  a.pos = offset;
                  a.end = offset + end;
                  a.error = 0;
                  b = a;
                  x = rwma_symbol(&a, books[book]);
                  y = rwma_symbol_prefix(&b, books[book], prefixes[book]);
                  check(x == y && a.pos == b.pos && a.error == b.error,
                        "prefix matches tree result, cursor and truncation");
               }
            }
      }
   for (value = 0; value < 913; value++)
   {
      float expected = (float)pow(10.0, ((int)value - 512) / 20.0);
      check(!memcmp(&expected, &pro_gain[value], sizeof(expected)),
            "cached gain matches binary32 pow");
   }
}

int main(void)
{
   reader_test();
   prefix_test();
   configure_test();
   checkpoint_test();
   raw_test();
   packet_test();
   long_packet_test();
   long_packet_damage_test();
   transform_test();
   damaged_test();
   standard_test();
   wave_test();
   puts("PASS rwma raw, packet, transform and damaged-input tests");
   return 0;
}
