/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* Core for checking RetroArch's view presentation: it sends a view map
 * (RETRO_ENVIRONMENT_SET_VIDEO_VIEWS), draws every view in its own solid
 * colour with a white marker at its top-left corner, follows
 * RETRO_ENVIRONMENT_GET_VIDEO_VIEWS_STATUS, and logs where touches land
 * in the packed frame. It sends its map even without
 * RETRO_VIDEO_VIEWS_STATUS_PRESENTS, so the frontend's answer there is
 * logged too. It draws in software, or with the video_views_test_hw
 * option through a GL core context, with either origin, or into
 * Vulkan images of its own handed over with set_image. With
 * video_views_test_max it declares a far larger maximum than it draws.
 * e2e/run.py checks the colours on screen and the status in the log.
 *
 * In the Vulkan modes context_destroy first waits on the device without
 * the queue lock, as a core draining its work may, and logs when:
 * samples/openxr/e2e/run.py checks nothing of the frontend's used the
 * queue meanwhile. vulkan_keep keeps its context over video reinits.
 *
 * It also logs its pads, analog values and light gun when they change,
 * and rumbles a port while it holds Start, for the headset input tests.
 *
 * With video_views_test_fps it reports another frame rate, and logs
 * every tenth frame with the monotonic time, for the headset pacing
 * tests. With video_views_test_pattern checker it fills each view with
 * a one-pixel black and white checkerboard instead of its colour, in
 * software or its own Vulkan images, for the headset's shrinking. */
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <libretro.h>
#include <libretro_vulkan.h>

#define MAX_W 800
#define MAX_H 480
/* The crop map's frame, past the declared maximum. Threaded video keeps
 * next_pow2(MAX_W)^2 pixels of it: 655 rows at this width. */
#define CROP_W 1600
#define CROP_H 960
/* Azahar's maximum, at 10x. */
#define LARGE_MAX_W 8000
#define LARGE_MAX_H 4800

#define COL_BG     0x202020
#define COL_RED    0xFF0000
#define COL_BLUE   0x0000FF
#define COL_GREEN  0x00FF00
#define COL_YELLOW 0xFFFF00
#define COL_WHITE  0xFFFFFF
#define COL_BLACK  0x000000

enum map_kind
{
   MAP_3DS = 0,
   MAP_3DS_FORCE,
   MAP_DS,
   MAP_VB,
   MAP_INVALID,
   MAP_NONE,
   MAP_CROP
};

enum hw_kind
{
   HW_OFF = 0,
   HW_GL,         /* bottom-left origin */
   HW_GL_TOPLEFT,
   HW_VULKAN,
   HW_VULKAN_KEEP
};

/* The GL the hardware mode uses, loaded through the frontend's
 * get_proc_address: the core links no GL library. */
#ifndef APIENTRY
#ifdef _WIN32
#define APIENTRY __stdcall
#else
#define APIENTRY
#endif
#endif
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_SCISSOR_TEST     0x0C11
#define GL_FRAMEBUFFER      0x8D40
typedef void (APIENTRY *gl_bind_framebuffer_t)(unsigned, unsigned);
typedef void (APIENTRY *gl_enable_t)(unsigned);
typedef void (APIENTRY *gl_scissor_t)(int, int, int, int);
typedef void (APIENTRY *gl_clear_color_t)(float, float, float, float);
typedef void (APIENTRY *gl_clear_t)(unsigned);

/* The Vulkan modes' device functions, from the frontend's
 * get_device_proc_addr: the core links no Vulkan loader either. */
struct vk_funcs
{
   PFN_vkCreateImage                 vkCreateImage;
   PFN_vkDestroyImage                vkDestroyImage;
   PFN_vkGetImageMemoryRequirements  vkGetImageMemoryRequirements;
   PFN_vkBindImageMemory             vkBindImageMemory;
   PFN_vkCreateImageView             vkCreateImageView;
   PFN_vkDestroyImageView            vkDestroyImageView;
   PFN_vkCreateBuffer                vkCreateBuffer;
   PFN_vkDestroyBuffer               vkDestroyBuffer;
   PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
   PFN_vkBindBufferMemory            vkBindBufferMemory;
   PFN_vkAllocateMemory              vkAllocateMemory;
   PFN_vkFreeMemory                  vkFreeMemory;
   PFN_vkMapMemory                   vkMapMemory;
   PFN_vkCreateCommandPool           vkCreateCommandPool;
   PFN_vkDestroyCommandPool          vkDestroyCommandPool;
   PFN_vkAllocateCommandBuffers      vkAllocateCommandBuffers;
   PFN_vkBeginCommandBuffer          vkBeginCommandBuffer;
   PFN_vkEndCommandBuffer            vkEndCommandBuffer;
   PFN_vkCmdPipelineBarrier          vkCmdPipelineBarrier;
   PFN_vkCmdCopyBufferToImage        vkCmdCopyBufferToImage;
   PFN_vkQueueSubmit                 vkQueueSubmit;
   PFN_vkDeviceWaitIdle              vkDeviceWaitIdle;
   PFN_vkCreateFence                 vkCreateFence;
   PFN_vkDestroyFence                vkDestroyFence;
   PFN_vkWaitForFences               vkWaitForFences;
   PFN_vkResetFences                 vkResetFences;
   PFN_vkCreateSemaphore             vkCreateSemaphore;
   PFN_vkDestroySemaphore            vkDestroySemaphore;
};

/* A sync index's frame: the image handed over, the host copy of the
 * frame it is filled from, and the submission filling it. */
struct vk_slot
{
   struct retro_vulkan_image image;
   VkDeviceMemory  image_memory;
   VkBuffer        buffer;
   VkDeviceMemory  buffer_memory;
   void           *mapped;
   VkCommandBuffer cmd;
   VkFence         fence;
   VkSemaphore     semaphore;
   bool            pending;
};

/* One per bit get_sync_index_mask can set. */
#define VK_SLOTS 32

static retro_environment_t   environ_cb;
static retro_video_refresh_t video_cb;
static retro_input_poll_t    input_poll_cb;
static retro_input_state_t   input_state_cb;
static retro_log_printf_t    log_cb;

static uint32_t frame_buf[CROP_W * CROP_H];
static enum map_kind map_kind = MAP_3DS;
static enum hw_kind hw_kind   = HW_OFF;
static bool large_max;
static bool checker;
static double core_fps = 60.0;
static unsigned frames_run;
static struct retro_hw_render_callback hw_render;
static gl_bind_framebuffer_t p_glBindFramebuffer;
static gl_enable_t           p_glEnable;
static gl_enable_t           p_glDisable;
static gl_scissor_t          p_glScissor;
static gl_clear_color_t      p_glClearColor;
static gl_clear_t            p_glClear;
static bool gl_ready;
static const struct retro_hw_render_interface_vulkan *vk;
static struct vk_funcs vkf;
static VkPhysicalDeviceMemoryProperties vk_memory;
static VkCommandPool vk_pool;
static struct vk_slot vk_slots[VK_SLOTS];
static bool vk_ready;
static int last_status        = -1;
static int last_accepted      = -1;
static int last_pressed       = -1;
static int16_t last_px, last_py;
static struct retro_rumble_interface rumble;
static int last_pad[2][7];      /* buttons, lx, ly, rx, ry, l2, r2 */
static bool pad_logged[2];
static int last_gun[4];         /* x, y, offscreen, trigger */
static bool gun_logged;
static unsigned last_rumble[2];

static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
   va_list va;
   (void)level;
   va_start(va, fmt);
   vfprintf(stderr, fmt, va);
   va_end(va);
}

static struct retro_video_view mkview(unsigned x, unsigned y,
      unsigned w, unsigned h, unsigned screen, unsigned eye)
{
   struct retro_video_view v;
   v.x            = x;
   v.y            = y;
   v.width        = w;
   v.height       = h;
   v.screen       = screen;
   v.eye          = eye;
   v.aspect_ratio = 0.0f;
   return v;
}

static void fill(unsigned x, unsigned y, unsigned w, unsigned h,
      unsigned fw, uint32_t c)
{
   unsigned i, j;
   for (j = y; j < y + h; j++)
      for (i = x; i < x + w; i++)
         frame_buf[j * fw + i] = c;
}

/* By frame position, so neighbouring views meet seamlessly. */
static void fill_checker(unsigned x, unsigned y, unsigned w, unsigned h,
      unsigned fw)
{
   unsigned i, j;
   for (j = y; j < y + h; j++)
      for (i = x; i < x + w; i++)
         frame_buf[j * fw + i] = ((i ^ j) & 1) ? COL_WHITE : COL_BLACK;
}

/* A top-left rectangle of the frame, cleared to c through the scissor.
 * With a bottom-left origin, GL counts rows from the frame's bottom. */
static void gl_rect(unsigned x, unsigned y, unsigned w, unsigned h,
      unsigned fh, uint32_t c)
{
   int gy = (hw_kind == HW_GL) ? (int)(fh - (y + h)) : (int)y;
   p_glScissor((int)x, gy, (int)w, (int)h);
   p_glClearColor(((c >> 16) & 0xff) / 255.0f, ((c >> 8) & 0xff) / 255.0f,
         (c & 0xff) / 255.0f, 1.0f);
   p_glClear(GL_COLOR_BUFFER_BIT);
}

static void rect(unsigned x, unsigned y, unsigned w, unsigned h,
      unsigned fw, unsigned fh, uint32_t c)
{
   if (hw_kind == HW_GL || hw_kind == HW_GL_TOPLEFT)
      gl_rect(x, y, w, h, fh, c);
   else
      fill(x, y, w, h, fw, c);
}

static void context_reset(void)
{
   p_glBindFramebuffer = (gl_bind_framebuffer_t)
      hw_render.get_proc_address("glBindFramebuffer");
   p_glEnable          = (gl_enable_t)hw_render.get_proc_address("glEnable");
   p_glDisable         = (gl_enable_t)hw_render.get_proc_address("glDisable");
   p_glScissor         = (gl_scissor_t)hw_render.get_proc_address("glScissor");
   p_glClearColor      = (gl_clear_color_t)
      hw_render.get_proc_address("glClearColor");
   p_glClear           = (gl_clear_t)hw_render.get_proc_address("glClear");
   gl_ready            = p_glBindFramebuffer && p_glEnable && p_glDisable
      && p_glScissor && p_glClearColor && p_glClear;
   if (!gl_ready)
      log_cb(RETRO_LOG_ERROR, "[video_views] GL entry points missing\n");
}

static void context_destroy(void)
{
   gl_ready = false;
}

static long long now_us(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return (long long)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static bool vk_alloc(const VkMemoryRequirements *req,
      VkMemoryPropertyFlags flags, VkDeviceMemory *mem)
{
   uint32_t i;
   VkMemoryAllocateInfo info;
   for (i = 0; i < vk_memory.memoryTypeCount; i++)
      if (     (req->memoryTypeBits & (1u << i))
            && (vk_memory.memoryTypes[i].propertyFlags & flags) == flags)
         break;
   if (i == vk_memory.memoryTypeCount)
      return false;
   memset(&info, 0, sizeof(info));
   info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   info.allocationSize  = req->size;
   info.memoryTypeIndex = i;
   return vkf.vkAllocateMemory(vk->device, &info, NULL, mem) == VK_SUCCESS;
}

/* Command buffers go with the pool. */
static void vk_slot_free(struct vk_slot *s)
{
   VkDevice dev = vk->device;
   if (s->image.image_view)
      vkf.vkDestroyImageView(dev, s->image.image_view, NULL);
   if (s->image.create_info.image)
      vkf.vkDestroyImage(dev, s->image.create_info.image, NULL);
   if (s->image_memory)
      vkf.vkFreeMemory(dev, s->image_memory, NULL);
   if (s->buffer)
      vkf.vkDestroyBuffer(dev, s->buffer, NULL);
   if (s->buffer_memory)
      vkf.vkFreeMemory(dev, s->buffer_memory, NULL);
   if (s->fence)
      vkf.vkDestroyFence(dev, s->fence, NULL);
   if (s->semaphore)
      vkf.vkDestroySemaphore(dev, s->semaphore, NULL);
   memset(s, 0, sizeof(*s));
}

static bool vk_slot_init(struct vk_slot *s, unsigned fw, unsigned fh)
{
   VkImageCreateInfo image;
   VkBufferCreateInfo buffer;
   VkMemoryRequirements req;
   VkCommandBufferAllocateInfo cmd;
   VkFenceCreateInfo fence;
   VkSemaphoreCreateInfo sem;
   VkImageViewCreateInfo *view = &s->image.create_info;
   VkDevice dev                = vk->device;

   memset(&image, 0, sizeof(image));
   image.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   /* The frontend may view an 8-bit format as sRGB. */
   image.flags         = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
   image.imageType     = VK_IMAGE_TYPE_2D;
   image.format        = VK_FORMAT_B8G8R8A8_UNORM;
   image.extent.width  = fw;
   image.extent.height = fh;
   image.extent.depth  = 1;
   image.mipLevels     = 1;
   image.arrayLayers   = 1;
   image.samples       = VK_SAMPLE_COUNT_1_BIT;
   image.tiling        = VK_IMAGE_TILING_OPTIMAL;
   image.usage         = VK_IMAGE_USAGE_TRANSFER_SRC_BIT
      | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
   image.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
   image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

   memset(&buffer, 0, sizeof(buffer));
   buffer.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   buffer.size        = (VkDeviceSize)fw * fh * sizeof(uint32_t);
   buffer.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
   buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

   memset(view, 0, sizeof(*view));
   view->sType                       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   view->viewType                    = VK_IMAGE_VIEW_TYPE_2D;
   view->format                      = image.format;
   view->subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   view->subresourceRange.levelCount = 1;
   view->subresourceRange.layerCount = 1;

   memset(&cmd, 0, sizeof(cmd));
   cmd.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   cmd.commandPool        = vk_pool;
   cmd.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   cmd.commandBufferCount = 1;
   memset(&fence, 0, sizeof(fence));
   fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   memset(&sem, 0, sizeof(sem));
   sem.sType   = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

   if (vkf.vkCreateImage(dev, &image, NULL, &view->image) != VK_SUCCESS)
      return false;
   vkf.vkGetImageMemoryRequirements(dev, view->image, &req);
   if (     !vk_alloc(&req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
               &s->image_memory)
         || vkf.vkBindImageMemory(dev, view->image, s->image_memory, 0)
            != VK_SUCCESS
         || vkf.vkCreateImageView(dev, view, NULL, &s->image.image_view)
            != VK_SUCCESS
         || vkf.vkCreateBuffer(dev, &buffer, NULL, &s->buffer) != VK_SUCCESS)
      return false;
   vkf.vkGetBufferMemoryRequirements(dev, s->buffer, &req);
   if (     !vk_alloc(&req, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
               | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &s->buffer_memory)
         || vkf.vkBindBufferMemory(dev, s->buffer, s->buffer_memory, 0)
            != VK_SUCCESS
         || vkf.vkMapMemory(dev, s->buffer_memory, 0, VK_WHOLE_SIZE, 0,
            &s->mapped) != VK_SUCCESS
         || vkf.vkAllocateCommandBuffers(dev, &cmd, &s->cmd) != VK_SUCCESS
         || vkf.vkCreateFence(dev, &fence, NULL, &s->fence) != VK_SUCCESS
         || vkf.vkCreateSemaphore(dev, &sem, NULL, &s->semaphore)
            != VK_SUCCESS)
      return false;
   s->image.image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   return true;
}

#define VK_LOAD(name) ok = (vkf.name = (PFN_##name) \
      vk->get_device_proc_addr(vk->device, #name)) && ok

static void vk_context_reset(void)
{
   VkCommandPoolCreateInfo pool;
   PFN_vkGetPhysicalDeviceMemoryProperties get_memory;
   bool ok  = true;
   vk_ready = false;
   vk       = NULL;
   if (     !environ_cb(RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE,
               (void*)&vk)
         || !vk || vk->interface_type != RETRO_HW_RENDER_INTERFACE_VULKAN)
   {
      log_cb(RETRO_LOG_ERROR, "[video_views] no Vulkan interface\n");
      return;
   }
   get_memory = (PFN_vkGetPhysicalDeviceMemoryProperties)
      vk->get_instance_proc_addr(vk->instance,
            "vkGetPhysicalDeviceMemoryProperties");
   VK_LOAD(vkCreateImage);
   VK_LOAD(vkDestroyImage);
   VK_LOAD(vkGetImageMemoryRequirements);
   VK_LOAD(vkBindImageMemory);
   VK_LOAD(vkCreateImageView);
   VK_LOAD(vkDestroyImageView);
   VK_LOAD(vkCreateBuffer);
   VK_LOAD(vkDestroyBuffer);
   VK_LOAD(vkGetBufferMemoryRequirements);
   VK_LOAD(vkBindBufferMemory);
   VK_LOAD(vkAllocateMemory);
   VK_LOAD(vkFreeMemory);
   VK_LOAD(vkMapMemory);
   VK_LOAD(vkCreateCommandPool);
   VK_LOAD(vkDestroyCommandPool);
   VK_LOAD(vkAllocateCommandBuffers);
   VK_LOAD(vkBeginCommandBuffer);
   VK_LOAD(vkEndCommandBuffer);
   VK_LOAD(vkCmdPipelineBarrier);
   VK_LOAD(vkCmdCopyBufferToImage);
   VK_LOAD(vkQueueSubmit);
   VK_LOAD(vkDeviceWaitIdle);
   VK_LOAD(vkCreateFence);
   VK_LOAD(vkDestroyFence);
   VK_LOAD(vkWaitForFences);
   VK_LOAD(vkResetFences);
   VK_LOAD(vkCreateSemaphore);
   VK_LOAD(vkDestroySemaphore);
   if (!ok || !get_memory)
   {
      log_cb(RETRO_LOG_ERROR, "[video_views] Vulkan entry points missing\n");
      return;
   }
   get_memory(vk->gpu, &vk_memory);

   memset(&pool, 0, sizeof(pool));
   pool.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   pool.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   pool.queueFamilyIndex = vk->queue_index;
   if (vkf.vkCreateCommandPool(vk->device, &pool, NULL, &vk_pool)
         != VK_SUCCESS)
   {
      log_cb(RETRO_LOG_ERROR, "[video_views] no Vulkan command pool\n");
      return;
   }
   vk_ready = true;
   log_cb(RETRO_LOG_INFO, "[video_views] Vulkan context reset\n");
}

/* Half a second of waits, so a submission beside them is caught. */
static void vk_context_destroy(void)
{
   unsigned i;
   long long t0 = now_us();
   long long t1 = t0;
   if (!vk_ready)
      return;
   while (t1 - t0 < 500000)
   {
      vkf.vkDeviceWaitIdle(vk->device);
      t1 = now_us();
   }
   log_cb(RETRO_LOG_INFO,
         "[video_views] context_destroy waited on the device from %lld to %lld us\n",
         t0, t1);
   for (i = 0; i < VK_SLOTS; i++)
      vk_slot_free(&vk_slots[i]);
   vkf.vkDestroyCommandPool(vk->device, vk_pool, NULL);
   vk_pool  = VK_NULL_HANDLE;
   vk_ready = false;
}

/* Copies frame_buf into the image for this sync index and hands the
 * image over, behind a semaphore the copy signals. */
static bool vk_send(unsigned fw, unsigned fh)
{
   VkCommandBufferBeginInfo begin;
   VkImageMemoryBarrier barrier;
   VkBufferImageCopy region;
   VkSubmitInfo submit;
   VkResult res;
   struct vk_slot *s;
   uint32_t index;

   vk->wait_sync_index(vk->handle);
   index = vk->get_sync_index(vk->handle);
   if (index >= VK_SLOTS)
      return false;
   s = &vk_slots[index];
   if (!s->semaphore && !vk_slot_init(s, fw, fh))
   {
      log_cb(RETRO_LOG_ERROR, "[video_views] Vulkan image %u not made\n",
            index);
      vk_slot_free(s);
      return false;
   }
   if (s->pending)
   {
      vkf.vkWaitForFences(vk->device, 1, &s->fence, VK_TRUE, UINT64_MAX);
      vkf.vkResetFences(vk->device, 1, &s->fence);
      s->pending = false;
   }
   memcpy(s->mapped, frame_buf, (size_t)fw * fh * sizeof(uint32_t));

   memset(&begin, 0, sizeof(begin));
   begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   vkf.vkBeginCommandBuffer(s->cmd, &begin);

   memset(&barrier, 0, sizeof(barrier));
   barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
   barrier.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
   barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
   barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
   barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   barrier.image               = s->image.create_info.image;
   barrier.subresourceRange    = s->image.create_info.subresourceRange;
   vkf.vkCmdPipelineBarrier(s->cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

   memset(&region, 0, sizeof(region));
   region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   region.imageSubresource.layerCount = 1;
   region.imageExtent.width           = fw;
   region.imageExtent.height          = fh;
   region.imageExtent.depth           = 1;
   vkf.vkCmdCopyBufferToImage(s->cmd, s->buffer, barrier.image,
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

   barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
   barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
   barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
   barrier.newLayout     = s->image.image_layout;
   vkf.vkCmdPipelineBarrier(s->cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL,
         1, &barrier);
   vkf.vkEndCommandBuffer(s->cmd);

   memset(&submit, 0, sizeof(submit));
   submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submit.commandBufferCount   = 1;
   submit.pCommandBuffers      = &s->cmd;
   submit.signalSemaphoreCount = 1;
   submit.pSignalSemaphores    = &s->semaphore;
   vk->lock_queue(vk->handle);
   res = vkf.vkQueueSubmit(vk->queue, 1, &submit, s->fence);
   vk->unlock_queue(vk->handle);
   if (res != VK_SUCCESS)
      return false;
   s->pending = true;
   vk->set_image(vk->handle, &s->image, 1, &s->semaphore,
         VK_QUEUE_FAMILY_IGNORED);
   return true;
}

static void log_status(unsigned status, int accepted, unsigned n)
{
   if ((int)status == last_status && accepted == last_accepted)
      return;
   log_cb(RETRO_LOG_INFO,
         "[video_views] presents=%d stereo=%d accepted=%d views=%u\n",
         (status & RETRO_VIDEO_VIEWS_STATUS_PRESENTS) ? 1 : 0,
         (status & RETRO_VIDEO_VIEWS_STATUS_STEREO) ? 1 : 0, accepted, n);
   last_status   = (int)status;
   last_accepted = accepted;
}

static void frame_size(unsigned *fw, unsigned *fh)
{
   switch (map_kind)
   {
      case MAP_DS: *fw = 256; *fh = 384; break;
      case MAP_VB: *fw = 768; *fh = 224; break;
      case MAP_CROP: *fw = CROP_W; *fh = CROP_H; break;
      default:     *fw = 800; *fh = 480; break;
   }
}

static unsigned build_map(struct retro_video_view *v, uint32_t *c,
      bool stereo)
{
   switch (map_kind)
   {
      case MAP_DS:
         v[0] = mkview(0,   0, 256, 192, 0, RETRO_VIDEO_VIEW_EYE_NONE);
         v[1] = mkview(0, 192, 256, 192, 1, RETRO_VIDEO_VIEW_EYE_NONE);
         c[0] = COL_GREEN;
         c[1] = COL_YELLOW;
         return 2;
      case MAP_VB:
         if (!stereo)
         {
            v[0] = mkview(0, 0, 384, 224, 0, RETRO_VIDEO_VIEW_EYE_NONE);
            c[0] = COL_GREEN;
            return 1;
         }
         v[0] = mkview(  0, 0, 384, 224, 0, RETRO_VIDEO_VIEW_EYE_LEFT);
         v[1] = mkview(384, 0, 384, 224, 0, RETRO_VIDEO_VIEW_EYE_RIGHT);
         c[0] = COL_RED;
         c[1] = COL_BLUE;
         return 2;
      case MAP_CROP:
         /* The second screen lies past the rows threaded video keeps. */
         v[0] = mkview(0,   0, CROP_W, 480, 0, RETRO_VIDEO_VIEW_EYE_NONE);
         v[1] = mkview(0, 480, CROP_W, 480, 1, RETRO_VIDEO_VIEW_EYE_NONE);
         c[0] = COL_GREEN;
         c[1] = COL_YELLOW;
         return 2;
      case MAP_INVALID:
         /* A left eye with no right: the frontend must refuse it. */
         v[0] = mkview(0, 0, 400, 240, 0, RETRO_VIDEO_VIEW_EYE_LEFT);
         c[0] = COL_RED;
         return 1;
      default:
         if (stereo || map_kind == MAP_3DS_FORCE)
         {
            v[0] = mkview(  0,   0, 400, 240, 0, RETRO_VIDEO_VIEW_EYE_LEFT);
            v[1] = mkview(400,   0, 400, 240, 0, RETRO_VIDEO_VIEW_EYE_RIGHT);
            v[2] = mkview(240, 240, 320, 240, 1, RETRO_VIDEO_VIEW_EYE_NONE);
            c[0] = COL_RED;
            c[1] = COL_BLUE;
            c[2] = COL_YELLOW;
            return 3;
         }
         v[0] = mkview(  0,   0, 400, 240, 0, RETRO_VIDEO_VIEW_EYE_NONE);
         v[1] = mkview(240, 240, 320, 240, 1, RETRO_VIDEO_VIEW_EYE_NONE);
         c[0] = COL_GREEN;
         c[1] = COL_YELLOW;
         return 2;
   }
}

static void read_options(void)
{
   struct retro_variable var;
   var.key   = "video_views_test_hw";
   var.value = NULL;
   hw_kind   = HW_OFF;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "gl"))
         hw_kind = HW_GL;
      else if (!strcmp(var.value, "gl_topleft"))
         hw_kind = HW_GL_TOPLEFT;
      else if (!strcmp(var.value, "vulkan"))
         hw_kind = HW_VULKAN;
      else if (!strcmp(var.value, "vulkan_keep"))
         hw_kind = HW_VULKAN_KEEP;
   }

   var.key   = "video_views_test_max";
   var.value = NULL;
   large_max = environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var)
      && var.value && !strcmp(var.value, "large");

   var.key   = "video_views_test_pattern";
   var.value = NULL;
   checker   = environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var)
      && var.value && !strcmp(var.value, "checker");

   var.key   = "video_views_test_fps";
   var.value = NULL;
   core_fps  = 60.0;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      core_fps = atof(var.value);
   if (core_fps <= 0.0)
      core_fps = 60.0;

   var.key   = "video_views_test_map";
   var.value = NULL;
   map_kind  = MAP_3DS;
   if (!environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) || !var.value)
      return;
   if (!strcmp(var.value, "3ds_force"))
      map_kind = MAP_3DS_FORCE;
   else if (!strcmp(var.value, "ds"))
      map_kind = MAP_DS;
   else if (!strcmp(var.value, "vb"))
      map_kind = MAP_VB;
   else if (!strcmp(var.value, "invalid"))
      map_kind = MAP_INVALID;
   else if (!strcmp(var.value, "none"))
      map_kind = MAP_NONE;
   else if (!strcmp(var.value, "crop"))
      map_kind = MAP_CROP;
}

/* A core may show or hide options by the views status; this one only
 * logs it, for the e2e checks that the frontend asks after a change. */
static bool options_update_display(void)
{
   unsigned status = 0;
   if (!environ_cb(RETRO_ENVIRONMENT_GET_VIDEO_VIEWS_STATUS, &status))
      status = 0;
   log_cb(RETRO_LOG_INFO, "[video_views] options display status=%u\n",
         status);
   return false;
}

void retro_set_environment(retro_environment_t cb)
{
   static const struct retro_variable vars[] = {
      { "video_views_test_map",
        "View map; 3ds|3ds_force|ds|vb|invalid|none|crop" },
      { "video_views_test_hw",
        "Hardware rendering; off|gl|gl_topleft|vulkan|vulkan_keep" },
      { "video_views_test_max",
        "Declared maximum size; normal|large" },
      { "video_views_test_fps",
        "Frame rate; 60|10|16" },
      { "video_views_test_pattern",
        "View fill; solid|checker" },
      { NULL, NULL }
   };
   struct retro_log_callback logging;
   struct retro_core_options_update_display_callback update_display;
   bool no_content = true;

   environ_cb = cb;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);
   cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)vars);
   log_cb = fallback_log;
   if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging) && logging.log)
      log_cb = logging.log;
   update_display.callback = options_update_display;
   cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK,
         &update_display);
}

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { (void)cb; }
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

void retro_init(void) { }
void retro_deinit(void) { }
unsigned retro_api_version(void) { return RETRO_API_VERSION; }

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "video_views test";
   info->library_version  = "1";
   info->valid_extensions = "";
   info->need_fullpath    = false;
   info->block_extract    = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   unsigned fw, fh;
   frame_size(&fw, &fh);
   memset(info, 0, sizeof(*info));
   info->geometry.base_width   = fw;
   info->geometry.base_height  = fh;
   info->geometry.max_width    = large_max ? LARGE_MAX_W : MAX_W;
   info->geometry.max_height   = large_max ? LARGE_MAX_H : MAX_H;
   info->geometry.aspect_ratio = (float)fw / (float)fh;
   info->timing.fps            = core_fps;
   info->timing.sample_rate    = 48000.0;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   (void)port;
   (void)device;
}

void retro_reset(void) { }

static void log_pads(void)
{
   unsigned p;
   for (p = 0; p < 2; p++)
   {
      int s[7];
      s[0] = (int)(uint16_t)input_state_cb(p, RETRO_DEVICE_JOYPAD, 0,
            RETRO_DEVICE_ID_JOYPAD_MASK);
      s[1] = input_state_cb(p, RETRO_DEVICE_ANALOG,
            RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
      s[2] = input_state_cb(p, RETRO_DEVICE_ANALOG,
            RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);
      s[3] = input_state_cb(p, RETRO_DEVICE_ANALOG,
            RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);
      s[4] = input_state_cb(p, RETRO_DEVICE_ANALOG,
            RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);
      s[5] = input_state_cb(p, RETRO_DEVICE_ANALOG,
            RETRO_DEVICE_INDEX_ANALOG_BUTTON, RETRO_DEVICE_ID_JOYPAD_L2);
      s[6] = input_state_cb(p, RETRO_DEVICE_ANALOG,
            RETRO_DEVICE_INDEX_ANALOG_BUTTON, RETRO_DEVICE_ID_JOYPAD_R2);
      if (pad_logged[p] && !memcmp(s, last_pad[p], sizeof(s)))
         continue;
      log_cb(RETRO_LOG_INFO,
            "[video_views] pad port=%u buttons=0x%04x lx=%d ly=%d rx=%d ry=%d l2=%d r2=%d\n",
            p, (unsigned)s[0], s[1], s[2], s[3], s[4], s[5], s[6]);
      memcpy(last_pad[p], s, sizeof(s));
      pad_logged[p] = true;
   }
}

static void log_lightgun(unsigned fw, unsigned fh)
{
   int s[4];
   int cx = -1;
   int cy = -1;
   s[0] = input_state_cb(0, RETRO_DEVICE_LIGHTGUN, 0,
         RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X);
   s[1] = input_state_cb(0, RETRO_DEVICE_LIGHTGUN, 0,
         RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y);
   s[2] = input_state_cb(0, RETRO_DEVICE_LIGHTGUN, 0,
         RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN) ? 1 : 0;
   s[3] = input_state_cb(0, RETRO_DEVICE_LIGHTGUN, 0,
         RETRO_DEVICE_ID_LIGHTGUN_TRIGGER) ? 1 : 0;
   if (gun_logged && !memcmp(s, last_gun, sizeof(s)))
      return;
   if (!s[2])
   {
      cx = (int)(((long)s[0] + 0x7fff) * (long)(fw - 1) / 0xfffe);
      cy = (int)(((long)s[1] + 0x7fff) * (long)(fh - 1) / 0xfffe);
   }
   log_cb(RETRO_LOG_INFO,
         "[video_views] lightgun x=%d y=%d offscreen=%d trigger=%d packed=(%d,%d)\n",
         s[0], s[1], s[2], s[3], cx, cy);
   memcpy(last_gun, s, sizeof(s));
   gun_logged = true;
}

/* A port rumbles while it holds Start; each port at its own strength. */
static void update_rumble(void)
{
   static const uint16_t strong[2] = { 0xC000, 0x8000 };
   static const uint16_t weak[2]   = { 0x4000, 0x2000 };
   unsigned p;
   if (!rumble.set_rumble_state)
      return;
   for (p = 0; p < 2; p++)
   {
      unsigned on = ((unsigned)last_pad[p][0] >> RETRO_DEVICE_ID_JOYPAD_START) & 1;
      if (on == last_rumble[p])
         continue;
      rumble.set_rumble_state(p, RETRO_RUMBLE_STRONG, on ? strong[p] : 0);
      rumble.set_rumble_state(p, RETRO_RUMBLE_WEAK,   on ? weak[p]   : 0);
      log_cb(RETRO_LOG_INFO, "[video_views] rumble port=%u strong=%u weak=%u\n",
            p, on ? (unsigned)strong[p] : 0u, on ? (unsigned)weak[p] : 0u);
      last_rumble[p] = on;
   }
}

void retro_run(void)
{
   struct retro_video_view v[RETRO_VIDEO_VIEWS_MAX];
   uint32_t c[RETRO_VIDEO_VIEWS_MAX];
   struct retro_video_views views;
   unsigned i, n, fw, fh;
   int16_t px, py;
   int pressed;
   int accepted    = 0;
   unsigned status = 0;
   bool stereo;

   if (core_fps != 60.0 && !(frames_run % 10))
   {
      struct timespec ts;
      clock_gettime(CLOCK_MONOTONIC, &ts);
      log_cb(RETRO_LOG_INFO, "[video_views] frame %u at %lld us\n",
            frames_run,
            (long long)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000);
   }
   frames_run++;

   input_poll_cb();
   if (!environ_cb(RETRO_ENVIRONMENT_GET_VIDEO_VIEWS_STATUS, &status))
      status = 0;
   stereo = (status & RETRO_VIDEO_VIEWS_STATUS_STEREO) != 0;

   frame_size(&fw, &fh);
   n = build_map(v, c, stereo);
   if (hw_kind >= HW_VULKAN)
   {
      if (!vk_ready)
      {
         video_cb(NULL, fw, fh, 0);
         return;
      }
   }
   else if (hw_kind != HW_OFF)
   {
      if (!gl_ready)
      {
         video_cb(NULL, fw, fh, 0);
         return;
      }
      p_glBindFramebuffer(GL_FRAMEBUFFER,
            (unsigned)hw_render.get_current_framebuffer());
      p_glEnable(GL_SCISSOR_TEST);
   }
   rect(0, 0, fw, fh, fw, fh, COL_BG);
   for (i = 0; i < n; i++)
   {
      /* GL clears through a scissor, so it keeps solid colours. */
      if (checker && (hw_kind == HW_OFF || hw_kind >= HW_VULKAN))
         fill_checker(v[i].x, v[i].y, v[i].width, v[i].height, fw);
      else
         rect(v[i].x, v[i].y, v[i].width, v[i].height, fw, fh, c[i]);
      rect(v[i].x, v[i].y, 8, 8, fw, fh, COL_WHITE);
   }

   if (map_kind != MAP_NONE)
   {
      views.views     = v;
      views.num_views = n;
      accepted        = environ_cb(RETRO_ENVIRONMENT_SET_VIDEO_VIEWS,
            &views) ? 1 : 0;
   }
   log_status(status, accepted, n);

   px      = input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X);
   py      = input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y);
   pressed = input_state_cb(0, RETRO_DEVICE_POINTER, 0,
         RETRO_DEVICE_ID_POINTER_PRESSED) ? 1 : 0;
   if (pressed)
   {
      unsigned cx = (unsigned)(((long)px + 0x7fff) * (long)(fw - 1) / 0xfffe);
      unsigned cy = (unsigned)(((long)py + 0x7fff) * (long)(fh - 1) / 0xfffe);
      if (cx >= 4 && cx + 5 <= fw && cy >= 4 && cy + 5 <= fh)
      {
         rect(cx - 4, cy, 9, 1, fw, fh, COL_WHITE);
         rect(cx, cy - 4, 1, 9, fw, fh, COL_WHITE);
      }
      if (px != last_px || py != last_py || pressed != last_pressed)
         log_cb(RETRO_LOG_INFO,
               "[video_views] pointer x=%d y=%d pressed=1 packed=(%u,%u)\n",
               px, py, cx, cy);
   }
   else if (last_pressed == 1)
      log_cb(RETRO_LOG_INFO, "[video_views] pointer pressed=0\n");
   last_px      = px;
   last_py      = py;
   last_pressed = pressed;

   log_pads();
   log_lightgun(fw, fh);
   update_rumble();

   if (hw_kind >= HW_VULKAN)
   {
      if (vk_send(fw, fh))
         video_cb(RETRO_HW_FRAME_BUFFER_VALID, fw, fh, 0);
      else
         video_cb(NULL, fw, fh, 0);
   }
   else if (hw_kind != HW_OFF)
   {
      p_glDisable(GL_SCISSOR_TEST);
      video_cb(RETRO_HW_FRAME_BUFFER_VALID, fw, fh, 0);
   }
   else
      video_cb(frame_buf, fw, fh, fw * sizeof(uint32_t));
}

size_t retro_serialize_size(void) { return 0; }
bool retro_serialize(void *data, size_t size) { (void)data; (void)size; return false; }
bool retro_unserialize(const void *data, size_t size) { (void)data; (void)size; return false; }
void retro_cheat_reset(void) { }
void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}

bool retro_load_game(const struct retro_game_info *game)
{
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
   (void)game;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
      return false;
   read_options();
   if (!environ_cb(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE, &rumble))
      memset(&rumble, 0, sizeof(rumble));
   if (hw_kind >= HW_VULKAN)
   {
      memset(&hw_render, 0, sizeof(hw_render));
      hw_render.context_type    = RETRO_HW_CONTEXT_VULKAN;
      hw_render.version_major   = VK_API_VERSION_1_1;
      hw_render.cache_context   = (hw_kind == HW_VULKAN_KEEP);
      hw_render.context_reset   = vk_context_reset;
      hw_render.context_destroy = vk_context_destroy;
      if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render))
         return false;
   }
   else if (hw_kind != HW_OFF)
   {
      memset(&hw_render, 0, sizeof(hw_render));
      hw_render.context_type       = RETRO_HW_CONTEXT_OPENGL_CORE;
      hw_render.version_major      = 3;
      hw_render.version_minor      = 2;
      hw_render.context_reset      = context_reset;
      hw_render.context_destroy    = context_destroy;
      hw_render.bottom_left_origin = (hw_kind == HW_GL);
      if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render))
         return false;
   }
   return true;
}

bool retro_load_game_special(unsigned type,
      const struct retro_game_info *info, size_t num)
{
   (void)type;
   (void)info;
   (void)num;
   return false;
}

void retro_unload_game(void) { }
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
void *retro_get_memory_data(unsigned id) { (void)id; return NULL; }
size_t retro_get_memory_size(unsigned id) { (void)id; return 0; }
