/* The real vfs_implementation_smb.c over the built-in client: the path
 * a core takes when it opens smb://server/share/file. Same arguments
 * as smb_test; expects rsmb_test.bin from that run to exist. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net/net_compat.h>
#include <vfs/vfs.h>
#include <vfs/vfs_implementation.h>
#include "../../../libretro-common/vfs/vfs_implementation_smb.h"

int main(int argc, char **argv)
{
   struct smb_settings cfg;
   libretro_vfs_implementation_file stream;
   smb_dir_handle *dh;
   struct smbc_dirent *ent;
   char url[512];
   uint8_t buf[4096];
   int64_t size = 0;
   int found = 0;

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
   cfg.num_contexts   = 2;
   cfg.auth_mode      = RETRO_SMB2_SEC_NTLMSSP;
   if (!smb_init_cfg(&cfg))
   {
      fprintf(stderr, "FAIL: smb_init_cfg\n");
      return 1;
   }
   snprintf(url, sizeof(url), "smb://%s/%s/rsmb_test.bin", argv[1], argv[2]);
   if (retro_vfs_stat_smb(url, &size) == 0 || size != 200000)
   {
      fprintf(stderr, "FAIL: stat (%lld)\n", (long long)size);
      return 1;
   }
   memset(&stream, 0, sizeof(stream));
   if (!retro_vfs_file_open_smb(&stream, url, RETRO_VFS_FILE_ACCESS_READ, 0))
   {
      fprintf(stderr, "FAIL: open\n");
      return 1;
   }
   if (retro_vfs_file_seek_smb(&stream, 100, RETRO_VFS_SEEK_POSITION_START) != 0
         || retro_vfs_file_read_smb(&stream, buf, 16) != 16
         || buf[0] != (uint8_t)(100 * 31 + 7)
         || retro_vfs_file_tell_smb(&stream) != 116)
   {
      fprintf(stderr, "FAIL: seek/read/tell\n");
      return 1;
   }
   retro_vfs_file_close_smb(&stream);

   snprintf(url, sizeof(url), "smb://%s/%s", argv[1], argv[2]);
   if (!(dh = retro_vfs_opendir_smb(url, false)))
   {
      fprintf(stderr, "FAIL: opendir\n");
      return 1;
   }
   while ((ent = retro_vfs_readdir_smb(dh)))
      if (strcmp(ent->name, "rsmb_test.bin") == 0 && ent->size == 200000)
         found = 1;
   retro_vfs_closedir_smb(dh);
   if (!found)
   {
      fprintf(stderr, "FAIL: readdir\n");
      return 1;
   }
   /* the share browser: with no share configured, smb://server lists
    * the shares; "share" is there, IPC$ and the hidden one are not */
   smb_shutdown();
   cfg.share = "";
   if (!smb_init_cfg(&cfg))
   {
      fprintf(stderr, "FAIL: smb_init_cfg (browse)\n");
      return 1;
   }
   snprintf(url, sizeof(url), "smb://%s", argv[1]);
   found = 0;
   if (!(dh = retro_vfs_opendir_smb(url, false)))
   {
      fprintf(stderr, "FAIL: opendir server\n");
      return 1;
   }
   while ((ent = retro_vfs_readdir_smb(dh)))
   {
      if (strcmp(ent->name, argv[2]) == 0)
         found = 1;
      if (strcmp(ent->name, "IPC$") == 0 || strcmp(ent->name, "hidden$") == 0)
         found = -1;
   }
   retro_vfs_closedir_smb(dh);
   if (found != 1)
   {
      fprintf(stderr, "FAIL: share browse (%d)\n", found);
      return 1;
   }
   smb_shutdown();
   printf("ok: VFS over the SMB backend\n");
   return 0;
}
