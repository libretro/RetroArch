/* Drives an open and a read entirely through the offset-identified
 * supply calls, borrowing rather than copying where the whole request
 * is already resident. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/rchd.h>

static uint8_t *img;
static size_t   img_len;
static unsigned long borrowed, copied;

static int supply(rchd_t *c)
{
   rchd_request_t r;
   if (rchd_read_pending(c, &r, 1) != 1)
      return 0;
   if (r.source == RCHD_SOURCE_SELF && r.offset + r.length <= img_len
         && rchd_feed_borrow(c, r.offset, r.source,
               img + r.offset, r.length) == RCHD_OK)
   { borrowed++; return 1; }
   if (r.offset + r.length > img_len)
      return 0;
   rchd_feed_at(c, r.offset, r.source, img + r.offset, r.length);
   copied++;
   return 1;
}

int main(int argc, char **argv)
{
   FILE *f; rchd_t *c; rchd_request_t req; const rchd_info_t *i;
   uint8_t *hunk; uint32_t n, bad = 0; int e;

   f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); img_len = ftell(f);
   fseek(f, 0, SEEK_SET); img = malloc(img_len);
   if (fread(img, 1, img_len, f) != img_len) return 1;
   fclose(f);

   c = rchd_new();
   while ((e = rchd_open_step(c, &req)) == RCHD_PENDING)
      if (!supply(c)) break;
   if (e != RCHD_OK) { printf("  open failed %d\n", e); return 1; }

   i = rchd_info(c); hunk = malloc(i->hunk_bytes);
   for (n = 0; n < 200 && n < i->hunk_count; n++)
   {
      if (rchd_read_hunk_begin(c, n, hunk) != RCHD_OK) { bad++; continue; }
      while ((e = rchd_read_step(c, &req)) == RCHD_PENDING)
         if (!supply(c)) break;
      if (e != RCHD_OK)
      {
         if (bad < 3) printf("    hunk %u failed: step=%d\n", n, e);
         bad++;
      }
   }
   printf("  %-42s 200 hunks: bad=%u borrowed=%lu copied=%lu\n",
          argv[1], bad, borrowed, copied);
   printf("    depth 1 -> %d  depth 2 -> %d  depth 0 -> %d\n",
          rchd_set_pipeline_depth(c, 1), rchd_set_pipeline_depth(c, 2),
          rchd_set_pipeline_depth(c, 0));
   rchd_free(c); free(hunk); free(img);
   return bad ? 1 : 0;
}
int path_is_directory(const char *p) { (void)p; return 0; }
int path_mkdir(const char *d) { (void)d; return 0; }
