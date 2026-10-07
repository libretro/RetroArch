/* The real vfs_implementation.c over the nfs:// backend: the path a
 * core takes when it opens nfs://server/export/file. Same arguments
 * as nfs_test; runs after it so rnfs_dir exists. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net/net_compat.h>
#include <streams/file_stream.h>
#include <file/file_path.h>
#include <retro_dirent.h>
#include <vfs/vfs_implementation.h>
#include "../../../libretro-common/vfs/vfs_implementation_nfs.h"

int main(int argc, char **argv)
{
   struct nfs_settings cfg;
   RFILE *f;
   char url[600];
   uint8_t buf[300];
   int found = 0;
   struct RDIR *d;

   if (argc < 3)
      return 2;
   network_init();
   memset(&cfg, 0, sizeof(cfg));
   cfg.server_address = argv[1];
   cfg.export_path    = argv[2];
   cfg.timeout        = 5;
   cfg.num_contexts   = 2;
   if (argc > 4)
   {
      cfg.nfs_port   = (unsigned)atoi(argv[3]);
      cfg.mount_port = (unsigned)atoi(argv[4]);
   }
   if (argc > 5)
      cfg.version = (unsigned)atoi(argv[5]);
   if (!nfs_init_cfg(&cfg))
   {
      fprintf(stderr, "FAIL: nfs_init_cfg\n");
      return 1;
   }
   /* write through the generic VFS */
   snprintf(url, sizeof(url), "nfs://%s/rnfs_dir/vfs.bin", argv[1]);
   f = filestream_open(url, RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!f)
   {
      fprintf(stderr, "FAIL: open for write (%s)\n", nfs_get_last_error());
      return 1;
   }
   memset(buf, 0x5a, sizeof(buf));
   if (filestream_write(f, buf, sizeof(buf)) != (int64_t)sizeof(buf))
   {
      fprintf(stderr, "FAIL: write\n");
      return 1;
   }
   filestream_close(f);

   /* read back, seek, size */
   f = filestream_open(url, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!f || filestream_get_size(f) != (int64_t)sizeof(buf))
   {
      fprintf(stderr, "FAIL: open for read / size\n");
      return 1;
   }
   if (filestream_seek(f, 100, RETRO_VFS_SEEK_POSITION_START) != 0
         || filestream_read(f, buf, 16) != 16 || buf[0] != 0x5a
         || filestream_tell(f) != 116)
   {
      fprintf(stderr, "FAIL: seek/read/tell\n");
      return 1;
   }
   filestream_close(f);
   if (!path_is_valid(url) || path_is_directory(url))
   {
      fprintf(stderr, "FAIL: stat\n");
      return 1;
   }

   /* listing */
   snprintf(url, sizeof(url), "nfs://%s/rnfs_dir", argv[1]);
   if (!path_is_directory(url) || !(d = retro_opendir(url)))
   {
      fprintf(stderr, "FAIL: opendir\n");
      return 1;
   }
   while (retro_readdir(d))
      if (strcmp(retro_dirent_get_name(d), "vfs.bin") == 0 && !retro_dirent_is_dir(d, NULL))
         found = 1;
   retro_closedir(d);
   if (!found)
   {
      fprintf(stderr, "FAIL: readdir\n");
      return 1;
   }
   nfs_shutdown();
   printf("ok: VFS over the built-in NFS client\n");
   return 0;
}
