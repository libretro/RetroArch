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
   /* the read-ahead window: a run of small sequential reads, seeks
    * inside and outside it, a tiny window so its edges are crossed */
   {
      size_t pos = 0, ok = 1;
      rnfs_set_readahead(c, 4096);
      CHECK(rnfs_seek(c, f, 0, 0) == 0, "seek start");
      while (pos < sizeof(big) && ok)
      {
         size_t want = 1000;
         int64_t got = rnfs_read(c, f, small, want > sizeof(small) ? sizeof(small) : want);
         if (got <= 0 || memcmp(small, big + pos, (size_t)got) != 0)
            ok = 0;
         pos += (size_t)got;
      }
      CHECK(ok && pos == sizeof(big), "small sequential reads through the window");
      CHECK(rnfs_seek(c, f, 4090, 0) == 4090 && rnfs_read(c, f, small, 20) == 20
            && memcmp(small, big + 4090, 20) == 0, "read across a window edge");
      CHECK(rnfs_seek(c, f, 150000, 0) == 150000 && rnfs_read(c, f, small, 16) == 16
            && memcmp(small, big + 150000, 16) == 0, "read after a seek out of the window");
      CHECK(rnfs_seek(c, f, 150004, 0) == 150004 && rnfs_read(c, f, small, 8) == 8
            && memcmp(small, big + 150004, 8) == 0, "read inside the window after a seek");
      CHECK(rnfs_seek(c, f, -3, 2) == (int64_t)sizeof(big) - 3 && rnfs_read(c, f, small, 64) == 3
            && memcmp(small, big + sizeof(big) - 3, 3) == 0, "short read at end through the window");
      CHECK(rnfs_read(c, f, small, 64) == 0, "eof through the window");
      rnfs_set_readahead(c, 0);
      CHECK(rnfs_seek(c, f, 7, 0) == 7 && rnfs_read(c, f, small, 9) == 9
            && memcmp(small, big + 7, 9) == 0, "read with read-ahead off");
      rnfs_set_readahead(c, 1024 * 1024);
   }
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

   /* NFSv3 opens in a directory walked recently cost one LOOKUP, not
    * one per component plus a GETATTR; a rename empties the cache so
    * a path through the old name does not resolve to the new one */
   if (argc <= 5 || atoi(argv[5]) != 4)
   {
      uint32_t before, cost;
      CHECK(rnfs_mkdir(c, "rnfs_dir/a") == 0, "mkdir a");
      CHECK(rnfs_mkdir(c, "rnfs_dir/a/b") == 0, "mkdir a/b");
      f = rnfs_open(c, "rnfs_dir/a/b/f.bin", RNFS_O_WRONLY | RNFS_O_CREAT | RNFS_O_TRUNC);
      CHECK(f != NULL, "create a/b/f.bin");
      rnfs_close(c, f);
      f = rnfs_open(c, "rnfs_dir/a/b/f.bin", RNFS_O_RDONLY);
      CHECK(f != NULL, "open a/b/f.bin");
      rnfs_close(c, f);
      before = rnfs_get_call_count(c);
      f = rnfs_open(c, "rnfs_dir/a/b/f.bin", RNFS_O_RDONLY);
      cost = rnfs_get_call_count(c) - before;
      CHECK(f != NULL, "open a/b/f.bin again");
      rnfs_close(c, f);
      printf("open three directories deep, directory known: %u call(s)\n", cost);
      CHECK(cost == 1, "an open in a known directory is one LOOKUP");
      CHECK(rnfs_rename(c, "rnfs_dir/a", "rnfs_dir/a2") == 0, "rename a");
      f = rnfs_open(c, "rnfs_dir/a2/b/f.bin", RNFS_O_RDONLY);
      CHECK(f != NULL, "open through the new name");
      rnfs_close(c, f);
      CHECK(!rnfs_open(c, "rnfs_dir/a/b/f.bin", RNFS_O_RDONLY), "old name gone");
      CHECK(rnfs_unlink(c, "rnfs_dir/a2/b/f.bin") == 0, "unlink a2/b/f.bin");
   }

   {
      retro_time_t t0 = cpu_features_get_time_usec();
      for (i = 0; i < 20; i++)
         CHECK(rnfs_ping(c) == 0, "null");
      printf("null latency: %.2f ms per request\n",
            (cpu_features_get_time_usec() - t0) / 1000.0 / 20);
   }
   if (argc > 5 && atoi(argv[5]) == 4)
      printf("minor version %u\n", rnfs_get_minor_version(c));
   rnfs_free(c);
   printf("ok: %s:%s (%u entries)\n", argv[1], argv[2], n);
   return 0;
}
