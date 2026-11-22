#!/usr/bin/env python3
"""Shipping Vulkan menu draw must cover the display outside a core viewport."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/drivers/vulkan.c").read_text(encoding="utf-8")


def function(name):
    start = source.index(name)
    return source[start:source.index("\n}\n", start) + 3]


draw = function("static void gfx_display_vk_draw(")
assert draw.count("vulkan_display_draw_triangles(vk, &call)") == 3
assert "vulkan_draw_triangles(vk, &call)" not in draw
functions = "\n".join(function(name) for name in (
    "static bool vulkan_draw_bind(", "static void vulkan_draw_triangles(",
    "static void vulkan_display_draw_triangles("))
fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#undef assert
#define assert(c) do { if (!(c)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c); exit(1); } } while (0)
#define VIDEO_SCALE_PACK(w,h) (((w)<<16)|(h))
#define VIDEO_SCALE_W(d) ((d)>>16)
#define VIDEO_SCALE_H(d) ((d)&65535)
#define VIDEO_POS_X VIDEO_SCALE_W
#define VIDEO_POS_Y VIDEO_SCALE_H
#define VK_FLAG_TRACKER_USE_SCISSOR 1
#define VULKAN_DIRTY_DYNAMIC_BIT 1
#define VK_PIPELINE_BIND_POINT_GRAPHICS 0
#define VK_NULL_HANDLE 0
typedef unsigned VkDescriptorSet;
typedef struct { struct { int x,y; } offset; struct { unsigned width,height; } extent; } VkRect2D;
typedef struct { float x,y,width,height,minDepth,maxDepth; } VkViewport;
struct vk_texture { unsigned image; struct { struct vk_texture *texture; } *media_version; };
static struct vk_texture *vulkan_media_sample_texture(struct vk_texture *t){return t;}
static void vulkan_media_read(void *owner){(void)owner;}
struct vk_buffer_range { void *data; unsigned buffer,offset; };
struct vk_draw_triangles {
   struct vk_texture *texture;
   unsigned pipeline,sampler;
   const void *uniform;
   size_t uniform_size;
   const struct vk_buffer_range *vbo;
   unsigned vertices;
};
struct context { unsigned device; };
struct chain { unsigned ubo,descriptor_manager; };
typedef struct {
   struct context *context;
   struct chain *chain;
   struct { unsigned layout; } pipelines;
   unsigned cmd,flags,display_dims;
   struct { unsigned pos,dims; } vp;
   VkViewport vk_vp;
   struct { VkRect2D scissor; unsigned pipeline,dirty,view,sampler;
      struct { float data[16]; } mvp; } tracker;
} vk_t;
static VkRect2D recorded;
static bool fail_alloc;
static unsigned rendered;
static unsigned vulkan_display_dims(const vk_t *vk) { return vk->display_dims; }
static void vulkan_transition_texture(vk_t *v,unsigned c,struct vk_texture *t) { (void)v;(void)c;(void)t; }
static void vkCmdBindPipeline(unsigned c,unsigned b,unsigned p) { (void)c;(void)b;(void)p; }
static void vkCmdSetViewport(unsigned c,unsigned f,unsigned n,const VkViewport *v) { (void)c;(void)f;(void)n;(void)v; }
static void vkCmdSetScissor(unsigned c,unsigned f,unsigned n,const VkRect2D *s) { (void)c;(void)f;assert(n==1);recorded=*s; }
static bool vulkan_buffer_chain_alloc(struct context *c,unsigned *chain,size_t n,struct vk_buffer_range *r)
{ static unsigned ubo; (void)c;(void)chain;assert(n==sizeof(ubo));r->data=&ubo;return !fail_alloc; }
static unsigned vulkan_descriptor_manager_alloc(unsigned d,unsigned *m) { (void)d;(void)m;return 1; }
static void vulkan_write_quad_descriptors(unsigned d,unsigned s,unsigned b,unsigned o,size_t z,struct vk_texture *t,unsigned p)
{ (void)d;(void)s;(void)b;(void)o;(void)z;(void)t;(void)p; }
static void vkCmdBindDescriptorSets(unsigned c,unsigned b,unsigned l,unsigned f,unsigned n,const unsigned *s,unsigned m,const void *o)
{ (void)c;(void)b;(void)l;(void)f;(void)n;(void)s;(void)m;(void)o; }
static void vkCmdBindVertexBuffers(unsigned c,unsigned f,unsigned n,const unsigned *b,const unsigned *o)
{ (void)c;(void)f;(void)n;(void)b;(void)o; }
static void vkCmdDraw(unsigned c,unsigned n,unsigned i,unsigned f,unsigned j)
{ (void)c;(void)n;(void)i;(void)f;(void)j;rendered++; }
FUNCTIONS
int main(void)
{
   vk_t vk;
   struct context context = {0};
   struct chain chain = {0};
   struct vk_buffer_range vbo = {0};
   unsigned uniform = 0, i;
   struct vk_draw_triangles call = {0};
   VkRect2D saved = {{123,45},{600,400}};
   memset(&vk,0,sizeof(vk)); vk.context=&context; vk.chain=&chain;
   call.pipeline=1; call.uniform=&uniform; call.uniform_size=sizeof(uniform);
   call.vbo=&vbo; call.vertices=4;
   /* The reported 4K case, then a smaller offscreen display target. */
   for(i=0;i<2;i++)
   {
      unsigned w=i?1280:3840, h=i?720:2160;
      vk.display_dims=VIDEO_SCALE_PACK(w,h);
      vk.vp.pos=i?VIDEO_SCALE_PACK(0,h/8):VIDEO_SCALE_PACK(w/8,0);
      vk.vp.dims=i?VIDEO_SCALE_PACK(w,h*3/4):VIDEO_SCALE_PACK(w*3/4,h);
      vk.tracker.scissor=saved; vk.flags=0;
      MENU_DRAW(&vk,&call);
      assert(recorded.offset.x==0 && recorded.offset.y==0);
      assert(recorded.extent.width==w && recorded.extent.height==h);
      assert(!vk.flags && !memcmp(&vk.tracker.scissor,&saved,sizeof(saved)));
      assert(vk.tracker.dirty & VULKAN_DIRTY_DYNAMIC_BIT);
      /* The following core draw still uses its own viewport clip. */
      vulkan_draw_triangles(&vk,&call);
      assert(recorded.offset.x==(int)VIDEO_POS_X(vk.vp.pos)
            && recorded.offset.y==(int)VIDEO_POS_Y(vk.vp.pos)
            && recorded.extent.width==VIDEO_SCALE_W(vk.vp.dims)
            && recorded.extent.height==VIDEO_SCALE_H(vk.vp.dims));
      /* Explicit menu/list clipping remains intact. */
      vk.flags=VK_FLAG_TRACKER_USE_SCISSOR;
      vk.tracker.dirty=VULKAN_DIRTY_DYNAMIC_BIT;
      MENU_DRAW(&vk,&call);
      assert(!memcmp(&recorded,&saved,sizeof(saved)));
      assert(vk.flags==VK_FLAG_TRACKER_USE_SCISSOR);
      /* Allocation failure still restores the previous clip policy. */
      vk.flags=0; fail_alloc=true;
      MENU_DRAW(&vk,&call);
      assert(!vk.flags && !memcmp(&vk.tracker.scissor,&saved,sizeof(saved)));
      fail_alloc=false;
   }
   assert(rendered==6);
   puts("PASS: menu covers letterboxed edges, explicit clips and core state preserved");
   return 0;
}
'''
cc = shlex.split(os.environ.get("CC", "cc"))
flags = shlex.split(os.environ.get("CFLAGS", ""))
with tempfile.TemporaryDirectory(prefix="menu-scissor-") as directory:
    src = Path(directory) / "test.c"
    exe = Path(directory) / ("test.exe" if "mingw" in cc[0] else "test")
    for negative in (False, True):
        text = fixture.replace("FUNCTIONS", functions).replace(
            "MENU_DRAW", "vulkan_draw_triangles" if negative else "vulkan_display_draw_triangles")
        src.write_text(text, encoding="utf-8")
        subprocess.run(cc + flags + ["-std=c89", "-O1", "-Wall", "-Wextra", "-Werror",
                       "-Wno-unused-function", str(src), "-o", str(exe)], check=True)
        result = subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)],
                                capture_output=True, text=True)
        if negative:
            assert result.returncode, "old viewport clipping escaped the regression"
            print("PASS: old core-viewport clipping is rejected")
        else:
            assert result.returncode == 0, result.stdout + result.stderr
            print(result.stdout.strip())
