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
#ifndef VC_CONTAINERS_TIME_H
#define VC_CONTAINERS_TIME_H

/** \file
 * Utility functions to help with timestamping of elementary stream frames
 */

typedef struct VC_CONTAINER_TIME_T
{
   uint32_t samplerate_num;
   uint32_t samplerate_den;
   uint32_t time_base;

   uint32_t remainder;

   int64_t time;

} VC_CONTAINER_TIME_T;

/*****************************************************************************/
STATIC_INLINE void vc_container_time_init( VC_CONTAINER_TIME_T *time, uint32_t time_base )
{
   time->samplerate_num = 0;
   time->samplerate_den = 0;
   time->remainder = 0;
   time->time_base = time_base;
   time->time = VC_CONTAINER_TIME_UNKNOWN;
}

/*****************************************************************************/
STATIC_INLINE int64_t vc_container_time_get( VC_CONTAINER_TIME_T *time )
{
   if (time->time == VC_CONTAINER_TIME_UNKNOWN || !time->samplerate_num || !time->samplerate_den)
      return VC_CONTAINER_TIME_UNKNOWN;
   return time->time + time->remainder * (int64_t)time->time_base * time->samplerate_den / time->samplerate_num;
}

/*****************************************************************************/
STATIC_INLINE void vc_container_time_set_samplerate( VC_CONTAINER_TIME_T *time, uint32_t samplerate_num, uint32_t samplerate_den )
{
   if(time->samplerate_num == samplerate_num &&
      time->samplerate_den == samplerate_den)
      return;

   /* We're changing samplerate, we need to reset our remainder */
   if(time->remainder)
      time->time = vc_container_time_get( time );
   time->remainder = 0;
   time->samplerate_num = samplerate_num;
   time->samplerate_den = samplerate_den;
}

/*****************************************************************************/
STATIC_INLINE void vc_container_time_set( VC_CONTAINER_TIME_T *time, int64_t new_time )
{
   if (new_time == VC_CONTAINER_TIME_UNKNOWN)
      return;
   time->remainder = 0;
   time->time = new_time;
}

/*****************************************************************************/
STATIC_INLINE int64_t vc_container_time_add( VC_CONTAINER_TIME_T *time, uint32_t samples )
{
   uint32_t increment;

   if (time->time == VC_CONTAINER_TIME_UNKNOWN || !time->samplerate_num || !time->samplerate_den)
      return VC_CONTAINER_TIME_UNKNOWN;

   samples += time->remainder;
   increment = samples * time->samplerate_den / time->samplerate_num;
   time->time += increment * time->time_base;
   time->remainder = samples - increment * time->samplerate_num / time->samplerate_den;
   return vc_container_time_get(time);
}

#endif /* VC_CONTAINERS_TIME_H */
