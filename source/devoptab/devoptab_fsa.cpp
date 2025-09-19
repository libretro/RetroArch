#include "devoptab_fsa.h"
#include "logger.h"
#include "mocha/mocha.h"
#include <coreinit/cache.h>
#include <coreinit/filesystem_fsa.h>
#include <mutex>
#include <string>

static const devoptab_t fsa_default_devoptab = {
        .structSize   = sizeof(__fsa_file_t),
        .open_r       = __fsa_open,
        .close_r      = __fsa_close,
        .write_r      = __fsa_write,
        .read_r       = __fsa_read,
        .seek_r       = __fsa_seek,
        .fstat_r      = __fsa_fstat,
        .stat_r       = __fsa_stat,
        .link_r       = __fsa_link,
        .unlink_r     = __fsa_unlink,
        .chdir_r      = __fsa_chdir,
        .rename_r     = __fsa_rename,
        .mkdir_r      = __fsa_mkdir,
        .dirStateSize = sizeof(__fsa_dir_t),
        .diropen_r    = __fsa_diropen,
        .dirreset_r   = __fsa_dirreset,
        .dirnext_r    = __fsa_dirnext,
        .dirclose_r   = __fsa_dirclose,
        .statvfs_r    = __fsa_statvfs,
        .ftruncate_r  = __fsa_ftruncate,
        .fsync_r      = __fsa_fsync,
        .chmod_r      = __fsa_chmod,
        .fchmod_r     = __fsa_fchmod,
        .rmdir_r      = __fsa_rmdir,
        .lstat_r      = __fsa_stat,
        .utimes_r     = __fsa_utimes,
};

static bool fsa_initialised = false;
static FSADeviceData fsa_mounts[0x10];

static void fsaResetMount(FSADeviceData *mount, uint32_t id) {
    *mount = {};
    memcpy(&mount->device, &fsa_default_devoptab, sizeof(fsa_default_devoptab));
    mount->device.name         = mount->name;
    mount->device.deviceData   = mount;
    mount->id                  = id;
    mount->setup               = false;
    mount->mounted             = false;
    mount->clientHandle        = -1;
    mount->deviceSizeInSectors = 0;
    mount->deviceSectorSize    = 0;
    memset(mount->mount_path, 0, sizeof(mount->mount_path));
    memset(mount->name, 0, sizeof(mount->name));
    DCFlushRange(mount, sizeof(*mount));
}

void fsaInit() {
    if (!fsa_initialised) {
        uint32_t total = sizeof(fsa_mounts) / sizeof(fsa_mounts[0]);
        for (uint32_t i = 0; i < total; i++) {
            fsaResetMount(&fsa_mounts[i], i);
        }
        fsa_initialised = true;
    }
}

std::mutex fsaMutex;

FSADeviceData *fsa_alloc() {
    uint32_t i;
    uint32_t total = sizeof(fsa_mounts) / sizeof(fsa_mounts[0]);
    FSADeviceData *mount;

    fsaInit();

    for (i = 0; i < total; i++) {
        mount = &fsa_mounts[i];
        if (!mount->setup) {
            return mount;
        }
    }

    return nullptr;
}

static void fsa_free(FSADeviceData *mount) {
    FSError res;
    if (mount->mounted) {
        if ((res = FSAUnmount(mount->clientHandle, mount->mount_path, FSA_UNMOUNT_FLAG_FORCE)) < 0) {
            DEBUG_FUNCTION_LINE_WARN("FSAUnmount %s for %s failed: %s", mount->mount_path, mount->name, FSAGetStatusStr(res));
        }
    }
    res = FSADelClient(mount->clientHandle);
    if (res < 0) {
        DEBUG_FUNCTION_LINE_WARN("FSADelClient for %s failed: %s", mount->name, FSAGetStatusStr(res));
    }
    fsaResetMount(mount, mount->id);
}

MochaUtilsStatus Mocha_UnmountFS(const char *virt_name) {
    if (!virt_name) {
        return MOCHA_RESULT_INVALID_ARGUMENT;
    }
    std::lock_guard<std::mutex> lock(fsaMutex);
    uint32_t total = sizeof(fsa_mounts) / sizeof(fsa_mounts[0]);

    fsaInit();

    for (uint32_t i = 0; i < total; i++) {
        FSADeviceData *mount = &fsa_mounts[i];
        if (!mount->setup) {
            continue;
        }
        if (strcmp(mount->name, virt_name) == 0) {
            std::string removeName = std::string(mount->name).append(":");
            RemoveDevice(removeName.c_str());
            fsa_free(mount);
            return MOCHA_RESULT_SUCCESS;
        }
    }

    DEBUG_FUNCTION_LINE_WARN("Failed to find fsa mount data for %s", virt_name);
    return MOCHA_RESULT_NOT_FOUND;
}
extern int mochaInitDone;

MochaUtilsStatus Mocha_MountFS(const char *virt_name, const char *dev_path, const char *mount_path) {
    return Mocha_MountFSEx(virt_name, dev_path, mount_path, FSA_MOUNT_FLAG_GLOBAL_MOUNT, nullptr, 0);
}

MochaUtilsStatus Mocha_MountFSEx(const char *virt_name, const char *dev_path, const char *mount_path, FSAMountFlags mountFlags, void *mountArgBuf, int mountArgBufLen) {
    if (!mochaInitDone) {
        if (Mocha_InitLibrary() != MOCHA_RESULT_SUCCESS) {
            DEBUG_FUNCTION_LINE_ERR("Mocha_InitLibrary failed");
            return MOCHA_RESULT_UNSUPPORTED_COMMAND;
        }
    }

    FSAInit();
    std::lock_guard<std::mutex> lock(fsaMutex);

    FSADeviceData *mount = fsa_alloc();
    if (mount == nullptr) {
        DEBUG_FUNCTION_LINE_ERR("fsa_alloc() failed");
        OSMemoryBarrier();
        return MOCHA_RESULT_MAX_CLIENT;
    }

    mount->clientHandle = FSAAddClient(nullptr);
    if (mount->clientHandle < 0) {
        DEBUG_FUNCTION_LINE_ERR("FSAAddClient() failed: %s", FSAGetStatusStr(static_cast<FSError>(mount->clientHandle)));
        fsa_free(mount);
        return MOCHA_RESULT_MAX_CLIENT;
    }

    MochaUtilsStatus status;
    if ((status = Mocha_UnlockFSClientEx(mount->clientHandle)) != MOCHA_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Mocha_UnlockFSClientEx failed: %s", Mocha_GetStatusStr(status));
        return MOCHA_RESULT_UNSUPPORTED_COMMAND;
    }

    mount->mounted = false;

    strncpy(mount->name, virt_name, sizeof(mount->name) - 1);
    strncpy(mount->mount_path, mount_path, sizeof(mount->mount_path) - 1);
    FSError res;
    if (dev_path) {
        res = FSAMount(mount->clientHandle, dev_path, mount_path, mountFlags, mountArgBuf, mountArgBufLen);
        if (res < 0) {
            DEBUG_FUNCTION_LINE_ERR("FSAMount(0x%08X, %s, %s, %08X, %p, %08X) failed: %s", mount->clientHandle, dev_path, mount_path, mountFlags, mountArgBuf, mountArgBufLen, FSAGetStatusStr(res));
            fsa_free(mount);
            if (res == FS_ERROR_ALREADY_EXISTS) {
                return MOCHA_RESULT_ALREADY_EXISTS;
            }
            return MOCHA_RESULT_UNKNOWN_ERROR;
        }
        mount->mounted = true;
    } else {
        mount->mounted = false;
    }

    if ((res = FSAChangeDir(mount->clientHandle, mount->mount_path)) < 0) {
        DEBUG_FUNCTION_LINE_WARN("FSAChangeDir(0x%08X, %s) failed: %s", mount->clientHandle, mount->mount_path, FSAGetStatusStr(res));
    }

    FSADeviceInfo deviceInfo;
    if ((res = FSAGetDeviceInfo(mount->clientHandle, mount_path, &deviceInfo)) >= 0) {
        mount->deviceSizeInSectors = deviceInfo.deviceSizeInSectors;
        mount->deviceSectorSize    = deviceInfo.deviceSectorSize;
    } else {
        mount->deviceSizeInSectors = 0xFFFFFFFF;
        mount->deviceSectorSize    = 512;
        DEBUG_FUNCTION_LINE_WARN("Failed to get DeviceInfo for %s: %s", mount_path, FSAGetStatusStr(res));
    }

    if (AddDevice(&mount->device) < 0) {
        DEBUG_FUNCTION_LINE_ERR("AddDevice failed for %s.", virt_name);
        fsa_free(mount);
        return MOCHA_RESULT_ADD_DEVOPTAB_FAILED;
    }

    mount->setup = true;

    return MOCHA_RESULT_SUCCESS;
}
