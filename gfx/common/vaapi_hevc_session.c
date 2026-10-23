/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "vaapi_hevc_session.h"
#include "vaapi_vp9_session.h"
#include "vaapi_av1_session.h"
#include <stdlib.h>
#include <string.h>
typedef union vaapi_hevc_query {
 VAProfile profiles[256];
 VAEntrypoint entrypoints[256];
 VASurfaceAttrib surfaces[64];
} vaapi_hevc_query_t;
static bool vaapi_hevc_session_surfaces(vaapi_hevc_session_t *s)
{
 unsigned i,j;
 bool complete=true;
 for(i=0;i<s->slots;i++)
 {
  if(s->surfaces[i]==VA_INVALID_SURFACE){complete=false;continue;}
  for(j=0;j<i;j++)if(s->surfaces[i]==s->surfaces[j])
  {s->surfaces[i]=VA_INVALID_SURFACE;complete=false;break;}
 }
 return complete;
}

static bool vaapi_session_open(vaapi_hevc_session_t *s, VADisplay display,
      const vaapi_hevc_session_ops_t *ops, unsigned width, unsigned height,
      unsigned depth, unsigned slots, unsigned slice_mode, bool vp9, bool av1)
{
   vaapi_hevc_query_t *q;
   vaapi_hevc_session_ops_t native;
   VAConfigAttrib attrs[4];
   VASurfaceAttrib request[2];
   VAProfile profile = vp9
         ? (depth == 10 ? VAProfileVP9Profile2 : VAProfileVP9Profile0)
         : (depth == 10 ? VAProfileHEVCMain10 : VAProfileHEVCMain);
   unsigned format = depth == 10 ? VA_RT_FORMAT_YUV420_10 : VA_RT_FORMAT_YUV420;
   unsigned fourcc = depth == 10 ? VA_FOURCC_P010 : VA_FOURCC_NV12;
   bool frame = vp9 || av1;
   unsigned minimum_slots = av1 ? 9u : vp9 ? 10u : 2u;
   unsigned i, count, requests = 1, config_attrs = frame ? 1 : 2;
   unsigned width_attr = frame ? 1 : 2, height_attr = frame ? 2 : 3;
   int maximum, n;
   bool found;
   if (av1)
   {
#if VA_CHECK_VERSION(1, 8, 0)
      profile = VAProfileAV1Profile0;
#else
      return false;
#endif
   }
   if ((vp9 && av1) || !s || s->initialized || !display || !ops
         || !ops->max_profiles
         || !ops->profiles || !ops->max_entrypoints || !ops->entrypoints
         || !ops->attributes || !ops->create_config || !ops->destroy_config
         || !ops->surface_attributes || !ops->create_surfaces
         || !ops->destroy_surfaces || !ops->create_context
         || !ops->destroy_context || width < (frame ? 1u : 8u)
         || width > (av1 ? 65536u : 65535u)
         || height < (frame ? 1u : 8u)
         || height > (av1 ? 65536u : 65535u)
         || (!frame && ((width & 1) || (height & 1)))
         || (depth != 8 && depth != 10) || slots < minimum_slots || slots > 32
         || (slice_mode != VAAPI_HEVC_SLICE_AUTO
               && slice_mode != VA_DEC_SLICE_MODE_NORMAL
               && slice_mode != VA_DEC_SLICE_MODE_BASE))
      return false;
   native = *ops;
   memset(s, 0, sizeof(*s));
   s->ops         = native;
   s->display     = display;
   ops            = &s->ops;
   s->width       = width;
   s->height      = height;
   s->depth       = depth;
   s->profile     = profile;
   s->slots       = slots;
   s->slice_mode  = slice_mode;
   s->initialized = true;
   s->config      = VA_INVALID_ID;
   s->context     = VA_INVALID_ID;
   for (i = 0; i < 32; i++)
      s->surfaces[i] = VA_INVALID_SURFACE;
   s->error = VA_STATUS_ERROR_ALLOCATION_FAILED;
   q        = (vaapi_hevc_query_t *)calloc(1, sizeof(*q));
   if (!q)
      return false;
   s->error = VA_STATUS_ERROR_OPERATION_FAILED;
   maximum  = ops->max_profiles(display);
   if (maximum < 1 || maximum > 256)
      goto done;
   n        = maximum;
   s->error = ops->profiles(display, q->profiles, &n);
   if (s->error != VA_STATUS_SUCCESS || n < 0 || n > maximum)
      goto failed;
   found = false;
   for (i = 0; i < (unsigned)n; i++)
      if (q->profiles[i] == profile)
         found = true;
   if (!found)
   {
      s->error = VA_STATUS_ERROR_UNSUPPORTED_PROFILE;
      goto done;
   }
   maximum = ops->max_entrypoints(display);
   if (maximum < 1 || maximum > 256)
      goto failed;
   n        = maximum;
   s->error = ops->entrypoints(display, profile, q->entrypoints, &n);
   if (s->error != VA_STATUS_SUCCESS || n < 0 || n > maximum)
      goto failed;
   found = false;
   for (i = 0; i < (unsigned)n; i++)
      if (q->entrypoints[i] == VAEntrypointVLD)
         found = true;
   if (!found)
   {
      s->error = VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT;
      goto done;
   }
   memset(attrs, 0, sizeof(attrs));
   attrs[0].type = VAConfigAttribRTFormat;
   attrs[1].type = VAConfigAttribDecSliceMode;
   attrs[width_attr].type = VAConfigAttribMaxPictureWidth;
   attrs[height_attr].type = VAConfigAttribMaxPictureHeight;
   for (i = 0; i < 4; i++)
      attrs[i].value = VA_ATTRIB_NOT_SUPPORTED;
   s->error = ops->attributes(
         display, profile, VAEntrypointVLD, attrs, frame ? 3 : 4);
   if (s->error != VA_STATUS_SUCCESS)
      goto done;
   if (attrs[0].value == VA_ATTRIB_NOT_SUPPORTED || !(attrs[0].value & format))
   {
      s->error = VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT;
      goto done;
   }
   if (!frame && slice_mode == VAAPI_HEVC_SLICE_AUTO)
   {
      if (attrs[1].value == VA_ATTRIB_NOT_SUPPORTED
            || (attrs[1].value & VA_DEC_SLICE_MODE_NORMAL))
         slice_mode = VA_DEC_SLICE_MODE_NORMAL;
      else
         slice_mode = VA_DEC_SLICE_MODE_BASE;
      s->slice_mode = slice_mode;
   }
   if (!frame && attrs[1].value == VA_ATTRIB_NOT_SUPPORTED
         && slice_mode == VA_DEC_SLICE_MODE_NORMAL)
      config_attrs = 1;
   else if (!frame && (attrs[1].value == VA_ATTRIB_NOT_SUPPORTED
            || !(attrs[1].value & slice_mode)))
   {
      s->error = VA_STATUS_ERROR_ATTR_NOT_SUPPORTED;
      goto done;
   }
   if ((attrs[width_attr].value != VA_ATTRIB_NOT_SUPPORTED
               && width > attrs[width_attr].value)
         || (attrs[height_attr].value != VA_ATTRIB_NOT_SUPPORTED
               && height > attrs[height_attr].value))
   {
      s->error = VA_STATUS_ERROR_RESOLUTION_NOT_SUPPORTED;
      goto done;
   }
   attrs[0].value = format;
   if (!frame)
      attrs[1].value = slice_mode;
   s->error       = ops->create_config(display, profile, VAEntrypointVLD, attrs,
               (int)config_attrs, &s->config);
   if (s->error != VA_STATUS_SUCCESS || s->config == VA_INVALID_ID)
      goto failed;
   count    = 0;
   s->error = ops->surface_attributes(display, s->config, NULL, &count);
   if (s->error != VA_STATUS_SUCCESS || !count || count > 64)
      goto failed;
   memset(q, 0, sizeof(*q));
   s->error = ops->surface_attributes(display, s->config, q->surfaces, &count);
   if (s->error != VA_STATUS_SUCCESS || count > 64)
      goto failed;
   memset(request, 0, sizeof(request));
   found = false;
   for (i = 0; i < count; i++)
   {
      const VASurfaceAttrib *a = &q->surfaces[i];
      if (!(a->flags & VA_SURFACE_ATTRIB_SETTABLE)
            || a->value.type != VAGenericValueTypeInteger)
         continue;
      if (a->type == VASurfaceAttribPixelFormat
            && (unsigned)a->value.value.i == fourcc)
         found = true;
      if (a->type == VASurfaceAttribUsageHint
            && (a->value.value.i & VA_SURFACE_ATTRIB_USAGE_HINT_DECODER))
      {
         request[1].type          = VASurfaceAttribUsageHint;
         request[1].flags         = VA_SURFACE_ATTRIB_SETTABLE;
         request[1].value.type    = VAGenericValueTypeInteger;
         request[1].value.value.i = VA_SURFACE_ATTRIB_USAGE_HINT_DECODER;
         requests                 = 2;
      }
   }
   if (!found)
   {
      s->error = VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT;
      goto done;
   }
   request[0].type          = VASurfaceAttribPixelFormat;
   request[0].flags         = VA_SURFACE_ATTRIB_SETTABLE;
   request[0].value.type    = VAGenericValueTypeInteger;
   request[0].value.value.i = (int)fourcc;
   s->error                 = ops->create_surfaces(
         display, format, width, height, s->surfaces, slots, request, requests);
   found = vaapi_hevc_session_surfaces(s);
   if (s->error != VA_STATUS_SUCCESS)
      goto done;
   if (!found)
      goto failed;
   s->error = ops->create_context(display, s->config, (int)width, (int)height,
         VA_PROGRESSIVE, s->surfaces, (int)slots, &s->context);
   if (s->error != VA_STATUS_SUCCESS || s->context == VA_INVALID_ID)
      goto failed;
   s->ready = true;
   goto done;
failed:
   if (s->error == VA_STATUS_SUCCESS)
      s->error = VA_STATUS_ERROR_OPERATION_FAILED;
done:
   free(q);
   return s->ready;
}
bool vaapi_hevc_session_open(vaapi_hevc_session_t *s, VADisplay display,
      const vaapi_hevc_session_ops_t *ops, unsigned width, unsigned height,
      unsigned depth, unsigned slots, unsigned slice_mode)
{
   return vaapi_session_open(s, display, ops, width, height, depth, slots,
         slice_mode, false, false);
}

bool vaapi_vp9_session_open(vaapi_vp9_session_t *s, VADisplay display,
      const vaapi_vp9_session_ops_t *ops, unsigned width, unsigned height,
      unsigned depth, unsigned slots)
{
   return vaapi_session_open(s, display, ops, width, height, depth, slots,
         0, true, false);
}

bool vaapi_av1_session_open(vaapi_av1_session_t *s, VADisplay display,
      const vaapi_av1_session_ops_t *ops, unsigned width, unsigned height,
      unsigned depth, unsigned slots)
{
   return vaapi_session_open(s, display, ops, width, height, depth, slots,
         0, false, true);
}

bool vaapi_hevc_session_close(vaapi_hevc_session_t *s)
{
 unsigned i;
 bool failed=false;
 VAStatus result;
 if(!s || !s->initialized)return true;
 s->closing=true;s->ready=false;
 if(s->context!=VA_INVALID_ID)
 {
  result=s->ops.destroy_context(s->display,s->context);
  if(result!=VA_STATUS_SUCCESS){s->error=result;return false;}
  s->context=VA_INVALID_ID;
 }
 for(i=0;i<s->slots;i++)if(s->surfaces[i]!=VA_INVALID_SURFACE)
 {
  VASurfaceID surface=s->surfaces[i];
  result=s->ops.destroy_surfaces(s->display,&surface,1);
  if(result!=VA_STATUS_SUCCESS){s->error=result;failed=true;}
  else s->surfaces[i]=VA_INVALID_SURFACE;
 }
 if(failed)return false;
 if(s->config!=VA_INVALID_ID)
 {
  result=s->ops.destroy_config(s->display,s->config);
  if(result!=VA_STATUS_SUCCESS){s->error=result;return false;}
  s->config=VA_INVALID_ID;
 }
 memset(s,0,sizeof(*s));return true;
}
bool vaapi_hevc_session_terminated(vaapi_hevc_session_t *s,VADisplay display)
{
 if(!s || !s->initialized || !s->closing || !display || s->display!=display)return false;
 memset(s,0,sizeof(*s));return true;
}
