/* nfs_init_cfg() copies the settings it is given: the caller's struct
 * may be freed or rewritten at once, while other threads resolve URLs
 * against whatever was published last. Needs no server - the ports
 * point where nothing listens, so every connect is refused at once.
 *
 * A heap settings struct is freed right after it is handed over and a
 * stat then resolves a URL (caught under ASan if the client kept the
 * pointer); then reader threads stat and list while the main thread
 * rewrites one struct in place and hands it over again and again
 * (caught under TSan, CFLAGS=-fsanitize=thread). Every call must fail
 * cleanly with a reason.
 *
 * Exit status is the failure count. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <compat/strl.h>
#include <rthreads/rthreads.h>
#include <retro_atomic.h>
#include "../../../libretro-common/vfs/vfs_implementation_nfs.h"

#define READERS 4
#define REPUBLISH 400

static struct nfs_settings cfg;
static char                cfg_server[256];
static char                cfg_export[512];
static retro_atomic_int_t  done;
static retro_atomic_int_t  answered;    /* calls that wrongly succeeded */
static retro_atomic_int_t  calls;

static void reader(void *arg)
{
   (void)arg;
   while (!retro_atomic_load_acquire_int(&done))
   {
      int64_t         size = 0;
      nfs_dir_handle *dh;
      if (retro_vfs_stat_nfs("nfs://127.0.0.1/roms/a.bin", &size))
         retro_atomic_fetch_add_int(&answered, 1);
      if ((dh = retro_vfs_opendir_nfs("nfs://127.0.0.1/roms", false)))
      {
         retro_atomic_fetch_add_int(&answered, 1);
         retro_vfs_closedir_nfs(dh);
      }
      retro_atomic_fetch_add_int(&calls, 1);
   }
}

static void fill(struct nfs_settings *s, unsigned round)
{
   strlcpy(cfg_server, "127.0.0.1", sizeof(cfg_server));
   strlcpy(cfg_export, (round & 1) ? "/srv/export/one" : "/e", sizeof(cfg_export));
   s->server_address = cfg_server;
   s->export_path    = cfg_export;
   s->timeout        = 1;
   s->num_contexts   = 2;
   s->nfs_port       = 1 + (round & 1);
   s->mount_port     = 1 + (round & 1);
   s->version        = (round & 2) ? 4 : 3;
   s->readahead      = (round & 4) ? 64 : 0;
}

int main(void)
{
   sthread_t *t[READERS];
   unsigned   i;
   int        failures = 0;
   int64_t    size     = 0;

   /* a struct the caller frees at once */
   {
      struct nfs_settings *heap = (struct nfs_settings*)calloc(1, sizeof(*heap));
      char                *exp  = (char*)malloc(32);
      if (!heap || !exp)
         return 1;
      strlcpy(exp, "/srv/freed", 32);
      fill(heap, 0);
      heap->export_path = exp;
      if (!nfs_init_cfg(heap))
      {
         fprintf(stderr, "FAIL: nfs_init_cfg\n");
         return 1;
      }
      free(exp);
      free(heap);
      if (retro_vfs_stat_nfs("nfs://127.0.0.1/a.bin", &size))
      {
         fprintf(stderr, "FAIL: a stat with nothing listening succeeded\n");
         failures++;
      }
      else if (!*nfs_get_last_error())
      {
         fprintf(stderr, "FAIL: a failed stat gave no reason\n");
         failures++;
      }
      else
         printf("ok:   settings freed by the caller: %s\n", nfs_get_last_error());
   }

   /* one struct rewritten in place while readers resolve */
   fill(&cfg, 0);
   nfs_init_cfg(&cfg);
   retro_atomic_int_init(&done, 0);
   retro_atomic_int_init(&answered, 0);
   retro_atomic_int_init(&calls, 0);
   for (i = 0; i < READERS; i++)
      if (!(t[i] = sthread_create(reader, NULL)))
      {
         fprintf(stderr, "FAIL: thread %u\n", i);
         return 1;
      }
   for (i = 0; i < REPUBLISH; i++)
   {
      fill(&cfg, i);
      nfs_init_cfg(&cfg);
   }
   retro_atomic_store_release_int(&done, 1);
   for (i = 0; i < READERS; i++)
      sthread_join(t[i]);
   if (retro_atomic_load_acquire_int(&answered))
   {
      fprintf(stderr, "FAIL: %d calls succeeded with nothing listening\n",
            retro_atomic_load_acquire_int(&answered));
      failures++;
   }
   else
      printf("ok:   %d reader rounds across %d republished settings\n",
            retro_atomic_load_acquire_int(&calls), REPUBLISH);
   nfs_shutdown();
   if (!failures)
      printf("[pass] nfs_cfg_race_test\n");
   return failures;
}
