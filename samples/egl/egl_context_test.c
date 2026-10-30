/* Execute context negotiation from egl_common.c against a bounded fake EGL. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../../gfx/common/egl_common.h"
#include "../../gfx/video_driver.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"egl_context_test:%u: %s\n",__LINE__,#x); exit(1); } } while (0)
static const char *extensions;
static EGLint errors[4], attributes[4][96];
static unsigned calls, error_reads, lost_flags;
static EGLContext shares[4];
static unsigned bind_calls;
static unsigned unbind_calls, release_calls, query_calls;
static EGLBoolean unbind_ok, release_ok;
static EGLenum thread_api, last_bound_api;

static void test_log(const char *format, ...) { CHECK(format != NULL); }
#define RARCH_ERR test_log
#define RARCH_WARN test_log
#define RARCH_LOG test_log

void video_driver_modify_disp_flags(uint32_t set, uint32_t clear)
{
   CHECK(clear == 0);
   lost_flags |= set;
}

static const char *query_string(EGLDisplay display, EGLint name)
{
   CHECK(display == (EGLDisplay)(uintptr_t)1 && name == EGL_EXTENSIONS);
   return extensions;
}

static EGLint get_error(void)
{
   CHECK(calls > 0 && calls <= 4);
   error_reads++;
   return errors[calls - 1];
}

static EGLContext create_context(EGLDisplay display, EGLConfig config,
      EGLContext share, const EGLint *attrs)
{
   unsigned n = calls++, i = 0;
   CHECK(display == (EGLDisplay)(uintptr_t)1 && config == (EGLConfig)(uintptr_t)2);
   CHECK(n < 4);
   shares[n] = share;
   while (attrs && attrs[i] != EGL_NONE)
   {
      CHECK(i + 2 < ARRAY_SIZE(attributes[n]));
      attributes[n][i] = attrs[i];
      attributes[n][i + 1] = attrs[i + 1];
      i += 2;
   }
   attributes[n][i] = EGL_NONE;
   return errors[n] == EGL_SUCCESS ? (EGLContext)(uintptr_t)(10 + n) : EGL_NO_CONTEXT;
}

#define _egl_query_string query_string
#define _egl_get_error get_error
#define _egl_create_context create_context
static EGLBoolean bind_api(EGLenum api)
{
   CHECK(api == EGL_OPENGL_ES_API || api == EGL_OPENGL_API);
   bind_calls++;
   last_bound_api = thread_api = api;
   return EGL_TRUE;
}

static EGLBoolean make_current(EGLDisplay display, EGLSurface draw,
      EGLSurface read, EGLContext context)
{
   CHECK(display == (EGLDisplay)(uintptr_t)1);
   CHECK(draw == EGL_NO_SURFACE && read == EGL_NO_SURFACE && context == EGL_NO_CONTEXT);
   unbind_calls++;
   return unbind_ok;
}

static EGLenum query_api(void)
{
   query_calls++;
   return thread_api;
}

static EGLBoolean release_thread(void)
{
   release_calls++;
   thread_api = EGL_OPENGL_ES_API;
   return release_ok;
}

#define HAVE_DYLIB
#define HAVE_DYNAMIC_EGL
static EGLenum (*_egl_query_api)(void) = query_api;
static EGLBoolean (*_egl_release_thread)(void) = release_thread;
#define _egl_make_current make_current

static EGLBoolean destroy_context(EGLDisplay display, EGLContext context)
{
   (void)display;
   (void)context;
   return EGL_TRUE;
}

static EGLSurface egl_create_hw_pbuffer(egl_ctx_data_t *egl)
{
   CHECK(egl != NULL);
   return (EGLSurface)(uintptr_t)20;
}

gfx_ctx_proc_t egl_get_proc_address(const char *symbol)
{
   (void)symbol;
   return NULL;
}

#define _egl_bind_api bind_api
#define _egl_destroy_context destroy_context
#define egl_report_error() egl_report_error_code(get_error())
#include "egl_context_fragment.c"

static void reset(egl_ctx_data_t *egl, const char *metadata)
{
   unsigned i;
   memset(egl,0,sizeof(*egl));
   egl->dpy = (EGLDisplay)(uintptr_t)1;
   egl->config = (EGLConfig)(uintptr_t)2;
   extensions = metadata;
   calls = error_reads = lost_flags = bind_calls = 0;
   unbind_calls = release_calls = query_calls = 0;
   unbind_ok = release_ok = EGL_TRUE;
   thread_api = EGL_OPENGL_API;
   last_bound_api = 0;
   _egl_query_api = query_api;
   _egl_release_thread = release_thread;
   memset(attributes,0,sizeof(attributes));
   for (i = 0; i < ARRAY_SIZE(errors); i++) errors[i] = EGL_SUCCESS;
}

int main(void)
{
   EGLint basic[] = {EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
   EGLint explicit_reset[] = {EGL_CONTEXT_CLIENT_VERSION,2,0x3138,0x31be,EGL_NONE};
   EGLint full[63];
   egl_ctx_data_t egl;
   gfx_ctx_shared_context_t auxiliary;
   unsigned i;
   const char *ext = "EGL_EXT_create_context_robustness";
   reset(&egl,ext);
   egl.use_hw_ctx = true;
   CHECK(egl_create_context(&egl,basic) && calls == 2);
   CHECK(attributes[0][2] == 0x3138 && attributes[0][3] == 0x31bf);
   CHECK(memcmp(attributes[0],attributes[1],5*sizeof(EGLint)) == 0);
   CHECK(memcmp(egl.ctx_attribs,attributes[0],5*sizeof(EGLint)) == 0);
   CHECK(shares[0] == EGL_NO_CONTEXT && shares[1] == egl.ctx && egl.ctx_attribs_ok);
   reset(&egl,"prefixEGL_EXT_create_context_robustness EGL_EXT_create_context_robustness_suffix");
   CHECK(egl_create_context(&egl,basic) && attributes[0][2] == EGL_NONE);
   reset(&egl,NULL);
   CHECK(egl_create_context(&egl,basic) && calls == 1 && attributes[0][2] == EGL_NONE);
   reset(&egl,"EGL_KHR_create_context");
   CHECK(egl_create_context(&egl,NULL) && attributes[0][0] == 0x31bd);
   reset(&egl,ext);
   CHECK(egl_create_context(&egl,explicit_reset));
   CHECK(attributes[0][3] == 0x31be && attributes[0][4] == EGL_NONE);
   for (i = 0; i < 60; i += 2) { full[i] = 0x4000 + i; full[i + 1] = i; }
   full[60] = EGL_NONE;
   reset(&egl,ext);
   CHECK(egl_create_context(&egl,full) && !egl.ctx_attribs_ok);
   CHECK(attributes[0][60] == EGL_NONE && calls == 1);
   for (i = 0; i < 2; i++)
   {
      reset(&egl,ext);
      egl.use_hw_ctx = true;
      errors[0] = i ? EGL_BAD_MATCH : EGL_BAD_ATTRIBUTE;
      CHECK(egl_create_context(&egl,basic) && calls == 3 && error_reads == 1);
      CHECK(attributes[1][2] == EGL_NONE && attributes[2][2] == EGL_NONE);
      CHECK(egl.ctx_attribs[2] == EGL_NONE && shares[2] == egl.ctx);
   }
   reset(&egl,ext);
   CHECK(egl_create_context(&egl,basic));
   errors[1] = EGL_CONTEXT_LOST;
   CHECK(!egl_create_context(&egl,basic) && calls == 2);
   CHECK(lost_flags == VIDEO_FLAG_GPU_DEVICE_LOST && egl.ctx == (EGLContext)(uintptr_t)10);
   reset(&egl,NULL);
   errors[0] = EGL_CONTEXT_LOST;
   CHECK(!egl_create_context(&egl,basic) && calls == 1 && lost_flags == VIDEO_FLAG_GPU_DEVICE_LOST);
   reset(&egl,ext);
   errors[0] = EGL_BAD_ATTRIBUTE;
   errors[1] = EGL_CONTEXT_LOST;
   CHECK(!egl_create_context(&egl,basic) && calls == 2 && lost_flags == VIDEO_FLAG_GPU_DEVICE_LOST);
   reset(&egl,ext);
   errors[0] = EGL_BAD_ALLOC;
   CHECK(!egl_create_context(&egl,basic) && calls == 1 && lost_flags == 0);
   reset(&egl,ext);
   egl.use_hw_ctx = true;
   errors[1] = EGL_CONTEXT_LOST;
   CHECK(!egl_create_context(&egl,basic) && calls == 2 && error_reads == 1);
   CHECK(lost_flags == VIDEO_FLAG_GPU_DEVICE_LOST && egl.hw_ctx == EGL_NO_CONTEXT);
   reset(&egl,ext);
   egl.use_hw_ctx = true;
   errors[1] = EGL_BAD_MATCH;
   CHECK(egl_create_context(&egl,basic) && calls == 2 && error_reads == 1);
   CHECK(lost_flags == 0 && egl.ctx != EGL_NO_CONTEXT && egl.hw_ctx == EGL_NO_CONTEXT);
   CHECK(egl_no_shared_context(&egl));
   reset(&egl,"EGL_KHR_surfaceless_context");
   egl.single_context = true;
   egl.use_hw_ctx = true;
   CHECK(egl_create_context(&egl,basic) && calls == 1);
   CHECK(egl.ctx != EGL_NO_CONTEXT && egl.hw_ctx == EGL_NO_CONTEXT);
   CHECK(egl_no_shared_context(&egl));
   memset(&auxiliary,0xa5,sizeof(auxiliary));
   CHECK(!egl_shared_context_new(&egl,EGL_OPENGL_ES_API,&auxiliary));
   CHECK(calls == 1 && bind_calls == 0 && auxiliary.context == EGL_NO_CONTEXT);
   /* The same consumer can create a shared context on an unaffected backend. */
   egl.single_context = false;
   CHECK(egl_shared_context_new(&egl,EGL_OPENGL_ES_API,&auxiliary));
   CHECK(calls == 2 && bind_calls == 1 && auxiliary.context != EGL_NO_CONTEXT);
   CHECK(shares[1] == egl.ctx && auxiliary.kind == GFX_CTX_SHARED_EGL);
   reset(&egl,NULL);
   egl.single_context = true;
   CHECK(egl_create_context(&egl,basic) && !egl_no_shared_context(&egl));
   reset(&egl,NULL);
   CHECK(egl_create_context(&egl,basic));
   egl_release_current(&egl);
   CHECK(unbind_calls == 1 && release_calls == 0 && query_calls == 0);
   CHECK(bind_calls == 0 && lost_flags == 0);
   unbind_ok = EGL_FALSE;
   errors[0] = EGL_CONTEXT_LOST;
   egl_release_current(&egl);
   CHECK(unbind_calls == 2 && release_calls == 1 && query_calls == 1);
   CHECK(bind_calls == 1 && last_bound_api == EGL_OPENGL_API && thread_api == EGL_OPENGL_API);
   CHECK(lost_flags == VIDEO_FLAG_GPU_DEVICE_LOST);
   release_ok = EGL_FALSE;
   bind_calls = 0;
   egl_release_current(&egl);
   CHECK(release_calls == 2 && bind_calls == 0);
   _egl_release_thread = NULL;
   egl_release_current(&egl);
   CHECK(release_calls == 2 && bind_calls == 0);
   unbind_calls = 0;
   egl_release_current(NULL);
   egl.dpy = EGL_NO_DISPLAY;
   egl_release_current(&egl);
   CHECK(unbind_calls == 0);
   puts("EGL context negotiation and loss reporting: passed");
   return 0;
}
