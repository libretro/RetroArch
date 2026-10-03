/* Wii SD card and FAT checks through the C library, run in Dolphin by
 * os/gekko/tests/run-dolphin.sh with SDIMG naming an image that
 * sd-image.sh made.  The files this leaves are checked on the host
 * afterwards. */

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#include <gekko/disk.h>
#include <gekko/power.h>
#include <gekko/thread.h>

static unsigned failures;

#define CHECK(cond, what) \
   do { \
      if (cond) \
         gk_debug_printf("ok   %s", what); \
      else \
      { \
         gk_debug_printf("FAIL %s", what); \
         failures++; \
      } \
   } while (0)

#define BIG (4u << 20)

static uint8_t pattern(uint32_t seed, uint32_t i)
{
   return (uint8_t)(((seed + i) * 2654435761u) >> 24);
}

static int file_is(const char *path, const char *want)
{
   char buf[64];
   size_t n;
   FILE *f = fopen(path, "rb");
   if (!f)
      return 0;
   n = fread(buf, 1, sizeof(buf) - 1, f);
   fclose(f);
   buf[n] = '\0';
   return !strcmp(buf, want);
}

static void test_read(void)
{
   struct stat st;
   DIR *d;
   struct dirent *de;
   unsigned n = 0, long_seen = 0;
   char what[96];

   CHECK(file_is("sd:/hello.txt", "hello\n"), "read a short name");
   CHECK(file_is("sd:/A Long File Name.data", "long\n"), "read a long name");
   CHECK(file_is("sd:/dir/sub/deep name.txt", "deep\n"),
         "read through directories");
   CHECK(file_is("/hello.txt", "hello\n"), "the default device");
   CHECK(stat("sd:/dir", &st) == 0 && S_ISDIR(st.st_mode), "stat a directory");
   CHECK(stat("sd:/hello.txt", &st) == 0 && S_ISREG(st.st_mode)
         && st.st_size == 6, "stat a file");
   CHECK(stat("sd:/missing", &st) == -1 && errno == ENOENT, "stat ENOENT");

   if ((d = opendir("sd:/dir/many")))
   {
      while ((de = readdir(d)))
      {
         n++;
         if (!strcmp(de->d_name, "file with long name 99.txt"))
            long_seen = 1;
      }
      closedir(d);
   }
   snprintf(what, sizeof(what), "readdir: %u entries", n);
   CHECK(n == 100 && long_seen, what);

   CHECK(chdir("sd:/dir/sub") == 0 && file_is("deep name.txt", "deep\n"),
         "a relative path after chdir");
   chdir("sd:/");
}

static void test_write(void)
{
   uint8_t *buf = (uint8_t*)malloc(BIG);
   struct statvfs vfs;
   uint64_t t0, t1;
   uint32_t i;
   FILE *f;
   size_t n;
   int ok;
   char what[96];

   CHECK(mkdir("sd:/out", 0777) == 0, "mkdir");
   CHECK(mkdir("sd:/out", 0777) == -1 && errno == EEXIST, "mkdir EEXIST");
   f = fopen("sd:/out/Written by the Wii.txt", "w");
   CHECK(f != NULL, "create a long name");
   if (f)
   {
      fputs("written\n", f);
      fclose(f);
   }
   CHECK(file_is("sd:/out/written by the wii.TXT", "written\n"),
         "read it back, any case");

   for (i = 0; i < BIG; i++)
      buf[i] = pattern(5, i);
   t0 = gk_ticks();
   f  = fopen("sd:/out/big.bin", "wb");
   n  = f ? fwrite(buf, 1, BIG, f) : 0;
   ok = f && !fclose(f) && n == BIG;
   t1 = gk_ticks();
   snprintf(what, sizeof(what), "write 4 MiB (%.1f MiB/s)",
         4.0 * gk_tb_hz / (double)(t1 - t0));
   CHECK(ok, what);

   memset(buf, 0, BIG);
   t0 = gk_ticks();
   f  = fopen("sd:/out/big.bin", "rb");
   /* setvbuf off so the reads reach the driver whole. */
   if (f)
      setvbuf(f, NULL, _IONBF, 0);
   n  = f ? fread(buf, 1, BIG, f) : 0;
   if (f)
      fclose(f);
   t1 = gk_ticks();
   for (ok = n == BIG, i = 0; ok && i < BIG; i++)
      ok = buf[i] == pattern(5, i);
   snprintf(what, sizeof(what), "read 4 MiB back (%.1f MiB/s)",
         4.0 * gk_tb_hz / (double)(t1 - t0));
   CHECK(ok, what);

   /* Unaligned buffers go through the bounce buffer. */
   f = fopen("sd:/out/big.bin", "rb");
   if (f)
      setvbuf(f, NULL, _IONBF, 0);
   ok = f && fseek(f, 512, SEEK_SET) == 0
      && fread(buf + 3, 1, 8192, f) == 8192;
   if (f)
      fclose(f);
   for (i = 0; ok && i < 8192; i++)
      ok = buf[3 + i] == pattern(5, 512 + i);
   CHECK(ok, "unaligned read");

   f = fopen("sd:/out/trunc", "w");
   ok = f && fwrite(buf, 1, 10000, f) == 10000 && fflush(f) == 0
      && ftruncate(fileno(f), 100) == 0;
   if (f)
      fclose(f);
   {
      struct stat st;
      CHECK(ok && stat("sd:/out/trunc", &st) == 0 && st.st_size == 100,
            "ftruncate");
   }

   f = fopen("sd:/out/temp", "w");
   if (f)
   {
      fputs("renamed\n", f);
      fclose(f);
   }
   CHECK(rename("sd:/out/temp", "sd:/out/final name.txt") == 0
         && file_is("sd:/out/final name.txt", "renamed\n"), "rename");
   f = fopen("sd:/out/gone", "w");
   if (f)
      fclose(f);
   CHECK(remove("sd:/out/gone") == 0 && access("sd:/out/gone", F_OK) != 0,
         "remove");
   CHECK(unlink("sd:/hello.txt") == 0, "unlink a file the host made");
   CHECK(mkdir("sd:/out/empty", 0777) == 0 && rmdir("sd:/out/empty") == 0,
         "rmdir");
   CHECK(statvfs("sd:/", &vfs) == 0 && vfs.f_bfree > 0
         && vfs.f_bfree < vfs.f_blocks, "statvfs");
   free(buf);
}

/* What is open when the volume goes fails until closed; the same name
 * mounts again. */
static void test_unplug(gk_blockdev_t *dev)
{
   FILE *f = fopen("sd:/out/final name.txt", "rb");
   DIR  *d = opendir("sd:/out");
   char c;
   CHECK(f && d, "open across an unmount");
   gk_fat_unmount("sd");
   CHECK(access("sd:/out", F_OK) != 0, "unmounted");
   if (f)
   {
      CHECK(fread(&c, 1, 1, f) == 0 && ferror(f), "a read after it fails");
      fclose(f);
   }
   if (d)
   {
      CHECK(readdir(d) == NULL, "a listing after it fails");
      closedir(d);
   }
   CHECK(gk_sd_inserted(), "the card is still in");
   CHECK(gk_fat_mount("sd", dev) == 0 && access("sd:/out", F_OK) == 0,
         "mounted again");
}

int main(int argc, char **argv)
{
   gk_blockdev_t *dev;
   int ret;
   char what[96];
   (void)argc;
   (void)argv;

   dev = gk_sd_open();
   snprintf(what, sizeof(what), "SD card: %lu sectors",
         dev ? (unsigned long)dev->sectors : 0ul);
   CHECK(dev && dev->sectors >= (60ull << 20) / 512, what);
   ret = dev ? gk_fat_mount("sd", dev) : -ENODEV;
   snprintf(what, sizeof(what), "mount: %d", ret);
   CHECK(ret == 0, what);
   if (!ret)
   {
      test_read();
      test_write();
      test_unplug(dev);
      gk_fat_unmount("sd");
      CHECK(access("sd:/out", F_OK) != 0, "unmounted");
   }
   gk_sd_close();
   gk_debug_printf("%s (%u failure(s))", failures ? "FAILED" : "PASSED",
         failures);
   gk_power_off();
   return 0;
}
