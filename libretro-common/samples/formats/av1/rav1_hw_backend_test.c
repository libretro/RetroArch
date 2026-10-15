/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <formats/rav1_hw.h>

static rav1_sequence_t sequence;
static rav1_hw_backend_t factory, retained, empty_backend;
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
   for (mode = 1; mode <= 3; mode++)
   {
      CHECK(!rav1_hw_backend_open(&sequence, 9, &ops, &retained));
      CHECK(!memcmp(&ops, &empty_ops, sizeof(ops)));
      CHECK(!memcmp(&retained, &empty_backend, sizeof(retained)));
      CHECK(opens == (unsigned)mode && closes == (unsigned)mode);
   }
   mode = 0;
   CHECK(rav1_hw_backend_open(&sequence, 9, &ops, &retained));
   CHECK(ops.decoder == &sequence && retained.close == close_backend);
   rav1_set_hw_backend(NULL);
   retained.close(&ops);
   CHECK(opens == 4 && closes == 4 && !ops.decoder);
   CHECK(!rav1_hw_backend_open(&sequence, 9, &ops, &retained));
   factory.close = NULL;
   rav1_set_hw_backend(&factory);
   CHECK(!rav1_hw_backend_open(&sequence, 9, &ops, &retained) && opens == 4);
   rav1_set_hw_backend(NULL);
   printf("AV1 hardware backend registry: %u failures\n", failures);
   return failures ? 1 : 0;
}
