/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (ngx_loader.c).
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

#include <encodings/utf.h>
#include <file/file_path.h>
#include <retro_dirent.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>

#include <gfx/nvidia/ngx_loader.h>

#if defined(_WIN64) && !defined(__WINRT__) && !defined(_XBOX)
#define NGX_HAVE_LOADER 1
#include <windows.h>
#elif defined(__linux__) && (defined(__x86_64__) || defined(__aarch64__))
#define NGX_HAVE_LOADER 1
#include <dlfcn.h>
#endif

bool ngx_utf8_to_wide(wchar_t *out, size_t out_chars, const char *utf8)
{
   size_t i = 0;
   if (!out_chars)
      return false;
   while (*utf8)
   {
      const char *at = utf8;
      uint32_t c     = utf8_walk(&utf8);
#if defined(_WIN32)
      if (c >= 0x10000)
      {
         if (i + 2 >= out_chars)
         {
            utf8 = at;
            break;
         }
         c     -= 0x10000;
         out[i++] = (wchar_t)(0xD800 | (c >> 10));
         out[i++] = (wchar_t)(0xDC00 | (c & 0x3FF));
         continue;
      }
#endif
      if (i + 1 >= out_chars)
      {
         utf8 = at;
         break;
      }
      out[i++] = (wchar_t)c;
   }
   out[i] = 0;
   if (*utf8)
   {
      out[0] = 0;
      return false;
   }
   return true;
}

bool ngx_dlss_feature_in(const char *dir)
{
#if defined(_WIN32)
   char path[DIR_MAX_LENGTH];
   if (string_is_empty(dir))
      return false;
   fill_pathname_join_special(path, dir, "nvngx_dlss.dll", sizeof(path));
   return path_is_valid(path);
#else
   bool found = false;
   struct RDIR *rdir;
   if (string_is_empty(dir))
      return false;
   if (!(rdir = retro_opendir(dir)))
      return false;
   while (!found && retro_readdir(rdir))
   {
      const char *name = retro_dirent_get_name(rdir);
      found = name && string_starts_with(name, "libnvidia-ngx-dlss.so");
   }
   retro_closedir(rdir);
   return found;
#endif
}

#ifdef NGX_HAVE_LOADER

static ngx_lib_t ngx_lib;
static bool ngx_probed;
static char ngx_status[512] = "not probed";
static char ngx_where[DIR_MAX_LENGTH];

static void ngx_status_set(const char *s)
{
   strlcpy(ngx_status, s, sizeof(ngx_status));
}
static void ngx_status_add(const char *s)
{
   strlcat(ngx_status, s, sizeof(ngx_status));
}

typedef void (*ngx_proc_t)(void);

#if defined(_WIN32)
typedef HMODULE ngx_module_t;

static ngx_proc_t ngx_sym(ngx_module_t m, const char *name)
{
   return (ngx_proc_t)GetProcAddress(m, name);
}

/* NGXPath under the display driver's service key (declarative
 * installs), then the legacy FullPath Wine and Proton set */
static bool ngx_registry_dir(wchar_t *out, DWORD out_bytes,
      const wchar_t *key, const wchar_t *value)
{
   HKEY hkey;
   DWORD type = 0;
   DWORD len  = out_bytes - sizeof(wchar_t);
   LONG res;
   if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, key, 0, KEY_READ, &hkey)
         != ERROR_SUCCESS)
      return false;
   res = RegQueryValueExW(hkey, value, NULL, &type, (LPBYTE)out, &len);
   RegCloseKey(hkey);
   if (res != ERROR_SUCCESS || type != REG_SZ || len < sizeof(wchar_t))
      return false;
   out[len / sizeof(wchar_t)] = 0;
   return true;
}

static ngx_module_t ngx_open_in(wchar_t *dir, size_t dir_chars)
{
   static const wchar_t *const names[] = {
#if defined(_M_ARM64) || defined(__aarch64__)
      L"\\_arm64_nvngx.dll"
#else
      L"\\_nvngx.dll", L"\\nvngx.dll"
#endif
   };
   size_t len = wcslen(dir);
   size_t i;
   for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
   {
      HMODULE m;
      if (len + wcslen(names[i]) + 1 > dir_chars)
         continue;
      wcscpy(dir + len, names[i]);
      m        = LoadLibraryW(dir);
      if (m)
      {
         WideCharToMultiByte(CP_UTF8, 0, dir, -1, ngx_where, sizeof(ngx_where), NULL, NULL);
         dir[len] = 0;
         return m;
      }
      dir[len] = 0;
   }
   return NULL;
}

static ngx_module_t ngx_open(void)
{
   wchar_t dir[MAX_PATH + 32];
   HMODULE m = NULL;
   DWORD n   = GetModuleFileNameW(NULL, dir, MAX_PATH);
   if (n && n < MAX_PATH)
   {
      wchar_t *slash = wcsrchr(dir, L'\\');
      if (slash)
      {
         *slash = 0;
         m      = ngx_open_in(dir, MAX_PATH);
      }
   }
   if (!m && (
            ngx_registry_dir(dir, MAX_PATH * sizeof(wchar_t),
               L"System\\CurrentControlSet\\Services\\nvlddmkm\\Parameters\\NGXCore",
               L"NGXPath")
         || ngx_registry_dir(dir, MAX_PATH * sizeof(wchar_t),
               L"System\\CurrentControlSet\\Services\\nvlddmkm\\NGXCore",
               L"NGXPath")
         || ngx_registry_dir(dir, MAX_PATH * sizeof(wchar_t),
               L"SOFTWARE\\NVIDIA Corporation\\Global\\NGXCore",
               L"FullPath")))
      m = ngx_open_in(dir, MAX_PATH);
   /* the driver installs it in System32 too: the library search */
   if (!m)
      m = LoadLibraryW(L"_nvngx.dll");
   if (!m)
      m = LoadLibraryW(L"nvngx.dll");
   if (m && !ngx_where[0])
   {
      wchar_t path[MAX_PATH];
      if (GetModuleFileNameW(m, path, MAX_PATH))
         WideCharToMultiByte(CP_UTF8, 0, path, -1, ngx_where, sizeof(ngx_where), NULL, NULL);
   }
   if (!m)
      ngx_status_set("no NGX core library: _nvngx.dll/nvngx.dll not beside the executable, "
            "not where the registry's NGXCore NGXPath/FullPath points, not on the library search path (System32)");
   return m;
}
#else
typedef void *ngx_module_t;

static ngx_proc_t ngx_sym(ngx_module_t m, const char *name)
{
   ngx_proc_t p;
   void *sym = dlsym(m, name);
   memcpy(&p, &sym, sizeof(p));
   return p;
}

static ngx_module_t ngx_open(void)
{
   void *m = dlopen("libnvidia-ngx.so.1", RTLD_NOW | RTLD_LOCAL);
   if (m)
      strlcpy(ngx_where, "libnvidia-ngx.so.1", sizeof(ngx_where));
   else
      ngx_status_set("no NGX core library: libnvidia-ngx.so.1 could not be loaded");
   return m;
}
#endif

#define NGX_SYM_OPT(dst, name) \
   do { \
      ngx_proc_t fn = ngx_sym(m, name); \
      memcpy(&(dst), &fn, sizeof(fn)); \
   } while (0)

#define NGX_SYM(dst, name) \
   do { \
      ngx_proc_t fn = ngx_sym(m, name); \
      memcpy(&(dst), &fn, sizeof(fn)); \
      if (!fn) \
      { \
         complete = false; \
         ngx_status_add(complete_first ? " lacks " : ", "); \
         ngx_status_add(name); \
         complete_first = false; \
      } \
   } while (0)

/* NVSDK_NGX_Parameter is a C++ interface: the core library exports no
 * accessors, the SDK's static library calls the object's virtual
 * methods. So does this, through the slots the SDK's static libraries
 * use (read from them): on MSVC the overloads of Set and of Get sit in
 * the vtable in the reverse of their declared order, on the Itanium
 * ABI in that order. Set(ull, f, d, ui, i, ID3D11Resource*,
 * ID3D12Resource*, void*), Get(the same, through pointers), Reset. The
 * methods take the object first, as a C call does on both ABIs. */
#if defined(_MSC_VER) || defined(_WIN32)
#define NGX_SLOT_SET(declared)   (7 - (declared))
#define NGX_SLOT_GET(declared)   (15 - (declared))
#else
#define NGX_SLOT_SET(declared)   (declared)
#define NGX_SLOT_GET(declared)   (8 + (declared))
#endif

typedef void (NGX_CONV *ngx_method_t)(void);

static ngx_method_t ngx_param_slot(ngx_parameter_t *p, unsigned slot)
{
   ngx_method_t m;
   const ngx_method_t *const *vtable = (const ngx_method_t *const *)p;
   memcpy(&m, &(*vtable)[slot], sizeof(m));
   return m;
}

#define NGX_PARAM_SET(suffix, type, declared) \
   static void NGX_CONV ngx_param_set_##suffix(ngx_parameter_t *p, const char *name, type v) \
   { \
      void (NGX_CONV *fn)(ngx_parameter_t*, const char*, type); \
      ngx_method_t m = ngx_param_slot(p, NGX_SLOT_SET(declared)); \
      memcpy(&fn, &m, sizeof(fn)); \
      fn(p, name, v); \
   }
#define NGX_PARAM_GET(suffix, type, declared) \
   static ngx_result_t NGX_CONV ngx_param_get_##suffix(ngx_parameter_t *p, const char *name, type *v) \
   { \
      ngx_result_t (NGX_CONV *fn)(ngx_parameter_t*, const char*, type*); \
      ngx_method_t m = ngx_param_slot(p, NGX_SLOT_GET(declared)); \
      memcpy(&fn, &m, sizeof(fn)); \
      return fn(p, name, v); \
   }

NGX_PARAM_SET(ull, uint64_t, 0)
NGX_PARAM_SET(f, float, 1)
NGX_PARAM_SET(d, double, 2)
NGX_PARAM_SET(ui, unsigned, 3)
NGX_PARAM_SET(i, int, 4)
NGX_PARAM_SET(d3d11_resource, void *, 5)
NGX_PARAM_SET(d3d12_resource, void *, 6)
NGX_PARAM_SET(void_pointer, void *, 7)
NGX_PARAM_GET(ull, uint64_t, 0)
NGX_PARAM_GET(f, float, 1)
NGX_PARAM_GET(d, double, 2)
NGX_PARAM_GET(ui, unsigned, 3)
NGX_PARAM_GET(i, int, 4)
NGX_PARAM_GET(d3d11_resource, void *, 5)
NGX_PARAM_GET(d3d12_resource, void *, 6)
NGX_PARAM_GET(void_pointer, void *, 7)

static void ngx_param_api_init(ngx_param_api_t *p)
{
   p->set_ull            = ngx_param_set_ull;
   p->set_f              = ngx_param_set_f;
   p->set_d              = ngx_param_set_d;
   p->set_ui             = ngx_param_set_ui;
   p->set_i              = ngx_param_set_i;
   p->set_d3d11_resource = ngx_param_set_d3d11_resource;
   p->set_d3d12_resource = ngx_param_set_d3d12_resource;
   p->set_void_pointer   = ngx_param_set_void_pointer;
   p->get_ull            = ngx_param_get_ull;
   p->get_f              = ngx_param_get_f;
   p->get_d              = ngx_param_get_d;
   p->get_ui             = ngx_param_get_ui;
   p->get_i              = ngx_param_get_i;
   p->get_d3d11_resource = ngx_param_get_d3d11_resource;
   p->get_d3d12_resource = ngx_param_get_d3d12_resource;
   p->get_void_pointer   = ngx_param_get_void_pointer;
}

static bool ngx_resolve_d3d12(ngx_module_t m, ngx_d3d12_api_t *a)
{
   bool complete = true;
   bool complete_first = true;
   NGX_SYM(a->init_project_id, "NVSDK_NGX_D3D12_Init_ProjectID");
   NGX_SYM(a->shutdown1, "NVSDK_NGX_D3D12_Shutdown1");
   NGX_SYM(a->get_capability_parameters, "NVSDK_NGX_D3D12_GetCapabilityParameters");
   NGX_SYM(a->allocate_parameters, "NVSDK_NGX_D3D12_AllocateParameters");
   NGX_SYM(a->destroy_parameters, "NVSDK_NGX_D3D12_DestroyParameters");
   NGX_SYM(a->get_feature_requirements, "NVSDK_NGX_D3D12_GetFeatureRequirements");
   NGX_SYM(a->create_feature, "NVSDK_NGX_D3D12_CreateFeature");
   NGX_SYM(a->release_feature, "NVSDK_NGX_D3D12_ReleaseFeature");
   NGX_SYM(a->evaluate_feature, "NVSDK_NGX_D3D12_EvaluateFeature");
   return complete;
}

static bool ngx_resolve_vulkan(ngx_module_t m, ngx_vulkan_api_t *a)
{
   bool complete = true;
   bool complete_first = true;
   NGX_SYM(a->required_extensions, "NVSDK_NGX_VULKAN_RequiredExtensions");
   NGX_SYM(a->get_feature_instance_extension_requirements,
         "NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements");
   NGX_SYM(a->get_feature_device_extension_requirements,
         "NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements");
   NGX_SYM(a->get_feature_requirements, "NVSDK_NGX_VULKAN_GetFeatureRequirements");
   NGX_SYM(a->init_project_id, "NVSDK_NGX_VULKAN_Init_ProjectID");
   NGX_SYM_OPT(a->init_project_id_ext, "NVSDK_NGX_VULKAN_Init_ProjectID_Ext");
   NGX_SYM(a->shutdown1, "NVSDK_NGX_VULKAN_Shutdown1");
   NGX_SYM(a->get_capability_parameters, "NVSDK_NGX_VULKAN_GetCapabilityParameters");
   NGX_SYM(a->allocate_parameters, "NVSDK_NGX_VULKAN_AllocateParameters");
   NGX_SYM(a->destroy_parameters, "NVSDK_NGX_VULKAN_DestroyParameters");
   NGX_SYM(a->create_feature1, "NVSDK_NGX_VULKAN_CreateFeature1");
   NGX_SYM(a->release_feature, "NVSDK_NGX_VULKAN_ReleaseFeature");
   NGX_SYM(a->evaluate_feature, "NVSDK_NGX_VULKAN_EvaluateFeature");
   return complete;
}

const ngx_lib_t *ngx_lib_get(void)
{
   if (!ngx_probed)
   {
      ngx_module_t m;
      bool d3d12, vulkan;
      ngx_probed = true;
      if (!(m = ngx_open()))
         return NULL;
      ngx_status_set("NGX core library ");
      ngx_status_add(ngx_where);
      d3d12  = ngx_resolve_d3d12(m, &ngx_lib.d3d12);
      vulkan = ngx_resolve_vulkan(m, &ngx_lib.vulkan);
      ngx_param_api_init(&ngx_lib.param);
      if (!(d3d12 || vulkan))
      {
         ngx_status_add(": unusable");
         memset(&ngx_lib, 0, sizeof(ngx_lib));
         return NULL;
      }
      ngx_status_add(d3d12 && vulkan ? ": Vulkan and Direct3D 12"
            : vulkan ? ": Vulkan only" : ": Direct3D 12 only");
      if (!d3d12)
         memset(&ngx_lib.d3d12, 0, sizeof(ngx_lib.d3d12));
      if (!vulkan)
         memset(&ngx_lib.vulkan, 0, sizeof(ngx_lib.vulkan));
      ngx_lib.handle = (void*)m;
   }
   return ngx_lib.handle ? &ngx_lib : NULL;
}

const char *ngx_lib_status(void)
{
   return ngx_status;
}

#else

const ngx_lib_t *ngx_lib_get(void)
{
   return NULL;
}

const char *ngx_lib_status(void)
{
   return "no NGX on this platform";
}

#endif
