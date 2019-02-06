/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "containers/core/containers_private.h"
#include "containers/core/containers_io_helpers.h"
#include "containers/core/containers_logging.h"

void vc_container_helper_format_debug(VC_CONTAINER_T *ctx, int indent, const char *format, ...)
{
   char debug_string[512];
   va_list args;
   int result;

   if(indent >= (int)sizeof(debug_string)) return;
   memset(debug_string, ' ', indent);

   va_start( args, format );
   result = vsnprintf(debug_string + indent, sizeof(debug_string) - indent, format, args);
   va_end( args );

   if(result <= 0) return;

   vc_container_log(ctx, VC_CONTAINER_LOG_FORMAT, debug_string);
   fflush(0);
}

uint64_t vc_container_helper_int_debug(VC_CONTAINER_T *ctx, int type, uint64_t value, const char *name, int indent)
{
   VC_CONTAINER_PARAM_UNUSED(ctx);

   if(type == LOG_FORMAT_TYPE_HEX)
      vc_container_helper_format_debug(ctx, indent, "%s: 0x%"PRIx64, name, value);
   else
      vc_container_helper_format_debug(ctx, indent, "%s: %"PRIi64, name, value);
   return value;
}

uint64_t vc_container_helper_read_debug(VC_CONTAINER_T *ctx, int type, int size,
   const char *name, uint8_t *buffer, int indent, int b_skip)
{
   int64_t offset = STREAM_POSITION(ctx);
   uint64_t value = 0;
   GUID_T guid;

   if(type == LOG_FORMAT_TYPE_STRING ||
      type == LOG_FORMAT_TYPE_STRING_UTF16_LE ||
      type == LOG_FORMAT_TYPE_STRING_UTF16_BE)
   {
      uint8_t stringbuf[256];
      char utf8buf[256];
      int stringsize = sizeof(stringbuf) - 2;

      if(!buffer)
      {
         buffer = stringbuf;
         if(size < stringsize) stringsize = size;
      }
      else stringsize = size;

      value = vc_container_io_read(ctx->priv->io, buffer, stringsize);

      if(!utf8_from_charset(type == LOG_FORMAT_TYPE_STRING ? "UTF8" : "UTF16-LE",
            utf8buf, sizeof(utf8buf), buffer, stringsize))
         vc_container_helper_format_debug(ctx, indent, "%s: \"%s\"", name, utf8buf);
      else
         vc_container_helper_format_debug(ctx, indent, "%s: (could not read)", name);

      if(size - stringsize)
         value += vc_container_io_skip(ctx->priv->io, size - stringsize);
      return value;
   }

   if(type == LOG_FORMAT_TYPE_UINT_LE)
   {
      switch(size)
      {
      case 1: value = vc_container_io_read_uint8(ctx->priv->io);  break;
      case 2: value = vc_container_io_read_le_uint16(ctx->priv->io); break;
      case 3: value = vc_container_io_read_le_uint24(ctx->priv->io); break;
      case 4: value = vc_container_io_read_le_uint32(ctx->priv->io); break;
      case 5: value = vc_container_io_read_le_uint40(ctx->priv->io); break;
      case 6: value = vc_container_io_read_le_uint48(ctx->priv->io); break;
      case 7: value = vc_container_io_read_le_uint56(ctx->priv->io); break;
      case 8: value = vc_container_io_read_le_uint64(ctx->priv->io); break;
      }
   }
   else if(type == LOG_FORMAT_TYPE_UINT_BE)
   {
      switch(size)
      {
      case 1: value = vc_container_io_read_uint8(ctx->priv->io);  break;
      case 2: value = vc_container_io_read_be_uint16(ctx->priv->io); break;
      case 3: value = vc_container_io_read_be_uint24(ctx->priv->io); break;
      case 4: value = vc_container_io_read_be_uint32(ctx->priv->io); break;
      case 5: value = vc_container_io_read_be_uint40(ctx->priv->io); break;
      case 6: value = vc_container_io_read_be_uint48(ctx->priv->io); break;
      case 7: value = vc_container_io_read_be_uint56(ctx->priv->io); break;
      case 8: value = vc_container_io_read_be_uint64(ctx->priv->io); break;
      }
   }
   else if(type == LOG_FORMAT_TYPE_FOURCC)
   {
      value = vc_container_io_read_fourcc(ctx->priv->io);
   }
   else if(type == LOG_FORMAT_TYPE_GUID)
   {
      value = vc_container_io_read(ctx->priv->io, &guid, 16);
   }
   else
   {
      vc_container_assert(0);
      return 0;
   }

   if(type == LOG_FORMAT_TYPE_GUID)
   {
      if(value == 16)
      {
      vc_container_helper_format_debug(ctx, indent, "%s: 0x%x-0x%x-0x%x-0x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",
            name, guid.word0, guid.short0, guid.short1,
            guid.bytes[0], guid.bytes[1], guid.bytes[2], guid.bytes[3],
            guid.bytes[4], guid.bytes[5], guid.bytes[6], guid.bytes[7]);
      if(buffer) memcpy(buffer, &guid, sizeof(guid));
      }
   }
   else if(type == LOG_FORMAT_TYPE_FOURCC)
   {
      uint32_t val = value;
      vc_container_helper_format_debug(ctx, indent, "%s: %4.4s", name, (char *)&val);
   }
   else
   {
      vc_container_helper_format_debug(ctx, indent, "%s: %"PRIi64, name, value);
   }

   if(b_skip) value = (STREAM_POSITION(ctx) - offset) != size;
   return value;
}

VC_CONTAINER_STATUS_T vc_container_helper_write_debug(VC_CONTAINER_T *ctx, int type, int size,
   const char *name, uint64_t value, const uint8_t *buffer, int indent, int silent)
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;

   if(type == LOG_FORMAT_TYPE_STRING)
   {
      value = vc_container_io_write(ctx->priv->io, buffer, size);
      if(!silent)
         vc_container_helper_format_debug(ctx, indent, "%s: \"%ls\"", name, buffer);
      return value == (uint64_t)size ? VC_CONTAINER_SUCCESS : VC_CONTAINER_ERROR_FAILED;
   }

   if(type == LOG_FORMAT_TYPE_UINT_LE)
   {
      switch(size)
      {
      case 1: status = vc_container_io_write_uint8(ctx->priv->io, (uint8_t)value);  break;
      case 2: status = vc_container_io_write_le_uint16(ctx->priv->io, (uint16_t)value); break;
      case 3: status = vc_container_io_write_le_uint24(ctx->priv->io, (uint32_t)value); break;
      case 4: status = vc_container_io_write_le_uint32(ctx->priv->io, (uint32_t)value); break;
      case 8: status = vc_container_io_write_le_uint64(ctx->priv->io, value); break;
      }
   }
   else if(type == LOG_FORMAT_TYPE_UINT_BE)
   {
      switch(size)
      {
      case 1: status = vc_container_io_write_uint8(ctx->priv->io, (uint8_t)value);  break;
      case 2: status = vc_container_io_write_be_uint16(ctx->priv->io, (uint16_t)value); break;
      case 3: status = vc_container_io_write_be_uint24(ctx->priv->io, (uint32_t)value); break;
      case 4: status = vc_container_io_write_be_uint32(ctx->priv->io, (uint32_t)value); break;
      case 8: status = vc_container_io_write_be_uint64(ctx->priv->io, value); break;
      }
   }
   else if(type == LOG_FORMAT_TYPE_FOURCC)
   {
      status = vc_container_io_write_fourcc(ctx->priv->io, (uint32_t)value);
   }
   else if(type == LOG_FORMAT_TYPE_GUID)
   {
      value = vc_container_io_write(ctx->priv->io, buffer, 16);
   }
   else
   {
      vc_container_assert(0);
      return 0;
   }

   if(status)
   {
      vc_container_helper_format_debug(ctx, indent, "write failed for %s", name);
      return status;
   }

   if(!silent)
   {
      if (type == LOG_FORMAT_TYPE_FOURCC)
      {
         vc_container_helper_format_debug(ctx, indent, "%s: %4.4s", name, (char *)&value);
      }
      else if(type == LOG_FORMAT_TYPE_GUID)
      {
         GUID_T guid;
         memcpy(&guid, buffer, sizeof(guid));
         vc_container_helper_format_debug(ctx, indent, "%s: 0x%x-0x%x-0x%x-0x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",
               name, guid.word0, guid.short0, guid.short1,
               guid.bytes[0], guid.bytes[1], guid.bytes[2], guid.bytes[3],
               guid.bytes[4], guid.bytes[5], guid.bytes[6], guid.bytes[7]);
      }
      else
      {
         vc_container_helper_format_debug(ctx, indent, "%s: %"PRIi64, name, value);
      }
   }

   return status;
}
