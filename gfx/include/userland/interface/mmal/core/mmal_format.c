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

#include "mmal_types.h"
#include "mmal_format.h"
#include "util/mmal_util_rational.h"

#define MMAL_ES_FORMAT_MAGIC MMAL_FOURCC('m','a','g','f')
#define EXTRADATA_SIZE_DEFAULT 32
#define EXTRADATA_SIZE_MAX (10*1024)

typedef struct MMAL_ES_FORMAT_PRIVATE_T
{
   MMAL_ES_FORMAT_T format;
   MMAL_ES_SPECIFIC_FORMAT_T es;

   uint32_t magic;

   unsigned int extradata_size;
   uint8_t *extradata;

   uint8_t buffer[EXTRADATA_SIZE_DEFAULT];

} MMAL_ES_FORMAT_PRIVATE_T;

/** Allocate a format structure */
MMAL_ES_FORMAT_T *mmal_format_alloc(void)
{
   MMAL_ES_FORMAT_PRIVATE_T *private;

   private = vcos_calloc(1, sizeof(*private), "mmal format");
   if(!private) return 0;
   memset(private, 0, sizeof(*private));

   private->magic = MMAL_ES_FORMAT_MAGIC;
   private->format.es = (void *)&private->es;
   private->extradata_size = EXTRADATA_SIZE_DEFAULT;

   return &private->format;
}

/** Free a format structure */
void mmal_format_free(MMAL_ES_FORMAT_T *format)
{
   MMAL_ES_FORMAT_PRIVATE_T *private = (MMAL_ES_FORMAT_PRIVATE_T *)format;
   vcos_assert(private->magic == MMAL_ES_FORMAT_MAGIC);
   if(private->extradata) vcos_free(private->extradata);
   vcos_free(private);
}

/** Copy a format structure */
void mmal_format_copy(MMAL_ES_FORMAT_T *fmt_dst, MMAL_ES_FORMAT_T *fmt_src)
{
   void *backup = fmt_dst->es;
   *fmt_dst->es = *fmt_src->es;
   *fmt_dst = *fmt_src;
   fmt_dst->es = backup;
   fmt_dst->extradata = 0;
   fmt_dst->extradata_size = 0;
}

/** Full copy of a format structure (including extradata) */
MMAL_STATUS_T mmal_format_full_copy(MMAL_ES_FORMAT_T *fmt_dst, MMAL_ES_FORMAT_T *fmt_src)
{
   mmal_format_copy(fmt_dst, fmt_src);

   if (fmt_src->extradata_size)
   {
      MMAL_STATUS_T status = mmal_format_extradata_alloc(fmt_dst, fmt_src->extradata_size);
      if (status != MMAL_SUCCESS)
         return status;
      fmt_dst->extradata_size = fmt_src->extradata_size;
      memcpy(fmt_dst->extradata, fmt_src->extradata, fmt_src->extradata_size);
   }
   return MMAL_SUCCESS;
}

/** Compare 2 format structures */
uint32_t mmal_format_compare(MMAL_ES_FORMAT_T *fmt1, MMAL_ES_FORMAT_T *fmt2)
{
   MMAL_VIDEO_FORMAT_T *video1, *video2;
   uint32_t result = 0;

   if (fmt1->type != fmt2->type)
      return MMAL_ES_FORMAT_COMPARE_FLAG_TYPE;

   if (fmt1->encoding != fmt2->encoding)
      result |= MMAL_ES_FORMAT_COMPARE_FLAG_ENCODING;
   if (fmt1->bitrate != fmt2->bitrate)
      result |= MMAL_ES_FORMAT_COMPARE_FLAG_BITRATE;
   if (fmt1->flags != fmt2->flags)
      result |= MMAL_ES_FORMAT_COMPARE_FLAG_FLAGS;
   if (fmt1->extradata_size != fmt2->extradata_size ||
       (fmt1->extradata_size && (!fmt1->extradata || !fmt2->extradata)) ||
       memcmp(fmt1->extradata, fmt2->extradata, fmt1->extradata_size))
      result |= MMAL_ES_FORMAT_COMPARE_FLAG_EXTRADATA;

   /* Compare the ES specific information */
   switch (fmt1->type)
   {
   case MMAL_ES_TYPE_VIDEO:
      video1 = &fmt1->es->video;
      video2 = &fmt2->es->video;
      if (video1->width != video2->width || video1->height != video2->height)
         result |= MMAL_ES_FORMAT_COMPARE_FLAG_VIDEO_RESOLUTION;
      if (memcmp(&video1->crop, &video2->crop, sizeof(video1->crop)))
         result |= MMAL_ES_FORMAT_COMPARE_FLAG_VIDEO_CROPPING;
      if (!mmal_rational_equal(video1->par, video2->par))
         result |= MMAL_ES_FORMAT_COMPARE_FLAG_VIDEO_ASPECT_RATIO;
      if (!mmal_rational_equal(video1->frame_rate, video2->frame_rate))
         result |= MMAL_ES_FORMAT_COMPARE_FLAG_VIDEO_FRAME_RATE;
      if (video1->color_space != video2->color_space)
         result |= MMAL_ES_FORMAT_COMPARE_FLAG_VIDEO_COLOR_SPACE;
      /* coverity[overrun-buffer-arg] We're comparing the rest of the video format structure */
      if (memcmp(((char*)&video1->color_space) + sizeof(video1->color_space),
                 ((char*)&video2->color_space) + sizeof(video2->color_space),
                 sizeof(*video1) - offsetof(MMAL_VIDEO_FORMAT_T, color_space) - sizeof(video1->color_space)))
         result |= MMAL_ES_FORMAT_COMPARE_FLAG_ES_OTHER;
      break;
   case MMAL_ES_TYPE_AUDIO:
      if (memcmp(fmt1->es, fmt2->es, sizeof(MMAL_AUDIO_FORMAT_T)))
         result |= MMAL_ES_FORMAT_COMPARE_FLAG_ES_OTHER;
      break;
   case MMAL_ES_TYPE_SUBPICTURE:
      if (memcmp(fmt1->es, fmt2->es, sizeof(MMAL_SUBPICTURE_FORMAT_T)))
         result |= MMAL_ES_FORMAT_COMPARE_FLAG_ES_OTHER;
      break;
   default:
      break;
   }

   return result;
}

/** */
MMAL_STATUS_T mmal_format_extradata_alloc(MMAL_ES_FORMAT_T *format, unsigned int size)
{
   MMAL_ES_FORMAT_PRIVATE_T *private = (MMAL_ES_FORMAT_PRIVATE_T *)format;

   /* Sanity check the size requested */
   if(size > EXTRADATA_SIZE_MAX)
      return MMAL_EINVAL;

   /* Allocate memory if needed */
   if(private->extradata_size < size)
   {
      if(private->extradata) vcos_free(private->extradata);
      private->extradata = vcos_calloc(1, size, "mmal format extradata");
      if(!private->extradata)
         return MMAL_ENOMEM;
      private->extradata_size = size;
   }

   /* Set the fields in the actual format structure */
   if(private->extradata) private->format.extradata = private->extradata;
   else private->format.extradata = private->buffer;

   return MMAL_SUCCESS;
}
