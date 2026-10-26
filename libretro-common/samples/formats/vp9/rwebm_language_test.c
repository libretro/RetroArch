/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <formats/rwebm.h>

struct fixture
{
   uint8_t data[4096];
   size_t used;
   const char *text[8];
   size_t length[8];
   enum rwebm_language_kind kind[8];
};

static struct fixture first, second;
static char long_language[1024];

static size_t begin(struct fixture *f, uint32_t id, unsigned bytes)
{
   size_t mark;
   assert(f->used + bytes + 2 <= sizeof(f->data));
   while (bytes--)
      f->data[f->used++] = (uint8_t)(id >> (bytes * 8));
   mark = f->used;
   f->data[f->used++] = 0x40;
   f->data[f->used++] = 0;
   return mark;
}

static void end(struct fixture *f, size_t mark)
{
   size_t size = f->used - mark - 2;
   assert(size < 16383);
   f->data[mark] = (uint8_t)(0x40 | (size >> 8));
   f->data[mark + 1] = (uint8_t)size;
}

static const char *text(struct fixture *f, uint32_t id, unsigned bytes,
      const char *value, size_t length)
{
   size_t mark = begin(f, id, bytes);
   const char *result = (const char*)f->data + f->used;
   assert(length <= sizeof(f->data) - f->used);
   memcpy(f->data + f->used, value, length);
   f->used += length;
   end(f, mark);
   return result;
}

static void build(struct fixture *f, int replacement)
{
   static const char padded[] = { 'f', 'r', 'a', 0, 0 };
   static const size_t lengths[] = { 3, 3, 5, 5, 7, 0, 5, 1024 };
   static const enum rwebm_language_kind kinds[] = {
      RWEBM_LANGUAGE_LEGACY, RWEBM_LANGUAGE_LEGACY,
      RWEBM_LANGUAGE_BCP47, RWEBM_LANGUAGE_BCP47,
      RWEBM_LANGUAGE_BCP47, RWEBM_LANGUAGE_BCP47,
      RWEBM_LANGUAGE_LEGACY, RWEBM_LANGUAGE_BCP47
   };
   size_t segment, tracks, entry;
   unsigned i;
   memset(f, 0, sizeof(*f));
   entry = begin(f, 0x1a45dfa3u, 4);
   end(f, entry);
   segment = begin(f, 0x18538067u, 4);
   tracks = begin(f, 0x1654ae6bu, 4);
   for (i = 0; i < 8; i++)
   {
      char number = (char)(i + 1);
      entry = begin(f, 0xaeu, 1);
      text(f, 0xd7u, 1, &number, 1);
      text(f, 0x83u, 1, "\021", 1);
      text(f, 0x86u, 1, "D_WEBVTT/SUBTITLES", 18);
      f->text[i] = "eng";
      f->length[i] = lengths[i];
      f->kind[i] = kinds[i];
      switch (i)
      {
         case 0:
            if (replacement)
            {
               f->text[i] = text(f, 0x22b59du, 3, "it-IT", 5);
               f->length[i] = 5;
               f->kind[i] = RWEBM_LANGUAGE_BCP47;
            }
            break;
         case 1:
            f->length[i] = replacement ? 0 : 3;
            f->text[i] = text(f, 0x22b59cu, 3, "fra", f->length[i]);
            break;
         case 2: f->text[i] = text(f, 0x22b59du, 3, "fr-FR", 5); break;
         case 3:
            text(f, 0x22b59cu, 3, "ger", 3);
            f->text[i] = text(f, 0x22b59du, 3, replacement ? "en-GB" : "de-DE", 5);
            break;
         case 4:
            f->text[i] = text(f, 0x22b59du, 3, "zh-Hant", 7);
            text(f, 0x22b59cu, 3, "chi", 3);
            break;
         case 5:
            if (!replacement) text(f, 0x22b59cu, 3, "eng", 3);
            f->text[i] = text(f, 0x22b59du, 3, "", 0);
            if (replacement) text(f, 0x22b59cu, 3, "eng", 3);
            break;
         case 6: f->text[i] = text(f, 0x22b59cu, 3, padded, sizeof(padded)); break;
         case 7: f->text[i] = text(f, 0x22b59du, 3, long_language, sizeof(long_language)); break;
      }
      end(f, entry);
   }
   end(f, tracks);
   end(f, segment);
}

static void check(const rwebm_t *webm, const struct fixture *f)
{
   unsigned i;
   assert(rwebm_num_tracks(webm) == 8);
   for (i = 0; i < 8; i++)
   {
      enum rwebm_language_kind kind = RWEBM_LANGUAGE_NONE;
      size_t length = 0;
      const char *value = rwebm_get_track_language(webm, (int)i, &length, &kind);
      assert(value && length == f->length[i] && kind == f->kind[i]);
      assert(!memcmp(value, f->text[i], length));
      if (i || f == &second) assert(value == f->text[i]);
      assert(rwebm_get_track_language(webm, (int)i, NULL, NULL) == value);
   }
}

int main(void)
{
   rwebm_t *webm, *other, *copy;
   const char *value;
   size_t length, avail;
   enum rwebm_language_kind kind;
   unsigned i;
   memset(long_language, 'a', sizeof(long_language));
   memcpy(long_language, "en-x-", 5);
   build(&first, 0);
   build(&second, 1);
   webm = rwebm_open_memory(first.data, first.used);
   other = rwebm_open_memory(second.data, second.used);
   assert(webm && other);
   check(webm, &first);
   check(other, &second);
   copy = rwebm_cursor_clone(webm);
   assert(copy);
   check(copy, &first);
   rwebm_close(webm);
   check(copy, &first);
   rwebm_cursor_copy(copy, other);
   check(copy, &second);
   rwebm_cursor_copy(copy, copy);
   rwebm_cursor_copy(copy, NULL);
   rwebm_cursor_copy(NULL, other);
   rwebm_close(other);
   check(copy, &second);
   for (i = 0; i < 3; i++)
   {
      length = 123;
      kind = RWEBM_LANGUAGE_BCP47;
      value = rwebm_get_track_language(i == 2 ? NULL : copy,
            i ? 8 : -1, &length, &kind);
      assert(!value && !length && kind == RWEBM_LANGUAGE_NONE);
   }
   rwebm_close(copy);
   for (avail = 0; avail <= first.used; avail++)
   {
      int more = 0;
      webm = rwebm_open_memory_avail(first.data, first.used, avail, &more);
      if (webm)
      {
         check(webm, &first);
         rwebm_close(webm);
      }
      else assert(more);
   }
   puts("PASS: raw WebM languages, BCP47 precedence, default, padding, bounds and cursor lifetime");
   return 0;
}
