/* A failed nfs:// listing must say why: the reason nfs_get_last_error()
 * gives is never empty and names what failed. The first checks need no
 * server; with the port of fake_nfs42.py, a v4 export it does not have
 * must name the LOOKUP that failed and the status it got.
 *
 * Usage: nfs_error_test [fake_nfs42.py port]
 * Exit status is the failure count. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net/net_compat.h>
#include <compat/strl.h>
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
   nfs_shutdown();
   return fails;
}
