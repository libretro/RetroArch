/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>

#include "slang_blob.h"

static struct slang_blob *slang_blob_new(void)
{
   struct slang_blob *b = (struct slang_blob*)malloc(sizeof(*b));
   if (!b)
      return NULL;
   memset(b, 0, sizeof(*b));
   b->refs = 1;
   return b;
}

struct slang_blob *slang_blob_adopt_words(uint32_t *words, size_t nwords)
{
   struct slang_blob *b = words ? slang_blob_new() : NULL;
   if (!b)
   {
      free(words);
      return NULL;
   }
   b->mem    = words;
   b->len    = nwords * sizeof(uint32_t);
   b->words  = words;
   b->nwords = nwords;
   return b;
}

#ifdef HAVE_RSLC
struct slang_blob *slang_blob_adopt_result(struct rslc_result *res,
      const struct rslc_opts *opts)
{
   struct slang_blob *b = res->blob && res->spirv ? slang_blob_new() : NULL;
   if (!b)
   {
      rslc_result_free(res, opts);
      return NULL;
   }
   b->mem     = res->blob;
   b->len     = res->blob_len;
   b->free_fn = opts->free_fn;
   b->ud      = opts->ud;
   b->refl    = res->refl;
   b->words   = res->spirv;
   b->nwords  = res->spirv_len;
   memset(res, 0, sizeof(*res));
   return b;
}
#endif

struct slang_blob *slang_blob_ref(struct slang_blob *b)
{
   if (b)
      b->refs++;
   return b;
}

static struct
{
   struct slang_blob **v;
   size_t              n;
   size_t              cap;
   unsigned            reused;
} blob_table;

static void slang_blob_table_remove(struct slang_blob *b)
{
   size_t i;
   for (i = 0; i < blob_table.n; i++)
   {
      if (blob_table.v[i] != b)
         continue;
      blob_table.v[i] = blob_table.v[--blob_table.n];
      break;
   }
   b->listed = false;
   if (!blob_table.n)
   {
      free(blob_table.v);
      blob_table.v   = NULL;
      blob_table.cap = 0;
   }
}

struct slang_blob *slang_blob_table_find(const char *key, bool fragment)
{
   size_t i;
   for (i = 0; i < blob_table.n; i++)
   {
      struct slang_blob *b = blob_table.v[i];
      if (b->fragment != fragment || memcmp(b->key, key, sizeof(b->key)))
         continue;
      blob_table.reused++;
      return slang_blob_ref(b);
   }
   return NULL;
}

void slang_blob_table_add(struct slang_blob *b, const char *key,
      bool fragment)
{
   if (!b || b->listed)
      return;
   if (blob_table.n == blob_table.cap)
   {
      size_t cap             = blob_table.cap ? blob_table.cap * 2 : 64;
      struct slang_blob **v  = (struct slang_blob**)realloc(blob_table.v,
            cap * sizeof(*v));
      if (!v)
         return;
      blob_table.v   = v;
      blob_table.cap = cap;
   }
   memcpy(b->key, key, sizeof(b->key));
   b->fragment                  = fragment;
   b->listed                    = true;
   blob_table.v[blob_table.n++] = b;
}

void slang_blob_table_stats(size_t *live, unsigned *reused)
{
   *live   = blob_table.n;
   *reused = blob_table.reused;
}

void slang_blob_unref(struct slang_blob *b)
{
   if (!b || --b->refs)
      return;
   if (b->listed)
      slang_blob_table_remove(b);
   if (b->free_fn)
      b->free_fn(b->ud, b->mem);
   else
      free(b->mem);
   free(b);
}
