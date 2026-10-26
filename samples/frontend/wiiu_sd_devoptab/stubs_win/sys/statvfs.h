/* <sys/statvfs.h> for the Windows build of this lane: the fields the
 * devoptab's statvfs fills. */
#ifndef WIIU_SD_DEVOPTAB_TEST_STATVFS_H
#define WIIU_SD_DEVOPTAB_TEST_STATVFS_H
typedef unsigned long fsblkcnt_t;
typedef unsigned long fsfilcnt_t;
struct statvfs {
   unsigned long f_bsize, f_frsize;
   fsblkcnt_t    f_blocks, f_bfree, f_bavail;
   fsfilcnt_t    f_files, f_ffree, f_favail;
   unsigned long f_fsid, f_flag, f_namemax;
};
#define ST_NOSUID 2
#endif
