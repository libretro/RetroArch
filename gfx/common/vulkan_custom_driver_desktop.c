/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <boolean.h>
#include <compat/strl.h>
#include <dynamic/dylib.h>
#include <encodings/utf.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <vulkan/vulkan_symbol_wrapper.h>

#include "vulkan_custom_driver.h"

#include "../../paths.h"
#include "../../verbosity.h"

/* Static, not on the stack: frames stay under 4 KiB. */
static struct
{
   struct vulkan_custom_driver_icd icd;
   char library[PATH_MAX_LENGTH]; /* of the load in progress */
   char name[PATH_MAX_LENGTH];    /* a module's file, for the log */
   dylib_t handle;                /* of the open() in progress */
   dylib_t lingering;             /* a failed library still mapped */
   void *lingering_module;
#ifdef _WIN32
   DWORD load_error;              /* GetLastError() of the last load */
#endif
   bool probed;
   bool supported;
   bool opened;                   /* a custom library was loaded */
} custom_driver_desktop_st;

/* The module holding addr: its base on Linux, its HMODULE on Windows.
 * NULL when no module does. */
static void *custom_driver_module(const void *addr)
{
#ifdef _WIN32
   HMODULE mod = NULL;
   if (!addr || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
            | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCWSTR)addr, &mod))
      return NULL;
   return (void*)mod;
#else
   Dl_info info;
   if (!addr || !dladdr(addr, &info))
      return NULL;
   return info.dli_fbase;
#endif
}

/* The file of the module holding addr, for the log. */
static const char *custom_driver_module_name(const void *addr)
{
#ifdef _WIN32
   wchar_t wname[MAX_PATH];
   char *name;
   HMODULE mod = (HMODULE)custom_driver_module(addr);
   custom_driver_desktop_st.name[0] = '\0';
   if (mod && GetModuleFileNameW(mod, wname, MAX_PATH)
         && (name = utf16_to_utf8_string_alloc(wname)))
   {
      strlcpy(custom_driver_desktop_st.name, name,
            sizeof(custom_driver_desktop_st.name));
      free(name);
   }
#else
   Dl_info info;
   custom_driver_desktop_st.name[0] = '\0';
   if (addr && dladdr(addr, &info) && info.dli_fname)
      strlcpy(custom_driver_desktop_st.name, info.dli_fname,
            sizeof(custom_driver_desktop_st.name));
#endif
   return custom_driver_desktop_st.name;
}

/* True when the module holding addr is a driver, not a layer. */
static bool custom_driver_module_is_icd(const void *addr)
{
#ifdef _WIN32
   HMODULE mod = (HMODULE)custom_driver_module(addr);
   return mod && GetProcAddress(mod, "vk_icdGetInstanceProcAddr");
#else
   Dl_info info;
   void *lib;
   bool icd = false;
   if (!addr || !dladdr(addr, &info) || !info.dli_fname)
      return false;
   if ((lib = dlopen(info.dli_fname, RTLD_NOW | RTLD_NOLOAD)))
   {
      icd = (dlsym(lib, "vk_icdGetInstanceProcAddr") != NULL);
      dlclose(lib);
   }
   return icd;
#endif
}

/* An address inside a custom library: its vk_icdGetInstanceProcAddr. */
static void *custom_driver_icd_addr(dylib_t handle)
{
   function_t fn = dylib_proc(handle, "vk_icdGetInstanceProcAddr");
   void *addr;
   memcpy(&addr, &fn, sizeof(addr));
   return addr;
}

static dylib_t custom_driver_dlopen(const char *path)
{
#ifdef _WIN32
   dylib_t lib;
   UINT mode;
   wchar_t *wpath = utf8_to_utf16_string_alloc(path);
   custom_driver_desktop_st.load_error = ERROR_NOT_ENOUGH_MEMORY;
   if (!wpath)
      return NULL;
   mode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
   /* Its own directory first, for the DLLs it links */
   lib  = (dylib_t)LoadLibraryExW(wpath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
   custom_driver_desktop_st.load_error = GetLastError();
   SetErrorMode(mode);
   free(wpath);
   return lib;
#else
   return dylib_load(path);
#endif
}

/* Closes a failed driver's library. If something else keeps it mapped, a
 * reference is held so the device check still knows its module. */
static void custom_driver_close(dylib_t handle)
{
   void *addr = custom_driver_icd_addr(handle);
#ifndef _WIN32
   Dl_info info;
#endif
   vulkan_custom_driver_session_drop(
         &vulkan_custom_driver_glue_get_ptr()->session, handle);
   dylib_close(handle);
   if (custom_driver_desktop_st.lingering)
      return;
#ifdef _WIN32
   {
      HMODULE mod = NULL;
      if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
               (LPCWSTR)addr, &mod))
         custom_driver_desktop_st.lingering = (dylib_t)mod;
   }
#else
   if (dladdr(addr, &info) && info.dli_fname)
      custom_driver_desktop_st.lingering = dlopen(info.dli_fname,
            RTLD_NOW | RTLD_NOLOAD);
#endif
   if (custom_driver_desktop_st.lingering)
      custom_driver_desktop_st.lingering_module = custom_driver_module(addr);
}

/* vulkan_custom_driver_load_t: runs on a cache miss only. */
void *vulkan_custom_driver_platform_load(void *data, const char *path,
      uint32_t key, struct vulkan_custom_driver_meta *meta,
      enum vulkan_custom_driver_msg *msg)
{
   struct vulkan_custom_driver_glue *glue =
      vulkan_custom_driver_glue_get_ptr();
   int64_t len        = 0;
   char *buf          = NULL;
   const char *reason = NULL;
   dylib_t handle;
   (void)data;
   (void)key;

   if (path_get_size(path) > VULKAN_CUSTOM_DRIVER_META_MAX)
      reason = "the .json is over 4 KiB";
   else if (!(buf = vulkan_custom_driver_glue_read(path, &len)))
      reason = "the .json is not readable";
   else if (!(reason = vulkan_custom_driver_parse_icd(buf, (size_t)len,
               &custom_driver_desktop_st.icd)))
      reason = vulkan_custom_driver_icd_library(path,
            custom_driver_desktop_st.icd.library_path,
            custom_driver_desktop_st.library,
            sizeof(custom_driver_desktop_st.library));
   free(buf);
#ifdef _WIN32
   if (!reason)
      pathname_conform_slashes_to_os(custom_driver_desktop_st.library);
#endif
   if (!reason && !path_is_valid(custom_driver_desktop_st.library))
      reason = "the library it names is missing";
   if (reason)
   {
      RARCH_WARN("[Vulkan] Custom driver \"%s\" not used: %s.\n",
            path, reason);
      *msg = VULKAN_CUSTOM_DRIVER_MSG_INVALID;
      return NULL;
   }

   /* The library's constructors run in the open. */
   vulkan_custom_driver_glue_mark(path, glue->core);
   if (!(handle = custom_driver_dlopen(custom_driver_desktop_st.library)))
   {
#ifdef _WIN32
      RARCH_WARN("[Vulkan] Custom driver \"%s\" not used: \"%s\" does not load (error %lu).\n",
            path, custom_driver_desktop_st.library,
            (unsigned long)custom_driver_desktop_st.load_error);
#else
      const char *err = dylib_error();
      RARCH_WARN("[Vulkan] Custom driver \"%s\" not used: \"%s\" does not load: %s.\n",
            path, custom_driver_desktop_st.library,
            err ? err : "no reason given");
#endif
      filestream_delete(glue->marker);
      *msg = VULKAN_CUSTOM_DRIVER_MSG_FAILED;
      return NULL;
   }
   if (!dylib_proc(handle, "vk_icdGetInstanceProcAddr"))
   {
      RARCH_WARN("[Vulkan] Custom driver \"%s\" not used: \"%s\" has no vk_icdGetInstanceProcAddr.\n",
            path, custom_driver_desktop_st.library);
      dylib_close(handle);
      filestream_delete(glue->marker);
      *msg = VULKAN_CUSTOM_DRIVER_MSG_INVALID;
      return NULL;
   }
   strlcpy(meta->library_name,
         path_basename(custom_driver_desktop_st.library),
         sizeof(meta->library_name));
   strlcpy(meta->version, custom_driver_desktop_st.icd.api_version,
         sizeof(meta->version));
   custom_driver_desktop_st.opened = true;
   return handle;
}

/* The state goes next to the config. */
bool vulkan_custom_driver_platform_dir(char *s, size_t len)
{
   const char *config = path_get(RARCH_PATH_CONFIG);
   if (string_is_empty(config))
      return false;
   fill_pathname_basedir(s, config, len);
   return true;
}

/* A .json has nothing to extract ahead. */
void vulkan_custom_driver_platform_picked(const char *path)
{
}

dylib_t vulkan_custom_driver_open(void)
{
   struct vulkan_custom_driver_request req;
   const struct vulkan_custom_driver_meta *meta;
   enum vulkan_custom_driver_msg msg;
   void *handle;

   custom_driver_desktop_st.handle = NULL;
   handle = vulkan_custom_driver_glue_open(&req, &meta, &msg);
   if (msg == VULKAN_CUSTOM_DRIVER_MSG_INVALID && req.size < 0)
      RARCH_WARN("[Vulkan] Custom driver \"%s\" not used: the .json is missing.\n",
            req.path);
   if (msg == VULKAN_CUSTOM_DRIVER_MSG_CRASHED)
      RARCH_WARN("[Vulkan] Custom driver \"%s\" not used: it crashed in an earlier start with core \"%s\".\n",
            req.path, req.core);
   if (msg == VULKAN_CUSTOM_DRIVER_MSG_UNSUPPORTED)
      RARCH_WARN("[Vulkan] Custom driver \"%s\" not used: the Vulkan loader has no VK_LUNARG_direct_driver_loading.\n",
            req.path);
   if (handle)
   {
      custom_driver_desktop_st.handle = (dylib_t)handle;
      RARCH_LOG("[Vulkan] Custom driver: \"%s\" (%s), API %s, core \"%s\".\n",
            req.path, meta->library_name, meta->version, req.core);
   }
   return (dylib_t)handle;
}

void vulkan_custom_driver_failed(void)
{
   dylib_t handle                  = custom_driver_desktop_st.handle;
   custom_driver_desktop_st.handle = NULL;
   /* Closed before the system driver's instance: the loader must not
    * find it under the system driver's name. */
   if (vulkan_custom_driver_glue_failed() && handle)
      custom_driver_close(handle);
}

/* Asks the loader the system Vulkan library loaded, once per process:
 * call it after vulkan_symbol_wrapper_load_global_symbols(). The probe
 * loads every installed driver, so it runs before any custom library
 * opens, or a bare-named one would bind to it. */
bool vulkan_custom_driver_supported(void)
{
   VkExtensionProperties *props;
   uint32_t i;
   uint32_t count = 0;
   if (custom_driver_desktop_st.probed)
      return custom_driver_desktop_st.supported;
   if (!vkEnumerateInstanceExtensionProperties)
      return false;
   custom_driver_desktop_st.probed = true;
   if (     vkEnumerateInstanceExtensionProperties(NULL, &count, NULL)
               != VK_SUCCESS
         || !count)
      return false;
   if (!(props = (VkExtensionProperties*)malloc(count * sizeof(*props))))
      return false;
   if (vkEnumerateInstanceExtensionProperties(NULL, &count, props) >= 0)
      for (i = 0; i < count && !custom_driver_desktop_st.supported; i++)
         custom_driver_desktop_st.supported = string_is_equal(
               props[i].extensionName,
               VK_LUNARG_DIRECT_DRIVER_LOADING_EXTENSION_NAME);
   free(props);
   return custom_driver_desktop_st.supported;
}

void vulkan_custom_driver_set_safe_mode(bool safe_mode)
{
   struct vulkan_custom_driver_glue *glue =
      vulkan_custom_driver_glue_get_ptr();
   /* Once: every content load parses the environment again. */
   if (!safe_mode || glue->safe_mode)
      return;
   glue->safe_mode     = true;
   glue->safe_mode_msg = true;
}

bool vulkan_custom_driver_device(function_t queue_submit)
{
   struct vulkan_custom_driver_glue *glue =
      vulkan_custom_driver_glue_get_ptr();
   enum vulkan_custom_driver_module module = VULKAN_CUSTOM_DRIVER_MODULE_UNKNOWN;
   enum vulkan_custom_driver_msg msg;
   bool current;
   bool ok;
   unsigned i;
   void *addr;
   void *mod;

   if (!glue->ready || !custom_driver_desktop_st.opened)
      return true;
   memcpy(&addr, &queue_submit, sizeof(addr));
   current = vulkan_custom_driver_session_current(&glue->session);
   if ((mod = custom_driver_module(addr)))
   {
      for (i = 0; i < VULKAN_CUSTOM_DRIVER_SLOTS; i++)
      {
         dylib_t h = (dylib_t)glue->session.slots[i].handle;
         if (h && custom_driver_module(custom_driver_icd_addr(h)) == mod)
            module = (h == custom_driver_desktop_st.handle)
               ? VULKAN_CUSTOM_DRIVER_MODULE_CURRENT
               : VULKAN_CUSTOM_DRIVER_MODULE_OTHER;
      }
      if (     module == VULKAN_CUSTOM_DRIVER_MODULE_UNKNOWN
            && custom_driver_desktop_st.lingering
            && custom_driver_desktop_st.lingering_module == mod)
         module = VULKAN_CUSTOM_DRIVER_MODULE_OTHER;
      if (     module == VULKAN_CUSTOM_DRIVER_MODULE_UNKNOWN
            && custom_driver_module_is_icd(addr))
         module = VULKAN_CUSTOM_DRIVER_MODULE_SYSTEM;
   }
   ok = vulkan_custom_driver_session_device(&glue->session, module, &msg);
   if (!ok)
      RARCH_WARN("[Vulkan] Custom driver \"%s\" not used: the device runs on \"%s\".\n",
            glue->session.current->path, custom_driver_module_name(addr));
   else if (module == VULKAN_CUSTOM_DRIVER_MODULE_CURRENT)
      RARCH_LOG("[Vulkan] Custom driver confirmed: the device runs on \"%s\".\n",
            custom_driver_module_name(addr));
   else if (module == VULKAN_CUSTOM_DRIVER_MODULE_UNKNOWN && current)
      RARCH_WARN("[Vulkan] Custom driver not confirmed: \"%s\", a layer such as MangoHud or validation, is in front of the driver.\n",
            custom_driver_module_name(addr));
   else if (msg == VULKAN_CUSTOM_DRIVER_MSG_RESTART)
      RARCH_WARN("[Vulkan] The system driver runs on custom driver library \"%s\".\n",
            custom_driver_module_name(addr));
   vulkan_custom_driver_glue_msg(msg);
   return ok;
}

void vulkan_custom_driver_reason(enum vulkan_custom_driver_msg msg)
{
   struct vulkan_custom_driver_glue *glue =
      vulkan_custom_driver_glue_get_ptr();
   if (glue->ready)
      vulkan_custom_driver_session_reason(&glue->session, msg);
}
