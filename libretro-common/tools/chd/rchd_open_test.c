/* Opens real CHD images through rchd and reports the geometry, map and
 * metadata it finds.
 *
 *   cc -I libretro-common/include -o rchd_open_test \
 *      libretro-common/tools/chd/rchd_open_test.c \
 *      libretro-common/formats/chd/rchd.c \
 *      libretro-common/encodings/encoding_huffman.c \
 *      libretro-common/encodings/encoding_crc32.c
 *
 * Codecs are optional: without HAVE_RCHD_* an image opens but its hunks
 * do not decode, which is all this exercises. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/rchd.h>

int main(int argc, char **argv)
{
   FILE *f; rchd_t *chd; rchd_request_t req; int e;
   const rchd_info_t *info;
   uint8_t buf[65536];
   unsigned long rounds = 0;

   if (argc < 2) return 2;
   if (!(f = fopen(argv[1], "rb"))) return 1;
   if (!(chd = rchd_new())) return 1;

   for (;;)
   {
      size_t take, got;
      e = rchd_open_step(chd, &req);
      if (e != RCHD_PENDING) break;
      if (++rounds > 100000) { printf("  LOOP at off=%lu len=%u\n", (unsigned long)req.offset, req.length); e=-97; break; }
      take = req.length > sizeof(buf) ? sizeof(buf) : req.length;
      if (fseek(f, (long)req.offset, SEEK_SET) != 0) { e = -99; break; }
      got = fread(buf, 1, take, f);
      if (!got) { e = -98; break; }
      rchd_feed(chd, buf, got);
   }

   if (e != RCHD_OK)
   {
      printf("  %-44s open failed (%d)\n", argv[1], e);
      rchd_free(chd); fclose(f); return 1;
   }

   info = rchd_info(chd);
   printf("  %-44s v%u hunks=%-6u hunk=%-6u unit=%-5u meta=%-2u rounds=%lu\n",
          argv[1], info->version, info->hunk_count, info->hunk_bytes,
          info->unit_bytes, rchd_metadata_count(chd), rounds);
   rchd_free(chd); fclose(f);
   return 0;
}
