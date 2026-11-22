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
def extract(signature):
    start = source.index(signature)
    return source[start:source.index("\n}\n", start) + 3]

lifetime = "\n".join(extract(signature) for signature in (
    "static void vulkan_destroy_texture(",
    "static void vulkan_media_history_retain(",
    "static void vulkan_media_history_release(",
    "static void vulkan_deferred_textures_push(",
    "static void vulkan_deferred_textures_collect(",
    "static void vulkan_deferred_textures_tick(",
    "static void vulkan_deferred_textures_flush("))
fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <queues/mpsc_stack.h>
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
typedef unsigned VkDevice;
#define VULKAN_TEXTURE_STREAMED 1
#define VK_FORMAT_UNDEFINED 0
#define VK_IMAGE_LAYOUT_UNDEFINED 0
struct vk_texture {
   unsigned image,view,format,history_view,history_refs,park_frames,levels;
   unsigned memory,buffer,type,flags,memory_type,dims,layout;
   size_t offset,stride,size,memory_size;
   void *mapped;
   struct vk_media_version *media_version;
   struct vk_media_pool *media_pool;
   struct vk_texture *media_owner;
   bool immutable;
   mpsc_stack_node_t park;
};
struct vulkan_filter_chain_texture { unsigned image,view; };
typedef struct { unsigned image, hdr; float rows[12]; void *owner; unsigned lease_view; void (*release)(void *); } vulkan_filter_chain_t;
struct image { unsigned image,framebuffer; };
struct context { unsigned current_frame_index,num_swapchain_images,device; };
typedef struct {
   mpsc_stack_t deferred_textures_in;
   mpsc_stack_node_t *deferred_textures;
   uint64_t media_submit_serial;
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
struct vk_media_version { struct vk_texture texture; };
static bool vulkan_media_pool_held(const struct vk_texture *t){return t->history_refs!=0;}
static unsigned pool_requests;
static bool needs_history;
static void vulkan_media_pool_get(vk_t *v,struct vk_texture *t){(void)v;(void)t;pool_requests++;}
static void vulkan_media_read(void *owner){(void)owner;}
static bool vulkan_filter_chain_uses_history(const vulkan_filter_chain_t *c){(void)c;return needs_history;}
static void vulkan_filter_chain_visit_history(const vulkan_filter_chain_t *c,void (*read)(void *))
{if(c->owner)read(c->owner);}
#define VK_PARKED_TEXTURE(node) ((struct vk_texture*)((char*)(node) - offsetof(struct vk_texture,park)))
static unsigned retired,views_deleted[512],images_deleted;
static void vkUnmapMemory(VkDevice device,unsigned memory){(void)device;(void)memory;}
static void vkDestroyImageView(VkDevice device,unsigned view,void *allocator)
{(void)device;(void)allocator;assert(view<512);assert(!views_deleted[view]++);}
static void vkDestroyImage(VkDevice device,unsigned image,void *allocator)
{(void)device;(void)image;(void)allocator;images_deleted++;}
static void vkDestroyBuffer(VkDevice device,unsigned buffer,void *allocator)
{(void)device;(void)buffer;(void)allocator;}
static void vkFreeMemory(VkDevice device,unsigned memory,void *allocator)
{(void)device;(void)memory;(void)allocator;}
static void vulkan_destroy_texture(VkDevice device,struct vk_texture *t);
static void vulkan_texture_retire(vk_t *v,struct vk_texture *t)
{assert(!t->history_refs);retired++;vulkan_destroy_texture(v->context->device,t);}
LIFETIME
static void vulkan_filter_chain_set_history_lease(vulkan_filter_chain_t *c,
      const struct vulkan_filter_chain_texture *texture,void *owner,
      void (*retain)(void *),void (*release)(void *))
{
   if(c->owner)c->release(c->owner);
   c->owner=owner;c->release=release;
   if(owner){retain(owner);c->lease_view=texture->view;}
}
FUNCTION
int main(void)
{
   vk_t v;
   vulkan_filter_chain_t chains[2];
   struct vk_texture texture;
   struct image target = { 3,4 };
   struct context context = { 0,3,7 };
   unsigned i;
   memset(&v,0,sizeof(v));memset(chains,0,sizeof(chains));memset(&texture,0,sizeof(texture));
   texture.image=9;texture.view=10;texture.format=97;
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
   assert(!pool_requests);
   needs_history=true;
   assert(gfx_display_vk_media_shader_draw(&v,(uintptr_t)&texture,
      VIDEO_SCALE_PACK(320,240),VIDEO_SCALE_PACK(640,480),0,VIDEO_SCALE_PACK(320,240)));
   assert(pool_requests==1);needs_history=false;
   texture.history_view=123;texture.immutable=true;
   v.flags=0;
   assert(gfx_display_vk_media_shader_draw(&v,(uintptr_t)&texture,
      VIDEO_SCALE_PACK(320,240),VIDEO_SCALE_PACK(640,480),0,VIDEO_SCALE_PACK(320,240)));
   assert(texture.history_refs==1 && chains[0].lease_view==123);
   mpsc_stack_init(&v.deferred_textures_in);
   vulkan_deferred_textures_push(&v,&texture);
   for(i=0;i<20;i++)vulkan_deferred_textures_tick(&v);
   assert(!retired && texture.history_refs==1);
   vulkan_deferred_textures_flush(&v);
   assert(!retired && texture.history_refs==1 && v.deferred_textures);
   texture.history_view=0;
   assert(!gfx_display_vk_media_shader_draw(&v,(uintptr_t)&texture,
      VIDEO_SCALE_PACK(320,240),VIDEO_SCALE_PACK(640,480),0,VIDEO_SCALE_PACK(320,240)));
   texture.immutable=false;
   assert(gfx_display_vk_media_shader_draw(&v,(uintptr_t)&texture,
      VIDEO_SCALE_PACK(320,240),VIDEO_SCALE_PACK(640,480),0,VIDEO_SCALE_PACK(320,240)));
   assert(!texture.history_refs);
   texture.history_view=123;
   for(i=0;i<3;i++){vulkan_deferred_textures_tick(&v);assert(!retired);}
   vulkan_deferred_textures_tick(&v);assert(retired==1 && !v.deferred_textures);
   assert(views_deleted[123]==1 && views_deleted[10]==1 && images_deleted==1);
   texture.image=11;texture.view=texture.history_view=124;
   vulkan_deferred_textures_push(&v,&texture);
   vulkan_deferred_textures_flush(&v);
   assert(retired==2 && !v.deferred_textures);
   assert(views_deleted[124]==1 && images_deleted==2 && !texture.history_view);
   puts("PASS: original texture/HDR input, immutable history lease, unload deferral and level-zero allocation failure");
   return 0;
}
'''.replace("FUNCTION",function).replace("LIFETIME",lifetime)
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
