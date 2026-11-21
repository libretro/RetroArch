#!/usr/bin/env python3
"""Run the shipping Slang history/input paths against counted GPU operations."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]


def function(source, signature):
    match = re.search(re.escape(signature) + r"[^;]*?\n\{", source)
    assert match, signature
    return source[match.start():source.index("\n}\n", match.end()) + 3]


fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "gfx/common/media_history.h"
#include "gfx/video_defines.h"
#define gl3_filter_chain chain
#define vulkan_filter_chain chain
#define gl3_filter_chain_texture texture
#define vulkan_filter_chain_texture texture
typedef struct texture {
   unsigned image,view,layout,dims,padded_dims,origin,format;
} texture;
typedef unsigned VkImageLayout;
typedef unsigned VkCommandBuffer;
#define VK_IMAGE_LAYOUT_GENERAL 1
#define VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL 2
#define VK_FORMAT_UNDEFINED 0
typedef struct { unsigned dims,layout,pixels[64]; } gpu;
static gpu images[128];
static unsigned allocated,copies,barriers,errors;
static bool fail_resize;
static unsigned image_new(unsigned dims)
{
   unsigned id=++allocated;assert(id<128);images[id].dims=dims;return id;
}
static void copy_pixels(unsigned to,unsigned from,unsigned w,unsigned h,unsigned x,unsigned y)
{
   unsigned row,col,stride=VIDEO_SCALE_W(images[from].dims);
   assert(w*h<=64 && x+w<=stride && y+h<=VIDEO_SCALE_H(images[from].dims));
   for(row=0;row<h;row++)for(col=0;col<w;col++)
      images[to].pixels[row*w+col]=images[from].pixels[(row+y)*stride+col+x];
   copies++;
}
struct framebuffer { unsigned size_dims,format,image,framebuffer;bool complete; };
#define gl3_framebuffer framebuffer
#define slang_framebuffer framebuffer
struct deferred_disposes { int unused; };
struct chain {
   texture input_texture,history_input;
   bool history_input_valid,require_clear,frame_in_place;
   gfx_media_history_t *media_history;
   struct framebuffer **original_history;
   struct framebuffer *copy_framebuffer;
   size_t num_original_history,num_history;
   unsigned history_ring_index,current_sync_index;
   struct deferred_disposes deferred_calls[1];
   struct { unsigned quad_program,partial_vbo,quad_vbo;
      struct { unsigned flat_ubo_vertex; } quad_loc; } common;
};
typedef struct chain gl3_filter_chain_t;
typedef struct chain vulkan_filter_chain_t;
static void resize(struct framebuffer *f,unsigned dims,unsigned format)
{
   f->size_dims=dims;f->format=format;f->complete=!fail_resize;
   if(f->complete)f->framebuffer=f->image=image_new(dims);
}
#ifdef TEST_GL
static struct framebuffer *gl3_framebuffer_new(unsigned format,unsigned levels)
{
   static struct framebuffer f;
   assert(levels==1);memset(&f,0,sizeof(f));f.format=format;return &f;
}
static void gl3_framebuffer_set_size(struct framebuffer *f,unsigned dims,unsigned format)
{ resize(f,dims,format); }
static void gl3_framebuffer_copy_partial(unsigned f,unsigned p,unsigned b,unsigned u,
      unsigned w,unsigned h,unsigned from,float x,float y,float sx,float sy)
{
   (void)p;(void)b;(void)u;(void)sx;(void)sy;
   copy_pixels(f,from,w,h,(unsigned)(x*VIDEO_SCALE_W(images[from].dims)),
         (unsigned)(y*VIDEO_SCALE_H(images[from].dims)));
}
static void gl3_framebuffer_copy(unsigned f,unsigned p,unsigned b,unsigned u,
      unsigned w,unsigned h,unsigned from)
{ (void)p;(void)b;(void)u;copy_pixels(f,from,w,h,0,0); }
#define API gl3_filter_chain_set_input_texture_history
#define NORMAL gl3_filter_chain_set_input_texture
#else
static void fake_error(const char *fmt,...) { (void)fmt;errors++; }
#define RARCH_ERR fake_error
static bool slang_framebuffer_set_size(struct framebuffer *f,struct deferred_disposes *d,
      unsigned dims,unsigned format)
{ (void)d;resize(f,dims,format);return f->complete; }
static void transition(unsigned image,unsigned old_layout,unsigned new_layout)
{ assert(images[image].layout==old_layout);images[image].layout=new_layout;barriers++; }
#define VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(cmd,img,levels,old,next,sa,da,ss,ds,sq,dq) \
   transition(img,old,next)
static void vulkan_framebuffer_copy(unsigned to,unsigned w,unsigned h,unsigned cmd,
      unsigned from,unsigned layout,unsigned origin)
{ (void)cmd;assert(images[from].layout==layout);
   copy_pixels(to,from,w,h,VIDEO_SCALE_W(origin),VIDEO_SCALE_H(origin)); }
#define API vulkan_filter_chain_set_input_texture_history
#define NORMAL vulkan_filter_chain_set_input_texture
#endif
#ifdef TEST_GL
typedef struct { gfx_media_history_t media_history;uintptr_t media_last_texture,media_history_texture; } gl3_t;
#else
typedef struct { gfx_media_history_t media_history[2];uintptr_t media_last_texture,media_history_texture[2]; } vk_t;
#endif
PRODUCER
INPUT
NORMAL_SETTER
HISTORY_SETTER
HISTORY
END
static void end_frame(struct chain *c)
{
#ifdef TEST_GL
   gl3_chain_end_frame(c);
#else
   slang_chain_end_frame(c,1);
#endif
   assert(!c->media_history);
}
static void draw(struct chain *c,texture *t,gfx_media_history_t *h)
{ API(c,t,h);c->require_clear=false;end_frame(c); }
static void warm(struct chain *c,texture *t,gfx_media_history_t *h)
{
   unsigned i,before=copies;
   for(i=0;i<6;i++)draw(c,t,h);
   assert(copies==before+3 && h->copies==3);
}
static void check_pixels(struct chain *c,texture *t)
{
   unsigned i,row,col,w=VIDEO_SCALE_W(t->dims),h=VIDEO_SCALE_H(t->dims);
   unsigned x=VIDEO_SCALE_W(t->origin),y=VIDEO_SCALE_H(t->origin);
   for(i=0;i<3;i++)for(row=0;row<h;row++)for(col=0;col<w;col++)
      assert(images[c->original_history[i]->image].pixels[row*w+col]==
            images[t->image].pixels[(row+y)*VIDEO_SCALE_W(images[t->image].dims)+col+x]);
}
int main(void)
{
#ifdef TEST_GL
   gl3_t driver;
#else
   vk_t driver;
#endif
   struct chain c;
   struct framebuffer frames[3],*slots[3];
   gfx_media_history_t h;
   texture t;
   unsigned i,before,oldest,old_barriers;
   memset(&driver,0,sizeof(driver));
#ifdef TEST_GL
   driver.media_history_texture=42;driver.media_history.copies=3;
   gl3_media_texture_changed(&driver,42,true);
   assert(!driver.media_history.copies && driver.media_last_texture==42);
   driver.media_history.copies=3;gl3_media_texture_changed(&driver,99,true);
   assert(driver.media_history.copies==3 && driver.media_last_texture==99);
   gl3_media_texture_changed(&driver,99,false);assert(!driver.media_last_texture);
   gl3_media_texture_changed(&driver,42,false);assert(!driver.media_history.copies);
#else
   for(i=0;i<2;i++) { driver.media_history_texture[i]=42;driver.media_history[i].copies=3; }
   vulkan_media_texture_changed(&driver,42,true);
   assert(!driver.media_history[0].copies && !driver.media_history[1].copies && driver.media_last_texture==42);
   driver.media_history[0].copies=3;vulkan_media_texture_changed(&driver,99,true);
   assert(driver.media_history[0].copies==3 && driver.media_last_texture==99);
   vulkan_media_texture_changed(&driver,99,false);assert(!driver.media_last_texture);
   vulkan_media_texture_changed(&driver,42,false);assert(!driver.media_history[0].copies);
#endif
   memset(&c,0,sizeof(c));memset(&h,0,sizeof(h));memset(&t,0,sizeof(t));
   memset(frames,0,sizeof(frames));
   for(i=0;i<3;i++)slots[i]=&frames[i];
   c.original_history=slots;c.num_history=c.num_original_history=3;
   c.require_clear=true;c.frame_in_place=true;
   t.dims=VIDEO_SCALE_PACK(4,3);t.padded_dims=VIDEO_SCALE_PACK(8,8);
   t.origin=VIDEO_SCALE_PACK(2,1);t.format=1;t.view=1;
   t.image=image_new(t.padded_dims);
   for(i=0;i<64;i++)images[t.image].pixels[i]=i+100;
   h.managed=true;
   warm(&c,&t,&h);check_pixels(&c,&t);
   before=copies;old_barriers=barriers;
#ifdef TEST_GL
   oldest=c.original_history[0]->image;
#else
   oldest=c.history_ring_index;
#endif
   draw(&c,&t,&h);assert(copies==before && barriers==old_barriers);
#ifdef TEST_GL
   assert(c.original_history[0]->image!=oldest);
#else
   assert(c.history_ring_index!=oldest);
#endif
   images[t.image].pixels[10]=999;h.copies=0;warm(&c,&t,&h);check_pixels(&c,&t);
   t.origin=VIDEO_SCALE_PACK(1,2);warm(&c,&t,&h);check_pixels(&c,&t);
   t.padded_dims=VIDEO_SCALE_PACK(8,7);warm(&c,&t,&h);
   t.format=2;warm(&c,&t,&h);check_pixels(&c,&t);
   t.image=image_new(VIDEO_SCALE_PACK(8,8));warm(&c,&t,&h);check_pixels(&c,&t);
#ifndef TEST_GL
   t.view++;warm(&c,&t,&h);
   t.layout=images[t.image].layout=VK_IMAGE_LAYOUT_GENERAL;
   old_barriers=barriers;warm(&c,&t,&h);assert(barriers==old_barriers);
#endif
   c.history_input_valid=false;warm(&c,&t,&h); /* new chain at an old address */
   c.require_clear=true;warm(&c,&t,&h);
   before=copies;NORMAL(&c,&t);assert(!c.media_history);end_frame(&c);
   assert(copies==before+1);warm(&c,&t,&h);
   before=copies;h.managed=false;
   for(i=0;i<6;i++)draw(&c,&t,&h);
   assert(copies==before+6);h.managed=true;h.copies=0;warm(&c,&t,&h);
   t.dims=VIDEO_SCALE_PACK(3,2);fail_resize=true;before=copies;
   draw(&c,&t,&h);assert(copies==before && h.copies==0);
   fail_resize=false;
   for(i=0;i<12;i++)draw(&c,&t,&h);
   assert(h.copies==3);check_pixels(&c,&t);
#ifdef TEST_GL
   c.frame_in_place=false;before=copies;draw(&c,&t,&h);
   assert(c.copy_framebuffer && copies>before);check_pixels(&c,&t);
   before=copies;draw(&c,&t,&h);assert(copies==before); /* crop also reused */
   h.copies=0;before=copies;
   for(i=0;i<6;i++)draw(&c,&t,&h);
   assert(copies==before+6); /* crop and history each need three writes */
   c.copy_framebuffer->complete=false;fail_resize=true;
   draw(&c,&t,&h);assert(h.copies<3);
   fail_resize=false;
   for(i=0;i<6;i++)draw(&c,&t,&h);
   assert(c.copy_framebuffer->complete && h.copies==3);check_pixels(&c,&t);
#else
   assert(errors==1);
#endif
   c.num_original_history=c.num_history=0;before=copies;
   for(i=0;i<3;i++)draw(&c,&t,&h);
#ifndef TEST_GL
   assert(copies==before);
#endif
   (void)before;(void)errors;
   puts("PASS Slang history: pixels, crop/format/source changes, ring rotation, no repeat copies/barriers, fallback and failures");
   return 0;
}
'''

cc = shlex.split(os.environ.get("CC", "cc"))
flags = shlex.split(os.environ.get("CFLAGS", ""))
for api, filename, prefix in (("gl3", "shader_gl3.c", "gl3"), ("vulkan", "shader_vulkan.c", "slang")):
    source = (root / "gfx/drivers_shader" / filename).read_text(encoding="utf-8")
    producer = (root / "gfx/drivers" / ("gl3.c" if api == "gl3" else "vulkan.c")).read_text(encoding="utf-8")
    unit = fixture.replace("\nPRODUCER\n", "\n" + function(producer,
        "static void " + api + "_media_texture_changed(") + "\n")
    unit = unit.replace("\nINPUT\n", "\n" + function(source, "static void " + prefix + "_chain_set_input_texture(") + "\n")
    unit = unit.replace("\nNORMAL_SETTER\n", "\n" + function(source, "void " + api + "_filter_chain_set_input_texture(") + "\n")
    unit = unit.replace("\nHISTORY_SETTER\n", "\n" + function(source, "void " + api + "_filter_chain_set_input_texture_history(") + "\n")
    unit = unit.replace("\nEND\n", "\n" + function(source, "static void " + prefix + "_chain_end_frame(") + "\n")
    unit = unit.replace("\nHISTORY\n", "\n" if api == "gl3" else "\n" + function(source, "static void slang_chain_update_history(") + "\n")
    dialect = ["-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror", "-Werror=declaration-after-statement"]
    if any("clang" in word for word in cc):
        dialect += ["-Wno-c99-extensions", "-Wno-c11-extensions"]
    with tempfile.TemporaryDirectory(prefix="slang-history-") as directory:
        path = Path(directory) / "test.c"
        exe = Path(directory) / ("test.exe" if os.name == "nt" or "mingw" in " ".join(cc) else "test")
        path.write_text(unit, encoding="utf-8")
        subprocess.run(cc + flags + dialect + (["-DTEST_GL", "-DHAVE_SLANG"] if api == "gl3" else []) + [
            "-I" + str(root), "-I" + str(root / "libretro-common/include"), str(path), "-o", str(exe)], check=True)
        subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
