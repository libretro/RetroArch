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
#ifndef VC_CONTAINERS_BYTESTREAM_H
#define VC_CONTAINERS_BYTESTREAM_H

/** \file
 * Utility functions to provide a byte stream out of a list of container packets
 */

typedef struct VC_CONTAINER_BYTESTREAM_T
{
   VC_CONTAINER_PACKET_T *first;  /**< first packet in the chain */
   VC_CONTAINER_PACKET_T **last;  /**< last packet in the chain */

   VC_CONTAINER_PACKET_T *current;  /**< packet containing the current read pointer */
   size_t current_offset; /**< position of current packet (in bytes) */
   size_t offset; /**< position within the current packet */

   size_t bytes; /**< Number of bytes available in the bytestream */

} VC_CONTAINER_BYTESTREAM_T;

/*****************************************************************************/
STATIC_INLINE void bytestream_init( VC_CONTAINER_BYTESTREAM_T *stream )
{
   stream->first = stream->current = NULL;
   stream->last = &stream->first;
   stream->offset = stream->current_offset = stream->bytes = 0;
}

STATIC_INLINE void bytestream_push( VC_CONTAINER_BYTESTREAM_T *stream,
                                    VC_CONTAINER_PACKET_T *packet )
{
   *stream->last = packet;
   stream->last = &packet->next;
   packet->next = NULL;
   if( !stream->current ) stream->current = packet;
   stream->bytes += packet->size;
}

STATIC_INLINE VC_CONTAINER_PACKET_T *bytestream_pop( VC_CONTAINER_BYTESTREAM_T *stream )
{
   VC_CONTAINER_PACKET_T *packet = stream->first;

   if( stream->current == packet )
      return NULL;
   vc_container_assert(packet);

   stream->bytes -= packet->size;
   stream->current_offset -= packet->size;
   stream->first = packet->next;
   if( !stream->first )
      stream->last = &stream->first;
   return packet;
}

STATIC_INLINE VC_CONTAINER_PACKET_T *bytestream_get_packet( VC_CONTAINER_BYTESTREAM_T *stream, size_t *offset )
{
   VC_CONTAINER_PACKET_T *packet = stream->current;
   size_t off = stream->offset;

   while(packet && packet->size == off)
   {
      packet = packet->next;
      off = 0;
   }

   if (offset)
      *offset = off;

   return packet;
}

STATIC_INLINE bool bytestream_skip_packet( VC_CONTAINER_BYTESTREAM_T *stream )
{
   VC_CONTAINER_PACKET_T *packet = stream->current;

   if( packet )
   {
      stream->current = packet->next;
      stream->current_offset += (packet->size - stream->offset);
      stream->offset = 0;
   }

   return !!packet;
}

STATIC_INLINE void bytestream_get_timestamps( VC_CONTAINER_BYTESTREAM_T *stream, int64_t *pts, int64_t *dts, bool b_same )
{
   VC_CONTAINER_PACKET_T *packet = bytestream_get_packet( stream, 0 );

   if(packet)
   {
      if(b_same && packet->pts == VC_CONTAINER_TIME_UNKNOWN) packet->pts = packet->dts;
      if(pts) *pts = packet->pts;
      if(dts) *dts = packet->dts;

      packet->pts = packet->dts = VC_CONTAINER_TIME_UNKNOWN;
      return;
   }

   if(pts) *pts = VC_CONTAINER_TIME_UNKNOWN;
   if(dts) *dts = VC_CONTAINER_TIME_UNKNOWN;
}

STATIC_INLINE void bytestream_get_timestamps_and_offset( VC_CONTAINER_BYTESTREAM_T *stream,
   int64_t *pts, int64_t *dts, size_t *offset, bool b_same )
{
   VC_CONTAINER_PACKET_T *packet = bytestream_get_packet( stream, offset );

   if(packet)
   {
      if(b_same && packet->pts == VC_CONTAINER_TIME_UNKNOWN) packet->pts = packet->dts;
      if(pts) *pts = packet->pts;
      if(dts) *dts = packet->dts;

      packet->pts = packet->dts = VC_CONTAINER_TIME_UNKNOWN;
      return;
   }

   if(pts) *pts = VC_CONTAINER_TIME_UNKNOWN;
   if(dts) *dts = VC_CONTAINER_TIME_UNKNOWN;
}

STATIC_INLINE size_t bytestream_size( VC_CONTAINER_BYTESTREAM_T *stream )
{
   return stream->bytes - stream->current_offset - stream->offset;
}

STATIC_INLINE VC_CONTAINER_STATUS_T bytestream_skip( VC_CONTAINER_BYTESTREAM_T *stream, size_t size )
{
   VC_CONTAINER_PACKET_T *packet;
   size_t offset, bytes = 0, skip;

   if( !size )
      return VC_CONTAINER_SUCCESS; /* Nothing to do */
   if( stream->bytes - stream->current_offset - stream->offset < size )
      return VC_CONTAINER_ERROR_EOS; /* Not enough data */

   for( packet = stream->current, offset = stream->offset; ;
        packet = packet->next, offset = 0 )
   {
      if( packet->size - offset >= size)
         break;

      skip = packet->size - offset;
      bytes += skip;
      size -= skip;
   }

   stream->current = packet;
   stream->current_offset += stream->offset - offset + bytes;
   stream->offset = offset + size;
   return VC_CONTAINER_SUCCESS;
}

STATIC_INLINE VC_CONTAINER_STATUS_T bytestream_get( VC_CONTAINER_BYTESTREAM_T *stream, uint8_t *data, size_t size )
{
   VC_CONTAINER_PACKET_T *packet;
   size_t offset, bytes = 0, copy;

   if( !size )
      return VC_CONTAINER_SUCCESS; /* Nothing to do */
   if( stream->bytes - stream->current_offset - stream->offset < size )
      return VC_CONTAINER_ERROR_EOS; /* Not enough data */

   for( packet = stream->current, offset = stream->offset; ;
        packet = packet->next, offset = 0 )
   {
      if( packet->size - offset >= size)
         break;

      copy = packet->size - offset;
      memcpy( data, packet->data + offset, copy );
      bytes += copy;
      data += copy;
      size -= copy;
   }

   memcpy( data, packet->data + offset, size );
   stream->current = packet;
   stream->current_offset += stream->offset - offset + bytes;
   stream->offset = offset + size;
   return VC_CONTAINER_SUCCESS;
}

STATIC_INLINE VC_CONTAINER_STATUS_T bytestream_peek( VC_CONTAINER_BYTESTREAM_T *stream, uint8_t *data, size_t size )
{
   VC_CONTAINER_PACKET_T *packet;
   size_t offset, copy;

   if( !size )
      return VC_CONTAINER_SUCCESS; /* Nothing to do */
   if( stream->bytes - stream->current_offset - stream->offset < size )
      return VC_CONTAINER_ERROR_EOS; /* Not enough data */

   for( packet = stream->current, offset = stream->offset; ;
        packet = packet->next, offset = 0 )
   {
      if( packet->size - offset >= size)
         break;

      copy = packet->size - offset;
      memcpy( data, packet->data + offset, copy );
      data += copy;
      size -= copy;
   }

   memcpy( data, packet->data + offset, size );
   return VC_CONTAINER_SUCCESS;
}

STATIC_INLINE VC_CONTAINER_STATUS_T bytestream_peek_at( VC_CONTAINER_BYTESTREAM_T *stream,
   size_t peek_offset, uint8_t *data, size_t size )
{
   VC_CONTAINER_PACKET_T *packet;
   size_t copy;

   if( !size )
      return VC_CONTAINER_SUCCESS; /* Nothing to do */
   if( stream->bytes - stream->current_offset - stream->offset < peek_offset + size )
      return VC_CONTAINER_ERROR_EOS; /* Not enough data */

   peek_offset += stream->offset;

   /* Find the right place */
   for( packet = stream->current; ; packet = packet->next )
   {
      if( packet->size > peek_offset )
         break;

      peek_offset -= packet->size;
   }

   /* Copy the data */
   for( ; ; packet = packet->next, peek_offset = 0 )
   {
      if( packet->size - peek_offset >= size)
         break;

      copy = packet->size - peek_offset;
      memcpy( data, packet->data + peek_offset, copy );
      data += copy;
      size -= copy;
   }

   memcpy( data, packet->data + peek_offset, size );
   return VC_CONTAINER_SUCCESS;
}

STATIC_INLINE VC_CONTAINER_STATUS_T bytestream_skip_byte( VC_CONTAINER_BYTESTREAM_T *stream )
{
   VC_CONTAINER_PACKET_T *packet = stream->current;

   if( !packet )
      return VC_CONTAINER_ERROR_EOS;

   /* Fast path first */
   if( packet->size - stream->offset )
   {
      stream->offset++;
      return VC_CONTAINER_SUCCESS;
   }

   return bytestream_skip( stream, 1 );
}

STATIC_INLINE VC_CONTAINER_STATUS_T packet_peek_byte( VC_CONTAINER_BYTESTREAM_T *stream,
                                                      uint8_t *data )
{
   VC_CONTAINER_PACKET_T *packet = stream->current;

   if( !packet )
      return VC_CONTAINER_ERROR_EOS;

   /* Fast path first */
   if( packet->size - stream->offset )
   {
      *data = packet->data[stream->offset];
      return VC_CONTAINER_SUCCESS;
   }

   return bytestream_peek( stream, data, 1 );
}

STATIC_INLINE VC_CONTAINER_STATUS_T packet_get_byte( VC_CONTAINER_BYTESTREAM_T *stream,
                                                     uint8_t *data )
{
   VC_CONTAINER_PACKET_T *packet = stream->current;

   if( !packet )
      return VC_CONTAINER_ERROR_EOS;

   /* Fast path first */
   if( packet->size - stream->offset )
   {
      *data = packet->data[stream->offset];
      stream->offset++;
      return VC_CONTAINER_SUCCESS;
   }

   return bytestream_get( stream, data, 1 );
}

STATIC_INLINE VC_CONTAINER_STATUS_T bytestream_find_startcode( VC_CONTAINER_BYTESTREAM_T *stream,
   size_t *search_offset, const uint8_t *startcode, unsigned int length )
{
   VC_CONTAINER_PACKET_T *packet, *backup_packet = NULL;
   size_t position, start_offset = position = *search_offset;
   size_t offset, backup_offset = 0;
   unsigned int match = 0;

   if( stream->bytes - stream->current_offset - stream->offset < start_offset + length )
      return VC_CONTAINER_ERROR_EOS; /* Not enough data */

   /* Find the right place */
   for( packet = stream->current, offset = stream->offset;
        packet != NULL; packet = packet->next, offset = 0 )
   {
      if( packet->size - offset > start_offset)
         break;

      start_offset -= (packet->size - offset);
   }

   /* Start the search for the start code.
    * To make things simple we try to find a match one byte at a time. */
   for( offset += start_offset;
        packet != NULL; packet = packet->next, offset = 0 )
   {
      for( ; offset < packet->size; offset++ )
      {
         if( packet->data[offset] != startcode[match] )
         {
            if ( match ) /* False positive */
            {
               packet = backup_packet;
               offset = backup_offset;
               match = 0;
            }
            position++;
            continue;
         }

         /* We've got a match */
         if( !match++ )
         {
            backup_packet = packet;
            backup_offset = offset;
         }

         if( match == length )
         {
            /* We have the full start code it */
            *search_offset = position;
            return VC_CONTAINER_SUCCESS;
         }
      }
   }

   *search_offset = position;
   return VC_CONTAINER_ERROR_EOS; /* No luck in finding the start code */
}

#endif /* VC_CONTAINERS_BYTESTREAM_H */
