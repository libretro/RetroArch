/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (ffx_loader.c).
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

#include <string.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>

#include <gfx/amd/ffx_loader.h>

#if defined(_WIN64) && !defined(__WINRT__) && !defined(_XBOX)
#define FFX_HAVE_LOADER 1
#include <windows.h>
#elif defined(__linux__) && (defined(__x86_64__) || defined(__aarch64__))
#define FFX_HAVE_LOADER 1
#include <dlfcn.h>
#endif

/* The files, in the order tried: SDK 2's loader (with the effect
 * library beside it), then SDK 1.1's library of everything */
static const char *ffx_names_d3d12[] = {
#if defined(_WIN32)
   "amd_fidelityfx_loader_dx12.dll", "amd_fidelityfx_dx12.dll"
#else
   "libamd_fidelityfx_loader_dx12.so", "libamd_fidelityfx_dx12.so"
#endif
};
static const char *ffx_names_vulkan[] = {
#if defined(_WIN32)
   "amd_fidelityfx_vk.dll"
#else
   "libamd_fidelityfx_vk.so"
#endif
};

static unsigned ffx_names(enum ffx_api api, const char ***names)
{
   if (api == FFX_API_VULKAN)
   {
      *names = ffx_names_vulkan;
      return (unsigned)(sizeof(ffx_names_vulkan) / sizeof(ffx_names_vulkan[0]));
   }
   *names = ffx_names_d3d12;
   return (unsigned)(sizeof(ffx_names_d3d12) / sizeof(ffx_names_d3d12[0]));
}

bool ffx_runtime_in(enum ffx_api api, const char *dir)
{
   char path[DIR_MAX_LENGTH];
   const char **names;
   unsigned n, i;
   if (string_is_empty(dir) || (unsigned)api >= FFX_API_LAST)
      return false;
   n = ffx_names(api, &names);
   for (i = 0; i < n; i++)
   {
      fill_pathname_join_special(path, dir, names[i], sizeof(path));
      if (path_is_valid(path))
         return true;
   }
   return false;
}

#ifdef FFX_HAVE_LOADER

static ffx_lib_t ffx_libs[FFX_API_LAST];
static bool ffx_probed[FFX_API_LAST];
static char ffx_status[FFX_API_LAST][DIR_MAX_LENGTH + 128] = { "not probed", "not probed" };

typedef void (*ffx_proc_t)(void);

#if defined(_WIN32)
typedef HMODULE ffx_module_t;

static ffx_module_t ffx_open_file(const char *path)
{
   /* (the loader's effect libraries sit beside it: found from its own directory) */
   return LoadLibraryExA(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
}

static ffx_proc_t ffx_sym(ffx_module_t m, const char *name)
{
   FARPROC p = GetProcAddress(m, name);
   ffx_proc_t fn;
   memcpy(&fn, &p, sizeof(fn));
   return fn;
}
#else
typedef void *ffx_module_t;

static ffx_module_t ffx_open_file(const char *path)
{
   return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

static ffx_proc_t ffx_sym(ffx_module_t m, const char *name)
{
   ffx_proc_t fn;
   void *sym = dlsym(m, name);
   memcpy(&fn, &sym, sizeof(fn));
   return fn;
}
#endif

#define FFX_SYM(dst, name) \
   do { \
      ffx_proc_t fn = ffx_sym(m, name); \
      memcpy(&(dst), &fn, sizeof(fn)); \
      if (!fn) \
         missing = name; \
   } while (0)

const ffx_lib_t *ffx_lib_get(enum ffx_api api, const char *dir)
{
   if ((unsigned)api >= FFX_API_LAST)
      return NULL;
   if (!ffx_probed[api])
   {
      char path[DIR_MAX_LENGTH];
      const char **names;
      unsigned n, i;
      ffx_module_t m = NULL;
      ffx_lib_t *lib = &ffx_libs[api];
      const char *missing = NULL;
      char *status = ffx_status[api];
      const size_t status_len = sizeof(ffx_status[api]);
      if (string_is_empty(dir))
      {
         strlcpy(status, "no FSR directory", status_len);
         return NULL;
      }
      n = ffx_names(api, &names);
      for (i = 0; i < n && !m; i++)
      {
         fill_pathname_join_special(path, dir, names[i], sizeof(path));
         if (path_is_valid(path))
            m = ffx_open_file(path);
      }
      if (!m)
      {
         /* (not probed: the directory is a setting, tried again as set) */
         strlcpy(status, "no FSR library (", status_len);
         for (i = 0; i < n; i++)
         {
            strlcat(status, i ? " or " : "", status_len);
            strlcat(status, names[i], status_len);
         }
         strlcat(status, ") in \"", status_len);
         strlcat(status, dir, status_len);
         strlcat(status, "\"", status_len);
         return NULL;
      }
      ffx_probed[api] = true;
      FFX_SYM(lib->create_context,  "ffxCreateContext");
      FFX_SYM(lib->destroy_context, "ffxDestroyContext");
      FFX_SYM(lib->configure,       "ffxConfigure");
      FFX_SYM(lib->query,           "ffxQuery");
      FFX_SYM(lib->dispatch,        "ffxDispatch");
      if (missing)
      {
         strlcpy(status, "FSR library ", status_len);
         strlcat(status, path, status_len);
         strlcat(status, " lacks ", status_len);
         strlcat(status, missing, status_len);
         memset(lib, 0, sizeof(*lib));
         /* (left loaded: a library is never unloaded under a device) */
         return NULL;
      }
      lib->handle = (void*)m;
      strlcpy(status, "FSR library ", status_len);
      strlcat(status, path, status_len);
   }
   return ffx_libs[api].handle ? &ffx_libs[api] : NULL;
}

const char *ffx_lib_status(enum ffx_api api)
{
   if ((unsigned)api >= FFX_API_LAST)
      return "";
   return ffx_status[api];
}

#else

const ffx_lib_t *ffx_lib_get(enum ffx_api api, const char *dir)
{
   (void)api;
   (void)dir;
   return NULL;
}

const char *ffx_lib_status(enum ffx_api api)
{
   (void)api;
   return "no FSR on this platform";
}

#endif

unsigned ffx_lib_versions(const ffx_lib_t *lib, void *device,
      uint64_t *ids, const char **names, unsigned cap)
{
   ffx_query_versions_t q;
   uint64_t count = 0;
   if (!lib || !cap)
      return 0;
   memset(&q, 0, sizeof(q));
   q.header.type      = FFX_TYPE_GET_VERSIONS;
   q.create_desc_type = FFX_TYPE_UPSCALE_CREATE;
   q.device           = device;
   q.output_count     = &count;
   if (lib->query(NULL, &q.header) != FFX_RETURN_OK || !count)
      return 0;
   if (count > cap)
      count = cap;
   q.version_ids   = ids;
   q.version_names = names;
   if (lib->query(NULL, &q.header) != FFX_RETURN_OK)
      return 0;
   return (unsigned)count;
}
