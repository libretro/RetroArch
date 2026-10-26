/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <formats/rmp4.h>

struct fixture
{
   uint8_t data[8192];
   size_t used;
   const uint8_t *value[8];
   size_t length[8];
   enum rmp4_language_kind kind[8];
};

struct language_case
{
   unsigned version, mdhd_size, code;
   int elng_header;
   const char *text;
   size_t length;
   unsigned first;
   enum rmp4_language_kind kind;
   unsigned source;
};

static char long_language[1024];
static struct fixture first, second;
static const struct language_case cases[] = {
   {0, 24, 0x15c7, -1, NULL, 0, 0, RMP4_LANGUAGE_MDHD, 1},
   {1, 36, 0x2a0e, -1, NULL, 0, 0, RMP4_LANGUAGE_MDHD, 1},
   {0, 24, 0x15c7, 0, "zh-Hant", 8, 1, RMP4_LANGUAGE_BCP47, 2},
   {1, 36, 0x2a0e, 0, "", 1, 0, RMP4_LANGUAGE_BCP47, 2},
   {2, 24, 0x15c7, -1, NULL, 0, 0, RMP4_LANGUAGE_NONE, 0},
   {0, 21, 0x15c7, -1, NULL, 0, 0, RMP4_LANGUAGE_NONE, 0},
   {0, 24, 0, -1, NULL, 0, 0, RMP4_LANGUAGE_MDHD, 1},
   {1, 36, 0x7fff, -1, NULL, 0, 0, RMP4_LANGUAGE_MDHD, 1},
   {0, 24, 0x1a41, 0x01000000, "fr-FR", 6, 1, RMP4_LANGUAGE_MDHD, 1},
   {0, 24, 0x1a41, 1, "fr-FR", 6, 0, RMP4_LANGUAGE_MDHD, 1},
   {0, 24, 0x1a41, 0, long_language, 1024, 1, RMP4_LANGUAGE_BCP47, 2},
   {0, 24, 0x1a41, 0, "fr-FR", 5, 0, RMP4_LANGUAGE_BCP47, 2},
   {0, 24, 0x1a41, 0, "", 0, 1, RMP4_LANGUAGE_BCP47, 2},
   {1, 36, 0x15c7, 0, "iw\0tail", 8, 0, RMP4_LANGUAGE_BCP47, 2},
   {2, 24, 0x15c7, 0, "und", 4, 1, RMP4_LANGUAGE_BCP47, 2},
   {1, 33, 0x15c7, -1, NULL, 0, 0, RMP4_LANGUAGE_NONE, 0}
};

static void bytes(struct fixture *f, const void *value, size_t length)
{
   assert(length <= sizeof(f->data) - f->used);
   memcpy(f->data + f->used, value, length);
   f->used += length;
}

static void word(struct fixture *f, uint32_t value)
{
   uint8_t data[4];
   data[0] = (uint8_t)(value >> 24);
   data[1] = (uint8_t)(value >> 16);
   data[2] = (uint8_t)(value >> 8);
   data[3] = (uint8_t)value;
   bytes(f, data, 4);
}

static void zero(struct fixture *f, unsigned length)
{
   assert(length <= sizeof(f->data) - f->used);
   memset(f->data + f->used, 0, length);
   f->used += length;
}

static size_t box(struct fixture *f, const char *type)
{
   size_t start = f->used;
   word(f, 0);
   bytes(f, type, 4);
   return start;
}

static void end(struct fixture *f, size_t start)
{
   size_t used = f->used;
   f->used = start;
   word(f, (uint32_t)(used - start));
   f->used = used;
}

static const uint8_t *elng(struct fixture *f, const struct language_case *c)
{
   const uint8_t *value;
   size_t b = box(f, "elng");
   word(f, (uint32_t)c->elng_header);
   value = f->data + f->used;
   bytes(f, c->text, c->length);
   end(f, b);
   return value;
}

static void build(struct fixture *f, unsigned group)
{
   size_t b, moov, trak, mdia, minf, stbl, body;
   unsigned i;
   memset(f, 0, sizeof(*f));
   b = box(f, "ftyp");
   bytes(f, "iso6", 4);
   word(f, 0);
   bytes(f, "iso6", 4);
   end(f, b);
   moov = box(f, "moov");
   b = box(f, "mvhd");
   zero(f, 12);
   word(f, 1000);
   word(f, 1000);
   zero(f, 80);
   end(f, b);
   for (i = 0; i < 8; i++)
   {
      const struct language_case *c = &cases[group * 8 + i];
      const uint8_t *packed, *extended = NULL;
      trak = box(f, "trak");
      mdia = box(f, "mdia");
      if (c->elng_header >= 0 && c->first) extended = elng(f, c);
      b = box(f, "mdhd");
      body = f->used;
      word(f, (uint32_t)c->version << 24);
      zero(f, c->version == 1 ? 16 : 8);
      word(f, 1000);
      zero(f, c->version == 1 ? 8 : 4);
      packed = f->data + f->used;
      word(f, (uint32_t)c->code << 16);
      assert(c->mdhd_size <= f->used - body);
      f->used = body + c->mdhd_size;
      end(f, b);
      if (c->elng_header >= 0 && !c->first) extended = elng(f, c);
      b = box(f, "hdlr");
      zero(f, 8);
      bytes(f, "sbtl", 4);
      zero(f, 12);
      end(f, b);
      minf = box(f, "minf");
      stbl = box(f, "stbl");
      end(f, stbl);
      end(f, minf);
      end(f, mdia);
      end(f, trak);
      f->kind[i] = c->kind;
      f->value[i] = c->source == 1 ? packed : c->source == 2 ? extended : NULL;
      f->length[i] = c->source == 1 ? 2 : c->source == 2 ? c->length : 0;
   }
   end(f, moov);
}

static void check(const struct fixture *f, rmp4_t *m)
{
   unsigned i, pass;
   size_t length;
   enum rmp4_language_kind kind;
   assert(m && rmp4_num_tracks(m) == 8);
   for (pass = 0; pass < 100; pass++)
      for (i = 0; i < 8; i++)
      {
         const uint8_t *value;
         length = 99;
         kind = RMP4_LANGUAGE_MDHD;
         value = rmp4_get_track_language(m, (int)i, &length, &kind);
         assert(value == f->value[i] && length == f->length[i] && kind == f->kind[i]);
         assert(rmp4_get_track_language(m, (int)i, NULL, NULL) == value);
         if (value)
         {
            assert(value >= f->data && value + length <= f->data + f->used);
            assert(!memcmp(value, f->value[i], length));
         }
      }
   length = 99;
   kind = RMP4_LANGUAGE_MDHD;
   assert(!rmp4_get_track_language(m, -1, &length, &kind));
   assert(!length && kind == RMP4_LANGUAGE_NONE);
   length = 99;
   kind = RMP4_LANGUAGE_MDHD;
   assert(!rmp4_get_track_language(m, 8, &length, &kind));
   assert(!length && kind == RMP4_LANGUAGE_NONE);
   length = 99;
   kind = RMP4_LANGUAGE_MDHD;
   assert(!rmp4_get_track_language(NULL, 0, &length, &kind));
   assert(!length && kind == RMP4_LANGUAGE_NONE);
}

int main(void)
{
   rmp4_t *a, *b;
   size_t prefix;
   memset(long_language, 'x', sizeof(long_language));
   build(&first, 0);
   build(&second, 1);
   a = rmp4_open_memory(first.data, first.used);
   b = rmp4_open_memory(second.data, second.used);
   check(&first, a);
   check(&second, b);
   rmp4_rewind(a);
   rmp4_set_avail(b, second.used);
   check(&first, a);
   check(&second, b);
   rmp4_close(a);
   rmp4_close(b);
   for (prefix = 0; prefix <= second.used; prefix++)
   {
      int more = 0;
      size_t lo = 0, hi = 0;
      a = rmp4_open_memory_avail(second.data, second.used, prefix, &more, &lo, &hi);
      if (a)
      {
         check(&second, a);
         rmp4_close(a);
      }
      else assert(lo <= hi && hi <= second.used);
   }
   puts("PASS: raw MP4 mdhd/elng language spans, precedence, bounds and partial input");
   return 0;
}
