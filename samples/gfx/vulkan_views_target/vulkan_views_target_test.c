/* Vulkan retained-target failures use the real allocator, retirement and
 * shared ensure policy. Only device allocation and logging are replaced.
 * The XR draw and its slots run with the runtime's swapchains and the
 * pass steps stubbed: what reaches the runtime's images, when a chain
 * ends a frame, and when a repeat replays the last frame's final pass. */
#include <stdio.h>
#include <string.h>
#include <vulkan/vulkan.h>
#include "gfx/video_views.h"
#include "gfx/video_defines.h"
#include "gfx/video_xr.h"

#define VULKAN_OPENXR_MAX_IMAGES 4
#define VULKAN_DIRTY_DYNAMIC_BIT 0x0001
#define VULKAN_IMAGE_LAYOUT_TRANSITION(cmd, img, a, b, c, d, e, f) \
   do { (void)(cmd); (void)(img); } while (0)

struct vk_image
{
   VkImage image;
   VkDeviceMemory memory;
   VkFramebuffer framebuffer;
};

typedef struct
{
   struct { VkPhysicalDeviceLimits limits; } gpu_properties;
   VkDevice device;
   VkFormat swapchain_format;
   struct vulkan_openxr *xr;
} test_context_t;

typedef struct
{
   struct vk_image view_retired[2];
} test_chain_t;

typedef struct
{
   test_context_t *context;
   test_chain_t *chain;
   VkCommandBuffer cmd;
   struct
   {
      VkImageView view;
      VkSampler sampler;
      VkPipeline pipeline;
      unsigned dirty;
      struct { float data[16]; } mvp;
   } tracker;
   struct
   {
      struct vk_image ui;
      struct vk_image canvas;
      VkFormat ui_format;
      VkRenderPass ui_render_pass;
      VkRenderPass render_pass;
   } views;
   struct vk_image cached[3];
   unsigned rotation_raw;
   struct
   {
      struct
      {
         VkImage images[VULKAN_OPENXR_MAX_IMAGES][2];
         VkImageView views[VULKAN_OPENXR_MAX_IMAGES][2];
         VkFramebuffer framebuffers[VULKAN_OPENXR_MAX_IMAGES][2];
         unsigned num_images;
         unsigned dims;
         unsigned layers;
         unsigned failed_dims;
         unsigned index[2];
         bool drawn;
         bool released;
      } slots[VIDEO_XR_MAX_SLOTS];
      video_xr_quad_set_t set;
      unsigned ui_dims;
      struct { float data[16]; } mvp;
      VkFormat format;
      bool mutable_format;
      bool proj_shown;
   } xr;
} vk_t;

typedef struct vulkan_filter_chain vulkan_filter_chain_t;
typedef struct vulkan_openxr vulkan_openxr_t;

typedef struct
{
   video_views_map_t views;
} video_frame_info_t;

static unsigned allocations, destructions, warnings, failures;
static unsigned fail_allocation;

static void vulkan_init_render_target(struct vk_image *img, unsigned dims,
      VkFormat format, VkRenderPass pass, test_context_t *context)
{
   (void)dims;
   (void)format;
   (void)pass;
   (void)context;
   allocations++;
   img->image = (VkImage)(uintptr_t)allocations;
   img->memory = fail_allocation == 1 ? VK_NULL_HANDLE
      : (VkDeviceMemory)(uintptr_t)allocations;
   img->framebuffer = fail_allocation == 2 ? VK_NULL_HANDLE
      : (VkFramebuffer)(uintptr_t)allocations;
}

static void vulkan_destroy_hdr_buffer(VkDevice device, struct vk_image *img)
{
   (void)device;
   destructions++;
   memset(img, 0, sizeof(*img));
}

static void RARCH_WARN(const char *format, ...)
{
   (void)format;
   warnings++;
}

static video_xr_quad_set_t planned;
static unsigned acquire_fails;

static void vulkan_xr_plan(vk_t *vk, const video_frame_info_t *video_info,
      bool views, bool eyes, unsigned frame_dims, unsigned src_dims,
      unsigned ui_dims)
{
   (void)video_info;
   (void)views;
   (void)eyes;
   (void)frame_dims;
   (void)src_dims;
   (void)ui_dims;
   vk->xr.set = planned;
}

bool video_xr_shrinks(unsigned image_dims, unsigned source_dims,
      unsigned rotation)
{
   (void)image_dims;
   (void)source_dims;
   (void)rotation;
   return false;
}

static void vulkan_openxr_slot_forget(vulkan_openxr_t *xr, unsigned slot)
{
   (void)xr;
   (void)slot;
}


/* What reached the runtime's images: offscreen runs, final passes,
 * replays of the last frame's, the size of the last, the render passes
 * into a swapchain framebuffer, and frames the chains ended. */
static unsigned offscreen_passes, viewport_passes, replays;
static unsigned last_pass_w, last_pass_h;
static unsigned render_passes, frames_ended;
static bool chain_drawn = true;

static void vulkan_filter_chain_build_offscreen_passes(
      vulkan_filter_chain_t *chain, VkCommandBuffer cmd, const VkViewport *vp)
{
   (void)chain;
   (void)cmd;
   (void)vp;
   offscreen_passes++;
}

static bool vulkan_filter_chain_replay_ready(
      const vulkan_filter_chain_t *chain)
{
   (void)chain;
   return chain_drawn;
}

static void vulkan_filter_chain_build_viewport_pass_replay(
      vulkan_filter_chain_t *chain, VkCommandBuffer cmd,
      const VkViewport *vp, const float *mvp)
{
   (void)chain;
   (void)cmd;
   (void)mvp;
   replays++;
   last_pass_w = (unsigned)vp->width;
   last_pass_h = (unsigned)vp->height;
}

static void vulkan_filter_chain_build_viewport_pass(
      vulkan_filter_chain_t *chain, VkCommandBuffer cmd,
      const VkViewport *vp, const float *mvp)
{
   (void)chain;
   (void)cmd;
   (void)mvp;
   viewport_passes++;
   last_pass_w = (unsigned)vp->width;
   last_pass_h = (unsigned)vp->height;
}

static void vulkan_filter_chain_end_frame(vulkan_filter_chain_t *chain,
      VkCommandBuffer cmd)
{
   (void)chain;
   (void)cmd;
   frames_ended++;
}

static void test_begin_render_pass(VkCommandBuffer cmd,
      const VkRenderPassBeginInfo *rp, VkSubpassContents contents)
{
   (void)cmd;
   (void)contents;
   if (rp->framebuffer)
      render_passes++;
}

static void test_end_render_pass(VkCommandBuffer cmd)
{
   (void)cmd;
}

#define vkCmdBeginRenderPass test_begin_render_pass
#define vkCmdEndRenderPass test_end_render_pass

static unsigned slot_creates[VIDEO_XR_MAX_SLOTS];
static unsigned slot_refuses;

static void vulkan_wait_own_submissions(vk_t *vk)
{
   (void)vk;
}

static void vulkan_xr_slot_free_views(vk_t *vk, unsigned s)
{
   memset(vk->xr.slots[s].views, 0, sizeof(vk->xr.slots[s].views));
   memset(vk->xr.slots[s].framebuffers, 0,
         sizeof(vk->xr.slots[s].framebuffers));
}

static void vulkan_xr_slot_free(vk_t *vk, unsigned s)
{
   memset(&vk->xr.slots[s], 0, sizeof(vk->xr.slots[s]));
}

/* The runtime's swapchains: a refused slot is left with none. */
static bool vulkan_openxr_slot_create(vulkan_openxr_t *xr, unsigned slot,
      VkFormat format, bool mutable_format, unsigned dims, unsigned layers,
      VkImage (*images)[2], unsigned *num_images)
{
   (void)xr;
   (void)format;
   (void)mutable_format;
   (void)dims;
   (void)layers;
   slot_creates[slot]++;
   if (slot_refuses & (1u << slot))
      return false;
   images[0][0] = (VkImage)(uintptr_t)1;
   images[1][0] = (VkImage)(uintptr_t)2;
   *num_images  = 2;
   return true;
}

static VkResult test_create_image_view(VkDevice device,
      const VkImageViewCreateInfo *info, const VkAllocationCallbacks *alloc,
      VkImageView *view)
{
   (void)device;
   (void)info;
   (void)alloc;
   *view = (VkImageView)(uintptr_t)1;
   return VK_SUCCESS;
}

static VkResult test_create_framebuffer(VkDevice device,
      const VkFramebufferCreateInfo *info, const VkAllocationCallbacks *alloc,
      VkFramebuffer *framebuffer)
{
   (void)device;
   (void)info;
   (void)alloc;
   *framebuffer = (VkFramebuffer)(uintptr_t)1;
   return VK_SUCCESS;
}

#define vkCreateImageView test_create_image_view
#define vkCreateFramebuffer test_create_framebuffer

static bool vulkan_openxr_slot_acquire(vulkan_openxr_t *xr, unsigned slot,
      unsigned *index)
{
   (void)xr;
   (void)index;
   return !(acquire_fails & (1u << slot));
}

static void vulkan_xr_copy_ui(vk_t *vk, unsigned s, unsigned ui_dims)
{
   (void)vk;
   (void)s;
   (void)ui_dims;
}

static void vulkan_xr_draw_projection(vk_t *vk,
      const video_frame_info_t *video_info)
{
   (void)vk;
   (void)video_info;
}

#include "driver_code.inc"

#define CHECK(cond) do { if (!(cond)) { \
   printf("FAIL line %u: %s\n", (unsigned)__LINE__, #cond); \
   failures++; } } while (0)

static void init(vk_t *vk, test_context_t *context, test_chain_t *chain)
{
   memset(vk, 0, sizeof(*vk));
   memset(context, 0, sizeof(*context));
   memset(chain, 0, sizeof(*chain));
   context->gpu_properties.limits.maxImageDimension2D = 1024;
   context->gpu_properties.limits.maxFramebufferWidth = 1024;
   context->gpu_properties.limits.maxFramebufferHeight = 1024;
   vk->context = context;
   vk->chain = chain;
   allocations = destructions = warnings = fail_allocation = 0;
   offscreen_passes = viewport_passes = replays = 0;
   last_pass_w = last_pass_h = 0;
   render_passes = frames_ended = 0;
   chain_drawn = true;
   memset(slot_creates, 0, sizeof(slot_creates));
   slot_refuses = 0;
}

static void test_desktop_failure(bool ui)
{
   vk_t vk;
   test_context_t context;
   test_chain_t chain;
   video_views_target_t target;
   struct vk_image *image;
   unsigned dims = VIDEO_SCALE_PACK(64, 32);
   init(&vk, &context, &chain);
   memset(&target, 0, sizeof(target));
   image = ui ? &vk.views.ui : &vk.views.canvas;
   fail_allocation = 1;
   CHECK(!vulkan_vv_target_ensure(&vk, &target, image, dims));
   CHECK(warnings == 1 && allocations == 1 && destructions == 1);
   CHECK(!vulkan_vv_target_ensure(&vk, &target, image, dims));
   CHECK(warnings == 1 && allocations == 1);
}

static void test_retirement_saturation(void)
{
   vk_t vk;
   test_context_t context;
   test_chain_t chain;
   video_views_target_t targets[3];
   unsigned i;
   unsigned dims = VIDEO_SCALE_PACK(64, 32);
   unsigned changed = VIDEO_SCALE_PACK(80, 32);
   init(&vk, &context, &chain);
   memset(targets, 0, sizeof(targets));
   for (i = 0; i < 3; i++)
      CHECK(vulkan_vv_target_ensure(&vk, &targets[i], &vk.cached[i], dims));
   CHECK(vulkan_vv_target_ensure(&vk, &targets[0], &vk.cached[0], changed));
   CHECK(vulkan_vv_target_ensure(&vk, &targets[1], &vk.cached[1], changed));
   CHECK(chain.view_retired[0].image && chain.view_retired[1].image);
   CHECK(!vulkan_vv_target_ensure(&vk, &targets[2], &vk.cached[2], changed));
   CHECK(targets[2].dims == dims && targets[2].made);
   CHECK(allocations == 5 && warnings == 0);
   /* Model a completed frame slot freeing a retired image. */
   vulkan_destroy_hdr_buffer(context.device, &chain.view_retired[0]);
   CHECK(vulkan_vv_target_ensure(&vk, &targets[2], &vk.cached[2], changed));
   CHECK(targets[2].dims == changed && targets[2].made);
   CHECK(allocations == 6 && warnings == 0);
}

/* @screens screens drawn whole, then the menu. */
static void xr_plan(unsigned dims, unsigned screens)
{
   unsigned s;
   memset(&planned, 0, sizeof(planned));
   for (s = 0; s < screens; s++)
   {
      planned.slots[s].dims    = dims;
      planned.slots[s].layers  = 1;
      planned.slots[s].view[0] = -1;
      planned.quads[s].slot    = s;
   }
   planned.slots[VIDEO_XR_MENU_SLOT].dims   = dims;
   planned.slots[VIDEO_XR_MENU_SLOT].layers = 1;
   planned.quads[s].slot                    = VIDEO_XR_MENU_SLOT;
   planned.num_quads                        = screens + 1;
}

/* Any chain: the pass steps are stubbed. */
#define XR_FRAME_CHAIN ((vulkan_filter_chain_t*)(uintptr_t)1)

static void xr_draw_frame(vk_t *vk, unsigned src_dims, bool new_frame)
{
   video_frame_info_t info;
   memset(&info, 0, sizeof(info));
   vulkan_xr_draw(vk, &info, new_frame, true, false, NULL, XR_FRAME_CHAIN,
         NULL, src_dims, src_dims, src_dims, true);
}

static void xr_draw(vk_t *vk, unsigned src_dims)
{
   xr_draw_frame(vk, src_dims, true);
}

/* After the frame's submit, as vulkan_xr_release. */
static void xr_release(vk_t *vk)
{
   unsigned s;
   for (s = 0; s < VIDEO_XR_MAX_SLOTS; s++)
      if (vk->xr.slots[s].drawn)
      {
         vk->xr.slots[s].drawn    = false;
         vk->xr.slots[s].released = true;
      }
}

/* A slot whose image isn't ready keeps its quads: the headset shows its
 * last image, or the swapchains a resize keeps until the new ones have
 * one. */
static void test_xr_draw_unready(void)
{
   vk_t vk;
   test_context_t context;
   test_chain_t chain;
   unsigned dims = VIDEO_SCALE_PACK(64, 32);
   init(&vk, &context, &chain);
   xr_plan(dims, 3);
   acquire_fails = (1u << 0) | (1u << VIDEO_XR_MENU_SLOT);
   xr_draw(&vk, dims);
   CHECK(vk.xr.set.num_quads == 4);
   CHECK(!vk.xr.slots[0].drawn && vk.xr.slots[1].drawn);
   CHECK(!vk.xr.slots[VIDEO_XR_MENU_SLOT].drawn);
   acquire_fails = 0;
   xr_draw(&vk, dims);
   CHECK(vk.xr.set.num_quads == 4);
   CHECK(vk.xr.slots[0].drawn && vk.xr.slots[1].drawn);
}

/* A screen is drawn straight into the runtime's image: one final pass
 * per layer, no image of the driver's own between, and no allocation.
 * A new frame ends the chain's frame; a repeat whose image the runtime
 * has draws nothing. */
static void test_xr_draw_direct(void)
{
   vk_t vk;
   test_context_t context;
   test_chain_t chain;
   unsigned dims = VIDEO_SCALE_PACK(64, 32);
   init(&vk, &context, &chain);
   acquire_fails = 0;
   xr_plan(dims, 2);
   xr_draw(&vk, dims);
   CHECK(vk.xr.slots[0].drawn && vk.xr.slots[1].drawn);
   CHECK(offscreen_passes == 2 && viewport_passes == 2);
   CHECK(render_passes == 2 && frames_ended == 2 && replays == 0);
   CHECK(allocations == 0);
   xr_release(&vk);
   xr_draw_frame(&vk, dims, false);
   CHECK(!vk.xr.slots[0].drawn && !vk.xr.slots[1].drawn);
   CHECK(viewport_passes == 2 && frames_ended == 2);
}

/* A new frame with no frame data - a movie frame the driver imported -
 * reaches the headset, as a core frame does. */
static void test_xr_draw_movie_frame(void)
{
   vk_t vk;
   test_context_t context;
   test_chain_t chain;
   unsigned dims = VIDEO_SCALE_PACK(64, 32);
   init(&vk, &context, &chain);
   acquire_fails = 0;
   xr_plan(dims, 1);
   xr_draw(&vk, dims);
   xr_release(&vk);
   viewport_passes = frames_ended = 0;
   xr_draw_frame(&vk, dims, true);
   CHECK(vk.xr.slots[0].drawn);
   CHECK(viewport_passes == 1 && frames_ended == 1 && replays == 0);
}

/* Resized while paused: the last frame's final pass is replayed at the
 * new size straight into the new image. No other pass runs and no frame
 * ends, so shader feedback and history stay where that frame left them. */
static void test_xr_draw_resize_repeat(void)
{
   vk_t vk;
   test_context_t context;
   test_chain_t chain;
   unsigned dims    = VIDEO_SCALE_PACK(64, 32);
   unsigned changed = VIDEO_SCALE_PACK(80, 40);
   init(&vk, &context, &chain);
   acquire_fails = 0;
   xr_plan(dims, 1);
   xr_draw(&vk, dims);
   xr_release(&vk);
   offscreen_passes = viewport_passes = render_passes = frames_ended = 0;
   xr_plan(changed, 1);
   xr_draw_frame(&vk, dims, false);
   CHECK(vk.xr.slots[0].drawn && vk.xr.slots[0].dims == changed);
   CHECK(replays == 1 && render_passes == 1);
   CHECK(offscreen_passes == 0 && viewport_passes == 0);
   CHECK(frames_ended == 0 && allocations == 0);
   CHECK(last_pass_w == 80 && last_pass_h == 40);
   CHECK(slot_creates[0] == 2);
}

/* A chain with no frame drawn since it was built - a preset loaded while
 * paused - has nothing to replay: its passes run, and no frame ends. */
static void test_xr_draw_repeat_unready(void)
{
   vk_t vk;
   test_context_t context;
   test_chain_t chain;
   unsigned dims    = VIDEO_SCALE_PACK(64, 32);
   unsigned changed = VIDEO_SCALE_PACK(80, 40);
   init(&vk, &context, &chain);
   acquire_fails = 0;
   xr_plan(dims, 1);
   xr_draw(&vk, dims);
   xr_release(&vk);
   offscreen_passes = viewport_passes = frames_ended = 0;
   chain_drawn = false;
   xr_plan(changed, 1);
   xr_draw_frame(&vk, dims, false);
   CHECK(offscreen_passes == 1 && viewport_passes == 1);
   CHECK(replays == 0 && frames_ended == 0);
}

/* Model completed frame slots freeing the retire ring. */
static void xr_retired(vk_t *vk)
{
   unsigned i;
   for (i = 0; i < 2; i++)
      if (vk->chain->view_retired[i].image)
         vulkan_destroy_hdr_buffer(vk->context->device,
               &vk->chain->view_retired[i]);
}

/* A size the runtime refused is not tried again. A resize back to it
 * keeps the quad: the slot still has the swapchain it had. */
static void test_xr_draw_slot_refused(void)
{
   vk_t vk;
   test_context_t context;
   test_chain_t chain;
   unsigned dims    = VIDEO_SCALE_PACK(64, 32);
   unsigned changed = VIDEO_SCALE_PACK(80, 32);
   init(&vk, &context, &chain);
   acquire_fails = 0;
   xr_plan(dims, 1);
   xr_draw(&vk, dims);
   CHECK(vk.xr.set.num_quads == 2 && vk.xr.slots[0].drawn);
   slot_refuses = 1u << 0;
   xr_plan(changed, 1);
   xr_draw(&vk, changed);
   xr_retired(&vk);
   CHECK(vk.xr.set.num_quads == 1);
   CHECK(vk.xr.set.quads[0].slot == VIDEO_XR_MENU_SLOT);
   CHECK(slot_creates[0] == 2);
   slot_refuses = 0;
   xr_plan(dims, 1);
   xr_draw(&vk, dims);
   xr_retired(&vk);
   CHECK(vk.xr.set.num_quads == 2 && vk.xr.slots[0].drawn);
   CHECK(slot_creates[0] == 3);
   xr_plan(changed, 1);
   xr_draw(&vk, changed);
   xr_retired(&vk);
   CHECK(vk.xr.set.num_quads == 2 && !vk.xr.slots[0].drawn);
   CHECK(vk.xr.slots[0].dims == dims && slot_creates[0] == 3);
}

/* A slot the runtime never made is left out, and not tried again every
 * frame. */
static void test_xr_draw_slot_unmade(void)
{
   vk_t vk;
   test_context_t context;
   test_chain_t chain;
   unsigned dims = VIDEO_SCALE_PACK(64, 32);
   init(&vk, &context, &chain);
   acquire_fails = 0;
   slot_refuses  = 1u << 0;
   xr_plan(dims, 1);
   xr_draw(&vk, dims);
   CHECK(vk.xr.set.num_quads == 1);
   CHECK(vk.xr.set.quads[0].slot == VIDEO_XR_MENU_SLOT);
   xr_draw(&vk, dims);
   CHECK(vk.xr.set.num_quads == 1 && slot_creates[0] == 1);
}

int main(void)
{
   test_desktop_failure(false);
   test_desktop_failure(true);
   test_retirement_saturation();
   test_xr_draw_unready();
   test_xr_draw_direct();
   test_xr_draw_movie_frame();
   test_xr_draw_resize_repeat();
   test_xr_draw_repeat_unready();
   test_xr_draw_slot_refused();
   test_xr_draw_slot_unmade();
   printf("%u failed (10 Vulkan target cases)\n", failures);
   return failures ? 1 : 0;
}
