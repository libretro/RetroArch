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
 * option through a GL core context, with either origin, or a
 * compatibility one (gl_compat, which the gl driver takes), or a GLES 3
 * one (gles3, which the gl driver takes on Android), into Vulkan
 * images of its own handed over with set_image, or on Windows into
 * D3D11 textures of its own, one per sync index, handed over with
 * set_texture (libretro_d3d11.h version 2), or into D3D12 textures
 * handed over with set_texture_fenced (libretro_d3d12.h version 2). With
 * video_views_test_max it declares a far larger maximum than it draws.
 * e2e/run.py checks the colours on screen and the status in the log;
 * e2e/d3d.sh checks the d3d11 and d3d12 drivers' frames, under Wine
 * or on Windows.
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
 * a one-pixel black and white checkerboard instead of its colour, for
 * the headset's shrinking, and with noise a different colour in every
 * pixel, for e2e/run.py to check views arrive pixel for pixel. A pattern
 * is drawn in software or its own Vulkan images and, in the GL modes,
 * uploaded and blitted into the core's framebuffer. With
 * video_views_test_format rgb565 a software frame is sent in RGB565.
 *
 * With video_views_test_vr local or stage it sends its stereo map as
 * TRACKED while the frontend reports HMD, reads the pose calls first,
 * and logs what they answer with the frame's number. Each eye carries
 * that number's low sixteen bits along its bottom (white for a set bit,
 * low bit first), so what the headset shows can be matched with the
 * poses it was drawn from. nopair sends a TRACKED map without eyes,
 * which the frontend must refuse. poses reads the pose calls and sends
 * its map flat, as a core sampled through them alone. It logs HMD and
 * the recommended eye size whatever the mode. Once a file named vr_flat
 * is in its working directory it sends the map flat. With
 * video_views_test_vr_scene grid, software eyes draw one world-locked
 * floor grid, axes and a wall ahead from their own poses, instead of
 * a solid colour, for checking by eye that the world holds still. */
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <libretro.h>
#include <libretro_vulkan.h>
#ifdef _WIN32
#include <libretro_d3d11.h>
#include <libretro_d3d12.h>
#endif

#define MAX_W 800
#define MAX_H 480
/* The crop map's frame, past the declared maximum. Threaded video keeps
 * next_pow2(MAX_W)^2 pixels of it: 655 rows at this width. */
#define CROP_W 1600
#define CROP_H 960
/* Azahar's maximum, at 10x. */
#define LARGE_MAX_W 8000
#define LARGE_MAX_H 4800

/* Two eyes of the largest grid size. */
#define GRID_MAX 1024
#define GRID_MAX_PIXELS (2 * GRID_MAX * GRID_MAX)
#define FRAME_MAX_PIXELS ((GRID_MAX_PIXELS > CROP_W * CROP_H) \
      ? GRID_MAX_PIXELS : CROP_W * CROP_H)

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
   HW_VULKAN_KEEP,
   HW_D3D11,
   HW_D3D12
};

/* The Vulkan modes, by the order above. */
#define HW_IS_VULKAN(k) ((k) == HW_VULKAN || (k) == HW_VULKAN_KEEP)

enum pattern_kind
{
   PATTERN_SOLID = 0,
   PATTERN_CHECKER,
   PATTERN_NOISE
};

enum vr_kind
{
   VR_OFF = 0,
   VR_LOCAL,
   VR_STAGE,
   VR_NOPAIR,
   VR_POSES
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
#define GL_TEXTURE_2D       0x0DE1
#define GL_RGBA8            0x8058
#define GL_BGRA             0x80E1
#define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367
#define GL_NEAREST          0x2600
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_COLOR_ATTACHMENT0 0x8CE0
typedef void (APIENTRY *gl_bind_framebuffer_t)(unsigned, unsigned);
typedef void (APIENTRY *gl_enable_t)(unsigned);
typedef void (APIENTRY *gl_scissor_t)(int, int, int, int);
typedef void (APIENTRY *gl_clear_color_t)(float, float, float, float);
typedef void (APIENTRY *gl_clear_t)(unsigned);
typedef void (APIENTRY *gl_gen_t)(int, unsigned*);
typedef void (APIENTRY *gl_delete_t)(int, const unsigned*);
typedef void (APIENTRY *gl_bind_texture_t)(unsigned, unsigned);
typedef void (APIENTRY *gl_tex_image_2d_t)(unsigned, int, int, int, int,
      int, unsigned, unsigned, const void*);
typedef void (APIENTRY *gl_tex_sub_image_2d_t)(unsigned, int, int, int,
      int, int, unsigned, unsigned, const void*);
typedef void (APIENTRY *gl_framebuffer_texture_2d_t)(unsigned, unsigned,
      unsigned, unsigned, int);
typedef void (APIENTRY *gl_blit_framebuffer_t)(int, int, int, int,
      int, int, int, int, unsigned, unsigned);

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

static uint32_t frame_buf[FRAME_MAX_PIXELS];
static uint16_t frame_buf16[FRAME_MAX_PIXELS];
static enum map_kind map_kind = MAP_3DS;
static enum hw_kind hw_kind   = HW_OFF;
/* HW_GL through a compatibility context. */
static bool hw_gl_compat;
static bool hw_gles3;
static enum pattern_kind pattern = PATTERN_SOLID;
static bool large_max;
static bool rgb565;
static double core_fps = 60.0;
static unsigned frames_run;
static struct retro_hw_render_callback hw_render;
static gl_bind_framebuffer_t p_glBindFramebuffer;
static gl_enable_t           p_glEnable;
static gl_enable_t           p_glDisable;
static gl_scissor_t          p_glScissor;
static gl_clear_color_t      p_glClearColor;
static gl_clear_t            p_glClear;
static gl_gen_t              p_glGenTextures;
static gl_delete_t           p_glDeleteTextures;
static gl_bind_texture_t     p_glBindTexture;
static gl_tex_image_2d_t     p_glTexImage2D;
static gl_tex_sub_image_2d_t p_glTexSubImage2D;
static gl_gen_t              p_glGenFramebuffers;
static gl_delete_t           p_glDeleteFramebuffers;
static gl_framebuffer_texture_2d_t p_glFramebufferTexture2D;
static gl_blit_framebuffer_t p_glBlitFramebuffer;
static bool gl_ready;
/* A pattern's upload: a frame_buf-sized texture and its framebuffer. */
static unsigned gl_upload_tex;
static unsigned gl_upload_fbo;
static const struct retro_hw_render_interface_vulkan *vk;
static struct vk_funcs vkf;
static VkPhysicalDeviceMemoryProperties vk_memory;
static VkCommandPool vk_pool;
static struct vk_slot vk_slots[VK_SLOTS];
static bool vk_ready;
static int last_status        = -1;
static int last_accepted      = -1;
static int last_pressed[2]    = { -1, -1 };
static int16_t last_px[2], last_py[2];
static struct retro_rumble_interface rumble;
static int last_pad[2][7];      /* buttons, lx, ly, rx, ry, l2, r2 */
static bool pad_logged[2];
static int last_gun[4];         /* x, y, offscreen, trigger */
static bool gun_logged;
static unsigned last_rumble[2];
static enum vr_kind vr_kind = VR_OFF;
static int last_hmd         = -1;
static unsigned last_rec_w, last_rec_h;
static float last_eyes[14];
static bool eyes_logged;
static bool vr_flat;
static bool grid_scene;
static unsigned grid_cap = 512;
static unsigned eye_w, eye_h;
static unsigned geo_w, geo_h;
static struct retro_vr_frame_state vr_fs;
static bool vr_fs_ok;
static uint32_t last_head_flags = 0xffffffffu;

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

/* One of seven colours by frame position, so a view copied from the
 * wrong place shows. None is white, which the markers keep, and every
 * channel is 0 or 255, exact in RGB565. e2e/run.py repeats it. */
static uint32_t noise_colour(unsigned x, unsigned y)
{
   static const uint32_t cols[7] = { COL_BLACK, COL_RED, COL_GREEN,
      COL_BLUE, 0x00FFFF, 0xFF00FF, COL_YELLOW };
   uint32_t h = (uint32_t)x * 0x9E3779B1u + (uint32_t)y * 0x85EBCA77u;
   h ^= h >> 15;
   h *= 0x2C1B3C6Du;
   h ^= h >> 12;
   return cols[h % 7];
}

static void fill_noise(unsigned x, unsigned y, unsigned w, unsigned h,
      unsigned fw)
{
   unsigned i, j;
   for (j = y; j < y + h; j++)
      for (i = x; i < x + w; i++)
         frame_buf[j * fw + i] = noise_colour(i, j);
}

/* Whether the frame is drawn through GL clears rather than into
 * frame_buf: GL modes do so for solid colours only. */
static bool gl_clears(void)
{
   return (hw_kind == HW_GL || hw_kind == HW_GL_TOPLEFT)
      && pattern == PATTERN_SOLID;
}

/* frame_buf into the core's framebuffer, its top row last with a
 * bottom-left origin. Scissor must be off: it clips a blit. */
static void gl_upload(unsigned fw, unsigned fh)
{
   unsigned fbo = (unsigned)hw_render.get_current_framebuffer();
   if (!gl_upload_tex)
   {
      p_glGenTextures(1, &gl_upload_tex);
      p_glBindTexture(GL_TEXTURE_2D, gl_upload_tex);
      p_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, CROP_W, CROP_H, 0,
            GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
      p_glGenFramebuffers(1, &gl_upload_fbo);
      p_glBindFramebuffer(GL_FRAMEBUFFER, gl_upload_fbo);
      p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, gl_upload_tex, 0);
   }
   p_glBindTexture(GL_TEXTURE_2D, gl_upload_tex);
   p_glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (int)fw, (int)fh,
         GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, frame_buf);
   p_glBindTexture(GL_TEXTURE_2D, 0);
   p_glBindFramebuffer(GL_READ_FRAMEBUFFER, gl_upload_fbo);
   p_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
   if (hw_kind == HW_GL)
      p_glBlitFramebuffer(0, 0, (int)fw, (int)fh, 0, (int)fh, (int)fw, 0,
            GL_COLOR_BUFFER_BIT, GL_NEAREST);
   else
      p_glBlitFramebuffer(0, 0, (int)fw, (int)fh, 0, 0, (int)fw, (int)fh,
            GL_COLOR_BUFFER_BIT, GL_NEAREST);
   p_glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}

static void send_rgb565(unsigned fw, unsigned fh)
{
   unsigned i;
   for (i = 0; i < fw * fh; i++)
   {
      uint32_t c     = frame_buf[i];
      frame_buf16[i] = (uint16_t)(((c >> 8) & 0xF800)
            | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x001F));
   }
   video_cb(frame_buf16, fw, fh, fw * sizeof(uint16_t));
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
   if (gl_clears())
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
   p_glGenTextures     = (gl_gen_t)hw_render.get_proc_address("glGenTextures");
   p_glDeleteTextures  = (gl_delete_t)
      hw_render.get_proc_address("glDeleteTextures");
   p_glBindTexture     = (gl_bind_texture_t)
      hw_render.get_proc_address("glBindTexture");
   p_glTexImage2D      = (gl_tex_image_2d_t)
      hw_render.get_proc_address("glTexImage2D");
   p_glTexSubImage2D   = (gl_tex_sub_image_2d_t)
      hw_render.get_proc_address("glTexSubImage2D");
   p_glGenFramebuffers = (gl_gen_t)
      hw_render.get_proc_address("glGenFramebuffers");
   p_glDeleteFramebuffers = (gl_delete_t)
      hw_render.get_proc_address("glDeleteFramebuffers");
   p_glFramebufferTexture2D = (gl_framebuffer_texture_2d_t)
      hw_render.get_proc_address("glFramebufferTexture2D");
   p_glBlitFramebuffer = (gl_blit_framebuffer_t)
      hw_render.get_proc_address("glBlitFramebuffer");
   gl_upload_tex       = 0;
   gl_upload_fbo       = 0;
   gl_ready            = p_glBindFramebuffer && p_glEnable && p_glDisable
      && p_glScissor && p_glClearColor && p_glClear && p_glGenTextures
      && p_glDeleteTextures && p_glBindTexture && p_glTexImage2D
      && p_glTexSubImage2D && p_glGenFramebuffers && p_glDeleteFramebuffers
      && p_glFramebufferTexture2D && p_glBlitFramebuffer;
   if (!gl_ready)
      log_cb(RETRO_LOG_ERROR, "[video_views] GL entry points missing\n");
}

static long long now_us(void);

/* When, for a test that orders it against the headset's session. */
static void context_destroy(void)
{
   if (gl_ready && gl_upload_tex)
   {
      p_glDeleteFramebuffers(1, &gl_upload_fbo);
      p_glDeleteTextures(1, &gl_upload_tex);
   }
   gl_upload_tex = 0;
   gl_upload_fbo = 0;
   gl_ready      = false;
   log_cb(RETRO_LOG_INFO, "[video_views] context_destroy at %lld us\n",
         now_us());
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

/* Whether the grid draws eyes sized by the headset: software only,
 * where the frame is not tied to an image of the Vulkan modes. */
static bool grid_sized(void)
{
   return grid_scene && (vr_kind == VR_LOCAL || vr_kind == VR_STAGE)
      && hw_kind == HW_OFF
      && (map_kind == MAP_3DS || map_kind == MAP_3DS_FORCE);
}

static void frame_size(unsigned *fw, unsigned *fh)
{
   if (eye_w)
   {
      *fw = 2 * eye_w;
      *fh = eye_h;
      return;
   }
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
         if (eye_w)
         {
            v[0] = mkview(    0, 0, eye_w, eye_h, 0, RETRO_VIDEO_VIEW_EYE_LEFT);
            v[1] = mkview(eye_w, 0, eye_w, eye_h, 0, RETRO_VIDEO_VIEW_EYE_RIGHT);
            c[0] = COL_RED;
            c[1] = COL_BLUE;
            return 2;
         }
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
   hw_kind      = HW_OFF;
   hw_gl_compat = false;
   hw_gles3     = false;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "gl"))
         hw_kind = HW_GL;
      else if (!strcmp(var.value, "gl_compat"))
      {
         hw_kind      = HW_GL;
         hw_gl_compat = true;
      }
      else if (!strcmp(var.value, "gles3"))
      {
         hw_kind  = HW_GL;
         hw_gles3 = true;
      }
      else if (!strcmp(var.value, "gl_topleft"))
         hw_kind = HW_GL_TOPLEFT;
      else if (!strcmp(var.value, "vulkan"))
         hw_kind = HW_VULKAN;
      else if (!strcmp(var.value, "vulkan_keep"))
         hw_kind = HW_VULKAN_KEEP;
#ifdef _WIN32
      else if (!strcmp(var.value, "d3d11"))
         hw_kind = HW_D3D11;
      else if (!strcmp(var.value, "d3d12"))
         hw_kind = HW_D3D12;
#endif
   }

   var.key   = "video_views_test_max";
   var.value = NULL;
   large_max = environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var)
      && var.value && !strcmp(var.value, "large");

   var.key   = "video_views_test_pattern";
   var.value = NULL;
   pattern   = PATTERN_SOLID;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "checker"))
         pattern = PATTERN_CHECKER;
      else if (!strcmp(var.value, "noise"))
         pattern = PATTERN_NOISE;
   }
   if (hw_gles3 && pattern != PATTERN_SOLID)
      log_cb(RETRO_LOG_ERROR,
            "[video_views] gles3 can't upload a pattern: BGRA isn't GLES 3.0\n");

   var.key   = "video_views_test_format";
   var.value = NULL;
   rgb565    = environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var)
      && var.value && !strcmp(var.value, "rgb565");

   var.key   = "video_views_test_fps";
   var.value = NULL;
   core_fps  = 60.0;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      core_fps = atof(var.value);
   if (core_fps <= 0.0)
      core_fps = 60.0;

   var.key   = "video_views_test_vr";
   var.value = NULL;
   vr_kind   = VR_OFF;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "local"))
         vr_kind = VR_LOCAL;
      else if (!strcmp(var.value, "stage"))
         vr_kind = VR_STAGE;
      else if (!strcmp(var.value, "nopair"))
         vr_kind = VR_NOPAIR;
      else if (!strcmp(var.value, "poses"))
         vr_kind = VR_POSES;
   }

   var.key    = "video_views_test_vr_scene";
   var.value  = NULL;
   grid_scene = environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var)
      && var.value && !strcmp(var.value, "grid");

   var.key   = "video_views_test_vr_scene_size";
   var.value = NULL;
   grid_cap  = 512;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      unsigned n = (unsigned)atoi(var.value);
      if (n >= 384 && n <= GRID_MAX)
         grid_cap = n;
   }

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
        "Hardware rendering; off|gl|gl_topleft|gl_compat|vulkan|vulkan_keep|d3d11|d3d12|gles3" },
      { "video_views_test_max",
        "Declared maximum size; normal|large" },
      { "video_views_test_fps",
        "Frame rate; 60|10|16" },
      { "video_views_test_pattern",
        "View fill; solid|checker|noise" },
      { "video_views_test_format",
        "Software pixel format; xrgb8888|rgb565" },
      { "video_views_test_vr",
        "Head tracking; off|local|stage|nopair|poses" },
      { "video_views_test_vr_scene",
        "Head tracking scene; solid|grid" },
      { "video_views_test_vr_scene_size",
        "Grid eye size, at most; 512|384|768|1024" },
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
   if (grid_sized())
   {
      if (info->geometry.max_width < 2 * grid_cap)
         info->geometry.max_width = 2 * grid_cap;
      if (info->geometry.max_height < grid_cap)
         info->geometry.max_height = grid_cap;
   }
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

/* Pointer @idx logged when it changes; 1 is the second finger, for
 * touches on two displays at once. True while pressed, at @cx,@cy in
 * the frame. */
static bool log_pointer(unsigned idx, unsigned fw, unsigned fh,
      unsigned *cx, unsigned *cy)
{
   const char *name = idx ? "pointer1" : "pointer";
   int16_t x        = input_state_cb(0, RETRO_DEVICE_POINTER, idx,
         RETRO_DEVICE_ID_POINTER_X);
   int16_t y        = input_state_cb(0, RETRO_DEVICE_POINTER, idx,
         RETRO_DEVICE_ID_POINTER_Y);
   int pressed      = input_state_cb(0, RETRO_DEVICE_POINTER, idx,
         RETRO_DEVICE_ID_POINTER_PRESSED) ? 1 : 0;
   *cx = (unsigned)(((long)x + 0x7fff) * (long)(fw - 1) / 0xfffe);
   *cy = (unsigned)(((long)y + 0x7fff) * (long)(fh - 1) / 0xfffe);
   if (pressed)
   {
      if (     x != last_px[idx] || y != last_py[idx]
            || pressed != last_pressed[idx])
         log_cb(RETRO_LOG_INFO,
               "[video_views] %s x=%d y=%d pressed=1 packed=(%u,%u)\n",
               name, x, y, *cx, *cy);
   }
   else if (last_pressed[idx] == 1)
      log_cb(RETRO_LOG_INFO, "[video_views] %s pressed=0\n", name);
   last_px[idx]      = x;
   last_py[idx]      = y;
   last_pressed[idx] = pressed;
   return pressed != 0;
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

#ifdef _WIN32
/* The D3D11 mode: frame_buf uploaded into a texture per sync index,
 * exactly the frame's size, and handed over with set_texture. */
#define D3D_TEXTURES 8
static const struct retro_hw_render_interface_d3d11 *d3d;
static ID3D11Texture2D *d3d_tex[D3D_TEXTURES];
static unsigned d3d_dims[D3D_TEXTURES];
static struct retro_hw_render_context_negotiation_interface_d3d11 d3d_nego;

static void d3d_context_reset(void)
{
   d3d = NULL;
   if (     !environ_cb(RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE,
               (void*)&d3d)
         || !d3d
         || d3d->interface_type    != RETRO_HW_RENDER_INTERFACE_D3D11
         || d3d->interface_version <  RETRO_HW_RENDER_INTERFACE_D3D11_VERSION_2)
   {
      log_cb(RETRO_LOG_ERROR, "[video_views] no D3D11 interface version 2\n");
      d3d = NULL;
   }
}

static void d3d_context_destroy(void)
{
   unsigned i;
   for (i = 0; i < D3D_TEXTURES; i++)
   {
      if (d3d_tex[i])
         d3d_tex[i]->lpVtbl->Release(d3d_tex[i]);
      d3d_tex[i]  = NULL;
      d3d_dims[i] = 0;
   }
   d3d = NULL;
}

/* With the large maximum the frame is drawn into the corner of a
 * texture of the regular maximum's size, as a core rendering at a
 * varying resolution does. */
static bool d3d_send(unsigned fw, unsigned fh)
{
   unsigned i, k;
   D3D11_BOX box;
   ID3D11DeviceContext *ctx;
   unsigned tw = (large_max && fw <= MAX_W && fh <= MAX_H) ? MAX_W : fw;
   unsigned th = (large_max && fw <= MAX_W && fh <= MAX_H) ? MAX_H : fh;
   if (!d3d)
      return false;
   i = d3d->get_sync_index(d3d->handle);
   if (i >= D3D_TEXTURES)
      return false;
   d3d->wait_sync_index(d3d->handle);
   /* Always taken; true only says the frontend has used the context,
    * and an upload binds nothing. */
   d3d->lock_context(d3d->handle);
   if (!d3d_tex[i] || d3d_dims[i] != (tw << 16 | th))
   {
      D3D11_TEXTURE2D_DESC desc;
      if (d3d_tex[i])
         d3d_tex[i]->lpVtbl->Release(d3d_tex[i]);
      d3d_tex[i]              = NULL;
      memset(&desc, 0, sizeof(desc));
      desc.Width              = tw;
      desc.Height             = th;
      desc.MipLevels          = 1;
      desc.ArraySize          = 1;
      desc.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
      desc.SampleDesc.Count   = 1;
      desc.Usage              = D3D11_USAGE_DEFAULT;
      desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
      if (FAILED(d3d->device->lpVtbl->CreateTexture2D(d3d->device, &desc,
                  NULL, &d3d_tex[i])))
      {
         d3d_tex[i] = NULL;
         d3d->unlock_context(d3d->handle);
         return false;
      }
      d3d_dims[i] = tw << 16 | th;
   }
   /* XRGB: the X byte is the texture's alpha. */
   for (k = 0; k < fw * fh; k++)
      frame_buf[k] |= 0xFF000000u;
   ctx        = d3d->context;
   box.left   = 0;
   box.top    = 0;
   box.front  = 0;
   box.right  = fw;
   box.bottom = fh;
   box.back   = 1;
   ctx->lpVtbl->UpdateSubresource(ctx, (ID3D11Resource*)d3d_tex[i], 0,
         &box, frame_buf, fw * sizeof(uint32_t), 0);
   d3d->set_texture(d3d->handle, d3d_tex[i]);
   d3d->unlock_context(d3d->handle);
   return true;
}

/* The D3D12 mode: frame_buf copied through an upload buffer into a
 * texture per sync index, on the frontend's queue, and handed over with
 * set_texture_fenced and the fence the copy signals. With the large
 * maximum the textures are the regular maximum's size, as in the D3D11
 * mode. */
static const struct retro_hw_render_interface_d3d12 *d12;
static ID3D12Resource            *d12_tex[D3D_TEXTURES];
static unsigned                   d12_dims[D3D_TEXTURES];
static ID3D12Resource            *d12_upload;
static size_t                     d12_upload_size;
static ID3D12CommandAllocator    *d12_alloc;
static ID3D12GraphicsCommandList *d12_list;
static ID3D12Fence               *d12_fence;
static HANDLE                     d12_event;
static UINT64                     d12_value;
static struct retro_hw_render_context_negotiation_interface_d3d12 d12_nego;

/* The last copy has executed: its upload buffer and list are free. */
static void d12_wait(void)
{
   if (!d12_fence || d12_fence->lpVtbl->GetCompletedValue(d12_fence) >= d12_value)
      return;
   d12_fence->lpVtbl->SetEventOnCompletion(d12_fence, d12_value, d12_event);
   WaitForSingleObject(d12_event, INFINITE);
}

static void d12_context_destroy(void)
{
   unsigned i;
   d12_wait();
   for (i = 0; i < D3D_TEXTURES; i++)
   {
      if (d12_tex[i])
         d12_tex[i]->lpVtbl->Release(d12_tex[i]);
      d12_tex[i]  = NULL;
      d12_dims[i] = 0;
   }
   if (d12_upload)
      d12_upload->lpVtbl->Release(d12_upload);
   if (d12_list)
      d12_list->lpVtbl->Release(d12_list);
   if (d12_alloc)
      d12_alloc->lpVtbl->Release(d12_alloc);
   if (d12_fence)
      d12_fence->lpVtbl->Release(d12_fence);
   if (d12_event)
      CloseHandle(d12_event);
   d12_upload      = NULL;
   d12_upload_size = 0;
   d12_list        = NULL;
   d12_alloc       = NULL;
   d12_fence       = NULL;
   d12_event       = NULL;
   d12_value       = 0;
   d12             = NULL;
}

static void d12_context_reset(void)
{
   ID3D12Device *dev;
   d12 = NULL;
   if (     !environ_cb(RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE,
               (void*)&d12)
         || !d12
         || d12->interface_type    != RETRO_HW_RENDER_INTERFACE_D3D12
         || d12->interface_version <  RETRO_HW_RENDER_INTERFACE_D3D12_VERSION_2)
   {
      log_cb(RETRO_LOG_ERROR, "[video_views] no D3D12 interface version 2\n");
      d12 = NULL;
      return;
   }
   dev       = d12->device;
   d12_event = CreateEvent(NULL, FALSE, FALSE, NULL);
   if (     !d12_event
         || FAILED(dev->lpVtbl->CreateCommandAllocator(dev,
               D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator,
               (void**)&d12_alloc))
         || FAILED(dev->lpVtbl->CreateCommandList(dev, 0,
               D3D12_COMMAND_LIST_TYPE_DIRECT, d12_alloc, NULL,
               &IID_ID3D12GraphicsCommandList, (void**)&d12_list))
         || FAILED(dev->lpVtbl->CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE,
               &IID_ID3D12Fence, (void**)&d12_fence)))
   {
      log_cb(RETRO_LOG_ERROR, "[video_views] D3D12 setup failed\n");
      d12_context_destroy();
      return;
   }
   d12_list->lpVtbl->Close(d12_list);
}

static void d12_barrier(ID3D12Resource *res, D3D12_RESOURCE_STATES before,
      D3D12_RESOURCE_STATES after)
{
   D3D12_RESOURCE_BARRIER b;
   memset(&b, 0, sizeof(b));
   b.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
   b.Transition.pResource   = res;
   b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
   b.Transition.StateBefore = before;
   b.Transition.StateAfter  = after;
   d12_list->lpVtbl->ResourceBarrier(d12_list, 1, &b);
}

static bool d12_send(unsigned fw, unsigned fh)
{
   unsigned i, j;
   uint8_t *dst;
   ID3D12CommandList *lists[1];
   D3D12_HEAP_PROPERTIES heap;
   D3D12_RESOURCE_DESC desc;
   D3D12_TEXTURE_COPY_LOCATION from, to;
   D3D12_RANGE none;
   ID3D12Device *dev;
   unsigned tw    = (large_max && fw <= MAX_W && fh <= MAX_H) ? MAX_W : fw;
   unsigned th    = (large_max && fw <= MAX_W && fh <= MAX_H) ? MAX_H : fh;
   unsigned pitch = (fw * 4 + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1)
      & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
   size_t   size  = (size_t)pitch * fh;
   if (!d12)
      return false;
   dev = d12->device;
   i   = d12->get_sync_index(d12->handle);
   if (i >= D3D_TEXTURES)
      return false;
   d12->wait_sync_index(d12->handle);
   d12_wait();

   memset(&heap, 0, sizeof(heap));
   memset(&desc, 0, sizeof(desc));
   desc.DepthOrArraySize = 1;
   desc.MipLevels        = 1;
   desc.SampleDesc.Count = 1;
   if (!d12_tex[i] || d12_dims[i] != (tw << 16 | th))
   {
      if (d12_tex[i])
         d12_tex[i]->lpVtbl->Release(d12_tex[i]);
      d12_tex[i]       = NULL;
      heap.Type        = D3D12_HEAP_TYPE_DEFAULT;
      desc.Dimension   = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
      desc.Width       = tw;
      desc.Height      = th;
      desc.Format      = DXGI_FORMAT_B8G8R8A8_UNORM;
      if (FAILED(dev->lpVtbl->CreateCommittedResource(dev, &heap,
                  D3D12_HEAP_FLAG_NONE, &desc, d12->required_state, NULL,
                  &IID_ID3D12Resource, (void**)&d12_tex[i])))
      {
         d12_tex[i] = NULL;
         return false;
      }
      d12_dims[i] = tw << 16 | th;
   }
   if (d12_upload_size < size)
   {
      if (d12_upload)
         d12_upload->lpVtbl->Release(d12_upload);
      d12_upload       = NULL;
      d12_upload_size  = 0;
      heap.Type        = D3D12_HEAP_TYPE_UPLOAD;
      desc.Dimension   = D3D12_RESOURCE_DIMENSION_BUFFER;
      desc.Width       = size;
      desc.Height      = 1;
      desc.Format      = DXGI_FORMAT_UNKNOWN;
      desc.Layout      = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
      if (FAILED(dev->lpVtbl->CreateCommittedResource(dev, &heap,
                  D3D12_HEAP_FLAG_NONE, &desc,
                  D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                  &IID_ID3D12Resource, (void**)&d12_upload)))
      {
         d12_upload = NULL;
         return false;
      }
      d12_upload_size = size;
   }

   /* XRGB: the X byte is the texture's alpha. */
   none.Begin = 0;
   none.End   = 0;
   if (FAILED(d12_upload->lpVtbl->Map(d12_upload, 0, &none, (void**)&dst)))
      return false;
   for (j = 0; j < fh; j++)
   {
      unsigned k;
      uint32_t *row = (uint32_t*)(dst + (size_t)j * pitch);
      for (k = 0; k < fw; k++)
         row[k] = frame_buf[j * fw + k] | 0xFF000000u;
   }
   d12_upload->lpVtbl->Unmap(d12_upload, 0, NULL);

   d12_alloc->lpVtbl->Reset(d12_alloc);
   d12_list->lpVtbl->Reset(d12_list, d12_alloc, NULL);
   d12_barrier(d12_tex[i], d12->required_state,
         D3D12_RESOURCE_STATE_COPY_DEST);
   memset(&from, 0, sizeof(from));
   memset(&to, 0, sizeof(to));
   from.pResource                          = d12_upload;
   from.Type                               = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
   from.PlacedFootprint.Footprint.Format   = DXGI_FORMAT_B8G8R8A8_UNORM;
   from.PlacedFootprint.Footprint.Width    = fw;
   from.PlacedFootprint.Footprint.Height   = fh;
   from.PlacedFootprint.Footprint.Depth    = 1;
   from.PlacedFootprint.Footprint.RowPitch = pitch;
   to.pResource                            = d12_tex[i];
   to.Type                                 = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
   d12_list->lpVtbl->CopyTextureRegion(d12_list, &to, 0, 0, 0, &from, NULL);
   d12_barrier(d12_tex[i], D3D12_RESOURCE_STATE_COPY_DEST,
         d12->required_state);
   d12_list->lpVtbl->Close(d12_list);
   lists[0] = (ID3D12CommandList*)d12_list;
   d12->queue->lpVtbl->ExecuteCommandLists(d12->queue, 1, lists);
   d12->queue->lpVtbl->Signal(d12->queue, d12_fence, ++d12_value);
   d12->set_texture_fenced(d12->handle, d12_tex[i],
         DXGI_FORMAT_B8G8R8A8_UNORM, d12_fence, d12_value);
   return true;
}
#endif

/* The pose calls, as a VR core makes them before drawing; what changed
 * is logged, rounded so a still headset logs once. */
static void read_vr(void)
{
   unsigned i;
   float e[14];
   struct retro_vr_frame_state fs;
   struct retro_vr_head_pose head;

   memset(&fs, 0, sizeof(fs));
   vr_fs_ok = environ_cb(RETRO_ENVIRONMENT_GET_VR_FRAME_STATE, &fs);
   if (vr_fs_ok)
   {
      vr_fs = fs;
      if (fs.flags)
         log_cb(RETRO_LOG_INFO, "[video_views] vr frame flags=%u\n",
               (unsigned)fs.flags);
      for (i = 0; i < 3; i++)
      {
         e[i]     = fs.eyes[0].position[i];
         e[i + 3] = fs.eyes[1].position[i];
      }
      for (i = 0; i < 4; i++)
      {
         e[i + 6]  = fs.eyes[0].fov_tan[i];
         e[i + 10] = fs.eyes[1].fov_tan[i];
      }
      for (i = 0; i < 14; i++)
         e[i] = (float)((long)(e[i] * 100000.0f + (e[i] < 0 ? -0.5f : 0.5f)))
            / 100000.0f;
      if (!eyes_logged || memcmp(e, last_eyes, sizeof(e)))
      {
         log_cb(RETRO_LOG_INFO,
               "[video_views] vr eyes frame=%u left=%.5f,%.5f,%.5f "
               "right=%.5f,%.5f,%.5f fov=%.5f,%.5f,%.5f,%.5f "
               "rfov=%.5f,%.5f,%.5f,%.5f\n",
               frames_run, e[0], e[1], e[2], e[3], e[4], e[5], e[6], e[7],
               e[8], e[9], e[10], e[11], e[12], e[13]);
         memcpy(last_eyes, e, sizeof(e));
         eyes_logged = true;
      }
   }
   memset(&head, 0, sizeof(head));
   if (     environ_cb(RETRO_ENVIRONMENT_GET_VR_HEAD_POSE, &head)
         && head.flags != last_head_flags)
   {
      log_cb(RETRO_LOG_INFO, "[video_views] vr head flags=%u\n",
            (unsigned)head.flags);
      last_head_flags = head.flags;
   }
}

#define SCENE_EYE_HEIGHT 1.6f
#define SCENE_WALL_Z    -2.0f

/* Every mix is a convex blend, so the channels stay within 0..1. */
static uint32_t scene_rgb(float r, float g, float b)
{
   return ((uint32_t)(r * 255.0f) << 16) | ((uint32_t)(g * 255.0f) << 8)
      | (uint32_t)(b * 255.0f);
}

static float scene_abs(float x)
{
   return x < 0.0f ? -x : x;
}

static float scene_clamp01(float x)
{
   return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
}

/* Coverage of a pixel fp wide by a line of half-width w, dist from its
 * centre; past a spacing of step it settles at the line's density. */
static float scene_line(float dist, float w, float fp, float step)
{
   float cov, m;
   if (fp < step * 0.5f && dist >= w + fp * 0.5f)
      return 0.0f;
   cov = scene_clamp01((w + fp * 0.5f - dist) / fp);
   if (cov > 2.0f * w / fp)
      cov = 2.0f * w / fp;
   m   = scene_clamp01((fp - step * 0.5f) / (step * 0.5f));
   return cov + (scene_clamp01(2.0f * w / step) - cov) * m;
}

static void scene_mix(float *c, float r, float g, float b, float a)
{
   if (a <= 0.0f)
      return;
   c[0] += (r - c[0]) * a;
   c[1] += (g - c[1]) * a;
   c[2] += (b - c[2]) * a;
}

static void scene_sky(float *c, float elev)
{
   float t = elev < 0.0f ? 0.0f : (elev > 1.0f ? 1.0f : elev);
   c[0] = 0.62f - 0.40f * t;
   c[1] = 0.70f - 0.32f * t;
   c[2] = 0.78f - 0.14f * t;
}

/* Colour of the world ray o + t * d; the floor is the plane y = fy.
 * ps is the pixel's size on the eye's z = -1 plane, for edge coverage. */
static uint32_t scene_ray(const float *o, const float *d, float fy,
      float ps)
{
   float c[3], hx, hz, w, fp, a, tw, tf, qx, qz, fx, fz;
   long ix, iz;
   float ad1 = scene_abs(d[1]);

   tf = (d[1] < 0.0f && o[1] > fy) ? (fy - o[1]) / d[1] : -1.0f;
   tw = (d[2] < 0.0f && o[2] > SCENE_WALL_Z)
      ? (SCENE_WALL_Z - o[2]) / d[2] : -1.0f;

   /* Past the fog only the horizon shows, and the casts below stay
    * in range. */
   if (tf >= 40.0f)
      scene_sky(c, 0.0f);
   else if (tf <= 0.0f)
      scene_sky(c, d[1] / (scene_abs(d[0]) + ad1 + scene_abs(d[2])) * 2.0f);
   else
   {
      hx = o[0] + tf * d[0];
      hz = o[2] + tf * d[2];
      /* The footprint along the floor, which the grazing view stretches. */
      fp = tf * ps / ad1;
      w  = 0.008f;
      /* Faint checker under the grid lines, flat where it is too fine.
       * The half-metre index gives the checker cell and which lines are
       * the thick metre ones. */
      qx = (hx + 1024.0f) * 2.0f;
      qz = (hz + 1024.0f) * 2.0f;
      ix = (long)qx;
      iz = (long)qz;
      fx = qx - (float)ix;
      fz = qz - (float)iz;
      if (((ix >> 1) + (iz >> 1)) & 1)
      {
         c[0] = 0.26f;
         c[1] = 0.30f;
         c[2] = 0.28f;
      }
      else
      {
         c[0] = 0.32f;
         c[1] = 0.36f;
         c[2] = 0.34f;
      }
      scene_mix(c, 0.29f, 0.33f, 0.31f, scene_clamp01(fp));
      if (fx > 0.5f)
      {
         fx = 1.0f - fx;
         ix++;
      }
      if (fz > 0.5f)
      {
         fz = 1.0f - fz;
         iz++;
      }
      if (ix & 1)
         a = scene_line(fx * 0.5f, w, fp, 0.5f);
      else
         a = scene_line(fx * 0.5f, w * 2.5f, fp, 1.0f);
      scene_mix(c, ix & 1 ? 0.55f : 0.80f, ix & 1 ? 0.58f : 0.82f,
            ix & 1 ? 0.58f : 0.80f, a);
      if (iz & 1)
         a = scene_line(fz * 0.5f, w, fp, 0.5f);
      else
         a = scene_line(fz * 0.5f, w * 2.5f, fp, 1.0f);
      scene_mix(c, iz & 1 ? 0.55f : 0.80f, iz & 1 ? 0.58f : 0.82f,
            iz & 1 ? 0.58f : 0.80f, a);
      /* Axes: X red, Z blue, brighter toward -Z, the way ahead. */
      hx = scene_abs(hx);
      hz = scene_abs(hz);
      if (hz < 0.25f + fp)
      {
         a = scene_line(hz, w * 3.0f, fp, 1000.0f);
         scene_mix(c, 0.85f, 0.25f, 0.25f, a);
      }
      if (hx < 0.25f + fp)
      {
         a = scene_line(hx, w * 3.0f, fp, 1000.0f);
         if (o[2] + tf * d[2] < 0.0f)
            scene_mix(c, 0.25f, 0.45f, 1.0f, a);
         else
            scene_mix(c, 0.20f, 0.28f, 0.50f, a);
         if (hz < 0.25f + fp)
         {
            a = scene_clamp01((0.25f - hx) / fp + 0.5f)
               * scene_clamp01((0.25f - hz) / fp + 0.5f);
            scene_mix(c, 0.95f, 0.95f, 0.85f, a);
         }
      }
      a = tf / 40.0f;
      scene_mix(c, 0.62f, 0.70f, 0.78f, a);
   }

   if (tw > 0.0f && (tf < 0.0f || tw < tf))
   {
      hx = o[0] + tw * d[0];
      hz = o[1] + tw * d[1] - (fy + SCENE_EYE_HEIGHT);
      fp = tw * ps / scene_abs(d[2]);
      /* The patch hangs 2 m ahead, at eye height over the floor. */
      a = scene_clamp01((0.5f - scene_abs(hx)) / fp + 0.5f)
         * scene_clamp01((0.5f - scene_abs(hz)) / fp + 0.5f);
      if (a > 0.0f)
      {
         float in = scene_clamp01((0.1f - scene_abs(hx)) / fp + 0.5f)
            * scene_clamp01((0.1f - scene_abs(hz)) / fp + 0.5f);
         scene_mix(c, 0.90f + 0.05f * in, 0.55f + 0.40f * in,
               0.20f + 0.65f * in, a);
      }
   }
   return scene_rgb(c[0], c[1], c[2]);
}

/* One eye of the shared world, from the eye's own pose and tangents. */
static void draw_scene(const struct retro_video_view *v, unsigned fw,
      const struct retro_vr_eye_state *e, bool stage)
{
   unsigned i, j;
   float fy  = stage ? 0.0f : -SCENE_EYE_HEIGHT;
   float qx  = e->orientation[0];
   float qy  = e->orientation[1];
   float qz  = e->orientation[2];
   float qw  = e->orientation[3];
   float l   = e->fov_tan[0];
   float r   = e->fov_tan[1];
   float u   = e->fov_tan[2];
   float dn  = e->fov_tan[3];
   float sx  = (l + r) / (float)v->width;
   float sy  = (u + dn) / (float)v->height;
   float ps  = sx > sy ? sx : sy;
   float rx[3], ry[3], rz[3];
   /* The eye's axes in world space: the rotated unit vectors. */
   rx[0] = 1.0f - 2.0f * (qy * qy + qz * qz);
   rx[1] = 2.0f * (qx * qy + qw * qz);
   rx[2] = 2.0f * (qx * qz - qw * qy);
   ry[0] = 2.0f * (qx * qy - qw * qz);
   ry[1] = 1.0f - 2.0f * (qx * qx + qz * qz);
   ry[2] = 2.0f * (qy * qz + qw * qx);
   rz[0] = 2.0f * (qx * qz + qw * qy);
   rz[1] = 2.0f * (qy * qz - qw * qx);
   rz[2] = 1.0f - 2.0f * (qx * qx + qy * qy);

   for (j = 0; j < v->height; j++)
   {
      uint32_t *row = &frame_buf[(v->y + j) * fw + v->x];
      float py = u - sy * ((float)j + 0.5f);
      float px = -l + sx * 0.5f;
      float d[3], step[3];
      /* d = px * right + py * up - forward, advanced along the row. */
      for (i = 0; i < 3; i++)
      {
         d[i]    = px * rx[i] + py * ry[i] - rz[i];
         step[i] = sx * rx[i];
      }
      for (i = 0; i < v->width; i++)
      {
         row[i] = scene_ray(e->position, d, fy, ps);
         d[0] += step[0];
         d[1] += step[1];
         d[2] += step[2];
      }
   }
}

static void frame_tag(const struct retro_video_view *v, unsigned fw,
      unsigned fh)
{
   unsigned b;
   for (b = 0; b < 16; b++)
      rect(v->x + 16 + 20 * b, v->y + v->height - 24, 16, 16, fw, fh,
            ((frames_run >> b) & 1) ? COL_WHITE : COL_BLACK);
}

/* Whether this frame's map is TRACKED: with HMD, until vr_flat exists. */
static bool vr_tracking(bool hmd)
{
   FILE *flat;
   if (vr_kind == VR_OFF || vr_kind == VR_POSES || !hmd || vr_flat)
      return false;
   if (!(flat = fopen("vr_flat", "rb")))
      return true;
   fclose(flat);
   vr_flat = true;
   log_cb(RETRO_LOG_INFO, "[video_views] vr flat\n");
   return false;
}

void retro_run(void)
{
   struct retro_video_view v[RETRO_VIDEO_VIEWS_MAX];
   uint32_t c[RETRO_VIDEO_VIEWS_MAX];
   struct retro_video_views views;
   unsigned i, n, fw, fh, cx, cy;
   int accepted    = 0;
   unsigned status = 0;
   bool stereo;
   bool hmd;
   bool vr;

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
   hmd    = (status & RETRO_VIDEO_VIEWS_STATUS_HMD) != 0;
   vr     = vr_tracking(hmd);
   if (vr)
   {
      read_vr();
      /* Tracked eyes, or for nopair two screens without any. */
      stereo = (vr_kind != VR_NOPAIR);
   }
   else if (vr_kind == VR_POSES && hmd)
      read_vr();

   eye_w = eye_h = 0;
   if (vr && grid_sized())
   {
      /* No narrower than the frame tag, which would spill over. */
      eye_w = (last_rec_w >= 332 && last_rec_w < grid_cap) ? last_rec_w
         : grid_cap;
      eye_h = (last_rec_h >= 64 && last_rec_h < grid_cap) ? last_rec_h
         : grid_cap;
   }
   frame_size(&fw, &fh);
   if (fw != geo_w || fh != geo_h)
   {
      /* A grid frame is not the size declared at load. */
      if (geo_w || grid_sized())
      {
         struct retro_system_av_info av;
         retro_get_system_av_info(&av);
         environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &av.geometry);
      }
      geo_w = fw;
      geo_h = fh;
   }
   n = build_map(v, c, stereo);
   if (HW_IS_VULKAN(hw_kind))
   {
      if (!vk_ready)
      {
         video_cb(NULL, fw, fh, 0);
         return;
      }
   }
#ifdef _WIN32
   else if (hw_kind == HW_D3D11)
   {
      if (!d3d)
      {
         video_cb(NULL, fw, fh, 0);
         return;
      }
   }
   else if (hw_kind == HW_D3D12)
   {
      if (!d12)
      {
         video_cb(NULL, fw, fh, 0);
         return;
      }
   }
#endif
   else if (hw_kind != HW_OFF)
   {
      if (!gl_ready)
      {
         video_cb(NULL, fw, fh, 0);
         return;
      }
      p_glBindFramebuffer(GL_FRAMEBUFFER,
            (unsigned)hw_render.get_current_framebuffer());
      if (gl_clears())
         p_glEnable(GL_SCISSOR_TEST);
   }
   rect(0, 0, fw, fh, fw, fh, COL_BG);
   for (i = 0; i < n; i++)
   {
      if (     grid_scene && vr && vr_fs_ok
            && (v[i].eye == RETRO_VIDEO_VIEW_EYE_LEFT
               || v[i].eye == RETRO_VIDEO_VIEW_EYE_RIGHT)
            && (hw_kind == HW_OFF || hw_kind >= HW_VULKAN))
         draw_scene(&v[i], fw,
               &vr_fs.eyes[v[i].eye == RETRO_VIDEO_VIEW_EYE_RIGHT],
               vr_kind == VR_STAGE);
      else if (pattern == PATTERN_CHECKER)
         fill_checker(v[i].x, v[i].y, v[i].width, v[i].height, fw);
      else if (pattern == PATTERN_NOISE)
         fill_noise(v[i].x, v[i].y, v[i].width, v[i].height, fw);
      else
         rect(v[i].x, v[i].y, v[i].width, v[i].height, fw, fh, c[i]);
      rect(v[i].x, v[i].y, 8, 8, fw, fh, COL_WHITE);
      if (vr && v[i].eye != RETRO_VIDEO_VIEW_EYE_NONE)
         frame_tag(&v[i], fw, fh);
   }

   if (map_kind != MAP_NONE)
   {
      memset(&views, 0, sizeof(views));
      views.views     = v;
      views.num_views = n;
      if (vr)
      {
         views.flags           = RETRO_VIDEO_VIEWS_FLAG_TRACKED;
         views.reference_space = (vr_kind == VR_STAGE)
            ? RETRO_VR_REFERENCE_SPACE_STAGE
            : RETRO_VR_REFERENCE_SPACE_LOCAL;
      }
      accepted        = environ_cb(RETRO_ENVIRONMENT_SET_VIDEO_VIEWS,
            &views) ? 1 : 0;
      if (     (int)hmd != last_hmd
            || views.recommended_view_width != last_rec_w
            || views.recommended_view_height != last_rec_h)
      {
         log_cb(RETRO_LOG_INFO, "[video_views] hmd=%d recommended=%ux%u\n",
               hmd ? 1 : 0, views.recommended_view_width,
               views.recommended_view_height);
         last_hmd   = hmd ? 1 : 0;
         last_rec_w = views.recommended_view_width;
         last_rec_h = views.recommended_view_height;
      }
   }
   log_status(status, accepted, n);

   if (     log_pointer(0, fw, fh, &cx, &cy)
         && cx >= 4 && cx + 5 <= fw && cy >= 4 && cy + 5 <= fh)
   {
      rect(cx - 4, cy, 9, 1, fw, fh, COL_WHITE);
      rect(cx, cy - 4, 1, 9, fw, fh, COL_WHITE);
   }
   log_pointer(1, fw, fh, &cx, &cy);
   log_pads();
   log_lightgun(fw, fh);
   update_rumble();

   if (HW_IS_VULKAN(hw_kind))
   {
      if (vk_send(fw, fh))
         video_cb(RETRO_HW_FRAME_BUFFER_VALID, fw, fh, 0);
      else
         video_cb(NULL, fw, fh, 0);
   }
#ifdef _WIN32
   else if (hw_kind == HW_D3D11)
      video_cb(d3d_send(fw, fh) ? RETRO_HW_FRAME_BUFFER_VALID : NULL,
            fw, fh, 0);
   else if (hw_kind == HW_D3D12)
      video_cb(d12_send(fw, fh) ? RETRO_HW_FRAME_BUFFER_VALID : NULL,
            fw, fh, 0);
#endif
   else if (hw_kind != HW_OFF)
   {
      p_glDisable(GL_SCISSOR_TEST);
      if (!gl_clears())
         gl_upload(fw, fh);
      video_cb(RETRO_HW_FRAME_BUFFER_VALID, fw, fh, 0);
   }
   else if (rgb565)
      send_rgb565(fw, fh);
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
   read_options();
   if (rgb565 && hw_kind == HW_OFF)
      fmt = RETRO_PIXEL_FORMAT_RGB565;
   else
      rgb565 = false;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
      return false;
   if (!environ_cb(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE, &rumble))
      memset(&rumble, 0, sizeof(rumble));
   if (HW_IS_VULKAN(hw_kind))
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
#ifdef _WIN32
   else if (hw_kind == HW_D3D11)
   {
      struct retro_hw_render_context_negotiation_interface probe;
      memset(&hw_render, 0, sizeof(hw_render));
      hw_render.context_type    = RETRO_HW_CONTEXT_D3D11;
      hw_render.version_major   = 11;
      hw_render.context_reset   = d3d_context_reset;
      hw_render.context_destroy = d3d_context_destroy;
      if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render))
         return false;
      probe.interface_type    =
         RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_D3D11;
      probe.interface_version = 0;
      d3d_nego.interface_type =
         RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_D3D11;
      d3d_nego.interface_version =
         RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_D3D11_VERSION;
      d3d_nego.max_render_interface_version =
         RETRO_HW_RENDER_INTERFACE_D3D11_VERSION_2;
      if (     environ_cb(
                  RETRO_ENVIRONMENT_GET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_SUPPORT,
                  &probe)
            && probe.interface_version >= 1)
         environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE,
               &d3d_nego);
   }
   else if (hw_kind == HW_D3D12)
   {
      struct retro_hw_render_context_negotiation_interface probe;
      memset(&hw_render, 0, sizeof(hw_render));
      hw_render.context_type    = RETRO_HW_CONTEXT_D3D12;
      hw_render.version_major   = 12;
      hw_render.context_reset   = d12_context_reset;
      hw_render.context_destroy = d12_context_destroy;
      if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render))
         return false;
      probe.interface_type    =
         RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_D3D12;
      probe.interface_version = 0;
      d12_nego.interface_type =
         RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_D3D12;
      d12_nego.interface_version =
         RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_D3D12_VERSION;
      d12_nego.max_render_interface_version =
         RETRO_HW_RENDER_INTERFACE_D3D12_VERSION_2;
      if (     environ_cb(
                  RETRO_ENVIRONMENT_GET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_SUPPORT,
                  &probe)
            && probe.interface_version >= 1)
         environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE,
               &d12_nego);
   }
#endif
   else if (hw_kind != HW_OFF)
   {
      memset(&hw_render, 0, sizeof(hw_render));
      hw_render.context_type       = RETRO_HW_CONTEXT_OPENGL_CORE;
      hw_render.version_major      = 3;
      hw_render.version_minor      = 2;
      if (hw_gl_compat)
      {
         hw_render.context_type    = RETRO_HW_CONTEXT_OPENGL;
         hw_render.version_major   = 0;
         hw_render.version_minor   = 0;
      }
      else if (hw_gles3)
      {
         hw_render.context_type    = RETRO_HW_CONTEXT_OPENGLES3;
         hw_render.version_major   = 3;
         hw_render.version_minor   = 0;
      }
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
