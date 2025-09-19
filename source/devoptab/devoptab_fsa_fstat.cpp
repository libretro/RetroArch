#include "devoptab_fsa.h"
#include "logger.h"
#include <mutex>

int __fsa_fstat(struct _reent *r,
                void *fd,
                struct stat *st) {
    FSError status;
    FSAStat fsStat;

    if (!fd || !st) {
        r->_errno = EINVAL;
        return -1;
    }

    auto *file       = (__fsa_file_t *) fd;
    auto *deviceData = (FSADeviceData *) r->deviceData;

    std::lock_guard<MutexWrapper> lock(file->mutex);

    status = FSAGetStatFile(deviceData->clientHandle, file->fd, &fsStat);
    if (status < 0) {
        DEBUG_FUNCTION_LINE_ERR("FSAGetStatFile(0x%08X, 0x%08X, %p) (%s) failed: %s",
                                deviceData->clientHandle, file->fd, &fsStat, file->fullPath, FSAGetStatusStr(status));
        r->_errno = __fsa_translate_error(status);
        return -1;
    }

    __fsa_translate_stat(&fsStat, st);

    return 0;
}
