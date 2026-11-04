/* A failed nfs:// listing must say why: the reason nfs_get_last_error()
 * gives is never empty and names what failed. The first checks need no
 * server; with the port of fake_nfs42.py, a v4 export it does not have
 * must name the LOOKUP that failed and the status it got. Given the
 * local copy of the fake's sparse.img too, a file opened before the
 * settings change must read on with the settings it was opened with,
 * and with no export set and every slot busy, a file opened by URL must
 * keep the export its own connection found.
 *
 * Usage: nfs_error_test [fake_nfs42.py port [local sparse.img]]
 * Exit status is the failure count. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net/net_compat.h>
#include <streams/file_stream.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <retro_dirent.h>
#include "../../../libretro-common/vfs/vfs_implementation_nfs.h"

static struct nfs_settings cfg;

/* Listing @url must fail with a reason that holds @want. */
static int expect_fail(const char *what, const char *url, const char *want)
{
   struct RDIR *d  = retro_opendir(url);
   const char *err = nfs_get_last_error();
   if (d)
   {
      retro_closedir(d);
      fprintf(stderr, "FAIL: %s: %s was listed\n", what, url);
      return 1;
   }
   if (!strstr(err, want))
   {
      fprintf(stderr, "FAIL: %s: \"%s\" does not say \"%s\"\n", what, err, want);
      return 1;
   }
   printf("ok:   %s: %s\n", what, err);
   return 0;
}

/* Once the port and version point where nothing answers, a stat must
 * fail, and a file opened before must still read what @local holds:
 * its handle means nothing on the new settings' connections. */
static int expect_open_file_kept(const char *local)
{
   static char got[16384], want[16384];
   const char *url = "nfs://127.0.0.1/sparse.img";
   int fails = 0;
   int part;
   RFILE *f = filestream_open(url, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   RFILE *l = filestream_open(local, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!f || !l)
   {
      fprintf(stderr, "FAIL: open %s and %s (%s)\n", url, local,
            nfs_get_last_error());
      fails++;
   }
   for (part = 0; part < 3 && !fails; part++)
   {
      if (part == 1)
      {
         cfg.nfs_port   = 1;
         cfg.mount_port = 1;
         cfg.version    = 3;
         nfs_init_cfg(&cfg);
      }
      if (part > 0 && path_is_valid(url))
      {
         fprintf(stderr, "FAIL: a stat after the change used the old settings\n");
         fails++;
      }
      if (     filestream_read(f, got, sizeof(got))  != (int64_t)sizeof(got)
            || filestream_read(l, want, sizeof(want)) != (int64_t)sizeof(want)
            || memcmp(got, want, sizeof(got)) != 0)
      {
         fprintf(stderr, "FAIL: read %d of the open file, %s the change (%s)\n",
               part, part ? "after" : "before", nfs_get_last_error());
         fails++;
      }
   }
   if (f)
      filestream_close(f);
   if (l)
      filestream_close(l);
   if (!fails && path_is_valid(url))
   {
      fprintf(stderr, "FAIL: a stat used the closed file's connection\n");
      fails++;
   }
   if (!fails)
      printf("ok:   a file opened before a port and version change reads on\n");
   return fails;
}

/* The only slot held by a prefetcher, a second open of @url goes over
 * a connection of its own. Its prefetcher connects again, and finds
 * the file only on the export that connection settled on. */
static int expect_private_open_keeps_export(const char *local)
{
   static char got[16384], want[16384];
   const char *url = "nfs://127.0.0.1/export/sparse.img";
   unsigned window_kib = 0, direct_kib = 0;
   int fails = 0;
   RFILE *hold, *f, *l;
   cfg.export_path  = "";
   cfg.num_contexts = 1;
   cfg.readahead    = 64;
   nfs_init_cfg(&cfg);
   hold = filestream_open(url, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   f    = filestream_open(url, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   l    = filestream_open(local, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!hold || !f || !l)
   {
      fprintf(stderr, "FAIL: open %s twice and %s (%s)\n", url, local,
            nfs_get_last_error());
      fails++;
   }
   else if (filestream_read(f, got, sizeof(got))  != (int64_t)sizeof(got)
         || filestream_read(l, want, sizeof(want)) != (int64_t)sizeof(want)
         || memcmp(got, want, sizeof(got)) != 0)
   {
      fprintf(stderr, "FAIL: read the file opened with every slot busy (%s)\n",
            nfs_get_last_error());
      fails++;
   }
   if (f)
      filestream_close(f);
   nfs_take_readahead_stats(&window_kib, &direct_kib);
   if (!fails && !window_kib && !direct_kib)
   {
      fprintf(stderr, "FAIL: the file opened with every slot busy had no prefetcher\n");
      fails++;
   }
   if (hold)
      filestream_close(hold);
   if (l)
      filestream_close(l);
   if (!fails)
      printf("ok:   a file opened by URL with every slot busy keeps its export\n");
   return fails;
}

int main(int argc, char **argv)
{
   int fails = 0;

   network_init();
   cfg.server_address = "127.0.0.1";
   cfg.export_path    = "";
   cfg.timeout        = 5;
   cfg.num_contexts   = 2;
   cfg.version        = 4;
   nfs_init_cfg(&cfg);
   fails += expect_fail("no export", "nfs://127.0.0.1", "NFS Export");
   fails += expect_fail("no server in the URL", "nfs:///export", "bad URL");

   cfg.export_path = "/export";
   cfg.nfs_port    = 1;
   nfs_init_cfg(&cfg);
   fails += expect_fail("nothing on the port", "nfs://127.0.0.1/",
         "127.0.0.1:1 refused");

   if (argc > 1)
   {
      cfg.export_path = "/export/missing";
      cfg.nfs_port    = (unsigned)atoi(argv[1]);
      nfs_init_cfg(&cfg);
      fails += expect_fail("v4 export not there", "nfs://127.0.0.1/",
            "mount: LOOKUP missing: NFS4ERR_NOENT");

      /* the same pooled connection fails again, this time before
       * sending anything: the reason must be its own */
      cfg.export_path = "/export";
      nfs_init_cfg(&cfg);
      fails += expect_fail("v4 path not there", "nfs://127.0.0.1/missing",
            "LOOKUP missing: NFS4ERR_NOENT");
      {
         char url[400];
         size_t _len = strlcpy(url, "nfs://127.0.0.1/", sizeof(url));
         memset(url + _len, 'a', 300);
         url[_len + 300] = '\0';
         fails += expect_fail("v4 name too long", url, "name too long");
      }
   }
   if (argc > 2)
   {
      cfg.export_path = "/export";
      nfs_init_cfg(&cfg);
      fails += expect_open_file_kept(argv[2]);
      cfg.nfs_port   = (unsigned)atoi(argv[1]);
      cfg.mount_port = 0;
      cfg.version    = 4;
      fails += expect_private_open_keeps_export(argv[2]);
   }
   nfs_shutdown();
   return fails;
}
