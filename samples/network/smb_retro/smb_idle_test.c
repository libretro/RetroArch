/* smb:// files kept open across a pause: reads and writes after it
 * must work however long the client stayed quiet - past the server's
 * idle disconnect, or across a server restart (a NAS rebooting).
 * Through the VFS, as a core reads, with read-ahead off as RetroArch
 * runs by default, so every read goes to the server.
 *
 * Usage: smb_idle_test <server> <share> <user> <password> <workgroup>
 *        <idle seconds> [w]
 * With w the write comes first after the pause, so it is the call
 * that finds the connection gone. Exit status is the failure count. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <retro_timers.h>
#include <net/net_compat.h>
#include <vfs/vfs.h>
#include <vfs/vfs_implementation.h>
#include "../../../libretro-common/vfs/vfs_implementation_smb.h"

#define SIZE 65536

static int write_at(libretro_vfs_implementation_file *s, const uint8_t *p,
      int64_t len)
{
   return retro_vfs_file_write_smb(s, p, (uint64_t)len) == len;
}

int main(int argc, char **argv)
{
   static uint8_t data[SIZE], back[SIZE];
   libretro_vfs_implementation_file r, w;
   struct smb_settings cfg;
   char url_r[512], url_w[512];
   unsigned i, idle;
   int fails = 0, write_first;

   if (argc < 7)
      return 2;
   idle        = (unsigned)atoi(argv[6]);
   write_first = argc > 7 && argv[7][0] == 'w';
   network_init();
   for (i = 0; i < SIZE; i++)
      data[i] = (uint8_t)(i * 7 + (i >> 8));
   memset(&cfg, 0, sizeof(cfg));
   cfg.server_address = argv[1];
   cfg.share          = argv[2];
   cfg.username       = argv[3];
   cfg.password       = argv[4];
   cfg.workgroup      = argv[5];
   cfg.timeout        = 5;
   cfg.num_contexts   = 2;
   cfg.readahead      = 0;
   cfg.auth_mode      = RETRO_SMB2_SEC_NTLMSSP;
   if (!smb_init_cfg(&cfg))
   {
      fprintf(stderr, "FAIL: smb_init_cfg\n");
      return 1;
   }
   snprintf(url_r, sizeof(url_r), "smb://%s/%s/idle.bin",   argv[1], argv[2]);
   snprintf(url_w, sizeof(url_w), "smb://%s/%s/idle_w.bin", argv[1], argv[2]);

   /* the file to read, made first */
   memset(&r, 0, sizeof(r));
   if (!retro_vfs_file_open_smb(&r, url_r, RETRO_VFS_FILE_ACCESS_WRITE, 0)
         || !write_at(&r, data, SIZE))
   {
      fprintf(stderr, "FAIL: create\n");
      return 1;
   }
   retro_vfs_file_close_smb(&r);

   memset(&w, 0, sizeof(w));
   if (!retro_vfs_file_open_smb(&w, url_w, RETRO_VFS_FILE_ACCESS_WRITE, 0)
         || !write_at(&w, data, 8192))
   {
      fprintf(stderr, "FAIL: write before the pause\n");
      return 1;
   }
   memset(&r, 0, sizeof(r));
   if (!retro_vfs_file_open_smb(&r, url_r, RETRO_VFS_FILE_ACCESS_READ, 0)
         || retro_vfs_file_read_smb(&r, back, 4096) != 4096 || memcmp(back, data, 4096))
   {
      fprintf(stderr, "FAIL: read before the pause\n");
      fails++;
   }
   printf("idle for %u s with the files open\n", idle);
   fflush(stdout);
   for (i = 0; i < idle; i++)
      retro_sleep(1000);

   if (write_first)
   {
      if (!write_at(&w, data + 8192, 8192))
      {
         fprintf(stderr, "FAIL: write after %u s idle (first call)\n", idle);
         fails++;
      }
      else
         printf("ok:   write after %u s idle, first call after the pause\n", idle);
   }
   if (retro_vfs_file_seek_smb(&r, 32768, RETRO_VFS_SEEK_POSITION_START) != 0
         || retro_vfs_file_read_smb(&r, back, 4096) != 4096
         || memcmp(back, data + 32768, 4096))
   {
      fprintf(stderr, "FAIL: read after %u s idle\n", idle);
      fails++;
   }
   else
      printf("ok:   read after %u s idle\n", idle);
   if (!write_first)
   {
      if (!write_at(&w, data + 8192, 8192))
      {
         fprintf(stderr, "FAIL: write after %u s idle\n", idle);
         fails++;
      }
      else
         printf("ok:   write after %u s idle\n", idle);
   }
   retro_vfs_file_close_smb(&w);
   retro_vfs_file_close_smb(&r);

   memset(&r, 0, sizeof(r));
   if (!retro_vfs_file_open_smb(&r, url_w, RETRO_VFS_FILE_ACCESS_READ, 0)
         || retro_vfs_file_read_smb(&r, back, 16384) != 16384
         || memcmp(back, data, 16384))
   {
      fprintf(stderr, "FAIL: what was written before and after the pause\n");
      fails++;
   }
   else
      printf("ok:   both writes landed\n");
   retro_vfs_file_close_smb(&r);
   smb_shutdown();
   return fails;
}
