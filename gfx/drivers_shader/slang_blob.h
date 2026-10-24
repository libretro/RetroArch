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

#ifndef SLANG_BLOB_H
#define SLANG_BLOB_H

#include <stdint.h>
#include <stddef.h>
#include <boolean.h>
#include <retro_common_api.h>
#include <retro_atomic.h>
#include <queues/mpsc_stack.h>

#ifdef HAVE_RSLC
#include <gfx/shaders/rslc.h>
#endif

RETRO_BEGIN_DECLS

struct rslc_reflect;

/* A compiled stage as one allocation.  The SPIR-V words are read where
 * they lie by the driver (vkCreateShaderModule, the GL SPIR-V path, a
 * SPIRV-Cross parse) and by the cache writer; when the compiler wrote
 * its reflection tables (rslc), @refl points at them inside the same
 * allocation and the chain binds by them without crossing the words.
 * A blob is shared by reference between the compile output, the pass
 * that binds it and whatever else holds it, and released when the last
 * reference goes; the count is atomic, so a chain on one thread may let
 * go of a stage a chain building on another just took. */
struct slang_blob
{
   mpsc_stack_node_t          park;     /* on the table's dead list */
   void                      *mem;      /* the allocation */
   size_t                     len;
   void                     (*free_fn)(void *ud, void *ptr); /* NULL: free() */
   void                      *ud;
   const struct rslc_reflect *refl;     /* NULL: a module without tables */
   const uint32_t            *words;    /* into mem */
   size_t                     nwords;
   retro_atomic_int_t         refs;
   char                       key[32];  /* the pass key it is listed under */
   bool                       fragment;
   bool                       listed;   /* in the table */
};

/* Takes malloc'd SPIR-V words of a module no tables describe (glslang's,
 * a stage slang_rect rewrote). NULL when out of memory: @words is freed
 * either way. */
struct slang_blob *slang_blob_adopt_words(uint32_t *words, size_t nwords);

#ifdef HAVE_RSLC
/* Takes an rslc blob produced for RSLC_DST_SPIRV: words and tables in
 * place, released through @opts's allocator.  NULL when out of memory:
 * the result is freed either way. */
struct slang_blob *slang_blob_adopt_result(struct rslc_result *res,
      const struct rslc_opts *opts);
#endif

struct slang_blob *slang_blob_ref(struct slang_blob *b);
void slang_blob_unref(struct slang_blob *b);

/* The table of live blobs, keyed by the pass they were compiled from:
 * @key is the 32-character cache key of the pass's two stage sources
 * and compiler (spirv_cache_compute_hash), and a pass that names a
 * .slang another live pass already compiled - the same helper in two
 * slots of one preset, or the chain being rebuilt while the one on
 * screen still holds its blobs - takes the blob it has.  The table holds
 * no reference: a blob leaves it when its last reference goes.
 *
 * One thread reads and writes it at a time: the thread that builds
 * chains, or, while a load builds its chain on the task thread, that
 * thread alone - it claims the table for the load and the other
 * threads' lookups answer nothing and their listings list nothing
 * meanwhile.  A listed blob whose last reference goes on any thread is
 * parked on a dead list the table drains at its next operation, so no
 * thread but the one at the table ever touches it. */

/* The calling thread alone uses the table until the release; claiming
 * again moves the claim to the calling thread, for a task whose calls
 * run on one worker after another but never at once. */
void slang_blob_table_claim(void);
void slang_blob_table_release(void);

/* A new reference to the listed blob of that stage, or NULL. */
struct slang_blob *slang_blob_table_find(const char *key, bool fragment);

/* Lists @b under @key.  Nothing happens for a blob already listed. */
void slang_blob_table_add(struct slang_blob *b, const char *key,
      bool fragment);

/* @live: blobs listed now; @reused: finds that returned one. */
void slang_blob_table_stats(size_t *live, unsigned *reused);

RETRO_END_DECLS

#endif
