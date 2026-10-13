/* Vulkan retained-target failures use the real allocator, retirement and
 * shared ensure policy. Only device allocation and logging are replaced.
 * The XR draw, its retained cache and its slots run with the runtime's
 * swapchains and the pass steps stubbed. */
#include <stdio.h>
#include <string.h>
#include <vulkan/vulkan.h>
#include "gfx/video_views.h"
#include "gfx/video_defines.h"
#include "gfx/video_xr.h"

#define VULKAN_OPENXR_MAX_IMAGES 4

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
      struct
      {
         struct vk_image images[2];
         video_views_target_t targets[2];
         bool valid;
      } cached[RETRO_VIDEO_VIEWS_MAX];
      video_xr_quad_set_t set;
      video_views_map_t cached_map;
      unsigned cached_dims;
      unsigned cached_rotation;
      unsigned ui_dims;
      bool cached_views;
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

static void vulkan_filter_chain_build_offscreen_passes(
      vulkan_filter_chain_t *chain, VkCommandBuffer cmd, const VkViewport *vp)
{
   (void)chain;
   (void)cmd;
   (void)vp;
}

static void vulkan_filter_chain_build_viewport_pass(
      vulkan_filter_chain_t *chain, VkCommandBuffer cmd,
      const VkViewport *vp, const float *mvp)
{
   (void)chain;
   (void)cmd;
   (void)vp;
   (void)mvp;
}

static void vulkan_filter_chain_end_frame(vulkan_filter_chain_t *chain,
      VkCommandBuffer cmd)
{
   (void)chain;
   (void)cmd;
}

static void vulkan_views_begin(vk_t *vk, const struct vk_image *img,
      unsigned dims, VkRenderPass render_pass)
{
   (void)vk;
   (void)img;
   (void)dims;
   (void)render_pass;
}

static void vulkan_views_end(vk_t *vk, const struct vk_image *img)
{
   (void)vk;
   (void)img;
}

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

static void vulkan_xr_copy_cached(vk_t *vk, unsigned s)
{
   (void)vk;
   (void)s;
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
   memset(slot_creates, 0, sizeof(slot_creates));
   slot_refuses = 0;
}

static void test_xr_failure(unsigned mode)
{
   vk_t vk;
   test_context_t context;
   test_chain_t chain;
   video_views_target_t target;
   unsigned dims = VIDEO_SCALE_PACK(64, 32);
   unsigned changed = VIDEO_SCALE_PACK(80, 32);
   init(&vk, &context, &chain);
   memset(&target, 0, sizeof(target));
   fail_allocation = mode;
   if (!mode)
      context.gpu_properties.limits.maxImageDimension2D = 16;
   CHECK(!vulkan_vv_target_ensure(&vk, &target, &vk.cached[0], dims));
   CHECK(target.dims == dims && !target.made);
   CHECK(!vk.cached[0].image);
   CHECK(warnings == 0);
   CHECK(allocations == (mode ? 1U : 0U));
   CHECK(destructions == allocations);
   /* Recovery at the device alone must not retry the same dimensions. */
   fail_allocation = 0;
   context.gpu_properties.limits.maxImageDimension2D = 1024;
   CHECK(!vulkan_vv_target_ensure(&vk, &target, &vk.cached[0], dims));
   CHECK(allocations == (mode ? 1U : 0U));
   CHECK(vulkan_vv_target_ensure(&vk, &target, &vk.cached[0], changed));
   CHECK(target.dims == changed && target.made);
   CHECK(allocations == (mode ? 2U : 1U));
   CHECK(warnings == 0);
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

static void xr_draw(vk_t *vk, unsigned src_dims)
{
   video_frame_info_t info;
   memset(&info, 0, sizeof(info));
   vulkan_xr_draw(vk, &info, true, true, false, NULL, NULL, NULL,
         src_dims, src_dims, src_dims, true);
}

/* A slot whose image isn't ready keeps its quads: the headset shows its
 * last image, or the swapchains a resize keeps until the new ones have
 * one. So does a screen whose resize waits for the retire ring. */
static void test_xr_draw_unready(void)
{
   vk_t vk;
   test_context_t context;
   test_chain_t chain;
   unsigned dims    = VIDEO_SCALE_PACK(64, 32);
   unsigned changed = VIDEO_SCALE_PACK(80, 32);
   init(&vk, &context, &chain);
   xr_plan(dims, 3);
   acquire_fails = (1u << 0) | (1u << VIDEO_XR_MENU_SLOT);
   xr_draw(&vk, dims);
   CHECK(vk.xr.set.num_quads == 4);
   CHECK(!vk.xr.slots[0].drawn && vk.xr.slots[1].drawn);
   CHECK(!vk.xr.slots[VIDEO_XR_MENU_SLOT].drawn);
   acquire_fails = 0;
   xr_plan(changed, 3);
   xr_draw(&vk, changed);
   CHECK(vk.xr.set.num_quads == 4);
   CHECK(vk.xr.slots[0].drawn && vk.xr.slots[1].drawn);
   CHECK(!vk.xr.slots[2].drawn);
   CHECK(vk.xr.cached[2].targets[0].made);
   CHECK(vk.xr.cached[2].targets[0].dims == dims);
   /* Model a completed frame slot freeing a retired image. */
   vulkan_destroy_hdr_buffer(context.device, &chain.view_retired[0]);
   xr_draw(&vk, changed);
   CHECK(vk.xr.set.num_quads == 4 && vk.xr.slots[2].drawn);
   CHECK(vk.xr.cached[2].targets[0].dims == changed);
}

/* A screen whose target was never made is left out, and is not tried
 * again every frame. */
static void test_xr_draw_unmade(void)
{
   vk_t vk;
   test_context_t context;
   test_chain_t chain;
   unsigned dims = VIDEO_SCALE_PACK(64, 32);
   init(&vk, &context, &chain);
   xr_plan(dims, 1);
   acquire_fails   = 0;
   fail_allocation = 1;
   xr_draw(&vk, dims);
   CHECK(vk.xr.set.num_quads == 1);
   CHECK(vk.xr.set.quads[0].slot == VIDEO_XR_MENU_SLOT);
   CHECK(!vk.xr.slots[0].drawn && allocations == 1);
   fail_allocation = 0;
   xr_draw(&vk, dims);
   CHECK(vk.xr.set.num_quads == 1 && allocations == 1);
   CHECK(warnings == 0);
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
   test_xr_failure(0);
   test_xr_failure(1);
   test_xr_failure(2);
   test_desktop_failure(false);
   test_desktop_failure(true);
   test_retirement_saturation();
   test_xr_draw_unready();
   test_xr_draw_unmade();
   test_xr_draw_slot_refused();
   test_xr_draw_slot_unmade();
   printf("%u failed (10 Vulkan target cases)\n", failures);
   return failures ? 1 : 0;
}
