#include "devoptab_fsa.h"
#include "logger.h"
#include <mutex>

int __fsa_fsync(struct _reent *r,
                void *fd) {
    FSError status;

    if (!fd) {
        r->_errno = EINVAL;
        return -1;
    }

    auto *file       = (__fsa_file_t *) fd;
    auto *deviceData = (FSADeviceData *) r->deviceData;

    std::lock_guard<MutexWrapper> lock(file->mutex);

    status = FSAFlushFile(deviceData->clientHandle, file->fd);
    if (status < 0) {
        DEBUG_FUNCTION_LINE_ERR("FSAFlushFile(0x%08X, 0x%08X) (%s) failed: %s",
                                deviceData->clientHandle, file->fd, file->fullPath, FSAGetStatusStr(status));
        r->_errno = __fsa_translate_error(status);
        return -1;
    }

    return 0;
}
