/* Differential fuzz target for config_file structural integrity.
 *
 * For every input, the same bytes are parsed two ways:
 *   (a) in one piece via config_file_new_from_string
 *   (b) through the streaming push API in pseudo-random packet
 *       sizes derived from the input itself
 * and the results must agree on three structural invariants:
 *
 *   1. ENTRY-LIST EQUALITY: same entries, same order, same
 *      key/value bytes, same count.
 *   2. MAP COHERENCE: for every key in the list, config_get_entry
 *      returns exactly the FIRST list entry holding that key (the
 *      duplicate policy), in both configs.  This cross-checks the
 *      fused parse-time hash against the real rhmap_hash_string
 *      used at lookup time on arbitrary byte sequences, not just
 *      the curated alphabet in the unit test.
 *   3. TAIL VALIDITY: conf->tail is the actual last list node.
 *
 * No io interface is registered, so '#include' directives are
 * recorded but never touch the file system - both paths behave
 * identically and the target stays hermetic.
 *
 * Build (clang):
 *   clang -O1 -g -fsanitize=fuzzer,address,undefined \
 *     -fno-sanitize-recover=all -I../../../include \
 *     -o config_file_fuzz config_file_fuzz.c \
 *     ../../../file/config_file.c <usual libretro-common deps>
 * Run:
 *   ./config_file_fuzz -max_len=1024 corpus/
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <file/config_file.h>

static uint32_t xs32(uint32_t *s)
{
   uint32_t x = *s;
   x ^= x << 13;
   x ^= x >> 17;
   x ^= x << 5;
   return (*s = x ? x : 0x9e3779b9u);
}

static void die(const char *what, const uint8_t *data, size_t size)
{
   size_t i;
   fprintf(stderr, "STRUCTURAL DIVERGENCE: %s\ninput (%u bytes):", what,
         (unsigned)size);
   for (i = 0; i < size; i++)
      fprintf(stderr, " %02x", data[i]);
   fprintf(stderr, "\n");
   abort();
}

static int str_eq(const char *a, const char *b)
{
   if (!a || !b)
      return a == b;
   return !strcmp(a, b);
}

/* Invariant 2+3 for one conf.  The exact first-entry check walks
 * the list per key (quadratic), so it runs only on small inputs
 * where it is cheap; larger inputs get the linear form - every
 * listed key must resolve through the map to SOME entry carrying
 * that exact key.  Small inputs dominate coverage of the map
 * logic anyway; large ones mostly exercise the window mechanics,
 * which invariant 1 covers at every size. */
static void check_conf(config_file_t *c, const uint8_t *data, size_t size)
{
   const struct config_entry_list *e;
   const struct config_entry_list *last = NULL;
   int exact = (size <= 2048);
   for (e = c->entries; e; e = e->next)
   {
      last = e;
      if (e->key)
      {
         const struct config_entry_list *hit = config_get_entry(c, e->key);
         if (!hit || !hit->key || strcmp(hit->key, e->key))
            die("map lookup missed or mismatched a listed key",
                  data, size);
         if (exact)
         {
            const struct config_entry_list *first = NULL;
            const struct config_entry_list *w;
            for (w = c->entries; w; w = w->next)
               if (w->key && !strcmp(w->key, e->key))
               {
                  first = w;
                  break;
               }
            if (hit != first)
               die("map does not resolve key to first list entry",
                     data, size);
         }
      }
   }
   if (c->tail != last)
      die("conf->tail is not the last list node", data, size);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
   config_file_t *slurped;
   config_file_t *streamed;
   config_file_stream_t *st;
   const struct config_entry_list *a;
   const struct config_entry_list *b;
   uint32_t rng = 0x811c9dc5u ^ (uint32_t)size;
   size_t off   = 0;
   char *copy;

   if (size > 65536)
      return 0;

   /* (a) slurp */
   if (!(copy = (char*)malloc(size + 1)))
      return 0;
   memcpy(copy, data, size);
   copy[size] = '\0';
   slurped = config_file_new_from_string(copy, NULL);
   free(copy);
   if (!slurped)
      return 0;

   /* (b) stream, packet sizes 1..37 driven by the input */
   if (!(st = config_file_stream_new(NULL)))
   {
      config_file_free(slurped);
      return 0;
   }
   while (off < size)
   {
      size_t n = 1 + (xs32(&rng) % 37);
      if (n > size - off)
         n = size - off;
      if (!config_file_stream_push(st, data + off, n))
      {
         config_file_stream_free(st);
         config_file_free(slurped);
         return 0;
      }
      off += n;
   }
   if (!(streamed = config_file_stream_finish(st)))
   {
      config_file_free(slurped);
      return 0;
   }

   /* Invariant 1: entry-list equality */
   a = slurped->entries;
   b = streamed->entries;
   while (a && b)
   {
      if (     !str_eq(a->key, b->key)
            || !str_eq(a->value, b->value)
            || a->readonly != b->readonly)
         die("entry mismatch", data, size);
      a = a->next;
      b = b->next;
   }
   if (a || b)
      die("entry count mismatch", data, size);

   /* Invariants 2+3 on both */
   check_conf(slurped, data, size);
   check_conf(streamed, data, size);

   config_file_free(slurped);
   config_file_free(streamed);
   return 0;
}
