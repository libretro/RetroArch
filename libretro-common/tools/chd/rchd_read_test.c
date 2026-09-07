/* Reads every hunk of an image through rchd and compares against the
 * same hunk decoded by an independent reader. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/rchd.h>

static FILE *g_f;
static int pull(rchd_t *c, rchd_request_t *r)
{
   static uint8_t buf[262144];
   size_t take = r->length > sizeof(buf) ? sizeof(buf) : r->length, got;
   if (fseek(g_f, (long)r->offset, SEEK_SET) != 0) return 0;
   got = fread(buf, 1, take, g_f);
   if (!got) return 0;
   return rchd_feed(c, buf, got) == RCHD_OK;
}

int main(int argc, char **argv)
{
   rchd_t *c; rchd_request_t req; int e;
   const rchd_info_t *info; uint8_t *hunk; uint32_t n, ok = 0, bad = 0;
   FILE *ref = NULL;

   if (argc < 2) return 2;
   if (!(g_f = fopen(argv[1], "rb"))) return 1;
   if (argc > 2) ref = fopen(argv[2], "rb");
   if (!(c = rchd_new())) return 1;

   while ((e = rchd_open_step(c, &req)) == RCHD_PENDING)
      if (!pull(c, &req)) { e = -1; break; }
   if (e != RCHD_OK) { printf("  open failed %d\n", e); return 1; }

   info = rchd_info(c);
   hunk = (uint8_t*)malloc(info->hunk_bytes);

   for (n = 0; n < info->hunk_count; n++)
   {
      if ((e = rchd_read_hunk_begin(c, n, hunk)) != RCHD_OK) { bad++; continue; }
      while ((e = rchd_read_step(c, &req)) == RCHD_PENDING)
         if (!pull(c, &req)) break;
      if (e != RCHD_OK) { bad++; continue; }
      if (ref)
      {
         static uint8_t exp[1024*1024];
         size_t want = info->hunk_bytes;
         if ((uint64_t)n * want < (uint64_t)info->logical_bytes)
         {
            fseek(ref, (long)((uint64_t)n * want), SEEK_SET);
            memset(exp, 0, want);
            fread(exp, 1, want, ref);
            if (memcmp(hunk, exp, want) != 0) { bad++; continue; }
         }
      }
      ok++;
   }
   printf("  %-44s hunks ok=%-6u bad=%-4u %s\n", argv[1], ok, bad,
          bad ? "FAIL" : "PASS");
   free(hunk); rchd_free(c); fclose(g_f); if (ref) fclose(ref);
   return bad ? 1 : 0;
}
