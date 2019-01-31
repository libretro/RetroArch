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
#ifndef RAW_VIDEO_COMMON_H
#define RAW_VIDEO_COMMON_H

static struct {
   const char *id;
   VC_CONTAINER_FOURCC_T codec;
   unsigned int size_num;
   unsigned int size_den;
} table[] = {
   {"420", VC_CONTAINER_CODEC_I420, 3, 2},
   {0, 0, 0, 0}
};

STATIC_INLINE bool from_yuv4mpeg2(const char *id, VC_CONTAINER_FOURCC_T *codec,
   unsigned int *size_num, unsigned int *size_den)
{
   unsigned int i;
   for (i = 0; table[i].id; i++)
      if (!strcmp(id, table[i].id))
         break;
  if (codec) *codec = table[i].codec;
  if (size_num) *size_num = table[i].size_num;
  if (size_den) *size_den = table[i].size_den;
  return !!table[i].id;
}

STATIC_INLINE bool to_yuv4mpeg2(VC_CONTAINER_FOURCC_T codec, const char **id,
   unsigned int *size_num, unsigned int *size_den)
{
   unsigned int i;
   for (i = 0; table[i].id; i++)
      if (codec == table[i].codec)
         break;
  if (id) *id = table[i].id;
  if (size_num) *size_num = table[i].size_num;
  if (size_den) *size_den = table[i].size_den;
  return !!table[i].id;
}

#endif /* RAW_VIDEO_COMMON_H */
