/* Vulkan retained-target failures use the real allocator, retirement and
 * shared ensure policy. Only device allocation and logging are replaced.
 * The XR draw and its slots run with the runtime's swapchains and the
 * pass steps stubbed: what reaches the runtime's images, when a chain
 * ends a frame, and when a repeat replays the last frame's final pass.
 * The second outputs' render passes run against a fake device, and the
 * headset's format pick against a fake runtime; a screen window's
 * output is checked for whether it encodes HDR. */
#include <stdio.h>
#include <string.h>
#include <vulkan/vulkan.h>
#include <retro_miscellaneous.h>
#include "gfx/video_views.h"
#include "gfx/video_defines.h"
#include "gfx/video_xr.h"

#define VULKAN_OPENXR_MAX_IMAGES 4
#define OPENXR_PROJECTION_SLOT VIDEO_XR_MAX_SLOTS
#define OPENXR_MAX_SLOTS (VIDEO_XR_MAX_SLOTS + 1)
#define RGBA16_WHITE (~(uint64_t)0)
#define VULKAN_DIRTY_DYNAMIC_BIT 0x0001
#define VULKAN_HDR_SWAPCHAIN
#define VK_CTX_FLAG_HDR_ENABLE 0x1
#define VK_CTX_FLAG_HDR_SCRGB 0x2
#define VULKAN_IMAGE_LAYOUT_TRANSITION(cmd, img, a, b, c, d, e, f) \
   do { (void)(cmd); (void)(img); } while (0)

struct vk_image
{
   VkImage image;
   VkDeviceMemory memory;
   VkFramebuffer framebuffer;
};

struct vk_vertex
{
   float x, y, tex_x, tex_y;
   uint16_t color[4];
};

struct vk_effect_template
{
   VkGraphicsPipelineCreateInfo           pipe;
   VkPipelineInputAssemblyStateCreateInfo input_assembly;
   VkPipelineVertexInputStateCreateInfo   vertex_input;
   VkVertexInputAttributeDescription      attributes[3];
   VkVertexInputBindingDescription        binding;
   VkPipelineRasterizationStateCreateInfo raster;
   VkPipelineColorBlendAttachmentState    blend_attachment;
   VkPipelineColorBlendStateCreateInfo    blend;
   VkPipelineMultisampleStateCreateInfo   multisample;
   VkPipelineViewportStateCreateInfo      vp;
   VkPipelineDepthStencilStateCreateInfo  depth_stencil;
   VkPipelineDynamicStateCreateInfo       dynamic;
   bool                                   valid;
};

struct vk_target_pass
{
   VkRenderPass pass;
   VkPipeline premult;
   VkPipeline stock_rect;
   VkFormat format;
};

#define VK_VIEWS_TARGETS 4

typedef struct { float data[16]; } math_matrix_4x4;

struct vk_texture
{
   VkImage image;
   VkImageView view;
};

struct vk_draw_quad
{
   struct vk_texture *texture;
   const math_matrix_4x4 *mvp;
   VkPipeline pipeline;
   VkSampler sampler;
   uint64_t color;
};

typedef struct
{
   struct { VkPhysicalDeviceLimits limits; } gpu_properties;
   VkDevice device;
   VkFormat swapchain_format;
   uint32_t flags;
   struct vulkan_openxr *xr;
} test_context_t;

typedef struct
{
   struct vk_image view_retired[2];
} test_chain_t;

typedef struct vulkan_filter_chain vulkan_filter_chain_t;

struct vulkan_filter_chain_swapchain_info
{
   VkViewport vp;
   VkFormat format;
   VkRenderPass render_pass;
   unsigned num_indices;
};

typedef struct
{
   test_context_t *context;
   test_chain_t *chain;
   vulkan_filter_chain_t *filter_chain;
   vulkan_filter_chain_t *filter_chain_default;
   VkRenderPass render_pass;
   VkRenderPass sdr_render_pass;
   float hdr_scanlines_latched;
   unsigned hdr_subpixel_latched;
   struct
   {
      struct
      {
         float paper_white_nits;
         unsigned expand_gamut;
         float inverse_tonemap;
         float hdr10;
      } ubo_values;
   } hdr;
   VkCommandBuffer cmd;
   struct video_viewport vp;
   VkViewport vk_vp;
   math_matrix_4x4 mvp_no_rot;
   struct { VkSampler linear, nearest; } samplers;
   struct vk_per_frame
   {
      struct vk_texture view_images[RETRO_VIDEO_VIEWS_MAX];
   } swapchain[2];
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
      char preset[64];
      unsigned last_index;
      unsigned copied;
      bool fp16;
      bool canvas_fp16;
      bool scanlines_logged;
      bool preset_hdr_logged;
      struct vk_target_pass targets[VK_VIEWS_TARGETS];
      unsigned passes_freed;
   } views;
   struct { struct vk_effect_template effect_tpl[2]; } display;
   struct
   {
      VkPipeline alpha_premult;
      VkPipeline stock_rect;
      VkPipelineCache cache;
   } pipelines;
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
      } slots[OPENXR_MAX_SLOTS];
      video_xr_quad_set_t set;
      unsigned ui_dims;
      unsigned proj_dims;
      struct { float data[16]; } mvp;
      VkFormat window_format;
      bool window_hdr;
      VkFormat format;
      bool mutable_format;
      VkFormat view_format;
      VkRenderPass render_pass;
      bool proj_shown;
   } xr;
} vk_t;

typedef struct vulkan_openxr vulkan_openxr_t;

typedef struct
{
   VkRenderPass render_pass;
   VkFormat format;
   VkColorSpaceKHR color_space;
} vulkan_output_t;

struct vk_window
{
   struct { void *surface; } native;
   vulkan_output_t output;
   void *surface;
   unsigned passes_freed;
};

typedef struct
{
   video_views_map_t views;
   int headset_eye[2];
   bool shader_active;
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

static unsigned logs;

static void RARCH_ERR(const char *format, ...)
{
   (void)format;
}

static void RARCH_LOG(const char *format, ...)
{
   (void)format;
   logs++;
}

/* What the main chain's preset writes */
static bool emits_hdr10, emits_hdr16;

static bool vulkan_chain_emits_hdr10(vk_t *vk, vulkan_filter_chain_t *chain)
{
   (void)vk;
   (void)chain;
   return emits_hdr10;
}

static bool vulkan_chain_emits_hdr16(vk_t *vk, vulkan_filter_chain_t *chain)
{
   (void)vk;
   (void)chain;
   return emits_hdr16;
}

/* The HDR values last given a chain */
static float set_paper_white, set_scanlines, set_inverse_tonemap, set_hdr10;
static unsigned set_expand_gamut, set_subpixel;

static void vulkan_filter_chain_set_paper_white_nits(
      vulkan_filter_chain_t *chain, float paper_white_nits)
{
   (void)chain;
   set_paper_white = paper_white_nits;
}

static void vulkan_filter_chain_set_expand_gamut(
      vulkan_filter_chain_t *chain, unsigned expand_gamut)
{
   (void)chain;
   set_expand_gamut = expand_gamut;
}

static void vulkan_filter_chain_set_scanlines(vulkan_filter_chain_t *chain,
      float scanlines)
{
   (void)chain;
   set_scanlines = scanlines;
}

static void vulkan_filter_chain_set_subpixel_layout(
      vulkan_filter_chain_t *chain, unsigned subpixel_layout)
{
   (void)chain;
   set_subpixel = subpixel_layout;
}

static void vulkan_filter_chain_set_inverse_tonemap(
      vulkan_filter_chain_t *chain, float inverse_tonemap)
{
   (void)chain;
   set_inverse_tonemap = inverse_tonemap;
}

static void vulkan_filter_chain_set_hdr10(vulkan_filter_chain_t *chain,
      float hdr10)
{
   (void)chain;
   set_hdr10 = hdr10;
}

static void hdr_values_unset(void)
{
   set_paper_white  = set_scanlines = -1.0f;
   set_inverse_tonemap = set_hdr10  = -1.0f;
   set_expand_gamut = set_subpixel  = 99;
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
/* The pass the last one began, and how many began another before it */
static VkRenderPass began_pass;
static unsigned mixed_passes;

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
   if (began_pass && rp->renderPass != began_pass)
      mixed_passes++;
   began_pass = rp->renderPass;
}

static void test_end_render_pass(VkCommandBuffer cmd)
{
   (void)cmd;
}

#define vkCmdBeginRenderPass test_begin_render_pass
#define vkCmdEndRenderPass test_end_render_pass

static unsigned slot_creates[OPENXR_MAX_SLOTS];
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

/* The format the last image view was made in, and the pass the last
 * framebuffer was made for */
static VkFormat view_format_made;
static VkRenderPass framebuffer_pass;

static VkResult test_create_image_view(VkDevice device,
      const VkImageViewCreateInfo *info, const VkAllocationCallbacks *alloc,
      VkImageView *view)
{
   (void)device;
   (void)alloc;
   view_format_made = info->format;
   *view = (VkImageView)(uintptr_t)1;
   return VK_SUCCESS;
}

static VkResult test_create_framebuffer(VkDevice device,
      const VkFramebufferCreateInfo *info, const VkAllocationCallbacks *alloc,
      VkFramebuffer *framebuffer)
{
   (void)device;
   (void)alloc;
   framebuffer_pass = info->renderPass;
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

/* The UI layer and the tracked eyes: quads drawn, and the pipeline the
 * last was drawn with and how many took another before it */
static unsigned quads_drawn, mixed_quads;
static VkPipeline quad_pipeline;
static unsigned eye_dims;

static void vulkan_views_texture(const struct vk_image *img,
      unsigned dims, VkFormat format, struct vk_texture *tex)
{
   (void)dims;
   (void)format;
   memset(tex, 0, sizeof(*tex));
   tex->image = img->image;
}

static void vulkan_draw_quad(vk_t *vk, const struct vk_draw_quad *quad)
{
   (void)vk;
   if (quads_drawn && quad->pipeline != quad_pipeline)
      mixed_quads++;
   quad_pipeline = quad->pipeline;
   quads_drawn++;
}

static unsigned vulkan_openxr_eye_dims(const vulkan_openxr_t *xr)
{
   (void)xr;
   return eye_dims;
}

/* The runtime's swapchain formats, and what the pick did on a change */
static VkFormat runtime_formats[4];
static unsigned xr_frees, xr_retargets;

static bool vulkan_openxr_supports_format(const vulkan_openxr_t *xr,
      VkFormat format)
{
   unsigned i;
   (void)xr;
   for (i = 0; i < 4; i++)
      if (runtime_formats[i] == format)
         return true;
   return false;
}

static void vulkan_xr_free(vk_t *vk)
{
   (void)vk;
   xr_frees++;
}

static void vulkan_xr_retarget(vk_t *vk)
{
   (void)vk;
   xr_retargets++;
}

/* The second outputs' render passes: what the device made and
 * destroyed */
static unsigned passes_made, passes_destroyed, pipes_made, pipes_destroyed;
static int modules_live;
static bool fail_pipeline;
static VkFormat pass_format;

static const uint32_t spirv_alpha_blend_vert[] = { 0x07230203 };
static const uint32_t spirv_alpha_blend_frag[] = { 0x07230203 };
static const uint32_t spirv_stock_rect_frag[] = { 0x07230203 };
#define spirv_alpha_blend_vert_size sizeof(spirv_alpha_blend_vert)
#define spirv_alpha_blend_frag_size sizeof(spirv_alpha_blend_frag)
#define spirv_stock_rect_frag_size sizeof(spirv_stock_rect_frag)
/* The blend each pipeline made was made with, by its number */
static bool pipe_blends[64];

static VkResult test_create_render_pass(VkDevice device,
      const VkRenderPassCreateInfo *info, const VkAllocationCallbacks *alloc,
      VkRenderPass *pass)
{
   (void)device;
   (void)alloc;
   pass_format = info->pAttachments[0].format;
   *pass       = (VkRenderPass)(uintptr_t)(0x100 + ++passes_made);
   return VK_SUCCESS;
}

static void test_destroy_render_pass(VkDevice device, VkRenderPass pass,
      const VkAllocationCallbacks *alloc)
{
   (void)device;
   (void)alloc;
   if (pass != VK_NULL_HANDLE)
      passes_destroyed++;
}

static VkResult test_create_shader_module(VkDevice device,
      const VkShaderModuleCreateInfo *info, const VkAllocationCallbacks *alloc,
      VkShaderModule *module)
{
   (void)device;
   (void)info;
   (void)alloc;
   *module = (VkShaderModule)(uintptr_t)(0x200 + ++modules_live);
   return VK_SUCCESS;
}

static void test_destroy_shader_module(VkDevice device,
      VkShaderModule module, const VkAllocationCallbacks *alloc)
{
   (void)device;
   (void)module;
   (void)alloc;
   modules_live--;
}

/* alpha_premult's state, against the pass just made */
static VkResult test_create_graphics_pipelines(VkDevice device,
      VkPipelineCache cache, uint32_t count,
      const VkGraphicsPipelineCreateInfo *info,
      const VkAllocationCallbacks *alloc, VkPipeline *pipe)
{
   (void)device;
   (void)cache;
   (void)alloc;
   if (     fail_pipeline || count != 1
         || info->renderPass != (VkRenderPass)(uintptr_t)(0x100 + passes_made)
         || info->pInputAssemblyState->topology
            != VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
         || info->pColorBlendState->pAttachments->srcColorBlendFactor
            != VK_BLEND_FACTOR_ONE)
      return VK_ERROR_INITIALIZATION_FAILED;
   *pipe = (VkPipeline)(uintptr_t)(0x300 + ++pipes_made);
   if (pipes_made < 64)
      pipe_blends[pipes_made] =
         info->pColorBlendState->pAttachments->blendEnable != VK_FALSE;
   return VK_SUCCESS;
}

static void test_destroy_pipeline(VkDevice device, VkPipeline pipe,
      const VkAllocationCallbacks *alloc)
{
   (void)device;
   (void)alloc;
   if (pipe != VK_NULL_HANDLE)
      pipes_destroyed++;
}

#define vkCreateRenderPass test_create_render_pass
#define vkDestroyRenderPass test_destroy_render_pass
#define vkCreateShaderModule test_create_shader_module
#define vkDestroyShaderModule test_destroy_shader_module
#define vkCreateGraphicsPipelines test_create_graphics_pipelines
#define vkDestroyPipeline test_destroy_pipeline

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
   began_pass = VK_NULL_HANDLE;
   mixed_passes = 0;
   view_format_made = VK_FORMAT_UNDEFINED;
   framebuffer_pass = VK_NULL_HANDLE;
   quads_drawn = mixed_quads = eye_dims = 0;
   quad_pipeline = VK_NULL_HANDLE;
   memset(runtime_formats, 0, sizeof(runtime_formats));
   xr_frees = xr_retargets = 0;
   chain_drawn = true;
   memset(slot_creates, 0, sizeof(slot_creates));
   slot_refuses = 0;
   logs = 0;
   passes_made = passes_destroyed = pipes_made = pipes_destroyed = 0;
   modules_live = 0;
   fail_pipeline = false;
   pass_format = VK_FORMAT_UNDEFINED;
   emits_hdr10 = emits_hdr16 = false;
   hdr_values_unset();
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

/* Second outputs' render passes: views.render_pass in the window's
 * format, one per other format, the same one when asked again, each with
 * alpha_premult's pipeline or neither, and each destroyed once */
static void test_target_passes(void)
{
   vk_t vk;
   test_context_t context;
   test_chain_t chain;
   VkRenderPass a, b;
   init(&vk, &context, &chain);
   context.swapchain_format       = VK_FORMAT_A2R10G10B10_UNORM_PACK32;
   vk.views.render_pass           = (VkRenderPass)(uintptr_t)0x77;
   vk.pipelines.alpha_premult     = (VkPipeline)(uintptr_t)0x78;
   vk.display.effect_tpl[0].valid = true;
   CHECK(vulkan_views_target_pass(&vk, context.swapchain_format, true)
         == vk.views.render_pass);
   CHECK(vulkan_views_target_premult(&vk, context.swapchain_format)
         == vk.pipelines.alpha_premult);
   CHECK(vulkan_views_target_pass(&vk, VK_FORMAT_B8G8R8A8_UNORM, false)
         == VK_NULL_HANDLE);
   CHECK(passes_made == 0);
   a = vulkan_views_target_pass(&vk, VK_FORMAT_B8G8R8A8_UNORM, true);
   CHECK(a != VK_NULL_HANDLE && pass_format == VK_FORMAT_B8G8R8A8_UNORM);
   b = vulkan_views_target_pass(&vk, VK_FORMAT_R8G8B8A8_UNORM, true);
   CHECK(b != VK_NULL_HANDLE && b != a
         && pass_format == VK_FORMAT_R8G8B8A8_UNORM);
   CHECK(vulkan_views_target_pass(&vk, VK_FORMAT_B8G8R8A8_UNORM, true) == a);
   CHECK(vulkan_views_target_pass(&vk, VK_FORMAT_B8G8R8A8_UNORM, false) == a);
   CHECK(passes_made == 2 && pipes_made == 4 && modules_live == 0);
   CHECK(vulkan_views_target_premult(&vk, VK_FORMAT_B8G8R8A8_UNORM)
         != VK_NULL_HANDLE);
   /* Each with stock_rect beside alpha_premult, opaque */
   vk.pipelines.stock_rect = (VkPipeline)(uintptr_t)0x79;
   CHECK(vulkan_views_target_stock(&vk, context.swapchain_format)
         == vk.pipelines.stock_rect);
   CHECK(vulkan_views_target_stock(&vk, VK_FORMAT_B8G8R8A8_UNORM)
         != VK_NULL_HANDLE);
   CHECK(vulkan_views_target_stock(&vk, VK_FORMAT_B8G8R8A8_UNORM)
         != vulkan_views_target_premult(&vk, VK_FORMAT_B8G8R8A8_UNORM));
   CHECK(!pipe_blends[(uintptr_t)vulkan_views_target_stock(&vk,
            VK_FORMAT_B8G8R8A8_UNORM) - 0x300]);
   CHECK(pipe_blends[(uintptr_t)vulkan_views_target_premult(&vk,
            VK_FORMAT_B8G8R8A8_UNORM) - 0x300]);
   CHECK(vulkan_views_target_stock(&vk, VK_FORMAT_R16G16B16A16_SFLOAT)
         == VK_NULL_HANDLE);
   CHECK(vulkan_views_target_premult(&vk, VK_FORMAT_B8G8R8A8_UNORM)
         != vulkan_views_target_premult(&vk, VK_FORMAT_R8G8B8A8_UNORM));
   vulkan_views_targets_free(&vk);
   CHECK(passes_destroyed == 2 && pipes_destroyed == 4);
   CHECK(vulkan_views_target_pass(&vk, VK_FORMAT_B8G8R8A8_UNORM, false)
         == VK_NULL_HANDLE);
   fail_pipeline = true;
   CHECK(vulkan_views_target_pass(&vk, VK_FORMAT_B8G8R8A8_UNORM, true)
         == VK_NULL_HANDLE);
   CHECK(passes_made == 3 && passes_destroyed == 3 && modules_live == 0);
   CHECK(vulkan_views_target_premult(&vk, VK_FORMAT_B8G8R8A8_UNORM)
         == VK_NULL_HANDLE);

   /* A full cache makes no more */
   fail_pipeline = false;
   CHECK(vulkan_views_target_pass(&vk, VK_FORMAT_B8G8R8A8_UNORM, true)
         != VK_NULL_HANDLE);
   CHECK(vulkan_views_target_pass(&vk, VK_FORMAT_R8G8B8A8_UNORM, true)
         != VK_NULL_HANDLE);
   CHECK(vulkan_views_target_pass(&vk, VK_FORMAT_B8G8R8A8_SRGB, true)
         != VK_NULL_HANDLE);
   CHECK(vulkan_views_target_pass(&vk, VK_FORMAT_A2B10G10R10_UNORM_PACK32,
            true) != VK_NULL_HANDLE);
   CHECK(passes_made == 7);
   CHECK(vulkan_views_target_pass(&vk, VK_FORMAT_R8G8B8A8_SRGB, true)
         == VK_NULL_HANDLE);
   CHECK(passes_made == 7);
   vulkan_views_targets_free(&vk);
   CHECK(passes_destroyed == 7 && pipes_destroyed == 12);

   /* Nor any without the menu effects' pipeline state */
   vk.display.effect_tpl[0].valid = false;
   CHECK(vulkan_views_target_pass(&vk, VK_FORMAT_B8G8R8A8_UNORM, true)
         == VK_NULL_HANDLE);
   CHECK(passes_made == 7 && pipes_made == 12 && modules_live == 0);
}

/* A screen window encodes HDR as the main window does on an output that
 * took the main window's HDR pair; an SDR output, or one in a format the
 * encode passes are not made for, takes the copy */
static void test_window_encodes(void)
{
   vk_t vk;
   test_context_t context;
   test_chain_t chain;
   vulkan_output_t out;
   unsigned mode = 0;
   init(&vk, &context, &chain);
   memset(&out, 0, sizeof(out));
   context.flags            = VK_CTX_FLAG_HDR_ENABLE;
   context.swapchain_format = VK_FORMAT_A2B10G10R10_UNORM_PACK32;

   out.format      = VK_FORMAT_B8G8R8A8_UNORM;
   out.color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
   CHECK(!vulkan_window_encodes(&vk, &out, &mode));
   out.format      = context.swapchain_format;
   CHECK(!vulkan_window_encodes(&vk, &out, &mode));

   out.color_space = VK_COLOR_SPACE_HDR10_ST2084_EXT;
   CHECK(vulkan_window_encodes(&vk, &out, &mode) && mode == 1);

   context.flags            = VK_CTX_FLAG_HDR_ENABLE | VK_CTX_FLAG_HDR_SCRGB;
   context.swapchain_format = VK_FORMAT_R16G16B16A16_SFLOAT;
   out.format               = context.swapchain_format;
   out.color_space          = VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
   mode                     = 0;
   CHECK(vulkan_window_encodes(&vk, &out, &mode) && mode == 2);

   out.format      = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
   out.color_space = VK_COLOR_SPACE_HDR10_ST2084_EXT;
   CHECK(!vulkan_window_encodes(&vk, &out, &mode));
}

/* A swapchain rebuild remakes a screen window's output, also when the
 * pass in the window's format comes back with the same handle */
static void test_window_output_stale(void)
{
   vk_t vk;
   test_context_t context;
   test_chain_t chain;
   struct vk_window win;
   init(&vk, &context, &chain);
   memset(&win, 0, sizeof(win));
   context.swapchain_format       = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
   vk.views.render_pass           = (VkRenderPass)(uintptr_t)0x77;
   vk.pipelines.alpha_premult     = (VkPipeline)(uintptr_t)0x78;
   vk.display.effect_tpl[0].valid = true;
   win.surface                    = &win;
   win.native.surface             = &win;
   win.output.format              = context.swapchain_format;
   win.output.render_pass         = vk.views.render_pass;
   CHECK(!vulkan_window_output_stale(&vk, &win));
   win.native.surface = &chain;
   CHECK(vulkan_window_output_stale(&vk, &win));
   win.native.surface = &win;

   vulkan_views_targets_free(&vk);
   CHECK(vulkan_window_output_stale(&vk, &win));

   /* One in a format of its own */
   memset(&win, 0, sizeof(win));
   win.surface            = &win;
   win.native.surface     = &win;
   win.passes_freed       = vk.views.passes_freed;
   win.output.format      = VK_FORMAT_B8G8R8A8_UNORM;
   win.output.render_pass = vulkan_views_target_pass(&vk,
         win.output.format, true);
   CHECK(win.output.render_pass != VK_NULL_HANDLE);
   CHECK(!vulkan_window_output_stale(&vk, &win));
   vulkan_views_targets_free(&vk);
   CHECK(vulkan_window_output_stale(&vk, &win));
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
   /* and the UI layer drawn into its slot */
   CHECK(render_passes == 3 && frames_ended == 2 && replays == 0);
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
   /* The screen's, and the UI's drawn to the menu's new size */
   CHECK(replays == 1 && render_passes == 2);
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

/* Render passes the tests tell apart */
#define TEST_RENDER_PASS     ((VkRenderPass)(uintptr_t)0x10)
#define TEST_SDR_RENDER_PASS ((VkRenderPass)(uintptr_t)0x20)
#define TEST_FP16            VK_FORMAT_R16G16B16A16_SFLOAT

static bool chain_targets(vk_t *vk, bool preset, VkRenderPass pass,
      VkFormat format)
{
   struct vulkan_filter_chain_swapchain_info info;
   memset(&info, 0, sizeof(info));
   vulkan_views_chain_target(vk, &info, preset);
   return info.render_pass == pass && info.format == format;
}

/* With HDR output a view chain draws into FP16 for the HDR game pass to
 * encode, unless it is a preset's and the preset writes the output's
 * encoding, by the main window's rule. */
static void test_view_chain_target(void)
{
   vk_t vk;
   test_context_t context;
   test_chain_t chain;
   init(&vk, &context, &chain);
   vk.render_pass     = TEST_RENDER_PASS;
   vk.sdr_render_pass = TEST_SDR_RENDER_PASS;
   vk.filter_chain    = XR_FRAME_CHAIN;

   context.swapchain_format = VK_FORMAT_B8G8R8A8_UNORM;
   strcpy(vk.views.preset, "pass.slangp");
   CHECK(chain_targets(&vk, true, TEST_RENDER_PASS,
            VK_FORMAT_B8G8R8A8_UNORM));
   CHECK(chain_targets(&vk, false, TEST_RENDER_PASS,
            VK_FORMAT_B8G8R8A8_UNORM));

   context.flags            = VK_CTX_FLAG_HDR_ENABLE;
   context.swapchain_format = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
   vk.views.preset[0]       = '\0';
   CHECK(chain_targets(&vk, false, TEST_SDR_RENDER_PASS, TEST_FP16));
   strcpy(vk.views.preset, "pass.slangp");
   CHECK(chain_targets(&vk, true, TEST_SDR_RENDER_PASS, TEST_FP16));
   emits_hdr10 = true;
   CHECK(chain_targets(&vk, true, TEST_RENDER_PASS,
            VK_FORMAT_A2B10G10R10_UNORM_PACK32));
   CHECK(chain_targets(&vk, false, TEST_SDR_RENDER_PASS, TEST_FP16));

   context.flags            = VK_CTX_FLAG_HDR_ENABLE | VK_CTX_FLAG_HDR_SCRGB;
   context.swapchain_format = VK_FORMAT_R16G16B16A16_SFLOAT;
   CHECK(chain_targets(&vk, true, TEST_SDR_RENDER_PASS, TEST_FP16));
   emits_hdr10 = false;
   emits_hdr16 = true;
   CHECK(chain_targets(&vk, true, TEST_RENDER_PASS,
            VK_FORMAT_R16G16B16A16_SFLOAT));
}

/* A view chain takes the main chain's HDR values but no scanlines, which
 * views do not draw, and the log says so once. */
static void test_view_chain_hdr_semantics(void)
{
   vk_t vk;
   test_context_t context;
   test_chain_t chain;
   init(&vk, &context, &chain);
   context.flags                      = VK_CTX_FLAG_HDR_ENABLE;
   vk.hdr.ubo_values.paper_white_nits = 203.0f;
   vk.hdr.ubo_values.expand_gamut     = 0;
   vk.hdr_subpixel_latched            = 1;
   vk.hdr.ubo_values.inverse_tonemap  = 1.0f;
   vk.hdr.ubo_values.hdr10            = 1.0f;
   vk.hdr_scanlines_latched           = 1.0f;
   vulkan_views_hdr_semantics(&vk, XR_FRAME_CHAIN);
   CHECK(set_paper_white == 203.0f);
   CHECK(set_expand_gamut == 0);
   CHECK(set_scanlines == 0.0f);
   CHECK(set_subpixel == 1);
   CHECK(set_inverse_tonemap == 1.0f);
   CHECK(set_hdr10 == 1.0f);
   CHECK(logs == 1);
   vulkan_views_hdr_semantics(&vk, XR_FRAME_CHAIN);
   CHECK(logs == 1);

   /* Without HDR output the chain is left alone */
   context.flags = 0;
   hdr_values_unset();
   vulkan_views_hdr_semantics(&vk, XR_FRAME_CHAIN);
   CHECK(set_paper_white == -1.0f && set_hdr10 == -1.0f);
}


#define TEST_XR_PASS ((VkRenderPass)(uintptr_t)0x30)

/* Under HDR output the headset stays SDR: its slots' image views take
 * its own format, and their framebuffers its own pass. */
static void test_xr_slots_sdr_under_hdr(void)
{
   vk_t vk;
   test_context_t context;
   test_chain_t chain;
   unsigned dims = VIDEO_SCALE_PACK(64, 32);
   init(&vk, &context, &chain);
   context.flags            = VK_CTX_FLAG_HDR_ENABLE;
   context.swapchain_format = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
   vk.views.render_pass     = TEST_RENDER_PASS;
   vk.xr.view_format        = VK_FORMAT_B8G8R8A8_UNORM;
   vk.xr.render_pass        = TEST_XR_PASS;
   CHECK(vulkan_xr_slot_ensure(&vk, 0, dims, 1));
   CHECK(view_format_made == VK_FORMAT_B8G8R8A8_UNORM);
   CHECK(framebuffer_pass == TEST_XR_PASS);
}

/* Everything the headset draws begins its own pass: the screens, the UI
 * layer and the tracked eyes, the last two with the premultiplied
 * pipeline made against it. */
static void test_xr_draw_in_xr_pass(void)
{
   vk_t vk;
   test_context_t context;
   test_chain_t chain;
   video_frame_info_t info;
   VkRenderPass pass;
   unsigned dims = VIDEO_SCALE_PACK(64, 32);
   init(&vk, &context, &chain);
   context.flags                  = VK_CTX_FLAG_HDR_ENABLE;
   context.swapchain_format       = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
   vk.views.render_pass           = TEST_RENDER_PASS;
   vk.pipelines.alpha_premult     = (VkPipeline)(uintptr_t)0x78;
   vk.display.effect_tpl[0].valid = true;
   vk.xr.view_format              = VK_FORMAT_B8G8R8A8_UNORM;
   vk.xr.render_pass = pass       = vulkan_views_target_pass(&vk,
         VK_FORMAT_B8G8R8A8_UNORM, true);
   CHECK(pass != VK_NULL_HANDLE && pass != TEST_RENDER_PASS);
   acquire_fails = 0;
   xr_plan(dims, 2);
   eye_dims       = dims;
   vk.views.copied = 3;
   memset(&info, 0, sizeof(info));
   info.views.views[0].width  = info.views.views[1].width  = 64;
   info.views.views[0].height = info.views.views[1].height = 32;
   info.headset_eye[0] = 0;
   info.headset_eye[1] = 1;
   vulkan_xr_draw(&vk, &info, true, true, true, NULL, XR_FRAME_CHAIN,
         NULL, dims, dims, dims, true);
   /* Two screens, the UI and two eyes */
   CHECK(render_passes == 5);
   CHECK(began_pass == pass && mixed_passes == 0);
   CHECK(quads_drawn == 3 && mixed_quads == 0);
   CHECK(quad_pipeline == vulkan_views_target_premult(&vk,
            VK_FORMAT_B8G8R8A8_UNORM));
   CHECK(quad_pipeline != vk.pipelines.alpha_premult);
}

/* A preset that writes HDR draws the headset's screens with its stock
 * chains under HDR output, which its SDR images can't show, and with
 * its own without. */
static void test_xr_hdr_preset_falls_back(void)
{
   vk_t vk;
   test_context_t context;
   test_chain_t chain;
   video_frame_info_t info;
   init(&vk, &context, &chain);
   memset(&info, 0, sizeof(info));
   info.shader_active = true;
   vk.filter_chain    = XR_FRAME_CHAIN;
   strcpy(vk.views.preset, "hdr10.slangp");
   emits_hdr10        = true;
   context.flags            = VK_CTX_FLAG_HDR_ENABLE;
   context.swapchain_format = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
   CHECK(vulkan_xr_stock(&vk, &info));
   context.flags = 0;
   CHECK(!vulkan_xr_stock(&vk, &info));
   /* Under scRGB too, though the window decodes it */
   context.flags = VK_CTX_FLAG_HDR_ENABLE | VK_CTX_FLAG_HDR_SCRGB;
   CHECK(vulkan_xr_stock(&vk, &info));
   /* A preset that writes SDR keeps its own under HDR */
   emits_hdr10   = false;
   CHECK(!vulkan_xr_stock(&vk, &info));
   context.flags = VK_CTX_FLAG_HDR_ENABLE;
   CHECK(!vulkan_xr_stock(&vk, &info));
}

/* Screen windows and the headset take the stock chains for a preset
 * that writes HDR in either encoding under HDR output, though under
 * scRGB the main window draws an HDR10 preset in FP16 and decodes it;
 * the log is armed again only once the preset stops. */
static void test_views_preset_hdr(void)
{
   vk_t vk;
   test_context_t context;
   test_chain_t chain;
   init(&vk, &context, &chain);
   vk.filter_chain = XR_FRAME_CHAIN;
   strcpy(vk.views.preset, "hdr10.slangp");
   emits_hdr10     = true;
   CHECK(!vulkan_views_preset_hdr(&vk));
   context.flags   = VK_CTX_FLAG_HDR_ENABLE;
   CHECK(vulkan_views_preset_hdr(&vk));

   context.flags   = VK_CTX_FLAG_HDR_ENABLE | VK_CTX_FLAG_HDR_SCRGB;
   CHECK(vulkan_views_preset_hdr(&vk));
   vk.views.preset_hdr_logged = true;
   vulkan_views_fp16_update(&vk);
   CHECK(vk.views.fp16 && vk.views.preset_hdr_logged);
   emits_hdr10     = false;
   emits_hdr16     = true;
   CHECK(vulkan_views_preset_hdr(&vk));
   emits_hdr16     = false;
   CHECK(!vulkan_views_preset_hdr(&vk));
   vulkan_views_fp16_update(&vk);
   CHECK(vk.views.fp16 && !vk.views.preset_hdr_logged);

   /* No preset: the stock chains anyway */
   emits_hdr10        = true;
   vk.views.preset[0] = '\0';
   CHECK(!vulkan_views_preset_hdr(&vk));
}

/* The headset's format: the window's without HDR output, its sRGB twin
 * first; with it 8-bit SDR of its own, sRGB first, in a pass of its own
 * format. A new pass retargets the chains, a new format makes the
 * swapchains again, and HDR turned off on the same format picks again. */
static void test_xr_pick_format(void)
{
   vk_t vk;
   test_context_t context;
   test_chain_t chain;
   VkRenderPass pass;
   init(&vk, &context, &chain);
   vk.views.render_pass           = TEST_RENDER_PASS;
   vk.pipelines.alpha_premult     = (VkPipeline)(uintptr_t)0x78;
   vk.display.effect_tpl[0].valid = true;
   runtime_formats[0]             = VK_FORMAT_B8G8R8A8_SRGB;
   runtime_formats[1]             = VK_FORMAT_B8G8R8A8_UNORM;
   runtime_formats[2]             = VK_FORMAT_R8G8B8A8_UNORM;
   runtime_formats[3]             = VK_FORMAT_A2B10G10R10_UNORM_PACK32;

   context.swapchain_format = VK_FORMAT_B8G8R8A8_UNORM;
   CHECK(vulkan_xr_pick_format(&vk));
   CHECK(vk.xr.format == VK_FORMAT_B8G8R8A8_SRGB && vk.xr.mutable_format);
   CHECK(vk.xr.view_format == VK_FORMAT_B8G8R8A8_UNORM);
   CHECK(vk.xr.render_pass == TEST_RENDER_PASS);
   CHECK(xr_retargets == 0 && xr_frees == 0);

   context.flags            = VK_CTX_FLAG_HDR_ENABLE;
   context.swapchain_format = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
   CHECK(vulkan_xr_pick_format(&vk));
   CHECK(vk.xr.format == VK_FORMAT_B8G8R8A8_SRGB && vk.xr.mutable_format);
   CHECK(vk.xr.view_format == VK_FORMAT_B8G8R8A8_UNORM);
   pass = vk.xr.render_pass;
   CHECK(pass != VK_NULL_HANDLE && pass != TEST_RENDER_PASS);
   CHECK(pass_format == VK_FORMAT_B8G8R8A8_UNORM);
   CHECK(xr_retargets == 1 && xr_frees == 0);
   CHECK(vulkan_xr_pick_format(&vk) && xr_retargets == 1);

   /* No sRGB format: the first 8-bit one, as it is */
   runtime_formats[0]   = VK_FORMAT_UNDEFINED;
   runtime_formats[1]   = VK_FORMAT_UNDEFINED;
   vk.xr.window_format  = VK_FORMAT_UNDEFINED;
   CHECK(vulkan_xr_pick_format(&vk));
   CHECK(vk.xr.format == VK_FORMAT_R8G8B8A8_UNORM && !vk.xr.mutable_format);
   CHECK(vk.xr.view_format == VK_FORMAT_R8G8B8A8_UNORM);
   CHECK(vk.xr.render_pass != pass && vk.xr.render_pass != TEST_RENDER_PASS);
   CHECK(xr_retargets == 2 && xr_frees == 1);

   /* HDR off with the same window format */
   context.flags = 0;
   CHECK(vulkan_xr_pick_format(&vk));
   CHECK(vk.xr.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32);
   CHECK(vk.xr.view_format == VK_FORMAT_A2B10G10R10_UNORM_PACK32);
   CHECK(vk.xr.render_pass == TEST_RENDER_PASS);
   CHECK(xr_retargets == 3 && xr_frees == 2);
   vulkan_views_targets_free(&vk);
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
   test_view_chain_target();
   test_view_chain_hdr_semantics();
   test_target_passes();
   test_window_encodes();
   test_window_output_stale();
   test_xr_slots_sdr_under_hdr();
   test_xr_draw_in_xr_pass();
   test_xr_hdr_preset_falls_back();
   test_views_preset_hdr();
   test_xr_pick_format();
   printf("%u failed (20 Vulkan target cases)\n", failures);
   return failures ? 1 : 0;
}
