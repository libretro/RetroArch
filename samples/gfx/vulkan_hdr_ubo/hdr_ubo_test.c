/* Runs the production vulkan_run_hdr_pipeline() against recording
 * stubs.  Each draw's uniform range is read only after every pass is
 * recorded, as the GPU reads it at submit. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <boolean.h>

#define VK_NULL_HANDLE 0
#define VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO 43
#define VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET 35
#define VK_SUBPASS_CONTENTS_INLINE 0
#define VK_PIPELINE_BIND_POINT_GRAPHICS 0
#define VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL 5
#define VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER 1
#define VK_INDEX_TYPE_UINT16 0
#define VULKAN_DIRTY_DYNAMIC_BIT 1
#define VIDEO_SCALE_W(d) (((unsigned)(d) >> 16) & 0xFFFF)
#define VIDEO_SCALE_H(d) ((unsigned)(d) & 0xFFFF)
#define VIDEO_SCALE_PACK(w, h) (((unsigned)(w) << 16) | ((unsigned)(h) & 0xFFFF))
#define RGBA16_WHITE 0xFFFFFFFFUL
#define VULKAN_SET_UNIFORM_BUFFER(dev, set, binding, buffer, offset, range) \
   set_uniform_buffer(dev, set, binding, buffer, offset, range)

typedef unsigned VkPipeline;
typedef unsigned VkRenderPass;
typedef unsigned VkFramebuffer;
typedef unsigned VkImageView;
typedef unsigned VkSampler;
typedef unsigned VkBuffer;
typedef unsigned VkDescriptorSet;
typedef unsigned VkDevice;
typedef unsigned VkCommandBuffer;
typedef unsigned VkPipelineLayout;
typedef size_t VkDeviceSize;

typedef struct { int32_t x, y; } VkOffset2D;
typedef struct { uint32_t width, height; } VkExtent2D;
typedef struct { VkOffset2D offset; VkExtent2D extent; } VkRect2D;
typedef struct { struct { float float32[4]; } color; } VkClearValue;
typedef struct { float x, y, width, height, minDepth, maxDepth; } VkViewport;
typedef struct
{
   int sType;
   const void *pNext;
   VkRenderPass renderPass;
   VkFramebuffer framebuffer;
   VkRect2D renderArea;
   uint32_t clearValueCount;
   const VkClearValue *pClearValues;
} VkRenderPassBeginInfo;
typedef struct
{
   VkSampler sampler;
   VkImageView imageView;
   int imageLayout;
} VkDescriptorImageInfo;
typedef struct
{
   int sType;
   const void *pNext;
   VkDescriptorSet dstSet;
   uint32_t dstBinding, dstArrayElement, descriptorCount;
   int descriptorType;
   const VkDescriptorImageInfo *pImageInfo;
   const void *pBufferInfo;
   const void *pTexelBufferView;
} VkWriteDescriptorSet;

typedef struct { float data[16]; } math_matrix_4x4;
typedef struct
{
   math_matrix_4x4 mvp;
   float paper_white_nits;
   unsigned subpixel_layout;
   float scanlines;
   unsigned expand_gamut;
   float inverse_tonemap;
   float hdr10;
   unsigned hdr_mode;
} vulkan_hdr_uniform_t;

struct vk_image { VkImageView view; VkFramebuffer framebuffer; };
struct vk_buffer_range { uint8_t *data; VkBuffer buffer; VkDeviceSize offset; };
struct vk_buffer_chain { int unused; };
struct context { VkDevice device; unsigned swapchain_dims; };
struct frame_chain
{
   struct vk_buffer_chain vbo, ubo;
   int descriptor_manager;
};
typedef struct
{
   struct context *context;
   struct frame_chain *chain;
   VkCommandBuffer cmd;
   struct
   {
      VkPipeline pipeline;
      unsigned dirty;
      VkImageView view;
      VkSampler sampler;
   } tracker;
   struct { VkSampler nearest; } samplers;
   struct { VkPipelineLayout layout; } pipelines;
   struct { VkBuffer buffer; } quad_ibo;
   math_matrix_4x4 mvp_no_rot;
   struct
   {
      vulkan_hdr_uniform_t ubo_values;
      float menu_nits;
   } hdr;
} vk_t;

struct vk_vertex { float x, y, tex_x, tex_y; unsigned long color; };

#define ARENA_BUFFER 0x100u
#define RANGE_SIZE   256
#define MAX_RANGES   16
#define MAX_SETS     16
#define MAX_DRAWS    16

struct binding { VkBuffer buffer; VkDeviceSize offset, range; };

/* One host arena stands in for the frame's buffer chain. */
static union { double align; uint8_t bytes[RANGE_SIZE * MAX_RANGES]; } arena;
static unsigned ranges_used;
static struct binding sets[MAX_SETS];
static unsigned sets_used;
static VkDescriptorSet bound_set;
static struct binding draws[MAX_DRAWS];
static unsigned num_draws;
/* Bindings 1 and 2, the image samplers, of each set and each draw. */
static VkImageView set_images[MAX_SETS][3];
static VkImageView draw_images[MAX_DRAWS][3];
static bool pass_open;
/* Passes begun and ended, and the allocation the next pass fails */
static unsigned passes_begun, passes_ended;
static bool fail_range, fail_set;
/* The last pass begun, viewport and scissor */
static VkFramebuffer began_framebuffer;
static VkRect2D began_area;
static VkViewport set_viewport;
static VkRect2D set_scissor;
static unsigned failures;

static struct context ctx;
static struct frame_chain chain;
static vk_t vk;
static struct vk_image offscreen, backbuffer, readback_image;

static void vkCmdBeginRenderPass(VkCommandBuffer cmd,
      const VkRenderPassBeginInfo *info, int contents)
{
   (void)cmd; (void)contents;
   pass_open         = true;
   passes_begun++;
   began_framebuffer = info->framebuffer;
   began_area        = info->renderArea;
}

static void vkCmdEndRenderPass(VkCommandBuffer cmd)
{
   (void)cmd;
   pass_open = false;
   passes_ended++;
}

static void vkCmdBindPipeline(VkCommandBuffer cmd, int point, VkPipeline p)
{
   (void)cmd; (void)point; (void)p;
}

static VkDescriptorSet vulkan_descriptor_manager_alloc(VkDevice dev,
      int *manager)
{
   (void)dev; (void)manager;
   if (fail_set || sets_used + 1 >= MAX_SETS)
      return VK_NULL_HANDLE;
   return ++sets_used;
}

static void set_uniform_buffer(VkDevice dev, VkDescriptorSet set,
      unsigned binding, VkBuffer buffer, VkDeviceSize offset,
      VkDeviceSize range)
{
   (void)dev;
   if (binding != 0 || set == VK_NULL_HANDLE || set >= MAX_SETS)
      return;
   sets[set].buffer = buffer;
   sets[set].offset = offset;
   sets[set].range  = range;
}

static void vkUpdateDescriptorSets(VkDevice dev, uint32_t count,
      const VkWriteDescriptorSet *writes, uint32_t copies, const void *p)
{
   uint32_t i;
   (void)dev; (void)copies; (void)p;
   for (i = 0; i < count; i++)
      if (     writes[i].dstSet < MAX_SETS
            && writes[i].dstBinding < 3
            && writes[i].pImageInfo)
         set_images[writes[i].dstSet][writes[i].dstBinding]
            = writes[i].pImageInfo->imageView;
}

static void vkCmdBindDescriptorSets(VkCommandBuffer cmd, int point,
      VkPipelineLayout layout, uint32_t first, uint32_t count,
      const VkDescriptorSet *s, uint32_t dynamic, const void *offsets)
{
   (void)cmd; (void)point; (void)layout; (void)first; (void)count;
   (void)dynamic; (void)offsets;
   bound_set = *s;
}

static void vkCmdSetViewport(VkCommandBuffer cmd, uint32_t first,
      uint32_t count, const VkViewport *vp)
{
   (void)cmd; (void)first; (void)count;
   set_viewport = *vp;
}

static void vkCmdSetScissor(VkCommandBuffer cmd, uint32_t first,
      uint32_t count, const VkRect2D *sci)
{
   (void)cmd; (void)first; (void)count;
   set_scissor = *sci;
}

static bool vulkan_buffer_chain_alloc(const struct context *c,
      struct vk_buffer_chain *bc, size_t size, struct vk_buffer_range *range)
{
   (void)c; (void)bc;
   if (fail_range || size > RANGE_SIZE || ranges_used >= MAX_RANGES)
      return false;
   range->buffer = ARENA_BUFFER;
   range->offset = (VkDeviceSize)ranges_used * RANGE_SIZE;
   range->data   = arena.bytes + range->offset;
   ranges_used++;
   return true;
}

static void vkCmdBindVertexBuffers(VkCommandBuffer cmd, uint32_t first,
      uint32_t count, const VkBuffer *buffers, const VkDeviceSize *offsets)
{
   (void)cmd; (void)first; (void)count; (void)buffers; (void)offsets;
}

static void vkCmdBindIndexBuffer(VkCommandBuffer cmd, VkBuffer buffer,
      VkDeviceSize offset, int type)
{
   (void)cmd; (void)buffer; (void)offset; (void)type;
}

static void vkCmdDrawIndexed(VkCommandBuffer cmd, uint32_t indices,
      uint32_t instances, uint32_t first, int32_t vertex_offset,
      uint32_t first_instance)
{
   (void)cmd; (void)indices; (void)instances; (void)first;
   (void)vertex_offset; (void)first_instance;
   if (pass_open && num_draws < MAX_DRAWS && bound_set < MAX_SETS)
   {
      memcpy(draw_images[num_draws], set_images[bound_set],
            sizeof(draw_images[0]));
      draws[num_draws++] = sets[bound_set];
   }
}

#include "production_hdr.h"

static void setup(void)
{
   unsigned i;
   memset(&arena, 0, sizeof(arena));
   memset(sets, 0, sizeof(sets));
   memset(draws, 0, sizeof(draws));
   memset(set_images, 0, sizeof(set_images));
   memset(draw_images, 0, sizeof(draw_images));
   memset(&vk, 0, sizeof(vk));
   ranges_used = sets_used = num_draws = 0;
   bound_set   = VK_NULL_HANDLE;
   pass_open   = false;
   passes_begun = passes_ended = 0;
   fail_range  = fail_set = false;
   began_framebuffer = VK_NULL_HANDLE;
   memset(&began_area, 0, sizeof(began_area));
   memset(&set_viewport, 0, sizeof(set_viewport));
   memset(&set_scissor, 0, sizeof(set_scissor));

   ctx.device         = 1;
   ctx.swapchain_dims = (640u << 16) | 480u;
   vk.context         = &ctx;
   vk.chain           = &chain;
   vk.cmd             = 2;
   for (i = 0; i < 16; i++)
   {
      vk.mvp_no_rot.data[i]          = (float)i;
      vk.hdr.ubo_values.mvp.data[i]  = -1.0f;
   }
   vk.hdr.ubo_values.paper_white_nits = 203.0f;
   vk.hdr.ubo_values.subpixel_layout  = 1;
   vk.hdr.ubo_values.expand_gamut     = 1;
   vk.hdr.ubo_values.inverse_tonemap  = 1.0f;
   vk.hdr.ubo_values.hdr10            = 1.0f;
   vk.hdr.ubo_values.hdr_mode         = 0;
   vk.hdr.menu_nits                   = 250.0f;

   offscreen.view             = 10;
   offscreen.framebuffer      = 11;
   backbuffer.view            = 20;
   backbuffer.framebuffer     = 21;
   readback_image.view        = 30;
   readback_image.framebuffer = 31;
}

static void expect(const char *test, int ok, const char *what, double got)
{
   if (ok)
      return;
   printf("[FAIL] %s: %s (got %g)\n", test, what, got);
   failures++;
}

/* What the GPU reads for each recorded draw, once the frame is submitted. */
static bool submit(const char *test, unsigned expected,
      vulkan_hdr_uniform_t *out)
{
   unsigned i;
   expect(test, num_draws == expected, "draw count", num_draws);
   if (num_draws != expected)
      return false;
   for (i = 0; i < num_draws; i++)
   {
      expect(test, draws[i].buffer == ARENA_BUFFER,
            "draw binds a range of the frame's buffer chain",
            draws[i].buffer);
      expect(test, draws[i].range >= sizeof(*out),
            "draw's uniform range covers the uniforms", (double)draws[i].range);
      if (draws[i].buffer != ARENA_BUFFER)
         return false;
      memcpy(&out[i], arena.bytes + draws[i].offset, sizeof(*out));
   }
   return true;
}

static void test_two_passes_keep_their_values(void)
{
   static const char t[] = "test_two_passes_keep_their_values";
   vulkan_hdr_uniform_t u[2];
   unsigned before = failures;

   setup();
   /* scRGB game pass decoding a PQ preset, then the SDR readback. */
   vulkan_run_hdr_pipeline(1, 2, &offscreen, backbuffer.framebuffer,
         ctx.swapchain_dims, &vk, 3, false);
   vulkan_run_hdr_pipeline(3, 4, &backbuffer, readback_image.framebuffer,
         ctx.swapchain_dims, &vk, 2, false);
   if (submit(t, 2, u))
   {
      expect(t, u[0].hdr_mode == 3, "first draw's hdr_mode is 3",
            u[0].hdr_mode);
      expect(t, u[0].inverse_tonemap == 0.0f,
            "first draw's inverse_tonemap is 0", u[0].inverse_tonemap);
      expect(t, u[0].hdr10 == 0.0f, "first draw's hdr10 is 0", u[0].hdr10);
      expect(t, !memcmp(&u[0].mvp, &vk.mvp_no_rot, sizeof(u[0].mvp)),
            "first draw's mvp is mvp_no_rot", u[0].mvp.data[1]);
      expect(t, u[1].hdr_mode == 2, "second draw's hdr_mode is 2",
            u[1].hdr_mode);
   }
   if (failures == before)
      printf("[pass] %s\n", t);
}

static void test_menu_nits_only_in_composite(void)
{
   static const char t[] = "test_menu_nits_only_in_composite";
   vulkan_hdr_uniform_t u[2];
   unsigned before = failures;

   setup();
   vulkan_run_hdr_pipeline(1, 2, &offscreen, backbuffer.framebuffer,
         ctx.swapchain_dims, &vk, 1, false);
   vulkan_run_hdr_pipeline(5, 6, &offscreen, backbuffer.framebuffer,
         ctx.swapchain_dims, &vk, 1, true);
   if (submit(t, 2, u))
   {
      expect(t, u[0].paper_white_nits == 203.0f,
            "game draw's paper_white_nits is 203", u[0].paper_white_nits);
      expect(t, u[1].paper_white_nits == 250.0f,
            "composite draw's paper_white_nits is 250 (menu nits)",
            u[1].paper_white_nits);
      expect(t, u[0].hdr10 == 1.0f && u[1].hdr10 == 1.0f,
            "both draws encode PQ", u[0].hdr10);
   }
   if (failures == before)
      printf("[pass] %s\n", t);
}

static void test_values_restored(void)
{
   static const char t[] = "test_values_restored";
   vulkan_hdr_uniform_t base;
   const uint8_t *a = (const uint8_t*)&base;
   const uint8_t *b = (const uint8_t*)&vk.hdr.ubo_values;
   unsigned before = failures;
   size_t i;

   setup();
   base = vk.hdr.ubo_values;
   vulkan_run_hdr_pipeline(1, 2, &offscreen, backbuffer.framebuffer,
         ctx.swapchain_dims, &vk, 3, false);
   vulkan_run_hdr_pipeline(5, 6, &offscreen, backbuffer.framebuffer,
         ctx.swapchain_dims, &vk, 2, true);
   for (i = 0; i < sizeof(base) && a[i] == b[i]; i++) { }
   expect(t, i == sizeof(base),
         "base uniform values unchanged; first changed byte", (double)i);
   expect(t, !pass_open, "render pass ended", pass_open);
   if (failures == before)
      printf("[pass] %s\n", t);
}

/* A slot's sets come back each time the slot comes round, holding what
 * its last frame wrote: here a quad's texture, since retired. A set the
 * pass draws with must name no image but this frame's. */
static void test_recycled_set_names_only_the_source(void)
{
   static const char t[] = "test_recycled_set_names_only_the_source";
   vulkan_hdr_uniform_t u[1];
   unsigned before = failures;
   unsigned b;

   setup();
   set_images[1][1] = 99;
   vulkan_run_hdr_pipeline(1, 2, &offscreen, backbuffer.framebuffer,
         ctx.swapchain_dims, &vk, 1, false);
   if (submit(t, 1, u))
      for (b = 1; b < 3; b++)
         expect(t, draw_images[0][b] == offscreen.view,
               "every image binding of the drawn set names the source",
               draw_images[0][b]);
   if (failures == before)
      printf("[pass] %s\n", t);
}

/* A pass into a screen window's swapchain covers that swapchain, not
 * the main window's */
static void test_dims_set_render_area(void)
{
   static const char t[] = "test_dims_set_render_area";
   unsigned before = failures;

   setup();
   ctx.swapchain_dims = VIDEO_SCALE_PACK(1600, 960);
   vulkan_run_hdr_pipeline(1, 2, &offscreen, backbuffer.framebuffer,
         VIDEO_SCALE_PACK(1240, 1080), &vk, 1, false);
   expect(t, began_framebuffer == backbuffer.framebuffer,
         "pass begins on the framebuffer given", began_framebuffer);
   expect(t, began_area.extent.width == 1240,
         "render area is 1240 wide", began_area.extent.width);
   expect(t, began_area.extent.height == 1080,
         "render area is 1080 high", began_area.extent.height);
   expect(t, set_viewport.width == 1240.0f && set_viewport.height == 1080.0f,
         "viewport is 1240x1080; width", set_viewport.width);
   expect(t, set_scissor.extent.width == 1240
         && set_scissor.extent.height == 1080,
         "scissor is 1240x1080; width", set_scissor.extent.width);
   if (failures == before)
      printf("[pass] %s\n", t);
}

/* Without its uniform range or its set the pass still begins and ends,
 * so the target is cleared and the command buffer has no pass left
 * open, and nothing is drawn */
static void test_failed_allocation_ends_the_pass(void)
{
   static const char t[] = "test_failed_allocation_ends_the_pass";
   unsigned before = failures;
   unsigned which;

   for (which = 0; which < 2; which++)
   {
      setup();
      fail_range = (which == 0);
      fail_set   = (which == 1);
      vulkan_run_hdr_pipeline(1, 2, &offscreen, backbuffer.framebuffer,
            ctx.swapchain_dims, &vk, 1, false);
      expect(t, passes_begun == 1, "the pass begins once", passes_begun);
      expect(t, passes_ended == 1, "the pass ends once", passes_ended);
      expect(t, !pass_open, "no pass left open", pass_open);
      expect(t, began_framebuffer == backbuffer.framebuffer,
            "the pass begins on the framebuffer given", began_framebuffer);
      expect(t, num_draws == 0, "nothing drawn", num_draws);
   }
   if (failures == before)
      printf("[pass] %s\n", t);
}

int main(void)
{
   test_two_passes_keep_their_values();
   test_menu_nits_only_in_composite();
   test_values_restored();
   test_recycled_set_names_only_the_source();
   test_dims_set_render_area();
   test_failed_allocation_ends_the_pass();
   return failures ? 1 : 0;
}
