/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (nbio_linux.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <file/nbio.h>

#if defined(__linux__)

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <linux/aio_abi.h>

/* O_DIRECT requires buffers aligned to the logical block size.
 * 512 bytes covers all common block devices (HDD, SSD, eMMC). */
#define NBIO_LINUX_ALIGN 512

/* Queue depth of 1: we only ever have one in-flight op per handle.
 * A smaller queue pins less kernel memory than the original 128. */
#define NBIO_LINUX_AIO_QUEUE_DEPTH 1

struct nbio_linux_t
{
   void*         ptr;
   aio_context_t ctx;
   struct iocb   cb;
   size_t        len;
   int           fd;
   bool          busy;
   bool          use_direct; /* true when O_DIRECT is active */
};

static int io_setup(unsigned nr, aio_context_t *ctxp)
{
   return syscall(__NR_io_setup, nr, ctxp);
}

static int io_destroy(aio_context_t ctx)
{
   return syscall(__NR_io_destroy, ctx);
}

static int io_submit(aio_context_t ctx, long nr, struct iocb **cbp)
{
   return syscall(__NR_io_submit, ctx, nr, cbp);
}

static int io_cancel(aio_context_t ctx, struct iocb *iocb, struct io_event *result)
{
   return syscall(__NR_io_cancel, ctx, iocb, result);
}

static int io_getevents(aio_context_t ctx, long min_nr, long nr,
      struct io_event *events, struct timespec *timeout)
{
   return syscall(__NR_io_getevents, ctx, min_nr, nr, events, timeout);
}

static void nbio_begin_op(struct nbio_linux_t *handle, uint16_t op)
{
   struct iocb *cbp = &handle->cb;

   /* Guard: submitting while already busy would queue a second op
    * into a depth-1 ring, causing io_submit to fail with EAGAIN. */
   if (handle->busy)
      abort();

   memset(&handle->cb, 0, sizeof(handle->cb));

   handle->cb.aio_fildes     = handle->fd;
   handle->cb.aio_lio_opcode = op;
   handle->cb.aio_buf        = (uint64_t)(uintptr_t)handle->ptr;
   handle->cb.aio_offset     = 0;
   handle->cb.aio_nbytes     = handle->len;

   if (io_submit(handle->ctx, 1, &cbp) != 1)
      abort();

   handle->busy = true;
}

static void *nbio_linux_open(const char *filename, unsigned mode)
{
   /* O_DIRECT is only safe for reads against existing files where we
    * know the exact file size upfront.  Write/create modes truncate
    * or grow the file, so skip O_DIRECT there to avoid EINVAL. */
   static const int o_flags[] = {
      O_RDONLY,                    /* NBIO_READ          */
      O_RDWR | O_CREAT | O_TRUNC, /* NBIO_WRITE         */
      O_RDWR,                      /* NBIO_UPDATE        */
      O_RDONLY,                    /* NBIO_READ_NOBLOCK  */
      O_RDWR | O_CREAT | O_TRUNC  /* NBIO_WRITE_NOBLOCK */
   };
   static const bool direct_ok[] = { true, false, false, true, false };

   aio_context_t        ctx    = 0;
   struct nbio_linux_t *handle = NULL;
   void                *ptr    = NULL;
   int                  flags  = o_flags[mode] | O_CLOEXEC;
   int                  fd;
   size_t               len;

   if (direct_ok[mode])
      flags |= O_DIRECT;

   fd = open(filename, flags, 0644);

   /* If O_DIRECT open failed (unsupported fs, e.g. tmpfs), fall back. */
   if (fd < 0 && direct_ok[mode])
   {
      flags &= ~O_DIRECT;
      fd     = open(filename, flags, 0644);
   }

   if (fd < 0)
      return NULL;

   if (io_setup(NBIO_LINUX_AIO_QUEUE_DEPTH, &ctx) < 0)
   {
      close(fd);
      return NULL;
   }

   len = (size_t)lseek(fd, 0, SEEK_END);

   /* posix_memalign: O_DIRECT mandates aligned buffers; using it
    * unconditionally also benefits non-O_DIRECT paths because aligned
    * memory avoids split cache lines on the DMA boundary. */
   if (posix_memalign(&ptr, NBIO_LINUX_ALIGN, len ? len : NBIO_LINUX_ALIGN) != 0)
   {
      io_destroy(ctx);
      close(fd);
      return NULL;
   }

   handle             = (struct nbio_linux_t*)malloc(sizeof(struct nbio_linux_t));
   if (!handle)
   {
      free(ptr);
      io_destroy(ctx);
      close(fd);
      return NULL;
   }

   handle->fd         = fd;
   handle->ctx        = ctx;
   handle->len        = len;
   handle->ptr        = ptr;
   handle->busy       = false;
   handle->use_direct = (flags & O_DIRECT) != 0;

   return handle;
}

static void nbio_linux_begin_read(void *data)
{
   struct nbio_linux_t *handle = (struct nbio_linux_t*)data;
   if (handle)
      nbio_begin_op(handle, IOCB_CMD_PREAD);
}

static void nbio_linux_begin_write(void *data)
{
   struct nbio_linux_t *handle = (struct nbio_linux_t*)data;
   if (handle)
      nbio_begin_op(handle, IOCB_CMD_PWRITE);
}

static bool nbio_linux_iterate(void *data)
{
   struct nbio_linux_t *handle = (struct nbio_linux_t*)data;
   if (!handle)
      return false;

   if (handle->busy)
   {
      struct io_event ev;
      /* min_nr=0: non-blocking poll — returns immediately if no event. */
      if (io_getevents(handle->ctx, 0, 1, &ev, NULL) == 1)
         handle->busy = false;
   }

   return !handle->busy;
}

static void nbio_linux_resize(void *data, size_t len)
{
   struct nbio_linux_t *handle = (struct nbio_linux_t*)data;
   void                *newptr = NULL;

   if (!handle)
      return;

   /* This works perfectly fine if this check is removed, but it
    * won't work on other nbio implementations */
   /* therefore, it's blocked so nobody accidentally relies on it */
   if (len < handle->len)
      abort();

   if (ftruncate(handle->fd, (off_t)len) != 0)
      abort();

   /* Keep alignment guarantee after resize. */
   if (posix_memalign(&newptr, NBIO_LINUX_ALIGN, len) != 0)
      abort();

   memcpy(newptr, handle->ptr, handle->len);
   free(handle->ptr);
   handle->ptr = newptr;
   handle->len = len;
}

static void *nbio_linux_get_ptr(void *data, size_t *len)
{
   struct nbio_linux_t *handle = (struct nbio_linux_t*)data;
   if (!handle)
      return NULL;
   if (len)
      *len = handle->len;
   if (!handle->busy)
      return handle->ptr;
   return NULL;
}

static void nbio_linux_cancel(void *data)
{
   struct nbio_linux_t *handle = (struct nbio_linux_t*)data;
   if (!handle)
      return;

   if (handle->busy)
   {
      struct io_event ev;
      io_cancel(handle->ctx, &handle->cb, &ev);
      handle->busy = false;
   }
}

static void nbio_linux_free(void *data)
{
   struct nbio_linux_t *handle = (struct nbio_linux_t*)data;
   if (!handle)
      return;

   /* Cancel any in-flight op before tearing down the context.
    * Destroying a context with a live iocb can leak kernel resources. */
   if (handle->busy)
   {
      struct io_event ev;
      io_cancel(handle->ctx, &handle->cb, &ev);
      handle->busy = false;
   }

   io_destroy(handle->ctx);
   close(handle->fd);
   free(handle->ptr);
   free(handle);
}

nbio_intf_t nbio_linux = {
   nbio_linux_open,
   nbio_linux_begin_read,
   nbio_linux_begin_write,
   nbio_linux_iterate,
   nbio_linux_resize,
   nbio_linux_get_ptr,
   nbio_linux_cancel,
   nbio_linux_free,
   "nbio_linux",
};

#else

nbio_intf_t nbio_linux = {
   NULL, NULL, NULL, NULL,
   NULL, NULL, NULL, NULL,
   "nbio_linux",
};

#endif
