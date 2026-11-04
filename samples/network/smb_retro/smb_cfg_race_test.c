/* smb_init_cfg() copies the settings it is given: the caller's struct
 * may be freed or rewritten at once, while other threads build paths
 * and look for connections against whatever was published last. Needs
 * no server - none is set, so every call stops before the network
 * after reading the share, subdir and credentials.
 *
 * A heap settings struct is freed right after it is handed over and a
 * stat then reads it (caught under ASan if the client kept the
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
#include "../../../libretro-common/vfs/vfs_implementation_smb.h"

#define READERS 4
#define REPUBLISH 400

static struct smb_settings cfg;
static char                cfg_share[256];
static char                cfg_subdir[256];
static char                cfg_user[128];
static retro_atomic_int_t  done;
static retro_atomic_int_t  answered;    /* calls that wrongly succeeded */
static retro_atomic_int_t  calls;

static void reader(void *arg)
{
   (void)arg;
   while (!retro_atomic_load_acquire_int(&done))
   {
      int64_t         size = 0;
      smb_dir_handle *dh;
      if (retro_vfs_stat_smb("smb://srv/share/roms/a.bin", &size))
         retro_atomic_fetch_add_int(&answered, 1);
      if ((dh = retro_vfs_opendir_smb("smb://srv/share/roms", false)))
      {
         retro_atomic_fetch_add_int(&answered, 1);
         retro_vfs_closedir_smb(dh);
      }
      retro_atomic_fetch_add_int(&calls, 1);
   }
}

static void fill(struct smb_settings *s, unsigned round)
{
   strlcpy(cfg_share,  (round & 1) ? "roms" : "a-much-longer-share-name", sizeof(cfg_share));
   strlcpy(cfg_subdir, (round & 2) ? "/games/snes" : "", sizeof(cfg_subdir));
   strlcpy(cfg_user,   (round & 4) ? "guest" : "", sizeof(cfg_user));
   s->server_address = "";
   s->share          = cfg_share;
   s->username       = cfg_user;
   s->password       = "";
   s->workgroup      = "";
   s->subdir         = cfg_subdir;
   s->realm          = "";
   s->kdc            = "";
   s->timeout        = 1;
   s->num_contexts   = 2;
   s->auth_mode      = 0;
   s->readahead      = (round & 8) ? 64 : 0;
}

int main(void)
{
   sthread_t *t[READERS];
   unsigned   i;
   int        failures = 0;
   int64_t    size     = 0;

   /* a struct the caller frees at once */
   {
      struct smb_settings *heap = (struct smb_settings*)calloc(1, sizeof(*heap));
      char                *sub  = (char*)malloc(32);
      if (!heap || !sub)
         return 1;
      fill(heap, 0);
      strlcpy(sub, "/freed/subdir", 32);
      heap->subdir = sub;
      if (!smb_init_cfg(heap))
      {
         fprintf(stderr, "FAIL: smb_init_cfg\n");
         return 1;
      }
      free(sub);
      free(heap);
      if (retro_vfs_stat_smb("smb://srv/share/a.bin", &size))
      {
         fprintf(stderr, "FAIL: a stat with no server set succeeded\n");
         failures++;
      }
      else if (!*smb_get_last_error())
      {
         fprintf(stderr, "FAIL: a failed stat gave no reason\n");
         failures++;
      }
      else
         printf("ok:   settings freed by the caller: %s\n", smb_get_last_error());
   }

   /* one struct rewritten in place while readers resolve */
   fill(&cfg, 0);
   smb_init_cfg(&cfg);
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
      smb_init_cfg(&cfg);
   }
   retro_atomic_store_release_int(&done, 1);
   for (i = 0; i < READERS; i++)
      sthread_join(t[i]);
   if (retro_atomic_load_acquire_int(&answered))
   {
      fprintf(stderr, "FAIL: %d calls succeeded with no server set\n",
            retro_atomic_load_acquire_int(&answered));
      failures++;
   }
   else
      printf("ok:   %d reader rounds across %d republished settings\n",
            retro_atomic_load_acquire_int(&calls), REPUBLISH);
   smb_shutdown();
   if (!failures)
      printf("[pass] smb_cfg_race_test\n");
   return failures;
}
