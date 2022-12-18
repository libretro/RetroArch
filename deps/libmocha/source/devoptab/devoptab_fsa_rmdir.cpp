#include "devoptab_fsa.h"
#include "logger.h"
#include <mutex>

int __fsa_rmdir(struct _reent *r,
                const char *name) {
    FSError status;

    if (!name) {
        r->_errno = EINVAL;
        return -1;
    }

    char *fixedPath = __fsa_fixpath(r, name);
    if (!fixedPath) {
        r->_errno = ENOMEM;
        return -1;
    }

    auto *deviceData = (FSADeviceData *) r->deviceData;

    status = FSARemove(deviceData->clientHandle, fixedPath);
    if (status < 0) {
        DEBUG_FUNCTION_LINE_ERR("FSARemove(0x%08X, %s) failed: %s",
                                deviceData->clientHandle, fixedPath, FSAGetStatusStr(status));
        free(fixedPath);
        r->_errno = status == FS_ERROR_ALREADY_EXISTS ? ENOTEMPTY : __fsa_translate_error(status);
        return -1;
    }

    free(fixedPath);

    return 0;
}
