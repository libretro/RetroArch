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
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#include "slang_blob.h"

static struct slang_blob *slang_blob_new(void)
{
   struct slang_blob *b = (struct slang_blob*)malloc(sizeof(*b));
   if (!b)
      return NULL;
   memset(b, 0, sizeof(*b));
   retro_atomic_int_init(&b->refs, 1);
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
      retro_atomic_fetch_add_int(&b->refs, 1);
   return b;
}

static void slang_blob_free(struct slang_blob *b)
{
   if (b->free_fn)
      b->free_fn(b->ud, b->mem);
   else
      free(b->mem);
   free(b);
}

static struct
{
   struct slang_blob **v;
   size_t              n;
   size_t              cap;
   unsigned            reused;
   mpsc_stack_t        dead;      /* listed blobs whose last reference went */
   retro_atomic_int_t  claimed;
   uintptr_t           owner;     /* read after claimed, written before */
} blob_table;

/* Whether this thread may touch the table now.  Without threads
 * there is one, and it may. */
static bool slang_blob_table_mine(void)
{
#ifdef HAVE_THREADS
   if (!retro_atomic_load_acquire_int(&blob_table.claimed))
      return true;
   return blob_table.owner == sthread_get_current_thread_id();
#else
   return true;
#endif
}

void slang_blob_table_claim(void)
{
#ifdef HAVE_THREADS
   blob_table.owner = sthread_get_current_thread_id();
   retro_atomic_store_release_int(&blob_table.claimed, 1);
#endif
}

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

/* Frees what died listed since the last operation: at the table, so
 * on the one thread that is. */
static void slang_blob_table_drain(void)
{
   mpsc_stack_node_t *node = mpsc_stack_drain(&blob_table.dead);
   while (node)
   {
      struct slang_blob *b    = (struct slang_blob*)node;
      node                    = node->next;
      slang_blob_table_remove(b);
      slang_blob_free(b);
   }
}

void slang_blob_table_release(void)
{
   slang_blob_table_drain();
   retro_atomic_store_release_int(&blob_table.claimed, 0);
}

struct slang_blob *slang_blob_table_find(const char *key, bool fragment)
{
   size_t i;
   if (!slang_blob_table_mine())
      return NULL;
   slang_blob_table_drain();
   for (i = 0; i < blob_table.n; i++)
   {
      struct slang_blob *b = blob_table.v[i];
      if (b->fragment != fragment || memcmp(b->key, key, sizeof(b->key)))
         continue;
      /* taken only while someone still holds it: a blob whose count
       * reached zero is on its way to the dead list, however briefly */
      for (;;)
      {
         int refs = retro_atomic_load_acquire_int(&b->refs);
         if (refs <= 0)
            return NULL;
         if (retro_atomic_cas_int(&b->refs, refs, refs + 1))
            break;
      }
      blob_table.reused++;
      return b;
   }
   return NULL;
}

void slang_blob_table_add(struct slang_blob *b, const char *key,
      bool fragment)
{
   if (!b || b->listed || !slang_blob_table_mine())
      return;
   slang_blob_table_drain();
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
   slang_blob_table_drain();
   *live   = blob_table.n;
   *reused = blob_table.reused;
}

void slang_blob_unref(struct slang_blob *b)
{
   if (!b || retro_atomic_fetch_add_int(&b->refs, -1) != 1)
      return;
   /* a listed blob leaves the table on the table's thread */
   if (b->listed)
      mpsc_stack_push(&blob_table.dead, &b->park);
   else
      slang_blob_free(b);
}
