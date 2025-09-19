#include "devoptab_fsa.h"
#include "logger.h"
#include <mutex>

off_t __fsa_seek(struct _reent *r,
                 void *fd,
                 off_t pos,
                 int whence) {
    FSError status;
    FSAStat fsStat;
    uint64_t offset;

    if (!fd) {
        r->_errno = EINVAL;
        return -1;
    }

    auto *file       = (__fsa_file_t *) fd;
    auto *deviceData = (FSADeviceData *) r->deviceData;

    std::lock_guard<MutexWrapper> lock(file->mutex);

    // Find the offset to see from
    switch (whence) {
        case SEEK_SET: { // Set absolute position; start offset is 0
            offset = 0;
            break;
        }
        case SEEK_CUR: { // Set position relative to the current position
            offset = file->offset;
            break;
        }
        case SEEK_END: { // Set position relative to the end of the file
            status = FSAGetStatFile(deviceData->clientHandle, file->fd, &fsStat);
            if (status < 0) {
                DEBUG_FUNCTION_LINE_ERR("FSAGetStatFile(0x%08X, 0x%08X, %p) (%s) failed: %s",
                                        deviceData->clientHandle, file->fd, &fsStat, file->fullPath, FSAGetStatusStr(status));
                r->_errno = __fsa_translate_error(status);
                return -1;
            }
            offset = fsStat.size;
            break;
        }
        default: { // An invalid option was provided
            r->_errno = EINVAL;
            return -1;
        }
    }

    if (pos < 0 && (off_t) offset < -pos) {
        // Don't allow seek to before the beginning of the file
        r->_errno = EINVAL;
        return -1;
    } else if (offset + pos > UINT32_MAX) {
        // Check for overflow
        r->_errno = EINVAL;
        return -1;
    }

    if ((uint32_t) (offset + pos) == file->offset) {
        return file->offset;
    }

    uint32_t old_pos = file->offset;
    file->offset     = offset + pos;

    status = FSASetPosFile(deviceData->clientHandle, file->fd, file->offset);
    if (status < 0) {
        DEBUG_FUNCTION_LINE_ERR("FSASetPosFile(0x%08X, 0x%08X, 0x%08X) (%s) failed: %s",
                                deviceData->clientHandle, file->fd, file->offset, file->fullPath, FSAGetStatusStr(status));
        file->offset = old_pos;
        r->_errno    = __fsa_translate_error(status);
        return -1;
    }

    return file->offset;
}
