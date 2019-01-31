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
#include "mmal_param_convert.h"
#include <stdlib.h>
#include <stdio.h>

typedef struct string_pair_t
{
   const char *string;
   int value;
} string_pair_t;

static MMAL_STATUS_T parse_enum(int *dest, string_pair_t *pairs, size_t n_pairs, const char *str)
{
   size_t i;
   for (i=0; i<n_pairs; i++)
   {
      if (vcos_strcasecmp(str, pairs[i].string) == 0)
      {
         *dest = pairs[i].value;
         return MMAL_SUCCESS;
      }
   }
   return MMAL_EINVAL;
}

MMAL_STATUS_T mmal_parse_video_size(uint32_t *w, uint32_t *h, const char *str)
{
   static struct {
      const char *name;
      uint32_t width;
      uint32_t height;
   } sizes[] = {
      { "1080p", 1920, 1080 },
      { "720p",  1280,  720 },
      { "vga",    640,  480 },
      { "wvga",   800,  480 },
      { "cif",    352,  288 },
      { "qcif",   352/2, 288/2 },
   };
   size_t i;
   for (i=0; i<vcos_countof(sizes); i++)
   {
      if (vcos_strcasecmp(str, sizes[i].name) == 0)
      {
         *w = sizes[i].width;
         *h = sizes[i].height;
         return MMAL_SUCCESS;
      }
   }
   return MMAL_EINVAL;
}

MMAL_STATUS_T mmal_parse_rational(MMAL_RATIONAL_T *dest, const char *str)
{
   MMAL_STATUS_T ret;
   char *endptr;
   long num, den = 1;
   num = strtoul(str, &endptr, 0);
   if (endptr[0] == '\0')
   {
      /* that's it */
      ret = MMAL_SUCCESS;
   }
   else if (endptr[0] == '/')
   {
      den = strtoul(endptr+1, &endptr, 0);
      if (endptr[0] == '\0')
         ret = MMAL_SUCCESS;
      else
         ret = MMAL_EINVAL;
   }
   else
   {
      ret = MMAL_EINVAL;
   }
   dest->num = num;
   dest->den = den;
   return ret;
}

MMAL_STATUS_T mmal_parse_int(int *dest, const char *str)
{
   char *endptr;
   long i = strtol(str, &endptr, 0);
   if (endptr[0] == '\0')
   {
      *dest = i;
      return MMAL_SUCCESS;
   }
   else
   {
      return MMAL_EINVAL;
   }
}

MMAL_STATUS_T mmal_parse_uint(unsigned int *dest, const char *str)
{
   char *endptr;
   unsigned long i = strtoul(str, &endptr, 0);
   if (endptr[0] == '\0')
   {
      *dest = i;
      return MMAL_SUCCESS;
   }
   else
   {
      return MMAL_EINVAL;
   }
}

MMAL_STATUS_T mmal_parse_video_codec(uint32_t *dest, const char *str)
{
   static string_pair_t video_codec_enums[] = {
      { "h264",  MMAL_ENCODING_H264 },
      { "h263",  MMAL_ENCODING_H263 },
      { "mpeg4", MMAL_ENCODING_MP4V },
      { "mpeg2", MMAL_ENCODING_MP2V },
      { "vp8",   MMAL_ENCODING_VP8 },
      { "vp7",   MMAL_ENCODING_VP7 },
      { "vp6",   MMAL_ENCODING_VP6 },
   };
   int i = 0;
   MMAL_STATUS_T ret;

   ret = parse_enum(&i, video_codec_enums, vcos_countof(video_codec_enums), str);
   *dest = i;
   return ret;
}

MMAL_STATUS_T mmal_parse_geometry(MMAL_RECT_T *dest, const char *str)
{
   MMAL_STATUS_T ret;
   uint32_t w, h, x, y;
   x = y = w = h = 0;
   /* coverity[secure_coding] */
   if (sscanf(str, "%d*%d+%d+%d", &w,&h,&x,&y) == 4 ||
       sscanf(str, "%d*%d", &w,&h) == 2)
   {
      dest->x = x;
      dest->y = y;
      dest->width = w;
      dest->height = h;
      ret = MMAL_SUCCESS;
   }
   else
   {
      ret = MMAL_EINVAL;
   }
   return ret;
}

