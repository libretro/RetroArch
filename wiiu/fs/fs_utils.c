#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <wiiu/fs.h>

/* FS defines and types */
#define FS_MAX_LOCALPATH_SIZE           511
#define FS_MAX_MOUNTPATH_SIZE           128
#define FS_MAX_FULLPATH_SIZE            (FS_MAX_LOCALPATH_SIZE + FS_MAX_MOUNTPATH_SIZE)
#define FS_MAX_ARGPATH_SIZE             FS_MAX_FULLPATH_SIZE

#define FS_STATUS_OK                    0
#define FS_RET_UNSUPPORTED_CMD          0x0400
#define FS_RET_NO_ERROR                 0x0000
#define FS_RET_ALL_ERROR                (unsigned int)(-1)

#define FS_STAT_FLAG_IS_DIRECTORY       0x80000000

/* max length of file/dir name */
#define FS_MAX_ENTNAME_SIZE             256

#define FS_SOURCETYPE_EXTERNAL          0
#define FS_SOURCETYPE_HFIO              1
#define FS_SOURCETYPE_HFIO              1

#define FS_MOUNT_SOURCE_SIZE            0x300
#define FS_CLIENT_SIZE                  0x1700
#define FS_CMD_BLOCK_SIZE               0xA80

int MountFS(void *pClient, void *pCmd, char **mount_path)
{
   char *mountPath = NULL;
   int result      = -1;
   void *mountSrc  = malloc(FS_MOUNT_SOURCE_SIZE);

   if (!mountSrc)
      return -3;

   if (!(mountPath = (char*) malloc(FS_MAX_MOUNTPATH_SIZE)))
   {
      free(mountSrc);
      return -4;
   }

   memset(mountSrc, 0, FS_MOUNT_SOURCE_SIZE);
   memset(mountPath, 0, FS_MAX_MOUNTPATH_SIZE);

   /* Mount sdcard */
   if (FSGetMountSource(pClient, pCmd, FS_SOURCETYPE_EXTERNAL, mountSrc, -1) == 0)
   {
      result = FSMount(pClient, pCmd, mountSrc, mountPath, FS_MAX_MOUNTPATH_SIZE, -1);

      if ((result == 0) && mount_path)
      {
         *mount_path = (char*)malloc(strlen(mountPath) + 1);
         if (*mount_path)
            strcpy(*mount_path, mountPath);
      }
   }

   free(mountPath);
   free(mountSrc);
   return result;
}
