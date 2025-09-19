#include "devoptab_fsa.h"
#include "logger.h"
#include <mutex>

int __fsa_stat(struct _reent *r,
               const char *path,
               struct stat *st) {
    FSError status;
    FSAStat fsStat;

    if (!path || !st) {
        r->_errno = EINVAL;
        return -1;
    }

    char *fixedPath = __fsa_fixpath(r, path);
    if (!fixedPath) {
        r->_errno = ENOMEM;
        return -1;
    }

    auto *deviceData = (FSADeviceData *) r->deviceData;

    status = FSAGetStat(deviceData->clientHandle, fixedPath, &fsStat);
    if (status < 0) {
        DEBUG_FUNCTION_LINE_ERR("FSAGetStat(0x%08X, %s, %p) failed: %s",
                                deviceData->clientHandle, fixedPath, &fsStat, FSAGetStatusStr(status));
        free(fixedPath);
        r->_errno = __fsa_translate_error(status);
        return -1;
    }
    free(fixedPath);

    __fsa_translate_stat(&fsStat, st);
    return 0;
}