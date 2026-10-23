/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "record_mediacodec_surface.h"
#include <stdlib.h>
#include <string.h>

struct record_mediacodec_surface
{
   int64_t last_pts;
   record_mediacodec_surface_ops_t ops;
   EGLDisplay display, previous_display;
   EGLContext context, previous_context;
   EGLSurface surface, previous_draw, previous_read;
   bool active, posted, failed;
};

static bool record_mcs_extension(const char *list, const char *name)
{
   const char *at = list;
   size_t length = strlen(name);
   if (!list)
      return false;
   while ((at = strstr(at, name)) != NULL)
   {
      if ((at == list || at[-1] == ' ') && (!at[length] || at[length] == ' '))
         return true;
      at += length;
   }
   return false;
}

bool record_mediacodec_surface_free(record_mediacodec_surface_t *s)
{
   if (!s)
      return true;
   if (s->active)
      return false;
   s->failed = true;
   if (s->context != EGL_NO_CONTEXT)
   {
      if (!s->ops.destroy_context(s->display, s->context))
         return false;
      s->context = EGL_NO_CONTEXT;
   }
   if (s->surface != EGL_NO_SURFACE)
   {
      if (!s->ops.destroy_surface(s->display, s->surface))
         return false;
      s->surface = EGL_NO_SURFACE;
   }
   free(s);
   return true;
}

bool record_mediacodec_surface_new(const record_mediacodec_surface_ops_t *ops,
      EGLDisplay display, EGLConfig config, EGLContext share,
      EGLNativeWindowType window, unsigned width, unsigned height, bool pq,
      record_mediacodec_surface_t **output)
{
   static const EGLint tokens[]
         = {EGL_RECORDABLE_ANDROID, EGL_SURFACE_TYPE, EGL_RENDERABLE_TYPE,
               EGL_RED_SIZE, EGL_GREEN_SIZE, EGL_BLUE_SIZE, EGL_ALPHA_SIZE};
   EGLint values[7];
   EGLint context_attrs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
   EGLint surface_attrs[] = {EGL_NONE, EGL_NONE, EGL_NONE};
   const char *extensions;
   const char *version;
   bool core15;
   record_mediacodec_surface_t *s;
   EGLint size;
   unsigned i;
   if (!output)
      return false;
   *output = NULL;
   if (!ops || !ops->query_string || !ops->config_attrib || !ops->create_context
         || !ops->create_surface || !ops->destroy_context
         || !ops->destroy_surface || !ops->query_surface || !ops->query_api
         || !ops->current_context || !ops->current_display
         || !ops->current_surface || !ops->make_current
         || !ops->presentation_time || !ops->swap || display == EGL_NO_DISPLAY
         || !config || !window || !width || !height || width > 16384
         || height > 16384 || ops->query_api() != EGL_OPENGL_ES_API)
      return false;
   extensions = ops->query_string(display, EGL_EXTENSIONS);
   version = ops->query_string(display, EGL_VERSION);
   core15 = version && !strncmp(version, "1.5", 3)
         && (!version[3] || version[3] == ' ');
   if (!record_mcs_extension(extensions, "EGL_ANDROID_recordable")
         || !record_mcs_extension(extensions, "EGL_ANDROID_presentation_time")
         || (!core15
               && !record_mcs_extension(extensions, "EGL_KHR_create_context"))
         || (pq
               && ((!core15
                         && !record_mcs_extension(
                               extensions, "EGL_KHR_gl_colorspace"))
                     || !record_mcs_extension(
                           extensions, "EGL_EXT_gl_colorspace_bt2020_pq"))))
      return false;
   for (i = 0; i < 7; i++)
      if (!ops->config_attrib(display, config, tokens[i], &values[i]))
         return false;
   if (values[0] != EGL_TRUE || !(values[1] & EGL_WINDOW_BIT)
         || !(values[2] & EGL_OPENGL_ES3_BIT_KHR) || values[3] != (pq ? 10 : 8)
         || values[4] != values[3] || values[5] != values[3]
         || (values[6] != 0 && values[6] != (pq ? 2 : 8)))
      return false;
   s = (record_mediacodec_surface_t *)calloc(1, sizeof(*s));
   if (!s)
      return false;
   *output = s;
   s->ops = *ops;
   s->display = display;
   s->context = ops->create_context(display, config, share, context_attrs);
   if (s->context == EGL_NO_CONTEXT)
      goto failed;
   if (pq)
   {
      surface_attrs[0] = EGL_GL_COLORSPACE_KHR;
      surface_attrs[1] = EGL_GL_COLORSPACE_BT2020_PQ_EXT;
   }
   s->surface = ops->create_surface(display, config, window, surface_attrs);
   if (s->surface == EGL_NO_SURFACE)
      goto failed;
   if (!ops->query_surface(display, s->surface, EGL_WIDTH, &size)
         || size != (EGLint)width
         || !ops->query_surface(display, s->surface, EGL_HEIGHT, &size)
         || size != (EGLint)height)
      goto failed;
   return true;
failed:
   if (record_mediacodec_surface_free(s))
      *output = NULL;
   return false;
}

static bool record_mcs_begin(record_mediacodec_surface_t *s, bool retire)
{
   if (!s || s->context == EGL_NO_CONTEXT || s->surface == EGL_NO_SURFACE
         || (!retire && s->failed) || s->ops.query_api() != EGL_OPENGL_ES_API)
      return false;
   if (retire)
      s->failed = true;
   if (s->active)
      return retire && s->ops.current_context() == s->context
            && s->ops.current_display() == s->display;
   if (s->ops.current_context() == s->context)
      return false;
   s->previous_context = s->ops.current_context();
   s->previous_display = s->ops.current_display();
   s->previous_draw = s->ops.current_surface(EGL_DRAW);
   s->previous_read = s->ops.current_surface(EGL_READ);
   if (!s->ops.make_current(s->display, s->surface, s->surface, s->context))
   {
      s->failed = true;
      return false;
   }
   s->active = true;
   return true;
}

bool record_mediacodec_surface_begin(record_mediacodec_surface_t *s)
{
   return record_mcs_begin(s, false);
}
bool record_mediacodec_surface_begin_retirement(record_mediacodec_surface_t *s)
{
   return record_mcs_begin(s, true);
}

bool record_mediacodec_surface_leave(record_mediacodec_surface_t *s)
{
   EGLDisplay display;
   if (!s)
      return false;
   if (!s->active)
      return true;
   display = s->previous_display;
   if (display == EGL_NO_DISPLAY)
      display = s->display;
   if (!s->ops.make_current(
             display, s->previous_draw, s->previous_read, s->previous_context))
   {
      s->failed = true;
      return false;
   }
   s->active = false;
   return true;
}

bool record_mediacodec_surface_present(
      record_mediacodec_surface_t *s, int64_t timestamp_us)
{
   bool ok;
   if (!s || !s->active || s->failed)
      return false;
   ok = timestamp_us >= 0
         && (uint64_t)timestamp_us <= (((uint64_t)1 << 63) - 1) / 1000
         && (!s->posted || timestamp_us > s->last_pts);
   if (ok)
      ok = s->ops.presentation_time(
                 s->display, s->surface, (EGLnsecsANDROID)(timestamp_us * 1000))
            && s->ops.swap(s->display, s->surface);
   if (ok)
   {
      s->posted = true;
      s->last_pts = timestamp_us;
   }
   else
      s->failed = true;
   return record_mediacodec_surface_leave(s) && ok;
}
