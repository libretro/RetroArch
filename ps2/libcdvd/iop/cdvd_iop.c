#include <tamtypes.h>
#include <loadcore.h>
#include <thsemap.h>
#include <intrman.h>
#include <thbase.h>
#include <loadcore.h>
#include <sifman.h>
#include <sifcmd.h>
#include <ioman.h>
#include <cdvdman.h>
#include <sysclib.h>
#include <stdio.h>
#include <sysmem.h>

#include "cdvd_iop.h"

#define TRUE 1
#define FALSE 0

enum PathMatch {
    NOT_MATCH = 0,
    MATCH,
    SUBDIR
};

//#define DEBUG

// 16 sectors worth of toc entry
#define MAX_DIR_CACHE_SECTORS 32


//static u8 cdVolDescriptor[2048];
static sceCdRMode cdReadMode;

int lastsector;
int last_bk = 0;


struct rootDirTocHeader
{
    u16 length;
    u32 tocLBA;
    u32 tocLBA_bigend;
    u32 tocSize;
    u32 tocSize_bigend;
    u8 dateStamp[8];
    u8 reserved[6];
    u8 reserved2;
    u8 reserved3;
} __attribute__((packed));

struct asciiDate
{
    char year[4];
    char month[2];
    char day[2];
    char hours[2];
    char minutes[2];
    char seconds[2];
    char hundreths[2];
    char terminator[1];
} __attribute__((packed));

struct cdVolDesc
{
    u8 filesystemType;  // 0x01 = ISO9660, 0x02 = Joliet, 0xFF = NULL
    u8 volID[5];        // "CD001"
    u8 reserved2;
    u8 reserved3;
    u8 sysIdName[32];
    u8 volName[32];  // The ISO9660 Volume Name
    u8 reserved5[8];
    u32 volSize;     // Volume Size
    u32 volSizeBig;  // Volume Size Big-Endian
    u8 reserved6[32];
    u32 unknown1;
    u32 unknown1_bigend;
    u16 volDescSize;
    u16 volDescSize_bigend;
    u32 unknown3;
    u32 unknown3_bigend;
    u32 priDirTableLBA;  // LBA of Primary Dir Table
    u32 reserved7;
    u32 secDirTableLBA;  // LBA of Secondary Dir Table
    u32 reserved8;
    struct rootDirTocHeader rootToc;
    u8 volSetName[128];
    u8 publisherName[128];
    u8 preparerName[128];
    u8 applicationName[128];
    u8 copyrightFileName[37];
    u8 abstractFileName[37];
    u8 bibliographyFileName[37];
    struct asciiDate creationDate;
    struct asciiDate modificationDate;
    struct asciiDate effectiveDate;
    struct asciiDate expirationDate;
    u8 reserved10;
    u8 reserved11[1166];
} __attribute__((packed));

struct dirTableEntry
{
    u8 dirNameLength;
    u8 reserved;
    u32 dirTOCLBA;
    u16 dirDepth;
    u8 dirName[32];
} __attribute__((packed));

struct dirTocEntry
{
    short length;
    unsigned int fileLBA;
    unsigned int fileLBA_bigend;
    unsigned int fileSize;
    unsigned int fileSize_bigend;
    unsigned char dateStamp[6];
    unsigned char reserved1;
    unsigned char fileProperties;
    unsigned char reserved2[6];
    unsigned char filenameLength;
    unsigned char filename[128];
} __attribute__((packed));  // This is the internal format on the CD
// a file with a single character filename will have a 34byte toc entry
// (max 60 entries per sector)6

// TocEntry structure contains only the important stuff needed for export
//

struct fdtable
{
    iop_file_t *fd;
    int fileSize;
    int LBA;
    int filePos;
};


struct dir_cache_info
{
    char pathname[1024];  // The pathname of the cached directory
    unsigned int valid;   // TRUE if cache data is valid, FALSE if not

    unsigned int path_depth;  // The path depth of the cached directory (0 = root)

    unsigned int sector_start;  // The start sector (LBA) of the cached directory
    unsigned int sector_num;    // The total size of the directory (in sectors)
    unsigned int cache_offset;  // The offset from sector_start of the cached area
    unsigned int cache_size;    // The size of the cached directory area (in sectors)

    char *cache;  // The actual cached data
};


static struct dir_cache_info CachedDirInfo;

enum Cache_getMode {
    CACHE_START = 0,
    CACHE_NEXT = 1
};

static struct cdVolDesc CDVolDesc;

static unsigned int *buffer;  // RPC send/receive buffer
struct t_SifRpcDataQueue qd;
struct t_SifRpcServerData sd0;


static struct fdtable fd_table[16];
static int fd_used[16];
static int files_open;

static iop_device_t file_driver;

/* Filing-system exported functions */
int CDVD_init(iop_device_t *driver);
int CDVD_open(iop_file_t *f, const char *name, int mode);
int CDVD_lseek(iop_file_t *f, int offset, int whence);
int CDVD_read(iop_file_t *f, void *buffer, int size);
int CDVD_write(iop_file_t *f, void *buffer, int size);
int CDVD_close(iop_file_t *f);

/* RPC exported functions */
int CDVD_findfile(const char *fname, struct TocEntry *tocEntry);
int CDVD_stop(void);
int CDVD_trayreq(int mode);
int CDVD_diskready(void);
int CDVD_GetDir_RPC(const char *pathname, const char *extensions, enum CDVD_getMode getMode, struct TocEntry tocEntry[], unsigned int req_entries);

int CDVD_getdir_IOP(const char *pathname, const char *extensions, enum CDVD_getMode getMode, struct TocEntry tocEntry[], unsigned int req_entries);


// Functions called by the RPC server
void *CDVDRpc_Stop();
void *CDVDRpc_FlushCache();
void *CDVDRpc_TrayReq(unsigned int *sbuff);
void *CDVDRpc_DiskReady(unsigned int *sbuff);
void *CDVDRpc_FindFile(unsigned int *sbuff);
void *CDVDRpc_Getdir(unsigned int *sbuff);
void *CDVDRpc_GetSize(unsigned int *sbuff);


/* Internal use functions */
int isValidDisc(void);
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, int limit);
int CDVD_GetVolumeDescriptor(void);
void _splitpath(const char *constpath, char *dir, char *fname);
void TocEntryCopy(struct TocEntry *tocEntry, struct dirTocEntry *internalTocEntry);
int TocEntryCompare(char *filename, const char *extensions);

enum PathMatch ComparePath(const char *path);

int CDVD_Cache_Dir(const char *pathname, enum Cache_getMode getMode);
int FindPath(char *pathname);


void *CDVD_rpc_server(int fno, void *data, int size);
void CDVD_Thread(void *param);



/********************
* Optimised CD Read *
********************/

int ReadSect(u32 lsn, u32 sectors, void *buf, sceCdRMode *mode)
{
    int retry;
    int result = 0;
    cdReadMode.trycount = 32;

    for (retry = 0; retry < 32; retry++)  // 32 retries
    {
        if (retry <= 8)
            cdReadMode.spindlctrl = 1;  // Try fast reads for first 8 tries
        else
            cdReadMode.spindlctrl = 0;  // Then try slow reads

        if (!isValidDisc())
            return FALSE;

        sceCdDiskReady(0);

        if (sceCdRead(lsn, sectors, buf, mode) != TRUE) {
            // Failed to read
            if (retry == 31) {
                // Still failed after last retry
                memset(buf, 0, (sectors << 11));
                // printf("Couldn't Read from file for some reason\n");
                return FALSE;  // error
            }
        } else {
            // Read okay
            sceCdSync(0);
            break;
        }

        result = sceCdGetError();
        if (result == 0)
            break;
    }

    cdReadMode.trycount = 32;
    cdReadMode.spindlctrl = 1;

    if (result == 0)
        return TRUE;

    memset(buf, 0, (sectors << 11));

    return FALSE;  // error
}

/***********************************************
* Determines if there is a valid disc inserted *
***********************************************/
int isValidDisc(void)
{
    int result;

    switch (sceCdGetDiskType()) {
        case SCECdPSCD:
        case SCECdPSCDDA:
        case SCECdPS2CD:
        case SCECdPS2CDDA:
        case SCECdPS2DVD:
            result = 1;
            break;
        default:
            result = 0;
    }

    return result;
}

/*************************************************************
* The functions below are the normal file-system operations, *
* used to provide a standard filesystem interface. There is  *
* no need to export these functions for calling via RPC      *
*************************************************************/
int dummy()
{
#ifdef DEBUG
    printf("CDVD: dummy function called\n");
#endif

    return -5;
}

int CDVD_init(iop_device_t *driver)
{
    printf("CDVD: CDVD Filesystem v1.15\n");
    printf("by A.Lee (aka Hiryu) & Nicholas Van Veen (aka Sjeep)\n");
    printf("CDVD: Initializing '%s' file driver.\n", driver->name);

    sceCdInit(SCECdINoD);

    memset(fd_table, 0, sizeof(fd_table));
    memset(fd_used, 0, 16 * 4);

    return 0;
}

int CDVD_deinit(iop_device_t *driver)
{
    return 0;
}

int CDVD_open(iop_file_t *f, const char *name, int mode)
{
    int j;
    static struct TocEntry tocEntry;

#ifdef DEBUG
    printf("CDVD: fd_open called.\n");
    printf("      kernel_fd.. %p\n", f);
    printf("      name....... %s %x\n", name, (int)name);
    printf("      mode....... %d\n\n", mode);
#endif

    // check if the file exists
    if (CDVD_findfile(name, &tocEntry) != TRUE) {
        return -1;
    }

    if (mode != O_RDONLY)
        return -2;

    // set up a new file descriptor
    for (j = 0; j < 16; j++) {
        if (fd_used[j] == 0)
            break;
    }

    if (j >= 16)
        return -3;


    fd_used[j] = 1;
    files_open++;

#ifdef DEBUG
    printf("CDVD: internal fd %d\n", j);
#endif

    fd_table[j].fd = f;
    fd_table[j].fileSize = tocEntry.fileSize;
    fd_table[j].LBA = tocEntry.fileLBA;
    fd_table[j].filePos = 0;

#ifdef DEBUG
    printf("tocEntry.fileSize = %d\n", tocEntry.fileSize);

    printf("Opened file: %s\n", name);
#endif

    return j;
}



int CDVD_lseek(iop_file_t *f, int offset, int whence)
{
    int i;

#ifdef DEBUG
    printf("CDVD: fd_seek called.\n");
    printf("      kernel_fd... %p\n", f);
    printf("      offset...... %d\n", offset);
    printf("      whence...... %d\n\n", whence);
#endif

    for (i = 0; i < 16; i++) {
        if (fd_table[i].fd == f)
            break;
    }

    if (i >= 16) {
#ifdef DEBUG
        printf("CDVD_lseek: ERROR: File does not appear to be open!\n");
#endif

        return -1;
    }

    switch (whence) {
        case SEEK_SET:
            fd_table[i].filePos = offset;
            break;

        case SEEK_CUR:
            fd_table[i].filePos += offset;
            break;

        case SEEK_END:
            fd_table[i].filePos = fd_table[i].fileSize + offset;
            break;

        default:
            return -1;
    }

    if (fd_table[i].filePos < 0)
        fd_table[i].filePos = 0;

    if (fd_table[i].filePos > fd_table[i].fileSize)
        fd_table[i].filePos = fd_table[i].fileSize;

    return fd_table[i].filePos;
}


int CDVD_read(iop_file_t *f, void *buffer, int size)
{
    int i;

    int start_sector;
    int off_sector;
    int num_sectors;

    int read = 0;
    //	int sector;

    //	int size_left;
    //	int copy_size;

    static char local_buffer[9 * 2048];


#ifdef DEBUG
    printf("CDVD: read called\n");
    printf("      kernel_fd... %p\n", f);
    printf("      buffer...... 0x%X\n", (int)buffer);
    printf("      size........ %d\n\n", size);
#endif

    for (i = 0; i < 16; i++) {
        if (fd_table[i].fd == f)
            break;
    }

    if (i >= 16) {
#ifdef DEBUG
        printf("CDVD_read: ERROR: File does not appear to be open!\n");
#endif

        return -1;
    }


    // A few sanity checks
    if (fd_table[i].filePos > fd_table[i].fileSize) {
        // We cant start reading from past the beginning of the file
        return 0;  // File exists but we couldnt read anything from it
    }

    if ((fd_table[i].filePos + size) > fd_table[i].fileSize)
        size = fd_table[i].fileSize - fd_table[i].filePos;

    if (size <= 0)
        return 0;

    if (size > 16384)
        size = 16384;

    // Now work out where we want to start reading from
    start_sector = fd_table[i].LBA + (fd_table[i].filePos >> 11);
    off_sector = (fd_table[i].filePos & 0x7FF);

    num_sectors = (off_sector + size);
    num_sectors = (num_sectors >> 11) + ((num_sectors & 2047) != 0);

#ifdef DEBUG
    printf("CDVD_read: read sectors %d to %d\n", start_sector, start_sector + num_sectors);
#endif

    // Skip a Sector for equal (use the last sector in buffer)
    if (start_sector == lastsector) {
        read = 1;
        if (last_bk > 0)
            memcpy(local_buffer, local_buffer + 2048 * (last_bk), 2048);
        last_bk = 0;
    }

    lastsector = start_sector + num_sectors - 1;
    // Read the data (we only ever get 16KB max request at once)

    if (read == 0 || (read == 1 && num_sectors > 1)) {
        if (ReadSect(start_sector + read, num_sectors - read, local_buffer + ((read) << 11), &cdReadMode) != TRUE) {
#ifdef DEBUG
            printf("Couldn't Read from file for some reason\n");
#endif
        }

        last_bk = num_sectors - 1;
    }

    memcpy(buffer, local_buffer + off_sector, size);

    fd_table[i].filePos += size;

    return (size);
}


int CDVD_write(iop_file_t *f, void *buffer, int size)
{
    if (size == 0)
        return 0;
    else
        return -1;
}



int CDVD_close(iop_file_t *f)
{
    int i;

#ifdef DEBUG
    printf("CDVD: fd_close called.\n");
    printf("      kernel fd.. %p\n\n", f);
#endif

    for (i = 0; i < 16; i++) {
        if (fd_table[i].fd == f)
            break;
    }

    if (i >= 16) {
#ifdef DEBUG
        printf("CDVD_close: ERROR: File does not appear to be open!\n");
#endif

        return -1;
    }

#ifdef DEBUG
    printf("CDVD: internal fd %d\n", i);
#endif

    fd_used[i] = 0;
    files_open--;

    return 0;
}


static iop_device_ops_t filedriver_ops = {
    &CDVD_init,
    &CDVD_deinit,
    (void *)&dummy,
    &CDVD_open,
    &CDVD_close,
    &CDVD_read,
    &CDVD_write,
    &CDVD_lseek,
    (void *)&dummy,
    (void *)&dummy,
    (void *)&dummy,
    (void *)&dummy,
    (void *)&dummy,
    (void *)&dummy,
    (void *)&dummy,
    (void *)&dummy,
    (void *)&dummy};

int _start(int argc, char **argv)
{
    int i;
    struct _iop_thread param;
    int th;

    // Initialise the directory cache
    strcpy(CachedDirInfo.pathname, "");  // The pathname of the cached directory
    CachedDirInfo.valid = FALSE;         // Cache is not valid
    CachedDirInfo.path_depth = 0;        // 0 = root)
    CachedDirInfo.sector_start = 0;      // The start sector (LBA) of the cached directory
    CachedDirInfo.sector_num = 0;        // The total size of the directory (in sectors)
    CachedDirInfo.cache_offset = 0;      // The offset from sector_start of the cached area
    CachedDirInfo.cache_size = 0;        // The size of the cached directory area (in sectors)

    if (CachedDirInfo.cache == NULL)
        CachedDirInfo.cache = (char *)AllocSysMemory(0, MAX_DIR_CACHE_SECTORS * 2048, NULL);


    // setup the cdReadMode structure
    cdReadMode.trycount = 0;
    cdReadMode.spindlctrl = SCECdSpinStm;
    cdReadMode.datapattern = SCECdSecS2048;

    // setup the file_driver structure
    file_driver.name = "cdfs";
    file_driver.type = IOP_DT_FS;
    file_driver.version = 1;
    file_driver.desc = "CDVD Filedriver";
    file_driver.ops = &filedriver_ops;

    DelDrv("cdfs");
    AddDrv(&file_driver);

    param.attr = TH_C;
    param.thread = (void *)CDVD_Thread;
    param.priority = 40;
    param.stacksize = 0x8000;
    param.option = 0;

    th = CreateThread(&param);

    if (th > 0) {
        StartThread(th, NULL);
        return MODULE_RESIDENT_END;
    } else
        return MODULE_NO_RESIDENT_END;
}

/**************************************************************
* The functions below are not exported for normal file-system *
* operations, but are used by the file-system operations, and *
* may also be exported  for use via RPC                       *
**************************************************************/


int CDVD_GetVolumeDescriptor(void)
{
    // Read until we find the last valid Volume Descriptor
    int volDescSector;

    static struct cdVolDesc localVolDesc;

#ifdef DEBUG
    printf("CDVD_GetVolumeDescriptor called\n");
#endif

    for (volDescSector = 16; volDescSector < 20; volDescSector++) {
        ReadSect(volDescSector, 1, &localVolDesc, &cdReadMode);

        // If this is still a volume Descriptor
        if (strncmp(localVolDesc.volID, "CD001", 5) == 0) {
            if ((localVolDesc.filesystemType == 1) ||
                (localVolDesc.filesystemType == 2)) {
                memcpy(&CDVolDesc, &localVolDesc, sizeof(struct cdVolDesc));
            }
        } else
            break;
    }

#ifdef DEBUG
    switch (CDVolDesc.filesystemType) {
        case 1:
            printf("CD FileSystem is ISO9660\n");
            break;

        case 2:
            printf("CD FileSystem is Joliet\n");
            break;

        default:
            printf("CD FileSystem is unknown type\n");
            break;
    }
#endif
    //	sceCdStop();

    return TRUE;
}


int CDVD_findfile(const char *fname, struct TocEntry *tocEntry)
{
    static char filename[128 + 1];
    static char pathname[1024 + 1];

    struct dirTocEntry *tocEntryPointer;

#ifdef DEBUG
    printf("CDVD_findfile called\n");
#endif

    _splitpath(fname, pathname, filename);

#ifdef DEBUG
    printf("Trying to find file: %s in directory: %s\n", filename, pathname);
#endif

    //	if ((CachedDirInfo.valid==TRUE)
    //		&& (strcasecmp(pathname, CachedDirInfo.pathname)==0))

    if ((CachedDirInfo.valid == TRUE) && (ComparePath(pathname) == MATCH)) {
        // the directory is already cached, so check through the currently
        // cached chunk of the directory first

        tocEntryPointer = (struct dirTocEntry *)CachedDirInfo.cache;

        for (; tocEntryPointer < (struct dirTocEntry *)(CachedDirInfo.cache + (CachedDirInfo.cache_size * 2048)); tocEntryPointer = (struct dirTocEntry *)((u8 *)tocEntryPointer + tocEntryPointer->length)) {
            if (tocEntryPointer->length == 0) {
#ifdef DEBUG
                printf("Got a null pointer entry, so either reached end of dir, or end of sector\n");
#endif

                tocEntryPointer = (struct dirTocEntry *)(CachedDirInfo.cache + (((((char *)tocEntryPointer - CachedDirInfo.cache) / 2048) + 1) * 2048));
            }

            if (tocEntryPointer >= (struct dirTocEntry *)(CachedDirInfo.cache + (CachedDirInfo.cache_size * 2048))) {
                // reached the end of the cache block
                break;
            }

            if ((tocEntryPointer->fileProperties & 0x02) == 0) {
                // It's a file
                TocEntryCopy(tocEntry, tocEntryPointer);

                if (strcasecmp(tocEntry->filename, filename) == 0) {
                    // and it matches !!
                    return TRUE;
                }
            }
        }  // end of for loop



        // If that was the only dir block, and we havent found it, then fail
        if (CachedDirInfo.cache_size == CachedDirInfo.sector_num)
            return FALSE;

        // Otherwise there is more dir to check
        if (CachedDirInfo.cache_offset == 0) {
            // If that was the first block then continue with the next block
            if (CDVD_Cache_Dir(pathname, CACHE_NEXT) != TRUE)
                return FALSE;
        } else {
            // otherwise (if that wasnt the first block) then start checking from the start
            if (CDVD_Cache_Dir(pathname, CACHE_START) != TRUE)
                return FALSE;
        }
    } else {
#ifdef DEBUG
        printf("Trying to cache directory\n");
#endif
        // The wanted directory wasnt already cached, so cache it now
        if (CDVD_Cache_Dir(pathname, CACHE_START) != TRUE) {
#ifdef DEBUG
            printf("Failed to cache directory\n");
#endif

            return FALSE;
        }
    }

// If we've got here, then we have a block of the directory cached, and want to check
// from this point, to the end of the dir
#ifdef DEBUG
    printf("cache_size = %d\n", CachedDirInfo.cache_size);
#endif

    while (CachedDirInfo.cache_size > 0) {
        tocEntryPointer = (struct dirTocEntry *)CachedDirInfo.cache;

        if (CachedDirInfo.cache_offset == 0)
            tocEntryPointer = (struct dirTocEntry *)((u8 *)tocEntryPointer + tocEntryPointer->length);

        for (; tocEntryPointer < (struct dirTocEntry *)(CachedDirInfo.cache + (CachedDirInfo.cache_size * 2048)); tocEntryPointer = (struct dirTocEntry *)((u8 *)tocEntryPointer + tocEntryPointer->length)) {
            if (tocEntryPointer->length == 0) {
#ifdef DEBUG
                printf("Got a null pointer entry, so either reached end of dir, or end of sector\n");
                printf("Offset into cache = %d bytes\n", (char *)tocEntryPointer - CachedDirInfo.cache);
#endif

                tocEntryPointer = (struct dirTocEntry *)(CachedDirInfo.cache + (((((char *)tocEntryPointer - CachedDirInfo.cache) / 2048) + 1) * 2048));
            }

            if (tocEntryPointer >= (struct dirTocEntry *)(CachedDirInfo.cache + (CachedDirInfo.cache_size * 2048))) {
                // reached the end of the cache block
                break;
            }

            TocEntryCopy(tocEntry, tocEntryPointer);

            if (strcasecmp(tocEntry->filename, filename) == 0) {
#ifdef DEBUG
                printf("Found a matching file\n");
#endif
                // and it matches !!
                return TRUE;
            }

#ifdef DEBUG
            printf("Non-matching file - looking for %s , found %s\n", filename, tocEntry->filename);
#endif
        }  // end of for loop

#ifdef DEBUG
        printf("Reached end of cache block\n");
#endif
        // cache the next block
        CDVD_Cache_Dir(pathname, CACHE_NEXT);
    }

// we've run out of dir blocks to cache, and still not found it, so fail

#ifdef DEBUG
    printf("CDVD_findfile: could not find file\n");
#endif

    return FALSE;
}



// Find, and cache, the requested directory, for use by GetDir or  (and thus open)
// provide an optional offset variable, for use when caching dirs of greater than 500 files

// returns TRUE if all TOC entries have been retrieved, or
// returns FALSE if there are more TOC entries to be retrieved
int CDVD_Cache_Dir(const char *pathname, enum Cache_getMode getMode)
{

    // macke sure that the requested pathname is not directly modified
    static char dirname[1024];

    int path_len;

#ifdef DEBUG
    printf("Attempting to find, and cache, directory: %s\n", pathname);
#endif

    // only take any notice of the existing cache, if it's valid
    if (CachedDirInfo.valid == TRUE) {
        // Check if the requested path is already cached
        //		if (strcasecmp(pathname,CachedDirInfo.pathname)==0)
        if (ComparePath(pathname) == MATCH) {
#ifdef DEBUG
            printf("CacheDir: The requested path is already cached\n");
#endif

            // If so, is the request ot cache the start of the directory, or to resume the next block ?
            if (getMode == CACHE_START) {
#ifdef DEBUG
                printf("          and requested cache from start of dir\n");
#endif

                if (CachedDirInfo.cache_offset == 0) {
// requested cache of start of the directory, and thats what's already cached
// so sit back and do nothing
#ifdef DEBUG
                    printf("          and start of dir is already cached so nothing to do :o)\n");
#endif

                    CachedDirInfo.valid = TRUE;
                    return TRUE;
                } else {
// Requested cache of start of the directory, but thats not what's cached
// so re-cache the start of the directory

#ifdef DEBUG
                    printf("          but dir isn't cached from start, so re-cache existing dir from start\n");
#endif

                    // reset cache data to start of existing directory
                    CachedDirInfo.cache_offset = 0;
                    CachedDirInfo.cache_size = CachedDirInfo.sector_num;

                    if (CachedDirInfo.cache_size > MAX_DIR_CACHE_SECTORS)
                        CachedDirInfo.cache_size = MAX_DIR_CACHE_SECTORS;

                    // Now fill the cache with the specified sectors
                    if (ReadSect(CachedDirInfo.sector_start + CachedDirInfo.cache_offset, CachedDirInfo.cache_size, CachedDirInfo.cache, &cdReadMode) != TRUE) {
#ifdef DEBUG
                        printf("Couldn't Read from CD !\n");
#endif

                        CachedDirInfo.valid = FALSE;  // should we completely invalidate just because we couldnt read first time?
                        return FALSE;
                    }

                    CachedDirInfo.valid = TRUE;
                    return TRUE;
                }
            } else  // getMode == CACHE_NEXT
            {
                // So get the next block of the existing directory

                CachedDirInfo.cache_offset += CachedDirInfo.cache_size;

                CachedDirInfo.cache_size = CachedDirInfo.sector_num - CachedDirInfo.cache_offset;

                if (CachedDirInfo.cache_size > MAX_DIR_CACHE_SECTORS)
                    CachedDirInfo.cache_size = MAX_DIR_CACHE_SECTORS;

                // Now fill the cache with the specified sectors
                if (ReadSect(CachedDirInfo.sector_start + CachedDirInfo.cache_offset, CachedDirInfo.cache_size, CachedDirInfo.cache, &cdReadMode) != TRUE) {
#ifdef DEBUG
                    printf("Couldn't Read from CD !\n");
#endif

                    CachedDirInfo.valid = FALSE;  // should we completely invalidate just because we couldnt read first time?
                    return FALSE;
                }

                CachedDirInfo.valid = TRUE;
                return TRUE;
            }
        } else  // requested directory is not the cached directory (but cache is still valid)
        {
#ifdef DEBUG
            printf("Cache is valid, but cached directory, is not the requested one\n"
                   "so check if the requested directory is a sub-dir of the cached one\n");

            printf("Requested Path = %s , Cached Path = %s\n", pathname, CachedDirInfo.pathname);
#endif


            // Is the requested pathname a sub-directory of the current-directory ?

            // if the requested pathname is longer than the pathname of the cached dir
            // and the pathname of the cached dir matches the beginning of the requested pathname
            // and the next character in the requested pathname is a dir seperator
            //			printf("Length of Cached pathname = %d, length of req'd pathname = %d\n",path_len, strlen(pathname));
            //			printf("Result of strncasecmp = %d\n",strncasecmp(pathname, CachedDirInfo.pathname, path_len));
            //			printf("next character after length of cached name = %c\n",pathname[path_len]);

            //			if ((strlen(pathname) > path_len)
            //				&& (strncasecmp(pathname, CachedDirInfo.pathname, path_len)==0)
            //				&& ((pathname[path_len]=='/') || (pathname[path_len]=='\\')))

            if (ComparePath(pathname) == SUBDIR) {
// If so then we can start our search for the path, from the currently cached directory
#ifdef DEBUG
                printf("Requested dir is a sub-dir of the cached directory,\n"
                       "so start search from current cached dir\n");
#endif
                // if the cached chunk, is not the start of the dir,
                // then we will need to re-load it before starting search
                if (CachedDirInfo.cache_offset != 0) {
                    CachedDirInfo.cache_offset = 0;
                    CachedDirInfo.cache_size = CachedDirInfo.sector_num;
                    if (CachedDirInfo.cache_size > MAX_DIR_CACHE_SECTORS)
                        CachedDirInfo.cache_size = MAX_DIR_CACHE_SECTORS;

                    // Now fill the cache with the specified sectors
                    if (ReadSect(CachedDirInfo.sector_start + CachedDirInfo.cache_offset, CachedDirInfo.cache_size, CachedDirInfo.cache, &cdReadMode) != TRUE) {
#ifdef DEBUG
                        printf("Couldn't Read from CD !\n");
#endif

                        CachedDirInfo.valid = FALSE;  // should we completely invalidate just because we couldnt read time?
                        return FALSE;
                    }
                }

                // start the search, with the path after the current directory
                path_len = strlen(CachedDirInfo.pathname);
                strcpy(dirname, pathname + path_len);

                // FindPath should use the current directory cache to start it's search
                // and should change CachedDirInfo.pathname, to the path of the dir it finds
                // it should also cache the first chunk of directory sectors,
                // and fill the contents of the other elements of CachedDirInfo appropriately

                return (FindPath(dirname));
            }
        }
    }

// If we've got here, then either the cache was not valid to start with
// or the requested path is not a subdirectory of the currently cached directory
// so lets start again
#ifdef DEBUG
    printf("The cache is not valid, or the requested directory is not a sub-dir of the cached one\n");
#endif

    if (!isValidDisc()) {
#ifdef DEBUG
        printf("No supported disc inserted.\n");
#endif

        return -1;
    }

    sceCdDiskReady(0);

    // Read the main volume descriptor
    if (CDVD_GetVolumeDescriptor() != TRUE) {
#ifdef DEBUG
        printf("Could not read the CD/DVD Volume Descriptor\n");
#endif

        return -1;
    }

#ifdef DEBUG
    printf("Read the CD Volume Descriptor\n");
#endif

    CachedDirInfo.path_depth = 0;

    strcpy(CachedDirInfo.pathname, "");

    // Setup the lba and sector size, for retrieving the root toc
    CachedDirInfo.cache_offset = 0;
    CachedDirInfo.sector_start = CDVolDesc.rootToc.tocLBA;
    CachedDirInfo.sector_num = (CDVolDesc.rootToc.tocSize >> 11) + ((CDVolDesc.rootToc.tocSize & 2047) != 0);

    CachedDirInfo.cache_size = CachedDirInfo.sector_num;

    if (CachedDirInfo.cache_size > MAX_DIR_CACHE_SECTORS)
        CachedDirInfo.cache_size = MAX_DIR_CACHE_SECTORS;


    // Now fill the cache with the specified sectors
    if (ReadSect(CachedDirInfo.sector_start + CachedDirInfo.cache_offset, CachedDirInfo.cache_size, CachedDirInfo.cache, &cdReadMode) != TRUE) {
#ifdef DEBUG
        printf("Couldn't Read from CD !\n");
#endif

        CachedDirInfo.valid = FALSE;  // should we completely invalidate just because we couldnt read time?
        return FALSE;
    }

#ifdef DEBUG
    printf("Read the first block from the root directory\n");
#endif

// FindPath should use the current directory cache to start it's search (in this case the root)
// and should change CachedDirInfo.pathname, to the path of the dir it finds
// it should also cache the first chunk of directory sectors,
// and fill the contents of the other elements of CachedDirInfo appropriately
#ifdef DEBUG
    printf("Calling FindPath\n");
#endif
    strcpy(dirname, pathname);

    return (FindPath(dirname));
}

int FindPath(char *pathname)
{
    char *dirname;
    char *seperator;

    int dir_entry;
    int found_dir;

    struct dirTocEntry *tocEntryPointer;
    struct TocEntry localTocEntry;

    dirname = strtok(pathname, "\\/");

#ifdef DEBUG
    printf("FindPath: trying to find directory %s\n", pathname);
#endif

    if (!isValidDisc())
        return FALSE;

    sceCdDiskReady(0);

    while (dirname != NULL) {
        found_dir = FALSE;

        tocEntryPointer = (struct dirTocEntry *)CachedDirInfo.cache;

        // Always skip the first entry (self-refencing entry)
        tocEntryPointer = (struct dirTocEntry *)((u8 *)tocEntryPointer + tocEntryPointer->length);

        dir_entry = 0;

        for (; tocEntryPointer < (struct dirTocEntry *)(CachedDirInfo.cache + (CachedDirInfo.cache_size * 2048)); tocEntryPointer = (struct dirTocEntry *)((u8 *)tocEntryPointer + tocEntryPointer->length)) {
            // If we have a null toc entry, then we've either reached the end of the dir, or have reached a sector boundary
            if (tocEntryPointer->length == 0) {
#ifdef DEBUG
                printf("Got a null pointer entry, so either reached end of dir, or end of sector\n");
#endif

                tocEntryPointer = (struct dirTocEntry *)(CachedDirInfo.cache + (((((char *)tocEntryPointer - CachedDirInfo.cache) / 2048) + 1) * 2048));
            }

            if (tocEntryPointer >= (struct dirTocEntry *)(CachedDirInfo.cache + (CachedDirInfo.cache_size * 2048))) {
                // If we've gone past the end of the cache
                // then check if there are more sectors to load into the cache

                if ((CachedDirInfo.cache_offset + CachedDirInfo.cache_size) < CachedDirInfo.sector_num) {
                    // If there are more sectors to load, then load them
                    CachedDirInfo.cache_offset += CachedDirInfo.cache_size;
                    CachedDirInfo.cache_size = CachedDirInfo.sector_num - CachedDirInfo.cache_offset;

                    if (CachedDirInfo.cache_size > MAX_DIR_CACHE_SECTORS)
                        CachedDirInfo.cache_size = MAX_DIR_CACHE_SECTORS;

                    if (ReadSect(CachedDirInfo.sector_start + CachedDirInfo.cache_offset, CachedDirInfo.cache_size, CachedDirInfo.cache, &cdReadMode) != TRUE) {
#ifdef DEBUG
                        printf("Couldn't Read from CD !\n");
#endif

                        CachedDirInfo.valid = FALSE;  // should we completely invalidate just because we couldnt read time?
                        return FALSE;
                    }

                    tocEntryPointer = (struct dirTocEntry *)CachedDirInfo.cache;
                } else {
                    CachedDirInfo.valid = FALSE;
                    return FALSE;
                }
            }

            // If the toc Entry is a directory ...
            if (tocEntryPointer->fileProperties & 0x02) {
                // Convert to our format (inc ascii name), for the check
                TocEntryCopy(&localTocEntry, tocEntryPointer);

                // If it's the link to the parent directory, then give it the name ".."
                if (dir_entry == 0) {
                    if (CachedDirInfo.path_depth != 0) {
#ifdef DEBUG
                        printf("First directory entry in dir, so name it '..'\n");
#endif

                        strcpy(localTocEntry.filename, "..");
                    }
                }

                // Check if this is the directory that we are looking for
                if (strcasecmp(dirname, localTocEntry.filename) == 0) {
#ifdef DEBUG
                    printf("Found the matching sub-directory\n");
#endif

                    found_dir = TRUE;

                    if (dir_entry == 0) {
                        // We've matched with the parent directory
                        // so truncate the pathname by one level

                        if (CachedDirInfo.path_depth > 0)
                            CachedDirInfo.path_depth--;

                        if (CachedDirInfo.path_depth == 0) {
                            // If at root then just clear the path to root
                            // (simpler than finding the colon seperator etc)
                            CachedDirInfo.pathname[0] = 0;
                        } else {
                            seperator = strrchr(CachedDirInfo.pathname, '/');

                            if (seperator != NULL)
                                *seperator = 0;
                        }
                    } else {
                        // otherwise append a seperator, and the matched directory
                        // to the pathname
                        strcat(CachedDirInfo.pathname, "/");

#ifdef DEBUG
                        printf("Adding '%s' to cached pathname - path depth = %d\n", dirname, CachedDirInfo.path_depth);
#endif

                        strcat(CachedDirInfo.pathname, dirname);

                        CachedDirInfo.path_depth++;
                    }

                    // Exit out of the search loop
                    // (and find the next sub-directory, if there is one)
                    break;
                } else {
#ifdef DEBUG
                    printf("Found a directory, but it doesn't match\n");
#endif
                }
            }

            dir_entry++;

        }  // end of cache block search loop


        // if we've reached here, without finding the directory, then it's not there
        if (found_dir != TRUE) {
            CachedDirInfo.valid = FALSE;
            return FALSE;
        }

        // find name of next dir
        dirname = strtok(NULL, "\\/");

        CachedDirInfo.sector_start = localTocEntry.fileLBA;
        CachedDirInfo.sector_num = (localTocEntry.fileSize >> 11) + ((CDVolDesc.rootToc.tocSize & 2047) != 0);

        // Cache the start of the found directory
        // (used in searching if this isn't the last dir,
        // or used by whatever requested the cache in the first place if it is the last dir)
        CachedDirInfo.cache_offset = 0;
        CachedDirInfo.cache_size = CachedDirInfo.sector_num;

        if (CachedDirInfo.cache_size > MAX_DIR_CACHE_SECTORS)
            CachedDirInfo.cache_size = MAX_DIR_CACHE_SECTORS;

        if (ReadSect(CachedDirInfo.sector_start + CachedDirInfo.cache_offset, CachedDirInfo.cache_size, CachedDirInfo.cache, &cdReadMode) != TRUE) {
#ifdef DEBUG
            printf("Couldn't Read from CD, trying to read %d sectors, starting at sector %d !\n",
                   CachedDirInfo.cache_size, CachedDirInfo.sector_start + CachedDirInfo.cache_offset);
#endif

            CachedDirInfo.valid = FALSE;  // should we completely invalidate just because we couldnt read time?
            return FALSE;
        }
    }

// If we've got here then we found the requested directory
#ifdef DEBUG
    printf("FindPath found the path\n");
#endif

    CachedDirInfo.valid = TRUE;
    return TRUE;
}



// This is the getdir for use by IOP clients
// fills an array of TocEntry stucts in IOP memory
int CDVD_getdir_IOP(const char *pathname, const char *extensions, enum CDVD_getMode getMode, struct TocEntry tocEntry[], unsigned int req_entries)
{
    // TO DO
    return FALSE;
}


// This is the getdir for use by the EE RPC client
// It DMA's entries to the specified buffer in EE memory
int CDVD_GetDir_RPC(const char *pathname, const char *extensions, enum CDVD_getMode getMode, struct TocEntry tocEntry[], unsigned int req_entries)
{
    int matched_entries;
    int dir_entry;

    struct TocEntry localTocEntry;

    struct dirTocEntry *tocEntryPointer;

    int intStatus;  // interrupt status - for dis/en-abling interrupts

    struct t_SifDmaTransfer dmaStruct;
    int dmaID;

    dmaID = 0;

#ifdef DEBUG
    printf("RPC GetDir Request\n");
#endif

    matched_entries = 0;

    // pre-cache the dir (and get the new pathname - in-case selected "..")
    if (CDVD_Cache_Dir(pathname, CACHE_START) != TRUE) {
#ifdef DEBUG
        printf("CDVD_GetDir_RPC - Call of CDVD_Cache_Dir failed\n");
#endif

        return -1;
    }

#ifdef DEBUG
    printf("requested directory is %d sectors\n", CachedDirInfo.sector_num);
#endif

    if ((getMode == CDVD_GET_DIRS_ONLY) || (getMode == CDVD_GET_FILES_AND_DIRS)) {
        // Cache the start of the requested directory
        if (CDVD_Cache_Dir(CachedDirInfo.pathname, CACHE_START) != TRUE) {
#ifdef DEBUG
            printf("CDVD_GetDir_RPC - Call of CDVD_Cache_Dir failed\n");
#endif

            return -1;
        }

        tocEntryPointer = (struct dirTocEntry *)CachedDirInfo.cache;

        // skip the first self-referencing entry
        tocEntryPointer = (struct dirTocEntry *)((u8 *)tocEntryPointer + tocEntryPointer->length);

        // skip the parent entry if this is the root
        if (CachedDirInfo.path_depth == 0)
            tocEntryPointer = (struct dirTocEntry *)((u8 *)tocEntryPointer + tocEntryPointer->length);

        dir_entry = 0;

        while (1) {
#ifdef DEBUG
            printf("CDVD_GetDir_RPC - inside while-loop\n");
#endif

            // parse the current cache block
            for (; tocEntryPointer < (struct dirTocEntry *)(CachedDirInfo.cache + (CachedDirInfo.cache_size * 2048)); tocEntryPointer = (struct dirTocEntry *)((u8 *)tocEntryPointer + tocEntryPointer->length)) {
                if (tocEntryPointer->length == 0) {
                    // if we have a toc entry length of zero,
                    // then we've either reached the end of the sector, or the end of the dir
                    // so point to next sector (if there is one - will be checked by next condition)

                    tocEntryPointer = (struct dirTocEntry *)(CachedDirInfo.cache + (((((char *)tocEntryPointer - CachedDirInfo.cache) / 2048) + 1) * 2048));
                }

                if (tocEntryPointer >= (struct dirTocEntry *)(CachedDirInfo.cache + (CachedDirInfo.cache_size * 2048))) {
                    // we've reached the end of the current cache block (which may be end of entire dir
                    // so just break the loop
                    break;
                }

                // Check if the current entry is a dir or a file
                if (tocEntryPointer->fileProperties & 0x02) {
#ifdef DEBUG
                    printf("We found a dir, and we want all dirs\n");
#endif

                    // wait for any previous DMA to complete
                    // before over-writing localTocEntry
                    while (sceSifDmaStat(dmaID) >= 0)
                        ;

                    TocEntryCopy(&localTocEntry, tocEntryPointer);

                    if (dir_entry == 0) {
                        if (CachedDirInfo.path_depth != 0) {
#ifdef DEBUG
                            printf("It's the first directory entry, so name it '..'\n");
#endif

                            strcpy(localTocEntry.filename, "..");
                        }
                    }

                    // DMA localTocEntry to the address specified by tocEntry[matched_entries]

                    // setup the dma struct
                    dmaStruct.src = &localTocEntry;
                    dmaStruct.dest = &tocEntry[matched_entries];
                    dmaStruct.size = sizeof(struct TocEntry);
                    dmaStruct.attr = 0;

                    // Do the DMA transfer
                    CpuSuspendIntr(&intStatus);

                    dmaID = sceSifSetDma(&dmaStruct, 1);

                    CpuResumeIntr(intStatus);

                    matched_entries++;
                } else  // it must be a file
                {
#ifdef DEBUG
                    printf("We found a file, but we dont want files (at least not yet)\n");
#endif
                }

                dir_entry++;

                if (matched_entries >= req_entries)  // if we've filled the requested buffer
                    return (matched_entries);        // then just return

            }  // end of the current cache block

            // if there is more dir to load, then load next chunk, else finish
            if ((CachedDirInfo.cache_offset + CachedDirInfo.cache_size) < CachedDirInfo.sector_num) {
                if (CDVD_Cache_Dir(CachedDirInfo.pathname, CACHE_NEXT) != TRUE) {
                    // failed to cache next block (should return TRUE even if
                    // there is no more directory, as long as a CD read didnt fail
                    return -1;
                }
            } else
                break;

            tocEntryPointer = (struct dirTocEntry *)CachedDirInfo.cache;
        }
    }

    // Next do files
    if ((getMode == CDVD_GET_FILES_ONLY) || (getMode == CDVD_GET_FILES_AND_DIRS)) {
        // Cache the start of the requested directory
        if (CDVD_Cache_Dir(CachedDirInfo.pathname, CACHE_START) != TRUE) {
#ifdef DEBUG
            printf("CDVD_GetDir_RPC - Call of CDVD_Cache_Dir failed\n");
#endif

            return -1;
        }

        tocEntryPointer = (struct dirTocEntry *)CachedDirInfo.cache;

        // skip the first self-referencing entry
        tocEntryPointer = (struct dirTocEntry *)((u8 *)tocEntryPointer + tocEntryPointer->length);

        // skip the parent entry if this is the root
        if (CachedDirInfo.path_depth == 0)
            tocEntryPointer = (struct dirTocEntry *)((u8 *)tocEntryPointer + tocEntryPointer->length);

        dir_entry = 0;

        while (1) {
#ifdef DEBUG
            printf("CDVD_GetDir_RPC - inside while-loop\n");
#endif

            // parse the current cache block
            for (; tocEntryPointer < (struct dirTocEntry *)(CachedDirInfo.cache + (CachedDirInfo.cache_size * 2048)); tocEntryPointer = (struct dirTocEntry *)((u8 *)tocEntryPointer + tocEntryPointer->length)) {
                if (tocEntryPointer->length == 0) {
                    // if we have a toc entry length of zero,
                    // then we've either reached the end of the sector, or the end of the dir
                    // so point to next sector (if there is one - will be checked by next condition)

                    tocEntryPointer = (struct dirTocEntry *)(CachedDirInfo.cache + (((((char *)tocEntryPointer - CachedDirInfo.cache) / 2048) + 1) * 2048));
                }

                if (tocEntryPointer >= (struct dirTocEntry *)(CachedDirInfo.cache + (CachedDirInfo.cache_size * 2048))) {
                    // we've reached the end of the current cache block (which may be end of entire dir
                    // so just break the loop
                    break;
                }

                // Check if the current entry is a dir or a file
                if (tocEntryPointer->fileProperties & 0x02) {
#ifdef DEBUG
                    printf("We don't want files now\n");
#endif
                } else  // it must be a file
                {
                    // wait for any previous DMA to complete
                    // before over-writing localTocEntry
                    while (sceSifDmaStat(dmaID) >= 0)
                        ;

                    TocEntryCopy(&localTocEntry, tocEntryPointer);

                    if (strlen(extensions) > 0) {
                        // check if the file matches the extension list
                        if (TocEntryCompare(localTocEntry.filename, extensions) == TRUE) {
#ifdef DEBUG
                            printf("We found a file that matches the requested extension list\n");
#endif

                            // DMA localTocEntry to the address specified by tocEntry[matched_entries]

                            // setup the dma struct
                            dmaStruct.src = &localTocEntry;
                            dmaStruct.dest = &tocEntry[matched_entries];
                            dmaStruct.size = sizeof(struct TocEntry);
                            dmaStruct.attr = 0;

                            // Do the DMA transfer
                            CpuSuspendIntr(&intStatus);

                            dmaID = sceSifSetDma(&dmaStruct, 1);

                            CpuResumeIntr(intStatus);

                            matched_entries++;
                        } else {
#ifdef DEBUG
                            printf("We found a file, but it didnt match the requested extension list\n");
#endif
                        }
                    } else  // no extension list to match against
                    {
#ifdef DEBUG
                        printf("We found a file, and there is not extension list to match against\n");
#endif

                        // DMA localTocEntry to the address specified by tocEntry[matched_entries]

                        // setup the dma struct
                        dmaStruct.src = &localTocEntry;
                        dmaStruct.dest = &tocEntry[matched_entries];
                        dmaStruct.size = sizeof(struct TocEntry);
                        dmaStruct.attr = 0;

                        // Do the DMA transfer
                        CpuSuspendIntr(&intStatus);

                        dmaID = sceSifSetDma(&dmaStruct, 1);

                        CpuResumeIntr(intStatus);

                        matched_entries++;
                    }
                }

                dir_entry++;

                if (matched_entries >= req_entries)  // if we've filled the requested buffer
                    return (matched_entries);        // then just return

            }  // end of the current cache block


            // if there is more dir to load, then load next chunk, else finish
            if ((CachedDirInfo.cache_offset + CachedDirInfo.cache_size) < CachedDirInfo.sector_num) {
                if (CDVD_Cache_Dir(CachedDirInfo.pathname, CACHE_NEXT) != TRUE) {
                    // failed to cache next block (should return TRUE even if
                    // there is no more directory, as long as a CD read didnt fail
                    return -1;
                }
            } else
                break;

            tocEntryPointer = (struct dirTocEntry *)CachedDirInfo.cache;
        }
    }
    // reached the end of the dir, before filling up the requested entries

    return (matched_entries);
}

int CdFlushCache(void)
{
    strcpy(CachedDirInfo.pathname, "");  // The pathname of the cached directory
    CachedDirInfo.valid = FALSE;         // Cache is not valid
    CachedDirInfo.path_depth = 0;        // 0 = root)
    CachedDirInfo.sector_start = 0;      // The start sector (LBA) of the cached directory
    CachedDirInfo.sector_num = 0;        // The total size of the directory (in sectors)
    CachedDirInfo.cache_offset = 0;      // The offset from sector_start of the cached area
    CachedDirInfo.cache_size = 0;        // The size of the cached directory area (in sectors)

    return TRUE;
}

unsigned int CdGetSize(void)
{
    if (CDVD_GetVolumeDescriptor() != TRUE)
        return TRUE;

    return CDVolDesc.volSize;
}

void *CDVDRpc_FlushCache()
{
    CdFlushCache();

    return NULL;
}


void *CDVDRpc_Stop()
{
    if (isValidDisc()) {
        sceCdStop();
        sceCdSync(0);
    }

    return NULL;
}

// Send:   Offset 0 = mode. Size = int
// Return: Offset 0 = traycnt. Size = int
void *CDVDRpc_TrayReq(unsigned int *sbuff)
{
    int ret;

    sceCdTrayReq(sbuff[0], (int *)&ret);

    sbuff[0] = ret;
    return sbuff;
}

// Send:   Offset 0 = mode
// Return: Offset 0 = ret val (cd status)
void *CDVDRpc_DiskReady(unsigned int *sbuff)
{
    int ret;

    if (isValidDisc())
        ret = sceCdDiskReady(sbuff[0]);
    else
        ret = -1;

    sbuff[0] = ret;
    return sbuff;
}

// Send:   Offset 0 = filename string (1024 bytes)
// Return: Offset 0 = ret val (true/false). Size = int
//         Offset 1024 = start of TocEntry structure
void *CDVDRpc_FindFile(unsigned int *sbuff)
{
    int ret;

    ret = CDVD_findfile((char *)&sbuff[0], (struct TocEntry *)&sbuff[1024 / 4]);

    sbuff[0] = ret;

    return sbuff;
}

// Send:   Offset 0 = filename string (1024 bytes)
// Send:   Offset 1024 = extension string (128 bytes)
// Send:   Offset 1152 = CDVD_getMode
// Send:   Offset 1156 = pointer to array of TocEntry structures in EE mem
// Send:   Offset 1160 = requested number of entries

// Return: Offset 0 = ret val (number of matched entries). Size = int
// Return: Offset 4 = updated pathname (for if path selected = ".."
void *CDVDRpc_Getdir(unsigned int *sbuff)
{
    int ret;

    ret = CDVD_GetDir_RPC(
        (char *)&sbuff[0 / 4],               // pathname string
        (char *)&sbuff[1024 / 4],            // extension string
        sbuff[1152 / 4],                     // CDVD_getMode
        (struct TocEntry *)sbuff[1156 / 4],  // pointer to array of TocEntry structures in EE mem
        sbuff[1160 / 4]                      // requested number of entries
        );

    sbuff[0] = ret;
    strcpy((char *)&sbuff[1], CachedDirInfo.pathname);
    return sbuff;
}

void *CDVDRpc_GetSize(unsigned int *sbuff)
{
    sbuff[0] = CdGetSize();
    return sbuff;
}

/*************************************************
* The functions below are for internal use only, *
* and are not to be exported                     *
*************************************************/

void CDVD_Thread(void *param)
{
#ifdef DEBUG
    printf("CDVD: RPC Initialize\n");
#endif

    sceSifInitRpc(0);

    // 0x4800 bytes for TocEntry structures (can fit 128 of them)
    // 0x400 bytes for the filename string
    buffer = AllocSysMemory(0, 0x4C00, NULL);
    if (buffer == NULL) {
#ifdef DEBUG
        printf("Failed to allocate memory for RPC buffer!\n");
#endif

        SleepThread();
    }

    sceSifSetRpcQueue(&qd, GetThreadId());
    sceSifRegisterRpc(&sd0, CDVD_IRX, CDVD_rpc_server, (void *)buffer, 0, 0, &qd);
    sceSifRpcLoop(&qd);
}

void *CDVD_rpc_server(int fno, void *data, int size)
{

    switch (fno) {
        case CDVD_FINDFILE:
            return CDVDRpc_FindFile((unsigned *)data);
        case CDVD_GETDIR:
            return CDVDRpc_Getdir((unsigned *)data);
        case CDVD_STOP:
            return CDVDRpc_Stop();
        case CDVD_TRAYREQ:
            return CDVDRpc_TrayReq((unsigned *)data);
        case CDVD_DISKREADY:
            return CDVDRpc_DiskReady((unsigned *)data);
        case CDVD_FLUSHCACHE:
            return CDVDRpc_FlushCache();
    }

    return NULL;
}

void _splitpath(const char *constpath, char *dir, char *fname)
{
    // 255 char max path-length is an ISO9660 restriction
    // we must change this for Joliet or relaxed iso restriction support
    static char pathcopy[1024 + 1];

    char *slash;

    strncpy(pathcopy, constpath, 1024);

    slash = strrchr(pathcopy, '/');

    // if the path doesn't contain a '/' then look for a '\'
    if (!slash)
        slash = strrchr(pathcopy, (int)'\\');

    // if a slash was found
    if (slash != NULL) {
        // null terminate the path
        slash[0] = 0;
        // and copy the path into 'dir'
        strncpy(dir, pathcopy, 1024);
        dir[255] = 0;

        // copy the filename into 'fname'
        strncpy(fname, slash + 1, 128);
        fname[128] = 0;
    } else {
        dir[0] = 0;

        strncpy(fname, pathcopy, 128);
        fname[128] = 0;
    }
}

// Copy a TOC Entry from the CD native format to our tidier format
void TocEntryCopy(struct TocEntry *tocEntry, struct dirTocEntry *internalTocEntry)
{
    int i;
    int filenamelen;

    tocEntry->fileSize = internalTocEntry->fileSize;
    tocEntry->fileLBA = internalTocEntry->fileLBA;
    tocEntry->fileProperties = internalTocEntry->fileProperties;

    if (CDVolDesc.filesystemType == 2) {
        // This is a Joliet Filesystem, so use Unicode to ISO string copy
        filenamelen = internalTocEntry->filenameLength / 2;

        for (i = 0; i < filenamelen; i++)
            tocEntry->filename[i] = internalTocEntry->filename[(i << 1) + 1];
    } else {
        filenamelen = internalTocEntry->filenameLength;

        // use normal string copy
        strncpy(tocEntry->filename, internalTocEntry->filename, 128);
    }

    tocEntry->filename[filenamelen] = 0;

    if (!(tocEntry->fileProperties & 0x02)) {
        // strip the ;1 from the filename (if it's there)
        strtok(tocEntry->filename, ";");
    }
}

// Check if a TOC Entry matches our extension list
int TocEntryCompare(char *filename, const char *extensions)
{
    static char ext_list[129];

    char *token;

    char *ext_point;

    strncpy(ext_list, extensions, 128);
    ext_list[128] = 0;

    token = strtok(ext_list, " ,");
    while (token != NULL) {
        // if 'token' matches extension of 'filename'
        // then return a match
        ext_point = strrchr(filename, '.');

        if (strcasecmp(ext_point, token) == 0)
            return (TRUE);

        /* Get next token: */
        token = strtok(NULL, " ,");
    }

    // If not match found then return FALSE
    return (FALSE);
}

// Used in findfile
//int tolower(int c);
int strcasecmp(const char *s1, const char *s2)
{
    while (*s1 != '\0' && tolower(*s1) == tolower(*s2)) {
        s1++;
        s2++;
    }

    return tolower(*(unsigned char *)s1) - tolower(*(unsigned char *)s2);
}

int strncasecmp(const char *s1, const char *s2, int limit)
{
    int i;

    for (i = 0; i < limit; i++) {
        if (*s1 == '\0')
            return tolower(*(unsigned char *)s1) - tolower(*(unsigned char *)s2);

        if (tolower(*s1) != tolower(*s2))
            return tolower(*(unsigned char *)s1) - tolower(*(unsigned char *)s2);

        s1++;
        s2++;
    }

    return 0;
}

enum PathMatch ComparePath(const char *path)
{
    int length;
    int i;

    length = strlen(CachedDirInfo.pathname);

    for (i = 0; i < length; i++) {
        // check if character matches
        if (path[i] != CachedDirInfo.pathname[i]) {
            // if not, then is it just because of different path seperator ?
            if ((path[i] == '/') || (path[i] == '\\')) {
                if ((CachedDirInfo.pathname[i] == '/') || (CachedDirInfo.pathname[i] == '\\')) {
                    continue;
                }
            }

            // if the characters don't match for any other reason then report a failure
            return NOT_MATCH;
        }
    }

    // Reached the end of the Cached pathname

    // if requested path is same length, then report exact match
    if (path[length] == 0)
        return MATCH;

    // if requested path is longer, and next char is a dir seperator
    // then report sub-dir match
    if ((path[length] == '/') || (path[length] == '\\'))
        return SUBDIR;
    else
        return NOT_MATCH;
}
