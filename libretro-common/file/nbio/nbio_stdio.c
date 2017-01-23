#include <stdio.h>
#include <stdlib.h>

#include <file/nbio.h>

struct nbio_t
{
   FILE* f;
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
   signed char mode;
};

static const char * modes[]={ "rb", "wb", "r+b", "rb", "wb", "r+b" };

struct nbio_t* nbio_open(const char * filename, unsigned mode)
{
   void *buf             = NULL;
   struct nbio_t* handle = NULL;
   size_t len            = 0;
   FILE* f               = fopen(filename, modes[mode]);
   if (!f)
      return NULL;

   handle                = (struct nbio_t*)malloc(sizeof(struct nbio_t));

   if (!handle)
      goto error;

   handle->f             = f;

   switch (mode)
   {
      case NBIO_WRITE:
      case BIO_WRITE:
         break;
      default:
         fseek(handle->f, 0, SEEK_END);
         len = ftell(handle->f);
         break;
   }

   handle->mode          = mode;

   if (len)
      buf                = malloc(len);

   if (!buf)
      goto error;

   handle->data          = buf;
   handle->len           = len;
   handle->progress      = handle->len;
   handle->op            = -2;

   return handle;

error:
   if (handle)
      free(handle);
   fclose(f);
   return NULL;
}

void nbio_begin_read(struct nbio_t* handle)
{
   if (!handle)
      return;

   if (handle->op >= 0)
   {
      puts("ERROR - attempted file read operation while busy");
      abort();
   }

   fseek(handle->f, 0, SEEK_SET);

   handle->op       = NBIO_READ;
   handle->progress = 0;
}

void nbio_begin_write(struct nbio_t* handle)
{
   if (!handle)
      return;

   if (handle->op >= 0)
   {
      puts("ERROR - attempted file write operation while busy");
      abort();
   }

   fseek(handle->f, 0, SEEK_SET);
   handle->op = NBIO_WRITE;
   handle->progress = 0;
}

bool nbio_iterate(struct nbio_t* handle)
{
   size_t amount = 65536;

   if (!handle)
      return false;

   if (amount > handle->len - handle->progress)
      amount = handle->len - handle->progress;

   switch (handle->op)
   {
      case NBIO_READ:
         if (handle->mode == BIO_READ)
         {
            amount = handle->len;
            fread((char*)handle->data, 1, amount, handle->f);
         }
         else
            fread((char*)handle->data + handle->progress, 1, amount, handle->f);
         break;
      case NBIO_WRITE:
         if (handle->mode == BIO_WRITE)
         {
            size_t written = 0;
            amount = handle->len;
            written = fwrite((char*)handle->data, 1, amount, handle->f);
            if (written != amount)
               return false;
         }
         else
            fwrite((char*)handle->data + handle->progress, 1, amount, handle->f);
         break;
   }

   handle->progress += amount;

   if (handle->progress == handle->len)
      handle->op = -1;
   return (handle->op < 0);
}

void nbio_resize(struct nbio_t* handle, size_t len)
{
   if (!handle)
      return;

   if (handle->op >= 0)
   {
      puts("ERROR - attempted file resize operation while busy");
      abort();
   }
   if (len < handle->len)
   {
      puts("ERROR - attempted file shrink operation, not implemented");
      abort();
   }

   handle->len  = len;
   handle->data = realloc(handle->data, handle->len);
   handle->op   = -1;
   handle->progress = handle->len;
}

void* nbio_get_ptr(struct nbio_t* handle, size_t* len)
{
   if (!handle)
      return NULL;
   if (len)
      *len = handle->len;
   if (handle->op == -1)
      return handle->data;
   return NULL;
}

void nbio_cancel(struct nbio_t* handle)
{
   if (!handle)
      return;

   handle->op = -1;
   handle->progress = handle->len;
}

void nbio_free(struct nbio_t* handle)
{
   if (!handle)
      return;
   if (handle->op >= 0)
   {
      puts("ERROR - attempted free() while busy");
      abort();
   }
   fclose(handle->f);
   free(handle->data);

   handle->f    = NULL;
   handle->data = NULL;
   free(handle);
}
