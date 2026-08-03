/* Differential harness for rh264: decode an Annex-B .h264 elementary
 * stream and hash every ready picture's I420 planes.  Access units are
 * grouped by starting a new AU when a slice NAL arrives and the
 * pending group already holds a slice. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <formats/rh264.h>

static uint32_t fnv1a(uint32_t h, const void *p, size_t n)
{
   const uint8_t *b = (const uint8_t *)p;
   size_t i;
   for (i = 0; i < n; i++) { h ^= b[i]; h *= 0x01000193u; }
   return h;
}

static uint32_t g_hash = 0x811c9dc5u;
static int g_pics = 0;

static void hash_pic(rh264_video *v)
{
   int pl;
   for (pl = 0; pl < 3; pl++)
   {
      int stride, w, h, y;
      const uint8_t *p = rh264_video_plane(v, pl, &stride, &w, &h);
      if (!p) return;
      for (y = 0; y < h; y++)
         g_hash = fnv1a(g_hash, p + (size_t)y * stride, (size_t)w);
   }
   g_pics++;
}

int main(int argc, char **argv)
{
   FILE *f; long n; uint8_t *d;
   size_t *starts; size_t nnal = 0, i, cap = 1 << 16;
   size_t au_begin = 0; int au_has_slice = 0;
   rh264_video *v;

   if (argc < 2) return 2;
   if (!(f = fopen(argv[1], "rb"))) return 2;
   fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
   d = (uint8_t *)malloc((size_t)n);
   if (fread(d, 1, (size_t)n, f) != (size_t)n) return 2;
   fclose(f);

   starts = (size_t *)malloc(cap * sizeof(size_t));
   for (i = 0; i + 3 < (size_t)n; i++)
      if (d[i] == 0 && d[i+1] == 0 && d[i+2] == 1)
      { starts[nnal++] = i; i += 2; }
   starts[nnal] = (size_t)n;
   if (!nnal) return 2;

   v = rh264_video_open();
   if (!v) return 3;

   au_begin = starts[0];
   for (i = 0; i < nnal; i++)
   {
      int type = d[starts[i] + 3] & 0x1f;
      int is_slice = (type == 1 || type == 5);
      if (is_slice && au_has_slice)
      {
         int r = rh264_video_decode(v, d + au_begin, starts[i] - au_begin);
         if (r < 0) { fprintf(stderr, "decode err at nal %zu\n", i); return 1; }
         if (r == 1) hash_pic(v);
         au_begin = starts[i];
         au_has_slice = 0;
      }
      if (is_slice) au_has_slice = 1;
   }
   {
      int r = rh264_video_decode(v, d + au_begin, (size_t)n - au_begin);
      if (r < 0) { fprintf(stderr, "decode err last\n"); return 1; }
      if (r == 1) hash_pic(v);
   }
   /* drain reordering: some decoders expose a flush; if not, held
    * pictures at EOS are simply not compared, identically on both
    * builds. */
   printf("%s pics=%d hash=%08x\n", argv[1], g_pics, g_hash);
   rh264_video_close(v);
   free(d); free(starts);
   return g_pics > 0 ? 0 : 1;
}
