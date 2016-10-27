#ifndef FS_DEFS_H
#define	FS_DEFS_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif


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

typedef struct
{
    uint32_t flag;
    uint32_t permission;
    uint32_t owner_id;
    uint32_t group_id;
    uint32_t size;
    uint32_t alloc_size;
    uint64_t quota_size;
    uint32_t ent_id;
    uint64_t ctime;
    uint64_t mtime;
    uint8_t attributes[48];
} __attribute__((packed)) FSStat;

typedef struct
{
    FSStat      stat;
    char        name[FS_MAX_ENTNAME_SIZE];
} FSDirEntry;


#ifdef __cplusplus
}
#endif

#endif	/* FS_DEFS_H */

