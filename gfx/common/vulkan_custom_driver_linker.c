/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later
 *
 * Parts of this file follow libadrenotools and liblinkernsbypass:
 *
 * BSD 2-Clause License
 *
 * Copyright (c) 2021, Billy Laws
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

/* Built twice: into libretroarch-activity.so, and with
 * VULKAN_CUSTOM_DRIVER_HOOK as libvulkan_driver_hook.so, which a private
 * copy of the system Vulkan loader calls to open its HAL driver. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef VULKAN_CUSTOM_DRIVER_HOOK
#include "vulkan_custom_driver.h"

/* ELF64 fields, little-endian, read byte by byte: no alignment needed */
static uint64_t vkcd_rd(const uint8_t *p, unsigned n)
{
   uint64_t v = 0;
   while (n--)
      v = (v << 8) | p[n];
   return v;
}

bool vulkan_custom_driver_patch_soname(uint8_t *elf, size_t len,
      const char *patch)
{
   uint64_t shoff;
   unsigned shentsize, shnum, i;

   if (     len < 64
         || memcmp(elf, "\177ELF", 4)
         || elf[4] != 2  /* ELFCLASS64 */
         || elf[5] != 1) /* ELFDATA2LSB */
      return false;
   shoff     = vkcd_rd(elf + 40, 8);
   shentsize = (unsigned)vkcd_rd(elf + 58, 2);
   shnum     = (unsigned)vkcd_rd(elf + 60, 2);
   if (     shentsize < 64
         || shoff > len
         || (uint64_t)shnum * shentsize > len - shoff)
      return false;

   for (i = 0; i < shnum; i++)
   {
      const uint8_t *sh = elf + shoff + (uint64_t)i * shentsize;
      const uint8_t *str_sh;
      uint64_t off, size, entsize, str_off, str_size, k;
      unsigned link;

      if (vkcd_rd(sh + 4, 4) != 6) /* SHT_DYNAMIC */
         continue;
      off     = vkcd_rd(sh + 24, 8);
      size    = vkcd_rd(sh + 32, 8);
      link    = (unsigned)vkcd_rd(sh + 40, 4);
      entsize = vkcd_rd(sh + 56, 8);
      if (     link >= shnum
            || entsize < 16
            || off > len
            || size > len - off)
         return false;
      str_sh   = elf + shoff + (uint64_t)link * shentsize;
      str_off  = vkcd_rd(str_sh + 24, 8);
      str_size = vkcd_rd(str_sh + 32, 8);
      if (str_off > len || str_size > len - str_off)
         return false;

      for (k = 0; k + entsize <= size; k += entsize)
      {
         const uint8_t *dyn = elf + off + k;
         uint64_t name, room, j;
         char *soname;

         if (vkcd_rd(dyn, 8) != 14) /* DT_SONAME */
            continue;
         name = vkcd_rd(dyn + 8, 8);
         if (name >= str_size)
            return false;
         soname = (char*)elf + str_off + name;
         room   = str_size - name;
         for (j = 0; j < room && soname[j] && patch[j]; j++)
            soname[j] = patch[j];
         return true;
      }
   }
   return false;
}
#endif

#if defined(__ANDROID__)
#include <dlfcn.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <android/api-level.h>
#include <android/dlext.h>
#include <android/log.h>
#ifdef VULKAN_CUSTOM_DRIVER_HOOK
#include <stdbool.h>
#endif

#define VKCD_NS_SHARED 2 /* ANDROID_NAMESPACE_TYPE_SHARED */

typedef void *(*vkcd_dlopen_ext_t)(const char *name, int flags,
      const android_dlextinfo *info);
typedef struct android_namespace_t *(*vkcd_create_ns_t)(const char *name,
      const char *ld_library_path, const char *default_library_path,
      uint64_t type, const char *permitted_when_isolated_path,
      struct android_namespace_t *parent, const void *caller);
typedef struct android_namespace_t *(*vkcd_exported_ns_t)(const char *name);
typedef bool (*vkcd_link_ns_t)(struct android_namespace_t *from,
      struct android_namespace_t *to, const char *shared_libs);
typedef bool (*vkcd_link_all_t)(struct android_namespace_t *from,
      struct android_namespace_t *to);

/* Filled by libretroarch-activity.so; the hook keeps it for good */
struct vkcd_hook_params
{
   vkcd_dlopen_ext_t dlopen_ext; /* libdl's, never the hook's own */
   vkcd_create_ns_t create_ns;
   vkcd_exported_ns_t exported_ns;
   vkcd_link_ns_t link_ns;
   char hook_dir[PATH_MAX];
   char driver_dir[PATH_MAX];
   char driver_name[PATH_MAX];
};

#ifndef VULKAN_CUSTOM_DRIVER_HOOK
typedef void *(*vkcd_loader_dlopen_t)(const char *name, int flags,
      const void *caller);
typedef void (*vkcd_hook_init_t)(const struct vkcd_hook_params *params);

static struct
{
   vkcd_create_ns_t create_ns;
   vkcd_exported_ns_t exported_ns;
   vkcd_link_ns_t link_ns;
   vkcd_link_all_t link_all;
   struct android_namespace_t *default_ns;
   unsigned copies;
   bool tried;
   bool ready;
} vkcd_st;

static void *vkcd_page(const void *p)
{
   return (void*)((uintptr_t)p & ~(uintptr_t)(sysconf(_SC_PAGESIZE) - 1));
}

/* Finds the linker's own namespace functions, which apps may not call:
 * dlopen passes its return address to __loader_dlopen, and calling that
 * with dlopen as the caller opens libraries as the linker itself. */
static bool vkcd_resolve(void)
{
   const uint32_t *insn = (const uint32_t*)(const void*)&dlopen;
   long page            = sysconf(_SC_PAGESIZE);
   vkcd_loader_dlopen_t loader_dlopen;
   void *ld;
   void *libdl;
   long off;
   unsigned i;

   if (vkcd_st.tried)
      return vkcd_st.ready;
   vkcd_st.tried = true;
   if (android_get_device_api_level() < 28)
      return false;

   /* Some devices map system code execute-only */
   mprotect(vkcd_page(insn), page, PROT_READ | PROT_EXEC);
   for (i = 0; i < 64; i++)
      if ((insn[i] >> 26) == 0x25) /* BL */
         break;
   if (i == 64)
      return false;
   off = (long)(insn[i] & 0x3ffffff);
   if (off & 0x2000000)
      off -= 0x4000000;
   loader_dlopen = (vkcd_loader_dlopen_t)(void*)(insn + i + off);
   /* No BTI: __loader_dlopen is not an indirect branch target */
   if (mprotect(vkcd_page((const void*)loader_dlopen), page,
         PROT_READ | PROT_EXEC))
      return false;

   if (     !(ld    = loader_dlopen("ld-android.so", RTLD_LAZY,
               (const void*)&dlopen))
         || !(libdl = loader_dlopen("libdl_android.so", RTLD_LAZY,
               (const void*)&dlopen)))
      return false;
   vkcd_st.link_all    = (vkcd_link_all_t)dlsym(ld,
         "__loader_android_link_namespaces_all_libs");
   vkcd_st.link_ns     = (vkcd_link_ns_t)dlsym(ld,
         "__loader_android_link_namespaces");
   vkcd_st.create_ns   = (vkcd_create_ns_t)dlsym(libdl,
         "__loader_android_create_namespace");
   vkcd_st.exported_ns = (vkcd_exported_ns_t)dlsym(libdl,
         "__loader_android_get_exported_namespace");
   if (     !vkcd_st.link_all  || !vkcd_st.link_ns
         || !vkcd_st.create_ns || !vkcd_st.exported_ns)
      return false;
   /* The default namespace has no handle: a shared one made with no
    * parent and dlopen as the caller is a copy of it. */
   if (!(vkcd_st.default_ns = vkcd_st.create_ns("default_copy", NULL, NULL,
               VKCD_NS_SHARED, NULL, NULL, (const void*)&dlopen)))
      return false;
   vkcd_st.ready = true;
   return true;
}

static bool vkcd_read_all(int fd, uint8_t *buf, size_t len)
{
   while (len)
   {
      ssize_t n = read(fd, buf, len);
      if (n <= 0)
         return false;
      buf += n;
      len -= (size_t)n;
   }
   return true;
}

/* The system loader is loaded already; a copy whose soname differs is a
 * library of its own, and its HAL lookups reach the hook. */
static void *vkcd_load_copy(const char *tmp_dir, int flags,
      struct android_namespace_t *ns)
{
   static const char lib[] = "/system/lib64/libvulkan.so";
   static char path[PATH_MAX]; /* frames stay under 4 KiB */
   char patch[4];
   android_dlextinfo info;
   struct stat st;
   uint8_t *map = (uint8_t*)MAP_FAILED;
   void *handle = NULL;
   unsigned id  = vkcd_st.copies++;
   int in       = -1;
   int fd;

   if (android_get_device_api_level() >= 29)
      fd = (int)syscall(__NR_memfd_create, lib, 0);
   else
   {
      snprintf(path, sizeof(path), "%s%u_patched.so", tmp_dir, id);
      fd = open(path, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
   }
   if (fd < 0)
      return NULL;
   snprintf(patch, sizeof(patch), "%03u", id % 1000);
   if (     (in = open(lib, O_RDONLY)) >= 0
         && !fstat(in, &st)
         && !ftruncate(fd, st.st_size)
         && (map = (uint8_t*)mmap(NULL, (size_t)st.st_size,
               PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) != MAP_FAILED
         && vkcd_read_all(in, map, (size_t)st.st_size)
         && vulkan_custom_driver_patch_soname(map, (size_t)st.st_size,
               patch))
   {
      info.flags             = ANDROID_DLEXT_USE_NAMESPACE
                             | ANDROID_DLEXT_USE_LIBRARY_FD;
      info.reserved_addr     = NULL;
      info.reserved_size     = 0;
      info.relro_fd          = 0;
      info.library_fd        = fd;
      info.library_fd_offset = 0;
      info.library_namespace = ns;
      snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);
      handle = android_dlopen_ext(path, flags, &info);
   }
   if (map != (uint8_t*)MAP_FAILED)
      munmap(map, (size_t)st.st_size);
   if (in >= 0)
      close(in);
   /* A loaded copy keeps its fd: the library's name points at it */
   if (!handle)
      close(fd);
   return handle;
}

void *vulkan_custom_driver_linker_open(const char *hook_dir,
      const char *driver_dir, const char *driver_name, const char *tmp_dir,
      int flags, const char **reason)
{
   struct vkcd_hook_params *params = NULL;
   struct android_namespace_t *ns;
   vkcd_hook_init_t init;
   void *hook;
   void *handle;
   android_dlextinfo info;

   *reason = "linker namespaces unavailable";
   if (!vkcd_resolve())
      return NULL;

   *reason = "hook library did not load";
   if (     !(params = (struct vkcd_hook_params*)calloc(1, sizeof(*params)))
         || strlen(hook_dir)    >= sizeof(params->hook_dir)
         || strlen(driver_dir)  >= sizeof(params->driver_dir)
         || strlen(driver_name) >= sizeof(params->driver_name))
   {
      free(params);
      return NULL;
   }
   strcpy(params->hook_dir,    hook_dir);
   strcpy(params->driver_dir,  driver_dir);
   strcpy(params->driver_name, driver_name);
   params->dlopen_ext  = android_dlopen_ext;
   params->create_ns   = vkcd_st.create_ns;
   params->exported_ns = vkcd_st.exported_ns;
   params->link_ns     = vkcd_st.link_ns;

   if (     !(ns = vkcd_st.create_ns("vulkan_driver_hook", hook_dir, NULL,
               VKCD_NS_SHARED, NULL, NULL,
               (const void*)&vulkan_custom_driver_linker_open))
         || !vkcd_st.link_all(ns, vkcd_st.default_ns))
   {
      free(params);
      return NULL;
   }
   info.flags             = ANDROID_DLEXT_USE_NAMESPACE;
   info.reserved_addr     = NULL;
   info.reserved_size     = 0;
   info.relro_fd          = 0;
   info.library_fd        = 0;
   info.library_fd_offset = 0;
   info.library_namespace = ns;
   if (     !(hook = android_dlopen_ext("libvulkan_driver_hook.so",
               RTLD_NOW | RTLD_GLOBAL, &info))
         || !(init = (vkcd_hook_init_t)dlsym(hook,
               "vulkan_custom_driver_hook_init")))
   {
      free(params);
      return NULL;
   }
   init(params);

   *reason = "Vulkan loader copy did not load";
   if (!(handle = vkcd_load_copy(tmp_dir, flags, ns)))
      return NULL;
   *reason = NULL;
   return handle;
}

#else /* VULKAN_CUSTOM_DRIVER_HOOK */

#define VKCD_EXPORT __attribute__((visibility("default")))

static const struct vkcd_hook_params *vkcd_hook;

VKCD_EXPORT void vulkan_custom_driver_hook_init(
      const struct vkcd_hook_params *params)
{
   vkcd_hook = params;
}

/* The loader asks for its HAL here: the custom driver goes in a namespace
 * of its own, a child of the one the loader asked for. */
static void *vkcd_hook_open(const char *name, int flags,
      const android_dlextinfo *info)
{
   android_dlextinfo driver_info;
   struct android_namespace_t *ns;
   void *handle;

   if (!vkcd_hook)
      return NULL;
   if (     !name || !strstr(name, "vulkan.")
         || !info || !(info->flags & ANDROID_DLEXT_USE_NAMESPACE)
         || !info->library_namespace)
      return vkcd_hook->dlopen_ext(name, flags, info);

   if (!(ns = vkcd_hook->create_ns(name, vkcd_hook->driver_dir,
               vkcd_hook->hook_dir, VKCD_NS_SHARED, NULL,
               info->library_namespace,
               (const void*)&vulkan_custom_driver_hook_init)))
   {
      const char *err = dlerror();
      __android_log_print(ANDROID_LOG_ERROR, "RetroArch",
            "[Vulkan] Driver hook: failed to load custom driver %s: "
            "no namespace (%s)\n", vkcd_hook->driver_name,
            err ? err : "unknown");
      return vkcd_hook->dlopen_ext(name, flags, info);
   }
   vkcd_hook->link_ns(ns, NULL, "libandroid.so");
   driver_info                   = *info;
   driver_info.library_namespace = ns;
   if ((handle = vkcd_hook->dlopen_ext(vkcd_hook->driver_name, flags,
               &driver_info)))
      return handle;
   __android_log_print(ANDROID_LOG_ERROR, "RetroArch",
         "[Vulkan] Driver hook: failed to load custom driver %s: %s\n",
         vkcd_hook->driver_name, dlerror());
   return vkcd_hook->dlopen_ext(name, flags, info);
}

VKCD_EXPORT void *android_dlopen_ext(const char *name, int flags,
      const android_dlextinfo *info)
{
   return vkcd_hook_open(name, flags, info);
}

VKCD_EXPORT void *android_load_sphal_library(const char *name, int flags)
{
   static const char *const names[] = { "sphal", "vendor", "default" };
   android_dlextinfo info;
   unsigned i;

   if (!vkcd_hook)
      return NULL;
   for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
   {
      struct android_namespace_t *ns = vkcd_hook->exported_ns(names[i]);
      if (ns)
      {
         info.flags             = ANDROID_DLEXT_USE_NAMESPACE;
         info.reserved_addr     = NULL;
         info.reserved_size     = 0;
         info.relro_fd          = 0;
         info.library_fd        = 0;
         info.library_fd_offset = 0;
         info.library_namespace = ns;
         return vkcd_hook_open(name, flags, &info);
      }
   }
   return NULL;
}
#endif
#endif
