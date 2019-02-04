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
#include <stdio.h>

#include "containers/core/containers_private.h"
#include "containers/core/containers_loader.h"

#if !defined(ENABLE_CONTAINERS_STANDALONE)
   #include "vcos_dlfcn.h"
   #define DL_SUFFIX VCOS_SO_EXT
   #ifndef DL_PATH_PREFIX
      #define DL_PATH_PREFIX ""
   #endif
#endif

/******************************************************************************
Type definitions.
******************************************************************************/

typedef VC_CONTAINER_STATUS_T (*VC_CONTAINER_READER_OPEN_FUNC_T)(VC_CONTAINER_T *);
typedef VC_CONTAINER_STATUS_T (*VC_CONTAINER_WRITER_OPEN_FUNC_T)(VC_CONTAINER_T *);

/******************************************************************************
Prototypes for local functions
******************************************************************************/

static void reset_context(VC_CONTAINER_T *p_ctx);
static VC_CONTAINER_READER_OPEN_FUNC_T load_library(void **handle, const char *name, const char *ext, int read);
static void unload_library(void *handle);
static VC_CONTAINER_READER_OPEN_FUNC_T load_reader(void **handle, const char *name);
static VC_CONTAINER_READER_OPEN_FUNC_T load_writer(void **handle, const char *name);
static VC_CONTAINER_READER_OPEN_FUNC_T load_metadata_reader(void **handle, const char *name);
static const char* container_for_fileext(const char *fileext);

/********************************************************************************
 List of supported containers
 ********************************************************************************/

static const char *readers[] =
{"mp4", "asf", "avi", "mkv", "wav", "flv", "simple", "rawvideo", "mpga", "ps", "rtp", "rtsp", "rcv", "rv9", "qsynth", "binary", 0};
static const char *writers[] =
{"mp4", "asf", "avi", "binary", "simple", "rawvideo", 0};
static const char *metadata_readers[] =
{"id3", 0};

#if defined(ENABLE_CONTAINERS_STANDALONE)
VC_CONTAINER_STATUS_T asf_reader_open( VC_CONTAINER_T * );
VC_CONTAINER_STATUS_T avi_reader_open( VC_CONTAINER_T * );
VC_CONTAINER_STATUS_T avi_writer_open( VC_CONTAINER_T * );
VC_CONTAINER_STATUS_T mp4_reader_open( VC_CONTAINER_T * );
VC_CONTAINER_STATUS_T mp4_writer_open( VC_CONTAINER_T * );
VC_CONTAINER_STATUS_T mpga_reader_open( VC_CONTAINER_T * );
VC_CONTAINER_STATUS_T mkv_reader_open( VC_CONTAINER_T * );
VC_CONTAINER_STATUS_T wav_reader_open( VC_CONTAINER_T * );
VC_CONTAINER_STATUS_T flv_reader_open( VC_CONTAINER_T * );
VC_CONTAINER_STATUS_T ps_reader_open( VC_CONTAINER_T * );
VC_CONTAINER_STATUS_T rtp_reader_open( VC_CONTAINER_T * );
VC_CONTAINER_STATUS_T rtsp_reader_open( VC_CONTAINER_T * );
VC_CONTAINER_STATUS_T binary_reader_open( VC_CONTAINER_T * );
VC_CONTAINER_STATUS_T binary_writer_open( VC_CONTAINER_T * );
VC_CONTAINER_STATUS_T rcv_reader_open( VC_CONTAINER_T * );
VC_CONTAINER_STATUS_T rv9_reader_open( VC_CONTAINER_T * );
VC_CONTAINER_STATUS_T qsynth_reader_open( VC_CONTAINER_T * );
VC_CONTAINER_STATUS_T simple_reader_open( VC_CONTAINER_T * );
VC_CONTAINER_STATUS_T simple_writer_open( VC_CONTAINER_T * );
VC_CONTAINER_STATUS_T rawvideo_reader_open( VC_CONTAINER_T * );
VC_CONTAINER_STATUS_T rawvideo_writer_open( VC_CONTAINER_T * );

VC_CONTAINER_STATUS_T id3_metadata_reader_open( VC_CONTAINER_T * );

static struct
{
   const char *name;
   VC_CONTAINER_READER_OPEN_FUNC_T func;
} reader_entry_points[] =
{
   {"asf", &asf_reader_open},
   {"avi", &avi_reader_open},
   {"mpga", &mpga_reader_open},
   {"mkv", &mkv_reader_open},
   {"wav", &wav_reader_open},
   {"mp4",  &mp4_reader_open},
   {"flv",  &flv_reader_open},
   {"ps",  &ps_reader_open},
   {"binary",  &binary_reader_open},
   {"rtp",  &rtp_reader_open},
   {"rtsp", &rtsp_reader_open},
   {"rcv", &rcv_reader_open},
   {"rv9", &rv9_reader_open},
   {"qsynth", &qsynth_reader_open},
   {"simple", &simple_reader_open},
   {"rawvideo", &rawvideo_reader_open},
   {0, 0}
};

static struct
{
   const char *name;
   VC_CONTAINER_READER_OPEN_FUNC_T func;
} metadata_reader_entry_points[] =
{
   {"id3", &id3_metadata_reader_open},
   {0, 0}
};

static struct
{
   const char *name;
   VC_CONTAINER_WRITER_OPEN_FUNC_T func;
} writer_entry_points[] =
{
   {"avi", &avi_writer_open},
   {"mp4", &mp4_writer_open},
   {"binary", &binary_writer_open},
   {"simple", &simple_writer_open},
   {"rawvideo", &rawvideo_writer_open},
   {0, 0}
};
#endif /* defined(ENABLE_CONTAINERS_STANDALONE) */

/** Table describing the mapping between file extensions and container name.
    This is only used as optimisation to decide which container to try first.
    Entries where the file extension and container have the same name can be omitted. */
static const struct {
   const char *extension;
   const char *container;
} extension_container_mapping[] =
{
   { "wma",  "asf" },
   { "wmv",  "asf" },
   { "mov",  "mp4" },
   { "3gp",  "mp4" },
   { "mp2",  "mpga" },
   { "mp3",  "mpga" },
   { "webm", "mkv" },
   { "mid",  "qsynth" },
   { "mld",  "qsynth" },
   { "mmf",  "qsynth" },
   { 0, 0 }
};

/********************************************************************************
 Public functions
 ********************************************************************************/
VC_CONTAINER_STATUS_T vc_container_load_reader(VC_CONTAINER_T *p_ctx, const char *fileext)
{
   const char *name;
   void *handle = NULL;
   VC_CONTAINER_READER_OPEN_FUNC_T func;
   VC_CONTAINER_STATUS_T status;
   unsigned int i;
   int64_t offset;
   
   vc_container_assert(p_ctx && !p_ctx->priv->module_handle);

   /* FIXME: the missing part here is code that reads a configuration or
      searches the filesystem for container libraries. Instead, we currently
      rely on static arrays i.e. 'readers', 'writers', etc. */

   /* Before trying proper container readers, iterate through metadata 
      readers to parse tags concatenated to start/end of stream */
   for(i = 0; metadata_readers[i]; i++)
   {
      if ((func = load_metadata_reader(&handle, metadata_readers[i])) != NULL)
      {
         status = (*func)(p_ctx);
         if(!status && p_ctx->priv->pf_close) p_ctx->priv->pf_close(p_ctx);
         reset_context(p_ctx);
         unload_library(handle);
         if(status == VC_CONTAINER_SUCCESS) break;
         if (status != VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED) goto error;
      }
   }

   /* Store the current position, in case any containers don't leave the stream
      at the start, and the IO layer can cope with the seek */
   offset = p_ctx->priv->io->offset;

   /* Now move to containers, try to find a readers using the file extension to name 
      mapping first */
   if (fileext && (name = container_for_fileext(fileext)) != NULL && (func = load_reader(&handle, name)) != NULL)
   {
      status = (*func)(p_ctx);
      if(status == VC_CONTAINER_SUCCESS) goto success;
      unload_library(handle);
      if (status != VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED) goto error;
   }

   /* If there was no suitable mapping, iterate through all readers. */
   for(i = 0; readers[i]; i++)
   {
      if ((func = load_reader(&handle, readers[i])) != NULL)
      {
         if(vc_container_io_seek(p_ctx->priv->io, offset) != VC_CONTAINER_SUCCESS)
         {
            unload_library(handle);
            goto error;
         }

         status = (*func)(p_ctx);
         if(status == VC_CONTAINER_SUCCESS) goto success;
         reset_context(p_ctx);
         unload_library(handle);
         if (status != VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED) goto error;
      }
   }

 error:
   return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

 success:
   p_ctx->priv->module_handle = handle;
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_load_writer(VC_CONTAINER_T *p_ctx, const char *fileext)
{
   const char *name;
   void *handle = NULL;
   VC_CONTAINER_WRITER_OPEN_FUNC_T func;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_FAILED;
   unsigned int i;
   
   vc_container_assert(p_ctx && !p_ctx->priv->module_handle);
     
   /* Do we have a container mapping for this file extension? */
   if ((name = container_for_fileext(fileext)) != NULL && (func = load_writer(&handle, name)) != NULL)
   {
      status = (*func)(p_ctx);
      if(status == VC_CONTAINER_SUCCESS) goto success;
      unload_library(handle);
      if (status != VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED) goto error;
   }

   /* If there was no suitable mapping, iterate through all writers. */
   for(i = 0; writers[i]; i++)
   {
      if ((func = load_writer(&handle, writers[i])) != NULL)
      {
         status = (*func)(p_ctx);
         if(status == VC_CONTAINER_SUCCESS) goto success;
         unload_library(handle);
         if (status != VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED) goto error;
      }
   }

 error:
   return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

 success:
   p_ctx->priv->module_handle = handle;
   return status;
}

/*****************************************************************************/
void vc_container_unload(VC_CONTAINER_T *p_ctx)
{
   if (p_ctx->priv->module_handle)
   {
      unload_library(p_ctx->priv->module_handle);
      p_ctx->priv->module_handle = NULL;
   }
}

/******************************************************************************
Local Functions
******************************************************************************/
static void reset_context(VC_CONTAINER_T *p_ctx)
{
   vc_container_assert(p_ctx);
   
   p_ctx->capabilities = 0;
   p_ctx->tracks = NULL;
   p_ctx->tracks_num = 0;
   p_ctx->drm = NULL;
   p_ctx->priv->module = NULL;
   p_ctx->priv->pf_close = NULL;
   p_ctx->priv->pf_read = NULL;
   p_ctx->priv->pf_write = NULL;
   p_ctx->priv->pf_seek = NULL;
   p_ctx->priv->pf_control = NULL;
   p_ctx->priv->tmp_io = NULL;
}

/*****************************************************************************/
static VC_CONTAINER_READER_OPEN_FUNC_T load_reader(void **handle, const char *name)
{
   return load_library(handle, name, NULL, 1);
}

/*****************************************************************************/
static VC_CONTAINER_READER_OPEN_FUNC_T load_writer(void **handle, const char *name)
{
   return load_library(handle, name, NULL, 0);
}

/*****************************************************************************/
static VC_CONTAINER_READER_OPEN_FUNC_T load_metadata_reader(void **handle, const char *name)
{
   #define DL_PREFIX_METADATA "metadata_"
   return load_library(handle, name, DL_PREFIX_METADATA, 1);
}

#if !defined(ENABLE_CONTAINERS_STANDALONE)

/*****************************************************************************/
static VC_CONTAINER_READER_OPEN_FUNC_T load_library(void **handle, const char *name, const char *ext, int read)
{
   #define DL_PREFIX_RD "reader_"
   #define DL_PREFIX_WR "writer_"
   const char *entrypt_read = {"reader_open"};
   const char *entrypt_write = {"writer_open"};
   char *dl_name, *entrypt_name;
   void *dl_handle;
   VC_CONTAINER_READER_OPEN_FUNC_T func = NULL;
   unsigned dl_size, ep_size, name_len = strlen(name) + (ext ? strlen(ext) : 0);
   
   vc_container_assert(read == 0 || read == 1);
   
   dl_size = strlen(DL_PATH_PREFIX) + MAX(strlen(DL_PREFIX_RD), strlen(DL_PREFIX_WR)) + name_len + strlen(DL_SUFFIX) + 1;
   if ((dl_name = malloc(dl_size)) == NULL)
      return NULL;

   ep_size = name_len + 1 + MAX(strlen(entrypt_read), strlen(entrypt_write)) + 1;
   if ((entrypt_name = malloc(ep_size)) == NULL)
   {
      free(dl_name);
      return NULL;
   }

   snprintf(dl_name, dl_size, "%s%s%s%s%s", DL_PATH_PREFIX, read ? DL_PREFIX_RD : DL_PREFIX_WR, ext ? ext : "", name, DL_SUFFIX);
   snprintf(entrypt_name, ep_size, "%s_%s%s", name, ext ? ext : "", read ? entrypt_read : entrypt_write);
      
   if ( (dl_handle = vcos_dlopen(dl_name, VCOS_DL_NOW)) != NULL )
   {
      /* Try generic entrypoint name before the mangled, full name */
      func = (VC_CONTAINER_READER_OPEN_FUNC_T)vcos_dlsym(dl_handle, read ? entrypt_read : entrypt_write);
#if !defined(__VIDEOCORE__) /* The following would be pointless on MW/VideoCore */
      if (!func) func = (VC_CONTAINER_READER_OPEN_FUNC_T)vcos_dlsym(dl_handle, entrypt_name);
#endif
      /* Only return handle if symbol found */
      if (func)
         *handle = dl_handle;
      else
         vcos_dlclose(dl_handle);
   }
  
   free(entrypt_name);
   free(dl_name);  
   return func;
}

/*****************************************************************************/
static void unload_library(void *handle)
{
   vcos_dlclose(handle);
}

#else /* !defined(ENABLE_CONTAINERS_STANDALONE) */

/*****************************************************************************/
static VC_CONTAINER_READER_OPEN_FUNC_T load_library(void **handle, const char *name, const char *ext, int read)
{
   int i;
   VC_CONTAINER_PARAM_UNUSED(handle);
   VC_CONTAINER_PARAM_UNUSED(ext);

   if (read)
   {
      for (i = 0; reader_entry_points[i].name; i++)
         if (!strcasecmp(reader_entry_points[i].name, name))
            return reader_entry_points[i].func;
      
      for (i = 0; metadata_reader_entry_points[i].name; i++)
         if (!strcasecmp(metadata_reader_entry_points[i].name, name))
            return metadata_reader_entry_points[i].func;
   }
   else
   {
      for (i = 0; writer_entry_points[i].name; i++)
         if (!strcasecmp(writer_entry_points[i].name, name))
            return writer_entry_points[i].func;
   }

   return NULL;
}

/*****************************************************************************/
static void unload_library(void *handle)
{
   (void)handle;
}

#endif /* !defined(ENABLE_CONTAINERS_STANDALONE) */

/*****************************************************************************/
static const char* container_for_fileext(const char *fileext)
{
   int i;

   for( i = 0; fileext && extension_container_mapping[i].extension; i++ )
   {
      if (!strcasecmp( fileext, extension_container_mapping[i].extension ))
         return extension_container_mapping[i].container;
   }

   return fileext;
}
