/* Several threads through the nfs:// VFS backend at once - open, read,
 * seek, stat and list on the same server - built under ThreadSanitizer
 * by the matrix. Same arguments as vfs_test. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <boolean.h>
#include <net/net_compat.h>
#include <streams/file_stream.h>
#include <vfs/vfs.h>
#include <file/file_path.h>
#include <retro_dirent.h>
#include <rthreads/rthreads.h>
#include <retro_timers.h>
#include "../../../libretro-common/vfs/vfs_implementation_nfs.h"

#define NTHREADS 4
#define ROUNDS   12

static char url_file[600], url_dir[600], url_big[600];
static int  failures = 0;

static void worker(void *arg)
{
   int i, bad = 0;
   uint8_t buf[64];
   (void)arg;
   for (i = 0; i < ROUNDS; i++)
   {
      RFILE *f = filestream_open(url_file, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
      struct RDIR *d;
      if (!f || filestream_seek(f, 8 * (i % 4), RETRO_VFS_SEEK_POSITION_START) != 0
            || filestream_read(f, buf, sizeof(buf)) != (int64_t)sizeof(buf) || buf[0] != 0x5a)
         bad++;
      if (f)
         filestream_close(f);
      if (!path_is_valid(url_file))
         bad++;
      if ((d = retro_opendir(url_dir)))
      {
         while (retro_readdir(d))
            (void)retro_dirent_get_name(d);
         retro_closedir(d);
      }
      else
         bad++;
   }
   if (bad)
      __sync_fetch_and_add(&failures, bad);
}

int main(int argc, char **argv)
{
   struct nfs_settings cfg;
   sthread_t *t[NTHREADS];
   int i, phase;

   if (argc < 3)
      return 2;
   network_init();
   memset(&cfg, 0, sizeof(cfg));
   cfg.server_address = argv[1];
   cfg.export_path    = argv[2];
   cfg.timeout        = 5;
   cfg.num_contexts   = 2;   /* fewer slots than threads: the pool is shared */
   if (argc > 4)
   {
      cfg.nfs_port   = (unsigned)atoi(argv[3]);
      cfg.mount_port = (unsigned)atoi(argv[4]);
   }
   if (argc > 5)
      cfg.version = (unsigned)atoi(argv[5]);
   snprintf(url_file, sizeof(url_file), "nfs://%s/rnfs_dir/vfs.bin", argv[1]);
   snprintf(url_dir, sizeof(url_dir), "nfs://%s/rnfs_dir", argv[1]);
   snprintf(url_big, sizeof(url_big), "nfs://%s/rnfs_dir/vfs_big.bin", argv[1]);
   /* read-ahead off, then on: off is one request per read with no
    * prefetcher; on, a read-only open starts one (with threads), once
    * with a window larger than the file and once with windows far
    * smaller than one read */
   for (phase = 0; phase < 3; phase++)
   {
      RFILE *f;
      cfg.readahead = phase == 0 ? 0 : phase == 1 ? 1024 : 1;
      nfs_init_cfg(&cfg);
      for (i = 0; i < NTHREADS; i++)
         t[i] = sthread_create(worker, (void*)(intptr_t)i);
      for (i = 0; i < NTHREADS; i++)
         if (t[i])
            sthread_join(t[i]);
      if ((f = filestream_open(url_file, RETRO_VFS_FILE_ACCESS_READ,
                  RETRO_VFS_FILE_ACCESS_HINT_NONE)))
      {
         libretro_vfs_implementation_file *h = filestream_get_vfs_handle(f);
         bool started = h && h->nfs_prefetch != 0;
         if (started != (phase != 0))
         {
            fprintf(stderr, "FAIL: read-ahead %u KiB %s a prefetcher\n",
                  cfg.readahead, started ? "started" : "did not start");
            failures++;
         }
         filestream_close(f);
      }
      /* the whole file read in order, then closed: read-ahead's counts
       * add up to it when on, and stay empty when off */
      if ((f = filestream_open(url_file, RETRO_VFS_FILE_ACCESS_READ,
                  RETRO_VFS_FILE_ACCESS_HINT_NONE)))
      {
         static uint8_t chunk[4096];
         uint64_t read_total = 0;
         unsigned window_kib, direct_kib, want_kib;
         int64_t  got;
         while ((got = filestream_read(f, chunk, sizeof(chunk))) > 0)
            read_total += (uint64_t)got;
         filestream_close(f);
         nfs_take_readahead_stats(&window_kib, &direct_kib);
         want_kib = (unsigned)((read_total + 512) >> 10);
         if (!phase && (window_kib || direct_kib))
         {
            fprintf(stderr, "FAIL: read-ahead off still counted %u + %u KiB\n",
                  window_kib, direct_kib);
            failures++;
         }
         if (phase && (window_kib + direct_kib + 1 < want_kib
                  || window_kib + direct_kib > want_kib + 1))
         {
            fprintf(stderr, "FAIL: read-ahead counted %u + %u KiB of %u KiB read\n",
                  window_kib, direct_kib, want_kib);
            failures++;
         }
      }
      /* one read reaching past the ready windows: it still gets every
       * octet it asked for, the rest read from the network, as a
       * parser checking for the exact length expects */
      if (phase == 2)
      {
         static uint8_t whole[200000];
         unsigned window_kib, direct_kib;
         int64_t  got = -1;
         uint32_t j;
         for (j = 0; j < sizeof(whole); j++)
            whole[j] = (uint8_t)(j * 31 + 7);
         if ((f = filestream_open(url_big, RETRO_VFS_FILE_ACCESS_WRITE,
                     RETRO_VFS_FILE_ACCESS_HINT_NONE)))
         {
            got = filestream_write(f, whole, sizeof(whole));
            filestream_close(f);
         }
         memset(whole, 0, sizeof(whole));
         if (got == (int64_t)sizeof(whole) && (f = filestream_open(url_big,
                     RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE)))
         {
            /* a first octet sends the prefetcher to the start of the file;
             * the rest is then asked for in one read, past its windows */
            got = filestream_read(f, whole, 1);
            retro_sleep(200);
            if (got == 1)
               got += filestream_read(f, whole + 1, sizeof(whole) - 1);
            filestream_close(f);
         }
         else
            got = -1;
         filestream_delete(url_big);
         nfs_take_readahead_stats(&window_kib, &direct_kib);
         if (got != (int64_t)sizeof(whole))
         {
            fprintf(stderr, "FAIL: read past the windows got %lld of %u\n",
                  (long long)got, (unsigned)sizeof(whole));
            failures++;
         }
         else
            for (j = 0; j < sizeof(whole); j++)
               if (whole[j] != (uint8_t)(j * 31 + 7))
               {
                  fprintf(stderr, "FAIL: read past the windows, octet %u\n", j);
                  failures++;
                  break;
               }
         if (got == (int64_t)sizeof(whole) && !window_kib)
         {
            fprintf(stderr, "FAIL: read past the windows took nothing from them (%u KiB direct)\n", direct_kib);
            failures++;
         }
      }
      /* a file lent where read-ahead holds it and read where it does
       * not, as a hash loop takes it: every octet in order, some lent
       * with read-ahead on and none with it off, and a read after the
       * lends picks up where they stopped. A first octet sends the
       * prefetcher to the start, as above; on a loopback the reader's
       * own reads would otherwise outrun it to the end of the file. */
      {
         static uint8_t whole[200000], chunk[4096];
         uint64_t at = 1, lent = 0;
         bool     ok = false;
         uint8_t  tail[16];
         uint32_t j;
         for (j = 0; j < sizeof(whole); j++)
            whole[j] = (uint8_t)(j * 31 + 7);
         if ((f = filestream_open(url_big, RETRO_VFS_FILE_ACCESS_WRITE,
                     RETRO_VFS_FILE_ACCESS_HINT_NONE)))
         {
            ok = filestream_write(f, whole, sizeof(whole)) == (int64_t)sizeof(whole);
            filestream_close(f);
         }
         if (ok && (f = filestream_open(url_big, RETRO_VFS_FILE_ACCESS_READ,
                     RETRO_VFS_FILE_ACCESS_HINT_NONE)))
         {
            if (filestream_read(f, tail, 1) != 1 || tail[0] != 7)
               ok = false;
            retro_sleep(200);
            /* all but the last 16, which the read after takes */
            while (ok && at + 16 < sizeof(whole))
            {
               const uint8_t *ptr  = NULL;
               int64_t        rest = (int64_t)(sizeof(whole) - 16 - at);
               int64_t        n    = filestream_view(f, rest, &ptr);
               if (n < 0)
               {
                  n   = filestream_read(f, chunk,
                        rest < (int64_t)sizeof(chunk) ? rest : (int64_t)sizeof(chunk));
                  ptr = chunk;
               }
               else
                  lent += (uint64_t)n;
               if (n <= 0 || memcmp(ptr, whole + at, (size_t)n))
                  ok = false;
               at += (uint64_t)n;
            }
            if (ok && (filestream_read(f, tail, 16) != 16
                     || memcmp(tail, whole + at, 16)))
               ok = false;
            filestream_close(f);
         }
         else
            ok = false;
         filestream_delete(url_big);
         /* taken so the next phase counts only its own streams */
         {
            unsigned window_kib, direct_kib;
            nfs_take_readahead_stats(&window_kib, &direct_kib);
         }
         if (!ok)
         {
            fprintf(stderr, "FAIL: read through lends, read-ahead %u KiB\n",
                  cfg.readahead);
            failures++;
         }
         if ((lent != 0) != (phase != 0))
         {
            fprintf(stderr, "FAIL: read-ahead %u KiB lent %llu octets\n",
                  cfg.readahead, (unsigned long long)lent);
            failures++;
         }
      }
      /* a read-only open asking to be mapped, as the archive layer's
       * does: a network stream has no descriptor to map, so it must still
       * open, seek and read through its own backend */
      {
         RFILE *mf = filestream_open(url_file, RETRO_VFS_FILE_ACCESS_READ,
               RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS);
         uint8_t buf2[16];
         if (     !mf
               || filestream_get_size(mf) <= 0
               || filestream_seek(mf, 100, RETRO_VFS_SEEK_POSITION_START) != 0
               || filestream_read(mf, buf2, sizeof(buf2)) != (int64_t)sizeof(buf2)
               || buf2[0] != 0x5a)
         {
            fprintf(stderr, "FAIL: open asking for a mapping\n");
            failures++;
         }
         if (mf)
            filestream_close(mf);
      }
      nfs_shutdown();
   }
   if (failures)
   {
      fprintf(stderr, "FAIL: %d bad operations across %d threads\n", failures, NTHREADS);
      return 1;
   }
   printf("ok: %d threads x %d rounds through the VFS, read-ahead off and on, reads past the windows\n",
         NTHREADS, ROUNDS);
   return 0;
}
