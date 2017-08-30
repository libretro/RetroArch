#ifndef __ISFS_H__
#define __ISFS_H__

#if defined(HW_RVL)

#include <gctypes.h>

#define ISFS_MAXPATH				IPC_MAXPATH_LEN

#define ISFS_OPEN_READ				0x01
#define ISFS_OPEN_WRITE				0x02
#define ISFS_OPEN_RW				(ISFS_OPEN_READ | ISFS_OPEN_WRITE)

#define ISFS_OK						   0
#define ISFS_ENOMEM					 -22
#define ISFS_EINVAL					-101

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef struct _fstats
{
	u32 file_length;
	u32 file_pos;
} fstats;

typedef s32 (*isfscallback)(s32 result,void *usrdata);

s32 ISFS_Initialize();
s32 ISFS_Deinitialize();

s32 ISFS_Open(const char *filepath,u8 mode);
s32 ISFS_OpenAsync(const char *filepath,u8 mode,isfscallback cb,void *usrdata);
s32 ISFS_Close(s32 fd);
s32 ISFS_CloseAsync(s32 fd,isfscallback cb,void *usrdata);
s32 ISFS_Delete(const char *filepath);
s32 ISFS_DeleteAsync(const char *filepath,isfscallback cb,void *usrdata);
s32 ISFS_ReadDir(const char *filepath,char *name_list,u32 *num);
s32 ISFS_ReadDirAsync(const char *filepath,char *name_list,u32 *num,isfscallback cb,void *usrdata);
s32 ISFS_CreateFile(const char *filepath,u8 attributes,u8 owner_perm,u8 group_perm,u8 other_perm);
s32 ISFS_CreateFileAsync(const char *filepath,u8 attributes,u8 owner_perm,u8 group_perm,u8 other_perm,isfscallback cb,void *usrdata);
s32 ISFS_Write(s32 fd,const void *buffer,u32 length);
s32 ISFS_WriteAsync(s32 fd,const void *buffer,u32 length,isfscallback cb,void *usrdata);
s32 ISFS_Read(s32 fd,void *buffer,u32 length);
s32 ISFS_ReadAsync(s32 fd,void *buffer,u32 length,isfscallback cb,void *usrdata);
s32 ISFS_Seek(s32 fd,s32 where,s32 whence);
s32 ISFS_SeekAsync(s32 fd,s32 where,s32 whence,isfscallback cb,void *usrdata);
s32 ISFS_CreateDir(const char *filepath,u8 attributes,u8 owner_perm,u8 group_perm,u8 other_perm);
s32 ISFS_CreateDirAsync(const char *filepath,u8 attributes,u8 owner_perm,u8 group_perm,u8 other_perm,isfscallback cb,void *usrdata);
s32 ISFS_GetStats(void *stats);
s32 ISFS_GetStatsAsync(void *stats,isfscallback cb,void *usrdata);
s32 ISFS_GetFileStats(s32 fd,fstats *status);
s32 ISFS_GetFileStatsAsync(s32 fd,fstats *status,isfscallback cb,void *usrdata);
s32 ISFS_GetAttr(const char *filepath,u32 *ownerID,u16 *groupID,u8 *attributes,u8 *ownerperm,u8 *groupperm,u8 *otherperm);
s32 ISFS_GetAttrAsync(const char *filepath,u32 *ownerID,u16 *groupID,u8 *attributes,u8 *ownerperm,u8 *groupperm,u8 *otherperm,isfscallback cb,void *usrdata);
s32 ISFS_Rename(const char *filepathOld,const char *filepathNew);
s32 ISFS_RenameAsync(const char *filepathOld,const char *filepathNew,isfscallback cb,void *usrdata);
s32 ISFS_SetAttr(const char *filepath,u32 ownerID,u16 groupID,u8 attributes,u8 ownerperm,u8 groupperm,u8 otherperm);
s32 ISFS_SetAttrAsync(const char *filepath,u32 ownerID,u16 groupID,u8 attributes,u8 ownerperm,u8 groupperm,u8 otherperm,isfscallback cb,void *usrdata);
s32 ISFS_GetUsage(const char* filepath, u32* usage1, u32* usage2);
s32 ISFS_GetUsageAsync(const char* filepath, u32* usage1, u32* usage2,isfscallback cb,void *usrdata);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif /* defined(HW_RVL) */

#endif
