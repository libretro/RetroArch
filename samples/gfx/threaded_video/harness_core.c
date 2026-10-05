/* Minimal libretro core for the threaded video harness: supports
 * running without content, so the frontend treats it as a real core
 * (the menu can be closed over it, unlike the dummy core), and hands
 * the frontend a frame per retro_run, sometimes duplicated and
 * sometimes taller than the geometry it declared. */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <libretro.h>
/* The hardware mode below reaches Vulkan through the entry points the
 * frontend's interface hands over, so the core links nothing. */
#define VK_NO_PROTOTYPES
#include <libretro_vulkan.h>

#define W 320
#define H 240
#define H_OVERSIZE 600

static retro_video_refresh_t video_cb;
static retro_environment_t   environ_cb;
/* Room for a 32-bit oversize frame; the pixel size follows the
 * format chosen at set_environment. */
static uint8_t  frame[W * H_OVERSIZE * 4];
static unsigned bpp = 2;
static enum retro_pixel_format pixel_format = RETRO_PIXEL_FORMAT_RGB565;
static unsigned runs;

/* --- hardware mode ------------------------------------------------------
 * With HARNESS_CORE_HW_VULKAN set the core is a Vulkan hardware core:
 * one image of its own, handed to the frontend with set_image every
 * frame, and destroyed when the context goes - without a word to the
 * frontend, which is what a core is entitled to do in context_destroy.
 * It can also withdraw the image in mid-session the way a core does
 * before it rebuilds its renderer: set_image(NULL), then destroy. Either
 * way the frontend must not draw from the image again.
 * harness_core_hw_ring() switches it to an image per sync index instead,
 * each cleared on the core's side of the sync index protocol and handed
 * over behind a semaphore, as a core with a renderer of its own does. */
static int hw_mode;
/* HARNESS_CORE_HW_VULKAN=2: the core creates the device, through the
 * negotiation interface's create_device, the way Beetle PSX and the
 * other cores with a renderer of their own do. The device has exactly
 * the extensions the frontend says it requires and none of the ones it
 * would have added to a device of its own making - so whatever the
 * frontend then does on the strength of an optional extension is a
 * validation error here. */
static int hw_own_device;
static const struct retro_hw_render_interface_vulkan *hw_vk;
static struct retro_hw_render_callback hw_cb;
static struct retro_vulkan_image hw_image;
static VkImage        hw_vkimage;
static VkDeviceMemory hw_memory;
static int hw_have_image;
static int hw_withdrawn;

static PFN_vkCreateImage                hw_vkCreateImage;
static PFN_vkDestroyImage               hw_vkDestroyImage;
static PFN_vkGetImageMemoryRequirements hw_vkGetImageMemoryRequirements;
static PFN_vkAllocateMemory             hw_vkAllocateMemory;
static PFN_vkFreeMemory                 hw_vkFreeMemory;
static PFN_vkBindImageMemory            hw_vkBindImageMemory;
static PFN_vkCreateImageView            hw_vkCreateImageView;
static PFN_vkDestroyImageView           hw_vkDestroyImageView;
static PFN_vkCreateCommandPool          hw_vkCreateCommandPool;
static PFN_vkDestroyCommandPool         hw_vkDestroyCommandPool;
static PFN_vkAllocateCommandBuffers     hw_vkAllocateCommandBuffers;
static PFN_vkBeginCommandBuffer         hw_vkBeginCommandBuffer;
static PFN_vkEndCommandBuffer           hw_vkEndCommandBuffer;
static PFN_vkCmdPipelineBarrier         hw_vkCmdPipelineBarrier;
static PFN_vkCmdClearColorImage         hw_vkCmdClearColorImage;
static PFN_vkQueueSubmit                hw_vkQueueSubmit;
static PFN_vkQueueWaitIdle              hw_vkQueueWaitIdle;
static PFN_vkCreateFence                hw_vkCreateFence;
static PFN_vkDestroyFence               hw_vkDestroyFence;
static PFN_vkWaitForFences              hw_vkWaitForFences;
static PFN_vkResetFences                hw_vkResetFences;
static PFN_vkCreateSemaphore            hw_vkCreateSemaphore;
static PFN_vkDestroySemaphore           hw_vkDestroySemaphore;
static PFN_vkGetPhysicalDeviceMemoryProperties hw_vkGetPhysicalDeviceMemoryProperties;

/* harness_core_hw_ring(): 0 off, 1 every third frame a dupe, 2 every
 * third frame a software frame, 3 every frame a hardware one. */
#define HW_RING_SLOTS 8
static int           hw_ring;
static unsigned      hw_ring_sent;
/* Told each image the core is about to clear, as its 64 bits. */
static void        (*hw_ring_clear_hook)(uint64_t image);
static VkCommandPool hw_ring_pool;
static struct hw_ring_slot
{
   struct retro_vulkan_image image;
   VkImage         vkimage;
   VkDeviceMemory  memory;
   VkCommandBuffer cmd;
   VkSemaphore     semaphore;
   VkFence         fence;
   int             pending;
   unsigned        uses;
} hw_ring_slots[HW_RING_SLOTS];

#define HW_SYM(name) \
   hw_##name = (PFN_##name)hw_vk->get_device_proc_addr(hw_vk->device, #name)

static void hw_barrier(VkCommandBuffer cmd, VkImage image,
      VkImageLayout from, VkImageLayout to,
      VkAccessFlags src_access, VkAccessFlags dst_access,
      VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage)
{
   VkImageMemoryBarrier b;
   memset(&b, 0, sizeof(b));
   b.sType                       = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
   b.srcAccessMask               = src_access;
   b.dstAccessMask               = dst_access;
   b.oldLayout                   = from;
   b.newLayout                   = to;
   b.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
   b.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
   b.image                       = image;
   b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   b.subresourceRange.levelCount = 1;
   b.subresourceRange.layerCount = 1;
   hw_vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &b);
}

static void hw_image_destroy(void)
{
   if (!hw_have_image)
      return;
   hw_vkDestroyImageView(hw_vk->device, hw_image.image_view, NULL);
   hw_vkDestroyImage(hw_vk->device, hw_vkimage, NULL);
   hw_vkFreeMemory(hw_vk->device, hw_memory, NULL);
   memset(&hw_image, 0, sizeof(hw_image));
   hw_vkimage    = VK_NULL_HANDLE;
   hw_memory     = VK_NULL_HANDLE;
   hw_have_image = 0;
}

/* A W x H image, its memory and the view the frontend samples. */
static int hw_image_alloc(VkImage *image, VkDeviceMemory *memory,
      struct retro_vulkan_image *out)
{
   VkImageCreateInfo ici;
   VkMemoryRequirements reqs;
   VkPhysicalDeviceMemoryProperties props;
   VkMemoryAllocateInfo mai;
   uint32_t i;

   memset(&ici, 0, sizeof(ici));
   ici.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   ici.imageType     = VK_IMAGE_TYPE_2D;
   ici.format        = VK_FORMAT_R8G8B8A8_UNORM;
   ici.extent.width  = W;
   ici.extent.height = H;
   ici.extent.depth  = 1;
   ici.mipLevels     = 1;
   ici.arrayLayers   = 1;
   ici.samples       = VK_SAMPLE_COUNT_1_BIT;
   ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
   ici.usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
   ici.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
   ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   if (hw_vkCreateImage(hw_vk->device, &ici, NULL, image) != VK_SUCCESS)
      return 0;

   hw_vkGetImageMemoryRequirements(hw_vk->device, *image, &reqs);
   hw_vkGetPhysicalDeviceMemoryProperties(hw_vk->gpu, &props);
   for (i = 0; i < props.memoryTypeCount; i++)
      if (reqs.memoryTypeBits & (1u << i))
         break;
   memset(&mai, 0, sizeof(mai));
   mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   mai.allocationSize  = reqs.size;
   mai.memoryTypeIndex = i;
   if (     i == props.memoryTypeCount
         || hw_vkAllocateMemory(hw_vk->device, &mai, NULL, memory) != VK_SUCCESS)
   {
      hw_vkDestroyImage(hw_vk->device, *image, NULL);
      return 0;
   }
   hw_vkBindImageMemory(hw_vk->device, *image, *memory, 0);

   memset(out, 0, sizeof(*out));
   out->image_layout                            = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   out->create_info.sType                       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   out->create_info.image                       = *image;
   out->create_info.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
   out->create_info.format                      = VK_FORMAT_R8G8B8A8_UNORM;
   out->create_info.components.r                = VK_COMPONENT_SWIZZLE_R;
   out->create_info.components.g                = VK_COMPONENT_SWIZZLE_G;
   out->create_info.components.b                = VK_COMPONENT_SWIZZLE_B;
   out->create_info.components.a                = VK_COMPONENT_SWIZZLE_A;
   out->create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   out->create_info.subresourceRange.levelCount = 1;
   out->create_info.subresourceRange.layerCount = 1;
   hw_vkCreateImageView(hw_vk->device, &out->create_info, NULL, &out->image_view);
   return 1;
}

/* A cleared W x H image in the layout the frontend samples it in. */
static int hw_image_create(void)
{
   VkCommandPoolCreateInfo cpi;
   VkCommandBufferAllocateInfo cai;
   VkCommandBufferBeginInfo cbi;
   VkFenceCreateInfo fci;
   VkSubmitInfo si;
   VkImageSubresourceRange range;
   VkClearColorValue colour;
   VkCommandPool pool  = VK_NULL_HANDLE;
   VkCommandBuffer cmd = VK_NULL_HANDLE;
   VkFence fence       = VK_NULL_HANDLE;

   if (!hw_image_alloc(&hw_vkimage, &hw_memory, &hw_image))
      return 0;

   memset(&cpi, 0, sizeof(cpi));
   cpi.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   cpi.queueFamilyIndex = hw_vk->queue_index;
   hw_vkCreateCommandPool(hw_vk->device, &cpi, NULL, &pool);
   memset(&cai, 0, sizeof(cai));
   cai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   cai.commandPool        = pool;
   cai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   cai.commandBufferCount = 1;
   hw_vkAllocateCommandBuffers(hw_vk->device, &cai, &cmd);
   memset(&cbi, 0, sizeof(cbi));
   cbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   cbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   hw_vkBeginCommandBuffer(cmd, &cbi);
   hw_barrier(cmd, hw_vkimage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         0, VK_ACCESS_TRANSFER_WRITE_BIT,
         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
   memset(&range, 0, sizeof(range));
   range.aspectMask  = VK_IMAGE_ASPECT_COLOR_BIT;
   range.levelCount  = 1;
   range.layerCount  = 1;
   colour.float32[0] = 0.1f;
   colour.float32[1] = 0.6f;
   colour.float32[2] = 0.3f;
   colour.float32[3] = 1.0f;
   hw_vkCmdClearColorImage(cmd, hw_vkimage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         &colour, 1, &range);
   hw_barrier(cmd, hw_vkimage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
         VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
   hw_vkEndCommandBuffer(cmd);

   memset(&fci, 0, sizeof(fci));
   fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   hw_vkCreateFence(hw_vk->device, &fci, NULL, &fence);
   memset(&si, 0, sizeof(si));
   si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   si.commandBufferCount = 1;
   si.pCommandBuffers    = &cmd;
   /* The queue is the frontend's as well. */
   hw_vk->lock_queue(hw_vk->handle);
   hw_vkQueueSubmit(hw_vk->queue, 1, &si, fence);
   hw_vk->unlock_queue(hw_vk->handle);
   hw_vkWaitForFences(hw_vk->device, 1, &fence, VK_TRUE, UINT64_MAX);
   hw_vkDestroyFence(hw_vk->device, fence, NULL);
   hw_vkDestroyCommandPool(hw_vk->device, pool, NULL);

   hw_have_image = 1;
   return 1;
}

static void hw_ring_slot_free(struct hw_ring_slot *s)
{
   if (s->image.image_view)
      hw_vkDestroyImageView(hw_vk->device, s->image.image_view, NULL);
   if (s->vkimage)
      hw_vkDestroyImage(hw_vk->device, s->vkimage, NULL);
   if (s->memory)
      hw_vkFreeMemory(hw_vk->device, s->memory, NULL);
   if (s->semaphore)
      hw_vkDestroySemaphore(hw_vk->device, s->semaphore, NULL);
   if (s->fence)
      hw_vkDestroyFence(hw_vk->device, s->fence, NULL);
   memset(s, 0, sizeof(*s));
}

static int hw_ring_slot_init(struct hw_ring_slot *s)
{
   VkCommandBufferAllocateInfo cai;
   VkSemaphoreCreateInfo sci;
   VkFenceCreateInfo fci;
   if (!hw_image_alloc(&s->vkimage, &s->memory, &s->image))
   {
      s->vkimage = VK_NULL_HANDLE;
      s->memory  = VK_NULL_HANDLE;
      return 0;
   }
   memset(&cai, 0, sizeof(cai));
   cai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   cai.commandPool        = hw_ring_pool;
   cai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   cai.commandBufferCount = 1;
   memset(&sci, 0, sizeof(sci));
   sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
   memset(&fci, 0, sizeof(fci));
   fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   return hw_vkAllocateCommandBuffers(hw_vk->device, &cai, &s->cmd) == VK_SUCCESS
       && hw_vkCreateSemaphore(hw_vk->device, &sci, NULL, &s->semaphore) == VK_SUCCESS
       && hw_vkCreateFence(hw_vk->device, &fci, NULL, &s->fence) == VK_SUCCESS;
}

/* The images go with the context. The frontend's reads of them are on
 * the queue, and so are its waits on their semaphores. */
static void hw_ring_free(void)
{
   unsigned i;
   if (!hw_ring_pool)
      return;
   hw_vk->lock_queue(hw_vk->handle);
   hw_vkQueueWaitIdle(hw_vk->queue);
   hw_vk->unlock_queue(hw_vk->handle);
   for (i = 0; i < HW_RING_SLOTS; i++)
      hw_ring_slot_free(&hw_ring_slots[i]);
   hw_vkDestroyCommandPool(hw_vk->device, hw_ring_pool, NULL);
   hw_ring_pool = VK_NULL_HANDLE;
}

/* The core's side of the sync index protocol: wait for the index, then
 * clear its image, which the frontend must no longer read, and hand it
 * over behind the clear's semaphore. */
static int hw_ring_send(void)
{
   VkCommandBufferBeginInfo cbi;
   VkSubmitInfo si;
   VkClearColorValue colour;
   struct hw_ring_slot *s;
   uint32_t index;

   if (!hw_ring_pool)
   {
      VkCommandPoolCreateInfo cpi;
      memset(&cpi, 0, sizeof(cpi));
      cpi.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
      cpi.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
      cpi.queueFamilyIndex = hw_vk->queue_index;
      if (hw_vkCreateCommandPool(hw_vk->device, &cpi, NULL, &hw_ring_pool) != VK_SUCCESS)
         return 0;
   }
   hw_vk->wait_sync_index(hw_vk->handle);
   index = hw_vk->get_sync_index(hw_vk->handle);
   if (index >= HW_RING_SLOTS)
      return 0;
   s = &hw_ring_slots[index];
   if (!s->semaphore && !hw_ring_slot_init(s))
   {
      hw_ring_slot_free(s);
      return 0;
   }
   if (s->pending)
   {
      hw_vkWaitForFences(hw_vk->device, 1, &s->fence, VK_TRUE, UINT64_MAX);
      hw_vkResetFences(hw_vk->device, 1, &s->fence);
      s->pending = 0;
      /* wait_sync_index says the frontend is done with the index, its
       * semaphore included. Every other use signals the same semaphore
       * again, a validation error if the frontend never waited it; the
       * rest recycle it, one if a wait of it is still on the queue. */
      if (s->uses++ & 1)
      {
         VkSemaphoreCreateInfo sci;
         hw_vkDestroySemaphore(hw_vk->device, s->semaphore, NULL);
         memset(&sci, 0, sizeof(sci));
         sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
         if (hw_vkCreateSemaphore(hw_vk->device, &sci, NULL, &s->semaphore) != VK_SUCCESS)
         {
            s->semaphore = VK_NULL_HANDLE;
            hw_ring_slot_free(s);
            return 0;
         }
      }
   }
   if (hw_ring_clear_hook)
   {
      uint64_t image = 0;
      memcpy(&image, &s->vkimage, sizeof(s->vkimage));
      hw_ring_clear_hook(image);
   }

   memset(&cbi, 0, sizeof(cbi));
   cbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   cbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   hw_vkBeginCommandBuffer(s->cmd, &cbi);
   hw_barrier(s->cmd, s->vkimage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         0, VK_ACCESS_TRANSFER_WRITE_BIT,
         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
   colour.float32[0] = (float)(runs & 0xff) / 255.0f;
   colour.float32[1] = 0.5f;
   colour.float32[2] = 0.25f;
   colour.float32[3] = 1.0f;
   hw_vkCmdClearColorImage(s->cmd, s->vkimage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         &colour, 1, &s->image.create_info.subresourceRange);
   hw_barrier(s->cmd, s->vkimage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
         VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
   hw_vkEndCommandBuffer(s->cmd);

   memset(&si, 0, sizeof(si));
   si.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   si.commandBufferCount   = 1;
   si.pCommandBuffers      = &s->cmd;
   si.signalSemaphoreCount = 1;
   si.pSignalSemaphores    = &s->semaphore;
   hw_vk->lock_queue(hw_vk->handle);
   if (hw_vkQueueSubmit(hw_vk->queue, 1, &si, s->fence) != VK_SUCCESS)
   {
      hw_vk->unlock_queue(hw_vk->handle);
      return 0;
   }
   hw_vk->unlock_queue(hw_vk->handle);
   s->pending = 1;
   hw_vk->set_image(hw_vk->handle, &s->image, 1, &s->semaphore,
         VK_QUEUE_FAMILY_IGNORED);
   hw_ring_sent++;
   return 1;
}

static void hw_context_reset(void)
{
   hw_vk = NULL;
   if (     !environ_cb(RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE, (void*)&hw_vk)
         || !hw_vk
         || hw_vk->interface_type != RETRO_HW_RENDER_INTERFACE_VULKAN)
   {
      hw_vk = NULL;
      return;
   }
   HW_SYM(vkCreateImage);
   HW_SYM(vkDestroyImage);
   HW_SYM(vkGetImageMemoryRequirements);
   HW_SYM(vkAllocateMemory);
   HW_SYM(vkFreeMemory);
   HW_SYM(vkBindImageMemory);
   HW_SYM(vkCreateImageView);
   HW_SYM(vkDestroyImageView);
   HW_SYM(vkCreateCommandPool);
   HW_SYM(vkDestroyCommandPool);
   HW_SYM(vkAllocateCommandBuffers);
   HW_SYM(vkBeginCommandBuffer);
   HW_SYM(vkEndCommandBuffer);
   HW_SYM(vkCmdPipelineBarrier);
   HW_SYM(vkCmdClearColorImage);
   HW_SYM(vkQueueSubmit);
   HW_SYM(vkQueueWaitIdle);
   HW_SYM(vkCreateFence);
   HW_SYM(vkDestroyFence);
   HW_SYM(vkWaitForFences);
   HW_SYM(vkResetFences);
   HW_SYM(vkCreateSemaphore);
   HW_SYM(vkDestroySemaphore);
   hw_vkGetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)
      hw_vk->get_instance_proc_addr(hw_vk->instance, "vkGetPhysicalDeviceMemoryProperties");
   if (!hw_withdrawn)
      hw_image_create();
}

static bool hw_create_device(struct retro_vulkan_context *context,
      VkInstance instance, VkPhysicalDevice gpu, VkSurfaceKHR surface,
      PFN_vkGetInstanceProcAddr gipa,
      const char **exts, unsigned num_exts,
      const char **layers, unsigned num_layers,
      const VkPhysicalDeviceFeatures *features)
{
   PFN_vkEnumeratePhysicalDevices enum_gpus = (PFN_vkEnumeratePhysicalDevices)
      gipa(instance, "vkEnumeratePhysicalDevices");
   PFN_vkGetPhysicalDeviceQueueFamilyProperties family_props =
      (PFN_vkGetPhysicalDeviceQueueFamilyProperties)
      gipa(instance, "vkGetPhysicalDeviceQueueFamilyProperties");
   PFN_vkGetPhysicalDeviceSurfaceSupportKHR surface_support =
      (PFN_vkGetPhysicalDeviceSurfaceSupportKHR)
      gipa(instance, "vkGetPhysicalDeviceSurfaceSupportKHR");
   PFN_vkCreateDevice create_device = (PFN_vkCreateDevice)
      gipa(instance, "vkCreateDevice");
   PFN_vkGetDeviceProcAddr gdpa = (PFN_vkGetDeviceProcAddr)
      gipa(instance, "vkGetDeviceProcAddr");
   PFN_vkGetDeviceQueue get_queue;
   VkQueueFamilyProperties props[16];
   VkDeviceQueueCreateInfo qi;
   VkDeviceCreateInfo di;
   VkDevice device       = VK_NULL_HANDLE;
   uint32_t count        = 16;
   uint32_t family       = UINT32_MAX;
   uint32_t i;
   static const float prio = 1.0f;

   if (!enum_gpus || !family_props || !create_device || !gdpa)
      return false;
   if (gpu == VK_NULL_HANDLE)
   {
      uint32_t one = 1;
      enum_gpus(instance, &one, &gpu);
      if (gpu == VK_NULL_HANDLE)
         return false;
   }
   family_props(gpu, &count, props);
   for (i = 0; i < count && family == UINT32_MAX; i++)
   {
      VkBool32 ok = VK_TRUE;
      if (!(props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
         continue;
      if (surface != VK_NULL_HANDLE && surface_support)
         surface_support(gpu, i, surface, &ok);
      if (ok)
         family = i;
   }
   if (family == UINT32_MAX)
      return false;

   memset(&qi, 0, sizeof(qi));
   qi.sType                   = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
   qi.queueFamilyIndex        = family;
   qi.queueCount              = 1;
   qi.pQueuePriorities        = &prio;
   memset(&di, 0, sizeof(di));
   di.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
   di.queueCreateInfoCount    = 1;
   di.pQueueCreateInfos       = &qi;
   di.enabledExtensionCount   = num_exts;
   di.ppEnabledExtensionNames = exts;
   di.enabledLayerCount       = num_layers;
   di.ppEnabledLayerNames     = layers;
   di.pEnabledFeatures        = features;
   if (create_device(gpu, &di, NULL, &device) != VK_SUCCESS)
      return false;

   get_queue = (PFN_vkGetDeviceQueue)gdpa(device, "vkGetDeviceQueue");
   context->gpu                             = gpu;
   context->device                          = device;
   context->queue_family_index              = family;
   context->presentation_queue_family_index = family;
   get_queue(device, family, 0, &context->queue);
   context->presentation_queue              = context->queue;
   return true;
}

/* Version 1: create_device, and the frontend destroys what it made. */
static const struct retro_hw_render_context_negotiation_interface_vulkan hw_negotiation = {
   RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN,
   1,
   NULL,
   hw_create_device,
   NULL
};

/* The image goes with the context, and the frontend is not told. */
static void hw_context_destroy(void)
{
   if (hw_vk)
   {
      hw_image_destroy();
      hw_ring_free();
   }
   hw_vk = NULL;
}

/* RETRO_API, as the exports further down are. 1 when the core is a
 * hardware core with its image up. */
RETRO_API unsigned harness_core_hw_active(void)
{
   return (hw_mode && hw_vk && hw_have_image) ? 1 : 0;
}

/* Withdraw the image and destroy it; retro_run pushes dupes until
 * harness_core_hw_restore(). */
RETRO_API unsigned harness_core_hw_withdraw(void)
{
   if (!hw_mode || !hw_vk || !hw_have_image)
      return 0;
   hw_vk->set_image(hw_vk->handle, NULL, 0, NULL, VK_QUEUE_FAMILY_IGNORED);
   hw_vk->wait_sync_index(hw_vk->handle);
   hw_image_destroy();
   hw_withdrawn = 1;
   return 1;
}

RETRO_API unsigned harness_core_hw_restore(void)
{
   if (!hw_mode || !hw_vk || hw_have_image)
      return 0;
   hw_withdrawn = 0;
   return hw_image_create() ? 1 : 0;
}

RETRO_API void harness_core_hw_ring(int mode) { hw_ring = mode; }

/* Images handed over in the ring mode. */
RETRO_API unsigned harness_core_hw_ring_sent(void) { return hw_ring_sent; }

RETRO_API void harness_core_hw_ring_on_clear(void (*hook)(uint64_t image))
{
   hw_ring_clear_hook = hook;
}

void retro_set_environment(retro_environment_t cb)
{
   bool no_content = true;
   environ_cb = cb;
   {
      const char *hw = getenv("HARNESS_CORE_HW_VULKAN");
      hw_mode       = (hw && *hw) ? 1 : 0;
      hw_own_device = (hw && *hw == '2') ? 1 : 0;
   }
   /* RGB565 by default; XRGB8888 with HARNESS_CORE_XRGB8888 set. The
    * two take different paths through a driver - Vulkan converts
    * RGB565 with a compute shader and samples a linear XRGB8888
    * image directly, and D3D12 lends its framebuffer only for a row
    * pitch on a 256-byte boundary, which 320 pixels reach at 4 bytes
    * and not at 2 - so the CI legs run the harness under both. */
   {
      const char *xrgb = getenv("HARNESS_CORE_XRGB8888");
      if (xrgb && *xrgb)
      {
         pixel_format = RETRO_PIXEL_FORMAT_XRGB8888;
         bpp          = 4;
      }
   }
   {
      /* Say what the frames below are, as a real core must. */
      enum retro_pixel_format fmt = pixel_format;
      cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
   }
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);
}
void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { (void)cb; }
void retro_set_input_poll(retro_input_poll_t cb) { (void)cb; }
void retro_set_input_state(retro_input_state_t cb) { (void)cb; }
void retro_init(void) { }
void retro_deinit(void) { }
unsigned retro_api_version(void) { return RETRO_API_VERSION; }
void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name    = "threaded_video_harness";
   info->library_version = "1";
   info->valid_extensions = "";
}
void retro_get_system_av_info(struct retro_system_av_info *info)
{
   memset(info, 0, sizeof(*info));
   info->timing.fps         = 60.0;
   info->timing.sample_rate = 48000.0;
   info->geometry.base_width   = W;
   info->geometry.base_height  = H;
   info->geometry.max_width    = W;
   info->geometry.max_height   = H;
   info->geometry.aspect_ratio = 4.0f / 3.0f;
}
void retro_set_controller_port_device(unsigned port, unsigned device) { (void)port; (void)device; }
void retro_reset(void) { }
/* Set by the harness through the core's own export below: when on,
 * retro_run asks the frontend for a framebuffer and renders into it,
 * exercising the wrapper's zero-copy lend. Counts how often the ask
 * was granted so the harness can check the lend actually happened.
 * Mode 2 renders the whole loan but pushes a cropped window into it -
 * a pointer past the start with the loan's pitch, the way a core that
 * crops overscan by offset does - which must still be a lend. */
#define CROP_X 8
#define CROP_Y 4
static int      harness_use_fb;
static unsigned harness_fb_granted;
/* Grants of memory the frontend did not report cached: slow to read
 * back on a device that maps its video memory (resizable BAR). */
static unsigned harness_fb_uncached;

/* RETRO_API, like the core's own entry points: a Windows DLL exports
 * only what is marked, and the harness looks these two up by name. */
RETRO_API void harness_core_use_framebuffer(int on) { harness_use_fb = on; }

/* The time each retro_run takes, spun on the frontend's clock: a real
 * core's cost, which the pacer reserves against.  0 runs as fast as
 * the frame can be written. */
static unsigned                harness_run_us;
static retro_perf_get_time_usec_t harness_time_usec;
/* How many times retro_run has run, for the lanes that check the core
 * keeps its own rate under a menu running at another */
RETRO_API unsigned harness_core_runs(void)
{
   return runs;
}

RETRO_API void harness_core_set_run_us(unsigned us)
{
   struct retro_perf_callback perf;
   harness_run_us = 0;
   if (!us || !environ_cb)
      return;
   memset(&perf, 0, sizeof(perf));
   if (     environ_cb(RETRO_ENVIRONMENT_GET_PERF_INTERFACE, &perf)
         && perf.get_time_usec)
   {
      harness_time_usec = perf.get_time_usec;
      harness_run_us    = us;
   }
}
RETRO_API unsigned harness_core_fb_granted(void)   { return harness_fb_granted; }
RETRO_API unsigned harness_core_fb_uncached(void)  { return harness_fb_uncached; }

void retro_run(void)
{
   unsigned h = (runs % 61 == 60) ? H_OVERSIZE : H;
   unsigned i;
   uint8_t *dst   = frame;
   size_t   pitch = W * bpp;
   unsigned out_w = W, out_h = h;
   const uint8_t *push = NULL;
   runs++;

   if (harness_run_us)
   {
      retro_time_t until = harness_time_usec() + (retro_time_t)harness_run_us;
      while (harness_time_usec() < until) { }
   }

   if (hw_mode && hw_vk && hw_ring)
   {
      if (runs % 3 == 0 && hw_ring == 2)
         video_cb(frame, W, H, W * bpp);
      else if ((runs % 3 != 0 || hw_ring == 3) && hw_ring_send())
         video_cb(RETRO_HW_FRAME_BUFFER_VALID, W, H, 0);
      else
         video_cb(NULL, W, H, 0);
      return;
   }

   if (hw_mode && hw_vk)
   {
      /* The same image every frame, and every third frame a dupe, as
       * the software frames below. Nothing but dupes while the image
       * is withdrawn. */
      if (hw_withdrawn || !hw_have_image || runs % 3 == 0)
         video_cb(NULL, W, H, 0);
      else
      {
         hw_vk->set_image(hw_vk->handle, &hw_image, 0, NULL,
               VK_QUEUE_FAMILY_IGNORED);
         video_cb(RETRO_HW_FRAME_BUFFER_VALID, W, H, 0);
      }
      return;
   }

   if (harness_use_fb && h == H)
   {
      struct retro_framebuffer fb;
      memset(&fb, 0, sizeof(fb));
      fb.width        = W;
      fb.height       = H;
      /* Read as well as write, as a core that snapshots its frame
       * for a wipe asks: the lend must not be refused for it. */
      fb.access_flags = RETRO_MEMORY_ACCESS_WRITE
                      | RETRO_MEMORY_ACCESS_READ;
      if (     environ_cb(RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER, &fb)
            && fb.format == pixel_format)
      {
         dst   = (uint8_t*)fb.data;
         pitch = fb.pitch;
         harness_fb_granted++;
         if (!(fb.memory_flags & RETRO_MEMORY_TYPE_CACHED))
            harness_fb_uncached++;
      }
   }

   /* Pixel i carries (run + i) in its low bits, at either size, so a
    * lane reading the first pixel as 16-bit sees the run number
    * move whichever format is on. Rows at the pitch: a loan may pad
    * them. */
   for (i = 0; i < W * h; i++)
   {
      uint8_t *px = dst + (i / W) * pitch + (i % W) * bpp;
      if (bpp == 4)
         *(uint32_t*)px = (uint32_t)(runs + i);
      else
         *(uint16_t*)px = (uint16_t)(runs + i);
   }
   push = dst;
   if (harness_use_fb == 2 && h == H)
   {
      /* Cropped window: CROP_Y rows down and CROP_X pixels in, at the
       * full pitch. Same for a loan and for the core's own buffer. */
      push  = dst + CROP_Y * pitch + CROP_X * bpp;
      out_w = W - 2 * CROP_X;
      out_h = H - 2 * CROP_Y;
   }
   if (runs % 3 == 0)
      video_cb(NULL, out_w, out_h, pitch);
   else
      video_cb(push, out_w, out_h, pitch);
}
size_t retro_serialize_size(void) { return 0; }
bool retro_serialize(void *data, size_t size) { (void)data; (void)size; return false; }
bool retro_unserialize(const void *data, size_t size) { (void)data; (void)size; return false; }
void retro_cheat_reset(void) { }
void retro_cheat_set(unsigned index, bool enabled, const char *code) { (void)index; (void)enabled; (void)code; }
bool retro_load_game(const struct retro_game_info *game)
{
   (void)game;
   if (hw_mode)
   {
      memset(&hw_cb, 0, sizeof(hw_cb));
      hw_cb.context_type    = RETRO_HW_CONTEXT_VULKAN;
      hw_cb.version_major   = VK_MAKE_VERSION(1, 0, 18);
      hw_cb.context_reset   = hw_context_reset;
      hw_cb.context_destroy = hw_context_destroy;
      /* Refused (another driver): a software core after all. */
      if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_cb))
         hw_mode = 0;
      else if (hw_own_device)
         environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE,
               (void*)&hw_negotiation);
   }
   return true;
}
bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num) { (void)type; (void)info; (void)num; return false; }
void retro_unload_game(void) { }
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
void *retro_get_memory_data(unsigned id) { (void)id; return NULL; }
size_t retro_get_memory_size(unsigned id) { (void)id; return 0; }
