#include "devoptab_fsa.h"
#include "logger.h"
#include <mutex>

int __fsa_chdir(struct _reent *r,
                const char *path) {
    FSError status;

    if (!path) {
        r->_errno = EINVAL;
        return -1;
    }

    char *fixedPath = __fsa_fixpath(r, path);
    if (!fixedPath) {
        r->_errno = ENOMEM;
        return -1;
    }
    auto *deviceData = (FSADeviceData *) r->deviceData;

    status = FSAChangeDir(deviceData->clientHandle, fixedPath);
    if (status < 0) {
        DEBUG_FUNCTION_LINE_ERR("FSAChangeDir(0x%08X, %s) failed: %s", deviceData->clientHandle, fixedPath, FSAGetStatusStr(status));
        free(fixedPath);
        r->_errno = __fsa_translate_error(status);
        return -1;
    }
    free(fixedPath);

    return 0;
}
