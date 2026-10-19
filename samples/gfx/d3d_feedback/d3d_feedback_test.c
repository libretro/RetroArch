/* Runs the shipping allocation, preparation and swap code against a
 * resource model. Checks feedback before the first draw, across frames
 * and after resize. No platform headers or graphics device required. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>

typedef int bool;
#define FBO_SCALE_FLAG_VALID 1
#define RARCH_SCALE_INPUT 1
#define RARCH_SCALE_VIEWPORT 2
#define RARCH_SCALE_ABSOLUTE 3
#define D3D11_BIND_RENDER_TARGET 1
#define D3D11_RESOURCE_MISC_GENERATE_MIPS 2
#define D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET 1
#define D3D11_ST_FLAG_RESIZE_RTS 1
#define D3D12_ST_FLAG_RESIZE_RTS 1
#define D3D12_RESOURCE_STATE_RENDER_TARGET 1
#define D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE 2
#define VIDEO_SCALE_W(d) ((d) & 65535)
#define VIDEO_SCALE_H(d) ((d) >> 16)
#define VIDEO_POS_X(p) ((p) & 65535)
#define VIDEO_POS_Y(p) ((p) >> 16)
#define RARCH_DBG(a, b, c) ((void)0)

static unsigned failures;
#define CHECK(x) do { if (!(x)) { \
   fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); failures++; \
} } while (0)

struct resource { int state; unsigned pixel, mip_levels; };
struct descriptor
{
   unsigned Width, Height, Format, Flags, BindFlags, MiscFlags, MipLevels;
};
typedef struct texture
{
   struct descriptor desc;
   struct resource *handle, *rt_view;
   void *srv_heap;
   struct { float x, y, z, w; } size_data;
} d3d11_texture_t;
typedef d3d11_texture_t d3d12_texture_t;

struct video_shader_pass
{
   struct
   {
      unsigned flags, abs_x, abs_y;
      int type_x, type_y;
      float scale_x, scale_y;
   } fbo;
   int feedback, mipmap, filter, wrap;
};
struct video_shader { unsigned passes; struct video_shader_pass pass[3]; };
typedef struct pass_data
{
   d3d11_texture_t rt, feedback;
   struct { unsigned format; } semantics;
   struct { float TopLeftX, TopLeftY, Width, Height, MinDepth, MaxDepth; } viewport;
   struct { unsigned left, top, right, bottom; } scissorRect;
   int sampler;
} d3d11_pass_t;
typedef d3d11_pass_t d3d12_pass_t;
typedef struct chain
{
   struct video_shader *shader_preset;
   d3d11_pass_t pass[3];
   int mvp_last_pass;
} d3d11_chain_t;
typedef d3d11_chain_t d3d12_filter_chain_t;
struct video_viewport { unsigned dims, pos; };
struct context;
struct context_vtable
{
   void (*ClearRenderTargetView)(struct context *, struct resource *, const float *, ...);
};
struct context { struct context_vtable *lpVtbl; };
typedef struct context *D3D12GraphicsCommandList;
typedef struct video
{
   struct { int srv_heap; } desc;
   struct { int mvp; } ubo_values;
   struct { D3D12GraphicsCommandList cmd; } queue;
   int mvp, samplers[3][3];
   unsigned flags;
   void *device;
   struct context *context;
} d3d11_video_t;
typedef d3d11_video_t d3d12_video_t;

static struct resource resources[256];
static unsigned created;

static void clear(struct context *ctx, struct resource *r, const float *color, ...)
{
   unsigned i;
   (void)ctx;
   CHECK(r != NULL);
   if (!r)
      return;
   CHECK(r->state == D3D12_RESOURCE_STATE_RENDER_TARGET);
   for (i = 0; i < 4; i++)
      CHECK(color[i] == 0.0f);
   r->pixel = 0;
}

static void transition(struct context *ctx, struct resource *r, int before, int after)
{
   (void)ctx;
   CHECK(r != NULL);
   if (!r)
      return;
   CHECK(r->state == before);
   r->state = after;
}
#define D3D12_RESOURCE_TRANSITION(c, r, b, a) transition(c, r, b, a)

static void init_texture(void *device, d3d11_texture_t *t)
{
   (void)device;
   if (created == sizeof(resources) / sizeof(resources[0]))
      abort();
   t->handle = &resources[created++];
   t->rt_view = t->handle;
   t->handle->state = D3D12_RESOURCE_STATE_RENDER_TARGET;
   t->handle->pixel = 0xdead;
   t->handle->mip_levels = t->desc.MipLevels;
   t->size_data.x = (float)t->desc.Width;
   t->size_data.y = (float)t->desc.Height;
}
static void release_texture(d3d11_texture_t *t)
{
   if (t->handle)
      CHECK(t->desc.MipLevels == t->handle->mip_levels);
   t->handle = t->rt_view = NULL;
}
#define d3d11_init_texture init_texture
#define d3d12_init_texture init_texture
#define d3d11_release_texture release_texture
/* D3D12's takes the driver too, to wait for frames in flight */
#define d3d12_release_texture(d, t) release_texture(t)
static unsigned glslang_format_to_dxgi(unsigned format) { return format; }
static int video_driver_get_rotation_snapshot(void) { return 0; }

static void d3d12_generate_mipmaps(struct context *cmd,
      d3d12_texture_t *texture, d3d12_video_t *video)
{
   (void)cmd;
   (void)video;
   CHECK(texture->handle->state == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

#include "driver_code.inc"

static void run_case(int api, unsigned passes, unsigned feedback_pass)
{
   struct context_vtable vtable = { clear };
   struct context context;
   struct video_shader preset;
   struct video v;
   struct video_viewport vp;
   d3d11_chain_t c;
   unsigned resize, frame, i, width, height;
   memset(&preset, 0, sizeof(preset));
   memset(&v, 0, sizeof(v));
   memset(&c, 0, sizeof(c));
   context.lpVtbl = &vtable;
   preset.passes = passes;
   if (feedback_pass < passes)
      preset.pass[feedback_pass].feedback = 1;
   c.shader_preset = &preset;
   v.context = &context;
   v.queue.cmd = &context;

   for (resize = 0; resize < 2; resize++)
   {
      width = 80 + 40 * resize;
      height = 60 + 30 * resize;
      vp.dims = width | (height << 16);
      vp.pos = 7 | (9 << 16);
      v.flags = 1;
      if (passes > 1)
         preset.pass[1].mipmap = !resize;
      if (api == 11)
      {
         d3d11_chain_init_render_targets(&v, &c, width, height, vp.dims);
         CHECK(!v.flags);
      }
      else
         d3d12_chain_init_render_targets(&v, &c, width, height, &vp);
      for (i = 0; i < passes; i++)
      {
         CHECK((c.pass[i].rt.handle != NULL)
               == (i + 1 < passes || i == feedback_pass));
         CHECK((c.pass[i].feedback.handle != NULL) == (i == feedback_pass));
         if (api == 12 && c.pass[i].rt.handle)
         {
            unsigned requested = i + 1 < passes && preset.pass[i + 1].mipmap
               ? UINT16_MAX : 1;
            CHECK(c.pass[i].rt.desc.MipLevels == requested);
            if (c.pass[i].feedback.handle)
               CHECK(c.pass[i].feedback.desc.MipLevels == requested);
         }
      }
      if (feedback_pass >= passes)
         continue;
      CHECK(c.pass[feedback_pass].feedback.desc.Width == width);
      CHECK(c.pass[feedback_pass].feedback.desc.Height == height);
      if (api == 12)
      {
         CHECK(c.pass[feedback_pass].viewport.TopLeftX == 0.0f);
         CHECK(c.pass[feedback_pass].viewport.TopLeftY == 0.0f);
         CHECK(c.pass[feedback_pass].scissorRect.left == 0);
         CHECK(c.pass[feedback_pass].scissorRect.top == 0);
         CHECK(c.pass[feedback_pass].scissorRect.right == width);
         CHECK(c.pass[feedback_pass].scissorRect.bottom == height);
      }
      if (!c.pass[feedback_pass].feedback.handle || !c.pass[feedback_pass].rt.handle)
         continue;
      CHECK(c.pass[feedback_pass].rt.handle->pixel == 0);
      CHECK(c.pass[feedback_pass].feedback.handle->pixel == 0);
      if (api == 12)
      {
         CHECK(c.pass[feedback_pass].rt.handle->state
               == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
         CHECK(c.pass[feedback_pass].feedback.handle->state
               == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
      }

      for (frame = 0; frame < 3; frame++)
      {
         struct resource *feedback;
         if (api == 11)
            swap_d3d11(&c);
         else
            swap_d3d12(&c);
         feedback = c.pass[feedback_pass].feedback.handle;
         /* Sample before any pass is drawn, including a future pass. */
         CHECK(feedback->pixel == frame);
         if (api == 12)
         {
            CHECK(feedback->state == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            transition(&context, c.pass[feedback_pass].rt.handle,
                  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                  D3D12_RESOURCE_STATE_RENDER_TARGET);
         }
         c.pass[feedback_pass].rt.handle->pixel = frame + 1;
         if (api == 12)
            transition(&context, c.pass[feedback_pass].rt.handle,
                  D3D12_RESOURCE_STATE_RENDER_TARGET,
                  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
      }
   }
}

int main(void)
{
   int api;
   for (api = 11; api <= 12; api++)
   {
      run_case(api, 1, 0); /* Final pass reads itself. */
      run_case(api, 3, 2); /* First pass reads final-pass feedback. */
      run_case(api, 3, 1); /* Feedback from an intermediate pass. */
      run_case(api, 3, 3); /* No feedback: final pass draws directly. */
   }
   if (!failures)
      puts("ok: D3D11/12 feedback allocation, initial contents, states, swaps and resize");
   return failures ? 1 : 0;
}
