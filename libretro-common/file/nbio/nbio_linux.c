/* Copyright  (C) 2010-2018 The RetroArch team
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

#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <linux/aio_abi.h>

struct nbio_linux_t
{
   int fd;
   bool busy;

   aio_context_t ctx;
   struct iocb cb;

   void* ptr;
   size_t len;
};

/* there's also a Unix AIO thingy, but it's not in glibc
 * and we don't want more dependencies */

static int io_setup(unsigned nr, aio_context_t * ctxp)
{
   return syscall(__NR_io_setup, nr, ctxp);
}

static int io_destroy(aio_context_t ctx)
{
   return syscall(__NR_io_destroy, ctx);
}

static int io_submit(aio_context_t ctx, long nr, struct iocb ** cbp)
{
   return syscall(__NR_io_submit, ctx, nr, cbp);
}

static int io_cancel(aio_context_t ctx, struct iocb * iocb, struct io_event * result)
{
   return syscall(__NR_io_cancel, ctx, iocb, result);
}

static int io_getevents(aio_context_t ctx, long min_nr, long nr,
      struct io_event * events, struct timespec * timeout)
{
   return syscall(__NR_io_getevents, ctx, min_nr, nr, events, timeout);
}

static void nbio_begin_op(struct nbio_linux_t* handle, uint16_t op)
{
   struct iocb * cbp         = &handle->cb;

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

static void *nbio_linux_open(const char * filename, unsigned mode)
{
   static const int o_flags[]  =   { O_RDONLY, O_RDWR|O_CREAT|O_TRUNC, O_RDWR, O_RDONLY, O_RDWR|O_CREAT|O_TRUNC };

   aio_context_t ctx           = 0;
   struct nbio_linux_t* handle = NULL;
   int fd                      = open(filename, o_flags[mode]|O_CLOEXEC, 0644);
   if (fd < 0)
      return NULL;

   if (io_setup(128, &ctx) < 0)
   {
      close(fd);
      return NULL;
   }

   handle       = (struct nbio_linux_t*)malloc(sizeof(struct nbio_linux_t));
   handle->fd   = fd;
   handle->ctx  = ctx;
   handle->len  = lseek(fd, 0, SEEK_END);
   handle->ptr  = malloc(handle->len);
   handle->busy = false;

   return handle;
}

static void nbio_linux_begin_read(void *data)
{
   struct nbio_linux_t* handle = (struct nbio_linux_t*)data;
   if (handle)
      nbio_begin_op(handle, IOCB_CMD_PREAD);
}

static void nbio_linux_begin_write(void *data)
{
   struct nbio_linux_t* handle = (struct nbio_linux_t*)data;
   if (handle)
      nbio_begin_op(handle, IOCB_CMD_PWRITE);
}

static bool nbio_linux_iterate(void *data)
{
   struct nbio_linux_t* handle = (struct nbio_linux_t*)data;
   if (!handle)
      return false;
   if (handle->busy)
   {
      struct io_event ev;
      if (io_getevents(handle->ctx, 0, 1, &ev, NULL) == 1)
         handle->busy = false;
   }
   return !handle->busy;
}

static void nbio_linux_resize(void *data, size_t len)
{
   struct nbio_linux_t* handle = (struct nbio_linux_t*)data;
   if (!handle)
      return;

   /* This works perfectly fine if this check is removed, but it
    * won't work on other nbio implementations */
   /* therefore, it's blocked so nobody accidentally relies on it */
   if (len < handle->len)
      abort();

   if (ftruncate(handle->fd, len) != 0)
      abort(); /* this one returns void and I can't find any other way
                  for it to report failure */

   handle->ptr = realloc(handle->ptr, len);
   handle->len = len;
}

static void *nbio_linux_get_ptr(void *data, size_t* len)
{
   struct nbio_linux_t* handle = (struct nbio_linux_t*)data;
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
   struct nbio_linux_t* handle = (struct nbio_linux_t*)data;
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
   struct nbio_linux_t* handle = (struct nbio_linux_t*)data;
   if (!handle)
      return;

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
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   "nbio_linux",
};

#endif
