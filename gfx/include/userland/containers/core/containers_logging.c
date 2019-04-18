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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "containers/containers.h"
#include "containers/core/containers_private.h"
#include "containers/core/containers_logging.h"

#ifndef ENABLE_CONTAINERS_STANDALONE
# include "vcos.h"
#endif

#ifdef __ANDROID__
#define LOG_TAG "ContainersCore"
#include <cutils/log.h>
#endif

/* Default verbosity that will be inherited by containers */
static uint32_t default_verbosity_mask = VC_CONTAINER_LOG_ALL;

/* By default log everything that's not associated with a container context */
static uint32_t verbosity_mask = VC_CONTAINER_LOG_ALL;

void vc_container_log_set_default_verbosity(uint32_t mask)
{
   default_verbosity_mask = mask;
}

uint32_t vc_container_log_get_default_verbosity(void)
{
   return default_verbosity_mask;
}

void vc_container_log_set_verbosity(VC_CONTAINER_T *ctx, uint32_t mask)
{
   if(!ctx) verbosity_mask = mask;
   else ctx->priv->verbosity = mask;
}

void vc_container_log(VC_CONTAINER_T *ctx, VC_CONTAINER_LOG_TYPE_T type, const char *format, ...)
{
   uint32_t verbosity = ctx ? ctx->priv->verbosity : verbosity_mask;
   va_list args;

   // Optimise out the call to vc_container_log_vargs etc. when it won't do anything.
   if(!(type & verbosity)) return;

   va_start( args, format );
   vc_container_log_vargs(ctx, type, format, args);
   va_end( args );
}

void vc_container_log_vargs(VC_CONTAINER_T *ctx, VC_CONTAINER_LOG_TYPE_T type, const char *format, va_list args)
{
   uint32_t verbosity = ctx ? ctx->priv->verbosity : verbosity_mask;

   // If the verbosity is such that the type doesn't need logging quit now.
   if(!(type & verbosity)) return;

#ifdef __ANDROID__
   {
      // Default to Android's "verbose" level (doesn't usually come out)
      android_LogPriority logLevel = ANDROID_LOG_VERBOSE;

      // Where type suggest a higher level is required update logLevel.
      // (Usually type contains only 1 bit as set by the LOG_DEBUG, LOG_ERROR or LOG_INFO macros)
      if (type & VC_CONTAINER_LOG_ERROR)
         logLevel = ANDROID_LOG_ERROR;
      else if (type & VC_CONTAINER_LOG_INFO)
         logLevel = ANDROID_LOG_INFO;
      else if (type & VC_CONTAINER_LOG_DEBUG)
         logLevel = ANDROID_LOG_DEBUG;

      // Actually put the message out.
      LOG_PRI_VA(logLevel, LOG_TAG, format, args);
   }
#else
#ifndef ENABLE_CONTAINERS_STANDALONE
   vcos_vlog(format, args);
#else
   vprintf(format, args); printf("\n");
   fflush(0);
#endif
#endif
}
