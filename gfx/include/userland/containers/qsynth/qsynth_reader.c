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
#include <string.h>

#include "containers/core/containers_private.h"
#include "containers/core/containers_io_helpers.h"
#include "containers/core/containers_utils.h"
#include "containers/core/containers_logging.h"

/******************************************************************************
Defines.
******************************************************************************/

#define BI32(b) (((b)[0]<<24)|((b)[1]<<16)|((b)[2]<<8)|((b)[3]))
#define BI16(b) (((b)[0]<<8)|((b)[1]))

#define HEADER_LENGTH 14
#define MAX_TRACKS 128

/******************************************************************************
Type definitions
******************************************************************************/
struct _QSYNTH_SEGMENT_T {
   struct _QSYNTH_SEGMENT_T *next;
   uint32_t len;
   uint8_t *data;
};
typedef struct _QSYNTH_SEGMENT_T QSYNTH_SEGMENT_T;

typedef struct VC_CONTAINER_MODULE_T
{
   VC_CONTAINER_TRACK_T *track;
   uint32_t filesize;
   QSYNTH_SEGMENT_T *seg;
   QSYNTH_SEGMENT_T *pass;
   uint32_t sent;
   int64_t timestamp;
   uint32_t seek;
} VC_CONTAINER_MODULE_T;

/******************************************************************************
Function prototypes
******************************************************************************/
VC_CONTAINER_STATUS_T qsynth_reader_open( VC_CONTAINER_T * );

/******************************************************************************
Local Functions
******************************************************************************/

static VC_CONTAINER_STATUS_T qsynth_read_header(uint8_t *data, uint32_t *tracks,
   uint32_t *division, uint8_t *fps, uint8_t *dpf)
{
   if(data[0] != 'M' || data[1] != 'T' || data[2] != 'h' || data[3] != 'd' ||
      data[4] != 0   || data[5] != 0   || data[6] != 0   || data[7] != 6)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   if(data[12] < 0x80)
   {
      if(division) *division = BI16(data+12);
   }
   else
   {
      if(fps) *fps = 256-data[12];
      if(dpf) *dpf = data[13];
   }

   if(tracks) *tracks = BI16(data+10);

   return VC_CONTAINER_SUCCESS;
}

static int qsynth_read_variable(uint8_t *data, uint32_t *val)
{
   int i = 0;
   *val = 0;
   do {
      *val = (*val << 7) + (data[i] & 0x7f);
   } while(data[i++] & 0x80);

   return i;
}

static VC_CONTAINER_STATUS_T qsynth_read_event(uint8_t *data, uint32_t *used, uint8_t *last,
                                               uint32_t *time, uint32_t *tempo, uint32_t *end)
{
   int read;

   // need at least 4 bytes here
   read = qsynth_read_variable(data, time);

   if(data[read] == 0xff) // meta event
   {
      uint32_t len;
      uint8_t type = data[read+1];

      read += 2;
      read += qsynth_read_variable(data+read, &len);

      if(type == 0x2f) // end of track
      {
         if(len != 0)
            return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

         *end = 1;
      }
      else if(type == 0x51) // tempo event
      {
         if(len != 3)
            return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

         *tempo = (data[read]<<16) | (data[read+1]<<8) | data[read+2];
      }

      read += len;
   }
   else if(data[read] == 0xf0 || data[read] == 0xf7) // sysex events
   {
      uint32_t len;
      read += 1;
      read += qsynth_read_variable(data+read, &len) + len;
   }
   else // midi event
   {
      uint8_t type;

      if(data[read] < 128)
         type = *last;
      else
      {
         type = data[read] >> 4;
         *last = type;
         read++;
      }

      switch(type) {
      case 8: case 9: case 0xa: case 0xb: case 0xe:
         read += 2;
         break;
      case 0xc: case 0xd:
         read += 1;
         break;
      default:
         return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
      }
   }

   *used = read;
   return VC_CONTAINER_SUCCESS;
}

static VC_CONTAINER_STATUS_T qsynth_read_track(QSYNTH_SEGMENT_T *seg,
                                               uint32_t *ticks, int64_t *time,
                                               uint32_t *us_perclock, uint32_t *tempo_ticks)
{
   uint32_t total_ticks = 0;
   uint32_t used = 8;
   uint8_t last = 0;

   *time = 0LL;
   *tempo_ticks = 0;

   while(used < seg->len)
   {
      VC_CONTAINER_STATUS_T status;
      uint32_t event_ticks, new_tempo = 0, end = 0, event_used;
      if((status = qsynth_read_event(seg->data+used, &event_used, &last, &event_ticks, &new_tempo, &end)) != VC_CONTAINER_SUCCESS)
         return status;

      used += event_used;
      total_ticks += event_ticks;

      if(new_tempo != 0)
      {
         *time += ((int64_t) (total_ticks - *tempo_ticks)) * (*us_perclock);
         *us_perclock = new_tempo;
         *tempo_ticks = total_ticks;
      }

      if(end)
         break;
   }

   *ticks = total_ticks;
   return VC_CONTAINER_SUCCESS;
}

static VC_CONTAINER_STATUS_T qsynth_get_duration(VC_CONTAINER_T *p_ctx)
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_STATUS_T status;
   QSYNTH_SEGMENT_T **seg = &(module->seg);
   uint32_t i, tracks, division = 0, max_ticks = 0, us_perclock = 500000;
   uint32_t end_uspc = 0, end_ticks = 0;
   int64_t end_time = 0;
   uint8_t fps = 1, dpf = 1;

   if((*seg = malloc(sizeof(QSYNTH_SEGMENT_T) + HEADER_LENGTH)) == NULL)
      return VC_CONTAINER_ERROR_OUT_OF_MEMORY;

   (*seg)->next = NULL;
   (*seg)->len = HEADER_LENGTH;
   (*seg)->data = (uint8_t *) ((*seg) + 1);

   if(PEEK_BYTES(p_ctx, (*seg)->data, HEADER_LENGTH) != HEADER_LENGTH)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   if((status = qsynth_read_header((*seg)->data, &tracks, &division, &fps, &dpf)) != VC_CONTAINER_SUCCESS)
      return status;

   // if we have a suspiciously large number of tracks, this is probably a bad file
   if(tracks > MAX_TRACKS)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   SKIP_BYTES(p_ctx, HEADER_LENGTH);

   seg = &((*seg)->next);
   module->filesize = HEADER_LENGTH;

   if(division == 0)
   {
      us_perclock = 1000000 / (fps * dpf);
      division = 1;
   }

   for(i=0; i<tracks; i++)
   {
      uint32_t len, ticks, tempo_ticks;
      int64_t time;
      uint8_t dummy[8];

      if(READ_BYTES(p_ctx, dummy, sizeof(dummy)) != sizeof(dummy) ||
         dummy[0] != 'M' || dummy[1] != 'T' || dummy[2] != 'r' || dummy[3] != 'k')
         return VC_CONTAINER_ERROR_FORMAT_INVALID;

      len = BI32(dummy+4);

      // impose a 1mb limit on track size
      if(len > (1<<20) || (*seg = malloc(sizeof(QSYNTH_SEGMENT_T) + 8 + len)) == NULL)
         return VC_CONTAINER_ERROR_OUT_OF_MEMORY;

      module->filesize += len+8;
      (*seg)->next = NULL;
      (*seg)->len = len + 8;
      (*seg)->data = (uint8_t *) ((*seg) + 1);

      memcpy((*seg)->data, dummy, 8);
      if(READ_BYTES(p_ctx, (*seg)->data+8, len) != len)
         return VC_CONTAINER_ERROR_FORMAT_INVALID;

      if((status = qsynth_read_track(*seg, &ticks, &time, &us_perclock, &tempo_ticks)) != VC_CONTAINER_SUCCESS)
         return status;

      if(end_uspc == 0)
      {
         end_uspc = us_perclock;
         end_ticks = tempo_ticks;
         end_time = time;
      }

      if(ticks > max_ticks)
         max_ticks = ticks;

      seg = &((*seg)->next);
   }

   if(end_uspc == 0)
      return VC_CONTAINER_ERROR_FORMAT_INVALID;

   module->pass = module->seg;
   module->sent = 0;
   p_ctx->duration = (end_time + (((int64_t) (max_ticks - end_ticks)) * end_uspc)) / division;
   module->track->format->extradata = (uint8_t *) &module->filesize;
   module->track->format->extradata_size = 4;
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************
Functions exported as part of the Container Module API
*****************************************************************************/
static VC_CONTAINER_STATUS_T qsynth_reader_read( VC_CONTAINER_T *p_ctx,
                                              VC_CONTAINER_PACKET_T *packet,
                                              uint32_t flags )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;

   if(module->pass)
   {
      packet->size = module->pass->len - module->sent;
      packet->dts = packet->pts = 0;
      packet->track = 0;
      packet->flags = module->sent ? 0 : VC_CONTAINER_PACKET_FLAG_FRAME_START;
   }
   else
   {
      if(module->timestamp > p_ctx->duration)
         return VC_CONTAINER_ERROR_EOS;

      packet->size = 5;
      packet->dts = packet->pts = module->timestamp;
      packet->track = 0;
      packet->flags = VC_CONTAINER_PACKET_FLAG_FRAME;
   }

   if(flags & VC_CONTAINER_READ_FLAG_SKIP)
   {
      if(module->pass)
      {
         module->pass = module->pass->next;
         module->sent = 0;
      }
      else
      {
         // if we're playing then we can't really skip, but have to simulate a seek instead
         module->seek = 1;
         module->timestamp += 40;
      }

      return VC_CONTAINER_SUCCESS;
   }

   if(flags & VC_CONTAINER_READ_FLAG_INFO)
      return VC_CONTAINER_SUCCESS;

   // read frame into packet->data
   if(module->pass)
   {
      uint32_t copy = MIN(packet->size, packet->buffer_size);
      memcpy(packet->data, module->pass->data + module->sent, copy);
      packet->size = copy;

      if((module->sent += copy) == module->pass->len)
      {
         module->pass = module->pass->next;
         module->sent = 0;
         packet->flags |= VC_CONTAINER_PACKET_FLAG_FRAME_END;
      }
   }
   else
   {
      if(packet->buffer_size < packet->size)
         return VC_CONTAINER_ERROR_BUFFER_TOO_SMALL;

      if(module->seek)
      {
         uint32_t current_time = module->timestamp / 1000;

         packet->data[0] = 'S';
         packet->data[1] = (uint8_t)((current_time >> 24) & 0xFF);
         packet->data[2] = (uint8_t)((current_time >> 16) & 0xFF);
         packet->data[3] = (uint8_t)((current_time >>  8) & 0xFF);
         packet->data[4] = (uint8_t)((current_time      ) & 0xFF);
         module->seek = 0;
      }
      else
      {
         packet->data[0] = 'P';
         packet->data[1] = 0;
         packet->data[2] = 0;
         packet->data[3] = 0;
         packet->data[4] = 40;
         module->timestamp += 40 * 1000;
      }
   }

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T qsynth_reader_seek( VC_CONTAINER_T *p_ctx,
                                              int64_t *offset,
                                              VC_CONTAINER_SEEK_MODE_T mode,
                                              VC_CONTAINER_SEEK_FLAGS_T flags)
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   VC_CONTAINER_PARAM_UNUSED(flags);

   if (mode != VC_CONTAINER_SEEK_MODE_TIME)
      return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;

   if(*offset < 0)
      *offset = 0;
   else if(*offset > p_ctx->duration)
      *offset = p_ctx->duration;

   module->timestamp = *offset;
   module->seek = 1;

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T qsynth_reader_close( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = p_ctx->priv->module;
   QSYNTH_SEGMENT_T *seg = module->seg;
   for(; p_ctx->tracks_num > 0; p_ctx->tracks_num--)
      vc_container_free_track(p_ctx, p_ctx->tracks[p_ctx->tracks_num-1]);
   while(seg != NULL)
   {
      QSYNTH_SEGMENT_T *next = seg->next;
      free(seg);
      seg = next;
   }
   free(module);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T qsynth_reader_open( VC_CONTAINER_T *p_ctx )
{
   VC_CONTAINER_MODULE_T *module = 0;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   uint8_t header[HEADER_LENGTH];

   /* Check the file header */
   if((PEEK_BYTES(p_ctx, header, HEADER_LENGTH) != HEADER_LENGTH) ||
      qsynth_read_header(header, 0, 0, 0, 0) != VC_CONTAINER_SUCCESS)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   /* Allocate our context */
   module = malloc(sizeof(*module));
   if(!module) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }
   memset(module, 0, sizeof(*module));
   p_ctx->priv->module = module;
   p_ctx->tracks_num = 1;
   p_ctx->tracks = &module->track;
   p_ctx->tracks[0] = vc_container_allocate_track(p_ctx, 0);
   if(!p_ctx->tracks[0]) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }
   p_ctx->tracks[0]->format->es_type = VC_CONTAINER_ES_TYPE_AUDIO;
   p_ctx->tracks[0]->format->codec = VC_CONTAINER_CODEC_MIDI;
   p_ctx->tracks[0]->is_enabled = true;

   if((status = qsynth_get_duration(p_ctx)) != VC_CONTAINER_SUCCESS) goto error;

   LOG_DEBUG(p_ctx, "using qsynth reader");

   p_ctx->capabilities = VC_CONTAINER_CAPS_CAN_SEEK;

   p_ctx->priv->pf_close = qsynth_reader_close;
   p_ctx->priv->pf_read = qsynth_reader_read;
   p_ctx->priv->pf_seek = qsynth_reader_seek;
   return VC_CONTAINER_SUCCESS;

 error:
   LOG_DEBUG(p_ctx, "qsynth: error opening stream (%i)", status);
   if(module) qsynth_reader_close(p_ctx);
   return status;
}

/********************************************************************************
 Entrypoint function
********************************************************************************/

#if !defined(ENABLE_CONTAINERS_STANDALONE) && defined(__HIGHC__)
# pragma weak reader_open qsynth_reader_open
#endif
