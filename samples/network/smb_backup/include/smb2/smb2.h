/* Stand-in for libsmb2's <smb2/smb2.h>: the types and constants
 * network/cloud_sync/smb.c and the in-memory share in fake_smb.c use.
 * The sample drives the driver as shipped against the fake, so no
 * libsmb2 has to be installed to run it; the shapes here follow the
 * library's public headers. */
#ifndef SMB_BACKUP_STANDIN_SMB2_H
#define SMB_BACKUP_STANDIN_SMB2_H

#include <stdint.h>

#define SMB2_NEGOTIATE_SIGNING_ENABLED 0x0001

#define SMB2_TYPE_FILE      0x00000000
#define SMB2_TYPE_DIRECTORY 0x00000001
#define SMB2_TYPE_LINK      0x00000002

struct smb2_stat_64
{
   uint32_t smb2_type;
   uint32_t smb2_nlink;
   uint64_t smb2_ino;
   uint64_t smb2_size;
   uint64_t smb2_atime;
   uint64_t smb2_atime_nsec;
   uint64_t smb2_mtime;
   uint64_t smb2_mtime_nsec;
   uint64_t smb2_ctime;
   uint64_t smb2_ctime_nsec;
   uint64_t smb2_btime;
   uint64_t smb2_btime_nsec;
};

#endif
