/* gfx/video_driver.c's coordinate array (video_coord_array_append()
 * and video_coord_array_free()), with the allocator routed through
 * the test so each allocation an append makes can be failed in turn:
 *
 *  - appends grow the array, a stream first given part-way through
 *    reads zero for the vertices before it, and the contents are what
 *    was appended;
 *  - an append onto an empty array, and a growth of a full one, with
 *    each of their allocations failed: the append reports failure,
 *    the vertices already held are kept, nothing is written past the
 *    streams, and free() leaves nothing behind (ASan, LSan).
 *
 * The code is taken out of video_driver.c by extract_coord_array.awk. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <boolean.h>
#include <retro_inline.h>
#include <retro_math.h>
#include <retro_miscellaneous.h>
#include "gfx/video_defines.h"

static unsigned alloc_calls; /* allocations so far */
static unsigned fail_at;     /* the one to fail, 1-based; 0 none */

static void *test_malloc(size_t len)
{
   if (++alloc_calls == fail_at)
      return NULL;
   return malloc(len);
}

static void *test_calloc(size_t n, size_t len)
{
   if (++alloc_calls == fail_at)
      return NULL;
   return calloc(n, len);
}

static void *test_realloc(void *p, size_t len)
{
   if (++alloc_calls == fail_at)
      return NULL;
   return realloc(p, len);
}

#define malloc  test_malloc
#define calloc  test_calloc
#define realloc test_realloc
bool video_coord_array_append(video_coord_array_t *ca,
      const video_coords_t *coords, unsigned count);
void video_coord_array_free(video_coord_array_t *ca);
#include "coord_array_driver.h"
#undef malloc
#undef calloc
#undef realloc

static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { \
   fprintf(stderr, "FAIL: " __VA_ARGS__); \
   fprintf(stderr, "\n"); failures++; } } while (0)

#define N 6

static float vtx[N * 2], col[N * 4], tex[N * 2];

static video_coords_t coords_of(unsigned n, bool color, bool texc)
{
   video_coords_t c;
   memset(&c, 0, sizeof(c));
   c.vertex    = vtx;
   c.color     = color ? col : NULL;
   c.tex_coord = texc  ? tex : NULL;
   c.vertices  = n;
   return c;
}

static void test_contents(void)
{
   video_coord_array_t ca;
   video_coords_t c;
   unsigned i;

   memset(&ca, 0, sizeof(ca));
   alloc_calls = 0;
   fail_at     = 0;

   c = coords_of(3, false, false);
   CHECK(video_coord_array_append(&ca, &c, 3), "contents: first append");
   /* Colour first given on the second append */
   c = coords_of(N, true, false);
   CHECK(video_coord_array_append(&ca, &c, N), "contents: second append");
   CHECK(ca.coords.vertices == 3 + N, "contents: %u vertices",
         (unsigned)ca.coords.vertices);
   CHECK(!memcmp(ca.coords.vertex, vtx, 3 * 2 * sizeof(float))
         && !memcmp(ca.coords.vertex + 3 * 2, vtx, N * 2 * sizeof(float)),
         "contents: the vertices are what was appended");
   for (i = 0; i < 3 * 4; i++)
      CHECK(ca.coords.color[i] == 0.0f,
            "contents: colour before it was given reads zero");
   CHECK(!memcmp(ca.coords.color + 3 * 4, col, N * 4 * sizeof(float)),
         "contents: the colours are what was appended");
   CHECK(!ca.coords.tex_coord, "contents: a stream never given stays NULL");
   video_coord_array_free(&ca);
   printf("contents: ok\n");
}

/* Each allocation of one append failed in turn, from @held vertices
 * already in the array */
static void sweep(const char *name, unsigned held)
{
   unsigned allocs, k;

   /* How many allocations the append makes */
   {
      video_coord_array_t ca;
      video_coords_t c;
      memset(&ca, 0, sizeof(ca));
      fail_at = 0;
      if (held)
      {
         c = coords_of(held, false, false);
         video_coord_array_append(&ca, &c, held);
      }
      alloc_calls = 0;
      c = coords_of(N, true, true);
      CHECK(video_coord_array_append(&ca, &c, N), "%s: with memory", name);
      allocs = alloc_calls;
      video_coord_array_free(&ca);
   }

   for (k = 1; k <= allocs; k++)
   {
      video_coord_array_t ca;
      video_coords_t c;
      memset(&ca, 0, sizeof(ca));
      fail_at = 0;
      if (held)
      {
         c = coords_of(held, false, false);
         video_coord_array_append(&ca, &c, held);
      }
      alloc_calls = 0;
      fail_at     = k;
      c           = coords_of(N, true, true);
      CHECK(!video_coord_array_append(&ca, &c, N),
            "%s: allocation %u of %u failed and the append succeeded",
            name, k, allocs);
      fail_at     = 0;
      CHECK(ca.coords.vertices == held,
            "%s: allocation %u of %u failed and the array holds %u "
            "vertices, not %u", name, k, allocs,
            (unsigned)ca.coords.vertices, held);
      if (held)
         CHECK(ca.coords.vertex
               && !memcmp(ca.coords.vertex, vtx, held * 2 * sizeof(float)),
               "%s: allocation %u of %u failed and the vertices held "
               "changed", name, k, allocs);
      video_coord_array_free(&ca);
   }
   printf("%s: ok (%u allocations)\n", name, allocs);
}

int main(void)
{
   unsigned i;
   for (i = 0; i < N * 2; i++) vtx[i] = (float)i + 1.0f;
   for (i = 0; i < N * 4; i++) col[i] = (float)i + 0.5f;
   for (i = 0; i < N * 2; i++) tex[i] = (float)i + 0.25f;

   test_contents();
   sweep("empty_sweep", 0);
   /* 2 held: 2 + N does not fit the 2 allocated, so the append grows */
   sweep("grow_sweep", 2);

   if (failures)
   {
      fprintf(stderr, "%d failure(s)\n", failures);
      return 1;
   }
   printf("coord_array: all passed\n");
   return 0;
}
