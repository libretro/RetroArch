/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../gfx/common/vaapi_hevc_backend.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#if defined(__linux__) && defined(NEED_DYNAMIC)
#include <sys/sysmacros.h>
#include <xf86drm.h>
static drmDevice device;
static char *nodes[DRM_NODE_MAX];
static unsigned loads, lookups, frees, closes, fail;

static int test_get_device(dev_t id, uint32_t flags, drmDevicePtr *out)
{
   assert(id == makedev(226, 129) && !flags && !*out);
   if (fail == 4)
      return -1;
   *out = &device;
   return fail == 5 ? -1 : 0;
}

static void test_free_device(drmDevicePtr *out)
{
   assert(*out == &device && !closes);
   *out = NULL;
   frees++;
}

static dylib_t test_load(const char *name)
{
   assert(!strcmp(name, "libdrm.so.2"));
   loads++;
   return fail == 1 ? NULL : (dylib_t)(uintptr_t)1;
}

static function_t test_symbol(dylib_t library, const char *name)
{
   function_t symbol = NULL;
   assert(library == (dylib_t)(uintptr_t)1);
   lookups++;
   if (!strcmp(name, "drmGetDeviceFromDevId"))
   {
      int (*typed)(dev_t, uint32_t, drmDevicePtr *) = test_get_device;
      assert(sizeof(symbol) == sizeof(typed));
      if (fail != 2)
         memcpy(&symbol, &typed, sizeof(symbol));
   }
   else
   {
      void (*typed)(drmDevicePtr *) = test_free_device;
      assert(!strcmp(name, "drmFreeDevice") && sizeof(symbol) == sizeof(typed));
      if (fail != 3)
         memcpy(&symbol, &typed, sizeof(symbol));
   }
   return symbol;
}

static void test_close(dylib_t library)
{
   assert(library == (dylib_t)(uintptr_t)1);
   closes++;
}

#define dylib_load test_load
#define dylib_proc test_symbol
#define dylib_close test_close
#include "../../../gfx/common/vaapi_hevc_backend.c"
#undef dylib_close
#undef dylib_proc
#undef dylib_load

int main(void)
{
   char path[64];
   unsigned mode;
   nodes[DRM_NODE_RENDER] = "/dev/dri/named-render-node";
   device.nodes           = nodes;
   device.available_nodes = 1 << DRM_NODE_RENDER;
   for (mode = 0; mode < 9; mode++)
   {
      loads = lookups = frees = closes = 0;
      fail                             = mode;
      device.available_nodes           = mode == 6 ? 0 : 1 << DRM_NODE_RENDER;
      nodes[DRM_NODE_RENDER] = mode == 7 ? NULL : "/dev/dri/named-render-node";
      memset(path, 0x5a, sizeof(path));
      assert(vaapi_hevc_backend_node_path(
                  226, 129, path, mode == 8 ? 4 : sizeof(path))
            == (mode == 0));
      assert(loads == 1 && closes == (mode != 1));
      assert(frees == (mode == 0 || mode >= 5));
      if (!mode)
         assert(!strcmp(path, "/dev/dri/named-render-node"));
      else
         assert(path[0] == 0x5a);
   }
   puts("PASS: typed libdrm render-node lookup, exact supplied path, missing bindings and partial device cleanup; no native device open");
   return 0;
}
#else
int main(void)
{
   puts("SKIP: Linux libdrm node-resolution ABI fixture");
   return 0;
}
#endif
