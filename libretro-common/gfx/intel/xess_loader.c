/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (xess_loader.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <string.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>

#include <gfx/intel/xess_loader.h>

#if defined(_WIN64) && !defined(__WINRT__) && !defined(_XBOX)
#define XESS_HAVE_LOADER 1
#include <windows.h>
#elif defined(__linux__) && (defined(__x86_64__) || defined(__aarch64__))
#define XESS_HAVE_LOADER 1
#include <dlfcn.h>
#endif

#if defined(_WIN32)
#define XESS_LIBRARY "libxess.dll"
#else
#define XESS_LIBRARY "libxess.so"
#endif

bool xess_runtime_in(const char *dir)
{
   char path[DIR_MAX_LENGTH];
   if (string_is_empty(dir))
      return false;
   fill_pathname_join_special(path, dir, XESS_LIBRARY, sizeof(path));
   return path_is_valid(path);
}

#ifdef XESS_HAVE_LOADER

static xess_lib_t xess_lib;
static bool xess_probed;
static char xess_status[DIR_MAX_LENGTH + 128] = "not probed";

typedef void (*xess_proc_t)(void);

#if defined(_WIN32)
typedef HMODULE xess_module_t;

static xess_module_t xess_open_file(const char *path)
{
   return LoadLibraryExA(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
}

static xess_proc_t xess_sym(xess_module_t m, const char *name)
{
   FARPROC p = GetProcAddress(m, name);
   xess_proc_t fn;
   memcpy(&fn, &p, sizeof(fn));
   return fn;
}
#else
typedef void *xess_module_t;

static xess_module_t xess_open_file(const char *path)
{
   return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

static xess_proc_t xess_sym(xess_module_t m, const char *name)
{
   xess_proc_t fn;
   void *sym = dlsym(m, name);
   memcpy(&fn, &sym, sizeof(fn));
   return fn;
}
#endif

/* An entry point; a missing one is named */
#define XESS_SYM(dst, name) \
   do { \
      xess_proc_t fn = xess_sym(m, name); \
      memcpy(&(dst), &fn, sizeof(fn)); \
      if (!fn) \
         missing = name; \
   } while (0)

static void xess_resolve_common(xess_module_t m, xess_common_api_t *a,
      const char **missing_out)
{
   const char *missing = NULL;
   XESS_SYM(a->get_version,                  "xessGetVersion");
   XESS_SYM(a->get_optimal_input_resolution, "xessGetOptimalInputResolution");
   XESS_SYM(a->destroy_context,              "xessDestroyContext");
   XESS_SYM(a->set_jitter_scale,             "xessSetJitterScale");
   XESS_SYM(a->set_velocity_scale,           "xessSetVelocityScale");
   XESS_SYM(a->set_exposure_multiplier,      "xessSetExposureMultiplier");
   XESS_SYM(a->set_logging_callback,         "xessSetLoggingCallback");
   *missing_out = missing;
   /* (XeSS 1.3 on: older libraries build their pipelines at init) */
   XESS_SYM(a->get_pipeline_build_status,    "xessGetPipelineBuildStatus");
}

static bool xess_resolve_d3d12(xess_module_t m, xess_d3d12_api_t *a)
{
   const char *missing = NULL;
   XESS_SYM(a->create_context,  "xessD3D12CreateContext");
   XESS_SYM(a->build_pipelines, "xessD3D12BuildPipelines");
   XESS_SYM(a->init,            "xessD3D12Init");
   XESS_SYM(a->execute,         "xessD3D12Execute");
   return !missing;
}

static bool xess_resolve_vk(xess_module_t m, xess_vk_api_t *a)
{
   const char *missing = NULL;
   XESS_SYM(a->get_required_instance_extensions, "xessVKGetRequiredInstanceExtensions");
   XESS_SYM(a->get_required_device_extensions,   "xessVKGetRequiredDeviceExtensions");
   XESS_SYM(a->get_required_device_features,     "xessVKGetRequiredDeviceFeatures");
   XESS_SYM(a->create_context,                   "xessVKCreateContext");
   XESS_SYM(a->build_pipelines,                  "xessVKBuildPipelines");
   XESS_SYM(a->init,                             "xessVKInit");
   XESS_SYM(a->execute,                          "xessVKExecute");
   return !missing;
}

const xess_lib_t *xess_lib_get(const char *dir)
{
   if (!xess_probed)
   {
      char path[DIR_MAX_LENGTH];
      xess_module_t m;
      const char *missing = NULL;
      bool d3d12, vk;
      char version[32];
      if (string_is_empty(dir))
      {
         strlcpy(xess_status, "no XeSS directory", sizeof(xess_status));
         return NULL;
      }
      fill_pathname_join_special(path, dir, XESS_LIBRARY, sizeof(path));
      if (!path_is_valid(path) || !(m = xess_open_file(path)))
      {
         /* (not probed: the directory is a setting, tried again as set) */
         strlcpy(xess_status, "no XeSS library (" XESS_LIBRARY ") in \"", sizeof(xess_status));
         strlcat(xess_status, dir, sizeof(xess_status));
         strlcat(xess_status, "\"", sizeof(xess_status));
         return NULL;
      }
      xess_probed = true;
      xess_resolve_common(m, &xess_lib.common, &missing);
      d3d12 = xess_resolve_d3d12(m, &xess_lib.d3d12);
      vk    = xess_resolve_vk(m, &xess_lib.vk);
      strlcpy(xess_status, "XeSS library ", sizeof(xess_status));
      strlcat(xess_status, path, sizeof(xess_status));
      if (missing || !(d3d12 || vk))
      {
         strlcat(xess_status, missing ? " lacks " : ": neither Direct3D 12 nor Vulkan", sizeof(xess_status));
         if (missing)
            strlcat(xess_status, missing, sizeof(xess_status));
         memset(&xess_lib, 0, sizeof(xess_lib));
         return NULL;
      }
      if (!d3d12)
         memset(&xess_lib.d3d12, 0, sizeof(xess_lib.d3d12));
      if (!vk)
         memset(&xess_lib.vk, 0, sizeof(xess_lib.vk));
      if (xess_lib.common.get_version(&xess_lib.version) == XESS_RESULT_SUCCESS)
      {
         /* (three 16-bit numbers: fits) */
         sprintf(version, " %u.%u.%u",
               (unsigned)xess_lib.version.major, (unsigned)xess_lib.version.minor,
               (unsigned)xess_lib.version.patch);
         strlcat(xess_status, version, sizeof(xess_status));
      }
      strlcat(xess_status, d3d12 && vk ? ": Vulkan and Direct3D 12"
            : vk ? ": Vulkan only" : ": Direct3D 12 only", sizeof(xess_status));
      xess_lib.handle = (void*)m;
   }
   return xess_lib.handle ? &xess_lib : NULL;
}

const char *xess_lib_status(void)
{
   return xess_status;
}

#else

const xess_lib_t *xess_lib_get(const char *dir)
{
   (void)dir;
   return NULL;
}

const char *xess_lib_status(void)
{
   return "no XeSS on this platform";
}

#endif
