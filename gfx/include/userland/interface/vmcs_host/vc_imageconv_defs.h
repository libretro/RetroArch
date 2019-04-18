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
#ifndef IMAGECONV_DEFS_H
#define IMAGECONV_DEFS_H

/** Statistics for image conversion to foreign image types
 */
typedef struct
{
   uint32_t magic;
   uint32_t size;                   /**< Size of this structure, in bytes */
   uint32_t conversions;            /**< Total conversions so far */
   uint32_t duplicate_conversions;  /**< Duplicate conversions (same image twice) */
   uint32_t size_requests;          /**< Num calls to get_converted_size */
   uint32_t consumed_count;         /**< How many converted images were consumed */
   uint32_t failures;               /**< Failed conversions */
   uint32_t time_spent;             /**< Time spent converting, us */
   uint32_t max_vrf_delay;          /**< The max time waiting for the VRF */
   uint32_t vrf_wait_time;          /**< Total time waiting for the VRF */
   uint32_t last_mem_handle;        /**< Last mem handle converted */
   uint32_t first_image_ts;         /**< Timestamp of first image */
   uint32_t last_image_ts;          /**< Timestamp of first image */
   uint32_t max_delay;              /**< Jitter */
} IMAGECONV_STATS_T;

#define IMAGECONV_STATS_MAGIC 0x494D454C
#endif
