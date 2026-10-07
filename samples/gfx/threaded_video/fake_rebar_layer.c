/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (fake_rebar_layer.c).
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

/* A Vulkan layer that makes a one-memory-type device (lavapipe) report
 * the memory a discrete card with resizable BAR reports:
 *
 *   heap 0  8 GB device local      heap 1  16 GB system
 *   type 0  host visible, coherent, cached       (heap 1)
 *   type 1  device local                         (heap 0)
 *   type 2  device local, host visible, coherent (heap 0)
 *
 * Every resource may use all three; every allocation lands in the real
 * device's first memory type. What it adds is the count: how many
 * allocations each reported type got, and how many images were bound
 * to type-2 memory, printed when each device goes (the loader loads the
 * layer afresh for each instance, so the counts are that device's), so
 * check-vulkan-rebar can add them up and see the driver put its
 * CPU-written buffers and streamed frame textures in type 2 - and, with FAKE_REBAR_REFUSE=1,
 * that every type-2 allocation refused falls back and the run still
 * passes.
 *
 * One instance and one device at a time, which is how the harness runs
 * the driver; the next layer's entry points are kept in globals. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../gfx/include/vulkan/vulkan.h"
#include "../../../gfx/include/vulkan/vk_layer.h"

#if defined(_WIN32)
#define FR_EXPORT __declspec(dllexport)
#else
#define FR_EXPORT __attribute__((visibility("default")))
#endif

#define FR_TYPES 3
#define FR_MB ((VkDeviceSize)1024 * 1024)
/* Type-2 allocations live at once: a frame texture per swapchain image
 * and the buffers, far below this. */
#define FR_LIVE 1024

static PFN_vkGetInstanceProcAddr s_next_gipa;
static PFN_vkGetDeviceProcAddr   s_next_gdpa;
static VkInstance                s_instance;
static unsigned long             s_allocs[FR_TYPES];
static unsigned long             s_refused;
static unsigned long             s_images_bar;
static VkDeviceMemory            s_live_bar[FR_LIVE];

static PFN_vkVoidFunction fr_next_instance(const char *name)
{
   return s_next_gipa ? s_next_gipa(s_instance, name) : NULL;
}

/* FAKE_REBAR_REFUSE set and not empty or 0 */
static int fr_refuse(void)
{
   const char *v = getenv("FAKE_REBAR_REFUSE");
   return v && *v && strcmp(v, "0");
}

static void fr_fill(VkPhysicalDeviceMemoryProperties *p)
{
   memset(p, 0, sizeof(*p));
   p->memoryHeapCount                 = 2;
   p->memoryHeaps[0].size             = 8192 * FR_MB;
   p->memoryHeaps[0].flags            = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
   p->memoryHeaps[1].size             = 16384 * FR_MB;
   p->memoryTypeCount                 = FR_TYPES;
   p->memoryTypes[0].propertyFlags    = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                                      | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
   p->memoryTypes[0].heapIndex        = 1;
   p->memoryTypes[1].propertyFlags    = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
   p->memoryTypes[1].heapIndex        = 0;
   p->memoryTypes[2].propertyFlags    = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                                      | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
   p->memoryTypes[2].heapIndex        = 0;
}

static VKAPI_ATTR void VKAPI_CALL fr_GetPhysicalDeviceMemoryProperties(
      VkPhysicalDevice gpu, VkPhysicalDeviceMemoryProperties *p)
{
   (void)gpu;
   fr_fill(p);
}

static VKAPI_ATTR void VKAPI_CALL fr_GetPhysicalDeviceMemoryProperties2(
      VkPhysicalDevice gpu, VkPhysicalDeviceMemoryProperties2 *p)
{
   PFN_vkGetPhysicalDeviceMemoryProperties2 next =
      (PFN_vkGetPhysicalDeviceMemoryProperties2)fr_next_instance(
            "vkGetPhysicalDeviceMemoryProperties2");
   if (next)
      next(gpu, p); /* chained structs, budgets included */
   fr_fill(&p->memoryProperties);
}

static VKAPI_ATTR void VKAPI_CALL fr_GetBufferMemoryRequirements(
      VkDevice device, VkBuffer buffer, VkMemoryRequirements *r)
{
   PFN_vkGetBufferMemoryRequirements next = (PFN_vkGetBufferMemoryRequirements)
      s_next_gdpa(device, "vkGetBufferMemoryRequirements");
   next(device, buffer, r);
   r->memoryTypeBits = (1u << FR_TYPES) - 1;
}

static VKAPI_ATTR void VKAPI_CALL fr_GetImageMemoryRequirements(
      VkDevice device, VkImage image, VkMemoryRequirements *r)
{
   PFN_vkGetImageMemoryRequirements next = (PFN_vkGetImageMemoryRequirements)
      s_next_gdpa(device, "vkGetImageMemoryRequirements");
   next(device, image, r);
   r->memoryTypeBits = (1u << FR_TYPES) - 1;
}

static VKAPI_ATTR void VKAPI_CALL fr_GetBufferMemoryRequirements2(
      VkDevice device, const VkBufferMemoryRequirementsInfo2 *info,
      VkMemoryRequirements2 *r)
{
   PFN_vkGetBufferMemoryRequirements2 next = (PFN_vkGetBufferMemoryRequirements2)
      s_next_gdpa(device, "vkGetBufferMemoryRequirements2");
   next(device, info, r);
   r->memoryRequirements.memoryTypeBits = (1u << FR_TYPES) - 1;
}

static VKAPI_ATTR void VKAPI_CALL fr_GetImageMemoryRequirements2(
      VkDevice device, const VkImageMemoryRequirementsInfo2 *info,
      VkMemoryRequirements2 *r)
{
   PFN_vkGetImageMemoryRequirements2 next = (PFN_vkGetImageMemoryRequirements2)
      s_next_gdpa(device, "vkGetImageMemoryRequirements2");
   next(device, info, r);
   r->memoryRequirements.memoryTypeBits = (1u << FR_TYPES) - 1;
}

static VKAPI_ATTR VkResult VKAPI_CALL fr_AllocateMemory(VkDevice device,
      const VkMemoryAllocateInfo *info, const VkAllocationCallbacks *cb,
      VkDeviceMemory *memory)
{
   PFN_vkAllocateMemory next = (PFN_vkAllocateMemory)
      s_next_gdpa(device, "vkAllocateMemory");
   VkMemoryAllocateInfo real = *info;
   if (real.memoryTypeIndex >= FR_TYPES)
      return VK_ERROR_OUT_OF_DEVICE_MEMORY;
   if (real.memoryTypeIndex == 2 && fr_refuse())
   {
      s_refused++;
      return VK_ERROR_OUT_OF_DEVICE_MEMORY;
   }
   s_allocs[info->memoryTypeIndex]++;
   real.memoryTypeIndex = 0;
   if (next(device, &real, cb, memory) != VK_SUCCESS)
      return VK_ERROR_OUT_OF_DEVICE_MEMORY;
   if (info->memoryTypeIndex == 2)
   {
      unsigned i;
      for (i = 0; i < FR_LIVE; i++)
         if (s_live_bar[i] == VK_NULL_HANDLE)
         {
            s_live_bar[i] = *memory;
            break;
         }
   }
   return VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL fr_FreeMemory(VkDevice device,
      VkDeviceMemory memory, const VkAllocationCallbacks *cb)
{
   PFN_vkFreeMemory next = (PFN_vkFreeMemory)
      s_next_gdpa(device, "vkFreeMemory");
   unsigned i;
   if (memory != VK_NULL_HANDLE)
      for (i = 0; i < FR_LIVE; i++)
         if (s_live_bar[i] == memory)
         {
            s_live_bar[i] = VK_NULL_HANDLE;
            break;
         }
   next(device, memory, cb);
}

static void fr_count_image(VkDeviceMemory memory)
{
   unsigned i;
   for (i = 0; i < FR_LIVE; i++)
      if (s_live_bar[i] == memory)
      {
         s_images_bar++;
         return;
      }
}

static VKAPI_ATTR VkResult VKAPI_CALL fr_BindImageMemory(VkDevice device,
      VkImage image, VkDeviceMemory memory, VkDeviceSize offset)
{
   PFN_vkBindImageMemory next = (PFN_vkBindImageMemory)
      s_next_gdpa(device, "vkBindImageMemory");
   fr_count_image(memory);
   return next(device, image, memory, offset);
}

static VKAPI_ATTR VkResult VKAPI_CALL fr_BindImageMemory2(VkDevice device,
      uint32_t count, const VkBindImageMemoryInfo *infos)
{
   PFN_vkBindImageMemory2 next = (PFN_vkBindImageMemory2)
      s_next_gdpa(device, "vkBindImageMemory2");
   uint32_t i;
   for (i = 0; i < count; i++)
      fr_count_image(infos[i].memory);
   return next(device, count, infos);
}

static VKAPI_ATTR void VKAPI_CALL fr_DestroyDevice(VkDevice device,
      const VkAllocationCallbacks *cb)
{
   PFN_vkDestroyDevice next = (PFN_vkDestroyDevice)
      s_next_gdpa(device, "vkDestroyDevice");
   printf("[info] fake-rebar: allocations in type 0 %lu, type 1 %lu, type 2 %lu; type 2 refused %lu; images in type 2 %lu\n",
         s_allocs[0], s_allocs[1], s_allocs[2], s_refused, s_images_bar);
   fflush(stdout);
   next(device, cb);
}

static VKAPI_ATTR VkResult VKAPI_CALL fr_CreateDevice(VkPhysicalDevice gpu,
      const VkDeviceCreateInfo *info, const VkAllocationCallbacks *cb,
      VkDevice *device)
{
   VkLayerDeviceCreateInfo *chain = (VkLayerDeviceCreateInfo*)info->pNext;
   PFN_vkGetInstanceProcAddr gipa;
   PFN_vkCreateDevice create;

   while (chain && !(chain->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO
            && chain->function == VK_LAYER_LINK_INFO))
      chain = (VkLayerDeviceCreateInfo*)chain->pNext;
   if (!chain)
      return VK_ERROR_INITIALIZATION_FAILED;

   gipa        = chain->u.pLayerInfo->pfnNextGetInstanceProcAddr;
   s_next_gdpa = chain->u.pLayerInfo->pfnNextGetDeviceProcAddr;
   chain->u.pLayerInfo = chain->u.pLayerInfo->pNext;

   create = (PFN_vkCreateDevice)gipa(s_instance, "vkCreateDevice");
   return create(gpu, info, cb, device);
}

static VKAPI_ATTR VkResult VKAPI_CALL fr_CreateInstance(
      const VkInstanceCreateInfo *info, const VkAllocationCallbacks *cb,
      VkInstance *instance)
{
   VkLayerInstanceCreateInfo *chain = (VkLayerInstanceCreateInfo*)info->pNext;
   PFN_vkCreateInstance create;
   VkResult res;

   while (chain && !(chain->sType == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO
            && chain->function == VK_LAYER_LINK_INFO))
      chain = (VkLayerInstanceCreateInfo*)chain->pNext;
   if (!chain)
      return VK_ERROR_INITIALIZATION_FAILED;

   s_next_gipa         = chain->u.pLayerInfo->pfnNextGetInstanceProcAddr;
   chain->u.pLayerInfo = chain->u.pLayerInfo->pNext;

   create = (PFN_vkCreateInstance)s_next_gipa(NULL, "vkCreateInstance");
   res    = create(info, cb, instance);
   if (res == VK_SUCCESS)
      s_instance = *instance;
   return res;
}

static PFN_vkVoidFunction fr_device_hook(const char *name)
{
#define FR_HOOK(n) if (!strcmp(name, "vk" #n)) return (PFN_vkVoidFunction)fr_##n
   FR_HOOK(GetBufferMemoryRequirements);
   FR_HOOK(GetImageMemoryRequirements);
   FR_HOOK(GetBufferMemoryRequirements2);
   FR_HOOK(GetImageMemoryRequirements2);
   FR_HOOK(AllocateMemory);
   FR_HOOK(FreeMemory);
   FR_HOOK(BindImageMemory);
   FR_HOOK(BindImageMemory2);
   FR_HOOK(DestroyDevice);
   if (!strcmp(name, "vkGetBufferMemoryRequirements2KHR"))
      return (PFN_vkVoidFunction)fr_GetBufferMemoryRequirements2;
   if (!strcmp(name, "vkGetImageMemoryRequirements2KHR"))
      return (PFN_vkVoidFunction)fr_GetImageMemoryRequirements2;
   if (!strcmp(name, "vkBindImageMemory2KHR"))
      return (PFN_vkVoidFunction)fr_BindImageMemory2;
   return NULL;
}

FR_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL fr_GetDeviceProcAddr(
      VkDevice device, const char *name)
{
   PFN_vkVoidFunction hook = fr_device_hook(name);
   if (hook)
      return hook;
   return s_next_gdpa ? s_next_gdpa(device, name) : NULL;
}

FR_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL fr_GetInstanceProcAddr(
      VkInstance instance, const char *name)
{
   PFN_vkVoidFunction hook = fr_device_hook(name);
   if (hook)
      return hook;
   FR_HOOK(CreateInstance);
   FR_HOOK(CreateDevice);
   FR_HOOK(GetPhysicalDeviceMemoryProperties);
   FR_HOOK(GetPhysicalDeviceMemoryProperties2);
   FR_HOOK(GetInstanceProcAddr);
   FR_HOOK(GetDeviceProcAddr);
   if (!strcmp(name, "vkGetPhysicalDeviceMemoryProperties2KHR"))
      return (PFN_vkVoidFunction)fr_GetPhysicalDeviceMemoryProperties2;
#undef FR_HOOK
   return s_next_gipa ? s_next_gipa(instance, name) : NULL;
}

FR_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(
      VkNegotiateLayerInterface *v)
{
   if (v->loaderLayerInterfaceVersion > 2)
      v->loaderLayerInterfaceVersion = 2;
   v->pfnGetInstanceProcAddr       = fr_GetInstanceProcAddr;
   v->pfnGetDeviceProcAddr         = fr_GetDeviceProcAddr;
   v->pfnGetPhysicalDeviceProcAddr = NULL;
   return VK_SUCCESS;
}
