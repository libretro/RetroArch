/****************************************************************************
 * Copyright (C) 2015
 * by Dimok
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 ***************************************************************************/
#include "fs_functions.h"
#include "os_functions.h"
#include "utils/utils.h"

EXPORT_DECL(int, FSInit, void);
EXPORT_DECL(int, FSShutdown, void);
EXPORT_DECL(int, FSAddClientEx, void *pClient, int unk_zero_param, int errHandling);
EXPORT_DECL(int, FSDelClient, void *pClient);
EXPORT_DECL(void, FSInitCmdBlock, void *pCmd);
EXPORT_DECL(int, FSGetMountSource, void *pClient, void *pCmd, int type, void *source, int errHandling);

EXPORT_DECL(int, FSMount, void *pClient, void *pCmd, void *source, char *target, uint32_t bytes, int errHandling);
EXPORT_DECL(int, FSUnmount, void *pClient, void *pCmd, const char *target, int errHandling);

EXPORT_DECL(int, FSGetStat, void *pClient, void *pCmd, const char *path, FSStat *stats, int errHandling);
EXPORT_DECL(int, FSGetStatAsync, void *pClient, void *pCmd, const char *path, void *stats, int error, void *asyncParams);
EXPORT_DECL(int, FSRename, void *pClient, void *pCmd, const char *oldPath, const char *newPath, int error);
EXPORT_DECL(int, FSRenameAsync, void *pClient, void *pCmd, const char *oldPath, const char *newPath, int error, void *asyncParams);
EXPORT_DECL(int, FSRemove, void *pClient, void *pCmd, const char *path, int error);
EXPORT_DECL(int, FSRemoveAsync, void *pClient, void *pCmd, const char *path, int error, void *asyncParams);
EXPORT_DECL(int, FSFlushQuota, void *pClient, void *pCmd, const char* path, int error);
EXPORT_DECL(int, FSFlushQuotaAsync, void *pClient, void *pCmd, const char *path, int error, void *asyncParams);
EXPORT_DECL(int, FSGetFreeSpaceSize, void *pClient, void *pCmd, const char *path, uint64_t *returnedFreeSize, int error);
EXPORT_DECL(int, FSGetFreeSpaceSizeAsync, void *pClient, void *pCmd, const char *path, uint64_t *returnedFreeSize, int error, void *asyncParams);
EXPORT_DECL(int, FSRollbackQuota, void *pClient, void *pCmd, const char *path, int error);
EXPORT_DECL(int, FSRollbackQuotaAsync, void *pClient, void *pCmd, const char *path, int error, void *asyncParams);

EXPORT_DECL(int, FSOpenDir, void *pClient, void *pCmd, const char *path, int *dh, int errHandling);
EXPORT_DECL(int, FSOpenDirAsync, void *pClient, void* pCmd, const char *path, int *handle, int error, void *asyncParams);
EXPORT_DECL(int, FSReadDir, void *pClient, void *pCmd, int dh, FSDirEntry *dir_entry, int errHandling);
EXPORT_DECL(int, FSRewindDir, void *pClient, void *pCmd, int dh, int errHandling);
EXPORT_DECL(int, FSCloseDir, void *pClient, void *pCmd, int dh, int errHandling);
EXPORT_DECL(int, FSChangeDir, void *pClient, void *pCmd, const char *path, int errHandling);
EXPORT_DECL(int, FSChangeDirAsync, void *pClient, void *pCmd, const char *path, int error, void *asyncParams);
EXPORT_DECL(int, FSMakeDir, void *pClient, void *pCmd, const char *path, int errHandling);
EXPORT_DECL(int, FSMakeDirAsync, void *pClient, void *pCmd, const char *path, int error, void *asyncParams);

EXPORT_DECL(int, FSOpenFile, void *pClient, void *pCmd, const char *path, const char *mode, int *fd, int errHandling);
EXPORT_DECL(int, FSOpenFileAsync, void *pClient, void *pCmd, const char *path, const char *mode, int *handle, int error, const void *asyncParams);
EXPORT_DECL(int, FSReadFile, void *pClient, void *pCmd, void *buffer, int size, int count, int fd, int flag, int errHandling);
EXPORT_DECL(int, FSCloseFile, void *pClient, void *pCmd, int fd, int errHandling);

EXPORT_DECL(int, FSFlushFile, void *pClient, void *pCmd, int fd, int error);
EXPORT_DECL(int, FSTruncateFile, void *pClient, void *pCmd, int fd, int error);
EXPORT_DECL(int, FSGetStatFile, void *pClient, void *pCmd, int fd, void *buffer, int error);
EXPORT_DECL(int, FSSetPosFile, void *pClient, void *pCmd, int fd, int pos, int error);
EXPORT_DECL(int, FSWriteFile, void *pClient, void *pCmd, const void *source, int block_size, int block_count, int fd, int flag, int error);

void InitFSFunctionPointers(void)
{
    unsigned int *funcPointer = 0;

    OS_FIND_EXPORT(coreinit_handle, FSInit);
    OS_FIND_EXPORT(coreinit_handle, FSShutdown);
    OS_FIND_EXPORT(coreinit_handle, FSAddClientEx);
    OS_FIND_EXPORT(coreinit_handle, FSDelClient);
    OS_FIND_EXPORT(coreinit_handle, FSInitCmdBlock);
    OS_FIND_EXPORT(coreinit_handle, FSGetMountSource);

    OS_FIND_EXPORT(coreinit_handle, FSMount);
    OS_FIND_EXPORT(coreinit_handle, FSUnmount);

    OS_FIND_EXPORT(coreinit_handle, FSGetStat);
    OS_FIND_EXPORT(coreinit_handle, FSGetStatAsync);
    OS_FIND_EXPORT(coreinit_handle, FSRename);
    OS_FIND_EXPORT(coreinit_handle, FSRenameAsync);
    OS_FIND_EXPORT(coreinit_handle, FSRemove);
    OS_FIND_EXPORT(coreinit_handle, FSRemoveAsync);
    OS_FIND_EXPORT(coreinit_handle, FSFlushQuota);
    OS_FIND_EXPORT(coreinit_handle, FSFlushQuotaAsync);
    OS_FIND_EXPORT(coreinit_handle, FSGetFreeSpaceSize);
    OS_FIND_EXPORT(coreinit_handle, FSGetFreeSpaceSizeAsync);
    OS_FIND_EXPORT(coreinit_handle, FSRollbackQuota);
    OS_FIND_EXPORT(coreinit_handle, FSRollbackQuotaAsync);

    OS_FIND_EXPORT(coreinit_handle, FSOpenDir);
    OS_FIND_EXPORT(coreinit_handle, FSOpenDirAsync);
    OS_FIND_EXPORT(coreinit_handle, FSReadDir);
    OS_FIND_EXPORT(coreinit_handle, FSRewindDir);
    OS_FIND_EXPORT(coreinit_handle, FSCloseDir);
    OS_FIND_EXPORT(coreinit_handle, FSChangeDir);
    OS_FIND_EXPORT(coreinit_handle, FSChangeDirAsync);
    OS_FIND_EXPORT(coreinit_handle, FSMakeDir);
    OS_FIND_EXPORT(coreinit_handle, FSMakeDirAsync);


    OS_FIND_EXPORT(coreinit_handle, FSOpenFile);
    OS_FIND_EXPORT(coreinit_handle, FSOpenFileAsync);
    OS_FIND_EXPORT(coreinit_handle, FSReadFile);
    OS_FIND_EXPORT(coreinit_handle, FSCloseFile);

    OS_FIND_EXPORT(coreinit_handle, FSFlushFile);
    OS_FIND_EXPORT(coreinit_handle, FSTruncateFile);
    OS_FIND_EXPORT(coreinit_handle, FSGetStatFile);
    OS_FIND_EXPORT(coreinit_handle, FSSetPosFile);
    OS_FIND_EXPORT(coreinit_handle, FSWriteFile);
}
