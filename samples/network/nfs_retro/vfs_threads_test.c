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
#include "../../../libretro-common/vfs/vfs_implementation_nfs.h"

#define NTHREADS 4
#define ROUNDS   12

static char url_file[600], url_dir[600];
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
   /* read-ahead off, then on: off is one request per read with no
    * prefetcher; on, a read-only open starts one (with threads) */
   for (phase = 0; phase < 2; phase++)
   {
      RFILE *f;
      cfg.readahead = phase ? 1024 : 0;
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
      nfs_shutdown();
   }
   if (failures)
   {
      fprintf(stderr, "FAIL: %d bad operations across %d threads\n", failures, NTHREADS);
      return 1;
   }
   printf("ok: %d threads x %d rounds through the VFS, read-ahead off and on\n",
         NTHREADS, ROUNDS);
   return 0;
}
