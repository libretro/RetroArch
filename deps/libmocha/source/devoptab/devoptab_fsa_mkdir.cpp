#include "devoptab_fsa.h"
#include "logger.h"
#include <mutex>

int __fsa_mkdir(struct _reent *r,
                const char *path,
                int mode) {
    FSError status;
    char *fixedPath;

    if (!path) {
        r->_errno = EINVAL;
        return -1;
    }

    fixedPath = __fsa_fixpath(r, path);
    if (!fixedPath) {
        r->_errno = ENOMEM;
        return -1;
    }

    auto *deviceData = (FSADeviceData *) r->deviceData;

    FSMode translatedMode = __fsa_translate_permission_mode(mode);

    status = FSAMakeDir(deviceData->clientHandle, fixedPath, translatedMode);
    if (status < 0) {
        DEBUG_FUNCTION_LINE_ERR("FSAMakeDir(0x%08X, %s, 0x%X) failed: %s",
                                deviceData->clientHandle, fixedPath, translatedMode, FSAGetStatusStr(status));
        free(fixedPath);
        r->_errno = __fsa_translate_error(status);
        return -1;
    }
    free(fixedPath);

    return 0;
}
