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

#include <stdlib.h>
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

/* A feature library's file in @dir into @path: @dll on Windows, on
 * Linux the first file named @so (a soname, versioned after it) */
static bool ngx_feature_path(const char *dir, const char *dll,
      const char *so, char *path, size_t len)
{
#if defined(_WIN32)
   (void)so;
   if (string_is_empty(dir))
      return false;
   fill_pathname_join_special(path, dir, dll, len);
   return path_is_valid(path);
#else
   bool found = false;
   struct RDIR *rdir;
   (void)dll;
   if (string_is_empty(dir))
      return false;
   if (!(rdir = retro_opendir(dir)))
      return false;
   while (!found && retro_readdir(rdir))
   {
      const char *name = retro_dirent_get_name(rdir);
      if (name && string_starts_with(name, so))
      {
         fill_pathname_join_special(path, dir, name, len);
         found = true;
      }
   }
   retro_closedir(rdir);
   return found;
#endif
}

#define NGX_DLSS_DLL   "nvngx_dlss.dll"
#define NGX_DLSS_SO    "libnvidia-ngx-dlss.so"
#define NGX_DLSSNR_DLL "nvngx_dlssnr.dll"
#define NGX_DLSSNR_SO  "libnvidia-ngx-dlssnr.so"

static bool ngx_dlss_feature_path(const char *dir, char *path, size_t len)
{
   return ngx_feature_path(dir, NGX_DLSS_DLL, NGX_DLSS_SO, path, len);
}

bool ngx_dlss_feature_in(const char *dir)
{
   char path[DIR_MAX_LENGTH];
   return ngx_dlss_feature_path(dir, path, sizeof(path));
}

bool ngx_dlssnr_feature_in(const char *dir)
{
   char path[DIR_MAX_LENGTH];
   return ngx_feature_path(dir, NGX_DLSSNR_DLL, NGX_DLSSNR_SO, path, sizeof(path));
}

uint32_t ngx_snippet_version(uint32_t snippet)
{
   /* 310.9.16 reads 0x01360910 */
   return NGX_VERSION_PACK(snippet >> 16, (snippet >> 8) & 0xffu,
         snippet & 0xffu);
}

#ifdef NGX_HAVE_LOADER

static ngx_lib_t ngx_lib;
static bool ngx_probed;
static bool ngx_optiscaler;
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

/* A REG_SZ value under HKEY_LOCAL_MACHINE */
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

static const wchar_t *const ngx_core_names[] = {
#if defined(_M_ARM64) || defined(__aarch64__)
   L"_arm64_nvngx.dll"
#else
   L"_nvngx.dll", L"nvngx.dll"
#endif
};
#define NGX_CORE_NAMES (sizeof(ngx_core_names) / sizeof(ngx_core_names[0]))

static ngx_module_t ngx_open_in(wchar_t *dir, size_t dir_chars)
{
   size_t len = wcslen(dir);
   size_t i;
   for (i = 0; i < NGX_CORE_NAMES; i++)
   {
      HMODULE m;
      if (len + wcslen(ngx_core_names[i]) + 2 > dir_chars)
         continue;
      dir[len] = L'\\';
      wcscpy(dir + len + 1, ngx_core_names[i]);
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

/* A file's version resource, through version.dll resolved at run time
 * (no import: the oldest toolchains build without it): its
 * ProductName and ProductVersion strings (@name, @version; either may
 * be NULL) and the fixed file version, packed as NGX_VERSION_PACK has
 * it (@fixed). OptiScaler's stand-in names itself there whatever file
 * name it was loaded under; the DLSS feature library's file version
 * is its version. */
static bool ngx_file_version(const wchar_t *path, wchar_t *name,
      wchar_t *version, size_t chars, uint32_t *fixed)
{
   typedef DWORD (WINAPI *size_fn)(LPCWSTR, LPDWORD);
   typedef BOOL (WINAPI *info_fn)(LPCWSTR, DWORD, DWORD, LPVOID);
   typedef BOOL (WINAPI *query_fn)(LPCVOID, LPCWSTR, LPVOID*, PUINT);
   /* (VS_FIXEDFILEINFO's leading fields, as the SDK lays them out) */
   typedef struct { DWORD signature, struc_version, ms, ls; } fixed_t;
   static const wchar_t *const keys[2] = {
      L"\\StringFileInfo\\040904b0\\ProductName",
      L"\\StringFileInfo\\040904b0\\ProductVersion"
   };
   HMODULE ver;
   size_fn get_size;
   info_fn get_info;
   query_fn query;
   DWORD size, handle = 0;
   void *data;
   wchar_t *outs[2];
   unsigned i;
   bool ok = false;
   outs[0] = name;
   outs[1] = version;
   for (i = 0; i < 2; i++)
      if (outs[i])
         outs[i][0] = 0;
   if (fixed)
      *fixed = 0;
   if (!(ver = LoadLibraryW(L"version.dll")))
      return false;
   get_size = (size_fn)GetProcAddress(ver, "GetFileVersionInfoSizeW");
   get_info = (info_fn)GetProcAddress(ver, "GetFileVersionInfoW");
   query    = (query_fn)GetProcAddress(ver, "VerQueryValueW");
   if (!get_size || !get_info || !query || !(size = get_size(path, &handle))
         || !(data = malloc(size)))
   {
      FreeLibrary(ver);
      return false;
   }
   if (get_info(path, 0, size, data))
   {
      void *value = NULL;
      UINT len    = 0;
      for (i = 0; i < 2; i++)
      {
         if (!outs[i])
            continue;
         value = NULL;
         len   = 0;
         if (query(data, keys[i], &value, &len) && value && len)
         {
            size_t n = len < chars ? len : chars - 1;
            memcpy(outs[i], value, n * sizeof(wchar_t));
            outs[i][n] = 0;
            ok = true;
         }
      }
      value = NULL;
      len   = 0;
      if (fixed && query(data, L"\\", &value, &len) && value
            && len >= sizeof(fixed_t)
            && ((const fixed_t*)value)->signature == 0xFEEF04BDu)
      {
         const fixed_t *f = (const fixed_t*)value;
         unsigned minor   = f->ms & 0xffffu;
         unsigned patch   = f->ls >> 16;
         *fixed = NGX_VERSION_PACK(f->ms >> 16,
               minor > 0xff ? 0xff : minor, patch > 0xff ? 0xff : patch);
         ok = true;
      }
   }
   free(data);
   FreeLibrary(ver);
   return ok;
}

static void ngx_note_optiscaler(HMODULE m)
{
   wchar_t path[MAX_PATH];
   wchar_t name[64], version[64];
   char narrow[64];
   if (!GetModuleFileNameW(m, path, MAX_PATH))
      return;
   if (!ngx_file_version(path, name, version, 64, NULL))
      return;
   if (wcscmp(name, L"OptiScaler"))
      return;
   ngx_optiscaler = true;
   ngx_status_add(" (OptiScaler ");
   WideCharToMultiByte(CP_UTF8, 0, version, -1, narrow, sizeof(narrow), NULL, NULL);
   ngx_status_add(narrow);
   ngx_status_add(")");
}

uint32_t ngx_dlss_feature_version(const char *dir)
{
   char path[DIR_MAX_LENGTH];
   wchar_t wpath[DIR_MAX_LENGTH];
   HMODULE m;
   uint32_t (NGX_CONV *snippet)(void);
   uint32_t version = 0;
   FARPROC p;
   if (!ngx_dlss_feature_path(dir, path, sizeof(path)))
      return 0;
   if (!ngx_utf8_to_wide(wpath, DIR_MAX_LENGTH, path))
      return 0;
   /* The file version: what the file's properties show */
   if (ngx_file_version(wpath, NULL, NULL, 0, &version) && version)
      return version;
   if (!(m = LoadLibraryExW(wpath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH)))
      return 0;
   p = GetProcAddress(m, "NVSDK_NGX_GetSnippetVersion");
   memcpy(&snippet, &p, sizeof(snippet));
   return snippet ? ngx_snippet_version(snippet()) : 0;
}

uint32_t ngx_dlssnr_feature_version(const char *dir)
{
   char path[DIR_MAX_LENGTH];
   wchar_t wpath[DIR_MAX_LENGTH];
   uint32_t version = 0;
   /* (the file version alone: the library is far too big to load
    * for a version) */
   if (     !ngx_feature_path(dir, NGX_DLSSNR_DLL, NGX_DLSSNR_SO, path, sizeof(path))
         || !ngx_utf8_to_wide(wpath, DIR_MAX_LENGTH, path)
         || !ngx_file_version(wpath, NULL, NULL, 0, &version))
      return 0;
   return version;
}

/* Where the core library may be, in the order it is looked for: the
 * executable's directory (a copy put there wins), then NGXPath under
 * the display driver's service key (declarative installs), then the
 * legacy FullPath Wine and Proton set. @which from 0; false past the
 * last, or where that one is not set. */
static bool ngx_core_dir(unsigned which, wchar_t *dir)
{
   switch (which)
   {
      case 0:
      {
         DWORD n = GetModuleFileNameW(NULL, dir, MAX_PATH);
         wchar_t *slash;
         if (!n || n >= MAX_PATH || !(slash = wcsrchr(dir, L'\\')))
            return false;
         *slash = 0;
         return true;
      }
      case 1:
         return ngx_registry_dir(dir, MAX_PATH * sizeof(wchar_t),
               L"System\\CurrentControlSet\\Services\\nvlddmkm\\Parameters\\NGXCore",
               L"NGXPath");
      case 2:
         return ngx_registry_dir(dir, MAX_PATH * sizeof(wchar_t),
               L"System\\CurrentControlSet\\Services\\nvlddmkm\\NGXCore",
               L"NGXPath");
      case 3:
         return ngx_registry_dir(dir, MAX_PATH * sizeof(wchar_t),
               L"SOFTWARE\\NVIDIA Corporation\\Global\\NGXCore",
               L"FullPath");
      default:
         break;
   }
   return false;
}
#define NGX_CORE_DIRS 4

static ngx_module_t ngx_open(void)
{
   wchar_t dir[MAX_PATH + 32];
   HMODULE m = NULL;
   unsigned which;
   for (which = 0; !m && which < NGX_CORE_DIRS; which++)
      if (ngx_core_dir(which, dir))
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

/* The file ngx_open would load, into @path (MAX_PATH + 32), without
 * loading it: a stand-in such as OptiScaler hooks the process as soon
 * as it is loaded. The library search is taken as SearchPath has it. */
static bool ngx_find(wchar_t *path)
{
   unsigned which, i;
   for (which = 0; which < NGX_CORE_DIRS; which++)
   {
      size_t len;
      if (!ngx_core_dir(which, path))
         continue;
      len = wcslen(path);
      for (i = 0; i < NGX_CORE_NAMES; i++)
      {
         DWORD attr;
         if (len + wcslen(ngx_core_names[i]) + 2 > MAX_PATH)
            continue;
         path[len] = L'\\';
         wcscpy(path + len + 1, ngx_core_names[i]);
         attr = GetFileAttributesW(path);
         if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
            return true;
      }
   }
   for (i = 0; i < NGX_CORE_NAMES; i++)
   {
      DWORD n = SearchPathW(NULL, ngx_core_names[i], NULL, MAX_PATH, path, NULL);
      if (n && n < MAX_PATH)
         return true;
   }
   return false;
}

static HMODULE ngx_stand_in;

static bool ngx_stand_in_path(const char *dir, wchar_t *wpath)
{
   char path[DIR_MAX_LENGTH];
   DWORD attr;
   if (!dir || !*dir)
      return false;
   fill_pathname_join_special(path, dir, NGX_STAND_IN_NAME, sizeof(path));
   if (!ngx_utf8_to_wide(wpath, DIR_MAX_LENGTH, path))
      return false;
   attr = GetFileAttributesW(wpath);
   return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool ngx_stand_in_in(const char *dir)
{
   bool found;
   wchar_t *wpath = (wchar_t*)malloc(DIR_MAX_LENGTH * sizeof(wchar_t));
   if (!wpath)
      return false;
   found = ngx_stand_in_path(dir, wpath);
   free(wpath);
   return found;
}

bool ngx_lib_load_stand_in(const char *dir)
{
   wchar_t *wpath;
   if (ngx_stand_in)
      return true;
   /* it has to be in before the core library is, to hook its load */
   if (ngx_probed || !(wpath = (wchar_t*)malloc(DIR_MAX_LENGTH * sizeof(wchar_t))))
      return false;
   if (ngx_stand_in_path(dir, wpath))
      ngx_stand_in = LoadLibraryExW(wpath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
   free(wpath);
   return ngx_stand_in != NULL;
}

/* GetModuleHandleExW's GET_MODULE_HANDLE_EX_FLAG_PIN (XP and up) */
#define NGX_MODULE_PIN 0x00000001u
typedef BOOL (WINAPI *ngx_get_module_ex_t)(DWORD, LPCWSTR, HMODULE*);

void ngx_lib_keep_features(const char *dir)
{
   static const wchar_t *names[2] = { L"nvngx_dlss.dll", L"nvngx_dlssnr.dll" };
   static bool kept[2];
   ngx_get_module_ex_t get_module = NULL;
   HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
   FARPROC p;
   unsigned i;
   (void)dir;
   if (!kernel || !(p = GetProcAddress(kernel, "GetModuleHandleExW")))
      return;
   memcpy(&get_module, &p, sizeof(get_module));
   /* (by name: whichever copy NGX loaded, the directory's or another) */
   for (i = 0; i < 2; i++)
   {
      HMODULE m;
      if (!kept[i] && get_module(NGX_MODULE_PIN, names[i], &m))
         kept[i] = true;
   }
}

unsigned ngx_lib_present(void)
{
   wchar_t path[MAX_PATH + 32];
   wchar_t name[64];
   unsigned present;
   if (ngx_lib.handle)
      return NGX_PRESENT_FOUND | (ngx_optiscaler ? NGX_PRESENT_OPTISCALER : 0);
   if (!ngx_find(path))
      return 0;
   present = NGX_PRESENT_FOUND;
   if (ngx_file_version(path, name, NULL, 64, NULL) && !wcscmp(name, L"OptiScaler"))
      present |= NGX_PRESENT_OPTISCALER;
   return present;
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

static void ngx_note_optiscaler(ngx_module_t m)
{
   (void)m;
}

/* A feature library's file name: <soname>.<major>.<minor>.<patch> */
static uint32_t ngx_so_version(const char *path)
{
   const char *dot = strstr(path_basename(path), ".so.");
   if (dot)
   {
      unsigned v[3] = { 0, 0, 0 };
      unsigned i    = 0;
      const char *c = dot + 4;
      for (; *c && i < 3; c++)
      {
         if (*c == '.')
            i++;
         else if (*c >= '0' && *c <= '9' && v[i] < 0x10000u)
            v[i] = v[i] * 10 + (unsigned)(*c - '0');
         else
            break;
      }
      if (!*c && v[0] && v[0] < 0x10000u)
         return NGX_VERSION_PACK(v[0], v[1] > 0xff ? 0xff : v[1],
               v[2] > 0xff ? 0xff : v[2]);
   }
   return 0;
}

uint32_t ngx_dlssnr_feature_version(const char *dir)
{
   char path[DIR_MAX_LENGTH];
   if (!ngx_feature_path(dir, NGX_DLSSNR_DLL, NGX_DLSSNR_SO, path, sizeof(path)))
      return 0;
   return ngx_so_version(path);
}

uint32_t ngx_dlss_feature_version(const char *dir)
{
   char path[DIR_MAX_LENGTH];
   void *m;
   void *sym;
   uint32_t (*snippet)(void);
   uint32_t version;
   if (!ngx_dlss_feature_path(dir, path, sizeof(path)))
      return 0;
   if ((version = ngx_so_version(path)))
      return version;
   if (!(m = dlopen(path, RTLD_NOW | RTLD_LOCAL)))
      return 0;
   sym = dlsym(m, "NVSDK_NGX_GetSnippetVersion");
   memcpy(&snippet, &sym, sizeof(snippet));
   return snippet ? ngx_snippet_version(snippet()) : 0;
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

/* (the driver's own library, found only by loading it) */
unsigned ngx_lib_present(void)
{
   return ngx_lib_get() ? NGX_PRESENT_FOUND : 0;
}

bool ngx_stand_in_in(const char *dir)
{
   (void)dir;
   return false;
}

bool ngx_lib_load_stand_in(const char *dir)
{
   (void)dir;
   return false;
}

void ngx_lib_keep_features(const char *dir)
{
#if defined(RTLD_NOLOAD) && defined(RTLD_NODELETE)
   static bool kept[2];
   char path[DIR_MAX_LENGTH];
   /* (a reference that is never closed, on an object already in) */
   if (     !kept[0]
         && ngx_dlss_feature_path(dir, path, sizeof(path))
         && dlopen(path, RTLD_NOW | RTLD_LOCAL | RTLD_NOLOAD | RTLD_NODELETE))
      kept[0] = true;
   if (     !kept[1]
         && ngx_feature_path(dir, NGX_DLSSNR_DLL, NGX_DLSSNR_SO, path, sizeof(path))
         && dlopen(path, RTLD_NOW | RTLD_LOCAL | RTLD_NOLOAD | RTLD_NODELETE))
      kept[1] = true;
#else
   (void)dir;
#endif
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
   /* the deprecated call or the two that replaced it, either: OptiScaler
    * has only the newer pair */
   NGX_SYM_OPT(a->required_extensions, "NVSDK_NGX_VULKAN_RequiredExtensions");
   NGX_SYM_OPT(a->get_feature_instance_extension_requirements,
         "NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements");
   NGX_SYM_OPT(a->get_feature_device_extension_requirements,
         "NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements");
   if (!a->required_extensions && !(a->get_feature_instance_extension_requirements
            && a->get_feature_device_extension_requirements))
   {
      complete = false;
      ngx_status_add(complete_first ? " lacks " : ", ");
      ngx_status_add("NVSDK_NGX_VULKAN_RequiredExtensions");
      complete_first = false;
   }
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
      ngx_note_optiscaler(m);
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

bool ngx_lib_is_optiscaler(void)
{
   return ngx_optiscaler;
}

#else

uint32_t ngx_dlss_feature_version(const char *dir)
{
   (void)dir;
   return 0;
}

uint32_t ngx_dlssnr_feature_version(const char *dir)
{
   (void)dir;
   return 0;
}

bool ngx_lib_is_optiscaler(void)
{
   return false;
}

unsigned ngx_lib_present(void)
{
   return 0;
}

bool ngx_stand_in_in(const char *dir)
{
   (void)dir;
   return false;
}

bool ngx_lib_load_stand_in(const char *dir)
{
   (void)dir;
   return false;
}

void ngx_lib_keep_features(const char *dir)
{
   (void)dir;
}

const ngx_lib_t *ngx_lib_get(void)
{
   return NULL;
}

const char *ngx_lib_status(void)
{
   return "no NGX on this platform";
}

#endif

bool ngx_params_from_optiscaler(const ngx_lib_t *lib, ngx_parameter_t *p)
{
   int v = 0;
   if (!lib || !p || !lib->param.get_i)
      return false;
   return !NGX_FAILED(lib->param.get_i(p, "OptiScaler", &v)) && v != 0;
}
