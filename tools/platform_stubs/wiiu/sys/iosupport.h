#ifndef WIIU_IOSUPPORT_STUB_H
#define WIIU_IOSUPPORT_STUB_H
/* Stand-in for devkitPPC newlib's sys/iosupport.h: the devoptab table
 * with newlib's field order, so the Wii U devoptabs (os/wiiu/fs) are
 * compiled - and their tables checked against that order - where the
 * toolchain is not installed. */
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

struct _reent;
#ifndef __DYNAMIC_REENT__
struct _reent { int _errno; };
#endif

typedef struct
{
   void *device;
   void *dirStruct;
} DIR_ITER;

typedef struct
{
   const char *name;
   size_t structSize;
   int (*open_r)(struct _reent *r, void *fileStruct, const char *path,
         int flags, int mode);
   int (*close_r)(struct _reent *r, void *fd);
   ssize_t (*write_r)(struct _reent *r, void *fd, const char *ptr,
         size_t len);
   ssize_t (*read_r)(struct _reent *r, void *fd, char *ptr, size_t len);
   off_t (*seek_r)(struct _reent *r, void *fd, off_t pos, int dir);
   int (*fstat_r)(struct _reent *r, void *fd, struct stat *st);
   int (*stat_r)(struct _reent *r, const char *file, struct stat *st);
   int (*link_r)(struct _reent *r, const char *existing,
         const char *newLink);
   int (*unlink_r)(struct _reent *r, const char *name);
   int (*chdir_r)(struct _reent *r, const char *name);
   int (*rename_r)(struct _reent *r, const char *oldName,
         const char *newName);
   int (*mkdir_r)(struct _reent *r, const char *path, int mode);
   size_t dirStateSize;
   DIR_ITER *(*diropen_r)(struct _reent *r, DIR_ITER *dirState,
         const char *path);
   int (*dirreset_r)(struct _reent *r, DIR_ITER *dirState);
   int (*dirnext_r)(struct _reent *r, DIR_ITER *dirState, char *filename,
         struct stat *filestat);
   int (*dirclose_r)(struct _reent *r, DIR_ITER *dirState);
   int (*statvfs_r)(struct _reent *r, const char *path,
         struct statvfs *buf);
   int (*ftruncate_r)(struct _reent *r, void *fd, off_t len);
   int (*fsync_r)(struct _reent *r, void *fd);
   void *deviceData;
   int (*chmod_r)(struct _reent *r, const char *path, mode_t mode);
   int (*fchmod_r)(struct _reent *r, void *fd, mode_t mode);
   int (*rmdir_r)(struct _reent *r, const char *name);
} devoptab_t;

#define STD_MAX 16
extern const devoptab_t *devoptab_list[];

#endif
