#include "devoptab_fsa.h"
#include "logger.h"
#include <mutex>

ssize_t __fsa_write(struct _reent *r, void *fd, const char *ptr, size_t len) {
    FSError status;

    if (!fd || !ptr) {
        r->_errno = EINVAL;
        return -1;
    }

    // Check that the file was opened with write access
    auto *file = (__fsa_file_t *) fd;
    if ((file->flags & O_ACCMODE) == O_RDONLY) {
        r->_errno = EBADF;
        return -1;
    }

    // cache-aligned, cache-line-sized
    __attribute__((aligned(0x40))) uint8_t alignedBuffer[0x40];

    auto *deviceData = (FSADeviceData *) r->deviceData;

    std::lock_guard<MutexWrapper> lock(file->mutex);

    // If O_APPEND is set, we always write to the end of the file.
    // When writing we file->offset to the file size to keep in sync.
    if (file->flags & O_APPEND) {
        file->offset = file->appendOffset;
    }

    size_t bytesWritten = 0;
    while (bytesWritten < len) {
        // only use input buffer if cache-aligned and write size is a multiple of cache line size
        // otherwise write from alignedBuffer
        uint8_t *tmp = (uint8_t *) ptr;
        size_t size  = len - bytesWritten;

        if ((uintptr_t) ptr & 0x3F) {
            // write partial cache-line front-end
            tmp  = alignedBuffer;
            size = MIN(size, 0x40 - ((uintptr_t) ptr & 0x3F));
        } else if (size < 0x40) {
            // write partial cache-line back-end
            tmp = alignedBuffer;
        } else {
            // write whole cache lines
            size &= ~0x3F;
        }

        // Limit each request to 256 KiB
        if (size > 0x40000) {
            size = 0x40000;
        }

        if (tmp == alignedBuffer) {
            memcpy(tmp, ptr, size);
        }

        status = FSAWriteFile(deviceData->clientHandle, tmp, 1, size, file->fd, 0);
        if (status < 0) {
            DEBUG_FUNCTION_LINE_ERR("FSAWriteFile(0x%08X, %p, 1, 0x%08X, 0x%08X, 0) (%s) failed: %s",
                                    deviceData->clientHandle, tmp, size, file->fd, file->fullPath, FSAGetStatusStr(status));
            if (bytesWritten != 0) {
                return bytesWritten; // error after partial write
            }

            r->_errno = __fsa_translate_error(status);
            return -1;
        }

        file->appendOffset += status;
        file->offset += status;
        bytesWritten += status;
        ptr += status;

        if ((size_t) status != size) {
            return bytesWritten; // partial write
        }
    }

    return bytesWritten;
}
