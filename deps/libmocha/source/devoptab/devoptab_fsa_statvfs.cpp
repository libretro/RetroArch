#include "devoptab_fsa.h"
#include "logger.h"
#include <mutex>

int __fsa_statvfs(struct _reent *r,
                  const char *path,
                  struct statvfs *buf) {
    FSError status;
    uint64_t freeSpace;

    memset(buf, 0, sizeof(struct statvfs));

    char *fixedPath = __fsa_fixpath(r, path);
    if (!fixedPath) {
        r->_errno = ENOMEM;
        return -1;
    }

    auto *deviceData = (FSADeviceData *) r->deviceData;

    status = FSAGetFreeSpaceSize(deviceData->clientHandle, fixedPath, &freeSpace);
    if (status < 0) {
        DEBUG_FUNCTION_LINE_ERR("FSAGetFreeSpaceSize(0x%08X, %s, %p) failed: %s",
                                deviceData->clientHandle, fixedPath, &freeSpace, FSAGetStatusStr(status));
        free(fixedPath);
        r->_errno = __fsa_translate_error(status);
        return -1;
    }
    free(fixedPath);

    // File system block size
    buf->f_bsize = deviceData->deviceSectorSize;
    // Fundamental file system block size
    buf->f_frsize = deviceData->deviceSectorSize;
    // Total number of blocks on file system in units of f_frsize
    buf->f_blocks = deviceData->deviceSizeInSectors;
    // Free blocks available for all and for non-privileged processes
    buf->f_bfree = buf->f_bavail = (uint32_t) (freeSpace / buf->f_frsize);
    // Number of inodes at this point in time
    buf->f_files = 0xFFFFFFFF;
    // Free inodes available for all and for non-privileged processes
    buf->f_ffree = 0xFFFFFFFF;
    // File system id
    buf->f_fsid = (unsigned long) deviceData->clientHandle;
    // Bit mask of f_flag values.
    buf->f_flag = 0;
    // Maximum length of filenames
    buf->f_namemax = 255;

    return 0;
}