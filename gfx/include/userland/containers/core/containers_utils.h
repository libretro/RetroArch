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
#ifndef VC_CONTAINERS_UTILS_H
#define VC_CONTAINERS_UTILS_H

#include "containers/containers.h"
#include "containers/containers_codecs.h"
#include "containers/core/containers_waveformat.h"

/*****************************************************************************
 * Type definitions
 *****************************************************************************/

/** Definition of the Global Unique Identifier type as used by some containers */
typedef struct GUID_T
{
   uint32_t word0;
   uint16_t short0;
   uint16_t short1;
   uint8_t bytes[8];

} GUID_T;

VC_CONTAINER_ES_FORMAT_T *vc_container_format_create(unsigned int extradata_size);
void vc_container_format_delete(VC_CONTAINER_ES_FORMAT_T *);
VC_CONTAINER_STATUS_T vc_container_format_extradata_alloc(
   VC_CONTAINER_ES_FORMAT_T *format, unsigned int size);
VC_CONTAINER_STATUS_T vc_container_format_copy( VC_CONTAINER_ES_FORMAT_T *p_out,
                                                VC_CONTAINER_ES_FORMAT_T *p_in,
                                                unsigned int extra_buffer_size );
int utf8_from_charset(const char *charset, char *out, unsigned int out_size,
                      const void *in, unsigned int in_size);
const char *vc_container_metadata_id_to_string(VC_CONTAINER_METADATA_KEY_T key);

unsigned int vc_container_es_format_to_waveformatex(VC_CONTAINER_ES_FORMAT_T *format,
                                                    uint8_t *buffer, unsigned int buffer_size);
unsigned int vc_container_es_format_to_bitmapinfoheader(VC_CONTAINER_ES_FORMAT_T *format,
                                                        uint8_t *buffer, unsigned int buffer_size);
VC_CONTAINER_STATUS_T vc_container_waveformatex_to_es_format(uint8_t *p,
   unsigned int buffer_size, unsigned int *extra_offset, unsigned int *extra_size,
   VC_CONTAINER_ES_FORMAT_T *format);
VC_CONTAINER_STATUS_T vc_container_bitmapinfoheader_to_es_format(uint8_t *p,
   unsigned int buffer_size, unsigned int *extra_offset, unsigned int *extra_size, 
   VC_CONTAINER_ES_FORMAT_T *format);

/** Find the greatest common denominator of 2 numbers.
 *
 * @param a first number
 * @param b second number
 *
 * @return greatest common denominator of a and b
 */
int64_t vc_container_maths_gcd(int64_t a, int64_t b);

/** Reduce a rational number to it's simplest form.
 *
 * @param num Pointer to the numerator of the rational number to simplify
 * @param den Pointer to the denominator of the rational number to simplify
 */
void vc_container_maths_rational_simplify(uint32_t *num, uint32_t *den);

#endif /* VC_CONTAINERS_UTILS_H */
