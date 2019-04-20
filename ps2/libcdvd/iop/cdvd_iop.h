#ifndef _CDVD_IOP_H
#define _CDVD_IOP_H

#include "../common/cdvd.h"

// Macros for READ Data pattan
#define CdSecS2048 0  // sector size 2048
#define CdSecS2328 1  // sector size 2328
#define CdSecS2340 2  // sector size 2340

// Macros for Spindle control
#define CdSpinMax 0
#define CdSpinNom 1  // Starts reading data at maximum rotational velocity and if a read error occurs, the rotational velocity is reduced.
#define CdSpinStm 0  // Recommended stream rotation speed.

typedef struct
{
    u8 stat;    // 0: normal. Any other: error
    u8 second;  // second (BCD value)
    u8 minute;  // minute (BCD value)
    u8 hour;    // hour (BCD value)
    u8 week;    // week (BCD value)
    u8 day;     // day (BCD value)
    u8 month;   // month (BCD value)
    u8 year;    // year (BCD value)
} CdCLOCK;

typedef struct
{
    u32 lsn;        // Logical sector number of file
    u32 size;       // File size (in bytes)
    char name[16];  // Filename
    u8 date[8];     // 1th: Seconds
                    // 2th: Minutes
                    // 3th: Hours
                    // 4th: Date
                    // 5th: Month
                    // 6th 7th: Year (4 digits)
} CdlFILE;

typedef struct
{
    u8 minute;  // Minutes
    u8 second;  // Seconds
    u8 sector;  // Sector
    u8 track;   // Track number
} CdlLOCCD;

typedef struct
{
    u8 trycount;     // Read try count (No. of error retries + 1) (0 - 255)
    u8 spindlctrl;   // SCECdSpinStm: Recommended stream rotation speed.
                     // SCECdSpinNom: Starts reading data at maximum rotational velocity and if a read error occurs, the rotational velocity is reduced.
    u8 datapattern;  // SCECdSecS2048: Data size 2048 bytes
                     // SCECdSecS2328: 2328 bytes
                     // SCECdSecS2340: 2340 bytes
    u8 pad;          // Padding data produced by alignment.
} CdRMode;


int CdBreak(void);
int CdCallback(void (*func)());
int CdDiskReady(int mode);
int CdGetDiskType(void);
int CdGetError(void);
u32 CdGetReadPos(void);
int CdGetToc(u8 *toc);
int CdInit(int init_mode);
CdlLOCCD *CdIntToPos(int i, CdlLOCCD *p);
int CdPause(void);
int CdPosToInt(CdlLOCCD *p);
int CdRead(u32 lsn, u32 sectors, void *buf, CdRMode *mode);
int CdReadClock(CdCLOCK *rtc);
int CdSearchFile(CdlFILE *fp, const char *name);
int CdSeek(u32 lsn);
int CdStandby(void);
int CdStatus(void);
int CdStop(void);
int CdSync(int mode);
int CdTrayReq(int mode, u32 *traycnt);
int CdFlushCache(void);
unsigned int CdGetSize(void);

#endif  // _CDVD_H
