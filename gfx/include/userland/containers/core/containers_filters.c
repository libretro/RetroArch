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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "containers/containers.h"
#include "containers/core/containers_private.h"
#include "containers/core/containers_filters.h"

#if !defined(ENABLE_CONTAINERS_STANDALONE)
   #include "vcos_dlfcn.h"
   #define DL_SUFFIX VCOS_SO_EXT
   #ifndef DL_PATH_PREFIX
      #define DL_PATH_PREFIX ""
   #endif
#endif

typedef struct VC_CONTAINER_FILTER_PRIVATE_T
{
   /** Pointer to the container filter code and symbols */
   void *handle;
} VC_CONTAINER_FILTER_PRIVATE_T;

typedef VC_CONTAINER_STATUS_T (*VC_CONTAINER_FILTER_OPEN_FUNC_T)(VC_CONTAINER_FILTER_T*, VC_CONTAINER_FOURCC_T);

static VC_CONTAINER_FILTER_OPEN_FUNC_T load_library(void **handle, VC_CONTAINER_FOURCC_T filter, const char *name);
static void unload_library(void *handle);

static struct {
   VC_CONTAINER_FOURCC_T filter;
   const char *name;
} filter_to_name_table[] =
{
   {VC_FOURCC('d','r','m',' '), "divx"},
   {VC_FOURCC('d','r','m',' '), "hdcp2"},
   {0, NULL}
};

static VC_CONTAINER_STATUS_T vc_container_filter_load(VC_CONTAINER_FILTER_T *p_ctx,
                                                      VC_CONTAINER_FOURCC_T filter,
                                                      VC_CONTAINER_FOURCC_T type)
{
   void *handle = NULL;
   VC_CONTAINER_FILTER_OPEN_FUNC_T func;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   unsigned int i;

   for(i = 0; filter_to_name_table[i].filter; ++i)
   {
      if (filter_to_name_table[i].filter == filter)
      {
         if ((func = load_library(&handle, filter, filter_to_name_table[i].name)) != NULL)
         {
            status = (*func)(p_ctx, type);
            if(status == VC_CONTAINER_SUCCESS) break;
            unload_library(handle);
            if (status != VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED) break;
         }
      }
   }

   p_ctx->priv->handle = handle;
   return status;
}

static void vc_container_filter_unload(VC_CONTAINER_FILTER_T *p_ctx)
{
   unload_library(p_ctx->priv->handle);
   p_ctx->priv->handle = NULL;
}

/*****************************************************************************/
VC_CONTAINER_FILTER_T *vc_container_filter_open(VC_CONTAINER_FOURCC_T filter,
                                                VC_CONTAINER_FOURCC_T type,
                                                VC_CONTAINER_T *p_container,
   VC_CONTAINER_STATUS_T *p_status )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_NOT_FOUND;
   VC_CONTAINER_FILTER_T *p_ctx = 0;
   VC_CONTAINER_FILTER_PRIVATE_T *priv = 0;

   /* Allocate our context before trying out the different filter modules */
   p_ctx = malloc(sizeof(*p_ctx) + sizeof(*priv));
   if(!p_ctx) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }
   memset(p_ctx, 0, sizeof(*p_ctx) + sizeof(*priv));
   p_ctx->priv = priv = (VC_CONTAINER_FILTER_PRIVATE_T *)&p_ctx[1];
   p_ctx->container = p_container;

   status = vc_container_filter_load(p_ctx, filter, type);
   if(status != VC_CONTAINER_SUCCESS) goto error;

 end:
   if(p_status) *p_status = status;
   return p_ctx;

 error:
   if(p_ctx) free(p_ctx);
   p_ctx = 0;
   goto end;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_filter_close( VC_CONTAINER_FILTER_T *p_ctx )
{
   if (p_ctx)
   {
      if(p_ctx->pf_close) p_ctx->pf_close(p_ctx);
      if(p_ctx->priv && p_ctx->priv->handle) vc_container_filter_unload(p_ctx);
      free(p_ctx);
   }
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_filter_process( VC_CONTAINER_FILTER_T *p_ctx, VC_CONTAINER_PACKET_T *p_packet )
{
   VC_CONTAINER_STATUS_T status;
   status = p_ctx->pf_process(p_ctx, p_packet);
   return status;
}

VC_CONTAINER_STATUS_T vc_container_filter_control(VC_CONTAINER_FILTER_T *p_ctx, VC_CONTAINER_CONTROL_T operation, ... )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;
   va_list args;

   va_start( args, operation );
   if(p_ctx->pf_control)
      status = p_ctx->pf_control(p_ctx, operation, args);
   va_end( args );

   return status;
}

static VC_CONTAINER_FILTER_OPEN_FUNC_T load_library(void **handle, VC_CONTAINER_FOURCC_T filter, const char *name)
{
   VC_CONTAINER_FILTER_OPEN_FUNC_T func = NULL;
#ifdef ENABLE_CONTAINERS_STANDALONE
   VC_CONTAINER_PARAM_UNUSED(handle);
   VC_CONTAINER_PARAM_UNUSED(filter);
   VC_CONTAINER_PARAM_UNUSED(name);
#else
   char *dl_name, *entrypt_name;
   const char *entrypt_name_short = "filter_open";
   char filter_[6], *ptr;
   void *dl_handle;
   int dl_name_len;
   int entrypt_name_len;

   snprintf(filter_, sizeof(filter_), "%4.4s", (const char*)&filter);
   ptr = strchr(filter_, '\0');
   while (ptr > filter_ && isspace(*--ptr)) *ptr = '\0';
   strncat(filter_, "_", 1);

   dl_name_len = strlen(DL_PATH_PREFIX) + strlen(filter_) + strlen(name) + strlen(DL_SUFFIX) + 1;
   dl_name = malloc(dl_name_len);
   if (!dl_name) return NULL;

   entrypt_name_len = strlen(name) + 1 + strlen(filter_) + strlen(entrypt_name_short) + 1;
   entrypt_name = malloc(entrypt_name_len);
   if (!entrypt_name)
   {
      free(dl_name);
      return NULL;
   }

   snprintf(dl_name, dl_name_len, "%s%s%s%s", DL_PATH_PREFIX, filter_, name, DL_SUFFIX);
   snprintf(entrypt_name, entrypt_name_len, "%s_%s%s", name, filter_, entrypt_name_short);

   if ((dl_handle = vcos_dlopen(dl_name, VCOS_DL_NOW)) != NULL)
   {
      /* Try generic entrypoint name before the mangled, full name */
      func = (VC_CONTAINER_FILTER_OPEN_FUNC_T)vcos_dlsym(dl_handle, entrypt_name_short);
      if (!func) func = (VC_CONTAINER_FILTER_OPEN_FUNC_T)vcos_dlsym(dl_handle, entrypt_name);
      /* Only return handle if symbol found */
      if (func)
         *handle = dl_handle;
      else
         vcos_dlclose(dl_handle);
   }

   free(dl_name);
   free(entrypt_name);
#endif
   return func;
}

static void unload_library(void *handle)
{
#ifdef ENABLE_CONTAINERS_STANDALONE
   VC_CONTAINER_PARAM_UNUSED(handle);
#else
   vcos_dlclose(handle);
#endif
}
