#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <file/nbio.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <linux/aio_abi.h>

struct nbio_t
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

struct nbio_t* nbio_open(const char * filename, unsigned mode)
{
   static const int o_flags[] =   { O_RDONLY, O_RDWR|O_CREAT|O_TRUNC, O_RDWR, O_RDONLY, O_RDWR|O_CREAT|O_TRUNC };

   aio_context_t ctx     = 0;
   struct nbio_t* handle = NULL;
   int fd                = open(filename, o_flags[mode]|O_CLOEXEC, 0644);
   if (fd < 0)
      return NULL;

   if (io_setup(128, &ctx) < 0)
   {
      close(fd);
      return NULL;
   }

   handle       = malloc(sizeof(struct nbio_t));
   handle->fd   = fd;
   handle->ctx  = ctx;
   handle->len  = lseek(fd, 0, SEEK_END);
   handle->ptr  = malloc(handle->len);
   handle->busy = false;

   return handle;
}

static void nbio_begin_op(struct nbio_t* handle, uint16_t op)
{
   struct iocb * cbp = &handle->cb;

   memset(&handle->cb, 0, sizeof(handle->cb));
   handle->cb.aio_fildes = handle->fd;
   handle->cb.aio_lio_opcode = op;

   handle->cb.aio_buf = (uint64_t)(uintptr_t)handle->ptr;
   handle->cb.aio_offset = 0;
   handle->cb.aio_nbytes = handle->len;

   if (io_submit(handle->ctx, 1, &cbp) != 1)
   {
      puts("ERROR - io_submit() failed");
      abort();
   }

   handle->busy = true;
}

void nbio_begin_read(struct nbio_t* handle)
{
   nbio_begin_op(handle, IOCB_CMD_PREAD);
}

void nbio_begin_write(struct nbio_t* handle)
{
   nbio_begin_op(handle, IOCB_CMD_PWRITE);
}

bool nbio_iterate(struct nbio_t* handle)
{
   if (handle->busy)
   {
      struct io_event ev;
      if (io_getevents(handle->ctx, 0, 1, &ev, NULL) == 1)
         handle->busy = false;
   }
   return !handle->busy;
}

void nbio_resize(struct nbio_t* handle, size_t len)
{
   if (len < handle->len)
   {
      /* this works perfectly fine if this check is removed, but it 
       * won't work on other nbio implementations */
      /* therefore, it's blocked so nobody accidentally relies on it */
      puts("ERROR - attempted file shrink operation, not implemented");
      abort();
   }
   if (ftruncate(handle->fd, len) != 0)
   {
      puts("ERROR - couldn't resize file (ftruncate)");
      abort(); /* this one returns void and I can't find any other way 
                  for it to report failure */
   }
   handle->ptr = realloc(handle->ptr, len);
   handle->len = len;
}

void* nbio_get_ptr(struct nbio_t* handle, size_t* len)
{
   if (!handle)
      return NULL;
   if (len)
      *len = handle->len;
   if (!handle->busy)
      return handle->ptr;
   return NULL;
}

void nbio_cancel(struct nbio_t* handle)
{
   if (!handle)
      return;

   if (handle->busy)
   {
      struct io_event ev;
      io_cancel(handle->ctx, &handle->cb, &ev);
      handle->busy = false;
   }
}

void nbio_free(struct nbio_t* handle)
{
   if (!handle)
      return;

   io_destroy(handle->ctx);
   close(handle->fd);
   free(handle->ptr);
   free(handle);
}
