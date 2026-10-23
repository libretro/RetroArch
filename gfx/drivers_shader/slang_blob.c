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

void slang_blob_unref(struct slang_blob *b)
{
   if (!b || --b->refs)
      return;
   if (b->free_fn)
      b->free_fn(b->ud, b->mem);
   else
      free(b->mem);
   free(b);
}
