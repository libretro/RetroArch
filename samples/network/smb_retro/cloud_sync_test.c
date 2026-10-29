/* Cloud sync connects and disconnects on one thread while the smb://
 * VFS connects privately on another, as during a scan. libsmb2 keeps
 * every context on one global list, so both must make and drop them
 * through the VFS's locked wrappers; the libsmb2 TSan lane reports
 * any that does not. Same arguments as
 * vfs_test; expects rsmb_test.bin to exist. */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <boolean.h>
#include <net/net_compat.h>
#include <vfs/vfs.h>
#include <vfs/vfs_implementation.h>
#include <rthreads/rthreads.h>
#include "../../../libretro-common/vfs/vfs_implementation_smb.h"

#include "../../../network/cloud_sync/smb.c"

#define ROUNDS 4

void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
}

settings_t *config_get_ptr(void)
{
   static settings_t settings;
   return &settings;
}

static char url[512];
static int  sync_bad, stat_bad;

static void on_done(void *user_data, const char *path, bool success,
      RFILE *file)
{
   (void)path;
   (void)file;
   *(bool*)user_data = success;
}

static void syncer(void *arg)
{
   int i;
   (void)arg;
   for (i = 0; i < ROUNDS; i++)
   {
      bool ok = false;
      smb_sync_begin(on_done, &ok);
      if (!ok)
         sync_bad++;
      smb_sync_end(on_done, &ok);
   }
}

static void stater(void *arg)
{
   int i;
   int64_t size;
   (void)arg;
   for (i = 0; i < ROUNDS * 2; i++)
      if (retro_vfs_stat_smb(url, &size) == 0)
         stat_bad++;
}

int main(int argc, char **argv)
{
   struct smb_settings cfg;
   libretro_vfs_implementation_file held;
   settings_t *settings = config_get_ptr();
   sthread_t *t[2];
   int i;

   if (argc < 5)
      return 2;
   network_init();
   memset(&cfg, 0, sizeof(cfg));
   cfg.server_address = argv[1];
   cfg.share          = argv[2];
   cfg.username       = argv[3];
   cfg.password       = argv[4];
   cfg.workgroup      = argc > 5 ? argv[5] : "";
   cfg.timeout        = 5;
   cfg.num_contexts   = 1;   /* held below: every stat connects privately */
   cfg.auth_mode      = RETRO_SMB2_SEC_NTLMSSP;
   if (!smb_init_cfg(&cfg))
   {
      fprintf(stderr, "FAIL: smb_init_cfg\n");
      return 1;
   }
   strlcpy(settings->arrays.smb_client_server_address, cfg.server_address,
         sizeof(settings->arrays.smb_client_server_address));
   strlcpy(settings->arrays.smb_client_share, cfg.share,
         sizeof(settings->arrays.smb_client_share));
   strlcpy(settings->arrays.smb_client_username, cfg.username,
         sizeof(settings->arrays.smb_client_username));
   strlcpy(settings->arrays.smb_client_password, cfg.password,
         sizeof(settings->arrays.smb_client_password));
   strlcpy(settings->arrays.smb_client_workgroup, cfg.workgroup,
         sizeof(settings->arrays.smb_client_workgroup));
   settings->uints.smb_client_auth_mode = cfg.auth_mode;
   settings->uints.smb_client_timeout   = cfg.timeout;
   smb_sync_capture();

   snprintf(url, sizeof(url), "smb://%s/%s/rsmb_test.bin", argv[1], argv[2]);
   memset(&held, 0, sizeof(held));
   if (!retro_vfs_file_open_smb(&held, url, RETRO_VFS_FILE_ACCESS_READ, 0))
   {
      fprintf(stderr, "FAIL: open %s\n", url);
      return 1;
   }
   t[0] = sthread_create(syncer, NULL);
   t[1] = sthread_create(stater, NULL);
   for (i = 0; i < 2; i++)
      if (t[i])
         sthread_join(t[i]);
   retro_vfs_file_close_smb(&held);
   smb_shutdown();

   if (!t[0] || !t[1] || sync_bad || stat_bad)
   {
      fprintf(stderr, "FAIL: %d of %d cloud sync connects, %d of %d stats\n",
            sync_bad, ROUNDS, stat_bad, ROUNDS * 2);
      return 1;
   }
   printf("PASS: cloud sync beside private VFS connections\n");
   return 0;
}
