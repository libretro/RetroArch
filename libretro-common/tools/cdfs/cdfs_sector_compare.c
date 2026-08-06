/* The path RetroAchievements hashes a disc through: open the whole
 * track with a NULL filename, then seek by sector and read.
 *
 * This is a different cdfs path from reading a file sequentially, and
 * cdfs_backend_compare.c does not reach it. cheevos_rvz aside, every
 * hash of a CHD goes through cdfs_seek_sector and cdfs_read_file in
 * rc_hash_handle_chd_read_sector, so a reader that agrees on file
 * contents and disagrees on seeks would still produce wrong hashes.
 *
 * The sector order is deliberately scattered rather than sequential.
 *
 * Build once per backend and diff the output.
 */
/* Exactly what cheevos does: open a track, then seek by sector and read,
 * which is a different cdfs path from sequential file reads. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/cdfs.h>
static unsigned long long h64(const uint8_t *p, size_t n, unsigned long long h)
{ size_t i; for (i = 0; i < n; i++) h = (h ^ p[i]) * 1099511628211ULL; return h; }
int main(int argc, char **argv)
{
   cdfs_track_t *t = cdfs_open_data_track(argv[1]);
   cdfs_file_t f;
   static uint8_t buf[2048];
   unsigned long long d = 1469598103934665603ULL;
   uint32_t first, nsec, s, hit = 0;
   printf("  %s\n", argv[1] + 7);
   if (!t) { printf("    data track refused\n"); return 1; }
   /* NULL path opens the whole track, which is what cheevos does */
   if (!cdfs_open_file(&f, t, NULL))
   { printf("    track open refused\n"); cdfs_close_track(t); return 0; }
   first = cdfs_get_first_sector(&f);
   nsec  = cdfs_get_num_sectors(&f);
   printf("    first_sector %u  num_sectors %u\n", first, nsec);
   /* read sectors the way rc_hash_handle_chd_read_sector does, in a
    * scattered order so seeking is genuinely exercised */
   for (s = 0; s < nsec && s < 4096; s++)
   {
      uint32_t k = (s * 2654435761u) % (nsec < 4096 ? nsec : 4096);
      size_t got;
      cdfs_seek_sector(&f, k);
      got = (size_t)cdfs_read_file(&f, buf, sizeof(buf));
      if (got) { d = h64(buf, got, d); hit++; }
   }
   printf("    %u scattered sector reads  %016llx\n", hit, d);
   cdfs_close_file(&f);
   cdfs_close_track(t);
   return 0;
}
int path_is_directory(const char *p){(void)p;return 0;}
int path_mkdir(const char *d){(void)d;return 0;}
