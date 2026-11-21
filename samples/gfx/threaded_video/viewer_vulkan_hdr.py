#!/usr/bin/env python3
"""Shipping viewer callback borrows HDR input without a conversion draw."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/drivers/vulkan.c").read_text(encoding="utf-8")
start = source.index("static bool gfx_display_vk_media_shader_draw(")
function = source[start:source.index("\n}\n", start) + 3]
fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "gfx/common/hdr_viewer_colour.h"
#include "gfx/common/media_history.h"
#define VIDEO_SCALE_PACK(w,h) (((w)<<16)|(h))
#define VIDEO_SCALE_W(d) ((d)>>16)
#define VIDEO_SCALE_H(d) ((d)&65535)
#define VIDEO_POS_X(d) ((d)>>16)
#define VIDEO_POS_Y(d) ((d)&65535)
#define VK_FLAG_SDR_PIPELINE 1
#define VK_FORMAT_R16G16B16A16_SFLOAT 97
#define VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO 1
#define VK_SUBPASS_CONTENTS_INLINE 1
#define VK_NULL_HANDLE 0
#define VULKAN_DIRTY_DYNAMIC_BIT 1
#define VULKAN_IMAGE_LAYOUT_TRANSITION(...) ((void)0)
typedef struct { float x,y,width,height,minDepth,maxDepth; } VkViewport;
typedef struct { struct { int x,y; } offset; struct { unsigned width,height; } extent; } VkRect2D;
typedef struct { unsigned sType,renderPass,framebuffer; VkRect2D renderArea; } VkRenderPassBeginInfo;
struct vk_texture { unsigned image,format; };
struct vulkan_filter_chain_texture { unsigned image; };
typedef struct { unsigned image, hdr; float rows[12]; } vulkan_filter_chain_t;
struct image { unsigned image,framebuffer; };
struct context { unsigned current_frame_index; };
typedef struct {
   gfx_media_history_t media_history[2];
   uintptr_t media_history_texture[2], media_last_texture;
   unsigned flags,keep_render_pass,cmd,media_shader_frame;
   bool media_shader_enabled;
   vulkan_filter_chain_t *media_shader_chain[2];
   struct { unsigned keep_sdr_pass; struct image *target; } bloom;
   struct context *context;
   VkViewport vk_vp;
   struct { VkRect2D scissor; unsigned pipeline,dirty; } tracker;
   struct { float data[16]; } mvp_no_rot;
   struct { float menu_nits; struct { unsigned expand_gamut; } ubo_values; } hdr;
} vk_t;
static unsigned begun,ended,offscreen,viewport;
static void vulkan_movie_input(struct vk_texture *t, struct vulkan_filter_chain_texture *i) { i->image=t->image; }
static void vulkan_views_sync_params(vk_t *v,vulkan_filter_chain_t *c) { (void)v;(void)c; }
static void vulkan_filter_chain_notify_sync_index(vulkan_filter_chain_t *c,unsigned i) { (void)c;(void)i; }
static void vulkan_filter_chain_set_frame_count(vulkan_filter_chain_t *c,unsigned i) { (void)c;(void)i; }
static void vulkan_filter_chain_set_frame_direction(vulkan_filter_chain_t *c,int i) { (void)c;(void)i; }
static void vulkan_filter_chain_set_rotation(vulkan_filter_chain_t *c,unsigned i) { (void)c;(void)i; }
static void vulkan_filter_chain_set_core_aspect(vulkan_filter_chain_t *c,float f) { (void)c;(void)f; }
static void vulkan_filter_chain_set_viewer_colour(vulkan_filter_chain_t *c,const float *r) { memcpy(c->rows,r,sizeof(c->rows)); }
static void vulkan_filter_chain_set_input_texture_history(vulkan_filter_chain_t *c,const struct vulkan_filter_chain_texture *i, struct gfx_media_history *h) { (void)h;c->image=i->image; }
static void vulkan_filter_chain_set_source_hdr(vulkan_filter_chain_t *c,unsigned i) { (void)c;assert(!i); }
static void vulkan_filter_chain_set_hdr_mode(vulkan_filter_chain_t *c,unsigned i) { c->hdr=i; }
static void vulkan_filter_chain_build_offscreen_passes(vulkan_filter_chain_t *c,unsigned cmd,const VkViewport *vp) { (void)c;(void)cmd;assert(vp->width==320);offscreen++; }
static void vulkan_filter_chain_build_viewport_pass(vulkan_filter_chain_t *c,unsigned cmd,const VkViewport *vp,const float *m) { (void)c;(void)cmd;(void)vp;(void)m;viewport++; }
static void vulkan_filter_chain_end_frame(vulkan_filter_chain_t *c,unsigned cmd) { (void)c;(void)cmd; }
static void vkCmdBeginRenderPass(unsigned cmd,const VkRenderPassBeginInfo *r,unsigned mode) { (void)cmd;(void)mode;assert(r->framebuffer==4);begun++; }
static void vkCmdEndRenderPass(unsigned cmd) { (void)cmd;ended++; }
static vulkan_filter_chain_t *vulkan_media_chain(vk_t *v,unsigned i) { return v->media_shader_chain[i]; }
FUNCTION
int main(void)
{
   vk_t v;
   vulkan_filter_chain_t chains[2];
   struct vk_texture texture = { 9,97 };
   struct image target = { 3,4 };
   struct context context = { 0 };
   unsigned i;
   memset(&v,0,sizeof(v));memset(chains,0,sizeof(chains));
   v.media_shader_enabled=true;v.bloom.target=&target;v.context=&context;
   v.bloom.keep_sdr_pass=5;v.keep_render_pass=6;v.vk_vp.x=17;
   v.hdr.menu_nits=203;v.hdr.ubo_values.expand_gamut=2;
   v.media_shader_chain[0]=&chains[0];v.media_shader_chain[1]=&chains[1];
   for(i=0;i<3;i++)
   {
      v.flags=i==0 ? 1:0;texture.format=i==2 ? 37:97;
      assert(gfx_display_vk_media_shader_draw(&v,(uintptr_t)&texture,
         VIDEO_SCALE_PACK(320,240),VIDEO_SCALE_PACK(640,480),0,VIDEO_SCALE_PACK(320,240)));
      assert(chains[i==0 ? 1:0].image==texture.image);
#ifdef VULKAN_HDR_SWAPCHAIN
      assert(chains[i==0 ? 1:0].rows[3]==(i==0 ? 1.0f:0.0f));
#else
      assert(!chains[i==0 ? 1:0].rows[3]);
#endif
      assert(v.vk_vp.x==17 && begun==2*(i+1) && ended==begun);
      assert(viewport==i+1 && offscreen==i+1);
   }
   puts("PASS: original texture borrowed, no conversion allocation or draw, HDR uniforms and SDR bypass");
   return 0;
}
'''.replace("FUNCTION",function)
cc = shlex.split(os.environ.get("CC", "cc"))
with tempfile.TemporaryDirectory(prefix="viewer-vulkan-hdr-") as directory:
    unit = Path(directory) / "test.c"
    exe = Path(directory) / ("test.exe" if os.name == "nt" or "mingw" in " ".join(cc) else "test")
    unit.write_bytes(fixture.encode())
    for hdr in (False, True):
        subprocess.run(cc + shlex.split(os.environ.get("CFLAGS", "")) + [
            "-std=gnu89", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function",
            "-Wno-unused-variable", "-Werror=declaration-after-statement"] +
            (["-DVULKAN_HDR_SWAPCHAIN"] if hdr else []) + ["-DHAVE_RSLC", "-I" + str(root), "-I" + str(root / "libretro-common/include")] + [str(unit), "-o", str(exe)], check=True)
        subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
