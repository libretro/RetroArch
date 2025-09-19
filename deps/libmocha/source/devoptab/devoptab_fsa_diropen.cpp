#include "devoptab_fsa.h"
#include "logger.h"
#include <mutex>

DIR_ITER *
__fsa_diropen(struct _reent *r,
              DIR_ITER *dirState,
              const char *path) {
    FSADirectoryHandle fd;
    FSError status;

    if (!dirState || !path) {
        r->_errno = EINVAL;
        return nullptr;
    }

    char *fixedPath = __fsa_fixpath(r, path);
    if (!fixedPath) {
        return nullptr;
    }
    auto *dir = (__fsa_dir_t *) (dirState->dirStruct);
    strncpy(dir->name, fixedPath, sizeof(dir->name) - 1);
    free(fixedPath);

    dir->mutex.init(dir->name);
    std::lock_guard<MutexWrapper> lock(dir->mutex);

    auto *deviceData = (FSADeviceData *) r->deviceData;
    status           = FSAOpenDir(deviceData->clientHandle, dir->name, &fd);
    if (status < 0) {
        DEBUG_FUNCTION_LINE_ERR("FSAOpenDir(0x%08X, %s, %p) failed: %s",
                                deviceData->clientHandle, dir->name, &fd, FSAGetStatusStr(status));
        r->_errno = __fsa_translate_error(status);
        return nullptr;
    }

    dir->magic = FSA_DIRITER_MAGIC;
    dir->fd    = fd;
    memset(&dir->entry_data, 0, sizeof(dir->entry_data));
    return dirState;
}
