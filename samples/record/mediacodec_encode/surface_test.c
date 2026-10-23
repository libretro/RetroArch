/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../record/record_mediacodec_surface.h"

#define DISPLAY ((EGLDisplay)(uintptr_t)5)
#define CONFIG ((EGLConfig)(uintptr_t)6)
#define WINDOW ((EGLNativeWindowType)(uintptr_t)7)
#define CONTEXT ((EGLContext)(uintptr_t)8)
#define SURFACE ((EGLSurface)(uintptr_t)9)
#define SHARE ((EGLContext)(uintptr_t)2)

static EGLDisplay current_dpy;
static EGLContext current_ctx;
static EGLSurface current_draw, current_read;
static EGLenum api;
static int bad_config, attrib_fail, query_fail, wrong_size;
static unsigned attrs, queries, make_calls, bad_make;
static unsigned context_live, surface_live, context_fail, surface_fail;
static unsigned stamp_calls, swaps;
static int64_t stamp;
static bool pq, context_new_fail, surface_new_fail, stamp_fail, swap_fail;
static const char *extensions;
static const char *version;

static const char *EGLAPIENTRY query_string(EGLDisplay d, EGLint name)
{
   assert(d == DISPLAY && (name == EGL_EXTENSIONS || name == EGL_VERSION));
   return name == EGL_EXTENSIONS ? extensions : version;
}
static EGLBoolean EGLAPIENTRY config_attrib(
      EGLDisplay d, EGLConfig config, EGLint token, EGLint *value)
{
   EGLint result;
   assert(d == DISPLAY && config == CONFIG);
   attrs++;
   if (attrib_fail == (int)attrs)
      return EGL_FALSE;
   switch (token)
   {
      case EGL_RECORDABLE_ANDROID:
         result = EGL_TRUE;
         break;
      case EGL_SURFACE_TYPE:
         result = EGL_WINDOW_BIT;
         break;
      case EGL_RENDERABLE_TYPE:
         result = EGL_OPENGL_ES3_BIT_KHR;
         break;
      case EGL_RED_SIZE:
      case EGL_GREEN_SIZE:
      case EGL_BLUE_SIZE:
         result = pq ? 10 : 8;
         break;
      case EGL_ALPHA_SIZE:
         result = pq ? 2 : 8;
         break;
      default:
         assert(0);
         return EGL_FALSE;
   }
   *value = bad_config == (int)attrs ? 16 : result;
   return EGL_TRUE;
}
static EGLContext EGLAPIENTRY create_context(
      EGLDisplay d, EGLConfig config, EGLContext share, const EGLint *a)
{
   assert(d == DISPLAY && config == CONFIG && share == SHARE);
   assert(a[0] == EGL_CONTEXT_CLIENT_VERSION && a[1] == 3 && a[2] == EGL_NONE);
   if (context_new_fail)
      return EGL_NO_CONTEXT;
   assert(!context_live);
   context_live++;
   return CONTEXT;
}
static EGLSurface EGLAPIENTRY create_surface(EGLDisplay d, EGLConfig config,
      EGLNativeWindowType window, const EGLint *a)
{
   assert(d == DISPLAY && config == CONFIG && window == WINDOW);
   if (pq)
      assert(a[0] == EGL_GL_COLORSPACE_KHR
            && a[1] == EGL_GL_COLORSPACE_BT2020_PQ_EXT && a[2] == EGL_NONE);
   else
      assert(a[0] == EGL_NONE);
   if (surface_new_fail)
      return EGL_NO_SURFACE;
   assert(!surface_live);
   surface_live++;
   return SURFACE;
}
static EGLBoolean EGLAPIENTRY destroy_context(EGLDisplay d, EGLContext c)
{
   assert(d == DISPLAY && c == CONTEXT && context_live && current_ctx != c);
   if (context_fail)
   {
      context_fail--;
      return EGL_FALSE;
   }
   context_live--;
   return EGL_TRUE;
}
static EGLBoolean EGLAPIENTRY destroy_surface(EGLDisplay d, EGLSurface s)
{
   assert(d == DISPLAY && s == SURFACE && surface_live && current_draw != s);
   if (surface_fail)
   {
      surface_fail--;
      return EGL_FALSE;
   }
   surface_live--;
   return EGL_TRUE;
}
static EGLBoolean EGLAPIENTRY query_surface(
      EGLDisplay d, EGLSurface s, EGLint token, EGLint *value)
{
   assert(d == DISPLAY && s == SURFACE);
   queries++;
   if (query_fail == (int)queries)
      return EGL_FALSE;
   assert(token == EGL_WIDTH || token == EGL_HEIGHT);
   *value = token == EGL_WIDTH ? 128 : 96;
   if (wrong_size == (int)queries)
      (*value)++;
   return EGL_TRUE;
}
static EGLenum EGLAPIENTRY query_api(void)
{
   return api;
}
static EGLContext EGLAPIENTRY current_context(void)
{
   return current_ctx;
}
static EGLDisplay EGLAPIENTRY current_display(void)
{
   return current_dpy;
}
static EGLSurface EGLAPIENTRY current_surface(EGLint which)
{
   assert(which == EGL_DRAW || which == EGL_READ);
   return which == EGL_DRAW ? current_draw : current_read;
}
static EGLBoolean EGLAPIENTRY make_current(
      EGLDisplay d, EGLSurface draw, EGLSurface read, EGLContext c)
{
   make_calls++;
   if (make_calls == bad_make)
      return EGL_FALSE;
   if (c == CONTEXT)
      assert(d == DISPLAY && draw == SURFACE && read == SURFACE);
   else
      assert(c == SHARE || c == EGL_NO_CONTEXT);
   current_ctx = c;
   current_dpy = c == EGL_NO_CONTEXT ? EGL_NO_DISPLAY : d;
   current_draw = draw;
   current_read = read;
   return EGL_TRUE;
}
static EGLBoolean EGLAPIENTRY presentation_time(
      EGLDisplay d, EGLSurface s, EGLnsecsANDROID time)
{
   assert(d == DISPLAY && s == SURFACE && current_ctx == CONTEXT);
   stamp_calls++;
   stamp = time;
   return stamp_fail ? EGL_FALSE : EGL_TRUE;
}
static EGLBoolean EGLAPIENTRY swap(EGLDisplay d, EGLSurface s)
{
   assert(d == DISPLAY && s == SURFACE && current_ctx == CONTEXT);
   swaps++;
   return swap_fail ? EGL_FALSE : EGL_TRUE;
}

static const record_mediacodec_surface_ops_t ops = {query_string, config_attrib,
      create_context, create_surface, destroy_context, destroy_surface,
      query_surface, query_api, current_context, current_display,
      current_surface, make_current, presentation_time, swap};

static void reset(bool hdr)
{
   assert(!context_live && !surface_live);
   current_dpy = (EGLDisplay)(uintptr_t)1;
   current_ctx = SHARE;
   current_draw = (EGLSurface)(uintptr_t)3;
   current_read = (EGLSurface)(uintptr_t)4;
   api = EGL_OPENGL_ES_API;
   bad_config = attrib_fail = query_fail = wrong_size = 0;
   attrs = queries = make_calls = bad_make = 0;
   context_fail = surface_fail = stamp_calls = swaps = 0;
   stamp = -1;
   pq = hdr;
   context_new_fail = surface_new_fail = stamp_fail = swap_fail = false;
   extensions = "EGL_ANDROID_recordable EGL_ANDROID_presentation_time "
                "EGL_KHR_create_context EGL_KHR_gl_colorspace "
                "EGL_EXT_gl_colorspace_bt2020_pq";
   version = "1.4 Test EGL";
}
static record_mediacodec_surface_t *create(void)
{
   record_mediacodec_surface_t *s = NULL;
   bool ok = record_mediacodec_surface_new(
         &ops, DISPLAY, CONFIG, SHARE, WINDOW, 128, 96, pq, &s);
   assert(ok || !s);
   return s;
}
static void restored(void)
{
   assert(current_ctx == SHARE && current_dpy == (EGLDisplay)(uintptr_t)1);
   assert(current_draw == (EGLSurface)(uintptr_t)3
         && current_read == (EGLSurface)(uintptr_t)4);
}
static void admission(void)
{
   record_mediacodec_surface_t *s;
   record_mediacodec_surface_ops_t missing;
   unsigned i;
   reset(false);
   assert(!record_mediacodec_surface_new(
         NULL, DISPLAY, CONFIG, SHARE, WINDOW, 128, 96, false, &s));
#define MISSING(member)                                                        \
   do                                                                          \
   {                                                                           \
      missing = ops;                                                           \
      missing.member = NULL;                                                   \
      assert(!record_mediacodec_surface_new(                                   \
            &missing, DISPLAY, CONFIG, SHARE, WINDOW, 128, 96, false, &s));    \
   } while (0)
   MISSING(query_string);
   MISSING(config_attrib);
   MISSING(create_context);
   MISSING(create_surface);
   MISSING(destroy_context);
   MISSING(destroy_surface);
   MISSING(query_surface);
   MISSING(query_api);
   MISSING(current_context);
   MISSING(current_display);
   MISSING(current_surface);
   MISSING(make_current);
   MISSING(presentation_time);
   MISSING(swap);
#undef MISSING
   assert(!record_mediacodec_surface_new(
         &ops, DISPLAY, CONFIG, SHARE, WINDOW, 0, 96, false, &s));
   assert(!record_mediacodec_surface_new(
         &ops, DISPLAY, CONFIG, SHARE, WINDOW, 128, 16385, false, &s));
   api = EGL_OPENGL_API;
   assert(!create());
   for (i = 1; i <= 7; i++)
   {
      reset(false);
      attrib_fail = (int)i;
      assert(!create());
      reset(false);
      bad_config = (int)i;
      assert(!create());
   }
   for (i = 1; i <= 2; i++)
   {
      reset(false);
      query_fail = (int)i;
      assert(!create());
      reset(false);
      wrong_size = (int)i;
      assert(!create());
   }
   reset(false);
   extensions = "EGL_ANDROID_recordableX EGL_ANDROID_presentation_time "
                "EGL_KHR_create_context";
   assert(!create());
   extensions = "EGL_ANDROID_recordable X_EGL_ANDROID_presentation_time "
                "EGL_KHR_create_context";
   assert(!create());
   extensions = NULL;
   assert(!create());
   reset(true);
   extensions = "EGL_ANDROID_recordable EGL_ANDROID_presentation_time "
                "EGL_KHR_create_context";
   assert(!create());
   reset(false);
   context_new_fail = true;
   assert(!create());
   reset(false);
   surface_new_fail = true;
   assert(!create());
   reset(true);
   s = create();
   assert(s && record_mediacodec_surface_free(s));
   reset(true);
   version = "1.5 Test EGL";
   extensions = "EGL_ANDROID_recordable EGL_ANDROID_presentation_time "
                "EGL_EXT_gl_colorspace_bt2020_pq";
   s = create();
   assert(s && record_mediacodec_surface_free(s));
   version = "1.4 Test EGL";
   assert(!create());
}

static void service(void)
{
   record_mediacodec_surface_t *s;
   unsigned i;
   int64_t limit = (int64_t)((((uint64_t)1 << 63) - 1) / 1000);
   reset(false);
   s = create();
   assert(s && !make_calls);
   restored();
   assert(!record_mediacodec_surface_present(s, 0));
   assert(record_mediacodec_surface_begin(s));
   assert(!record_mediacodec_surface_begin(s));
   assert(!record_mediacodec_surface_free(s));
   assert(record_mediacodec_surface_present(s, 0));
   assert(stamp == 0 && swaps == 1);
   restored();
   for (i = 1; i <= 10000; i++)
   {
      assert(record_mediacodec_surface_begin(s));
      assert(record_mediacodec_surface_present(s, i));
      assert(stamp == (int64_t)i * 1000);
      restored();
   }
   assert(record_mediacodec_surface_begin(s));
   assert(record_mediacodec_surface_present(s, limit));
   assert(stamp == limit * 1000);
   assert(record_mediacodec_surface_begin(s));
   assert(!record_mediacodec_surface_present(s, limit + 1));
   assert(stamp_calls == 10002);
   restored();
   assert(!record_mediacodec_surface_begin(s));
   assert(record_mediacodec_surface_free(s));
   reset(false);
   current_ctx = EGL_NO_CONTEXT;
   current_dpy = EGL_NO_DISPLAY;
   current_draw = current_read = EGL_NO_SURFACE;
   s = create();
   assert(s && record_mediacodec_surface_begin(s));
   assert(record_mediacodec_surface_leave(s));
   assert(current_ctx == EGL_NO_CONTEXT && current_dpy == EGL_NO_DISPLAY);
   assert(record_mediacodec_surface_free(s));
}

static void failures(void)
{
   record_mediacodec_surface_t *s;
   unsigned i;
   for (i = 0; i < 3; i++)
   {
      reset(false);
      s = create();
      assert(s && record_mediacodec_surface_begin(s));
      stamp_fail = i == 0;
      swap_fail = i == 1;
      assert(!record_mediacodec_surface_present(s, i < 2 ? 0 : -1));
      assert(stamp_calls == (i < 2 ? 1u : 0u));
      assert(swaps == (i == 1 ? 1u : 0u));
      restored();
      assert(!record_mediacodec_surface_begin(s));
      assert(record_mediacodec_surface_begin_retirement(s));
      assert(current_ctx == CONTEXT);
      assert(!record_mediacodec_surface_present(s, 100));
      assert(record_mediacodec_surface_leave(s));
      restored();
      assert(record_mediacodec_surface_free(s));
   }
   reset(false);
   s = create();
   bad_make = 1;
   assert(s && !record_mediacodec_surface_begin(s));
   restored();
   assert(record_mediacodec_surface_free(s));
   reset(false);
   s = create();
   bad_make = 2;
   assert(s && record_mediacodec_surface_begin(s));
   assert(!record_mediacodec_surface_present(s, 0));
   assert(!record_mediacodec_surface_free(s));
   assert(record_mediacodec_surface_begin_retirement(s));
   assert(!record_mediacodec_surface_present(s, 1) && swaps == 1);
   assert(record_mediacodec_surface_leave(s));
   restored();
   assert(record_mediacodec_surface_free(s));
   reset(false);
   s = create();
   context_fail = surface_fail = 1;
   assert(s && !record_mediacodec_surface_free(s));
   assert(context_live && surface_live);
   assert(!record_mediacodec_surface_begin(s));
   assert(!record_mediacodec_surface_free(s));
   assert(!context_live && surface_live);
   assert(record_mediacodec_surface_free(s));
   reset(false);
   query_fail = 1;
   context_fail = surface_fail = 1;
   assert(!record_mediacodec_surface_new(
         &ops, DISPLAY, CONFIG, SHARE, WINDOW, 128, 96, false, &s));
   assert(s && context_live && surface_live);
   assert(!record_mediacodec_surface_free(s));
   assert(!context_live && surface_live);
   assert(!record_mediacodec_surface_begin_retirement(s));
   assert(record_mediacodec_surface_free(s));
   reset(false);
   s = create();
   assert(s && record_mediacodec_surface_begin(s));
   assert(record_mediacodec_surface_present(s, 1));
   assert(record_mediacodec_surface_begin(s));
   assert(!record_mediacodec_surface_present(s, 1));
   assert(record_mediacodec_surface_free(s));
}
int main(void)
{
   admission();
   service();
   failures();
   assert(record_mediacodec_surface_free(NULL));
   assert(!context_live && !surface_live);
   puts("PASS: EGL encoder Surface config, source PTS, context restoration "
        "and failure ownership; native EGL mocked");
   return 0;
}
