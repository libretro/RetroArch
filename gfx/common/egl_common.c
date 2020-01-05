/*  RetroArch - A frontend for libretro.
 *  Copyright (c) 2011-2017 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef _MSC_VER
#pragma comment(lib, "libEGL")
#endif

#include <stdlib.h>

#include <retro_assert.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_OPENGL
#include "gl_common.h"
#endif

#include "egl_common.h"
#include "../../verbosity.h"
#include "../../frontend/frontend_driver.h"

bool g_egl_inited    = false;

unsigned g_egl_major = 0;
unsigned g_egl_minor = 0;

#if defined(HAVE_DYNAMIC) && defined(HAVE_DYNAMIC_EGL)
#include <dynamic/dylib.h>

typedef EGLBoolean(* PFN_EGL_QUERY_SURFACE)(
      EGLDisplay dpy,
      EGLSurface surface,
      EGLint attribute,
      EGLint *value);
typedef void *(* PFN_EGL_GET_PROC_ADDRESS)(const char *procname);
typedef EGLSurface(*PFN_EGL_CREATE_WINDOW_SURFACE) (EGLDisplay dpy,
					    EGLConfig config,
					    EGLNativeWindowType win,
					    const EGLint * attrib_list);
typedef EGLContext(*PFN_EGL_CREATE_CONTEXT)(EGLDisplay dpy, EGLConfig config,
				      EGLContext share_context,
				      const EGLint * attrib_list);
typedef EGLBoolean(*PFN_EGL_GET_CONFIGS) (EGLDisplay dpy, EGLConfig * configs,
				   EGLint config_size, EGLint * num_config);
typedef EGLint(*PFN_EGL_GET_ERROR) (void);
typedef EGLDisplay(*PFN_EGL_GET_DISPLAY) (EGLNativeDisplayType display_id);
typedef EGLBoolean(*PFN_EGL_CHOOSE_CONFIG) (EGLDisplay dpy,
				     const EGLint * attrib_list,
				     EGLConfig * configs,
				     EGLint config_size, EGLint * num_config);
typedef EGLBoolean(*PFN_EGL_TERMINATE)(EGLDisplay dpy);
typedef EGLBoolean(*PFN_EGL_INITIALIZE)(EGLDisplay dpy, EGLint * major,
				   EGLint * minor);
typedef EGLBoolean(*PFN_EGL_BIND_API) (EGLenum api);
typedef EGLBoolean(*PFN_EGL_MAKE_CURRENT) (EGLDisplay dpy, EGLSurface draw,
				    EGLSurface read, EGLContext ctx);
typedef EGLBoolean(*PFN_EGL_DESTROY_SURFACE) (EGLDisplay dpy, EGLSurface surface);
typedef EGLBoolean(*PFN_EGL_DESTROY_CONTEXT) (EGLDisplay dpy, EGLContext ctx);
typedef EGLContext(*PFN_EGL_GET_CURRENT_CONTEXT) (void);
typedef const char *(*PFN_EGL_QUERY_STRING) (EGLDisplay dpy, EGLint name);
typedef EGLBoolean(*PFN_EGL_GET_CONFIG_ATTRIB) (EGLDisplay dpy,
					EGLConfig config,
					EGLint attribute, EGLint * value);
typedef EGLBoolean(*PFN_EGL_SWAP_BUFFERS) (EGLDisplay dpy, EGLSurface surface);
typedef EGLBoolean(*PFN_EGL_SWAP_INTERVAL) (EGLDisplay dpy, EGLint interval);

static PFN_EGL_QUERY_SURFACE             _egl_query_surface;
static PFN_EGL_GET_PROC_ADDRESS          _egl_get_proc_address;
static PFN_EGL_CREATE_WINDOW_SURFACE     _egl_create_window_surface;
static PFN_EGL_CREATE_CONTEXT            _egl_create_context;
static PFN_EGL_GET_CONFIGS               _egl_get_configs;
static PFN_EGL_GET_ERROR                 _egl_get_error;
static PFN_EGL_GET_DISPLAY               _egl_get_display;
static PFN_EGL_CHOOSE_CONFIG             _egl_choose_config;
static PFN_EGL_TERMINATE                 _egl_terminate;
static PFN_EGL_INITIALIZE                _egl_initialize;
static PFN_EGL_BIND_API                  _egl_bind_api;
static PFN_EGL_MAKE_CURRENT              _egl_make_current;
static PFN_EGL_DESTROY_SURFACE           _egl_destroy_surface;
static PFN_EGL_DESTROY_CONTEXT           _egl_destroy_context;
static PFN_EGL_GET_CURRENT_CONTEXT       _egl_get_current_context;
static PFN_EGL_QUERY_STRING              _egl_query_string;
static PFN_EGL_GET_CONFIG_ATTRIB         _egl_get_config_attrib;
static PFN_EGL_SWAP_BUFFERS              _egl_swap_buffers;
static PFN_EGL_SWAP_INTERVAL             _egl_swap_interval;

#else
#define _egl_query_surface(a, b, c, d) eglQuerySurface(a, b, c, d)
#define _egl_get_proc_address(a) eglGetProcAddress(a)
#define _egl_create_window_surface(a, b, c, d) eglCreateWindowSurface(a, b, c, d)
#define _egl_create_context(a, b, c, d) eglCreateContext(a, b, c, d)
#define _egl_get_configs(a, b, c, d) eglGetConfigs(a, b, c, d)
#define _egl_get_display(a) eglGetDisplay(a)
#define _egl_choose_config(a, b, c, d, e) eglChooseConfig(a, b, c, d, e)
#define _egl_make_current(a, b, c, d) eglMakeCurrent(a, b, c, d)
#define _egl_initialize(a, b, c) eglInitialize(a, b, c)
#define _egl_destroy_surface(a, b) eglDestroySurface(a, b)
#define _egl_destroy_context(a, b) eglDestroyContext(a, b)
#define _egl_get_current_context() eglGetCurrentContext()
#define _egl_get_error() eglGetError()
#define _egl_terminate(dpy) eglTerminate(dpy)
#define _egl_bind_api(a) eglBindAPI(a)
#define _egl_query_string(a, b) eglQueryString(a, b)
#define _egl_get_config_attrib(a, b, c, d) eglGetConfigAttrib(a, b, c, d)
#define _egl_swap_buffers(a, b) eglSwapBuffers(a, b)
#define _egl_swap_interval(a, b) eglSwapInterval(a, b)
#endif

bool egl_init_dll(void)
{
#if defined(HAVE_DYNAMIC) && defined(HAVE_DYNAMIC_EGL)
   static dylib_t egl_dll;

   if (!egl_dll)
   {
      egl_dll = dylib_load("libEGL.dll");
      if (egl_dll)
      {
         /* Setup function callbacks once */
         _egl_query_surface         = (PFN_EGL_QUERY_SURFACE)dylib_proc(
               egl_dll, "eglQuerySurface");
         _egl_get_proc_address      = (PFN_EGL_GET_PROC_ADDRESS)dylib_proc(
               egl_dll, "eglGetProcAddress");
         _egl_create_window_surface = (PFN_EGL_CREATE_WINDOW_SURFACE)dylib_proc(
               egl_dll, "eglCreateWindowSurface");
         _egl_create_context        = (PFN_EGL_CREATE_CONTEXT)dylib_proc(
               egl_dll, "eglCreateContext");
         _egl_get_configs           = (PFN_EGL_GET_CONFIGS)dylib_proc(
               egl_dll, "eglGetConfigs");
         _egl_get_error             = (PFN_EGL_GET_ERROR)dylib_proc(
               egl_dll, "eglGetError");
         _egl_get_display           = (PFN_EGL_GET_DISPLAY)dylib_proc(
               egl_dll, "eglGetDisplay");
         _egl_choose_config         = (PFN_EGL_CHOOSE_CONFIG)dylib_proc(
               egl_dll, "eglChooseConfig");
         _egl_terminate             = (PFN_EGL_TERMINATE)dylib_proc(
               egl_dll, "eglTerminate");
         _egl_initialize            = (PFN_EGL_INITIALIZE)dylib_proc(
               egl_dll, "eglInitialize");
         _egl_bind_api              = (PFN_EGL_BIND_API)dylib_proc(
               egl_dll, "eglBindAPI");
         _egl_make_current          = (PFN_EGL_MAKE_CURRENT)dylib_proc(
               egl_dll, "eglMakeCurrent");
         _egl_destroy_surface       = (PFN_EGL_DESTROY_SURFACE)dylib_proc(
               egl_dll, "eglDestroySurface");
         _egl_destroy_context       = (PFN_EGL_DESTROY_CONTEXT)dylib_proc(
               egl_dll, "eglDestroyContext");
         _egl_get_current_context   = (PFN_EGL_GET_CURRENT_CONTEXT)dylib_proc(
               egl_dll, "eglGetCurrentContext");
         _egl_query_string          = (PFN_EGL_QUERY_STRING)dylib_proc(
               egl_dll, "eglQueryString");
         _egl_get_config_attrib     = (PFN_EGL_GET_CONFIG_ATTRIB)dylib_proc(
               egl_dll, "eglGetConfigAttrib");
         _egl_swap_buffers          = (PFN_EGL_SWAP_BUFFERS)dylib_proc(
               egl_dll, "eglSwapBuffers");
         _egl_swap_interval         = (PFN_EGL_SWAP_INTERVAL)dylib_proc(
               egl_dll, "eglSwapInterval");
      }
   }

   if (egl_dll)
      return true;
#endif
   return false;
}

void egl_report_error(void)
{
   EGLint    error = _egl_get_error();
   const char *str = NULL;
   switch (error)
   {
      case EGL_SUCCESS:
         str = "EGL_SUCCESS";
         break;

      case EGL_BAD_ACCESS:
         str = "EGL_BAD_ACCESS";
         break;

      case EGL_BAD_ALLOC:
         str = "EGL_BAD_ALLOC";
         break;

      case EGL_BAD_ATTRIBUTE:
         str = "EGL_BAD_ATTRIBUTE";
         break;

      case EGL_BAD_CONFIG:
         str = "EGL_BAD_CONFIG";
         break;

      case EGL_BAD_CONTEXT:
         str = "EGL_BAD_CONTEXT";
         break;

      case EGL_BAD_CURRENT_SURFACE:
         str = "EGL_BAD_CURRENT_SURFACE";
         break;

      case EGL_BAD_DISPLAY:
         str = "EGL_BAD_DISPLAY";
         break;

      case EGL_BAD_MATCH:
         str = "EGL_BAD_MATCH";
         break;

      case EGL_BAD_NATIVE_PIXMAP:
         str = "EGL_BAD_NATIVE_PIXMAP";
         break;

      case EGL_BAD_NATIVE_WINDOW:
         str = "EGL_BAD_NATIVE_WINDOW";
         break;

      case EGL_BAD_PARAMETER:
         str = "EGL_BAD_PARAMETER";
         break;

      case EGL_BAD_SURFACE:
         str = "EGL_BAD_SURFACE";
         break;

      default:
         str = "Unknown";
         break;
   }

   RARCH_ERR("[EGL]: #0x%x, %s\n", (unsigned)error, str);
}

gfx_ctx_proc_t egl_get_proc_address(const char *symbol)
{
   return _egl_get_proc_address(symbol);
}

void egl_terminate(EGLDisplay dpy)
{
   _egl_terminate(dpy);
}

bool egl_get_config_attrib(EGLDisplay dpy, EGLConfig config, EGLint attribute,
      EGLint *value)
{
   return _egl_get_config_attrib(dpy, config, attribute, value);
}

bool egl_initialize(EGLDisplay dpy, EGLint *major, EGLint *minor)
{
   return _egl_initialize(dpy, major, minor);
}

bool egl_bind_api(EGLenum egl_api)
{
   return _egl_bind_api(egl_api);
}

void egl_destroy(egl_ctx_data_t *egl)
{
   if (egl->dpy)
   {
#if defined HAVE_OPENGL
#if !defined(RARCH_MOBILE)
      if (egl->ctx != EGL_NO_CONTEXT)
      {
         glFlush();
         glFinish();
      }
#endif
#endif

      _egl_make_current(egl->dpy,
            EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      if (egl->ctx != EGL_NO_CONTEXT)
         _egl_destroy_context(egl->dpy, egl->ctx);

      if (egl->hw_ctx != EGL_NO_CONTEXT)
         _egl_destroy_context(egl->dpy, egl->hw_ctx);

      if (egl->surf != EGL_NO_SURFACE)
         _egl_destroy_surface(egl->dpy, egl->surf);
      egl_terminate(egl->dpy);
   }

   /* Be as careful as possible in deinit.
    * If we screw up, any TTY will not restore.
    */

   egl->ctx     = EGL_NO_CONTEXT;
   egl->hw_ctx  = EGL_NO_CONTEXT;
   egl->surf    = EGL_NO_SURFACE;
   egl->dpy     = EGL_NO_DISPLAY;
   egl->config  = 0;
   g_egl_inited  = false;

   frontend_driver_destroy_signal_handler_state();
}

void egl_bind_hw_render(egl_ctx_data_t *egl, bool enable)
{
   egl->use_hw_ctx = enable;

   if (egl->dpy  == EGL_NO_DISPLAY)
      return;
   if (egl->surf == EGL_NO_SURFACE)
      return;

   _egl_make_current(egl->dpy, egl->surf,
         egl->surf,
         enable ? egl->hw_ctx : egl->ctx);
}

void egl_swap_buffers(void *data)
{
   egl_ctx_data_t *egl = (egl_ctx_data_t*)data;
   if (  egl                         &&
         egl->dpy  != EGL_NO_DISPLAY &&
         egl->surf != EGL_NO_SURFACE
         )
      _egl_swap_buffers(egl->dpy, egl->surf);
}

void egl_set_swap_interval(egl_ctx_data_t *egl, int interval)
{
   /* Can be called before initialization.
    * Some contexts require that swap interval
    * is known at startup time.
    */
   egl->interval = interval;

   if (egl->dpy  == EGL_NO_DISPLAY)
      return;
   if (!_egl_get_current_context())
      return;

   RARCH_LOG("[EGL]: eglSwapInterval(%u)\n", interval);
   if (!_egl_swap_interval(egl->dpy, interval))
   {
      RARCH_ERR("[EGL]: eglSwapInterval() failed.\n");
      egl_report_error();
   }
}

void egl_get_video_size(egl_ctx_data_t *egl, unsigned *width, unsigned *height)
{
   *width  = 0;
   *height = 0;

   if (egl->dpy != EGL_NO_DISPLAY && egl->surf != EGL_NO_SURFACE)
   {
      EGLint gl_width, gl_height;

      _egl_query_surface(egl->dpy, egl->surf, EGL_WIDTH, &gl_width);
      _egl_query_surface(egl->dpy, egl->surf, EGL_HEIGHT, &gl_height);
      *width  = gl_width;
      *height = gl_height;
   }
}

bool check_egl_version(int minMajorVersion, int minMinorVersion)
{
   int count;
   int major, minor;
   const char *str = _egl_query_string(EGL_NO_DISPLAY, EGL_VERSION);

   if (!str)
      return false;

   count = sscanf(str, "%d.%d", &major, &minor);
   if (count != 2)
      return false;

   if (major < minMajorVersion)
      return false;

   if (major > minMajorVersion)
      return true;

   if (minor >= minMinorVersion)
      return true;

   return false;
}

bool check_egl_client_extension(const char *name)
{
   size_t nameLen;
   const char *str = _egl_query_string(EGL_NO_DISPLAY, EGL_EXTENSIONS);

   /* The EGL implementation doesn't support client extensions at all. */
   if (!str)
      return false;

   nameLen = strlen(name);
   while (*str != '\0')
   {
      /* Use strspn and strcspn to find the start position and length of each
       * token in the extension string. Using strtok could also work, but
       * that would require allocating a copy of the string. */
      size_t len = strcspn(str, " ");
      if (len == nameLen && strncmp(str, name, nameLen) == 0)
         return true;
      str += len;
      str += strspn(str, " ");
   }

   return false;
}

static EGLDisplay get_egl_display(EGLenum platform, void *native)
{
   if (platform != EGL_NONE)
   {
      /* If the client library supports at least EGL 1.5, then we can call
       * eglGetPlatformDisplay. Otherwise, see if eglGetPlatformDisplayEXT
       * is available. */
#if defined(EGL_VERSION_1_5)
      if (check_egl_version(1, 5))
      {
         typedef EGLDisplay (EGLAPIENTRY * pfn_eglGetPlatformDisplay)
            (EGLenum platform, void *native_display, const EGLAttrib *attrib_list);
         pfn_eglGetPlatformDisplay ptr_eglGetPlatformDisplay;

         RARCH_LOG("[EGL] Found EGL client version >= 1.5, trying eglGetPlatformDisplay\n");
         ptr_eglGetPlatformDisplay = (pfn_eglGetPlatformDisplay)
            egl_get_proc_address("eglGetPlatformDisplay");

         if (ptr_eglGetPlatformDisplay)
         {
            EGLDisplay dpy = ptr_eglGetPlatformDisplay(platform, native, NULL);
            if (dpy != EGL_NO_DISPLAY)
               return dpy;
         }
      }
#endif /* defined(EGL_VERSION_1_5) */

#if defined(EGL_EXT_platform_base)
      if (check_egl_client_extension("EGL_EXT_platform_base"))
      {
         PFNEGLGETPLATFORMDISPLAYEXTPROC ptr_eglGetPlatformDisplayEXT;

         RARCH_LOG("[EGL] Found EGL_EXT_platform_base, trying eglGetPlatformDisplayEXT\n");
         ptr_eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC)
            egl_get_proc_address("eglGetPlatformDisplayEXT");

         if (ptr_eglGetPlatformDisplayEXT)
         {
            EGLDisplay dpy = ptr_eglGetPlatformDisplayEXT(platform, native, NULL);
            if (dpy != EGL_NO_DISPLAY)
               return dpy;
         }
      }
#endif /* defined(EGL_EXT_platform_base) */
   }

   /* Either the caller didn't provide a platform type, or the EGL
    * implementation doesn't support eglGetPlatformDisplay. In this case, try
    * eglGetDisplay and hope for the best. */
   RARCH_LOG("[EGL] Falling back to eglGetDisplay\n");
   return _egl_get_display((EGLNativeDisplayType) native);
}

bool egl_get_native_visual_id(egl_ctx_data_t *egl, EGLint *value)
{
   if (!egl_get_config_attrib(egl->dpy, egl->config,
         EGL_NATIVE_VISUAL_ID, value))
   {
      RARCH_ERR("[EGL]: egl_get_native_visual_id failed.\n");
      return false;
   }

   return true;
}

bool egl_default_accept_config_cb(void *display_data, EGLDisplay dpy, EGLConfig config)
{
   /* Makes sure we have 8 bit color. */
   EGLint r, g, b;
   if (!egl_get_config_attrib(dpy, config, EGL_RED_SIZE, &r))
      return false;
   if (!egl_get_config_attrib(dpy, config, EGL_GREEN_SIZE, &g))
      return false;
   if (!egl_get_config_attrib(dpy, config, EGL_BLUE_SIZE, &b))
      return false;

   if (r != 8 || g != 8 || b != 8)
      return false;

   return true;
}

bool egl_init_context_common(
      egl_ctx_data_t *egl, EGLint *count,
      const EGLint *attrib_ptr,
      egl_accept_config_cb_t cb,
      void *display_data)
{
   EGLint i;
   EGLint matched     = 0;
   EGLConfig *configs = NULL;
   if (!egl)
      return false;

   if (!_egl_get_configs(egl->dpy, NULL, 0, count) || *count < 1)
   {
      RARCH_ERR("[EGL]: No configs to choose from.\n");
      return false;
   }

   configs = (EGLConfig*)malloc(*count * sizeof(*configs));
   if (!configs)
      return false;

   if (!_egl_choose_config(egl->dpy, attrib_ptr,
            configs, *count, &matched) || !matched)
   {
      RARCH_ERR("[EGL]: No EGL configs with appropriate attributes.\n");
      return false;
   }

   for (i = 0; i < *count; i++)
   {
      if (!cb || cb(display_data, egl->dpy, configs[i]))
      {
         egl->config = configs[i];
         break;
      }
   }

   free(configs);

   if (i == *count)
   {
      RARCH_ERR("[EGL]: No EGL config found which satifies requirements.\n");
      return false;
   }

   egl->major = g_egl_major;
   egl->minor = g_egl_minor;

   return true;
}


bool egl_init_context(egl_ctx_data_t *egl,
      EGLenum platform,
      void *display_data,
      EGLint *major, EGLint *minor,
      EGLint *count, const EGLint *attrib_ptr,
      egl_accept_config_cb_t cb)
{
   int config_index   = -1;
   EGLDisplay dpy     = get_egl_display(platform, display_data);

   if (dpy == EGL_NO_DISPLAY)
   {
      RARCH_ERR("[EGL]: Couldn't get EGL display.\n");
      return false;
   }

   egl->dpy = dpy;

   if (!egl_initialize(egl->dpy, major, minor))
      return false;

   RARCH_LOG("[EGL]: EGL version: %d.%d\n", *major, *minor);

   return egl_init_context_common(egl, count, attrib_ptr, cb,
         display_data);
}

bool egl_create_context(egl_ctx_data_t *egl, const EGLint *egl_attribs)
{
   EGLContext ctx = _egl_create_context(egl->dpy, egl->config, EGL_NO_CONTEXT,
         egl_attribs);

   if (ctx == EGL_NO_CONTEXT)
      return false;

   egl->ctx    = ctx;
   egl->hw_ctx = NULL;

   if (egl->use_hw_ctx)
   {
      egl->hw_ctx = _egl_create_context(egl->dpy, egl->config, egl->ctx,
            egl_attribs);
      RARCH_LOG("[EGL]: Created shared context: %p.\n", (void*)egl->hw_ctx);

      if (egl->hw_ctx == EGL_NO_CONTEXT)
         return false;
   }

   return true;
}

bool egl_create_surface(egl_ctx_data_t *egl, void *native_window)
{
   EGLint window_attribs[] = {
	   EGL_RENDER_BUFFER, EGL_BACK_BUFFER,
	   EGL_NONE,
   };

   egl->surf = _egl_create_window_surface(egl->dpy, egl->config, (NativeWindowType)native_window, window_attribs);

   if (egl->surf == EGL_NO_SURFACE)
      return false;

   /* Connect the context to the surface. */
   if (!_egl_make_current(egl->dpy, egl->surf, egl->surf, egl->ctx))
      return false;

   RARCH_LOG("[EGL]: Current context: %p.\n", (void*)_egl_get_current_context());

   return true;
}

bool egl_has_config(egl_ctx_data_t *egl)
{
   if (!egl->config)
   {
      RARCH_ERR("[EGL]: No EGL configurations available.\n");
      return false;
   }
   return true;
}
