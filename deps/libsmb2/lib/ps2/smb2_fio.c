/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
  SMB2MAN
  Ronnie Sahlberg <ronniesahlberg@gmail.com> 2021
  André Guilherme <andregui17@outlook.com> 2023-2024

  Based on SMBMAN:
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
*/

#include "types.h"
#include "defs.h"
#include "irx.h"
#include "intrman.h"
#include "iomanX.h"
#include "io_common.h"
#include "sifman.h"
#include "stdio.h"
#include "sysclib.h"
#include "thbase.h"
#include "thsemap.h"
#include "errno.h"
#include "ps2smb2.h"
#include "smb2_fio.h"
#include "loadcore.h"
#include <stdint.h>
#include <stdbool.h>

#include <smb2/smb2.h>
#include <smb2/libsmb2.h>

/* Uncomment to enable debug logging */
/*#define DEBUG*/
#ifdef DEBUG
#define SMB2_LOG_URL      "smb://10.10.10.11/PS2SMB/ps2.log"
#define SMB2_LOG_USER     "user"
#define SMB2_LOG_PASSWORD "password"

struct smb2_context *log_smb2;
struct smb2fh *log_fh;
char log_buf[1024];

#define SMB2LOG(...)                                                   \
    {                                                                  \
        int l;                                                         \
        if (log_fh) {                                                  \
            l = sprintf(log_buf, __VA_ARGS__);                         \
            smb2_write(log_smb2, log_fh, (const u8 *)log_buf, l); \
        }                                                              \
    }
#else
#define SMB2LOG(...)
#endif

void free(void *);
void *malloc(int);

int smb2man_io_sema;
static char *smb2_curdir;


/*
 * Linked list of all connected shares.
 * Each share is presented as smb:/<name>
 */
struct smb2_share_list
{
    struct smb2_share_list *next;
    char name[SMB2_MAX_NAME_LEN];
    struct smb2_context *smb2;
};
struct smb2_share_list *shares;

#define smb2_io_lock()   WaitSema(smb2man_io_sema)
#define smb2_io_unlock() SignalSema(smb2man_io_sema)

/*
 * Takes a path of the form <share>/<path> and returns the context that
 * represents <share> and return the path within that share as **remainder.
 */
static struct smb2_context *find_context(char *path, char **remainder)
{
    char *tmp;
    struct smb2_share_list *share = shares;

    tmp = strchr(path, '/');
    if (tmp == NULL) {
        return NULL;
    }

    *tmp++ = 0;
    if (*remainder) {
        *remainder = tmp;
    }

    while (share) {
        if (strcmp(share->name, path)) {
            return share->smb2;
        }
        share = share->next;
    }

    return NULL;
}

static char *prepare_path(const char *path)
{
    int i, len;
    char *p, *p2;
    len = strlen(path) + 1 + (smb2_curdir ? strlen(smb2_curdir) + 1 : 0);
    p = malloc(len);
    if (p == NULL) {
        return NULL;
    }
    p[0] = 0;

    if (smb2_curdir) {
        strcat(p, smb2_curdir);
        strcat(p, "/");
    }

    strcat(p, path);

    for (i = strlen(p) - 1; i > 0; i--) {
        if (p[i] == '\\') {
            p[i] = '/';
        }
    }

    /* strip trailing /. */
    if (strlen(p) > 2) {
        p2 = p + strlen(p) - 2;
        if (!strcmp(p2, "/.")) {
            *p2 = 0;
        }
    }

    /* strip trailing <component>/.. */
    if (strlen(p) > 3) {
        p2 = p + strlen(p) - 3;
        if (!strcmp(p2, "/..")) {
            *p2 = 0;
            p2  = strrchr(p, '/');
            if (p2 == NULL) {
                free(p);
                return NULL;
            }
            *p2 = 0;
        }
    }

    return p;
}

static int smb2_Connect(smb2Connect_in_t *in, smb2Connect_out_t *out)
{
    struct smb2_share_list *share;
    struct smb2_url *url;
    int rc;

    if (out) {
        out->ctx = NULL;
    }

#ifdef DEBUG
    if (log_smb2 == NULL) {
        log_smb2 = smb2_init_context();
        smb2_set_timeout(log_smb2, 30);
        url = smb2_parse_url(log_smb2, SMB2_LOG_URL);
        smb2_set_password(log_smb2, SMB2_LOG_PASSWORD);
        if (smb2_connect_share(log_smb2, url->server, url->share, SMB2_LOG_USER)) {
            return -EIO;
        }
        log_fh = smb2_open(log_smb2, url->path, O_RDWR | O_CREAT);
        smb2_ftruncate(log_smb2, log_fh, 0);
        smb2_destroy_url(url);
        SMB2LOG("Logging started\n");
    }
#endif

    SMB2LOG("smb2_Connect: Try to connect %s as smb:/%s\n", in->url, in->name);
    share = malloc(sizeof(struct smb2_share_list));
    if (share == NULL) {
        SMB2LOG("Failed to malloc share\n");
        return -ENOMEM;
    }
    memset(share, 0, sizeof(struct smb2_share_list));
    share->next = shares;
    strcpy(share->name, in->name);
    share->smb2 = smb2_init_context();
    smb2_set_timeout(share->smb2, 30);
    if (share->smb2 == NULL) {
        SMB2LOG("Failed to initialize smb2 context\n");
        free(share);
        return -ENOMEM;
    }
    url = smb2_parse_url(share->smb2, in->url);
    if (url == NULL) {
        SMB2LOG("Failed to parse URL: %s\n", in->url);
        free(share);
        return -EINVAL;
    }
    smb2_set_password(share->smb2, in->password);
    rc = smb2_connect_share(share->smb2, url->server, url->share, in->username);
    smb2_destroy_url(url);
    if (rc) {
        smb2_destroy_context(share->smb2);
        free(share);
        SMB2LOG("Failed to connect to share: %s %s\n", in->url, smb2_get_error(share->smb2));
        return -EIO;
    }
    shares = share;
    if (out) {
        out->ctx = share->smb2;
    }
    SMB2LOG("Connected to share %s\n", in->url);
    return 0;
}

int SMB2_devctl(iop_file_t *f, const char *devname, int cmd,
                void *arg, unsigned int arglen,
                void *bufp, unsigned int buflen)
{
    int r = 0;

    SMB2LOG("SMB2_devctl cmd:%d\n", cmd);

    smb2_io_lock();

    switch (cmd) {
        case SMB2_DEVCTL_CONNECT:
            r = smb2_Connect((smb2Connect_in_t *)arg, (smb2Connect_out_t *)bufp);
            break;

        default:
            r = -EINVAL;
    }

    smb2_io_unlock();

    return r;
}

struct dir_fh
{
    struct smb2_context *smb2;
    struct smb2dir *fh;
    int is_root;
    struct smb2_share_list *shares;
};

struct file_fh
{
    struct smb2_context *smb2;
    struct smb2fh *fh;
};

int SMB2_open(iop_file_t *f, const char *filename, int flags, int mode)
{
    char *path = NULL, *p;
    struct smb2_context *smb2;
    struct file_fh *ffh = NULL;
    int rc = 0;

    SMB2LOG("SMB2_OPEN %s flags:%x\n", filename, flags);

    /* no writing, yet */
    if (flags != O_RDONLY) {
        return -EROFS;
    }

    if (!filename)
        return -ENOENT;

    path = prepare_path(filename);
    if (path == NULL) {
        return -ENOMEM;
    }

    smb2 = find_context(path, &p);
    if (smb2 == NULL) {
        rc = -ENOENT;
        goto out;
    }

    ffh = malloc(sizeof(struct file_fh));
    if (ffh == NULL) {
        rc = -ENOMEM;
        goto out;
    }

    smb2_io_lock();
    ffh->smb2 = smb2;
    ffh->fh = smb2_open(smb2, p, flags);
    smb2_io_unlock();
    if (ffh->fh == NULL) {
        rc = -EINVAL;
        goto out;
    }

    f->privdata = ffh;


out:
    if (rc) {
        free(ffh);
    }
    free(path);
    return rc;
}

int SMB2_close(iop_file_t *f)
{
    SMB2LOG("SMB2_CLOSE\n");
    struct file_fh *ffh = f->privdata;

    if (ffh == NULL) {
        return -EBADF;
    }

    smb2_io_lock();
    smb2_close(ffh->smb2, ffh->fh);
    smb2_io_unlock();

    free(ffh);
    f->privdata = NULL;
    return 0;
}


int SMB2_dopen(iop_file_t *f, const char *dirname)
{
    char *path = NULL, *p;
    struct smb2_context *smb2;
    struct dir_fh *dfh = NULL;
    int rc = 0;

    SMB2LOG("SMB2_DOPEN %s\n", dirname);

    if (!dirname)
        return -ENOENT;

    path = prepare_path(dirname);
    if (path == NULL) {
        return -ENOMEM;
    }
    /* TODO:
     * If path[0] == 0 then we are at the virtual root and should return
     * all the shares that are mounted.
     */

    smb2 = find_context(path, &p);
    if (smb2 == NULL) {
        rc = -ENOENT;
        goto out;
    }

    dfh = malloc(sizeof(struct dir_fh));
    if (dfh == NULL) {
        rc = -ENOMEM;
        goto out;
    }

    dfh->smb2 = smb2;
    if (p[0]) {
        dfh->is_root = false;
        smb2_io_lock();
        dfh->fh = smb2_opendir(smb2, p);
        smb2_io_unlock();
    } else {
        dfh->is_root = true;
        dfh->shares  = shares;
    }
    if (dfh->fh == NULL) {
        rc = -EINVAL;
        goto out;
    }

    f->privdata = dfh;

out:
    if (rc) {
        free(dfh);
    }
    free(path);
    return rc;
}

int SMB2_dclose(iop_file_t *f)
{
    struct dir_fh *dfh = f->privdata;

    if (dfh == NULL) {
        return -EBADF;
    }

    if (!dfh->is_root) {
        smb2_closedir(dfh->smb2, dfh->fh);
    }

    free(dfh);
    f->privdata = NULL;
    return 0;
}

static void FileTimeToDate(u64 t, u8 *datetime)
{
    u8 daysPerMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int i;
    u64 time;
    u16 years, days;
    u8 leapdays, months, hours, minutes, seconds;

    time = t / 10000000; /* convert to seconds from 100-nanosecond intervals */

    years = (u16)(time / ((u64)60 * 60 * 24 * 365)); /* hurray for interger division */
    time -= years * ((u64)60 * 60 * 24 * 365);       /* truncate off the years */

    leapdays = (years / 4) - (years / 100) + (years / 400);
    years += 1601; /* add base year from FILETIME struct; */

    days = (u16)(time / (60 * 60 * 24));
    time -= (u32)days * (60 * 60 * 24);
    days -= leapdays;

    if ((years % 4) == 0 && ((years % 100) != 0 || (years % 400) == 0))
        daysPerMonth[1]++;

    months = 0;
    for (i = 0; i < 12; i++) {
        if (days > daysPerMonth[i]) {
            days -= daysPerMonth[i];
            months++;
        } else
            break;
    }

    if (months >= 12) {
        months -= 12;
        years++;
    }
    hours = (u8)(time / (60 * 60));
    time -= (u16)hours * (60 * 60);

    minutes = (u8)(time / 60);
    time -= minutes * 60;

    seconds = (u8)(time);

    datetime[0] = 0;
    datetime[1] = seconds;
    datetime[2] = minutes;
    datetime[3] = hours;
    datetime[4] = days + 1;
    datetime[5] = months + 1;
    datetime[6] = (u8)(years & 0xFF);
    datetime[7] = (u8)((years >> 8) & 0xFF);
}

static void smb2_statFiller(struct smb2_stat_64 *st, iox_stat_t *stat)
{
    FileTimeToDate(st->smb2_ctime, stat->ctime);
    FileTimeToDate(st->smb2_atime, stat->atime);
    FileTimeToDate(st->smb2_mtime, stat->mtime);

    stat->size   = (int)(st->smb2_size & 0xffffffff);
    stat->hisize = (int)((st->smb2_size >> 32) & 0xffffffff);

    stat->mode = (st->smb2_type == SMB2_TYPE_DIRECTORY) ? FIO_S_IFDIR : FIO_S_IFREG;
}

int SMB2_dread(iop_file_t *f, iox_dirent_t *dirent)
{
    struct dir_fh *dfh = f->privdata;
    struct smb2dirent *de;

    if (dfh == NULL) {
        return -EBADF;
    }

    if (!dfh->is_root) {
        de = smb2_readdir(dfh->smb2, dfh->fh);
        if (de == NULL) {
            return 0;
        }
    } else {
        if (dfh->shares == NULL) {
            return 0;
        }
        memset(&dirent->stat, 0, sizeof(iox_stat_t));
        strncpy(dirent->name, dfh->shares->name, 256);
        dirent->stat.mode = FIO_S_IFDIR;
        dfh->shares       = dfh->shares->next;
        return 1;
    }

    SMB2LOG("SMB2_DREAD %s\n", de->name);
    strncpy(dirent->name, de->name, 256);
    smb2_statFiller(&de->st, &dirent->stat);

    return 1;
}

int SMB2_getstat(iop_file_t *f, const char *filename, iox_stat_t *stat)
{
    char *path = NULL, *p;
    struct smb2_context *smb2;
    struct smb2_stat_64 st;
    int rc = 0;

    SMB2LOG("SMB2_GETSTAT %s\n", filename);
    if (!filename)
        return -ENOENT;

    path = prepare_path(filename);
    if (path == NULL) {
        return -ENOMEM;
    }

    smb2 = find_context(path, &p);
    if (smb2 == NULL) {
        rc = -ENOENT;
        goto out;
    }

    memset((void *)stat, 0, sizeof(iox_stat_t));

    smb2_io_lock();
    rc = smb2_stat(smb2, p, &st);
    smb2_io_unlock();
    if (rc) {
        goto out;
    }

    smb2_statFiller(&st, stat);

out:

    free(path);
    return rc;
}

s64 SMB2_lseek64(iop_file_t *f, s64 pos, int whence)
{
    struct file_fh *ffh = f->privdata;
    int rc = 0;

    SMB2LOG("SMB2_LSEEK64 pos:%d whence:%d\n", (int)pos, whence);

    rc = smb2_lseek(ffh->smb2, ffh->fh, pos, whence, NULL);

    return rc;
}

int SMB2_lseek(iop_file_t *f, int pos, int where)
{
    return (int)SMB2_lseek64(f, pos, where);
}

int SMB2_read(iop_file_t *f, void *buf, int size)
{
    struct file_fh *ffh = f->privdata;
    int rc;

    SMB2LOG("SMB2_READ len:%d\n", size);
    if (ffh == NULL) {
        return -EBADF;
    }

    smb2_io_lock();
    rc = smb2_read(ffh->smb2, ffh->fh, buf, size);
    smb2_io_unlock();

    return rc;
}

int SMB2_write(iop_file_t *f, void *buf, int size)
{
    struct file_fh *ffh = f->privdata;
    int rc;

    SMB2LOG("SMB2_write %d\n", size);

    smb2_io_lock();
    rc = smb2_write(ffh->smb2, ffh->fh, buf, size);
    smb2_io_unlock();

    return rc;
}

int SMB2_mkdir(iop_file_t *f, const char *dirname, int mode)
{
    char *path = NULL, *p;
    struct smb2_context *smb2;
    int rc;

    SMB2LOG("SMB2_mkdir %s\n", dirname);

    if (!dirname)
        return -ENOENT;

    path = prepare_path(dirname);
    if (path == NULL) {
        return -ENOMEM;
    }

    smb2 = find_context(path, &p);
    if (smb2 == NULL) {
        rc = -ENOENT;
        goto out;
    }

    smb2_io_lock();
    rc = smb2_mkdir(smb2, p);
    smb2_io_unlock();
out:
    free(path);
    return rc;
}

int SMB2_rmdir(iop_file_t *f, const char *dirname)
{
    char *path = NULL, *p;
    struct smb2_context *smb2;
    int rc;

    SMB2LOG("SMB2_rmdir %s\n", dirname);

    if (!dirname)
        return -ENOENT;

    path = prepare_path(dirname);
    if (path == NULL) {
        return -ENOMEM;
    }

    smb2 = find_context(path, &p);
    if (smb2 == NULL) {
        rc = -ENOENT;
        goto out;
    }

    smb2_io_lock();
    rc = smb2_rmdir(smb2, p);
    smb2_io_unlock();
out:
    free(path);
    return rc;
}

int SMB2_remove(iop_file_t *f, const char *filename)
{
    char *path = NULL, *p;
    struct smb2_context *smb2;
    int rc;

    SMB2LOG("SMB2_remove %s\n", filename);

    if (!filename)
        return -ENOENT;

    path = prepare_path(filename);
    if (path == NULL) {
        return -ENOMEM;
    }

    smb2 = find_context(path, &p);
    if (smb2 == NULL) {
        rc = -ENOENT;
        goto out;
    }

    smb2_io_lock();
    rc = smb2_unlink(smb2, p);
    smb2_io_unlock();
out:
    free(path);
    return rc;
}

int SMB2_rename(iop_file_t *f, const char *oldname, const char *newname)
{
    char *oldpath = NULL, *newpath = NULL, *oldp, *newp;
    struct smb2_context *oldsmb2, *newsmb2;
    int rc = 0;

    SMB2LOG("SMB2_rename %s -> %s\n", oldname, newname);

    oldpath = prepare_path(oldname);
    if (oldpath == NULL) {
        rc = -ENOMEM;
        goto out;
    }

    oldsmb2 = find_context(oldpath, &oldp);
    if (oldsmb2 == NULL) {
        rc = -ENOENT;
        goto out;
    }

    newpath = prepare_path(newname);
    if (newpath == NULL) {
        rc = -ENOMEM;
        goto out;
    }

    newsmb2 = find_context(newpath, &newp);
    if (newsmb2 == NULL) {
        rc = -ENOENT;
        goto out;
    }

    if (newsmb2 != oldsmb2) {
        rc = -EINVAL;
        goto out;
    }

    smb2_io_lock();
    rc = smb2_rename(oldsmb2, oldp, newp);
    smb2_io_unlock();
out:
    free(oldpath);
    free(newpath);
    return rc;
}


int SMB2_dummy(void)
{
    SMB2LOG("SMB2_dummy\n");
    return -EIO;
}

int SMB2_chdir(iop_file_t *f, const char *dirname)
{
    char *path;

    SMB2LOG("SMB2_chdir %s\n", dirname);
    if (!dirname)
        return -ENOENT;

    path = prepare_path(dirname);
    if (path == NULL) {
        return -ENOMEM;
    }

    free(smb2_curdir);
    smb2_curdir = path;

    return 0;
}

int SMB2_deinit(iop_device_t *dev)
{
    SMB2LOG("SMB2_deinit\n");

    DeleteSema(smb2man_io_sema);

    return 0;
}

int SMB2_init(iop_device_t *dev)
{
    SMB2LOG("SMB2_init\n");

    smb2man_io_sema = CreateMutex(IOP_MUTEX_UNLOCKED);

    return 0;
}


static iop_device_ops_t smb2man_ops = {
    &SMB2_init,
    &SMB2_deinit,
    (void *)&SMB2_dummy,
    &SMB2_open,
    &SMB2_close,
    &SMB2_read,
    &SMB2_write,
    &SMB2_lseek,
    (void *)&SMB2_dummy,
    &SMB2_remove,
    &SMB2_mkdir,
    &SMB2_rmdir,
    &SMB2_dopen,
    &SMB2_dclose,
    &SMB2_dread,
    &SMB2_getstat,
    (void *)&SMB2_dummy,
    &SMB2_rename,
    &SMB2_chdir,
    (void *)&SMB2_dummy,
    (void *)&SMB2_dummy,
    (void *)&SMB2_dummy,
    &SMB2_lseek64,
    &SMB2_devctl,
    (void *)&SMB2_dummy,
    (void *)&SMB2_dummy,
    (void *)&SMB2_dummy};

static iop_device_t smb2dev = {
    "smb",
    IOP_DT_FS | IOP_DT_FSEXT,
    1,
    "SMB",
    &smb2man_ops};

int SMB2_initdev(void)
{
    DelDrv(smb2dev.name);
    if (AddDrv((iop_device_t *)&smb2dev))
        return MODULE_NO_RESIDENT_END;

    return MODULE_RESIDENT_END;
}
