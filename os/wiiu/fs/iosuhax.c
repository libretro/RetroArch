/***************************************************************************
 * Copyright (C) 2016
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

/* Altered for RetroArch from libiosuhax (iosuhax.c, iosuhax_devoptab.c,
 * iosuhax_disc_interface.c), keeping only what RetroArch uses: the
 * IOSU handle, an FSA client, the devoptab main.c mounts storage_usb
 * with, and the raw sector access libfat reads sd: and usb: through.
 *
 * Every FSA call is one IPC request that the iosuhax server in IOSU
 * carries out on its own, and nothing here is shared between threads
 * once a device is mounted except through the one atomic below, so
 * there is no lock. Requests are built in aligned buffers on the stack
 * rather than allocated. File and sector data go:
 *   - up to IOSUHAX_STAGE bytes through a stack buffer, one request;
 *   - up to IOSUHAX_BOUNCE bytes through one buffer made at mount
 *     time, which a thread claims with an atomic exchange;
 *   - beyond that straight into the caller's buffer: the server puts
 *     its reply word in front of the data, so the 0x40 bytes before
 *     the stretch being read are kept aside and put back, and the
 *     data is never copied. */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/iosupport.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <wiiu/ios.h>

#include "iosuhax.h"
#include "iosuhax_disc_interface.h"

#define IOSUHAX_MAGIC_WORD          0x4E696365

#define IOCTL_FSA_OPEN              0x40
#define IOCTL_FSA_CLOSE             0x41
#define IOCTL_FSA_GETDEVICEINFO     0x44
#define IOCTL_FSA_OPENDIR           0x45
#define IOCTL_FSA_READDIR           0x46
#define IOCTL_FSA_CLOSEDIR          0x47
#define IOCTL_FSA_MAKEDIR           0x48
#define IOCTL_FSA_OPENFILE          0x49
#define IOCTL_FSA_READFILE          0x4A
#define IOCTL_FSA_WRITEFILE         0x4B
#define IOCTL_FSA_STATFILE          0x4C
#define IOCTL_FSA_CLOSEFILE         0x4D
#define IOCTL_FSA_SETFILEPOS        0x4E
#define IOCTL_FSA_GETSTAT           0x4F
#define IOCTL_FSA_REMOVE            0x50
#define IOCTL_FSA_REWINDDIR         0x51
#define IOCTL_FSA_CHDIR             0x52
#define IOCTL_FSA_RAW_OPEN          0x54
#define IOCTL_FSA_RAW_READ          0x55
#define IOCTL_FSA_RAW_WRITE         0x56
#define IOCTL_FSA_RAW_CLOSE         0x57
#define IOCTL_CHECK_IF_IOSUHAX      0x5B

/* IPC buffers start on a cache line and data follows a 0x40 header */
#define IOSUHAX_ALIGN               0x40
#define IOSUHAX_ROUNDUP(x)          (((x) + (IOSUHAX_ALIGN - 1)) & ~(IOSUHAX_ALIGN - 1))
/* Data a stack buffer carries in one request */
#define IOSUHAX_STAGE               0x400
/* Data the shared buffer carries in one request */
#define IOSUHAX_BOUNCE              0x20000
/* Longest path a request carries, mount path included (FS_MAX_PATH) */
#define IOSUHAX_PATH_MAX            0x280
/* A path request: up to three words, the path and a mode string */
#define IOSUHAX_REQ_SIZE            (12 + IOSUHAX_PATH_MAX + 8)
#define IOSUHAX_SECTOR              512

/* An aligned stack buffer: one alignment unit bigger than asked for,
 * used from its first aligned byte. */
#define IOSUHAX_STACK(name, size)   uint8_t name##_raw[(size) + IOSUHAX_ALIGN]
#define IOSUHAX_ALIGNED(name)       ((void*)(((uintptr_t)name##_raw \
         + (IOSUHAX_ALIGN - 1)) & ~(uintptr_t)(IOSUHAX_ALIGN - 1)))
#define IOSUHAX_IS_ALIGNED(p)       ((((uintptr_t)(p)) & (IOSUHAX_ALIGN - 1)) == 0)

typedef struct
{
   uint32_t flag;
   uint32_t permission;
   uint32_t owner_id;
   uint32_t group_id;
   uint32_t size;      /* in bytes */
   uint32_t physsize;  /* on disk, in bytes */
   uint32_t unk[3];
   uint32_t id;
   uint32_t ctime;
   uint32_t mtime;
   uint32_t unk2[0x0D];
} fileStat_s;

typedef struct
{
   fileStat_s stat;
   char name[0x100];
} directoryEntry_s;

#define DIR_ENTRY_IS_DIRECTORY      0x80000000

static int iosuhax_handle = -1;

/* The shared data buffer: made by the first mount or device startup,
 * at boot, and held by whichever thread swapped busy from 0 to 1. A
 * thread that finds it held stages through the stack instead. */
static uint8_t *iosuhax_bounce;
static retro_atomic_int_t iosuhax_bounce_busy;

static void iosuhax_bounce_init(void)
{
   if (!iosuhax_bounce)
   {
      retro_atomic_int_init(&iosuhax_bounce_busy, 0);
      iosuhax_bounce = (uint8_t*)memalign(IOSUHAX_ALIGN,
            IOSUHAX_ALIGN + IOSUHAX_BOUNCE);
   }
}

static uint8_t *iosuhax_bounce_claim(void)
{
   if (iosuhax_bounce
         && retro_atomic_exchange_int(&iosuhax_bounce_busy, 1) == 0)
      return iosuhax_bounce;
   return NULL;
}

static void iosuhax_bounce_release(void)
{
   retro_atomic_store_release_int(&iosuhax_bounce_busy, 0);
}

/* One request. Returns the IPC error, or the reply's first word. */
static int iosuhax_call(uint32_t cmd, void *in, uint32_t in_len,
      void *out, uint32_t out_len)
{
   int res;
   if (iosuhax_handle < 0)
      return iosuhax_handle;
   res = IOS_Ioctl(iosuhax_handle, cmd, in, in_len, out, out_len);
   if (res < 0)
      return res;
   return (int)((uint32_t*)out)[0];
}

int IOSUHAX_Open(const char *dev)
{
   uint32_t *res;
   IOSUHAX_STACK(buf, 4);

   if (iosuhax_handle >= 0)
      return iosuhax_handle;

   iosuhax_handle = IOS_Open((char*)(dev ? dev : "/dev/iosuhax"), 0);
   /* make sure the device is actually iosuhax */
   if (iosuhax_handle >= 0 && dev)
   {
      res    = (uint32_t*)IOSUHAX_ALIGNED(buf);
      res[0] = 0;
      IOS_Ioctl(iosuhax_handle, IOCTL_CHECK_IF_IOSUHAX, NULL, 0, res, 4);
      if (res[0] != IOSUHAX_MAGIC_WORD)
      {
         IOS_Close(iosuhax_handle);
         iosuhax_handle = -1;
      }
   }

   return iosuhax_handle;
}

int IOSUHAX_Close(void)
{
   int res;
   if (iosuhax_handle < 0)
      return 0;
   res            = IOS_Close(iosuhax_handle);
   iosuhax_handle = -1;
   free(iosuhax_bounce);
   iosuhax_bounce = NULL;
   return res;
}

/* A request of up to six words whose reply is one word */
static int fsa_words(uint32_t cmd, uint32_t w0, uint32_t w1, uint32_t w2,
      unsigned n)
{
   uint32_t *io;
   IOSUHAX_STACK(buf, 12);
   io    = (uint32_t*)IOSUHAX_ALIGNED(buf);
   io[0] = w0;
   io[1] = w1;
   io[2] = w2;
   return iosuhax_call(cmd, io, n * sizeof(uint32_t), io, 4);
}

int IOSUHAX_FSA_Open(void)
{
   uint32_t *io;
   IOSUHAX_STACK(buf, 4);
   io = (uint32_t*)IOSUHAX_ALIGNED(buf);
   return iosuhax_call(IOCTL_FSA_OPEN, NULL, 0, io, 4);
}

int IOSUHAX_FSA_Close(int fsaFd)
{
   return fsa_words(IOCTL_FSA_CLOSE, (uint32_t)fsaFd, 0, 0, 1);
}

/* Lays out a path request as the FSA calls take it: @n words, word 1
 * the offset of the path (@mount then @path), then @tail if any, its
 * offset in *tail_off. Returns the request's length, or -1 when it
 * would not fit. */
static int fsa_path_req(uint32_t *io, unsigned n, const char *mount,
      const char *path, const char *tail, uint32_t *tail_off)
{
   size_t off = n * sizeof(uint32_t);
   size_t ml  = mount ? strlen(mount) : 0;
   size_t pl  = strlen(path);

   if (off + ml + pl + 1 > IOSUHAX_REQ_SIZE)
      return -1;
   io[1] = (uint32_t)off;
   memcpy((char*)io + off, mount, ml);
   memcpy((char*)io + off + ml, path, pl + 1);
   off  += ml + pl + 1;

   if (tail)
   {
      size_t tl = strlen(tail);
      if (off + tl + 1 > IOSUHAX_REQ_SIZE)
         return -1;
      *tail_off = (uint32_t)off;
      memcpy((char*)io + off, tail, tl + 1);
      off      += tl + 1;
   }
   return (int)off;
}

/* A path request (word 2 = @w2 when @n is 3) whose reply, of @out_len
 * bytes, lands in @out (aligned). */
static int fsa_path_call(uint32_t cmd, int fsaFd, unsigned n, uint32_t w2,
      const char *mount, const char *path, void *out, uint32_t out_len)
{
   int len;
   uint32_t *io;
   IOSUHAX_STACK(buf, IOSUHAX_REQ_SIZE);

   io    = (uint32_t*)IOSUHAX_ALIGNED(buf);
   io[0] = (uint32_t)fsaFd;
   io[2] = w2;
   if ((len = fsa_path_req(io, n, mount, path, NULL, NULL)) < 0)
      return -ENAMETOOLONG;
   return iosuhax_call(cmd, io, (uint32_t)len, out ? out : io,
         out ? out_len : 4);
}

/* The reply of a call that answers [result][payload]: result returned,
 * payload of @size bytes copied to @dst. */
static int fsa_path_call_out(uint32_t cmd, int fsaFd, unsigned n,
      uint32_t w2, const char *mount, const char *path, void *dst,
      uint32_t size)
{
   int res;
   uint8_t *out;
   IOSUHAX_STACK(buf, 4 + sizeof(directoryEntry_s));

   out = (uint8_t*)IOSUHAX_ALIGNED(buf);
   res = fsa_path_call(cmd, fsaFd, n, w2, mount, path, out, 4 + size);
   if (res >= 0)
      memcpy(dst, out + 4, size);
   return res;
}

/* One read of @n bytes into @out: 0x40 of reply header, then data */
static int fsa_read_into(int fsaFd, int handle, uint8_t *out, uint32_t n)
{
   uint32_t *io;
   IOSUHAX_STACK(req, 20);
   io    = (uint32_t*)IOSUHAX_ALIGNED(req);
   io[0] = (uint32_t)fsaFd;
   io[1] = 1;
   io[2] = n;
   io[3] = (uint32_t)handle;
   io[4] = 0;
   return iosuhax_call(IOCTL_FSA_READFILE, io, 20, out,
         IOSUHAX_ROUNDUP(IOSUHAX_ALIGN + n));
}

/* Reads @len bytes into @dst. Returns the bytes read, or a negative
 * error when nothing was. */
static int fsa_read(int fsaFd, int handle, uint8_t *dst, uint32_t len)
{
   uint8_t saved[IOSUHAX_ALIGN];
   uint8_t *stage;
   uint32_t done = 0;
   IOSUHAX_STACK(stage, IOSUHAX_ALIGN + IOSUHAX_STAGE);

   stage = (uint8_t*)IOSUHAX_ALIGNED(stage);

   while (done < len)
   {
      int r;
      uint32_t n;
      uint8_t *bounce;
      uint8_t *p    = dst + done;
      uint32_t left = len - done;

      if (left <= IOSUHAX_STAGE)
      {
         n = left;
         if ((r = fsa_read_into(fsaFd, handle, stage, n)) > 0)
            memcpy(p, stage + IOSUHAX_ALIGN, (uint32_t)r < n ? (uint32_t)r : n);
      }
      else if (left <= IOSUHAX_BOUNCE && (bounce = iosuhax_bounce_claim()))
      {
         n = left;
         if ((r = fsa_read_into(fsaFd, handle, bounce, n)) > 0)
            memcpy(p, bounce + IOSUHAX_ALIGN, (uint32_t)r < n ? (uint32_t)r : n);
         iosuhax_bounce_release();
      }
      else if (done >= IOSUHAX_ALIGN && IOSUHAX_IS_ALIGNED(p))
      {
         /* Straight into the caller's buffer, the reply header over
          * the 0x40 bytes before it, which are put back after */
         n = left & ~(uint32_t)(IOSUHAX_ALIGN - 1);
         memcpy(saved, p - IOSUHAX_ALIGN, IOSUHAX_ALIGN);
         r = fsa_read_into(fsaFd, handle, p - IOSUHAX_ALIGN, n);
         memcpy(p - IOSUHAX_ALIGN, saved, IOSUHAX_ALIGN);
      }
      else
      {
         /* Just enough to leave the rest aligned with 0x40 bytes of
          * the caller's own in front of it */
         n = (uint32_t)(-(intptr_t)p) & (IOSUHAX_ALIGN - 1);
         while (done + n < IOSUHAX_ALIGN)
            n += IOSUHAX_ALIGN;
         if ((r = fsa_read_into(fsaFd, handle, stage, n)) > 0)
            memcpy(p, stage + IOSUHAX_ALIGN, (uint32_t)r < n ? (uint32_t)r : n);
      }

      /* a short read is not the end; nothing read is */
      if (r < 0)
         return done ? (int)done : r;
      if (r == 0)
         break;
      done += (uint32_t)r;
   }

   return (int)done;
}

/* Writes @len bytes from @src. Returns the bytes written, or a
 * negative error when nothing was. */
static int fsa_write(int fsaFd, int handle, const uint8_t *src,
      uint32_t len)
{
   uint8_t *buf;
   uint32_t *res;
   uint32_t done   = 0;
   uint32_t cap    = IOSUHAX_STAGE;
   uint8_t *bounce = NULL;
   IOSUHAX_STACK(stage, IOSUHAX_ALIGN + IOSUHAX_STAGE);
   IOSUHAX_STACK(out, 4);

   res = (uint32_t*)IOSUHAX_ALIGNED(out);
   buf = (uint8_t*)IOSUHAX_ALIGNED(stage);
   if (len > IOSUHAX_STAGE && (bounce = iosuhax_bounce_claim()))
   {
      buf = bounce;
      cap = IOSUHAX_BOUNCE;
   }

   while (done < len)
   {
      int r;
      uint32_t *io  = (uint32_t*)buf;
      uint32_t n    = len - done;
      if (n > cap)
         n = cap;

      io[0] = (uint32_t)fsaFd;
      io[1] = 1;
      io[2] = n;
      io[3] = (uint32_t)handle;
      io[4] = 0;
      memcpy(buf + IOSUHAX_ALIGN, src + done, n);
      r = iosuhax_call(IOCTL_FSA_WRITEFILE, buf,
            IOSUHAX_ROUNDUP(IOSUHAX_ALIGN + n), res, 4);

      if (r <= 0)
      {
         if (r < 0 && !done)
            done = (uint32_t)r;
         break;
      }
      done += (uint32_t)r;
   }

   if (bounce)
      iosuhax_bounce_release();
   return (int)done;
}

/* One raw request of @count sectors from @sector: the header in @io,
 * and for reads the reply (header, then data) in @out. */
static int fsa_raw_call(uint32_t cmd, int fsaFd, int dev, uint32_t sector,
      uint32_t count, uint8_t *io, uint32_t io_len, uint8_t *out,
      uint32_t out_len)
{
   uint32_t *w = (uint32_t*)io;
   w[0] = (uint32_t)fsaFd;
   w[1] = IOSUHAX_SECTOR;
   w[2] = count;
   w[3] = 0;           /* sector offset, high word */
   w[4] = sector;
   w[5] = (uint32_t)dev;
   return iosuhax_call(cmd, io, io_len, out, out_len);
}

static int fsa_raw_read(int fsaFd, int dev, uint8_t *dst, uint32_t sector,
      uint32_t count)
{
   uint8_t saved[IOSUHAX_ALIGN];
   uint8_t *stage, *req;
   int res         = 0;
   uint8_t *bounce = NULL;
   IOSUHAX_STACK(stage, IOSUHAX_ALIGN + IOSUHAX_STAGE);
   IOSUHAX_STACK(req, 24);

   stage = (uint8_t*)IOSUHAX_ALIGNED(stage);
   req   = (uint8_t*)IOSUHAX_ALIGNED(req);
   if (count * IOSUHAX_SECTOR > IOSUHAX_STAGE)
      bounce = iosuhax_bounce_claim();

   while (count)
   {
      uint32_t n     = count;
      uint32_t bytes = count * IOSUHAX_SECTOR;

      if (bytes <= IOSUHAX_STAGE)
      {
         res = fsa_raw_call(IOCTL_FSA_RAW_READ, fsaFd, dev, sector, n,
               req, 24, stage, IOSUHAX_ALIGN + bytes);
         if (res >= 0)
            memcpy(dst, stage + IOSUHAX_ALIGN, bytes);
      }
      else if (IOSUHAX_IS_ALIGNED(dst) && (bytes > IOSUHAX_BOUNCE || !bounce))
      {
         /* The first sector through the stack, the rest straight into
          * the caller's buffer with the reply header over the end of
          * that first sector, which is put back after */
         res = fsa_raw_call(IOCTL_FSA_RAW_READ, fsaFd, dev, sector, 1,
               req, 24, stage, IOSUHAX_ALIGN + IOSUHAX_SECTOR);
         if (res < 0)
            break;
         memcpy(dst, stage + IOSUHAX_ALIGN, IOSUHAX_SECTOR);
         memcpy(saved, dst + IOSUHAX_SECTOR - IOSUHAX_ALIGN, IOSUHAX_ALIGN);
         res = fsa_raw_call(IOCTL_FSA_RAW_READ, fsaFd, dev, sector + 1,
               n - 1, req, 24, dst + IOSUHAX_SECTOR - IOSUHAX_ALIGN,
               IOSUHAX_ALIGN + bytes - IOSUHAX_SECTOR);
         memcpy(dst + IOSUHAX_SECTOR - IOSUHAX_ALIGN, saved, IOSUHAX_ALIGN);
      }
      else
      {
         uint8_t *buf = bounce ? bounce : stage;
         uint32_t cap = (bounce ? IOSUHAX_BOUNCE : IOSUHAX_STAGE)
            / IOSUHAX_SECTOR;
         if (n > cap)
            n = cap;
         res = fsa_raw_call(IOCTL_FSA_RAW_READ, fsaFd, dev, sector, n,
               req, 24, buf, IOSUHAX_ALIGN + n * IOSUHAX_SECTOR);
         if (res >= 0)
            memcpy(dst, buf + IOSUHAX_ALIGN, n * IOSUHAX_SECTOR);
      }

      if (res < 0)
         break;
      dst    += n * IOSUHAX_SECTOR;
      sector += n;
      count  -= n;
   }

   if (bounce)
      iosuhax_bounce_release();
   return res;
}

static int fsa_raw_write(int fsaFd, int dev, const uint8_t *src,
      uint32_t sector, uint32_t count)
{
   uint8_t *buf;
   int res         = 0;
   uint32_t cap    = IOSUHAX_STAGE / IOSUHAX_SECTOR;
   uint8_t *bounce = NULL;
   IOSUHAX_STACK(stage, IOSUHAX_ALIGN + IOSUHAX_STAGE);

   buf = (uint8_t*)IOSUHAX_ALIGNED(stage);
   if (count > cap && (bounce = iosuhax_bounce_claim()))
   {
      buf = bounce;
      cap = IOSUHAX_BOUNCE / IOSUHAX_SECTOR;
   }

   while (count)
   {
      uint32_t n = count > cap ? cap : count;
      memcpy(buf + IOSUHAX_ALIGN, src, n * IOSUHAX_SECTOR);
      res = fsa_raw_call(IOCTL_FSA_RAW_WRITE, fsaFd, dev, sector, n,
            buf, IOSUHAX_ALIGN + n * IOSUHAX_SECTOR, buf, 4);
      if (res < 0)
         break;
      src    += n * IOSUHAX_SECTOR;
      sector += n;
      count  -= n;
   }

   if (bounce)
      iosuhax_bounce_release();
   return res;
}

/* Reply [result][fileStat_s] for an open file */
static int fsa_stat_file(int fsaFd, int handle, fileStat_s *out)
{
   int res;
   uint32_t *io;
   IOSUHAX_STACK(buf, 4 + sizeof(fileStat_s));
   io    = (uint32_t*)IOSUHAX_ALIGNED(buf);
   io[0] = (uint32_t)fsaFd;
   io[1] = (uint32_t)handle;
   if ((res = iosuhax_call(IOCTL_FSA_STATFILE, io, 8, io,
               4 + sizeof(fileStat_s))) >= 0)
      memcpy(out, io + 1, sizeof(*out));
   return res;
}

/* ---------------------------------------------------------------- */
/* devoptab                                                         */
/* ---------------------------------------------------------------- */

typedef struct fs_dev_private
{
   const char *mount_path;
   int fsaFd;
} fs_dev_private_t;

typedef struct fs_dev_file_state
{
   fs_dev_private_t *dev;
   int fd;          /* FSA file handle */
   int read;        /* allowed to read */
   int write;       /* allowed to write */
   uint32_t pos;    /* position, in bytes */
   uint32_t len;    /* length, in bytes */
} fs_dev_file_state_t;

typedef struct fs_dev_dir_entry
{
   fs_dev_private_t *dev;
   int dirHandle;
} fs_dev_dir_entry_t;

static int fs_dev_open_r(struct _reent *r, void *fileStruct,
      const char *path, int flags, int mode);

/* The device @path names (up to the first ':' or '/'), and in *rest
 * the path inside it. Matched by whole name: GetDeviceOpTab would take
 * "ntfs1" for "ntfs". */
static fs_dev_private_t *fs_dev_get(const char *path, const char **rest)
{
   int i;
   const char *colon = strchr(path, ':');
   size_t len        = strcspn(path, ":/");

   for (i = 3; i < STD_MAX; i++)
   {
      const devoptab_t *d = devoptab_list[i];
      if (     d && d->name && d->open_r == fs_dev_open_r
            && strlen(d->name) == len
            && !memcmp(d->name, path, len))
      {
         *rest = colon ? colon + 1 : path;
         return (fs_dev_private_t*)d->deviceData;
      }
   }
   return NULL;
}

static void fs_dev_fill_stat(struct stat *st, const fileStat_s *s,
      bool is_dir)
{
   memset(st, 0, sizeof(*st));
   st->st_mode   = is_dir ? S_IFDIR : S_IFREG;
   st->st_nlink  = 1;
   st->st_size   = s->size;
   st->st_blocks = (s->size + 511) >> 9;
   st->st_dev    = s->id;
   st->st_uid    = s->owner_id;
   st->st_gid    = s->group_id;
   st->st_ino    = s->id;
   st->st_atime  = s->mtime;
   st->st_ctime  = s->ctime;
   st->st_mtime  = s->mtime;
}

static int fs_dev_open_r(struct _reent *r, void *fileStruct,
      const char *path, int flags, int mode)
{
   int res;
   uint32_t mode_off;
   uint32_t *io;
   const char *fsMode;
   const char *rest;
   int len;
   fileStat_s stats;
   IOSUHAX_STACK(buf, IOSUHAX_REQ_SIZE);
   fs_dev_file_state_t *file = (fs_dev_file_state_t*)fileStruct;
   fs_dev_private_t *dev     = fs_dev_get(path, &rest);

   if (!dev)
   {
      r->_errno = ENODEV;
      return -1;
   }

   file->dev = dev;
   switch (flags)
   {
      case 0:     file->read = 1; file->write = 0; fsMode = "r";  break;
      case 2:     file->read = 1; file->write = 1; fsMode = "r+"; break;
      case 0x601: file->read = 0; file->write = 1; fsMode = "w";  break;
      case 0x602: file->read = 1; file->write = 1; fsMode = "w+"; break;
      case 0x209: file->read = 0; file->write = 1; fsMode = "a";  break;
      case 0x20A: file->read = 1; file->write = 1; fsMode = "a+"; break;
      default:
         r->_errno = EINVAL;
         return -1;
   }

   io    = (uint32_t*)IOSUHAX_ALIGNED(buf);
   io[0] = (uint32_t)dev->fsaFd;
   if ((len = fsa_path_req(io, 3, dev->mount_path, rest, fsMode,
               &mode_off)) < 0)
   {
      r->_errno = ENAMETOOLONG;
      return -1;
   }
   io[2] = mode_off;

   /* reply: result, handle */
   if ((res = iosuhax_call(IOCTL_FSA_OPENFILE, io, (uint32_t)len, io, 8)) != 0)
   {
      r->_errno = res;
      return -1;
   }
   file->fd  = (int)io[1];
   file->pos = 0;

   if ((res = fsa_stat_file(dev->fsaFd, file->fd, &stats)) != 0)
   {
      fsa_words(IOCTL_FSA_CLOSEFILE, (uint32_t)dev->fsaFd,
            (uint32_t)file->fd, 0, 2);
      r->_errno = res;
      return -1;
   }
   file->len = stats.size;
   return (int)(intptr_t)file;
}

static int fs_dev_close_r(struct _reent *r, void *fd)
{
   int res;
   fs_dev_file_state_t *file = (fs_dev_file_state_t*)fd;
   if (!file->dev)
   {
      r->_errno = ENODEV;
      return -1;
   }
   if ((res = fsa_words(IOCTL_FSA_CLOSEFILE, (uint32_t)file->dev->fsaFd,
               (uint32_t)file->fd, 0, 2)) < 0)
   {
      r->_errno = res;
      return -1;
   }
   return 0;
}

static off_t fs_dev_seek_r(struct _reent *r, void *fd, off_t pos, int dir)
{
   int res;
   uint32_t to;
   fs_dev_file_state_t *file = (fs_dev_file_state_t*)fd;
   if (!file->dev)
   {
      r->_errno = ENODEV;
      return -1;
   }

   switch (dir)
   {
      case SEEK_SET: to = (uint32_t)pos;             break;
      case SEEK_CUR: to = file->pos + (uint32_t)pos; break;
      case SEEK_END: to = file->len + (uint32_t)pos; break;
      default:
         r->_errno = EINVAL;
         return -1;
   }

   if ((res = fsa_words(IOCTL_FSA_SETFILEPOS, (uint32_t)file->dev->fsaFd,
               (uint32_t)file->fd, to, 3)) != 0)
   {
      r->_errno = res;
      return -1;
   }
   file->pos = to;
   return (off_t)to;
}

static ssize_t fs_dev_write_r(struct _reent *r, void *fd, const char *ptr,
      size_t len)
{
   int res;
   fs_dev_file_state_t *file = (fs_dev_file_state_t*)fd;
   if (!file->dev)
   {
      r->_errno = ENODEV;
      return -1;
   }
   if (!file->write)
   {
      r->_errno = EACCES;
      return -1;
   }

   if ((res = fsa_write(file->dev->fsaFd, file->fd, (const uint8_t*)ptr,
               (uint32_t)len)) < 0)
   {
      r->_errno = res;
      return -1;
   }
   file->pos += (uint32_t)res;
   if (file->pos > file->len)
      file->len = file->pos;
   return res;
}

static ssize_t fs_dev_read_r(struct _reent *r, void *fd, char *ptr,
      size_t len)
{
   int res;
   fs_dev_file_state_t *file = (fs_dev_file_state_t*)fd;
   if (!file->dev)
   {
      r->_errno = ENODEV;
      return -1;
   }
   if (!file->read)
   {
      r->_errno = EACCES;
      return -1;
   }

   if ((res = fsa_read(file->dev->fsaFd, file->fd, (uint8_t*)ptr,
               (uint32_t)len)) < 0)
   {
      r->_errno = res;
      return -1;
   }
   file->pos += (uint32_t)res;
   return res;
}

static int fs_dev_fstat_r(struct _reent *r, void *fd, struct stat *st)
{
   int res;
   fileStat_s stats;
   fs_dev_file_state_t *file = (fs_dev_file_state_t*)fd;
   if (!file->dev)
   {
      r->_errno = ENODEV;
      return -1;
   }
   if ((res = fsa_stat_file(file->dev->fsaFd, file->fd, &stats)) != 0)
   {
      r->_errno = res;
      return -1;
   }
   fs_dev_fill_stat(st, &stats, false);
   return 0;
}

static int fs_dev_ftruncate_r(struct _reent *r, void *fd, off_t len)
{
   r->_errno = ENOTSUP;
   return -1;
}

static int fs_dev_fsync_r(struct _reent *r, void *fd)
{
   r->_errno = ENOTSUP;
   return -1;
}

static int fs_dev_stat_r(struct _reent *r, const char *path,
      struct stat *st)
{
   int res;
   fileStat_s stats;
   const char *rest;
   fs_dev_private_t *dev = fs_dev_get(path, &rest);
   if (!dev)
   {
      r->_errno = ENODEV;
      return -1;
   }
   if ((res = fsa_path_call_out(IOCTL_FSA_GETSTAT, dev->fsaFd, 2, 0,
               dev->mount_path, rest, &stats, sizeof(stats))) < 0)
   {
      r->_errno = res;
      return -1;
   }
   /* the device's root counts as a directory too */
   fs_dev_fill_stat(st, &stats, (stats.flag & DIR_ENTRY_IS_DIRECTORY)
         || (rest[0] && !rest[1]));
   return 0;
}

static int fs_dev_link_r(struct _reent *r, const char *existing,
      const char *newLink)
{
   r->_errno = ENOTSUP;
   return -1;
}

/* A path call answering one word: 0 or the error in errno */
static int fs_dev_path_op(struct _reent *r, uint32_t cmd, const char *path,
      unsigned n, uint32_t w2)
{
   int res;
   const char *rest;
   fs_dev_private_t *dev = fs_dev_get(path, &rest);
   if (!dev)
   {
      r->_errno = ENODEV;
      return -1;
   }
   if ((res = fsa_path_call(cmd, dev->fsaFd, n, w2, dev->mount_path, rest,
               NULL, 0)) < 0)
   {
      r->_errno = res;
      return -1;
   }
   return 0;
}

static int fs_dev_unlink_r(struct _reent *r, const char *name)
{
   return fs_dev_path_op(r, IOCTL_FSA_REMOVE, name, 2, 0);
}

static int fs_dev_chdir_r(struct _reent *r, const char *name)
{
   return fs_dev_path_op(r, IOCTL_FSA_CHDIR, name, 2, 0);
}

static int fs_dev_rename_r(struct _reent *r, const char *oldName,
      const char *newName)
{
   r->_errno = ENOTSUP;
   return -1;
}

static int fs_dev_mkdir_r(struct _reent *r, const char *path, int mode)
{
   return fs_dev_path_op(r, IOCTL_FSA_MAKEDIR, path, 3, (uint32_t)mode);
}

static int fs_dev_statvfs_r(struct _reent *r, const char *path,
      struct statvfs *buf)
{
   int res;
   uint64_t size;
   /* type 0 answers 0x64 bytes; the free size is the first 8 */
   uint8_t info[0x64];
   const char *rest;
   fs_dev_private_t *dev = fs_dev_get(path, &rest);
   if (!dev)
   {
      r->_errno = ENODEV;
      return -1;
   }
   if ((res = fsa_path_call_out(IOCTL_FSA_GETDEVICEINFO, dev->fsaFd, 3, 0,
               dev->mount_path, rest, info, sizeof(info))) < 0)
   {
      r->_errno = res;
      return -1;
   }
   memcpy(&size, info, sizeof(size));

   memset(buf, 0, sizeof(*buf));
   buf->f_bsize   = 512;
   buf->f_frsize  = 512;
   buf->f_blocks  = size >> 9; /* unknown: the free size stands in */
   buf->f_bfree   = buf->f_bavail = size >> 9;
   buf->f_files   = 0xffffffff;
   buf->f_ffree   = 0xffffffff;
   buf->f_fsid    = (unsigned long)(uintptr_t)dev;
   buf->f_flag    = 0;
   buf->f_namemax = 255;
   return 0;
}

static DIR_ITER *fs_dev_diropen_r(struct _reent *r, DIR_ITER *dirState,
      const char *path)
{
   int res;
   uint32_t *out;
   const char *rest;
   IOSUHAX_STACK(buf, 8);
   fs_dev_dir_entry_t *dirIter = (fs_dev_dir_entry_t*)dirState->dirStruct;
   fs_dev_private_t *dev       = fs_dev_get(path, &rest);
   if (!dev)
   {
      r->_errno = ENODEV;
      return NULL;
   }
   out = (uint32_t*)IOSUHAX_ALIGNED(buf);
   /* reply: result, handle */
   if ((res = fsa_path_call(IOCTL_FSA_OPENDIR, dev->fsaFd, 2, 0,
               dev->mount_path, rest, out, 8)) < 0)
   {
      r->_errno = res;
      return NULL;
   }
   dirIter->dev       = dev;
   dirIter->dirHandle = (int)out[1];
   return dirState;
}

static int fs_dev_dir_op(struct _reent *r, DIR_ITER *dirState, uint32_t cmd)
{
   int res;
   fs_dev_dir_entry_t *dirIter = (fs_dev_dir_entry_t*)dirState->dirStruct;
   if (!dirIter->dev)
   {
      r->_errno = ENODEV;
      return -1;
   }
   if ((res = fsa_words(cmd, (uint32_t)dirIter->dev->fsaFd,
               (uint32_t)dirIter->dirHandle, 0, 2)) < 0)
   {
      r->_errno = res;
      return -1;
   }
   return 0;
}

static int fs_dev_dirclose_r(struct _reent *r, DIR_ITER *dirState)
{
   return fs_dev_dir_op(r, dirState, IOCTL_FSA_CLOSEDIR);
}

static int fs_dev_dirreset_r(struct _reent *r, DIR_ITER *dirState)
{
   return fs_dev_dir_op(r, dirState, IOCTL_FSA_REWINDDIR);
}

static int fs_dev_dirnext_r(struct _reent *r, DIR_ITER *dirState,
      char *filename, struct stat *st)
{
   int res;
   uint32_t *io;
   const directoryEntry_s *e;
   IOSUHAX_STACK(buf, 4 + sizeof(directoryEntry_s));
   fs_dev_dir_entry_t *dirIter = (fs_dev_dir_entry_t*)dirState->dirStruct;
   if (!dirIter->dev)
   {
      r->_errno = ENODEV;
      return -1;
   }

   io    = (uint32_t*)IOSUHAX_ALIGNED(buf);
   io[0] = (uint32_t)dirIter->dev->fsaFd;
   io[1] = (uint32_t)dirIter->dirHandle;
   /* reply: result, then the entry */
   if ((res = iosuhax_call(IOCTL_FSA_READDIR, io, 8, io,
               4 + sizeof(directoryEntry_s))) < 0)
   {
      r->_errno = res;
      return -1;
   }

   e = (const directoryEntry_s*)(io + 1);
   strcpy(filename, e->name);
   if (st)
      fs_dev_fill_stat(st, &e->stat,
            (e->stat.flag & DIR_ENTRY_IS_DIRECTORY) != 0);
   return 0;
}

static const devoptab_t devops_fs = {
   NULL, /* Device name */
   sizeof (fs_dev_file_state_t),
   fs_dev_open_r,
   fs_dev_close_r,
   fs_dev_write_r,
   fs_dev_read_r,
   fs_dev_seek_r,
   fs_dev_fstat_r,
   fs_dev_stat_r,
   fs_dev_link_r,
   fs_dev_unlink_r,
   fs_dev_chdir_r,
   fs_dev_rename_r,
   fs_dev_mkdir_r,
   sizeof (fs_dev_dir_entry_t),
   fs_dev_diropen_r,
   fs_dev_dirreset_r,
   fs_dev_dirnext_r,
   fs_dev_dirclose_r,
   fs_dev_statvfs_r,
   fs_dev_ftruncate_r,
   fs_dev_fsync_r,
   NULL, /* Device data, set by mount_fs */
   NULL, /* chmod_r */
   NULL  /* fchmod_r */
};

/* The devoptab, its name and the device's data are one allocation,
 * made at mount and freed at unmount. */
typedef struct fs_dev_mount
{
   devoptab_t ops;
   fs_dev_private_t priv;
} fs_dev_mount_t;

int mount_fs(const char *virt_name, int fsaFd, const char *mount_path)
{
   int i;
   char *name, *path;
   fs_dev_mount_t *m;
   size_t nl, pl;

   if (!virt_name || !mount_path)
   {
      errno = EINVAL;
      return -1;
   }

   iosuhax_bounce_init();

   nl = strlen(virt_name) + 1;
   pl = strlen(mount_path) + 1;
   if (!(m = (fs_dev_mount_t*)malloc(sizeof(*m) + nl + pl)))
   {
      errno = ENOMEM;
      return -1;
   }
   name = (char*)(m + 1);
   path = name + nl;
   memcpy(name, virt_name, nl);
   memcpy(path, mount_path, pl);

   memcpy(&m->ops, &devops_fs, sizeof(devoptab_t));
   m->priv.mount_path = path;
   m->priv.fsaFd      = fsaFd;
   m->ops.name        = name;
   m->ops.deviceData  = &m->priv;

   for (i = 3; i < STD_MAX; i++)
   {
      if (devoptab_list[i] == devoptab_list[0])
      {
         devoptab_list[i] = &m->ops;
         return 0;
      }
   }

   free(m);
   errno = EADDRNOTAVAIL;
   return -1;
}

int unmount_fs(const char *virt_name)
{
   int i;
   size_t len = strcspn(virt_name, ":/");

   for (i = 3; i < STD_MAX; i++)
   {
      const devoptab_t *d = devoptab_list[i];
      if (     d && d->name && d->open_r == fs_dev_open_r
            && strlen(d->name) == len
            && !memcmp(d->name, virt_name, len))
      {
         devoptab_list[i] = devoptab_list[0];
         /* ops is the allocation's first member */
         free((void*)d);
         return 0;
      }
   }
   return -1;
}

/* ---------------------------------------------------------------- */
/* libfat's sector interface                                        */
/* ---------------------------------------------------------------- */

static int fsa_fd_sd  = -1;
static int fsa_fd_usb = -1;
static int sdio_fd    = -1;
static int usb_fd     = -1;

static int fsa_raw_open(int fsaFd, const char *dev_path, int *handle)
{
   int res;
   uint32_t *out;
   IOSUHAX_STACK(buf, 8);
   out = (uint32_t*)IOSUHAX_ALIGNED(buf);
   /* reply: result, handle */
   if ((res = fsa_path_call(IOCTL_FSA_RAW_OPEN, fsaFd, 2, 0, NULL,
               dev_path, out, 8)) >= 0)
      *handle = (int)out[1];
   return res;
}

static bool iosuhax_disc_open_client(int *fsaFd)
{
   iosuhax_bounce_init();
   if (IOSUHAX_Open(NULL) < 0)
      return false;
   if (*fsaFd < 0)
      *fsaFd = IOSUHAX_FSA_Open();
   return *fsaFd >= 0;
}

static void iosuhax_disc_close_client(int *fsaFd)
{
   if (*fsaFd >= 0)
   {
      IOSUHAX_FSA_Close(*fsaFd);
      *fsaFd = -1;
   }
}

static bool IOSUHAX_sdio_startup(void)
{
   if (!iosuhax_disc_open_client(&fsa_fd_sd))
      return false;
   if (sdio_fd < 0 && fsa_raw_open(fsa_fd_sd, "/dev/sdcard01", &sdio_fd) < 0)
   {
      iosuhax_disc_close_client(&fsa_fd_sd);
      sdio_fd = -1;
   }
   return sdio_fd >= 0;
}

static bool IOSUHAX_sdio_isInserted(void)
{
   return fsa_fd_sd >= 0 && sdio_fd >= 0;
}

static bool IOSUHAX_sdio_clearStatus(void) { return true; }

static bool IOSUHAX_sdio_shutdown(void)
{
   if (!IOSUHAX_sdio_isInserted())
      return false;
   fsa_words(IOCTL_FSA_RAW_CLOSE, (uint32_t)fsa_fd_sd, (uint32_t)sdio_fd,
         0, 2);
   iosuhax_disc_close_client(&fsa_fd_sd);
   sdio_fd = -1;
   return true;
}

static bool IOSUHAX_sdio_readSectors(uint32_t sector, uint32_t numSectors,
      void *buffer)
{
   return IOSUHAX_sdio_isInserted()
      && fsa_raw_read(fsa_fd_sd, sdio_fd, (uint8_t*)buffer, sector,
            numSectors) >= 0;
}

static bool IOSUHAX_sdio_writeSectors(uint32_t sector, uint32_t numSectors,
      const void *buffer)
{
   return IOSUHAX_sdio_isInserted()
      && fsa_raw_write(fsa_fd_sd, sdio_fd, (const uint8_t*)buffer, sector,
            numSectors) >= 0;
}

const DISC_INTERFACE IOSUHAX_sdio_disc_interface =
{
   DEVICE_TYPE_WII_U_SD,
   FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_WII_U_SD,
   IOSUHAX_sdio_startup,
   IOSUHAX_sdio_isInserted,
   IOSUHAX_sdio_readSectors,
   IOSUHAX_sdio_writeSectors,
   IOSUHAX_sdio_clearStatus,
   IOSUHAX_sdio_shutdown
};

static bool IOSUHAX_usb_startup(void)
{
   if (!iosuhax_disc_open_client(&fsa_fd_usb))
      return false;
   if (     usb_fd < 0
         && fsa_raw_open(fsa_fd_usb, "/dev/usb01", &usb_fd) < 0
         && fsa_raw_open(fsa_fd_usb, "/dev/usb02", &usb_fd) < 0)
   {
      iosuhax_disc_close_client(&fsa_fd_usb);
      usb_fd = -1;
   }
   return usb_fd >= 0;
}

static bool IOSUHAX_usb_isInserted(void)
{
   return fsa_fd_usb >= 0 && usb_fd >= 0;
}

static bool IOSUHAX_usb_clearStatus(void) { return true; }

static bool IOSUHAX_usb_shutdown(void)
{
   if (!IOSUHAX_usb_isInserted())
      return false;
   fsa_words(IOCTL_FSA_RAW_CLOSE, (uint32_t)fsa_fd_usb, (uint32_t)usb_fd,
         0, 2);
   iosuhax_disc_close_client(&fsa_fd_usb);
   usb_fd = -1;
   return true;
}

static bool IOSUHAX_usb_readSectors(uint32_t sector, uint32_t numSectors,
      void *buffer)
{
   return IOSUHAX_usb_isInserted()
      && fsa_raw_read(fsa_fd_usb, usb_fd, (uint8_t*)buffer, sector,
            numSectors) >= 0;
}

static bool IOSUHAX_usb_writeSectors(uint32_t sector, uint32_t numSectors,
      const void *buffer)
{
   return IOSUHAX_usb_isInserted()
      && fsa_raw_write(fsa_fd_usb, usb_fd, (const uint8_t*)buffer, sector,
            numSectors) >= 0;
}

const DISC_INTERFACE IOSUHAX_usb_disc_interface =
{
   DEVICE_TYPE_WII_U_USB,
   FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_WII_U_USB,
   IOSUHAX_usb_startup,
   IOSUHAX_usb_isInserted,
   IOSUHAX_usb_readSectors,
   IOSUHAX_usb_writeSectors,
   IOSUHAX_usb_clearStatus,
   IOSUHAX_usb_shutdown
};
