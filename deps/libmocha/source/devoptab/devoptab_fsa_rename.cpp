#include "devoptab_fsa.h"
#include "logger.h"
#include <mutex>

int __fsa_rename(struct _reent *r,
                 const char *oldName,
                 const char *newName) {
    FSError status;
    char *fixedOldPath, *fixedNewPath;

    if (!oldName || !newName) {
        r->_errno = EINVAL;
        return -1;
    }

    fixedOldPath = __fsa_fixpath(r, oldName);
    if (!fixedOldPath) {
        r->_errno = ENOMEM;
        return -1;
    }

    fixedNewPath = __fsa_fixpath(r, newName);
    if (!fixedNewPath) {
        free(fixedOldPath);
        r->_errno = ENOMEM;
        return -1;
    }

    auto *deviceData = (FSADeviceData *) r->deviceData;

    status = FSARename(deviceData->clientHandle, fixedOldPath, fixedNewPath);
    if (status < 0) {
        DEBUG_FUNCTION_LINE_ERR("FSARename(0x%08X, %s, %s) failed: %s",
                                deviceData->clientHandle, fixedOldPath, fixedNewPath, FSAGetStatusStr(status));
        free(fixedOldPath);
        free(fixedNewPath);
        r->_errno = __fsa_translate_error(status);
        return -1;
    }
    free(fixedOldPath);
    free(fixedNewPath);

    return 0;
}
