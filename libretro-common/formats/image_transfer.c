#include <stdint.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_RPNG
#include <formats/rpng.h>
#endif
#ifdef HAVE_RJPEG
#include <formats/rjpeg.h>
#endif

#include <formats/image.h>

void image_transfer_free(void *data, enum image_type_enum type)
{
   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         rpng_free((rpng_t*)data);
#endif
         break;
      case IMAGE_TYPE_JPEG:
#ifdef HAVE_RJPEG
         rjpeg_free((rjpeg_t*)data);
#endif
         break;
      case IMAGE_TYPE_NONE:
         break;
   }
}

void *image_transfer_new(enum image_type_enum type)
{
   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         return rpng_alloc();
#else
         break;
#endif
      case IMAGE_TYPE_JPEG:
#ifdef HAVE_RJPEG
         return rjpeg_alloc();
#else
         break;
#endif
      default:
         break;
   }

   return NULL;
}

bool image_transfer_start(void *data, enum image_type_enum type)
{

   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         if (!rpng_start((rpng_t*)data))
            break;
         return true;
#else
         break;
#endif
      case IMAGE_TYPE_JPEG:
#ifdef HAVE_RJPEG
         return true;
#endif
         break;
      case IMAGE_TYPE_NONE:
         break;
   }

   return false;
}

bool image_transfer_is_valid(
      void *data,
      enum image_type_enum type)
{
   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         return rpng_is_valid((rpng_t*)data);
#else
         break;
#endif
      case IMAGE_TYPE_JPEG:
#ifdef HAVE_RJPEG
         return true;
#else
         break;
#endif
      case IMAGE_TYPE_NONE:
         break;
   }

   return false;
}

void image_transfer_set_buffer_ptr(
      void *data,
      enum image_type_enum type,
      void *ptr)
{
   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         rpng_set_buf_ptr((rpng_t*)data, (uint8_t*)ptr);
#endif
         break;
      case IMAGE_TYPE_JPEG:
#ifdef HAVE_RJPEG
         rjpeg_set_buf_ptr((rjpeg_t*)data, (uint8_t*)ptr);
#endif
         break;
      case IMAGE_TYPE_NONE:
         break;
   }
}

int image_transfer_process(
      void *data,
      enum image_type_enum type,
      uint32_t **buf, size_t len,
      unsigned *width, unsigned *height)
{
   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         if (!rpng_is_valid((rpng_t*)data))
            return IMAGE_PROCESS_ERROR;

         return rpng_process_image(
               (rpng_t*)data,
               (void**)buf, len, width, height);
#else
         break;
#endif
      case IMAGE_TYPE_JPEG:
#ifdef HAVE_RJPEG
         return rjpeg_process_image((rjpeg_t*)data,
               (void**)buf, len, width, height);
#else
         break;
#endif
      case IMAGE_TYPE_NONE:
         break;
   }

   return 0;
}

bool image_transfer_iterate(void *data, enum image_type_enum type)
{
   
   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         if (!rpng_iterate_image((rpng_t*)data))
            return false;
#endif
         break;
      case IMAGE_TYPE_JPEG:
#ifdef HAVE_RJPEG
         return false;
#else
         break;
#endif
      case IMAGE_TYPE_NONE:
         return false;
   }

   return true;
}
