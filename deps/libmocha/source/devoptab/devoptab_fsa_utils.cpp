#include "devoptab_fsa.h"

char *
__fsa_fixpath(struct _reent *r,
              const char *path) {
    char *p;
    char *fixedPath;

    if (!path) {
        r->_errno = EINVAL;
        return nullptr;
    }

    p = strchr(path, ':') + 1;
    if (!strchr(path, ':')) {
        p = (char *) path;
    }

    size_t pathLength = strlen(p);
    if (pathLength > FS_MAX_PATH) {
        r->_errno = ENAMETOOLONG;
        return nullptr;
    }

    // wii u softlocks on empty strings so give expected error back
    if (pathLength == 0) {
        r->_errno = ENOENT;
        return nullptr;
    }

    fixedPath = static_cast<char *>(memalign(0x40, FS_MAX_PATH + 1));
    if (!fixedPath) {
        r->_errno = ENOMEM;
        return nullptr;
    }

    if (p[0] == '/') {
        auto *deviceData = (FSADeviceData *) r->deviceData;
        strcpy(fixedPath, deviceData->mount_path);
        strcat(fixedPath, p);
    } else {
        strcpy(fixedPath, p);
    }

    return fixedPath;
}

mode_t __fsa_translate_stat_mode(FSAStat *fileStat) {
    mode_t retMode = 0;

    if ((fileStat->flags & FS_STAT_LINK) == FS_STAT_LINK) {
        retMode |= S_IFLNK;
    } else if ((fileStat->flags & FS_STAT_DIRECTORY) == FS_STAT_DIRECTORY) {
        retMode |= S_IFDIR;
    } else if ((fileStat->flags & FS_STAT_FILE) == FS_STAT_FILE) {
        retMode |= S_IFREG;
    } else if (fileStat->size == 0) {
        // Mounted paths like /vol/external01 have no flags set.
        // If no flag is set and the size is 0, it's a (root) dir
        retMode |= S_IFDIR;
    } else if (fileStat->size > 0) {
        // Some regular Wii U files have no type info but will have a size
        retMode |= S_IFREG;
    }

    // Convert normal CafeOS hexadecimal permission bits into Unix octal permission bits
    mode_t permissionMode = (((fileStat->mode >> 2) & S_IRWXU) | ((fileStat->mode >> 1) & S_IRWXG) | (fileStat->mode & S_IRWXO));

    return retMode | permissionMode;
}

void __fsa_translate_stat(FSAStat *fsStat, struct stat *posStat) {
    memset(posStat, 0, sizeof(struct stat));
    posStat->st_dev     = (dev_t) nullptr;
    posStat->st_ino     = fsStat->entryId;
    posStat->st_mode    = __fsa_translate_stat_mode(fsStat);
    posStat->st_nlink   = 1;
    posStat->st_uid     = fsStat->owner;
    posStat->st_gid     = fsStat->group;
    posStat->st_rdev    = posStat->st_dev;
    posStat->st_size    = fsStat->size;
    posStat->st_atime   = __fsa_translate_time(fsStat->modified);
    posStat->st_ctime   = __fsa_translate_time(fsStat->created);
    posStat->st_mtime   = __fsa_translate_time(fsStat->modified);
    posStat->st_blksize = 512;
    posStat->st_blocks  = (posStat->st_size + posStat->st_blksize - 1) / posStat->st_size;
}

// The Wii U FSTime epoch is at 1980, so we must map it to 1970 for gettime
#define WIIU_FSTIME_EPOCH_YEAR         (1980)

#define EPOCH_YEAR                     (1970)
#define EPOCH_YEARS_SINCE_LEAP         2
#define EPOCH_YEARS_SINCE_CENTURY      70
#define EPOCH_YEARS_SINCE_LEAP_CENTURY 370

#define EPOCH_DIFF_YEARS(year)         (year - EPOCH_YEAR)
#define EPOCH_DIFF_DAYS(year)                                         \
    ((EPOCH_DIFF_YEARS(year) * 365) +                                 \
     (EPOCH_DIFF_YEARS(year) - 1 + EPOCH_YEARS_SINCE_LEAP) / 4 -      \
     (EPOCH_DIFF_YEARS(year) - 1 + EPOCH_YEARS_SINCE_CENTURY) / 100 + \
     (EPOCH_DIFF_YEARS(year) - 1 + EPOCH_YEARS_SINCE_LEAP_CENTURY) / 400)
#define EPOCH_DIFF_SECS(year) (60ull * 60ull * 24ull * (uint64_t) EPOCH_DIFF_DAYS(year))

time_t __fsa_translate_time(FSTime timeValue) {
    return (timeValue / 1000000) + EPOCH_DIFF_SECS(WIIU_FSTIME_EPOCH_YEAR);
}

FSMode __fsa_translate_permission_mode(mode_t mode) {
    // Convert normal Unix octal permission bits into CafeOS hexadecimal permission bits
    return (FSMode) (((mode & S_IRWXU) << 2) | ((mode & S_IRWXG) << 1) | (mode & S_IRWXO));
}

int __fsa_translate_error(FSError error) {
    switch (error) {
        case FS_ERROR_END_OF_DIR:
        case FS_ERROR_END_OF_FILE:
            return ENOENT;
        case FS_ERROR_ALREADY_EXISTS:
            return EEXIST;
        case FS_ERROR_MEDIA_ERROR:
            return EIO;
        case FS_ERROR_NOT_FOUND:
            return ENOENT;
        case FS_ERROR_PERMISSION_ERROR:
            return EPERM;
        case FS_ERROR_STORAGE_FULL:
            return ENOSPC;
        case FS_ERROR_BUSY:
            return EBUSY;
        case FS_ERROR_CANCELLED:
            return ECANCELED;
        case FS_ERROR_FILE_TOO_BIG:
            return EFBIG;
        case FS_ERROR_INVALID_PATH:
            return ENAMETOOLONG;
        case FS_ERROR_NOT_DIR:
            return ENOTDIR;
        case FS_ERROR_NOT_FILE:
            return EISDIR;
        case FS_ERROR_OUT_OF_RANGE:
            return ESPIPE;
        case FS_ERROR_UNSUPPORTED_COMMAND:
            return ENOTSUP;
        case FS_ERROR_WRITE_PROTECTED:
            return EROFS;
        case FS_ERROR_NOT_INIT:
            return ENODEV;
            // TODO
        case FS_ERROR_MAX_MOUNT_POINTS:
            break;
        case FS_ERROR_MAX_VOLUMES:
            break;
        case FS_ERROR_MAX_CLIENTS:
            break;
        case FS_ERROR_MAX_FILES:
            break;
        case FS_ERROR_MAX_DIRS:
            break;
        case FS_ERROR_ALREADY_OPEN:
            break;
        case FS_ERROR_NOT_EMPTY:
            break;
        case FS_ERROR_ACCESS_ERROR:
            break;
        case FS_ERROR_DATA_CORRUPTED:
            break;
        case FS_ERROR_JOURNAL_FULL:
            break;
        case FS_ERROR_UNAVAILABLE_COMMAND:
            break;
        case FS_ERROR_INVALID_PARAM:
            break;
        case FS_ERROR_INVALID_BUFFER:
            break;
        case FS_ERROR_INVALID_ALIGNMENT:
            break;
        case FS_ERROR_INVALID_CLIENTHANDLE:
            break;
        case FS_ERROR_INVALID_FILEHANDLE:
            break;
        case FS_ERROR_INVALID_DIRHANDLE:
            break;
        case FS_ERROR_OUT_OF_RESOURCES:
            break;
        case FS_ERROR_MEDIA_NOT_READY:
            break;
        case FS_ERROR_INVALID_MEDIA:
            break;
        default:
            break;
    }
    return (int) EIO;
}
