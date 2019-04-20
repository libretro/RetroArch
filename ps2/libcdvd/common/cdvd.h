#ifndef _CDVD_H
#define _CDVD_H

// This header contains the common definitions for libcdvd
// that are used by both IOP and EE sides

#define CDVD_IRX 0xB001337
#define CDVD_FINDFILE 0x01
#define CDVD_GETDIR 0x02
#define CDVD_STOP 0x04
#define CDVD_TRAYREQ 0x05
#define CDVD_DISKREADY 0x06
#define CDVD_FLUSHCACHE 0x07
#define CDVD_GETSIZE 0x08


struct TocEntry
{
    u32 fileLBA;
    u32 fileSize;
    u8 fileProperties;
    u8 padding1[3];
    char filename[128 + 1];
    u8 padding2[3];
} __attribute__((packed));


enum CDVD_getMode {
    CDVD_GET_FILES_ONLY = 1,
    CDVD_GET_DIRS_ONLY = 2,
    CDVD_GET_FILES_AND_DIRS = 3
};

// Macros for TrayReq
#define CdTrayOpen 0
#define CdTrayClose 1
#define CdTrayCheck 2

// Macros for DiskReady
#define CdComplete 0x02
#define CdNotReady 0x06
#define CdBlock 0x00
#define CdNonBlock 0x01

#endif  // _CDVD_H
