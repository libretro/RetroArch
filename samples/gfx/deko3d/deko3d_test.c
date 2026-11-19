/* The deko3d video driver, as gfx/drivers/deko3d.c ships, against the
 * deko3d and libnx stand-in headers and a model of the device: memory
 * blocks are host memory, command buffers record what they are asked
 * to do, and the GPU finishes a submission only when the test lets it,
 * so a frame can be held in flight.
 *
 * Lanes: init and teardown leaving nothing behind; the core's frame
 * copied once into a ring slot and read in place from a lent
 * framebuffer, blitted flipped into the viewport, and duped; textures
 * written where they are sampled, updated in place, dropped while a
 * frame in flight samples them, and lent; the menu's quads sampled
 * upright; text from an atlas that keeps its cells for frames in
 * flight; RGUI rendered into a lent slot and into the threaded
 * wrapper's menu memory, sampled from there; one wait a frame, on the
 * next slot; the threaded ring's fence; and the menu's effects - their
 * programs, blends, uniforms and meshes, and the menu drawn after them. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "../../../gfx/drivers/deko3d.c"
#include "../../../gfx/drivers/deko3d_shaders/menu_effects_dksh.h"

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; \
   printf("[FAIL] " __VA_ARGS__); printf("\n"); } } while (0)

void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)  { (void)fmt; }

/* --- the device ------------------------------------------------------ */

struct DkDevice_T { int dummy; };
struct DkMemBlock_T { uint8_t *cpu; uint32_t size; uint32_t flags; unsigned flushes; };
struct DkSwapchain_T { int slot; unsigned images; };
struct DkQueue_T { int dummy; };

enum { OP_DRAW = 1, OP_BLIT, OP_CLEAR, OP_BARRIER, OP_SIGNAL };
typedef struct
{
   int      kind;
   unsigned count;
   DkResHandle handle;
   float    vtx[6 * 8];
   DkImage  src;
   DkImageRect sr, dr;
   uint32_t flags;
   DkFence *fence;
   /* A draw's pipeline: its programs' DKSH, vertex layout, blend and
    * the uniforms each stage reads */
   const void *vs, *fs;
   DkGpuAddr vbo;
   uint32_t stride, attribs, vbo_size;
   DkBlendState blend;
   bool     blend_on;
   float    ubo_v[204], ubo_f[204];
   uint32_t ubo_v_size, ubo_f_size;
} op_t;

#define MAX_OPS 512
struct DkCmdBuf_T { op_t ops[MAX_OPS]; unsigned n; DkGpuAddr vbo; uint32_t vbo_size; };

static struct DkDevice_T the_device;
static struct DkQueue_T  the_queue;
static unsigned blocks_live, blocking_waits;
static uint64_t submitted, completed;
static bool     auto_complete = true;
static unsigned presents;
static op_t     last_draw, last_blit;
static unsigned draws, blits;

DkDevice dkDeviceCreate(DkDeviceMaker const *m)
{
   CHECK(m->flags & DkDeviceFlags_OriginLowerLeft, "device without lower-left origin");
   return &the_device;
}
void dkDeviceDestroy(DkDevice d) { (void)d; }

DkMemBlock dkMemBlockCreate(DkMemBlockMaker const *m)
{
   DkMemBlock b = (DkMemBlock)calloc(1, sizeof(*b));
   if (m->size % DK_MEMBLOCK_ALIGNMENT)
      failures++;
   b->cpu   = (uint8_t*)calloc(1, m->size);
   b->size  = m->size;
   b->flags = m->flags;
   blocks_live++;
   return b;
}
void dkMemBlockDestroy(DkMemBlock b)
{
   if (!b)
      return;
   free(b->cpu);
   free(b);
   blocks_live--;
}
void     *dkMemBlockGetCpuAddr(DkMemBlock b) { return b->cpu; }
DkGpuAddr dkMemBlockGetGpuAddr(DkMemBlock b) { return (DkGpuAddr)(uintptr_t)b->cpu; }
DkResult  dkMemBlockFlushCpuCache(DkMemBlock b, uint32_t off, uint32_t size)
{
   CHECK(off + size <= b->size, "flush past its block");
   CHECK(b->flags & DkMemBlockFlags_CpuCached, "flush of an uncached block");
   b->flushes++;
   return DkResult_Success;
}

DkResult dkFenceWait(DkFence *f, int64_t timeout)
{
   if (!f->valid || f->serial <= completed)
      return DkResult_Success;
   if (!timeout)
      return DkResult_Timeout;
   blocking_waits++;
   completed = f->serial;
   return DkResult_Success;
}

DkCmdBuf dkCmdBufCreate(DkCmdBufMaker const *m) { (void)m; return (DkCmdBuf)calloc(1, sizeof(struct DkCmdBuf_T)); }
void     dkCmdBufDestroy(DkCmdBuf c) { free(c); }
void     dkCmdBufAddMemory(DkCmdBuf c, DkMemBlock m, uint32_t o, uint32_t s) { (void)c; (void)m; (void)o; (void)s; }
DkCmdList dkCmdBufFinishList(DkCmdBuf c) { return (DkCmdList)c; }
void     dkCmdBufClear(DkCmdBuf c) { c->n = 0; }
static op_t *push(DkCmdBuf c, int kind)
{
   op_t *o;
   if (c->n >= MAX_OPS)
   {
      failures++;
      c->n = 0;
   }
   o = &c->ops[c->n++];
   memset(o, 0, sizeof(*o));
   o->kind = kind;
   return o;
}
void dkCmdBufSignalFence(DkCmdBuf c, DkFence *f, bool flush) { (void)flush; push(c, OP_SIGNAL)->fence = f; }
void dkCmdBufBarrier(DkCmdBuf c, DkBarrier m, uint32_t fl) { (void)m; push(c, OP_BARRIER)->flags = fl; }
/* The pipeline state draws are recorded with */
static const void *cur_vs, *cur_fs;
static uint32_t cur_stride, cur_attribs;
static DkBlendState cur_blend;
static bool cur_blend_on;
static DkBufExtents cur_ubo[5];
void dkCmdBufBindShaders(DkCmdBuf c, uint32_t s, DkShader const *const sh[], uint32_t n)
{
   (void)c; (void)s;
   if (n == 2)
   {
      cur_vs = sh[0]->control;
      cur_fs = sh[1]->control;
   }
   else
      failures++;
}
void dkCmdBufBindUniformBuffers(DkCmdBuf c, DkStage s, uint32_t f, DkBufExtents const b[], uint32_t n)
{
   (void)c;
   if (f == 0 && n == 1 && (unsigned)s < 5)
      cur_ubo[s] = b[0];
}
static DkResHandle bound_tex;
void dkCmdBufBindTextures(DkCmdBuf c, DkStage s, uint32_t f, DkResHandle const h[], uint32_t n) { (void)c; (void)s; (void)f; (void)n; bound_tex = h[0]; }
void dkCmdBufBindImageDescriptorSet(DkCmdBuf c, DkGpuAddr a, uint32_t n) { (void)c; (void)a; (void)n; }
void dkCmdBufBindSamplerDescriptorSet(DkCmdBuf c, DkGpuAddr a, uint32_t n) { (void)c; (void)a; (void)n; }
void dkCmdBufBindRenderTargets(DkCmdBuf c, DkImageView const *const t[], uint32_t n, DkImageView const *d) { (void)c; (void)t; (void)n; (void)d; }
void dkCmdBufBindRasterizerState(DkCmdBuf c, DkRasterizerState const *s) { (void)c; (void)s; }
void dkCmdBufBindColorState(DkCmdBuf c, DkColorState const *s) { (void)c; cur_blend_on = (s->blendEnableMask & 1) != 0; }
void dkCmdBufBindColorWriteState(DkCmdBuf c, DkColorWriteState const *s) { (void)c; (void)s; }
void dkCmdBufBindBlendStates(DkCmdBuf c, uint32_t f, DkBlendState const s[], uint32_t n) { (void)c; (void)f; (void)n; cur_blend = s[0]; }
void dkCmdBufBindDepthStencilState(DkCmdBuf c, DkDepthStencilState const *s) { (void)c; (void)s; }
void dkCmdBufBindVtxAttribState(DkCmdBuf c, DkVtxAttribState const a[], uint32_t n) { (void)c; (void)a; cur_attribs = n; }
void dkCmdBufBindVtxBufferState(DkCmdBuf c, DkVtxBufferState const b[], uint32_t n) { (void)c; (void)n; cur_stride = b[0].stride; }
void dkCmdBufBindVtxBuffers(DkCmdBuf c, uint32_t f, DkBufExtents const b[], uint32_t n) { (void)f; (void)n; c->vbo = b[0].addr; c->vbo_size = b[0].size; }
void dkCmdBufSetViewports(DkCmdBuf c, uint32_t f, DkViewport const v[], uint32_t n) { (void)c; (void)f; (void)v; (void)n; }
void dkCmdBufSetScissors(DkCmdBuf c, uint32_t f, DkScissor const s[], uint32_t n) { (void)c; (void)f; (void)s; (void)n; }
void dkCmdBufClearColor(DkCmdBuf c, uint32_t t, uint32_t m, const void *d) { (void)t; (void)m; (void)d; push(c, OP_CLEAR); }
void dkCmdBufDraw(DkCmdBuf c, DkPrimitive p, uint32_t n, uint32_t i, uint32_t fv, uint32_t fi)
{
   op_t *o = push(c, OP_DRAW);
   (void)p; (void)i; (void)fv; (void)fi;
   o->count  = n;
   o->handle = bound_tex;
   memcpy(o->vtx, (const void*)(uintptr_t)c->vbo,
         MIN(sizeof(float) * 8 * (n < 6 ? n : 6), c->vbo_size));
   o->vs       = cur_vs;
   o->fs       = cur_fs;
   o->vbo      = c->vbo;
   o->vbo_size = c->vbo_size;
   o->stride   = cur_stride;
   o->attribs  = cur_attribs;
   o->blend    = cur_blend;
   o->blend_on = cur_blend_on;
   o->ubo_v_size = cur_ubo[DkStage_Vertex].size;
   o->ubo_f_size = cur_ubo[DkStage_Fragment].size;
   if (cur_ubo[DkStage_Vertex].size >= sizeof(o->ubo_v))
      memcpy(o->ubo_v, (const void*)(uintptr_t)cur_ubo[DkStage_Vertex].addr,
            sizeof(o->ubo_v));
   if (cur_ubo[DkStage_Fragment].size >= sizeof(o->ubo_f))
      memcpy(o->ubo_f, (const void*)(uintptr_t)cur_ubo[DkStage_Fragment].addr,
            sizeof(o->ubo_f));
   last_draw = *o;
   draws++;
}
/* Into the buffer, in order with the draws: the model runs them as
 * they are recorded */
void dkCmdBufPushConstants(DkCmdBuf c, DkGpuAddr a, uint32_t s, uint32_t o, uint32_t n, const void *d)
{
   (void)c;
   if (o + n > s)
      failures++;
   else
      memcpy((uint8_t*)(uintptr_t)a + o, d, n);
}
void dkCmdBufBlitImage(DkCmdBuf c, DkImageView const *sv, DkImageRect const *sr,
      DkImageView const *dv, DkImageRect const *dr, uint32_t flags, uint32_t factor)
{
   op_t *o = push(c, OP_BLIT);
   (void)dv; (void)factor;
   o->src   = *sv->pImage;
   o->sr    = *sr;
   o->dr    = *dr;
   o->flags = flags;
   last_blit = *o;
   blits++;
}

DkQueue dkQueueCreate(DkQueueMaker const *m) { (void)m; return &the_queue; }
void    dkQueueDestroy(DkQueue q) { (void)q; }
void    dkQueueWaitFence(DkQueue q, DkFence *f) { (void)q; (void)f; }
void    dkQueueSubmitCommands(DkQueue q, DkCmdList l)
{
   DkCmdBuf c = (DkCmdBuf)l;
   unsigned i;
   (void)q;
   for (i = 0; i < c->n; i++)
      if (c->ops[i].kind == OP_SIGNAL)
      {
         c->ops[i].fence->serial = ++submitted;
         c->ops[i].fence->valid  = 1;
      }
   if (auto_complete)
      completed = submitted;
}
void dkQueueWaitIdle(DkQueue q) { (void)q; completed = submitted; }
int  dkQueueAcquireImage(DkQueue q, DkSwapchain s) { (void)q; s->slot = (s->slot + 1) % (int)s->images; return s->slot; }
void dkQueuePresentImage(DkQueue q, DkSwapchain s, int slot) { (void)q; (void)s; (void)slot; presents++; }

void dkShaderInitialize(DkShader *s, DkShaderMaker const *m) { s->control = m->control; s->code = m->codeMem; s->offset = m->codeOffset; }
void dkImageLayoutInitialize(DkImageLayout *l, DkImageLayoutMaker const *m)
{
   l->maker = *m;
   if ((m->flags & DkImageFlags_PitchLinear) && (m->pitchStride % DK_IMAGE_LINEAR_STRIDE_ALIGNMENT))
      failures++;
}
uint64_t dkImageLayoutGetSize(DkImageLayout const *l) { return (uint64_t)l->maker.dimensions[0] * l->maker.dimensions[1] * 4; }
void dkImageInitialize(DkImage *i, DkImageLayout const *l, DkMemBlock m, uint32_t off)
{
   i->layout = *l;
   i->mem    = m;
   i->offset = off;
   if ((l->maker.flags & DkImageFlags_PitchLinear) && (off % DK_IMAGE_LINEAR_STRIDE_ALIGNMENT))
      failures++;
}
void dkImageDescriptorInitialize(DkImageDescriptor *d, DkImageView const *v, bool a, bool b)
{
   (void)a; (void)b;
   d->image = *v->pImage;
   memcpy(d->swizzle, v->swizzle, sizeof(d->swizzle));
}
void dkSamplerDescriptorInitialize(DkSamplerDescriptor *d, DkSampler const *s) { d->sampler = *s; }

DkSwapchain dkSwapchainCreate(DkSwapchainMaker const *m)
{
   DkSwapchain s = (DkSwapchain)calloc(1, sizeof(*s));
   s->images     = m->numImages;
   s->slot       = -1;
   return s;
}
void dkSwapchainDestroy(DkSwapchain s) { free(s); }
void dkSwapchainSetSwapInterval(DkSwapchain s, uint32_t i) { (void)s; (void)i; }

/* --- the console and the frontend ------------------------------------- */

NWindow *nwindowGetDefault(void) { return (NWindow*)&the_device; }
Result nwindowSetDimensions(NWindow *nw, u32 w, u32 h) { (void)nw; (void)w; (void)h; return 0; }
AppletOperationMode appletGetOperationMode(void) { return AppletOperationMode_Handheld; }

static settings_t the_settings;
settings_t *config_get_ptr(void) { return &the_settings; }
static struct retro_hw_render_callback hwr;
struct retro_hw_render_callback *video_driver_get_hw_context(void) { return &hwr; }
bool video_context_driver_set(const gfx_ctx_driver_t *d) { (void)d; return true; }
void input_driver_video_window(enum input_window_kind w, void *d) { (void)w; (void)d; }
void video_driver_set_output_dims(unsigned dims) { (void)dims; }
void video_driver_cached_frame_retire(void) { }
void video_driver_update_viewport(struct video_viewport *vp, bool f, bool k, bool y)
{
   (void)f; (void)k; (void)y;
   vp->pos  = VIDEO_POS_PACK(0, 0);
   vp->dims = vp->full_dims;
}
static void (*menu_draws)(dk3d_t *dk);
static dk3d_t *cur;
void menu_driver_frame(bool alive, video_frame_info_t *vi) { (void)alive; (void)vi; if (menu_draws) menu_draws(cur); }

/* A font of one 4x4 glyph at (8, 0) of a 16x8 atlas */
static uint8_t atlas_px[16 * 8];
static struct font_atlas atlas;
static struct font_glyph glyph;
static struct font_line_metrics metrics = { 8.0f, 6.0f, 2.0f };
static void *fake_init(uint8_t *d, size_t l, unsigned f, float s,
      enum font_atlas_format fmt) { (void)d; (void)l; (void)f; (void)s; (void)fmt; return &atlas; }
static struct font_atlas *fake_atlas(void *d) { (void)d; return &atlas; }
static const struct font_glyph *fake_glyph(void *d, uint32_t c) { (void)d; (void)c; return &glyph; }
static void fake_free(void *d) { (void)d; }
static void fake_metrics(void *d, struct font_line_metrics **m) { (void)d; *m = &metrics; }
static const font_renderer_driver_t fake_font = {
   fake_init, fake_atlas, fake_glyph, fake_free, NULL, "fake", fake_metrics, true
};
int font_renderer_create_default(const font_renderer_driver_t **drv,
      void **handle, const char *path, unsigned size, enum font_atlas_format fmt)
{
   unsigned x, y;
   (void)path; (void)size; (void)fmt;
   memset(atlas_px, 0, sizeof(atlas_px));
   for (y = 0; y < 4; y++)
      for (x = 0; x < 4; x++)
         atlas_px[y * 16 + 8 + x] = 200;
   memset(&atlas, 0, sizeof(atlas));
   atlas.buffer    = atlas_px;
   atlas.width     = 16;
   atlas.height    = 8;
   atlas.dirty     = true;
   atlas.dirty_xy0 = VIDEO_SCALE_PACK(0, 0);
   atlas.dirty_xy1 = VIDEO_SCALE_PACK(16, 8);
   glyph.dims          = VIDEO_SCALE_PACK(4, 4);
   glyph.atlas_pos     = VIDEO_SCALE_PACK(8, 0);
   glyph.draw_offset_y = -4;
   glyph.advance_x     = 5;
   *drv    = &fake_font;
   *handle = &atlas;
   return 1;
}
uint32_t utf8_walk(const char **s) { return (uint32_t)(unsigned char)*(*s)++; }
static font_params_resolved_t next_rp;
void font_driver_resolve_params(const struct font_params *p, font_params_resolved_t *o) { (void)p; *o = next_rp; }
static void *the_font;
void font_driver_render_msg(void *data, const char *msg, size_t len,
      const struct font_params *params, void *font_data)
{
   (void)font_data;
   dk3d_font_render_msg(data, the_font, msg, len, params);
}

/* --- helpers ------------------------------------------------------------ */

static video_frame_info_t vfi;
static const video_poke_interface_t *poke;
static uint8_t core_frame[320 * 240 * 4];

static dk3d_t *driver_new(unsigned w, unsigned h, bool rgb32)
{
   video_info_t info;
   dk3d_t *dk;
   memset(&info, 0, sizeof(info));
   info.dims  = VIDEO_SCALE_PACK(w, h);
   info.rgb32 = rgb32;
   info.vsync = true;
   the_settings.uints.video_max_swapchain_images = 3;
   dk = (dk3d_t*)video_deko3d.init(&info);
   video_deko3d.poke_interface(dk, &poke);
   cur      = dk;
   the_font = dk3d_font_init(dk, NULL, 16.0f, false);
   return dk;
}

static void driver_free(dk3d_t *dk)
{
   dk3d_font_free(the_font, false);
   the_font = NULL;
   video_deko3d.free(dk);
}

static void frame(dk3d_t *dk, const void *data, unsigned w, unsigned h,
      unsigned pitch)
{
   video_deko3d.frame(dk, data, data ? VIDEO_SCALE_PACK(w, h) : 0, 1,
         pitch, NULL, &vfi);
}

/* The texel the blit source holds at (x, y), in its own format */
static uint32_t src_texel(const DkImage *img, unsigned x, unsigned y)
{
   const uint8_t *p = img->mem->cpu + img->offset
      + (size_t)y * img->layout.maker.pitchStride;
   return img->layout.maker.format == DkImageFormat_BGR565_Unorm
      ? ((const uint16_t*)p)[x] : ((const uint32_t*)p)[x];
}

/* --- lanes --------------------------------------------------------------- */

static void lane_init(void)
{
   dk3d_t *dk = driver_new(256, 224, true);
   CHECK(dk && dk->num_frames == 2 && dk->num_swapchain == 3,
         "%u frames over %u images", dk ? dk->num_frames : 0,
         dk ? dk->num_swapchain : 0);
   CHECK(dk->sc_images[0].layout.maker.dimensions[0] == 1280
         && (dk->sc_images[0].layout.maker.flags & DkImageFlags_UsagePresent)
         && (dk->sc_images[0].layout.maker.flags & DkImageFlags_Usage2DEngine),
         "swapchain images");
   /* The generated DKSH: control sections kept, code where the shaders
    * name it */
   CHECK(dk->vsh.control == deko3d_menu_vsh_dksh
         && dk->fsh.control == deko3d_menu_fsh_dksh
         && !memcmp(dk->shader_mem->cpu + dk->vsh.offset,
               deko3d_menu_vsh_dksh + 256, 512)
         && !memcmp(dk->shader_mem->cpu + dk->fsh.offset,
               deko3d_menu_fsh_dksh + 256, 512)
         && dk->fsh.offset + 512 + DK_SHADER_CODE_UNUSABLE_SIZE
            <= dk->shader_mem->size,
         "the menu shaders not loaded from the generated DKSH");
   driver_free(dk);
   CHECK(!blocks_live, "%u memory blocks left at free", blocks_live);
   printf("[pass] init and teardown\n");
}

static void lane_frame(void)
{
   struct retro_framebuffer fb;
   unsigned x, y;
   dk3d_t *dk = driver_new(256, 224, true);
   for (y = 0; y < 224; y++)
      for (x = 0; x < 256; x++)
         ((uint32_t*)core_frame)[y * 256 + x] = (x << 16) | (y << 8) | 7;
   frame(dk, core_frame, 256, 224, 1024);
   CHECK(blits == 1 && last_blit.src.layout.maker.format == DkImageFormat_BGRX8_Unorm
         && (last_blit.src.layout.maker.flags & DkImageFlags_PitchLinear)
         && (last_blit.flags & DkBlitFlag_FlipY),
         "the frame's blit: format %d, flags %x", last_blit.src.layout.maker.format,
         last_blit.flags);
   CHECK(src_texel(&last_blit.src, 100, 50) == ((100u << 16) | (50u << 8) | 7),
         "the ring slot holds %08x", src_texel(&last_blit.src, 100, 50));
   CHECK(last_blit.dr.width == 1280 && last_blit.dr.height == 720,
         "blitted into %ux%u", last_blit.dr.width, last_blit.dr.height);
   /* A dupe blits the same source again */
   frame(dk, NULL, 0, 0, 0);
   CHECK(blits == 2 && src_texel(&last_blit.src, 100, 50)
         == ((100u << 16) | (50u << 8) | 7), "a dupe lost the frame");
   /* A lent framebuffer: the blit reads it where the core wrote it */
   memset(&fb, 0, sizeof(fb));
   fb.width  = 256;
   fb.height = 224;
   CHECK(poke->get_current_software_framebuffer(dk, &fb), "nothing lent");
   CHECK(fb.memory_flags == RETRO_MEMORY_TYPE_CACHED && !(fb.pitch % 32),
         "lent with pitch %u", (unsigned)fb.pitch);
   ((uint32_t*)fb.data)[0] = 0x00abcdefu;
   frame(dk, fb.data, 256, 224, fb.pitch);
   CHECK(last_blit.src.mem->cpu + last_blit.src.offset == (uint8_t*)fb.data,
         "the lent framebuffer was copied");
   driver_free(dk);
   blits = 0;
   printf("[pass] the core's frame: copied once, lent, duped, flipped into the viewport\n");
}

static void lane_textures(void)
{
   struct texture_image img, upd;
   uint32_t px[8 * 4], px2[8 * 4];
   unsigned i, blocks;
   size_t pitch = 0;
   dk3d_tex_t *t;
   uintptr_t id;
   void *lent;
   dk3d_t *dk = driver_new(256, 224, true);
   for (i = 0; i < 32; i++)
   {
      px[i]  = 0xff000000u | i;
      px2[i] = 0x80000000u | i;
   }
   memset(&img, 0, sizeof(img));
   img.pixels = px;
   img.width  = 8;
   img.height = 4;
   blocks     = blocks_live;
   id         = poke->load_texture(dk, &img, false, TEXTURE_FILTER_LINEAR);
   t          = (dk3d_tex_t*)id;
   CHECK(t && t->pitch == 32 && ((uint32_t*)(t->cpu + 3 * t->pitch))[5] == px[29],
         "texture not written where it is sampled");
   CHECK(dk->desc_cpu[t->desc].image.layout.maker.flags & DkImageFlags_PitchLinear,
         "texture's descriptor not the pitch-linear image");
   CHECK(blocks_live <= blocks + 1, "a texture load took %u blocks",
         blocks_live - blocks);

   /* Drawn by a frame the GPU has not finished: an update is dropped */
   auto_complete = false;
   menu_draws    = NULL;
   upd           = img;
   upd.pixels    = px2;
   t->use_serial = dk->serial + 1;
   frame(dk, NULL, 0, 0, 0);
   CHECK(poke->update_texture(dk, id, &upd, false) == VIDEO_TEXTURE_UPDATE_DROPPED,
         "updated under a frame in flight");
   completed     = submitted;
   auto_complete = true;
   CHECK(poke->update_texture(dk, id, &upd, false) == VIDEO_TEXTURE_UPDATE_DONE
         && ((uint32_t*)t->cpu)[0] == px2[0], "not updated in place once idle");

   /* Lent: the producer writes the texture's memory; nothing else does */
   CHECK(poke->texture_lend_ready(dk, id, 0), "lend never ready");
   lent = poke->texture_lend(dk, id, 0, 32, &pitch);
   CHECK(lent == t->cpu && pitch == t->pitch, "lent %p, not the texture", lent);
   CHECK(poke->update_texture(dk, id, &img, false) == VIDEO_TEXTURE_UPDATE_DROPPED,
         "an update from elsewhere wrote the lent slot");
   upd.pixels = lent;
   CHECK(poke->update_texture(dk, id, &upd, false) == VIDEO_TEXTURE_UPDATE_DONE,
         "an update from the lent slot refused");
   poke->unload_texture(dk, false, id);
   driver_free(dk);
   CHECK(!blocks_live, "%u blocks left", blocks_live);
   printf("[pass] textures: written in place, updated, dropped in flight, lent\n");
}

static uintptr_t quad_tex;
static void draws_quad(dk3d_t *dk)
{
   gfx_display_ctx_draw_t d;
   struct video_coords c;
   memset(&d, 0, sizeof(d));
   memset(&c, 0, sizeof(c));
   c.vertices = 4;
   d.coords   = &c;
   d.texture  = quad_tex;
   d.dims     = VIDEO_SCALE_PACK(64, 64);
   d.pos      = VIDEO_POS_PACK(10, 10);
   gfx_display_ctx_deko3d.draw(&d, dk, VIDEO_SCALE_PACK(1280, 720));
}

static void lane_menu_quad(void)
{
   struct texture_image img;
   uint32_t px[4] = { 1, 2, 3, 4 };
   dk3d_t *dk = driver_new(256, 224, true);
   memset(&img, 0, sizeof(img));
   img.pixels = px;
   img.width  = 2;
   img.height = 2;
   quad_tex   = poke->load_texture(dk, &img, false, TEXTURE_FILTER_LINEAR);
   vfi.menu_st_flags = MENU_ST_FLAG_ALIVE;
   menu_draws        = draws_quad;
   frame(dk, NULL, 0, 0, 0);
   menu_draws        = NULL;
   vfi.menu_st_flags = 0;
   /* Vertices BL BR TL TR, (x, y, u, v, ...): the top samples row 0 */
   CHECK(last_draw.count == 4 && last_draw.vtx[2 * 8 + 1] == 1.0f
         && last_draw.vtx[2 * 8 + 3] == 0.0f && last_draw.vtx[0 * 8 + 3] == 1.0f,
         "the quad's top samples v %g, its bottom v %g",
         last_draw.vtx[2 * 8 + 3], last_draw.vtx[0 * 8 + 3]);
   CHECK((last_draw.handle & 0xFFFFF) == ((dk3d_tex_t*)quad_tex)->desc,
         "drawn with descriptor %u", last_draw.handle & 0xFFFFF);
   poke->unload_texture(dk, false, quad_tex);
   driver_free(dk);
   printf("[pass] the menu's quads sample textures upright\n");
}

static void lane_text(void)
{
   dk3d_t *dk = driver_new(256, 224, true);
   dk3d_font_t *f = (dk3d_font_t*)the_font;
   memset(&next_rp, 0, sizeof(next_rp));
   next_rp.x        = 0.1f;
   next_rp.y        = 0.5f;
   next_rp.scale    = 1.0f;
   next_rp.color[0] = next_rp.color[1] = next_rp.color[2] = next_rp.color[3] = 1.0f;
   next_rp.text_align = TEXT_ALIGN_LEFT;
   CHECK(atlas.inflight == dk->num_frames,
         "the atlas keeps cells for %u frames, not %u", atlas.inflight,
         dk->num_frames);
   video_deko3d.frame(dk, NULL, 0, 1, 0, "A", &vfi);
   CHECK(f->tex && f->tex->cpu[2 * f->tex->pitch + 9] == 200,
         "the glyph's cell not in the sampled image");
   CHECK(last_draw.count == 6, "text drawn as %u vertices", last_draw.count);
   driver_free(dk);
   printf("[pass] text: cells written where sampled, kept for frames in flight\n");
}

static void lane_rgui(void)
{
   uint16_t *menu;
   size_t pitch = 0;
   unsigned blocks, s;
   uintptr_t id;
   const DkImageDescriptor *d;
   dk3d_t *dk = driver_new(320, 240, true);

   /* Rendered into a lent ring slot: sampled there, nothing copied */
   menu = (uint16_t*)poke->get_menu_framebuffer(dk, false,
         VIDEO_SCALE_PACK(320, 240), &pitch);
   CHECK(menu && pitch == 640, "no menu loan (pitch %u)", (unsigned)pitch);
   s      = (unsigned)dk->rgui_lent;
   menu[0] = 0xf00f;
   blocks = blocks_live;
   poke->set_texture_frame(dk, menu, false, VIDEO_SCALE_PACK(320, 240), 1.0f);
   d = &dk->desc_cpu[DK3D_DESC_RGUI + s];
   CHECK(d->image.mem->cpu + d->image.offset == (uint8_t*)menu
         && d->image.layout.maker.format == DkImageFormat_RGBA4_Unorm
         && d->swizzle[0] == DkImageSwizzle_Alpha && d->swizzle[3] == DkImageSwizzle_Red,
         "RGUI's lent frame not sampled where it was rendered");
   CHECK(blocks_live == blocks, "set_texture_frame allocated");

   /* The threaded wrapper's menu memory: the same */
   menu = (uint16_t*)poke->menu_mem_alloc(dk, 640 * 240, &id);
   CHECK(menu && poke->menu_mem_pitch_align == 32, "no menu memory");
   poke->set_texture_frame(dk, menu, false, VIDEO_SCALE_PACK(320, 240), 1.0f);
   d = &dk->desc_cpu[DK3D_DESC_RGUI + dk->rgui_last];
   CHECK(d->image.mem == dk->frame_mem[id - 1].mem && d->image.offset == 0,
         "menu memory copied");
   poke->set_texture_enable(dk, true, false);
   auto_complete = false;
   frame(dk, core_frame, 320, 240, 1280);
   CHECK(!poke->frame_mem_ready(dk, id), "menu memory ready while drawn in flight");
   completed     = submitted;
   auto_complete = true;
   poke->set_texture_enable(dk, false, false);
   frame(dk, core_frame, 320, 240, 1280);
   CHECK(poke->frame_mem_ready(dk, id), "menu memory never ready");
   poke->frame_mem_free(dk, id);
   CHECK(dk->rgui_last == -1, "RGUI still names freed menu memory");
   driver_free(dk);
   CHECK(!blocks_live, "%u blocks left", blocks_live);
   printf("[pass] RGUI sampled where it rendered: a lent slot, menu memory\n");
}

static void lane_sync(void)
{
   unsigned i, w0;
   DkFence f, *pf = NULL;
   dk3d_t *dk = driver_new(256, 224, true);
   auto_complete = false;
   w0 = blocking_waits;
   for (i = 0; i < 10; i++)
      frame(dk, core_frame, 256, 224, 1024);
   CHECK(blocking_waits - w0 <= 10, "%u waits in 10 frames", blocking_waits - w0);
   CHECK(submitted - completed <= dk->num_frames,
         "%u frames in flight over %u slots", (unsigned)(submitted - completed),
         dk->num_frames);
   auto_complete = true;
   completed     = submitted;
   poke->hw_ring_fence_new(dk, (void**)&pf);
   poke->hw_ring_fence_signal(dk, pf);
   f = *pf;
   CHECK(f.valid && f.serial == dk->last_done.serial,
         "the ring's fence is not the last frame's");
   poke->hw_ring_fence_free(dk, pf);
   driver_free(dk);
   printf("[pass] one wait a frame, frames in flight bounded; the ring's fence\n");
}

/* --- the menu's effects ------------------------------------------------ */

/* The ribbon's uniforms, a pattern; the y flip the driver asks for */
static float ribbon_pat[204];
static float ribbon_yflip_asked;
const float *gfx_display_ribbon_ubo(gfx_display_t *p_disp, float yflip)
{
   unsigned i;
   (void)p_disp;
   for (i = 0; i < 204; i++)
      ribbon_pat[i] = (float)i * 0.5f;
   ribbon_pat[1]      = yflip;
   ribbon_yflip_asked = yflip;
   return ribbon_pat;
}

static gfx_display_effect_desc_t fxd[6];
static void *fxh[6];
static gfx_display_t fx_disp;
static gfx_display_mesh_t fx_ribbon, fx_quad;
static float fx_ribbon_pos[24];
static float fx_quad_pos[8] = { -1, -1, 1, -1, -1, 1, 1, 1 };
static float fx_color[16];
static op_t fx_ops[8];
static unsigned fx_nops;

static void draws_fx(dk3d_t *dk)
{
   gfx_display_ctx_draw_t d;
   struct video_coords c;
   unsigned e, d0;
   memset(&c, 0, sizeof(c));
   memset(&d, 0, sizeof(d));
   c.vertices     = 4;
   c.color        = fx_color;
   d.coords       = &c;
   d.scale_factor = 1.0f;
   d.color        = fx_color;
   d.dims         = VIDEO_SCALE_PACK(1280, 720);
   fx_nops        = 0;
   for (e = 0; e < 6; e++)
   {
      fx_disp.effect_mesh     = e < 2 ? &fx_ribbon : &fx_quad;
      fx_disp.effect_vertices = e < 2 ? 10 : 4;
      d.effect                = fxh[e];
      d0                      = draws;
      gfx_display_ctx_deko3d.draw_pipeline(&d, &fx_disp, dk,
            VIDEO_SCALE_PACK(1280, 720));
      /* and the draw after it, which an effect leaves to the pipeline */
      gfx_display_ctx_deko3d.draw(&d, dk, VIDEO_SCALE_PACK(1280, 720));
      if (draws - d0 == 1)
         fx_ops[fx_nops++] = last_draw;
   }
   /* a menu quad after them */
   d.effect = NULL;
   d.pos    = VIDEO_POS_PACK(10, 10);
   d.dims   = VIDEO_SCALE_PACK(16, 16);
   gfx_display_ctx_deko3d.draw(&d, dk, VIDEO_SCALE_PACK(1280, 720));
   fx_ops[fx_nops++] = last_draw;
}

static void lane_effects(void)
{
#define FXD(e, f, v, fr) \
   fxd[e].flags = f; \
   fxd[e].dksh_vertex = v; fxd[e].dksh_vertex_size = sizeof(v); \
   fxd[e].dksh_fragment = fr; fxd[e].dksh_fragment_size = sizeof(fr)
   static const DkBlendFactor blend[6][4] = {
      { DkBlendFactor_One, DkBlendFactor_One, DkBlendFactor_One, DkBlendFactor_One },
      { DkBlendFactor_DstColor, DkBlendFactor_One, DkBlendFactor_Zero, DkBlendFactor_One },
      { DkBlendFactor_SrcAlpha, DkBlendFactor_InvSrcAlpha, DkBlendFactor_One, DkBlendFactor_InvSrcAlpha },
      { DkBlendFactor_SrcAlpha, DkBlendFactor_InvSrcAlpha, DkBlendFactor_One, DkBlendFactor_InvSrcAlpha },
      { DkBlendFactor_SrcAlpha, DkBlendFactor_InvSrcAlpha, DkBlendFactor_One, DkBlendFactor_InvSrcAlpha },
      { DkBlendFactor_SrcAlpha, DkBlendFactor_InvSrcAlpha, DkBlendFactor_One, DkBlendFactor_InvSrcAlpha }
   };
   unsigned e, i;
   DkGpuAddr ribbon_vbo;
   dk3d_t *dk = driver_new(1280, 720, true);
   FXD(0, GFX_EFFECT_MESH | GFX_EFFECT_BLEND_ADD | GFX_EFFECT_RIBBON,
         deko3d_fx_ribbon_vert_dksh, deko3d_fx_ribbon_frag_dksh);
   FXD(1, GFX_EFFECT_MESH | GFX_EFFECT_BLEND_SCREEN,
         deko3d_fx_ribbon_simple_vert_dksh, deko3d_fx_ribbon_simple_frag_dksh);
   FXD(2, 0, deko3d_fx_quad_vert_dksh, deko3d_fx_snow_simple_frag_dksh);
   FXD(3, 0, deko3d_fx_quad_vert_dksh, deko3d_fx_snow_frag_dksh);
   FXD(4, GFX_EFFECT_YFLIP, deko3d_fx_quad_vert_dksh, deko3d_fx_bokeh_frag_dksh);
   FXD(5, GFX_EFFECT_YFLIP, deko3d_fx_quad_vert_dksh,
         deko3d_fx_snowflake_frag_dksh);
#undef FXD
   for (i = 0; i < 12; i++)
   {
      fx_ribbon_pos[2 * i]     = (float)i * 0.1f;
      fx_ribbon_pos[2 * i + 1] = 1.0f - (float)i * 0.05f;
   }
   fx_ribbon.positions    = fx_ribbon_pos;
   fx_ribbon.vertex_count = 12;
   fx_ribbon.id           = 77;
   fx_quad.positions      = fx_quad_pos;
   fx_quad.vertex_count   = 4;
   fx_quad.id             = 1;
   fx_disp.effect_time    = 1.5f;
   for (i = 0; i < 16; i++)
      fx_color[i] = (i & 3) == 3 ? 0.75f : 1.0f;
   for (e = 0; e < 6; e++)
   {
      fxh[e] = gfx_display_ctx_deko3d.effect_install(dk, &fxd[e]);
      CHECK(fxh[e], "effect %u not installed", e);
   }
   /* the programs' code in the effects' block, where the shaders say */
   for (e = 0; e < 6 && fxh[e]; e++)
   {
      const dk3d_fx_t *fx = (const dk3d_fx_t*)fxh[e];
      CHECK(fx->vsh.control == fxd[e].dksh_vertex
            && fx->fsh.control == fxd[e].dksh_fragment
            && !memcmp(dk->fx_mem->cpu + fx->fsh.offset,
               fxd[e].dksh_fragment + dk3d_le32(fxd[e].dksh_fragment + 8),
               dk3d_le32(fxd[e].dksh_fragment + 12))
            && fx->fsh.offset % DK_SHADER_CODE_ALIGNMENT == 0,
            "effect %u's programs", e);
   }
   CHECK(dk->fx_used + DK_SHADER_CODE_UNUSABLE_SIZE <= dk->fx_mem->size,
         "the effects' code reaches the unusable tail");
   vfi.menu_st_flags = MENU_ST_FLAG_ALIVE;
   menu_draws        = draws_fx;
   frame(dk, NULL, 0, 0, 0);
   CHECK(fx_nops == 7, "%u draws, not 6 effects and a quad", fx_nops);
   for (e = 0; e < 6 && fx_nops == 7; e++)
   {
      const op_t *o = &fx_ops[e];
      unsigned n    = e < 2 ? 10 : 4;
      const float *pos = e < 2 ? fx_ribbon_pos : fx_quad_pos;
      CHECK(o->vs == fxd[e].dksh_vertex && o->fs == fxd[e].dksh_fragment,
            "effect %u drawn with other programs", e);
      CHECK(o->stride == 8 && o->attribs == 1 && o->count == n
            && o->vbo_size == n * 8
            && !memcmp((const void*)(uintptr_t)o->vbo, pos, n * 8),
            "effect %u: vertices", e);
      CHECK(o->blend_on && o->blend.srcColorBlendFactor == blend[e][0]
            && o->blend.dstColorBlendFactor == blend[e][1]
            && o->blend.srcAlphaBlendFactor == blend[e][2]
            && o->blend.dstAlphaBlendFactor == blend[e][3],
            "effect %u: blend", e);
      CHECK(o->ubo_v_size >= 0x400 && o->ubo_f_size >= 0x400
            && !memcmp(o->ubo_v, o->ubo_f, sizeof(o->ubo_v)),
            "effect %u: uniforms not bound to both stages", e);
      if (e == 0)
         CHECK(!memcmp(o->ubo_v, ribbon_pat, sizeof(ribbon_pat))
               && ribbon_yflip_asked == 1.0f, "ribbon uniforms");
      else if (e == 1)
         CHECK(o->ubo_v[0] == 1.5f && o->ubo_v[1] == 1.0f
               && o->ubo_v[2] == 0.75f, "ribbon simple uniforms");
      else
         CHECK(o->ubo_v[16] == 1280.0f && o->ubo_v[17] == 720.0f
               && o->ubo_v[18] == 1.5f
               && o->ubo_v[19] == (e >= 4 ? 1.0f : 0.0f),
               "effect %u uniforms %g %g %g %g", e, o->ubo_v[16],
               o->ubo_v[17], o->ubo_v[18], o->ubo_v[19]);
   }
   /* the menu's quad after them: its own pipeline, and its matrix sent
    * again over the ribbon's uniforms */
   if (fx_nops == 7)
   {
      const op_t *o = &fx_ops[6];
      CHECK(o->vs == deko3d_menu_vsh_dksh && o->fs == deko3d_menu_fsh_dksh
            && o->stride == 32 && o->attribs == 3
            && !memcmp(o->ubo_v, dk->mvp_default.data, 64),
            "the menu's quad after the effects");
   }
   /* the next frame: the mesh where it was */
   ribbon_vbo = fx_ops[0].vbo;
   frame(dk, NULL, 0, 0, 0);
   CHECK(fx_nops == 7 && fx_ops[0].vbo == ribbon_vbo,
         "the ribbon's positions made again");
   menu_draws        = NULL;
   vfi.menu_st_flags = 0;
   for (i = 0; i < 32; i++)
      if (!gfx_display_ctx_deko3d.effect_install(dk, &fxd[5]))
         break;
   CHECK(i < 32 && dk->fx_count <= DK3D_FX_MAX
         && dk->fx_used <= DK3D_FX_CODE, "installs past the code block");
   driver_free(dk);
   CHECK(!blocks_live, "%u memory blocks left at free", blocks_live);
   printf("[pass] effects: programs, blends, uniforms and meshes, the menu after them\n");
}

int main(void)
{
   lane_init();
   lane_frame();
   lane_textures();
   lane_menu_quad();
   lane_text();
   lane_rgui();
   lane_sync();
   lane_effects();
   if (failures)
   {
      printf("FAIL deko3d_test (%d)\n", failures);
      return 1;
   }
   printf("PASS deko3d_test\n");
   return 0;
}
