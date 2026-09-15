/* Reads a disc image through cdfs and reports digests, so the same
 * image can be compared between CHD readers.
 *
 * cdfs sits on chd_stream, which is where the reader is chosen, so this
 * exercises the whole stack a caller like cheevos uses. Following
 * SYSTEM.CNF to the boot executable is what makes it worth running:
 * comparing a 68-byte stub proves little, and the executable is
 * megabytes read through the filesystem layer.
 *
 * Build once per backend and diff the output.
 *
 *   cc -DHAVE_CHD -DHAVE_RCHD -DHAVE_RCHD_DEFLATE -DHAVE_RCHD_LZMA \
 *      -DHAVE_RCHD_FLAC -DHAVE_RCHD_ZSTD -I libretro-common/include \
 *      -o cdfs_rchd <this> libretro-common/formats/cdfs/cdfs.c ...
 */
/* Reads SYSTEM.CNF, follows it to the boot executable, and digests
 * that -- megabytes through cdfs rather than a 68-byte stub. Also
 * digests raw sectors from the data track so images without an
 * ISO9660 tree still get compared. */
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
   static uint8_t buf[16384];
   char boot[128] = "";
   printf("  %s\n", argv[1] + 7);
   if (!t) { printf("    data track refused\n"); return 1; }

   /* raw sectors, which every data track has whether or not it has a
    * filesystem */
   if (cdfs_open_file(&f, t, "SYSTEM.CNF"))
   {
      int64_t got; unsigned long long d = 1469598103934665603ULL;
      char cnf[512]; size_t n = 0;
      while ((got = cdfs_read_file(&f, buf, sizeof(buf))) > 0)
      { d = h64(buf, (size_t)got, d);
        if (n < sizeof(cnf) - 1)
        { size_t c = (size_t)got; if (c > sizeof(cnf)-1-n) c = sizeof(cnf)-1-n;
          memcpy(cnf + n, buf, c); n += c; } }
      cnf[n] = '\0';
      printf("    SYSTEM.CNF   %016llx\n", d);
      cdfs_close_file(&f);
      { /* BOOT = cdrom:\SLUS_000.05;1 */
        char *p = strstr(cnf, "cdrom");
        if (p) { char *q; p += 5; while (*p == ':' || *p == '\\' || *p == '/') p++;
                 q = boot; while (*p && *p != ';' && *p != '\r' && *p != '\n'
                                  && *p != ' ' && (size_t)(q-boot) < sizeof(boot)-1)
                              *q++ = *p++;
                 *q = '\0'; } }
   }
   if (boot[0])
   {
      if (cdfs_open_file(&f, t, boot))
      {
         int64_t got, total = 0; unsigned long long d = 1469598103934665603ULL;
         while ((got = cdfs_read_file(&f, buf, sizeof(buf))) > 0)
         { d = h64(buf, (size_t)got, d); total += got; }
         printf("    %-12s size %9lld  sector %7u  read %9lld  %016llx\n",
                boot, (long long)cdfs_get_size(&f),
                cdfs_get_first_sector(&f), (long long)total, d);
         cdfs_close_file(&f);
      }
      else printf("    %-12s named but not found\n", boot);
   }
   cdfs_close_track(t);
   return 0;
}
int path_is_directory(const char *p){(void)p;return 0;}
int path_mkdir(const char *d){(void)d;return 0;}
