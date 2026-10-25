/* Several threads through the smb:// VFS backend at once - open, read,
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
#include "../../../libretro-common/vfs/vfs_implementation_smb.h"

#define NTHREADS 4
#define ROUNDS   12

static char url_file[600], url_dir[600];
static int  failures = 0;

/* Each failure names its operation, thread and round, so a failure on
 * a CI runner says what went wrong rather than only how often. */
static void report(int id, int round, const char *what)
{
   fprintf(stderr, "thread %d round %d: %s failed\n", id, round, what);
}

static void worker(void *arg)
{
   int i, bad = 0;
   int id = (int)(intptr_t)arg;
   uint8_t buf[64];
   for (i = 0; i < ROUNDS; i++)
   {
      RFILE *f = filestream_open(url_file, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
      struct RDIR *d;
      int64_t off = 8 * (i % 4);
      if (!f)
      {
         report(id, i, "open");
         bad++;
      }
      else if (filestream_seek(f, off, RETRO_VFS_SEEK_POSITION_START) != 0)
      {
         report(id, i, "seek");
         bad++;
      }
      else
      {
         int64_t got = filestream_read(f, buf, sizeof(buf));
         if (got != (int64_t)sizeof(buf))
         {
            fprintf(stderr, "thread %d round %d: read at %d returned %d of %d\n",
                  id, i, (int)off, (int)got, (int)sizeof(buf));
            bad++;
         }
         else if (buf[0] != (uint8_t)(off * 31 + 7))
         {
            fprintf(stderr, "thread %d round %d: read at %d gave byte 0x%02x, want 0x%02x\n",
                  id, i, (int)off, buf[0], (uint8_t)(off * 31 + 7));
            bad++;
         }
      }
      if (f)
         filestream_close(f);
      if (!path_is_valid(url_file))
      {
         report(id, i, "stat");
         bad++;
      }
      if ((d = retro_opendir(url_dir)))
      {
         while (retro_readdir(d))
            (void)retro_dirent_get_name(d);
         retro_closedir(d);
      }
      else
      {
         report(id, i, "opendir");
         bad++;
      }
   }
   if (bad)
      __sync_fetch_and_add(&failures, bad);
}

int main(int argc, char **argv)
{
   struct smb_settings cfg;
   sthread_t *t[NTHREADS];
   int i, phase;

   if (argc < 3)
      return 2;
   network_init();
   memset(&cfg, 0, sizeof(cfg));
   cfg.server_address = argv[1];
   cfg.share          = argv[2];
   cfg.username       = argv[3];
   cfg.password       = argv[4];
   cfg.workgroup      = argc > 5 ? argv[5] : "";
   cfg.timeout        = 5;
   cfg.num_contexts   = 2;   /* fewer slots than threads: the pool is shared */
   cfg.auth_mode      = RETRO_SMB2_SEC_NTLMSSP;
   snprintf(url_file, sizeof(url_file), "smb://%s/%s/rsmb_test.bin", argv[1], argv[2]);
   snprintf(url_dir, sizeof(url_dir), "smb://%s/%s", argv[1], argv[2]);
   /* read-ahead off, then on: off is one request per read with no
    * prefetcher; on, a read-only open starts one (with threads), once
    * with a window larger than the file and once with windows far
    * smaller than one read */
   for (phase = 0; phase < 3; phase++)
   {
      RFILE *f;
      cfg.readahead = phase == 0 ? 0 : phase == 1 ? 1024 : 1;
      smb_init_cfg(&cfg);
      for (i = 0; i < NTHREADS; i++)
         t[i] = sthread_create(worker, (void*)(intptr_t)i);
      for (i = 0; i < NTHREADS; i++)
         if (t[i])
            sthread_join(t[i]);
      if ((f = filestream_open(url_file, RETRO_VFS_FILE_ACCESS_READ,
                  RETRO_VFS_FILE_ACCESS_HINT_NONE)))
      {
         libretro_vfs_implementation_file *h = filestream_get_vfs_handle(f);
         bool started = h && h->smb_prefetch != 0;
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
         smb_take_readahead_stats(&window_kib, &direct_kib);
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
      if (phase == 2 && (f = filestream_open(url_file, RETRO_VFS_FILE_ACCESS_READ,
                  RETRO_VFS_FILE_ACCESS_HINT_NONE)))
      {
         static uint8_t whole[200000];
         unsigned window_kib, direct_kib;
         int64_t  got;
         uint32_t j;
         /* a first octet sends the prefetcher to the start of the file;
          * the rest is then asked for in one read, past its windows */
         got = filestream_read(f, whole, 1);
         retro_sleep(200);
         if (got == 1)
            got += filestream_read(f, whole + 1, sizeof(whole) - 1);
         filestream_close(f);
         smb_take_readahead_stats(&window_kib, &direct_kib);
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
         if (!window_kib)
         {
            fprintf(stderr, "FAIL: read past the windows took nothing from them (%u KiB direct)\n", direct_kib);
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
               || buf2[0] != (uint8_t)(100 * 31 + 7))
         {
            fprintf(stderr, "FAIL: open asking for a mapping\n");
            failures++;
         }
         if (mf)
            filestream_close(mf);
      }
      smb_shutdown();
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
