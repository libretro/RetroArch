/* Exercises net_nfs3.c against an export: mount, write, read back,
 * seek, stat, mkdir / rename / unlink, list. Exit status is the
 * verdict: nfs_test server export [nfs_port mount_port] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net/net_compat.h>
#include <net/net_nfs3.h>
#include <features/features_cpu.h>

#define CHECK(x, msg) do { if (!(x)) { fprintf(stderr, "FAIL: %s (%s, status %u)\n", msg, rnfs_get_error(c), rnfs_get_status(c)); rnfs_free(c); return 1; } } while (0)

int main(int argc, char **argv)
{
   struct rnfs_ctx *c;
   struct rnfs_file *f;
   struct rnfs_dir *d;
   struct rnfs_stat st;
   const struct rnfs_dirent *e;
   static uint8_t big[200000], back[200000];
   char small[64];
   unsigned i, n = 0, found = 0, sub = 0;

   if (argc < 3)
      return 2;
   network_init();
   c = rnfs_new();
   if (!c)
      return 2;
   rnfs_set_timeout(c, 5);
   if (argc > 4)
      rnfs_set_ports(c, (uint16_t)atoi(argv[3]), (uint16_t)atoi(argv[4]));
   if (argc > 5)
      rnfs_set_version(c, (unsigned)atoi(argv[5]));
   CHECK(rnfs_connect(c, argv[1], argv[2]) == 0, "mount");
   CHECK(rnfs_ping(c) == 0, "null");

   for (i = 0; i < sizeof(big); i++)
      big[i] = (uint8_t)(i * 31 + 7);
   f = rnfs_open(c, "rnfs_test.bin", RNFS_O_WRONLY | RNFS_O_CREAT | RNFS_O_TRUNC);
   CHECK(f, "create");
   CHECK(rnfs_write(c, f, big, sizeof(big)) == (int64_t)sizeof(big), "write");
   CHECK(rnfs_close(c, f) == 0, "close");

   CHECK(rnfs_stat(c, "rnfs_test.bin", &st) == 0 && st.size == sizeof(big) && !st.is_dir, "stat");
   CHECK(rnfs_stat(c, "/", &st) == 0 && st.is_dir, "stat root");
   CHECK(rnfs_stat(c, "no_such", &st) != 0, "stat missing");

   f = rnfs_open(c, "/rnfs_test.bin", RNFS_O_RDONLY);
   CHECK(f, "open");
   CHECK(rnfs_read(c, f, back, sizeof(back)) == (int64_t)sizeof(back), "read");
   CHECK(memcmp(big, back, sizeof(big)) == 0, "read content");
   CHECK(rnfs_read(c, f, small, sizeof(small)) == 0, "read at eof");
   CHECK(rnfs_seek(c, f, 100, 0) == 100, "seek");
   CHECK(rnfs_read(c, f, small, 10) == 10 && memcmp(small, big + 100, 10) == 0, "read after seek");
   CHECK(rnfs_seek(c, f, -5, 2) == (int64_t)sizeof(big) - 5, "seek end");
   CHECK(rnfs_read(c, f, small, 64) == 5, "short read at end");
   rnfs_close(c, f);

   CHECK(rnfs_mkdir(c, "rnfs_dir") == 0, "mkdir");
   CHECK(rnfs_rename(c, "rnfs_test.bin", "rnfs_dir/moved.bin") == 0, "rename");
   CHECK(rnfs_stat(c, "rnfs_dir/moved.bin", &st) == 0 && st.size == sizeof(big), "stat moved");
   f = rnfs_open(c, "rnfs_dir/moved.bin", RNFS_O_RDWR);
   CHECK(f && rnfs_ftruncate(c, f, 1000) == 0, "ftruncate");
   rnfs_close(c, f);
   CHECK(rnfs_stat(c, "rnfs_dir/moved.bin", &st) == 0 && st.size == 1000, "stat after truncate");

   d = rnfs_opendir(c, "");
   CHECK(d, "opendir");
   while ((e = rnfs_readdir(c, d)))
   {
      n++;
      if (strcmp(e->name, "rnfs_dir") == 0 && e->st.is_dir)
         sub = 1;
   }
   rnfs_closedir(c, d);
   d = rnfs_opendir(c, "rnfs_dir");
   CHECK(d, "opendir sub");
   while ((e = rnfs_readdir(c, d)))
      if (strcmp(e->name, "moved.bin") == 0 && e->st.size == 1000)
         found = 1;
   rnfs_closedir(c, d);
   CHECK(sub && found, "readdir");

   CHECK(rnfs_unlink(c, "rnfs_dir/moved.bin") == 0, "unlink");
   CHECK(rnfs_stat(c, "rnfs_dir/moved.bin", &st) != 0, "gone");
   CHECK(!rnfs_open(c, "no_such", RNFS_O_RDONLY), "open missing");

   {
      retro_time_t t0 = cpu_features_get_time_usec();
      for (i = 0; i < 20; i++)
         CHECK(rnfs_ping(c) == 0, "null");
      printf("null latency: %.2f ms per request\n",
            (cpu_features_get_time_usec() - t0) / 1000.0 / 20);
   }
   rnfs_free(c);
   printf("ok: %s:%s (%u entries)\n", argv[1], argv[2], n);
   return 0;
}
