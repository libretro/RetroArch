#!/usr/bin/env python3
"""Exercise shipping viewer HDR input encoding, including allocation failures."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/drivers/vulkan.c").read_text(encoding="utf-8")
start = source.index("static bool vulkan_media_shader_input(")
function = source[start:source.index("\n}\n", start) + 3]
start = source.index("#define VULKAN_WRITE_QUAD_VBO(")
quad = source[start:source.index("\n\n", start)]
fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#define VK_FLAG_SDR_PIPELINE 1
#define VK_FLAG_TRACKER_USE_SCISSOR 2
#define VK_FORMAT_R16G16B16A16_SFLOAT 97
#define VIDEO_SCALE_PACK(w,h) (((w)<<16)|(h))
#define VIDEO_SCALE_W(d) ((d)>>16)
#define VIDEO_SCALE_H(d) ((d)&65535)
#define RGBA16_WHITE UINT64_MAX
typedef struct { float data[16]; } math_matrix_4x4;
struct vk_vertex { float x,y,tex_x,tex_y; uint64_t color; };
struct vk_image { unsigned image, memory, framebuffer; };
struct vk_texture { unsigned image, format; };
struct vulkan_filter_chain_texture { unsigned image, format; };
struct vk_buffer_range { void *data; unsigned buffer, offset; };
struct vk_draw_triangles {
   const void *uniform;
   struct vk_buffer_range *vbo;
   struct vk_texture *texture;
   unsigned pipeline, sampler;
   size_t uniform_size;
   unsigned vertices;
};
struct context { unsigned device; };
struct chain { unsigned vbo; };
typedef struct {
   unsigned flags, media_shader_input_dims, sdr_render_pass, cmd;
   struct vk_image media_shader_input;
   struct { unsigned pipeline_linear_sdr; } display;
   struct { unsigned nearest; } samplers;
   struct { float menu_nits; struct { unsigned expand_gamut; } ubo_values; } hdr;
   struct { float x,y,width,height,minDepth,maxDepth; } vk_vp;
   struct { struct { struct { int x,y; } offset; struct { unsigned width,height; } extent; } scissor; } tracker;
   math_matrix_4x4 mvp_no_rot;
   struct context *context;
   struct chain *chain;
} vk_t;
static unsigned allocations, draws, begun, ended;
static bool retire_ok = true, buffer_ok = true, bind_ok = true, target_ok = true;
static struct vk_vertex vertices[4];
static bool vulkan_views_retire(vk_t *v, struct vk_image *i)
{ (void)v; if (!retire_ok) return false; memset(i,0,sizeof(*i)); return true; }
static void vulkan_init_render_target(struct vk_image *i, unsigned dims,
      unsigned format, unsigned pass, struct context *ctx)
{ (void)ctx; assert(dims == VIDEO_SCALE_PACK(320,240) && format == 97 && pass == 5);
  i->image = 80; i->memory = i->framebuffer = target_ok; allocations++; }
static void vulkan_destroy_hdr_buffer(unsigned device, struct vk_image *i)
{ (void)device; memset(i,0,sizeof(*i)); }
static bool vulkan_buffer_chain_alloc(struct context *ctx, unsigned *vbo,
      size_t size, struct vk_buffer_range *r)
{ (void)ctx; (void)vbo; assert(size == sizeof(vertices)); r->data=vertices; r->buffer=1; r->offset=0; return buffer_ok; }
static void vulkan_views_begin(vk_t *v, struct vk_image *i, unsigned dims, unsigned pass)
{ (void)v; assert(i->image == 80 && dims == VIDEO_SCALE_PACK(320,240) && pass == 5); begun++; }
static void vulkan_views_end(vk_t *v, struct vk_image *i)
{ (void)v; assert(i->image == 80); ended++; }
static bool vulkan_draw_bind(vk_t *v, struct vk_draw_triangles *c)
{
   const float *u = (const float*)c->uniform;
   assert(c->pipeline == 42 && c->texture->image == 9 && c->sampler == 33);
   assert(c->uniform_size == 80 && u[16] == 203 && u[17] == 2 && !u[18] && !u[19]);
   assert(v->vk_vp.width == 320 && v->vk_vp.height == 240 && !v->vk_vp.x && !v->vk_vp.y);
   assert(v->tracker.scissor.extent.width == 320 && v->tracker.scissor.extent.height == 240);
   assert(vertices[0].x == 0 && vertices[0].tex_x == 0 && vertices[3].x == 1 && vertices[3].tex_x == 1);
   assert(vertices[0].color == UINT64_MAX && vertices[3].color == UINT64_MAX);
   return bind_ok;
}
static void vkCmdBindVertexBuffers(unsigned cmd, unsigned first, unsigned n, unsigned *buffer, unsigned *offset)
{ (void)cmd; assert(!first && n == 1 && *buffer == 1 && !*offset); }
static void vkCmdDraw(unsigned cmd, unsigned n, unsigned count, unsigned first, unsigned base)
{ (void)cmd; assert(n == 4 && count == 1 && !first && !base); draws++; }
static void vulkan_views_texture(struct vk_image *i, unsigned dims, unsigned format, struct vk_texture *t)
{ assert(dims == VIDEO_SCALE_PACK(320,240)); t->image=i->image; t->format=format; }
static void vulkan_movie_input(struct vk_texture *t, struct vulkan_filter_chain_texture *i)
{ i->image=t->image; i->format=t->format; }
QUAD
FUNCTION
int main(void)
{
   vk_t video;
   struct context context;
   struct chain chain;
   struct vk_texture texture = {9, 97};
   struct vulkan_filter_chain_texture input = {9, 97};
   unsigned dims = VIDEO_SCALE_PACK(320,240);
   memset(&video,0,sizeof(video));
   video.context=&context; video.chain=&chain;
   video.flags=VK_FLAG_SDR_PIPELINE;
   video.display.pipeline_linear_sdr=42; video.sdr_render_pass=5;
   video.samplers.nearest=33; video.hdr.menu_nits=203; video.hdr.ubo_values.expand_gamut=2;
   assert(vulkan_media_shader_input(&video,&texture,dims,&input));
#ifdef VULKAN_HDR_SWAPCHAIN
   assert(input.image == 80 && input.format == 97 && allocations == 1 && draws == 1);
   assert(begun == ended && !(video.flags & VK_FLAG_TRACKER_USE_SCISSOR));
   video.flags |= VK_FLAG_TRACKER_USE_SCISSOR;
   assert(vulkan_media_shader_input(&video,&texture,dims,&input));
   assert(allocations == 1 && draws == 2 && (video.flags & VK_FLAG_TRACKER_USE_SCISSOR));
   texture.format=37; input.image=9;
   assert(vulkan_media_shader_input(&video,&texture,dims,&input) && input.image == 9 && draws == 2);
   texture.format=97; video.flags=0;
   assert(vulkan_media_shader_input(&video,&texture,dims,&input) && input.image == 9 && draws == 2);
   video.flags=1; buffer_ok=false;
   assert(!vulkan_media_shader_input(&video,&texture,dims,&input) && begun == ended);
   buffer_ok=true; bind_ok=false;
   assert(!vulkan_media_shader_input(&video,&texture,dims,&input) && begun == ended && draws == 2);
   bind_ok=true; video.media_shader_input_dims=0; retire_ok=false;
   assert(!vulkan_media_shader_input(&video,&texture,dims,&input));
   retire_ok=true; target_ok=false;
   assert(!vulkan_media_shader_input(&video,&texture,dims,&input));
   assert(!video.media_shader_input.image && !video.media_shader_input_dims);
#else
   assert(input.image == 9 && !allocations && !draws);
#endif
   puts("PASS: Vulkan viewer HDR encode, SDR bypass, constants, reuse, failure cleanup and scissor restoration");
   return 0;
}
'''.replace("QUAD", quad).replace("FUNCTION", function)
cc = shlex.split(os.environ.get("CC", "cc"))
with tempfile.TemporaryDirectory(prefix="viewer-vulkan-hdr-") as directory:
    unit = Path(directory) / "test.c"
    exe = Path(directory) / ("test.exe" if os.name == "nt" or "mingw" in " ".join(cc) else "test")
    unit.write_bytes(fixture.encode())
    for hdr in (False, True):
        subprocess.run(cc + shlex.split(os.environ.get("CFLAGS", "")) + [
            "-std=gnu89", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function",
            "-Wno-unused-variable", "-Werror=declaration-after-statement"] +
            (["-DVULKAN_HDR_SWAPCHAIN"] if hdr else []) + [str(unit), "-o", str(exe)], check=True)
        subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
