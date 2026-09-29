/* Several threads through the smb:// VFS backend at once - open, read,
 * seek, stat and list on the same server - built under ThreadSanitizer
 * by the matrix. Same arguments as vfs_test. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net/net_compat.h>
#include <streams/file_stream.h>
#include <file/file_path.h>
#include <retro_dirent.h>
#include <rthreads/rthreads.h>
#include "../../../libretro-common/vfs/vfs_implementation_smb.h"

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
            || filestream_read(f, buf, sizeof(buf)) != (int64_t)sizeof(buf) || buf[0] != (uint8_t)((8 * (i % 4)) * 31 + 7))
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
   struct smb_settings cfg;
   sthread_t *t[NTHREADS];
   int i;

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
   smb_init_cfg(&cfg);
   snprintf(url_file, sizeof(url_file), "smb://%s/%s/rsmb_test.bin", argv[1], argv[2]);
   snprintf(url_dir, sizeof(url_dir), "smb://%s/%s", argv[1], argv[2]);

   for (i = 0; i < NTHREADS; i++)
      t[i] = sthread_create(worker, NULL);
   for (i = 0; i < NTHREADS; i++)
      if (t[i])
         sthread_join(t[i]);
   smb_shutdown();
   if (failures)
   {
      fprintf(stderr, "FAIL: %d bad operations across %d threads\n", failures, NTHREADS);
      return 1;
   }
   printf("ok: %d threads x %d rounds through the VFS\n", NTHREADS, ROUNDS);
   return 0;
}
