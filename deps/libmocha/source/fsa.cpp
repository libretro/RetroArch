#include "mocha/fsa.h"
#include "logger.h"
#include "utils.h"
#include <coreinit/debug.h>
#include <coreinit/filesystem.h>
#include <coreinit/filesystem_fsa.h>
#include <cstring>
#include <malloc.h>

FSError FSAEx_RawOpen(FSClient *client, const char *device_path, int32_t *outHandle) {
    if (!client) {
        return FS_ERROR_INVALID_CLIENTHANDLE;
    }
    return FSAEx_RawOpenEx(FSGetClientBody(client)->clientHandle, device_path, outHandle);
}

FSError FSAEx_RawOpenEx(int clientHandle, const char *device_path, int32_t *outHandle) {
    if (!outHandle) {
        return FS_ERROR_INVALID_BUFFER;
    }
    if (!device_path) {
        return FS_ERROR_INVALID_PATH;
    }
    auto *shim = (FSAShimBuffer *) memalign(0x40, sizeof(FSAShimBuffer));
    if (!shim) {
        return FS_ERROR_INVALID_BUFFER;
    }

    shim->clientHandle   = clientHandle;
    shim->command        = FSA_COMMAND_RAW_OPEN;
    shim->ipcReqType     = FSA_IPC_REQUEST_IOCTL;
    shim->response.word0 = 0xFFFFFFFF;

    FSARequestRawOpen *requestBuffer = &shim->request.rawOpen;

    strncpy(requestBuffer->path, device_path, 0x27F);

    auto res = __FSAShimSend(shim, 0);
    if (res >= 0) {
        *outHandle = shim->response.rawOpen.handle;
    }
    free(shim);
    return res;
}

FSError FSAEx_RawClose(FSClient *client, int32_t device_handle) {
    if (!client) {
        return FS_ERROR_INVALID_CLIENTHANDLE;
    }
    return FSAEx_RawCloseEx(FSGetClientBody(client)->clientHandle, device_handle);
}

FSError FSAEx_RawCloseEx(int clientHandle, int32_t device_handle) {
    auto *buffer = (FSAShimBuffer *) memalign(0x40, sizeof(FSAShimBuffer));
    if (!buffer) {
        return FS_ERROR_INVALID_BUFFER;
    }

    buffer->clientHandle   = clientHandle;
    buffer->command        = FSA_COMMAND_RAW_CLOSE;
    buffer->ipcReqType     = FSA_IPC_REQUEST_IOCTL;
    buffer->response.word0 = 0xFFFFFFFF;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
    auto *requestBuffer = &buffer->request.rawClose;
#pragma GCC diagnostic pop

    requestBuffer->handle = device_handle;

    auto res = __FSAShimSend(buffer, 0);
    free(buffer);
    return res;
}

FSError FSAEx_RawRead(FSClient *client, void *data, uint32_t size_bytes, uint32_t cnt, uint64_t blocks_offset, int device_handle) {
    if (!client) {
        return FS_ERROR_INVALID_CLIENTHANDLE;
    }
    return FSAEx_RawReadEx(FSGetClientBody(client)->clientHandle, data, size_bytes, cnt, blocks_offset, device_handle);
}

FSError FSAEx_RawReadEx(int clientHandle, void *data, uint32_t size_bytes, uint32_t cnt, uint64_t blocks_offset, int device_handle) {
    if (data == nullptr) {
        return FS_ERROR_INVALID_BUFFER;
    }
    auto *shim = (FSAShimBuffer *) memalign(0x40, sizeof(FSAShimBuffer));
    if (!shim) {
        return FS_ERROR_INVALID_BUFFER;
    }

    shim->clientHandle = clientHandle;
    shim->ipcReqType   = FSA_IPC_REQUEST_IOCTLV;
    shim->command      = FSA_COMMAND_RAW_READ;

    shim->ioctlvVecIn  = uint8_t{1};
    shim->ioctlvVecOut = uint8_t{2};

    shim->ioctlvVec[0].vaddr = &shim->request;
    shim->ioctlvVec[0].len   = sizeof(FSARequest);

    auto *tmp = data;

    if ((uint32_t) data & 0x3F) {
        auto *alignedBuffer = memalign(0x40, ROUNDUP(size_bytes * cnt, 0x40));
        if (!alignedBuffer) {
            DEBUG_FUNCTION_LINE_ERR("Buffer not aligned (%p).\n", data);
            return FS_ERROR_INVALID_ALIGNMENT;
        }
        DEBUG_FUNCTION_LINE_WARN("Buffer not aligned (%p). Align to 0x40 for best performance\n", data);
        tmp = alignedBuffer;
    }

    shim->ioctlvVec[1].vaddr = (void *) tmp;
    shim->ioctlvVec[1].len   = size_bytes * cnt;

    shim->ioctlvVec[2].vaddr = &shim->response;
    shim->ioctlvVec[2].len   = sizeof(FSAResponse);

    auto &request         = shim->request.rawRead;
    request.blocks_offset = blocks_offset;
    request.count         = cnt;
    request.size          = size_bytes;
    request.device_handle = device_handle;

    auto res = __FSAShimSend(shim, 0);
    if (res >= 0 && tmp != data) {
        memcpy(data, tmp, size_bytes * cnt);
    }
    if (tmp != data) {
        free(tmp);
    }

    free(shim);
    return res;
}

FSError FSAEx_RawWrite(FSClient *client, const void *data, uint32_t size_bytes, uint32_t cnt, uint64_t blocks_offset, int device_handle) {
    if (!client) {
        return FS_ERROR_INVALID_CLIENTHANDLE;
    }
    return FSAEx_RawWriteEx(FSGetClientBody(client)->clientHandle, data, size_bytes, cnt, blocks_offset, device_handle);
}

FSError FSAEx_RawWriteEx(int clientHandle, const void *data, uint32_t size_bytes, uint32_t cnt, uint64_t blocks_offset, int device_handle) {
    auto *shim = (FSAShimBuffer *) memalign(0x40, sizeof(FSAShimBuffer));
    if (!shim) {
        return FS_ERROR_INVALID_BUFFER;
    }

    shim->clientHandle = clientHandle;
    shim->ipcReqType   = FSA_IPC_REQUEST_IOCTLV;
    shim->command      = FSA_COMMAND_RAW_WRITE;

    shim->ioctlvVecIn  = uint8_t{2};
    shim->ioctlvVecOut = uint8_t{1};

    shim->ioctlvVec[0].vaddr = &shim->request;
    shim->ioctlvVec[0].len   = sizeof(FSARequest);

    void *tmp = (void *) data;
    if ((uint32_t) data & 0x3F) {
        auto *alignedBuffer = memalign(0x40, ROUNDUP(size_bytes * cnt, 0x40));
        if (!alignedBuffer) {
            DEBUG_FUNCTION_LINE_ERR("Buffer not aligned (%p).", data);
            return FS_ERROR_INVALID_ALIGNMENT;
        }
        DEBUG_FUNCTION_LINE_WARN("Buffer not aligned (%p). Align to 0x40 for best performance", data);
        tmp = alignedBuffer;
        memcpy(tmp, data, size_bytes * cnt);
    }

    shim->ioctlvVec[1].vaddr = tmp;
    shim->ioctlvVec[1].len   = size_bytes * cnt;

    shim->ioctlvVec[2].vaddr = &shim->response;
    shim->ioctlvVec[2].len   = sizeof(FSAResponse);

    auto &request         = shim->request.rawWrite;
    request.blocks_offset = blocks_offset;
    request.count         = cnt;
    request.size          = size_bytes;
    request.device_handle = device_handle;

    auto res = __FSAShimSend(shim, 0);

    if (tmp != data) {
        free(tmp);
    }

    free(shim);
    return res;
}