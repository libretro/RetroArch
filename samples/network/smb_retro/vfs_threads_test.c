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
#define FLIP_ROUNDS 25
#define FLIP_KEYS   5     /* more than the VFS has pools */

static char url_file[600], url_dir[600];
static char url_file_b[600], url_dir_b[600];
static bool two_shares = false;
static int  failures = 0;
static int  rounds = ROUNDS;
static bool flipping = false;
static struct smb_settings flip_cfg[FLIP_KEYS];

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
   for (i = 0; i < rounds; i++)
   {
      bool        b    = two_shares && ((id + i) & 1);
      const char *file = b ? url_file_b : url_file;
      const char *dir  = b ? url_dir_b  : url_dir;
      RFILE *f = filestream_open(file, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
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
      if (!path_is_valid(file))
      {
         report(id, i, "stat");
         bad++;
      }
      if ((d = retro_opendir(dir)))
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
      /* the next take may go to another pool than this give */
      if (flipping)
         smb_init_cfg(&flip_cfg[(id + i) % FLIP_KEYS]);
   }
   if (bad)
      __sync_fetch_and_add(&failures, bad);
}

int main(int argc, char **argv)
{
   struct smb_settings cfg;
   struct smb_settings cfg_off;
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
      /* the whole file lent where read-ahead holds it and read where it
       * does not, as a hash loop takes it: every octet in order, some
       * lent with read-ahead on and none with it off, and a read after
       * the lends picks up where they stopped. A first octet sends the
       * prefetcher to the start, as above; on a loopback the reader's
       * own reads would otherwise outrun it to the end of the file. */
      if ((f = filestream_open(url_file, RETRO_VFS_FILE_ACCESS_READ,
                  RETRO_VFS_FILE_ACCESS_HINT_NONE)))
      {
         static uint8_t chunk[4096];
         int64_t  size  = filestream_get_size(f);
         uint64_t at    = 1, lent = 0;
         bool     ok    = size > 64;
         uint8_t  tail[16];

         if (filestream_read(f, tail, 1) != 1 || tail[0] != 7)
            ok = false;
         retro_sleep(200);

         /* all but the last 16, which the read after takes */
         while (ok && at + 16 < (uint64_t)size)
         {
            const uint8_t *ptr  = NULL;
            int64_t        rest = size - 16 - (int64_t)at;
            int64_t        n    = filestream_view(f, rest, &ptr);
            uint32_t       j;
            if (n < 0)
            {
               n   = filestream_read(f, chunk,
                     rest < (int64_t)sizeof(chunk) ? rest : (int64_t)sizeof(chunk));
               ptr = chunk;
            }
            else
               lent += (uint64_t)n;
            if (n <= 0)
               ok = false;
            for (j = 0; ok && j < (uint32_t)n; j++)
               if (ptr[j] != (uint8_t)((at + j) * 31 + 7))
                  ok = false;
            at += (uint64_t)n;
         }
         if (ok && (filestream_read(f, tail, 16) != 16
                  || tail[0] != (uint8_t)(at * 31 + 7)))
            ok = false;
         filestream_close(f);
         /* taken so the next phase counts only its own streams */
         {
            unsigned window_kib, direct_kib;
            smb_take_readahead_stats(&window_kib, &direct_kib);
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
   /* read-ahead turned off while a prefetcher holds a slot: the slot
    * goes back with its own pool's window */
   cfg.readahead     = 1024;
   cfg_off           = cfg;
   cfg_off.readahead = 0;
   smb_init_cfg(&cfg);
   {
      RFILE *f = filestream_open(url_file, RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);
      libretro_vfs_implementation_file *h = f ? filestream_get_vfs_handle(f) : NULL;
      if (!h || !h->smb_prefetch)
      {
         fprintf(stderr, "FAIL: no prefetcher holding a slot\n");
         failures++;
      }
      smb_init_cfg(&cfg_off);
      if (f)
         filestream_close(f);
      if (!smb_pool_windows_ok())
      {
         fprintf(stderr, "FAIL: a slot went back with the window of a setting its pool was not built with\n");
         failures++;
      }
      smb_init_cfg(&cfg);
      smb_shutdown();
   }
   /* the key switched under the workers across more keys than there
    * are pools, half with read-ahead off: every operation succeeds as
    * idle pools are evicted beside busy ones, and every slot goes back
    * with its pool's window */
   for (i = 0; i < FLIP_KEYS; i++)
   {
      flip_cfg[i]           = cfg;
      flip_cfg[i].timeout   = cfg.timeout + i;
      flip_cfg[i].readahead = (i & 1) ? 0 : 1024;
   }
   rounds   = FLIP_ROUNDS;
   flipping = true;
   for (i = 0; i < NTHREADS; i++)
      if (!(t[i] = sthread_create(worker, (void*)(intptr_t)i)))
         failures++;
   for (i = 0; i < NTHREADS; i++)
      if (t[i])
         sthread_join(t[i]);
   flipping = false;
   rounds   = ROUNDS;
   if (!smb_pool_windows_ok())
   {
      fprintf(stderr, "FAIL: a slot went back with another pool's window after the key switches\n");
      failures++;
   }
   smb_init_cfg(&cfg);
   smb_shutdown();
   /* a second share of the same directory (argv[6]) with no share
    * configured: threads alternating between the two keep a pool each,
    * so neither is taken down under the other's users */
   if (argc > 6)
   {
      cfg.share     = "";
      cfg.readahead = 1024;
      snprintf(url_file_b, sizeof(url_file_b), "smb://%s/%s/rsmb_test.bin", argv[1], argv[6]);
      snprintf(url_dir_b, sizeof(url_dir_b), "smb://%s/%s", argv[1], argv[6]);
      two_shares = true;
      smb_init_cfg(&cfg);
      for (i = 0; i < NTHREADS; i++)
         t[i] = sthread_create(worker, (void*)(intptr_t)i);
      for (i = 0; i < NTHREADS; i++)
         if (t[i])
            sthread_join(t[i]);
      smb_shutdown();
   }
   if (failures)
   {
      fprintf(stderr, "FAIL: %d bad operations across %d threads\n", failures, NTHREADS);
      return 1;
   }
   printf("ok: %d threads x %d rounds through the VFS, read-ahead off and on, reads past the windows; %d rounds with the settings switched under them%s\n",
         NTHREADS, ROUNDS, FLIP_ROUNDS, argc > 6 ? "; two shares" : "");
   return 0;
}
