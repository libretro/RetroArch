/* Runs the production stock view draw (vulkan_views_draw_stock), the
 * HDR pass encode it borrows (vulkan_hdr_ubo_mode) and the decisions
 * that send stock views straight into an HDR swapchain, with no FP16
 * image and HDR game pass between, against a recording fake. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <boolean.h>

#define VULKAN_HDR_SWAPCHAIN 1
#define VKALIGN(x)

typedef uintptr_t VkPipeline;
typedef uintptr_t VkSampler;
typedef uintptr_t VkBuffer;
typedef uintptr_t VkCommandBuffer;
typedef uint64_t VkDeviceSize;
typedef int VkFormat;
typedef int VkColorSpaceKHR;
#define VK_NULL_HANDLE 0
#define VK_INDEX_TYPE_UINT16 0
#define VK_FORMAT_A2B10G10R10_UNORM_PACK32 64
#define VK_FORMAT_R16G16B16A16_SFLOAT 97
#define VK_FORMAT_B8G8R8A8_UNORM 44
#define VK_COLOR_SPACE_SRGB_NONLINEAR_KHR 0
#define VK_COLOR_SPACE_HDR10_ST2084_EXT 1000104008
#define VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT 1000104002
#define VK_CTX_FLAG_HDR_ENABLE (1u << 3)
#define VK_FLAG_SDR_PIPELINE (1u << 5)
#define VK_FLAG_TRACKER_USE_SCISSOR (1u << 6)
#define VULKAN_DIRTY_DYNAMIC_BIT 1u
#define RGBA16_WHITE 0xffffffffffffffffull
#define VIDEO_SCALE_W(d) ((d) & 0xffffu)
#define VIDEO_SCALE_H(d) ((d) >> 16)
#define VIDEO_SCALE_PACK(w, h) ((unsigned)(w) | ((unsigned)(h) << 16))

typedef struct { float data[16]; } math_matrix_4x4;
typedef struct { float x, y, width, height, minDepth, maxDepth; } VkViewport;
typedef struct { struct { int x, y; } offset;
   struct { unsigned width, height; } extent; } VkRect2D;
struct vk_texture { unsigned dims; };
struct retro_video_view { unsigned x, y, width, height; };
struct vk_vertex { float x, y; float tex_x, tex_y; uint64_t color; };
struct vk_buffer_range { uint8_t *data; VkBuffer buffer;
   VkDeviceSize offset; };
struct vk_draw_triangles
{
   const void *uniform;
   const struct vk_buffer_range *vbo;
   struct vk_texture *texture;
   VkPipeline pipeline;
   VkSampler sampler;
   size_t uniform_size;
   unsigned vertices;
};
typedef struct { VkFormat format; VkColorSpaceKHR color_space; }
   vulkan_output_t;

#define VULKAN_WRITE_QUAD_VBO(pv, _x, _y, _w, _h, _tx, _ty, _tw, _th, c) \
{ \
   pv[0].x = (_x); pv[0].y = (_y); pv[0].tex_x = (_tx); \
   pv[0].tex_y = (_ty); pv[0].color = (c); \
   pv[3].x = (_x) + (_w); pv[3].y = (_y) + (_h); \
   pv[3].tex_x = (_tx) + (_tw); pv[3].tex_y = (_ty) + (_th); \
   pv[3].color = (c); \
}

struct vk_context { unsigned flags; VkFormat swapchain_format; };
struct vk_chain { int vbo; };

#include "production_types.h"

typedef struct vk
{
   math_matrix_4x4 mvp;
   VkViewport vk_vp;
   struct { VkRect2D scissor; unsigned dirty; VkPipeline pipeline; }
      tracker;
   struct { VkPipeline stock_rect, stock_rect_sdr, stock_rect_hdr; }
      pipelines;
   struct { VkSampler linear, nearest; } samplers;
   struct { unsigned hdr_stock; bool fp16; } views;
   struct { vulkan_hdr_uniform_t ubo_values; } hdr;
   struct { bool smooth; } video;
   struct { VkBuffer buffer; unsigned num_quads; } quad_ibo;
   struct vk_context context_s;
   struct vk_context *context;
   struct vk_chain chain_s;
   struct vk_chain *chain;
   VkCommandBuffer cmd;
   unsigned flags;
} vk_t;

/* The draw the fake took */
static struct
{
   VkPipeline pipeline;
   VkSampler sampler;
   size_t uniform_size;
   unsigned char uniform[512];
   unsigned draws;
} last;
static struct vk_vertex vbo_mem[4];
static unsigned failures;

static bool vulkan_draw_bind(vk_t *vk, const struct vk_draw_triangles *c)
{
   (void)vk;
   last.pipeline     = c->pipeline;
   last.sampler      = c->sampler;
   last.uniform_size = c->uniform_size;
   memcpy(last.uniform, c->uniform, c->uniform_size);
   return true;
}
static bool vulkan_buffer_chain_alloc(const struct vk_context *ctx,
      int *chain, size_t size, struct vk_buffer_range *range)
{
   (void)ctx; (void)chain;
   if (size > sizeof(vbo_mem))
      return false;
   range->data   = (uint8_t*)vbo_mem;
   range->buffer = 1;
   range->offset = 0;
   return true;
}
static void vkCmdBindVertexBuffers(VkCommandBuffer c, unsigned f, unsigned n,
      const VkBuffer *b, const VkDeviceSize *o)
{ (void)c; (void)f; (void)n; (void)b; (void)o; }
static void vkCmdBindIndexBuffer(VkCommandBuffer c, VkBuffer b,
      VkDeviceSize o, int t)
{ (void)c; (void)b; (void)o; (void)t; }
static void vkCmdDrawIndexed(VkCommandBuffer c, unsigned n, unsigned i,
      unsigned f, int v, unsigned fi)
{ (void)c; (void)n; (void)i; (void)f; (void)v; (void)fi; last.draws++; }
static void vkCmdDraw(VkCommandBuffer c, unsigned n, unsigned i,
      unsigned f, unsigned fi)
{ (void)c; (void)n; (void)i; (void)f; (void)fi; last.draws++; }

#include "production_stock_hdr.h"

static void expect(const char *t, int ok, const char *what, double got)
{
   if (ok)
      return;
   printf("[FAIL] %s: %s (got %g)\n", t, what, got);
   failures++;
}

static vk_t vk;
static struct vk_texture frame;
static struct retro_video_view view;

static void setup(void)
{
   unsigned i;
   memset(&vk, 0, sizeof(vk));
   memset(&last, 0, sizeof(last));
   vk.context                         = &vk.context_s;
   vk.chain                           = &vk.chain_s;
   vk.context->flags                  = VK_CTX_FLAG_HDR_ENABLE;
   vk.context->swapchain_format       = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
   vk.pipelines.stock_rect            = 11;
   vk.pipelines.stock_rect_sdr        = 12;
   vk.pipelines.stock_rect_hdr        = 13;
   vk.samplers.linear                 = 21;
   vk.samplers.nearest                = 22;
   vk.video.smooth                    = true;
   vk.quad_ibo.buffer                 = 1;
   vk.quad_ibo.num_quads              = 1;
   vk.views.fp16                      = true;
   for (i = 0; i < 16; i++)
      vk.mvp.data[i]                  = (float)(i + 1);
   vk.hdr.ubo_values.paper_white_nits = 203.0f;
   vk.hdr.ubo_values.expand_gamut     = 2;
   vk.hdr.ubo_values.subpixel_layout  = 1;
   vk.hdr.ubo_values.inverse_tonemap  = 0.5f;
   vk.hdr.ubo_values.hdr10            = 0.5f;
   vk.hdr.ubo_values.hdr_mode         = 7;
   frame.dims  = VIDEO_SCALE_PACK(400, 480);
   view.x      = 0;
   view.y      = 240;
   view.width  = 320;
   view.height = 240;
}

static void draw(void)
{
   VkViewport vp;
   VkRect2D sci;
   memset(&vp, 0, sizeof(vp));
   memset(&sci, 0, sizeof(sci));
   vulkan_views_draw_stock(&vk, &frame, &view, &vp, &sci);
}

/* The uniform shapes stock_rect_hdr.frag reads: SourceSize, where the
 * clamp goes, right after the MVP, as hdr_common.glsl lays its UBO out */
static void test_uniform_layout(void)
{
   static const char t[] = "test_uniform_layout";
   unsigned before = failures;
   expect(t, offsetof(vulkan_hdr_uniform_t, not_used1) == 64,
         "SourceSize at 64", (double)offsetof(vulkan_hdr_uniform_t, not_used1));
   expect(t, offsetof(vulkan_hdr_uniform_t, paper_white_nits) == 96,
         "BrightnessNits at 96",
         (double)offsetof(vulkan_hdr_uniform_t, paper_white_nits));
   expect(t, offsetof(vulkan_hdr_uniform_t, hdr_mode) == 120,
         "HDRMode at 120", (double)offsetof(vulkan_hdr_uniform_t, hdr_mode));
   if (failures == before)
      printf("[pass] %s\n", t);
}

/* Encoding: the HDR pipeline, the HDR pass's values with the mode's
 * encode, the view's mvp, and the clamp in the source size slot */
static void test_encoded_draw(void)
{
   static const char t[] = "test_encoded_draw";
   unsigned before = failures;
   unsigned mode;
   for (mode = 1; mode <= 2; mode++)
   {
      vulkan_hdr_uniform_t u;
      setup();
      vk.views.hdr_stock = mode + 1;
      draw();
      memcpy(&u, last.uniform, sizeof(u));
      expect(t, last.draws == 1, "one draw", last.draws);
      expect(t, last.pipeline == vk.pipelines.stock_rect_hdr,
            "the HDR pipeline", (double)last.pipeline);
      expect(t, last.sampler == vk.samplers.linear, "the frame's filter",
            (double)last.sampler);
      expect(t, last.uniform_size == sizeof(vulkan_hdr_uniform_t),
            "the HDR uniform", (double)last.uniform_size);
      expect(t, !memcmp(&u.mvp, &vk.mvp, sizeof(u.mvp)), "the views' mvp",
            u.mvp.data[0]);
      expect(t, u.not_used1.width == 0.5f / 400.0f
            && u.not_used1.height == (240.0f + 0.5f) / 480.0f
            && u.not_used1.pad0 == (320.0f - 0.5f) / 400.0f
            && u.not_used1.pad1 == (480.0f - 0.5f) / 480.0f,
            "the first and last texel centres", u.not_used1.width);
      expect(t, u.paper_white_nits == 203.0f && u.expand_gamut == 2
            && u.subpixel_layout == 1, "the HDR pass's values",
            u.paper_white_nits);
      expect(t, u.hdr_mode == mode, "the mode", u.hdr_mode);
      expect(t, (mode == 1)
            ? (u.inverse_tonemap == 1.0f && u.hdr10 == 1.0f)
            : (u.inverse_tonemap == 0.0f && u.hdr10 == 0.0f),
            "the mode's encode", u.inverse_tonemap);
      expect(t, vk.hdr.ubo_values.hdr_mode == 7
            && vk.hdr.ubo_values.inverse_tonemap == 0.5f,
            "the HDR pass's own values untouched",
            vk.hdr.ubo_values.hdr_mode);
   }
   if (failures == before)
      printf("[pass] %s\n", t);
}

/* Not encoding: the plain or FP16 pipeline, with the small uniform */
static void test_plain_draw(void)
{
   static const char t[] = "test_plain_draw";
   unsigned before = failures;
   setup();
   draw();
   expect(t, last.pipeline == vk.pipelines.stock_rect, "stock_rect",
         (double)last.pipeline);
   expect(t, last.uniform_size == sizeof(math_matrix_4x4) + 4 * sizeof(float),
         "mvp and clamp", (double)last.uniform_size);
   setup();
   vk.flags |= VK_FLAG_SDR_PIPELINE;
   draw();
   expect(t, last.pipeline == vk.pipelines.stock_rect_sdr, "stock_rect_sdr",
         (double)last.pipeline);
   if (failures == before)
      printf("[pass] %s\n", t);
}

/* Stock views straight from the frame encode as they draw, unless the
 * blends read the canvas, the views are not in FP16 (no HDR, or a
 * preset's chains write the encoding), or the pipeline is missing */
static void test_main_window_decision(void)
{
   static const char t[] = "test_main_window_decision";
   unsigned before = failures;
   setup();
   expect(t, vulkan_views_stock_encodes(&vk, true, false), "encodes", 0);
   expect(t, !vulkan_views_stock_encodes(&vk, false, false),
         "not through chains", 1);
   expect(t, !vulkan_views_stock_encodes(&vk, true, true),
         "not into the canvas", 2);
   vk.views.fp16 = false;
   expect(t, !vulkan_views_stock_encodes(&vk, true, false),
         "not without FP16 views", 3);
   setup();
   vk.pipelines.stock_rect_hdr = VK_NULL_HANDLE;
   expect(t, !vulkan_views_stock_encodes(&vk, true, false),
         "not without the pipeline", 4);
   if (failures == before)
      printf("[pass] %s\n", t);
}

/* A window takes them straight in only in the main window's encoding;
 * an SDR window keeps its FP16 image and copy */
static void test_window_decision(void)
{
   static const char t[] = "test_window_decision";
   unsigned before = failures;
   vulkan_output_t out;
   setup();
   out.format      = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
   out.color_space = VK_COLOR_SPACE_HDR10_ST2084_EXT;
   expect(t, vulkan_window_stock_encodes(&vk, &out, true), "HDR10", 0);
   expect(t, !vulkan_window_stock_encodes(&vk, &out, false),
         "not through chains", 1);
   out.format      = VK_FORMAT_B8G8R8A8_UNORM;
   out.color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
   expect(t, !vulkan_window_stock_encodes(&vk, &out, true), "not SDR", 2);
   vk.context->swapchain_format = VK_FORMAT_R16G16B16A16_SFLOAT;
   out.format      = VK_FORMAT_R16G16B16A16_SFLOAT;
   out.color_space = VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
   expect(t, vulkan_window_stock_encodes(&vk, &out, true), "scRGB", 3);
   vk.context->flags = 0;
   expect(t, !vulkan_window_stock_encodes(&vk, &out, true),
         "not without HDR output", 4);
   if (failures == before)
      printf("[pass] %s\n", t);
}

int main(void)
{
   test_uniform_layout();
   test_encoded_draw();
   test_plain_draw();
   test_main_window_decision();
   test_window_decision();
   return failures ? 1 : 0;
}
