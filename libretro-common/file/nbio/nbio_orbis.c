/* Copyright  (C) 2010-2018 The RetroArch team
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
   int fd;
   void* data;
   size_t progress;
   size_t len;
   /*
    * possible values:
    * NBIO_READ, NBIO_WRITE - obvious
    * -1 - currently doing nothing
    * -2 - the pointer was reallocated since the last operation
    */
   signed char op;
   unsigned int mode;
};

static void *nbio_orbis_open(const char * filename, unsigned int mode)
{
   static const int o_flags[]  =   { O_RDONLY, O_RDWR | O_CREAT | O_TRUNC,
      O_RDWR, O_RDONLY, O_RDWR | O_CREAT | O_TRUNC };
   void *buf                   = NULL;
   struct nbio_orbis_t* handle = NULL;
   size_t len                  = 0;
   int fd                      = orbisOpen(filename, o_flags[mode], 0644);

   RARCH_LOG("[NBIO_ORBIS] open %s\n" , filename);

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

   if (!buf)
   {
      RARCH_LOG("[NBIO_ORBIS]  open error malloc %d bytes\n",len);
   }

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
   RARCH_LOG("[NBIO_ORBIS]  open error closing %s\n" , filename);
   orbisClose(fd);
   return NULL;
}

static void nbio_orbis_begin_read(void *data)
{

   struct nbio_orbis_t *handle = (struct nbio_orbis_t*)data;
   if (!handle)
      return;
   RARCH_LOG("[NBIO_ORBIS] begin read fd=%d\n", handle->fd );

   if (handle->op >= 0)
   {
      RARCH_LOG("[NBIO_ORBIS] ERROR - attempted file read operation while busy\n");
      return;
   }

   orbisLseek(handle->fd, 0, SEEK_SET);

   handle->op       = NBIO_READ;
   handle->progress = 0;
}

static void nbio_orbis_begin_write(void *data)
{
   struct nbio_orbis_t *handle = (struct nbio_orbis_t*)data;
   if (!handle)
      return;
   RARCH_LOG("[NBIO_ORBIS] begin write fd=%d\n", handle->fd );

   if (handle->op >= 0)
   {
      RARCH_LOG("[NBIO_ORBIS] ERROR - attempted file write operation while busy\n");
      return;
   }

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
   RARCH_LOG("[NBIO_ORBIS] begin iterate fd=%d\n", handle->fd );

   if (amount > handle->len - handle->progress)
      amount = handle->len - handle->progress;

   switch (handle->op)
   {
      case NBIO_READ:
         if (handle->mode == BIO_READ)
         {
            amount = handle->len;
            RARCH_LOG("[NBIO_ORBIS] iterate BIO_READ  fd=%d readbytes=%d\n", handle->fd, orbisRead(handle->fd, (char*)handle->data, amount));

         }
         else
         {
            RARCH_LOG("[NBIO_ORBIS] iterate read  fd=%d handle->progress=%d readbytes=%d\n", handle->fd, handle->progress, orbisRead(handle->fd, (char*)handle->data + handle->progress, amount));

         }
         break;
      case NBIO_WRITE:
         if (handle->mode == BIO_WRITE)
         {
            size_t written = 0;
            amount = handle->len;
            written = orbisWrite(handle->fd, (char*)handle->data, amount);
            RARCH_LOG("[NBIO_ORBIS] iterate BIO_WRITE  fd=%d writebytes=%d\n", handle->fd, written);

            if (written != amount)
            {
               RARCH_LOG("[NBIO_ORBIS] iterate BIO_WRITE error   fd=%d amount=%d != writebytes=%d\n", handle->fd, amount, written);

               return false;
            }
         }
         else
         {
            RARCH_LOG("[NBIO_ORBIS] iterate write  fd=%d writebytes=%d\n", handle->fd, orbisWrite(handle->fd, (char*)handle->data + handle->progress, amount));

         }
         break;
   }

   handle->progress += amount;
   RARCH_LOG("[NBIO_ORBIS] end iterate fd=%d\n", handle->fd );

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
   {
      RARCH_LOG("[NBIO_ORBIS] ERROR - attempted file resize operation while busy\n");
      return;
   }
   if (len < handle->len)
   {
      RARCH_LOG("[NBIO_ORBIS] ERROR - attempted file shrink operation, not implemented");
      return;
   }

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
   RARCH_LOG("[NBIO_ORBIS] get pointer\n");
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
   RARCH_LOG("[NBIO_ORBIS] cancel \n");
   handle->op = -1;
   handle->progress = handle->len;
}

static void nbio_orbis_free(void *data)
{
   struct nbio_orbis_t *handle = (struct nbio_orbis_t*)data;
   if (!handle)
      return;
   RARCH_LOG("[NBIO_ORBIS] begin free fd=%d\n", handle->fd );

   if (handle->op >= 0)
   {
      RARCH_LOG("[NBIO_ORBIS] ERROR - attempted free() while busy\n");
      return;
   }
   RARCH_LOG("[NBIO_ORBIS] free close fd=%d\n",handle->fd);

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
