/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (data_transfer.c).
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

#include <stdlib.h>

#include <file/nbio.h>
#include <formats/data_transfer.h>

struct data_transfer
{
   void   *nbio;
   size_t  len;      /* full file length, fixed at open              */
   size_t  avail;    /* bytes valid at the front; monotonic          */
   uint8_t done;     /* backend reported the operation over          */
   uint8_t failed;   /* ...but delivered less than the file          */
};

data_transfer_t *data_transfer_open(const char *path)
{
   data_transfer_t *dt;
   void *handle = nbio_open(path, NBIO_READ);
   if (!handle)
      return NULL;
   if (!(dt = (data_transfer_t*)calloc(1, sizeof(*dt))))
   {
      nbio_free(handle);
      return NULL;
   }
   dt->nbio = handle;
   nbio_get_ptr(handle, &dt->len);
   nbio_begin_read(handle);
   return dt;
}

data_transfer_t *data_transfer_adopt(void *nbio)
{
   data_transfer_t *dt;
   size_t done = 0, total = 0;
   if (!nbio)
      return NULL;
   if (!(dt = (data_transfer_t*)calloc(1, sizeof(*dt))))
      return NULL;
   dt->nbio = nbio;
   nbio_get_ptr(nbio, &dt->len);
   /* Fold in wherever the read already got to: mid-operation the
    * progress is the valid prefix; an already-finished operation
    * settles to complete or failed exactly as a fill would. */
   if (nbio_get_progress(nbio, &done, &total))
      dt->avail = done;
   else
   {
      dt->done = 1;
      if (total > 0 && done < total)
      {
         dt->failed = 1;
         dt->avail  = done;
      }
      else
         dt->avail = dt->len;
   }
   return dt;
}

/* Fold the backend's post-operation state into done/failed.  Backends
 * that do not track progress (the mmap family, whose iterate completes
 * immediately with the whole mapping) report zeros and count as
 * complete. */
static void data_transfer_settle(data_transfer_t *dt)
{
   size_t done = 0, total = 0;
   if (nbio_get_progress(dt->nbio, &done, &total))
   {
      /* still in flight */
      if (done > dt->avail)
         dt->avail = done;
      return;
   }
   dt->done = 1;
   if (total > 0 && done < total)
   {
      dt->failed = 1;
      if (done > dt->avail)
         dt->avail = done;
   }
   else
      dt->avail = dt->len;
}

size_t data_transfer_iterate(data_transfer_t *dt, size_t max_bytes)
{
   size_t start;
   if (!dt)
      return 0;
   if (dt->done)
      return dt->avail;
   start = dt->avail;
   for (;;)
   {
      if (nbio_iterate(dt->nbio))
         break;
      data_transfer_settle(dt);
      if (max_bytes && dt->avail - start >= max_bytes)
         return dt->avail;
   }
   data_transfer_settle(dt);
   return dt->avail;
}

const uint8_t *data_transfer_ptr(data_transfer_t *dt, size_t *len)
{
   if (!dt)
   {
      if (len)
         *len = 0;
      return NULL;
   }
   if (len)
      *len = dt->len;
   return (const uint8_t*)nbio_get_ptr(dt->nbio, NULL);
}

size_t data_transfer_avail(data_transfer_t *dt)
{
   return dt ? dt->avail : 0;
}

bool data_transfer_complete(data_transfer_t *dt)
{
   return dt && dt->done && !dt->failed;
}

bool data_transfer_failed(data_transfer_t *dt)
{
   return dt && dt->failed;
}

void data_transfer_free(data_transfer_t *dt)
{
   if (!dt)
      return;
   if (dt->nbio)
   {
      nbio_cancel(dt->nbio);
      nbio_free(dt->nbio);
   }
   free(dt);
}
