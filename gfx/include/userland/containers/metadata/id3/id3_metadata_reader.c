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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define CONTAINER_IS_BIG_ENDIAN
//#define ENABLE_CONTAINERS_LOG_FORMAT
//#define ENABLE_CONTAINERS_LOG_FORMAT_VERBOSE
#define CONTAINER_HELPER_LOG_INDENT(a) 0
#include "containers/core/containers_private.h"
#include "containers/core/containers_io_helpers.h"
#include "containers/core/containers_utils.h"
#include "containers/core/containers_logging.h"

#include "id3_metadata_strings.h"

/******************************************************************************
Defines
******************************************************************************/
#define ID3_SYNC_SAFE(x) ((((x >> 24) & 0x7f) << 21) | (((x >> 16) & 0x7f) << 14) | \
                          (((x >>  8) & 0x7f) <<  7) | (((x >>  0) & 0x7f) <<  0))
      
/******************************************************************************
Type definitions
******************************************************************************/

/******************************************************************************
Function prototypes
******************************************************************************/
VC_CONTAINER_STATUS_T id3_metadata_reader_open( VC_CONTAINER_T * );

/******************************************************************************
Local Functions
******************************************************************************/
static VC_CONTAINER_METADATA_T *id3_metadata_append( VC_CONTAINER_T *p_ctx,
                                                     VC_CONTAINER_METADATA_KEY_T key,
                                                     unsigned int size )
{
   VC_CONTAINER_METADATA_T *meta, **p_meta;
   unsigned int i;
   
   for (i = 0; i != p_ctx->meta_num; ++i)
   {
      if (key == p_ctx->meta[i]->key) break;
   }
   
   /* Avoid duplicate entries for now */
   if (i < p_ctx->meta_num) return NULL;
     
   /* Sanity check size, truncate if necessary */
   size = MIN(size, 512);

   /* Allocate a new metadata entry */
   if((meta = malloc(sizeof(VC_CONTAINER_METADATA_T) + size)) == NULL)
      return NULL;

   /* We need to grow the array holding the metadata entries somehow, ideally,
      we'd like to use a linked structure of some sort but realloc is probably 
      okay in this case */
   if((p_meta = realloc(p_ctx->meta, sizeof(VC_CONTAINER_METADATA_T *) * (p_ctx->meta_num + 1))) == NULL)
   {
      free(meta);
      return NULL;
   }

   p_ctx->meta = p_meta;
   memset(meta, 0, sizeof(VC_CONTAINER_METADATA_T) + size);
   p_ctx->meta[p_ctx->meta_num] = meta;
   meta->key = key;
   meta->value = (char *)&meta[1];
   meta->size = size;
   p_ctx->meta_num++;
      
   return meta;
}

/*****************************************************************************/
static VC_CONTAINER_METADATA_T *id3_read_metadata_entry( VC_CONTAINER_T *p_ctx, 
   VC_CONTAINER_METADATA_KEY_T key, unsigned int len )
{
   VC_CONTAINER_METADATA_T *meta;
      
   if ((meta = id3_metadata_append(p_ctx, key, len + 1)) != NULL)
   {
      unsigned int size = meta->size - 1;
      READ_BYTES(p_ctx, meta->value, size);
   
      if (len > size)
      {
         LOG_DEBUG(p_ctx, "metadata value truncated (%d characters lost)", len - size);
         SKIP_BYTES(p_ctx, len - size);
      }
   }
   else
   {
      SKIP_BYTES(p_ctx, len);
   }
   
   return meta;
}

/*****************************************************************************/
static VC_CONTAINER_METADATA_T *id3_read_metadata_entry_ex( VC_CONTAINER_T *p_ctx, 
   VC_CONTAINER_METADATA_KEY_T key, unsigned int len, const char *encoding )
{
   VC_CONTAINER_METADATA_T *meta;
      
   if ((meta = id3_metadata_append(p_ctx, key, encoding ? len + 2 : len + 1)) != NULL)
   {
      unsigned int size;
      
      if (encoding)
      {
         size = meta->size - 2;
         READ_STRING_UTF16(p_ctx, meta->value, size, "ID3v2 data");
      }
      else
      {
         size = meta->size - 1;
         READ_STRING(p_ctx, meta->value, size, "ID3v2 data");
      }

      if (len > size)
      {
         LOG_DEBUG(p_ctx, "metadata value truncated (%d characters lost)", len - size);
         SKIP_BYTES(p_ctx, len - size);
      }
   }
   
   return meta;
}

/*****************************************************************************/
static VC_CONTAINER_METADATA_T *id3_add_metadata_entry( VC_CONTAINER_T *p_ctx, 
   VC_CONTAINER_METADATA_KEY_T key, const char *value )
{
   VC_CONTAINER_METADATA_T *meta;
   unsigned int len = strlen(value);

   if ((meta = id3_metadata_append(p_ctx, key, len + 1)) != NULL)
   {
      unsigned int size = meta->size - 1;
      
      if (len > size)
      {
         LOG_DEBUG(p_ctx, "metadata value truncated (%d characters lost)", len - size);
      }
      
      strncpy(meta->value, value, size);
   }

   return meta;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T id3_read_id3v2_frame( VC_CONTAINER_T *p_ctx, 
   VC_CONTAINER_FOURCC_T frame_id, uint32_t frame_size )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_METADATA_KEY_T key;
   VC_CONTAINER_METADATA_T *meta = NULL;
   uint8_t encoding;
   const char *charset = NULL;

   if(frame_size < 1) return VC_CONTAINER_ERROR_CORRUPTED;

   switch (frame_id)
   {
      case VC_FOURCC('T','A','L','B'): key = VC_CONTAINER_METADATA_KEY_ALBUM; break;
      case VC_FOURCC('T','I','T','2'): key = VC_CONTAINER_METADATA_KEY_TITLE; break;
      case VC_FOURCC('T','R','C','K'): key = VC_CONTAINER_METADATA_KEY_TRACK; break;
      case VC_FOURCC('T','P','E','1'): key = VC_CONTAINER_METADATA_KEY_ARTIST; break;
      case VC_FOURCC('T','C','O','N'): key = VC_CONTAINER_METADATA_KEY_GENRE; break;
      default: key = VC_CONTAINER_METADATA_KEY_UNKNOWN; break;
   }

   if (key == VC_CONTAINER_METADATA_KEY_UNKNOWN) return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   encoding = READ_U8(p_ctx, "ID3v2 text encoding byte");
   frame_size -= 1;

   switch(encoding)
   {
      case 0: /* ISO-8859-1 */
      case 3: /* UTF-8 */
         break;
      case 1: /* UTF-16 with BOM */
         if(frame_size < 2) return VC_CONTAINER_ERROR_CORRUPTED;
         SKIP_U16(p_ctx, "ID3v2 text encoding BOM"); /* FIXME: Check BOM, 0xFFFE vs 0xFEFFF */
         frame_size -= 2;
         charset = "UTF16-LE";
         break;
      case 2: /* UTF-16BE */
         charset = "UTF16-BE";
         break;
      default:
         LOG_DEBUG(p_ctx, "skipping frame, text encoding %x not supported", encoding);
         SKIP_BYTES(p_ctx, frame_size);
         return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   }

   if ((meta = id3_read_metadata_entry_ex(p_ctx, key, frame_size, charset)) != NULL)
   {
      if (charset)
      {
         utf8_from_charset(charset, meta->value, meta->size, meta->value, meta->size);
      }

      meta->encoding = VC_CONTAINER_CHAR_ENCODING_UTF8; /* Okay for ISO-8859-1 as well? */

      status = VC_CONTAINER_SUCCESS;
   }
   else
   {
      SKIP_BYTES(p_ctx, frame_size);
   }

   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T id3_read_id3v2_tag( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   uint8_t maj_version, flags;
   uint32_t tag_size, size = 0;
   uint8_t peek_buf[10];
   
   SKIP_STRING(p_ctx, 3, "ID3v2 identifier");
   maj_version = READ_U8(p_ctx, "ID3v2 version (major)");
   SKIP_U8(p_ctx, "ID3v2 version (minor)");
   flags = READ_U8(p_ctx, "ID3v2 flags");
   tag_size = READ_U32(p_ctx, "ID3v2 syncsafe tag size");
   tag_size = ID3_SYNC_SAFE(tag_size);
   LOG_DEBUG(p_ctx, "ID3v2 tag size: %d", tag_size);

   /* Check that we support this major version */
   if (!(maj_version == 4 || maj_version == 3 || maj_version == 2))
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   /* We can't currently handle unsynchronisation */
   if ((flags >> 7) & 1)
   {
      LOG_DEBUG(p_ctx, "skipping unsynchronised tag, not supported");
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   }

   /* FIXME: check for version 2.2 and extract iTunes gapless playback information */
   if (maj_version == 2) return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   if ((flags >> 6) & 1)
   {
      /* Skip extended header, we don't support it */
      uint32_t ext_hdr_size;
      LOG_DEBUG(p_ctx, "skipping ID3v2 extended header, not supported");
      ext_hdr_size = READ_U32(p_ctx, "ID3v2 syncsafe extended header size");
      ext_hdr_size = ID3_SYNC_SAFE(ext_hdr_size);
      LOG_DEBUG(p_ctx, "ID3v2 extended header size: %d", ext_hdr_size);
      SKIP_BYTES(p_ctx, MIN(tag_size, ext_hdr_size));
      size += ext_hdr_size;
   }
   
   while (PEEK_BYTES(p_ctx, peek_buf, 10) == 10 && size < tag_size)
   {
      VC_CONTAINER_FOURCC_T frame_id;
      uint32_t frame_size;
      uint8_t format_flags;
      
      frame_id = READ_FOURCC(p_ctx, "Frame ID");
      frame_size = READ_U32(p_ctx, "Frame Size");
     
      if (maj_version >= 4)
      {
         frame_size = ID3_SYNC_SAFE(frame_size);
         LOG_DEBUG(p_ctx, "ID3v2 actual frame size: %d", frame_size);
      }

      SKIP_U8(p_ctx, "ID3v2 status message flags");
      format_flags = READ_U8(p_ctx, "ID3v2 format description flags");

      size += 10;

      if((status = STREAM_STATUS(p_ctx)) != VC_CONTAINER_SUCCESS || !frame_id) 
         break;

      /* Early exit if we detect an invalid tag size */
      if (size + frame_size > tag_size)
      {
         status = VC_CONTAINER_ERROR_FORMAT_INVALID;
         break;
      }
     
      /* We can't currently handle unsynchronised frames */
      if ((format_flags >> 1) & 1)
      {
         LOG_DEBUG(p_ctx, "skipping unsynchronised frame, not supported");
         SKIP_BYTES(p_ctx, frame_size);
         continue;
      }
      
      if ((status = id3_read_id3v2_frame(p_ctx, frame_id, frame_size)) != VC_CONTAINER_SUCCESS)
      {
         LOG_DEBUG(p_ctx, "skipping unsupported frame");
         SKIP_BYTES(p_ctx, frame_size);
      }

      size += frame_size;
   }

   /* Try to skip to end of tag in case we bailed out early */
   if (size < tag_size) SKIP_BYTES(p_ctx, tag_size - size);
      
   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T id3_read_id3v1_tag( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   uint8_t track, genre;
   char track_num[4] = {0};

   SKIP_STRING(p_ctx, 3, "ID3v1 identifier");
   /* ID3v1 title */
   id3_read_metadata_entry(p_ctx, VC_CONTAINER_METADATA_KEY_TITLE, 30);
   /* ID3v1 artist */
   id3_read_metadata_entry(p_ctx, VC_CONTAINER_METADATA_KEY_ARTIST, 30);
   /* ID3v1 album */
   id3_read_metadata_entry(p_ctx, VC_CONTAINER_METADATA_KEY_ALBUM, 30);
   /* ID3v1 year */
   id3_read_metadata_entry(p_ctx, VC_CONTAINER_METADATA_KEY_YEAR, 4);
   SKIP_STRING(p_ctx, 28, "ID3v1 comment");
   if (READ_U8(p_ctx, "ID3v1 zero-byte") == 0)
   {
      track = READ_U8(p_ctx, "ID3v1 track");
      snprintf(track_num, sizeof(track_num) - 1, "%02d", track);
      id3_add_metadata_entry(p_ctx, VC_CONTAINER_METADATA_KEY_TRACK, track_num);
   }
   else
   {
      SKIP_BYTES(p_ctx, 1);
   }
   genre = READ_U8(p_ctx, "ID3v1 genre");
   if (genre < countof(id3_genres))
   {
      id3_add_metadata_entry(p_ctx, VC_CONTAINER_METADATA_KEY_GENRE, id3_genres[genre]);
   }

   status = STREAM_STATUS(p_ctx);

   return status;
}

/*****************************************************************************
Functions exported as part of the Container Module API
 *****************************************************************************/

/*****************************************************************************/
static VC_CONTAINER_STATUS_T id3_metadata_reader_close( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_PARAM_UNUSED(p_ctx);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T id3_metadata_reader_open( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_FORMAT_INVALID;
   uint8_t peek_buf[10];
   int64_t data_offset;

   if (PEEK_BYTES(p_ctx, peek_buf, 10) != 10)
     return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   
   /* Initial ID3v2 tag(s), variable size */
   while ((peek_buf[0] == 'I') && (peek_buf[1] == 'D') && (peek_buf[2] == '3'))
   {
      if ((status = id3_read_id3v2_tag(p_ctx)) != VC_CONTAINER_SUCCESS)
      {
         LOG_DEBUG(p_ctx, "error reading ID3v2 tag (%i)", status);
      }

      if (PEEK_BYTES(p_ctx, peek_buf, 10) != 10) break;
   }

   data_offset = STREAM_POSITION(p_ctx);

   /* ID3v1 tag, 128 bytes at the end of a file */
   if (p_ctx->priv->io->size >= INT64_C(128) && STREAM_SEEKABLE(p_ctx))
   {
      SEEK(p_ctx, p_ctx->priv->io->size - INT64_C(128));
      if (PEEK_BYTES(p_ctx, peek_buf, 3) != 3) goto end;
        
      if ((peek_buf[0] == 'T') && (peek_buf[1] == 'A') && (peek_buf[2] == 'G'))
      {
         if ((status = id3_read_id3v1_tag(p_ctx)) != VC_CONTAINER_SUCCESS)
         {
            LOG_DEBUG(p_ctx, "error reading ID3v1 tag (%i)", status);
         }
      }
   }

end:
   /* Restore position to start of data */
   if (STREAM_POSITION(p_ctx) != data_offset)
      SEEK(p_ctx, data_offset);

   p_ctx->priv->pf_close = id3_metadata_reader_close;

   if((status = STREAM_STATUS(p_ctx)) != VC_CONTAINER_SUCCESS) goto error;   
   
   return VC_CONTAINER_SUCCESS;

error:
   LOG_DEBUG(p_ctx, "error opening stream (%i)", status);
   return status;
}

/********************************************************************************
 Entrypoint function
 ********************************************************************************/

#if !defined(ENABLE_CONTAINERS_STANDALONE) && defined(__HIGHC__)
# pragma weak reader_open id3_metadata_reader_open
#endif
