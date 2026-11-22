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
#include <stdlib.h>
#include <string.h>
#include "gfx/common/media_history.h"
#include "gfx/common/media_history_ring.h"
#include "gfx/video_defines.h"
#define gl3_filter_chain chain
#define vulkan_filter_chain chain
#define gl3_filter_chain_texture texture
#define vulkan_filter_chain_texture texture
typedef struct texture {
   unsigned image,view,layout,dims,padded_dims,origin,format;
} texture;
typedef struct { texture texture;unsigned filter,mip_filter,address; } Texture;
typedef Texture gl3_texture_t;
struct pass_stub { struct { unsigned source_filter,mip_filter,address; } pass_info; };
#ifdef TEST_GL
static unsigned gl3_pass_get_source_filter(struct pass_stub *p) { (void)p;return 0; }
static unsigned gl3_pass_get_mip_filter(struct pass_stub *p) { (void)p;return 0; }
static unsigned gl3_pass_get_address_mode(struct pass_stub *p) { (void)p;return 0; }
#define INFO gl3_chain_update_history_info
#else
#define INFO slang_chain_update_history_info
#endif

typedef unsigned VkImageLayout;
typedef unsigned VkCommandBuffer;
#define VK_IMAGE_LAYOUT_GENERAL 1
#define VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL 2
#define VK_FORMAT_UNDEFINED 0
#define VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL 3
typedef struct { unsigned dims,layout,pixels[64]; } gpu;
static gpu images[128];
static unsigned allocated,copies,barriers,errors;
#ifdef TEST_GL
typedef unsigned GLuint;
static unsigned deleted[128];
static void glDeleteTextures(unsigned count, const unsigned *ids)
{ unsigned i;for(i=0;i<count;i++){assert(ids[i] && !deleted[ids[i]]);deleted[ids[i]]++;} }
struct gl3_history_lease { texture texture;bool retired; };
#endif
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
struct framebuffer { unsigned size_dims,format,image,framebuffer,view;bool complete; };
#define gl3_framebuffer framebuffer
#define slang_framebuffer framebuffer
struct deferred_disposes { int unused; };
VULKAN_LEASE_TYPE
struct chain {
   texture input_texture,history_input;
   bool history_input_valid,require_clear,frame_in_place;
#ifdef TEST_GL
   bool borrow_input;
   struct gl3_history_lease *history_leases;
#endif
#ifndef TEST_GL
   struct vulkan_history_lease input_lease,*history_leases;
#endif
   gfx_media_history_t *media_history;
   gfx_media_history_ring_t history_ring;
   struct framebuffer **original_history;
   struct pass_stub *passes[1];
   struct framebuffer *copy_framebuffer;
   size_t num_original_history,num_history;
   unsigned current_sync_index;
   struct deferred_disposes deferred_calls[1];
   struct { unsigned quad_program,partial_vbo,quad_vbo;
      struct { unsigned flat_ubo_vertex; } quad_loc;
      Texture original_history[3];size_t num_original_history; } common;
};
typedef struct chain gl3_filter_chain_t;
typedef struct chain vulkan_filter_chain_t;
static void resize(struct framebuffer *f,unsigned dims,unsigned format)
{
   f->size_dims=dims;f->format=format;f->complete=!fail_resize;
   if(f->complete)f->view=f->framebuffer=f->image=image_new(dims);
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
static bool gl3_framebuffer_preserve(struct framebuffer *fb, const texture *t)
{
   if(!fb->complete || fb->size_dims!=t->dims || fb->format!=t->format)
      resize(fb,t->dims,t->format);
   if(!fb->complete)return false;
   copy_pixels(fb->image,t->image,VIDEO_SCALE_W(t->dims),VIDEO_SCALE_H(t->dims),0,0);
   return true;
}
#define API gl3_filter_chain_set_input_texture_history
#define NORMAL gl3_filter_chain_set_input_texture
#else
static void fake_error(const char *fmt,...) { (void)fmt;errors++; }
#define RARCH_ERR fake_error
static bool slang_framebuffer_set_size(struct framebuffer *f,struct deferred_disposes *d,
      unsigned dims,unsigned format)
{ (void)d;if(fail_resize)return false;resize(f,dims,format);return f->complete; }
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
typedef struct { gfx_media_history_t media_history;uintptr_t media_last_texture,media_history_texture,media_borrow_texture; } gl3_t;
#else
typedef struct { gfx_media_history_t media_history[2];uintptr_t media_last_texture,media_history_texture[2]; } vk_t;
#endif
PRODUCER
LEASES
INPUT
NORMAL_SETTER
HISTORY_SETTER
HISTORY
END
HISTORY_INFO
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
   assert(copies==before+1 && h->copies==1);
}
static void check_pixels(struct chain *c,texture *t)
{
   unsigned i,row,col,w=VIDEO_SCALE_W(t->dims),h=VIDEO_SCALE_H(t->dims);
   unsigned x=VIDEO_SCALE_W(t->origin),y=VIDEO_SCALE_H(t->origin);
   INFO(c);
   for(i=0;i<3;i++)for(row=0;row<h;row++)for(col=0;col<w;col++)
      assert(images[c->common.original_history[i].texture.image].pixels[row*w+col]==
            images[t->image].pixels[(row+y)*VIDEO_SCALE_W(images[t->image].dims)+col+x]);
}
static void test_ring(void)
{
   static unsigned pixels[129],expected[129],seen[129];
   unsigned count,frame,i,slot,version;
   gfx_media_history_ring_t r;
   for(count=1;count<=129;count++)
   {
      void **owned=(void**)calloc(count,sizeof(*owned)+2*sizeof(unsigned));
      assert(owned);
      gfx_media_history_ring_init(&r,(unsigned*)(owned+count),count);
      memset(pixels,0,sizeof(pixels));memset(expected,0,sizeof(expected));
      for(i=0;i<count;i++)owned[i]=&pixels[i];
      version=0;
      for(frame=0;frame<count*3+9;frame++)
      {
         bool repeated=frame%7!=0;
         if(!repeated)version++;
         if(frame==10)r.valid=false;
         slot=gfx_media_history_ring_target(&r,repeated);
         if(!repeated || !r.valid)
         {
            assert(!r.refs[slot] || (slot==gfx_media_history_ring_get(&r,count-1) && r.refs[slot]==1));
            pixels[slot]=version;
         }
         gfx_media_history_ring_advance(&r,slot);
         for(i=count-1;i;i--)expected[i]=expected[i-1];
         expected[0]=version;
         memset(seen,0,sizeof(seen));
         for(i=0;i<count;i++)
         {
            unsigned physical=gfx_media_history_ring_get(&r,i);
            assert(pixels[physical]==expected[i]);seen[physical]++;
            assert(owned[i]==&pixels[i]);
         }
         for(i=0;i<count;i++)assert(r.refs[i]==seen[i]);
      }
      free(owned);
   }
}

#ifdef TEST_GL
static void test_borrowed_history(void)
{
   struct chain c;
   struct pass_stub pass;
   struct framebuffer frames[3],*slots[3];
   struct gl3_history_lease leases[3];
   gfx_media_history_t h;
   texture t;
   unsigned storage[6],i,j,frame,repeat,before,names,expected[3]={0,0,0};
   memset(&c,0,sizeof(c));memset(&pass,0,sizeof(pass));
   memset(&h,0,sizeof(h));memset(&t,0,sizeof(t));
   memset(frames,0,sizeof(frames));memset(leases,0,sizeof(leases));
   for(i=0;i<3;i++)slots[i]=&frames[i];
   c.original_history=slots;c.num_original_history=3;c.common.num_original_history=3;
   c.passes[0]=&pass;c.frame_in_place=true;c.history_leases=leases;
   gfx_media_history_ring_init(&c.history_ring,storage,3);
   gl3_filter_chain_set_history_borrow(&c,true);assert(c.borrow_input);
   h.managed=true;t.dims=t.padded_dims=VIDEO_SCALE_PACK(4,3);t.format=2;
   t.image=image_new(t.dims);images[t.image].pixels[0]=77;
   before=copies;names=allocated;
   for(i=0;i<6;i++)draw(&c,&t,&h);
   assert(copies==before && allocated==names);INFO(&c);
   for(i=0;i<3;i++)assert(c.common.original_history[i].texture.image==t.image);
   for(i=0;i<3;i++)expected[i]=77;
   for(frame=1;frame<=16;frame++)
   {
      unsigned old=t.image;
      if(frame==1)
      {
         fail_resize=true;
         assert(!gl3_filter_chain_preserve_history(&c,t.image));
         INFO(&c);assert(c.common.original_history[0].texture.image==t.image);
         fail_resize=false;
      }
      if(frame%2)
      {
         assert(gl3_filter_chain_preserve_history(&c,t.image));
         INFO(&c);
         for(i=0;i<3;i++)assert(c.common.original_history[i].texture.image!=t.image);
         images[t.image].pixels[0]=100+frame;
      }
      else
      {
         assert(gl3_filter_chain_retire_history(&c,old));
         assert(!deleted[old]);
         t.image=image_new(t.dims);images[t.image].pixels[0]=100+frame;
      }
      h.copies=0;
      before=copies;
      for(repeat=0;repeat<4;repeat++)
      {
         draw(&c,&t,&h);INFO(&c);
         assert(copies==before);
         for(j=2;j;j--)expected[j]=expected[j-1];
         expected[0]=100+frame;
         for(j=0;j<3;j++)assert(images[c.common.original_history[j].texture.image].pixels[0]==expected[j]);
      }
      if(!(frame%2))assert(deleted[old]==1);
   }
   assert(!gl3_filter_chain_retire_history(&c,0));
   /* Returning to an earlier live image can put its name in two physical
    * slots. Retirement transfers deletion until both have been evicted. */
   {
      texture other=t;
      unsigned original=t.image;
      other.image=image_new(t.dims);
      h.copies=0;draw(&c,&other,&h);
      h.copies=0;draw(&c,&t,&h);
      assert(gl3_filter_chain_retire_history(&c,original));
      h.copies=0;draw(&c,&other,&h);assert(!deleted[original]);
      draw(&c,&other,&h);assert(!deleted[original]);
      draw(&c,&other,&h);assert(deleted[original]==1);
      t=other;
   }
   /* Teardown deletes retired sources once and leaves live producer names alone. */
   gl3_chain_release_history_leases(&c,true);assert(!deleted[t.image]);
   h.copies=0;draw(&c,&t,&h);
   assert(gl3_filter_chain_retire_history(&c,t.image));
   gl3_chain_release_history_leases(&c,true);assert(deleted[t.image]==1);
   gl3_chain_release_history_leases(&c,true);assert(deleted[t.image]==1);
   puts("PASS GLCore borrowed history: no snapshot draws, update preservation, immutable readers, retire/evict and teardown");
}
#endif

#ifndef TEST_GL
struct owner { unsigned refs,retains,releases,reads; };
static void read_owner(void *p){((struct owner*)p)->reads++;}
static void retain_owner(void *p)
{struct owner *o=(struct owner*)p;o->refs++;o->retains++;}
static void release_owner(void *p)
{struct owner *o=(struct owner*)p;assert(o->refs);o->refs--;o->releases++;}
static void test_vulkan_borrowed_history(void)
{
   struct chain c;
   struct pass_stub pass;
   struct framebuffer frames[3],*slots[3];
   struct vulkan_history_lease leases[3];
   struct owner owners[2];
   gfx_media_history_t h;
   texture t,other,history;
   unsigned storage[6],i,j,before,names,old_barriers;
   memset(&c,0,sizeof(c));memset(&pass,0,sizeof(pass));
   memset(&h,0,sizeof(h));memset(&t,0,sizeof(t));
   memset(owners,0,sizeof(owners));memset(leases,0,sizeof(leases));
   memset(frames,0,sizeof(frames));
   for(i=0;i<3;i++)slots[i]=&frames[i];
   c.original_history=slots;c.num_history=3;c.common.num_original_history=3;
   c.passes[0]=&pass;c.history_leases=leases;
   gfx_media_history_ring_init(&c.history_ring,storage,3);
   assert(vulkan_filter_chain_uses_history(&c));
   h.managed=true;t.dims=t.padded_dims=VIDEO_SCALE_PACK(4,3);t.format=2;
   t.layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   t.image=image_new(t.dims);images[t.image].layout=t.layout;images[t.image].pixels[0]=77;t.view=10;
   history=t;history.view=11;
   vulkan_filter_chain_set_history_lease(&c,&history,&owners[0],retain_owner,release_owner);
   assert(owners[0].refs==1);
   before=copies;names=allocated;old_barriers=barriers;
   for(i=0;i<12;i++)draw(&c,&t,&h);
   assert(copies==before && allocated==names && barriers==old_barriers);
   INFO(&c);
   for(i=0;i<3;i++)
   {
      assert(c.common.original_history[i].texture.image==t.image);
      assert(c.common.original_history[i].texture.view==11);
   }
   vulkan_filter_chain_visit_history(&c,read_owner);
   assert(owners[0].reads==owners[0].refs);
   assert(owners[0].refs==2); /* repeated logical entries share one physical lease */
   other=t;other.image=image_new(t.dims);images[other.image].layout=t.layout;images[other.image].pixels[0]=88;
   for(i=0;i<16;i++)
   {
      texture *next=i%2?&t:&other;
      unsigned index=i%2?0:1;
      history=*next;history.view=next->view+1;
      vulkan_filter_chain_set_history_lease(&c,&history,&owners[index],retain_owner,release_owner);
      h.copies=0;draw(&c,next,&h);INFO(&c);
      assert(copies==before);
      assert(c.common.original_history[0].texture.image==next->image);
      for(j=0;j<2;j++)
      {
         unsigned refs=c.input_lease.owner==&owners[j];
         unsigned k;
         for(k=0;k<3;k++)refs+=leases[k].owner==&owners[j];
         assert(owners[j].refs==refs);
      }
   }
   /* Switching to a mutable source cannot reuse an immutable alias as
    * though its owned framebuffer already held that source. */
   vulkan_filter_chain_set_history_lease(&c,NULL,NULL,NULL,NULL);
   draw(&c,&t,&h);assert(copies==before+1);
   INFO(&c);assert(!c.common.original_history[0].texture.padded_dims);
   slang_chain_release_history_leases(&c,true);
   slang_chain_release_history_leases(&c,true);
   assert(!owners[0].refs && !owners[1].refs);
   c.num_history=0;assert(!vulkan_filter_chain_uses_history(&c));
   for(i=0;i<2;i++)assert(owners[i].retains==owners[i].releases);
   puts("PASS Vulkan immutable history: original image/level-zero view, no copies or barriers, aliases, ownership and mutable fallback");
}
#endif

int main(void)
{
#ifdef TEST_GL
   gl3_t driver;
#else
   vk_t driver;
#endif
   struct chain c;
   struct pass_stub pass;
   struct framebuffer frames[3],*slots[3];
   gfx_media_history_t h;
   texture t;
   unsigned i,before,oldest,old_barriers,storage[6];
   test_ring();
   memset(&driver,0,sizeof(driver));
#ifdef TEST_GL
   driver.media_history_texture=42;driver.media_history.copies=3;
   gl3_media_texture_changed(&driver,42,true);
   assert(!driver.media_history.copies && driver.media_last_texture==42);
   driver.media_history.copies=3;gl3_media_texture_changed(&driver,99,true);
   assert(driver.media_history.copies==3 && driver.media_last_texture==99);
   gl3_media_texture_changed(&driver,99,false);assert(!driver.media_last_texture);
   driver.media_borrow_texture=42;
   gl3_media_texture_changed(&driver,42,true);assert(driver.media_borrow_texture==42);
   gl3_media_texture_changed(&driver,42,false);
   assert(!driver.media_history.copies && !driver.media_borrow_texture);
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
   gfx_media_history_ring_init(&c.history_ring,storage,3);
   memset(&pass,0,sizeof(pass));c.passes[0]=&pass;
   c.common.num_original_history=3;
   c.require_clear=true;c.frame_in_place=true;
   t.dims=VIDEO_SCALE_PACK(4,3);t.padded_dims=VIDEO_SCALE_PACK(8,8);
   t.origin=VIDEO_SCALE_PACK(2,1);t.format=1;t.view=1;
   t.image=image_new(t.padded_dims);
   for(i=0;i<64;i++)images[t.image].pixels[i]=i+100;
   h.managed=true;
   warm(&c,&t,&h);check_pixels(&c,&t);
   before=copies;old_barriers=barriers;
   oldest=c.history_ring.head;
   draw(&c,&t,&h);assert(copies==before && barriers==old_barriers);
   assert(c.history_ring.head!=oldest);
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
   {
      unsigned version,repeat,j,expected[3],names;
      INFO(&c);
      for(j=0;j<3;j++)expected[j]=images[c.common.original_history[j].texture.image].pixels[0];
      for(version=1;version<=24;version++)
      {
         for(j=0;j<64;j++)images[t.image].pixels[j]=1000+version;
         h.copies=0;
         for(repeat=0;repeat<version%5+1;repeat++)
         {
            before=copies;old_barriers=barriers;names=allocated;
            draw(&c,&t,&h);INFO(&c);
            assert(copies==before+(repeat?0:1));
            if(repeat)assert(barriers==old_barriers && allocated==names);
            for(j=2;j;j--)expected[j]=expected[j-1];
            expected[0]=1000+version;
            for(j=0;j<3;j++)
               assert(images[c.common.original_history[j].texture.image].pixels[0]==expected[j]);
         }
      }
   }
   before=copies;h.managed=false;
   for(i=0;i<6;i++)draw(&c,&t,&h);
   assert(copies==before+6);h.managed=true;h.copies=0;warm(&c,&t,&h);
   t.dims=VIDEO_SCALE_PACK(3,2);fail_resize=true;before=copies;
   draw(&c,&t,&h);assert(copies==before && h.copies==0);
   fail_resize=false;
   for(i=0;i<12;i++)draw(&c,&t,&h);
   assert(h.copies==1);check_pixels(&c,&t);
#ifdef TEST_GL
   c.frame_in_place=false;before=copies;draw(&c,&t,&h);
   assert(c.copy_framebuffer && copies>before);check_pixels(&c,&t);
   before=copies;draw(&c,&t,&h);assert(copies==before); /* crop also reused */
   h.copies=0;before=copies;
   for(i=0;i<6;i++)draw(&c,&t,&h);
   assert(copies==before+1); /* the crop itself becomes the owned snapshot */
   {
      unsigned version,repeat,j,k,expected[3],names;
      struct framebuffer *owned[4];
      for(j=0;j<4;j++) { h.copies=0;draw(&c,&t,&h); }
      INFO(&c);
      for(j=0;j<3;j++) {
         owned[j]=c.original_history[j];
         expected[j]=images[c.common.original_history[j].texture.image].pixels[0];
      }
      owned[3]=c.copy_framebuffer;
      for(version=1;version<=24;version++)
      {
         for(j=0;j<64;j++)images[t.image].pixels[j]=2000+version;
         h.copies=0;
         for(repeat=0;repeat<version%5+1;repeat++)
         {
            before=copies;names=allocated;
            draw(&c,&t,&h);INFO(&c);
            assert(copies==before+(repeat?0:1));
            assert(allocated==names);
            for(j=2;j;j--)expected[j]=expected[j-1];
            expected[0]=2000+version;
            for(j=0;j<3;j++) {
               assert(images[c.common.original_history[j].texture.image].pixels[0]==expected[j]);
               assert(c.original_history[j]!=c.copy_framebuffer);
               for(k=0;k<j;k++)assert(c.original_history[j]!=c.original_history[k]);
            }
            for(j=0;j<4;j++) {
               unsigned count=c.copy_framebuffer==owned[j];
               for(k=0;k<3;k++)count+=c.original_history[k]==owned[j];
               assert(count==1); /* each physical framebuffer has one owner */
            }
         }
      }
      before=copies;h.managed=false;
      for(j=0;j<4;j++)draw(&c,&t,&h);
      assert(copies==before+4);
      h.managed=true;h.copies=0;warm(&c,&t,&h);
   }
   before=copies;
   for(i=0;i<4;i++) { NORMAL(&c,&t);end_frame(&c); }
   assert(copies==before+4); /* ordinary core views transfer the crop too */
   warm(&c,&t,&h);check_pixels(&c,&t);
   c.copy_framebuffer->complete=false;fail_resize=true;
   draw(&c,&t,&h);assert(h.copies==0);
   fail_resize=false;
   for(i=0;i<6;i++)draw(&c,&t,&h);
   assert(c.copy_framebuffer->complete && h.copies==1);check_pixels(&c,&t);
#else
   assert(errors==1);
#endif
   c.num_original_history=c.num_history=0;before=copies;
   for(i=0;i<3;i++)draw(&c,&t,&h);
#ifdef TEST_GL
   assert(copies==before+1 && c.input_texture.image==c.copy_framebuffer->image);
   {
      unsigned version,repeat,j,row,col;
      h.copies=0;
      for(version=1;version<=24;version++)
      {
         for(j=0;j<64;j++)images[t.image].pixels[j]=3000+version+j;
         h.copies=0;
         for(repeat=0;repeat<5;repeat++)
         {
            before=copies;oldest=allocated;
            draw(&c,&t,&h);
            assert(copies==before+(repeat?0:1) && allocated==oldest);
            assert(c.input_texture.image==c.copy_framebuffer->image);
            for(row=0;row<VIDEO_SCALE_H(t.dims);row++)
               for(col=0;col<VIDEO_SCALE_W(t.dims);col++)
                  assert(images[c.input_texture.image].pixels[row*VIDEO_SCALE_W(t.dims)+col]
                        ==images[t.image].pixels[(row+VIDEO_SCALE_H(t.origin))*8
                              +col+VIDEO_SCALE_W(t.origin)]);
         }
      }
      t.origin=VIDEO_SCALE_PACK(2,1);warm(&c,&t,&h);
      t.padded_dims=VIDEO_SCALE_PACK(8,8);warm(&c,&t,&h);
      t.dims=VIDEO_SCALE_PACK(4,2);warm(&c,&t,&h);
      t.format++;warm(&c,&t,&h);
      t.image=image_new(VIDEO_SCALE_PACK(8,8));warm(&c,&t,&h);
      memset(&h,0,sizeof(h));h.managed=true;warm(&c,&t,&h);
      c.history_input_valid=false;warm(&c,&t,&h);
      c.require_clear=true;warm(&c,&t,&h);
      t.dims=VIDEO_SCALE_PACK(2,2);fail_resize=true;before=copies;
      draw(&c,&t,&h);assert(copies==before && !h.copies);
      fail_resize=false;warm(&c,&t,&h);
      before=copies;h.managed=false;
      for(j=0;j<4;j++)draw(&c,&t,&h);
      assert(copies==before+4);
      h.managed=true;h.copies=0;warm(&c,&t,&h);
      before=copies;
      for(j=0;j<4;j++) { NORMAL(&c,&t);end_frame(&c); }
      assert(copies==before+4);
      c.frame_in_place=true;h.copies=0;before=copies;
      for(j=0;j<4;j++)draw(&c,&t,&h);
      assert(copies==before && !h.copies && c.input_texture.image==t.image);
   }
#else
   assert(copies==before);
#endif
#ifdef TEST_GL
   test_borrowed_history();
#else
   test_vulkan_borrowed_history();
#endif
   (void)before;(void)errors;
   puts("PASS Slang history: pixels, moving/repeated cadence, logical bindings, crop/format/source changes, no repeat copies/barriers, fallback and failures");
   return 0;
}
'''

cc = shlex.split(os.environ.get("CC", "cc"))
flags = shlex.split(os.environ.get("CFLAGS", ""))
for api, filename, prefix in (("gl3", "shader_gl3.c", "gl3"), ("vulkan", "shader_vulkan.c", "slang")):
    source = (root / "gfx/drivers_shader" / filename).read_text(encoding="utf-8")
    producer = (root / "gfx/drivers" / ("gl3.c" if api == "gl3" else "vulkan.c")).read_text(encoding="utf-8")
    lease_type = ""
    if api == "vulkan":
        start = source.index("struct vulkan_history_lease\n{")
        lease_type = source[start:source.index("\n};", start) + 3]
    unit = fixture.replace("VULKAN_LEASE_TYPE", lease_type).replace("\nPRODUCER\n", "\n" + function(producer,
        "static void " + api + "_media_texture_changed(") + "\n")
    leases = "\n"
    if api == "gl3":
        leases += "\n".join(function(source, signature) for signature in (
            "static void gl3_chain_release_history_slot(",
            "static void gl3_chain_release_history_leases(",
            "bool gl3_filter_chain_preserve_history(",
            "bool gl3_filter_chain_retire_history(",
            "void gl3_filter_chain_set_history_borrow("))
    else:
        leases += "\n".join(function(source, signature) for signature in (
            "static void slang_history_lease_release(",
            "static void slang_chain_release_history_leases(",
            "void vulkan_filter_chain_set_history_lease(",
            "void vulkan_filter_chain_visit_history(",
            "bool vulkan_filter_chain_uses_history("))
    unit = unit.replace("\nLEASES\n", leases + "\n")
    unit = unit.replace("\nINPUT\n", "\n" + function(source, "static void " + prefix + "_chain_set_input_texture(") + "\n")
    unit = unit.replace("\nNORMAL_SETTER\n", "\n" + function(source, "void " + api + "_filter_chain_set_input_texture(") + "\n")
    unit = unit.replace("\nHISTORY_SETTER\n", "\n" + function(source, "void " + api + "_filter_chain_set_input_texture_history(") + "\n")
    unit = unit.replace("\nHISTORY_INFO\n", "\n" + function(source, "static void " + prefix + "_chain_update_history_info(") + "\n")
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
