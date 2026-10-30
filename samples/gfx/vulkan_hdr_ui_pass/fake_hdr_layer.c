/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (fake_hdr_layer.c).
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

/* A Vulkan layer that makes a 10-bit X11 surface (lavapipe on an Xvfb
 * of depth 30) look like an HDR10 display.
 *
 * Every 10-bit surface format the surface reports in sRGB is reported
 * a second time paired with VK_COLOR_SPACE_HDR10_ST2084_EXT, so the
 * Vulkan driver selects an HDR10 swapchain and runs its whole HDR
 * path: the offscreen buffer, the SDR UI layer, the PQ composite. A
 * swapchain created in that colour space is passed on in sRGB with the
 * same format and images, so nothing the driver records changes; only
 * the meaning the compositor would give the pixels does, and there is
 * no compositor here to give it.
 *
 * Whichever side of the validation layer this one lands on, the two
 * agree: below it, the validation layer reads the HDR10 pairs through
 * this layer and accepts the swapchain; above it, it sees the sRGB
 * swapchain the surface really has.
 *
 * Each swapchain it rewrote is logged, so a run can tell that the HDR
 * path was taken rather than silently falling back to SDR.
 *
 * One instance and one device at a time, which is how the frontend
 * runs the driver; the next layer's entry points are kept in globals. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../gfx/include/vulkan/vulkan.h"
#include "../../../gfx/include/vulkan/vk_layer.h"

#if defined(_WIN32)
#define FH_EXPORT __declspec(dllexport)
#else
#define FH_EXPORT __attribute__((visibility("default")))
#endif

/* Far more than any surface reports. */
#define FH_FORMATS 64

static PFN_vkGetInstanceProcAddr s_next_gipa;
static PFN_vkGetDeviceProcAddr   s_next_gdpa;
static VkInstance                s_instance;
static unsigned long             s_hdr10_swapchains;

static PFN_vkVoidFunction fh_next_instance(const char *name)
{
   return s_next_gipa ? s_next_gipa(s_instance, name) : NULL;
}

static int fh_is_10bit(VkFormat format)
{
   return format == VK_FORMAT_A2B10G10R10_UNORM_PACK32
       || format == VK_FORMAT_A2R10G10B10_UNORM_PACK32;
}

/* The real list in real[0..n), the HDR10 twins appended after it.
 * Returns the length of the whole list. */
static uint32_t fh_extend(VkSurfaceFormatKHR *real, uint32_t n)
{
   uint32_t i, total = n;
   for (i = 0; i < n && total < FH_FORMATS; i++)
   {
      if (     fh_is_10bit(real[i].format)
            && real[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
      {
         real[total].format     = real[i].format;
         real[total].colorSpace = VK_COLOR_SPACE_HDR10_ST2084_EXT;
         total++;
      }
   }
   return total;
}

/* The count/fill contract of every vkGet...KHR enumerator. */
static VkResult fh_hand_out(uint32_t total, uint32_t *count, int filling)
{
   if (!filling)
   {
      *count = total;
      return VK_SUCCESS;
   }
   if (*count < total)
      return VK_INCOMPLETE;
   *count = total;
   return VK_SUCCESS;
}

static VKAPI_ATTR VkResult VKAPI_CALL fh_GetPhysicalDeviceSurfaceFormatsKHR(
      VkPhysicalDevice gpu, VkSurfaceKHR surface, uint32_t *count,
      VkSurfaceFormatKHR *formats)
{
   PFN_vkGetPhysicalDeviceSurfaceFormatsKHR next =
      (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)fh_next_instance(
            "vkGetPhysicalDeviceSurfaceFormatsKHR");
   VkSurfaceFormatKHR list[FH_FORMATS];
   uint32_t n = FH_FORMATS / 2, total, i;
   VkResult res;

   if (!next)
      return VK_ERROR_INITIALIZATION_FAILED;
   res = next(gpu, surface, &n, list);
   if (res != VK_SUCCESS && res != VK_INCOMPLETE)
      return res;
   total = fh_extend(list, n);
   res   = fh_hand_out(total, count, formats != NULL);
   if (formats)
      for (i = 0; i < *count && i < total; i++)
         formats[i] = list[i];
   return res;
}

static VKAPI_ATTR VkResult VKAPI_CALL fh_GetPhysicalDeviceSurfaceFormats2KHR(
      VkPhysicalDevice gpu, const VkPhysicalDeviceSurfaceInfo2KHR *info,
      uint32_t *count, VkSurfaceFormat2KHR *formats)
{
   PFN_vkGetPhysicalDeviceSurfaceFormats2KHR next =
      (PFN_vkGetPhysicalDeviceSurfaceFormats2KHR)fh_next_instance(
            "vkGetPhysicalDeviceSurfaceFormats2KHR");
   VkSurfaceFormat2KHR real[FH_FORMATS / 2];
   VkSurfaceFormatKHR list[FH_FORMATS];
   uint32_t n = FH_FORMATS / 2, total, i;
   VkResult res;

   if (!next)
      return VK_ERROR_INITIALIZATION_FAILED;
   for (i = 0; i < n; i++)
   {
      real[i].sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR;
      real[i].pNext = NULL;
   }
   res = next(gpu, info, &n, real);
   if (res != VK_SUCCESS && res != VK_INCOMPLETE)
      return res;
   for (i = 0; i < n; i++)
      list[i] = real[i].surfaceFormat;
   total = fh_extend(list, n);
   res   = fh_hand_out(total, count, formats != NULL);
   /* The caller's sType and pNext stay; only the pair is written. */
   if (formats)
      for (i = 0; i < *count && i < total; i++)
         formats[i].surfaceFormat = list[i];
   return res;
}

static VKAPI_ATTR VkResult VKAPI_CALL fh_CreateSwapchainKHR(VkDevice device,
      const VkSwapchainCreateInfoKHR *info, const VkAllocationCallbacks *cb,
      VkSwapchainKHR *swapchain)
{
   PFN_vkCreateSwapchainKHR next = (PFN_vkCreateSwapchainKHR)
      s_next_gdpa(device, "vkCreateSwapchainKHR");
   VkSwapchainCreateInfoKHR real = *info;

   if (real.imageColorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT)
   {
      real.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
      s_hdr10_swapchains++;
      printf("[info] fake-hdr: HDR10 swapchain %lu, format %d, %ux%u\n",
            s_hdr10_swapchains, (int)real.imageFormat,
            real.imageExtent.width, real.imageExtent.height);
      fflush(stdout);
   }
   return next(device, &real, cb, swapchain);
}

static VKAPI_ATTR VkResult VKAPI_CALL fh_CreateDevice(VkPhysicalDevice gpu,
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

static VKAPI_ATTR VkResult VKAPI_CALL fh_CreateInstance(
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

FH_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL fh_GetDeviceProcAddr(
      VkDevice device, const char *name)
{
   if (!strcmp(name, "vkCreateSwapchainKHR"))
      return (PFN_vkVoidFunction)fh_CreateSwapchainKHR;
   if (!strcmp(name, "vkGetDeviceProcAddr"))
      return (PFN_vkVoidFunction)fh_GetDeviceProcAddr;
   return s_next_gdpa ? s_next_gdpa(device, name) : NULL;
}

FH_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL fh_GetInstanceProcAddr(
      VkInstance instance, const char *name)
{
#define FH_HOOK(n) if (!strcmp(name, "vk" #n)) return (PFN_vkVoidFunction)fh_##n
   FH_HOOK(CreateInstance);
   FH_HOOK(CreateDevice);
   FH_HOOK(CreateSwapchainKHR);
   FH_HOOK(GetPhysicalDeviceSurfaceFormatsKHR);
   FH_HOOK(GetPhysicalDeviceSurfaceFormats2KHR);
   FH_HOOK(GetInstanceProcAddr);
   FH_HOOK(GetDeviceProcAddr);
#undef FH_HOOK
   return s_next_gipa ? s_next_gipa(instance, name) : NULL;
}

FH_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(
      VkNegotiateLayerInterface *v)
{
   if (v->loaderLayerInterfaceVersion > 2)
      v->loaderLayerInterfaceVersion = 2;
   v->pfnGetInstanceProcAddr       = fh_GetInstanceProcAddr;
   v->pfnGetDeviceProcAddr         = fh_GetDeviceProcAddr;
   v->pfnGetPhysicalDeviceProcAddr = NULL;
   return VK_SUCCESS;
}
