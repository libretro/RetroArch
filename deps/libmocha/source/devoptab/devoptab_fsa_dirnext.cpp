#include "devoptab_fsa.h"
#include "logger.h"
#include <cstdio>
#include <mutex>

int __fsa_dirnext(struct _reent *r,
                  DIR_ITER *dirState,
                  char *filename,
                  struct stat *filestat) {
    FSError status;

    if (!dirState || !filename || !filestat) {
        r->_errno = EINVAL;
        return -1;
    }

    auto *deviceData = (FSADeviceData *) r->deviceData;
    auto *dir        = (__fsa_dir_t *) (dirState->dirStruct);

    std::lock_guard<MutexWrapper> lock(dir->mutex);
    memset(&dir->entry_data, 0, sizeof(dir->entry_data));

    status = FSAReadDir(deviceData->clientHandle, dir->fd, &dir->entry_data);
    if (status < 0) {
        if (status != FS_ERROR_END_OF_DIR) {
            DEBUG_FUNCTION_LINE_ERR("FSAReadDir(0x%08X, 0x%08X, %p) (%s) failed: %s",
                                    deviceData->clientHandle, dir->fd, &dir->entry_data, dir->name, FSAGetStatusStr(status));
        }
        r->_errno = __fsa_translate_error(status);
        return -1;
    }

    __fsa_translate_stat(&dir->entry_data.info, filestat);

    if (snprintf(filename, NAME_MAX, "%s", dir->entry_data.name) >= NAME_MAX) {
        DEBUG_FUNCTION_LINE_ERR("snprintf filename result was truncated\n");
    }
    return 0;
}
