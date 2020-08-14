/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (nbio_orbis.c).
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

#if defined(ORBIS)
#include <stdio.h>
#include <stdlib.h>
#include <orbisFile.h>
#include <unistd.h>
#include <sys/fcntl.h>

struct nbio_orbis_t
{
   void* data;
   size_t progress;
   size_t len;
   int fd;
   unsigned int mode;
   /*
    * possible values:
    * NBIO_READ, NBIO_WRITE - obvious
    * -1 - currently doing nothing
    * -2 - the pointer was reallocated since the last operation
    */
   signed char op;
};

static void *nbio_orbis_open(const char * filename, unsigned int mode)
{
   static const int o_flags[]  =   { O_RDONLY, O_RDWR | O_CREAT | O_TRUNC,
      O_RDWR, O_RDONLY, O_RDWR | O_CREAT | O_TRUNC };
   void *buf                   = NULL;
   struct nbio_orbis_t* handle = NULL;
   size_t len                  = 0;
   int fd                      = orbisOpen(filename, o_flags[mode], 0644);

   if (fd < 0)
      return NULL;
   handle                = (struct nbio_orbis_t*)malloc(sizeof(struct nbio_orbis_t));

   if (!handle)
      goto error;

   handle->fd             = fd;

   switch (mode)
   {
      case NBIO_WRITE:
      case BIO_WRITE:
         break;
      default:
         len=orbisLseek(handle->fd, 0, SEEK_END);
         orbisLseek(handle->fd, 0, SEEK_SET);
         break;
   }

   handle->mode          = mode;

   if (len)
      buf                = malloc(len);

   if (len && !buf)
      goto error;

   handle->data          = buf;
   handle->len           = len;
   handle->progress      = handle->len;
   handle->op            = -2;

   return handle;

error:
   if (handle)
      free(handle);
   orbisClose(fd);
   return NULL;
}

static void nbio_orbis_begin_read(void *data)
{

   struct nbio_orbis_t *handle = (struct nbio_orbis_t*)data;
   if (!handle)
      return;

   if (handle->op >= 0)
      return;

   orbisLseek(handle->fd, 0, SEEK_SET);

   handle->op       = NBIO_READ;
   handle->progress = 0;
}

static void nbio_orbis_begin_write(void *data)
{
   struct nbio_orbis_t *handle = (struct nbio_orbis_t*)data;
   if (!handle)
      return;

   if (handle->op >= 0)
      return;

   orbisLseek(handle->fd, 0, SEEK_SET);
   handle->op = NBIO_WRITE;
   handle->progress = 0;
}

static bool nbio_orbis_iterate(void *data)
{
   size_t amount               = 65536;
   struct nbio_orbis_t *handle = (struct nbio_orbis_t*)data;

   if (!handle)
      return false;

   if (amount > handle->len - handle->progress)
      amount = handle->len - handle->progress;

   switch (handle->op)
   {
      case NBIO_READ:
         if (handle->mode == BIO_READ)
            amount = handle->len;
         break;
      case NBIO_WRITE:
         if (handle->mode == BIO_WRITE)
         {
            size_t written = 0;
            amount = handle->len;
            written = orbisWrite(handle->fd, (char*)handle->data, amount);

            if (written != amount)
               return false;
         }
         break;
   }

   handle->progress += amount;

   if (handle->progress == handle->len)
      handle->op = -1;
   return (handle->op < 0);
}

static void nbio_orbis_resize(void *data, size_t len)
{
   struct nbio_orbis_t *handle = (struct nbio_orbis_t*)data;
   if (!handle)
      return;

   if (handle->op >= 0)
      return;
   if (len < handle->len)
      return;

   handle->len      = len;
   handle->data     = realloc(handle->data, handle->len);
   handle->op       = -1;
   handle->progress = handle->len;
}

static void *nbio_orbis_get_ptr(void *data, size_t* len)
{
   struct nbio_orbis_t *handle = (struct nbio_orbis_t*)data;
   if (!handle)
      return NULL;
   if (len)
      *len = handle->len;
   if (handle->op == -1)
      return handle->data;
   return NULL;
}

static void nbio_orbis_cancel(void *data)
{
   struct nbio_orbis_t *handle = (struct nbio_orbis_t*)data;
   if (!handle)
      return;
   handle->op = -1;
   handle->progress = handle->len;
}

static void nbio_orbis_free(void *data)
{
   struct nbio_orbis_t *handle = (struct nbio_orbis_t*)data;
   if (!handle)
      return;

   if (handle->op >= 0)
      return;

   orbisClose(handle->fd);
   free(handle->data);

   handle->data = NULL;
   free(handle);
}

nbio_intf_t nbio_orbis = {
   nbio_orbis_open,
   nbio_orbis_begin_read,
   nbio_orbis_begin_write,
   nbio_orbis_iterate,
   nbio_orbis_resize,
   nbio_orbis_get_ptr,
   nbio_orbis_cancel,
   nbio_orbis_free,
   "nbio_orbis",
};
#endif
