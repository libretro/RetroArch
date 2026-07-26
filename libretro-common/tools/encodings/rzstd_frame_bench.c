/* Times rzstd against the reference on real frames, given as
 * arguments.
 *
 * Frame size decides which way this goes, so the size has to be the
 * one a caller actually sees: a CHD hunk is two to twenty kilobytes,
 * where per-call setup dominates, and a synthetic four-megabyte frame
 * measures something no caller in this tree decodes.
 */
/* Times rzstd against the reference on real frames taken from
 * Zstandard-compressed disc images. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <encodings/rzstd.h>
#include <zstd.h>
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);
  return t.tv_sec+t.tv_nsec*1e-9;}
int main(int argc, char **argv)
{
   static uint8_t *fr[600]; static size_t fl[600]; static size_t ol[600];
   static uint8_t out[1<<20];
   int n = 0, i, reps = 200, r;
   double t0, trz, tzs; size_t total = 0;
   for (i = 1; i < argc && n < 600; i++)
   { FILE *f = fopen(argv[i], "rb"); long m;
     if (!f) continue;
     fseek(f,0,SEEK_END); m = ftell(f); fseek(f,0,SEEK_SET);
     fr[n] = malloc((size_t)m); fl[n] = (size_t)m;
     if (fread(fr[n],1,(size_t)m,f) != (size_t)m) { fclose(f); continue; }
     fclose(f);
     ol[n] = ZSTD_getFrameContentSize(fr[n], fl[n]);
     total += ol[n]; n++; }
   printf("  %d real frames, %lu bytes of content\n", n, (unsigned long)total);
   t0 = now();
   for (r = 0; r < reps; r++) for (i = 0; i < n; i++)
      rzstd_decode(out, sizeof(out), fr[i], fl[i], NULL);
   trz = (now()-t0)/reps;
   t0 = now();
   for (r = 0; r < reps; r++) for (i = 0; i < n; i++)
      ZSTD_decompress(out, sizeof(out), fr[i], fl[i]);
   tzs = (now()-t0)/reps;
   printf("  rzstd %7.1f MB/s   reference %7.1f MB/s   %.2fx\n",
          total/trz/1e6, total/tzs/1e6, tzs/trz);
   return 0;
}
