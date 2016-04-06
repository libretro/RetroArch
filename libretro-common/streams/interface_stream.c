#include <stdlib.h>

#include <streams/interface_stream.h>
#include <streams/file_stream.h>
#include <streams/memory_stream.h>

struct intfstream_internal
{
   enum intfstream_type type;

   struct
   {
      struct
      {
         uint8_t *data;
         unsigned size;
      } buf;
   } memory;
};

bool intfstream_resize(intfstream_internal_t *intf, intfstream_info_t *info)
{
   if (!intf || !info)
      return false;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         break;
      case INTFSTREAM_MEMORY:
         intf->memory.buf.data = info->buf.data;
         intf->memory.buf.size = info->buf.size;

         memstream_set_buffer(intf->memory.buf.data,
               intf->memory.buf.size);
         break;
   }

   return true;
}

void *intfstream_init(intfstream_info_t *info)
{
   intfstream_internal_t *intf = NULL;
   if (!info)
      goto error;

   intf = (intfstream_internal_t*)calloc(1, sizeof(*intf));

   if (!intf)
      goto error;

   intf->type = info->type;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         break;
      case INTFSTREAM_MEMORY:
         if (!intfstream_resize(intf, info))
            goto error;
         break;
   }

   return intf;

error:
   if (intf)
      free(intf);
   return NULL;
}

