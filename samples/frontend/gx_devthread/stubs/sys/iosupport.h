/* The part of devkitPPC's <sys/iosupport.h> the frontend uses. */
#ifndef STUB_SYS_IOSUPPORT_H
#define STUB_SYS_IOSUPPORT_H
typedef struct
{
   const char *name;
   int   structSize;
   void *open_r, *close_r, *write_r, *read_r, *seek_r, *fstat_r, *stat_r,
        *link_r, *unlink_r, *chdir_r, *rename_r, *mkdir_r;
   int   dirStateSize;
   void *diropen_r, *dirreset_r, *dirnext_r, *dirclose_r, *statvfs_r,
        *ftruncate_r, *fsync_r;
   void *deviceData;
} devoptab_t;
#define STD_OUT 1
#define STD_ERR 2
extern const devoptab_t *devoptab_list[];
#endif
