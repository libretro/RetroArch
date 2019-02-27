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
#ifndef VC_CONTAINERS_LOGGING_H
#define VC_CONTAINERS_LOGGING_H

#ifdef __cplusplus
extern "C" {
#endif

/** \file containers_logging.h
 * Logging API used by container readers and writers
 */

typedef enum {
   VC_CONTAINER_LOG_ERROR  = 0x01,
   VC_CONTAINER_LOG_INFO   = 0x02,
   VC_CONTAINER_LOG_DEBUG  = 0x04,
   VC_CONTAINER_LOG_FORMAT = 0x08,
   VC_CONTAINER_LOG_ALL = 0xFF
} VC_CONTAINER_LOG_TYPE_T;

void vc_container_log(VC_CONTAINER_T *ctx, VC_CONTAINER_LOG_TYPE_T type, const char *format, ...);
void vc_container_log_vargs(VC_CONTAINER_T *ctx, VC_CONTAINER_LOG_TYPE_T type, const char *format, va_list args);
void vc_container_log_set_verbosity(VC_CONTAINER_T *ctx, uint32_t mask);
void vc_container_log_set_default_verbosity(uint32_t mask);
uint32_t vc_container_log_get_default_verbosity(void);

#define ENABLE_CONTAINER_LOG_ERROR
#define ENABLE_CONTAINER_LOG_INFO

#ifdef ENABLE_CONTAINER_LOG_DEBUG
# define LOG_DEBUG(ctx, ...) vc_container_log(ctx, VC_CONTAINER_LOG_DEBUG, __VA_ARGS__)
#else
# define LOG_DEBUG(ctx, ...) VC_CONTAINER_PARAM_UNUSED(ctx)
#endif

#ifdef ENABLE_CONTAINER_LOG_ERROR
# define LOG_ERROR(ctx, ...) vc_container_log(ctx, VC_CONTAINER_LOG_ERROR, __VA_ARGS__)
#else
# define LOG_ERROR(ctx, ...) VC_CONTAINER_PARAM_UNUSED(ctx)
#endif

#ifdef ENABLE_CONTAINER_LOG_INFO
# define LOG_INFO(ctx, ...) vc_container_log(ctx, VC_CONTAINER_LOG_INFO, __VA_ARGS__)
#else
# define LOG_INFO(ctx, ...) VC_CONTAINER_PARAM_UNUSED(ctx)
#endif

#ifdef __cplusplus
}
#endif

#endif /* VC_CONTAINERS_LOGGING_H */
