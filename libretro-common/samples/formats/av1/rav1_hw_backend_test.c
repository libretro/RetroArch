/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stddef.h>
#include <formats/rav1_hw.h>

typedef struct legacy_backend
{
   void *user;
   bool (*open)(void *, const rav1_sequence_t *, unsigned, rav1_hw_ops_t *);
   void (*close)(rav1_hw_ops_t *);
   bool packet_decoder;
} legacy_backend_t;

typedef char backend_open_prefix[
   offsetof(rav1_hw_backend_t, open) == offsetof(legacy_backend_t, open) ? 1 : -1];
typedef char backend_close_prefix[
   offsetof(rav1_hw_backend_t, close) == offsetof(legacy_backend_t, close) ? 1 : -1];
typedef char backend_mode_prefix[
   offsetof(rav1_hw_backend_t, packet_decoder)
      == offsetof(legacy_backend_t, packet_decoder) ? 1 : -1];
typedef char backend_config_suffix[
   offsetof(rav1_hw_backend_t, open_config) >= sizeof(legacy_backend_t) ? 1 : -1];

static rav1_sequence_t sequence;
static rav1_hw_backend_t factory, retained, empty_backend, foreign;
static rav1_hw_ops_t ops, empty_ops;
static unsigned failures, opens, closes;
static int mode;
#define CHECK(c)                                                               \
   do                                                                          \
   {                                                                           \
      if (!(c))                                                                \
      {                                                                        \
         fprintf(stderr, "line %u: %s\n", (unsigned)__LINE__, #c);             \
         failures++;                                                           \
      }                                                                        \
   } while (0)

static int decode(void *owner, const uint8_t *packet, size_t size,
                  const rav1_sequence_t *seq, const rav1_hw_picture_t *picture)
{
   (void)owner;
   (void)packet;
   (void)size;
   (void)seq;
   (void)picture;
   return 0;
}

static int output(void *owner, unsigned slot, const rav1_grain_t *grain,
                  video_hw_frame_t *frame)
{
   (void)owner;
   (void)slot;
   (void)grain;
   (void)frame;
   return 1;
}

static bool open_backend(void *user, const rav1_sequence_t *seq, unsigned slots,
                         rav1_hw_ops_t *result)
{
   CHECK(user == &factory && seq == &sequence && slots == 9);
   opens++;
   result->decoder = &sequence;
   result->decode = mode == 2 ? NULL : decode;
   result->output = mode == 3 ? NULL : output;
   return mode != 1;
}

static void close_backend(rav1_hw_ops_t *result)
{
   CHECK(result->decoder == &sequence);
   closes++;
   memset(result, 0, sizeof(*result));
}

static unsigned config_opens;
static rav1_obu_t config_obu;
static video_color_metadata_t config_color;
static bool open_config(void *user, const rav1_sequence_t *seq,
      const rav1_obu_t *obu, const video_color_metadata_t *color,
      unsigned slots, rav1_hw_ops_t *result)
{
   CHECK(obu == &config_obu && color == &config_color);
   config_opens++;
   return open_backend(user, seq, slots, result);
}

static int drain_backend(void *decoder, bool discard)
{
   (void)decoder;
   (void)discard;
   return 0;
}

int main(void)
{
   rav1_set_hw_backend(NULL);
   CHECK(!rav1_hw_backend_open(&sequence, 9, &ops, &retained));
   factory.user = &factory;
   factory.open = open_backend;
   factory.close = close_backend;
   rav1_set_hw_backend(&factory);
   CHECK(!rav1_hw_backend_open(NULL, 9, &ops, &retained));
   CHECK(!rav1_hw_backend_open(&sequence, 8, &ops, &retained));
   CHECK(!rav1_hw_backend_open(&sequence, 33, &ops, &retained));
   CHECK(!rav1_hw_backend_open(&sequence, 9, NULL, &retained));
   CHECK(!rav1_hw_backend_open(&sequence, 9, &ops, NULL));
   CHECK(!opens && !closes);
   factory.drain = drain_backend;
   CHECK(!rav1_hw_backend_open(&sequence, 9, &ops, &retained));
   CHECK(!opens && !closes);
   CHECK(!memcmp(&ops, &empty_ops, sizeof(ops)));
   CHECK(!memcmp(&retained, &empty_backend, sizeof(retained)));
   factory.drain = NULL;
   for (mode = 1; mode <= 3; mode++)
   {
      CHECK(!rav1_hw_backend_open(&sequence, 9, &ops, &retained));
      CHECK(!memcmp(&ops, &empty_ops, sizeof(ops)));
      CHECK(!memcmp(&retained, &empty_backend, sizeof(retained)));
      CHECK(opens == (unsigned)mode && closes == (unsigned)mode);
   }
   mode = 0;
   factory.packet_decoder = true;
   CHECK(rav1_hw_backend_open(&sequence, 9, &ops, &retained));
   CHECK(ops.decoder == &sequence && retained.close == close_backend
         && retained.packet_decoder);
   factory.packet_decoder = false;
   CHECK(retained.packet_decoder);
   rav1_set_hw_backend(NULL);
   retained.close(&ops);
   CHECK(opens == 4 && closes == 4 && !ops.decoder);
   CHECK(!rav1_hw_backend_open(&sequence, 9, &ops, &retained));
   factory.close = NULL;
   rav1_set_hw_backend(&factory);
   CHECK(!rav1_hw_backend_open(&sequence, 9, &ops, &retained) && opens == 4);
   rav1_set_hw_backend(NULL);
   factory.close = close_backend;
   factory.open_config = open_config;
   factory.open = NULL;
   rav1_set_hw_backend(&factory);
   CHECK(rav1_hw_backend_open_config(&sequence, &config_obu, &config_color,
         9, &ops, &retained));
   CHECK(config_opens == 1 && retained.open_config == open_config);
   retained.close(&ops);
   mode = 1;
   CHECK(!rav1_hw_backend_open_config(&sequence, &config_obu, &config_color,
         9, &ops, &retained));
   CHECK(config_opens == 2 && !ops.decoder && !retained.open_config);
   rav1_set_hw_backend(NULL);
   rav1_set_hw_backend(&factory);
   CHECK(!rav1_unset_hw_backend(&foreign));
   CHECK(rav1_unset_hw_backend(&factory));
   CHECK(!rav1_unset_hw_backend(&factory) && !rav1_unset_hw_backend(NULL));
   foreign = factory;
   rav1_set_hw_backend(&foreign);
   CHECK(!rav1_unset_hw_backend(&factory));
   mode = 0;
   CHECK(rav1_hw_backend_open_config(&sequence, &config_obu, &config_color,
         9, &ops, &retained));
   CHECK(retained.open_config == foreign.open_config);
   retained.close(&ops);
   CHECK(rav1_unset_hw_backend(&foreign));
   printf("AV1 hardware backend registry: %u failures\n", failures);
   return failures ? 1 : 0;
}
