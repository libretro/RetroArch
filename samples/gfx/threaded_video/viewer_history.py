#!/usr/bin/env python3
"""Execute the shipping D3D11/D3D12/Metal history paths against counted GPU copies."""
import argparse
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index("\n}\n", start) + 3]


common = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gfx/common/media_history.h"
#include "gfx/common/media_history_ring.h"
#include "gfx/video_defines.h"
typedef struct { float x,y,z,w; } float4;
typedef struct gpu { unsigned width,height,pixelFormat,value,mipmapLevelCount,layout; } gpu;
static gpu pool[128];
static unsigned allocated,copies,renders,feedbacks;
static gpu *new_texture(unsigned width,unsigned height,unsigned format)
{
   gpu *p;
   assert(allocated<128);p=&pool[allocated++];
   p->width=width;p->height=height;p->pixelFormat=format;p->value=0;p->mipmapLevelCount=1;p->layout=1;return p;
}
struct retro_video_view { unsigned x,y,width,height; };
'''

d3d = (root / "gfx/drivers/d3d12.c").read_text(encoding="utf-8")
d3d_fixture = common + r'''
#define HAVE_SLANG 1
typedef gpu *D3D12Resource;
typedef struct command *D3D12GraphicsCommandList;
#define D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING 5768
#define D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX 0
#define D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE 1
#define D3D12_RESOURCE_STATE_COPY_SOURCE 2
#define D3D12_RESOURCE_STATE_COPY_DEST 3
typedef struct { gpu *pResource;unsigned Type,SubresourceIndex; } D3D12_TEXTURE_COPY_LOCATION;
static unsigned barriers;
static void transition(gpu *p,unsigned from,unsigned to)
{ assert(p->layout==from);p->layout=to;barriers++; }
#define D3D12_RESOURCE_TRANSITION(cmd,p,from,to) transition(p,from,to)
struct command { struct command_vtable {
   void (*CopyTextureRegion)(D3D12GraphicsCommandList,const D3D12_TEXTURE_COPY_LOCATION*,
         unsigned,unsigned,unsigned,const D3D12_TEXTURE_COPY_LOCATION*,const void*);
} *lpVtbl; };
static void copy_region(D3D12GraphicsCommandList cmd,const D3D12_TEXTURE_COPY_LOCATION *to,
      unsigned x,unsigned y,unsigned z,const D3D12_TEXTURE_COPY_LOCATION *from,const void *box)
{ (void)cmd;(void)x;(void)y;(void)z;assert(!box);to->pResource->value=from->pResource->value;copies++; }
static struct command_vtable command_api={copy_region};
static struct command command={&command_api};
typedef unsigned DXGI_FORMAT;
#define D3D12_FRAME_MAPPING_2101010 23
typedef struct {
   gfx_media_history_t media_history;
   gpu *handle,*upload_buffer,*upload_buffer2;
   struct { unsigned Width,Height,Format; } desc;
   unsigned srv_mapping,pending,upload_fence,lend_fence[2],layout;
   void *srv_heap,*planar;
   float4 size_data;
} d3d12_texture_t;
typedef struct { unsigned history_size; } preset;
typedef struct {
   preset *shader_preset;
   d3d12_texture_t history[4];
   gfx_media_history_t *media_history;
   gfx_media_history_ring_t history_ring;
   uintptr_t media_source;
   unsigned history_slot;
   bool history_shared;
   bool history_made;
   gpu *frame_res;
   unsigned rt_frame_dims,rt_vp_dims;
   float4 output_size;
   struct { float values[16]; } frame_rect;
} d3d12_filter_chain_t;
typedef struct {
   void *device;
   struct { D3D12GraphicsCommandList cmd; } queue;
   struct { void *srv_heap; } desc;
   d3d12_filter_chain_t *media_shader_chain,filter;
   d3d12_texture_t *media_shader_texture;
} d3d12_video_t;
static void slang_rect_values(float *values,unsigned dims,unsigned pos,unsigned src)
{ (void)values;(void)dims;(void)pos;(void)src; }
static void d3d12_release_texture(d3d12_video_t *v,d3d12_texture_t *t)
{ (void)v;t->handle=NULL; }
static void d3d12_init_texture(void *device,d3d12_texture_t *t)
{ (void)device;t->handle=new_texture(t->desc.Width,t->desc.Height,t->desc.Format);
  t->size_data.x=(float)t->desc.Width;t->size_data.y=(float)t->desc.Height; }
static void d3d12_frame_remap(d3d12_video_t *v,d3d12_texture_t *t,bool packed)
{ (void)v;(void)t;(void)packed; }
static void d3d12_chain_init_render_targets(d3d12_video_t *v,d3d12_filter_chain_t *c,
      unsigned w,unsigned h,struct video_viewport *vp)
{ (void)v;(void)c;(void)w;(void)h;(void)vp; }
static void d3d12_planar_record(D3D12GraphicsCommandList cmd,d3d12_texture_t *t,d3d12_video_t *v)
{ (void)cmd;(void)v;t->handle->value++; }
static void d3d12_upload_texture_from(D3D12GraphicsCommandList cmd,d3d12_texture_t *t,
      void *v,gpu *buffer,unsigned *layout,void *box)
{ (void)cmd;(void)v;(void)buffer;(void)layout;(void)box;t->handle->value++; }
HELPER
UPLOAD
FRAME
static bool draw(d3d12_video_t *v,d3d12_filter_chain_t *c,d3d12_texture_t *t)
{
   struct retro_video_view view;
   bool copy;
   d3d12_video_t *video=v;
   d3d12_filter_chain_t *chain=c;
   d3d12_texture_t *texture=t;
   uintptr_t image=(uintptr_t)t;
   unsigned dims=VIDEO_SCALE_PACK(800,600);
   unsigned source_dims;
   memset(&view,0,sizeof(view));view.width=t->desc.Width;view.height=t->desc.Height;
   source_dims=VIDEO_SCALE_PACK(view.width,view.height);
   v->media_shader_texture=t;
DRAW_BODY
   c->media_history=NULL;renders++;feedbacks++;
   return copy;
}
int main(void)
{
   d3d12_video_t v;
   d3d12_filter_chain_t c;
   d3d12_texture_t t,other;
   preset p={2};
   unsigned i,before,storage[6];
   memset(&v,0,sizeof(v));memset(&c,0,sizeof(c));memset(&t,0,sizeof(t));
   c.shader_preset=&p;v.media_shader_chain=&c;v.queue.cmd=&command;
   gfx_media_history_ring_init(&c.history_ring,storage,3);
   t.desc.Width=320;t.desc.Height=240;t.desc.Format=1;
   t.handle=new_texture(320,240,1);t.handle->value=19;t.media_history.managed=true;
   for(i=0;i<8;i++) assert(draw(&v,&c,&t)==(i==0));
   assert(copies==1 && renders==8 && feedbacks==8 && allocated==4);
   for(i=0;i<3;i++)assert(((const d3d12_texture_t*)d3d12_chain_history_data(&c,&c.history[i]))->handle->value==19);
   before=copies;d3d12_upload_texture(NULL,&t,&v);
   assert(t.media_history.copies==0);
   for(i=0;i<5;i++)assert(draw(&v,&c,&t)==(i==0));
   assert(copies==before+1);
   for(i=0;i<3;i++)assert(((const d3d12_texture_t*)d3d12_chain_history_data(&c,&c.history[i]))->handle->value==20);
   t.planar=&t;d3d12_upload_texture(NULL,&t,&v);t.planar=NULL;
   assert(t.media_history.copies==0 && draw(&v,&c,&t));
   other=t;memset(&other.media_history,0,sizeof(other.media_history));
   other.media_history.managed=true;other.handle=new_texture(320,240,1);other.handle->value=29;
   for(i=0;i<4;i++)assert(draw(&v,&c,&other)==(i==0));
   assert(draw(&v,&c,&t)); /* returning to an earlier texture */
   before=copies;
   memset(&t.media_history,0,sizeof(t.media_history));t.media_history.managed=true;
   assert(draw(&v,&c,&t) && copies==before+1); /* same address, new owner */
   t.media_history.managed=false;
   for(i=0;i<6;i++)assert(draw(&v,&c,&t));
   t.media_history.managed=true;t.desc.Width=160;t.desc.Format=2;
   for(i=0;i<5;i++)assert(draw(&v,&c,&t)==(i==0));
   assert(c.history[0].desc.Width==160 && c.history[0].desc.Format==2);
   c.history_made=false;
   assert(draw(&v,&c,&t)); /* a rebuilt chain at the same address */
   c.history[0].handle=NULL;before=allocated;
   assert(draw(&v,&c,&t) && allocated==before+3); /* failed or lost allocation */
   {
      unsigned version,repeat,j,expected[3],seen_copies,seen_barriers,names;
      gpu *owned[3];
      for(j=0;j<3;j++)
      {
         const d3d12_texture_t *history=(const d3d12_texture_t*)d3d12_chain_history_data(&c,&c.history[j]);
         expected[j]=history->handle->value;owned[j]=c.history[j].handle;
      }
      t.handle->value=1000;
      for(version=1;version<=24;version++)
      {
         d3d12_upload_texture(NULL,&t,&v);
         for(repeat=0;repeat<version%5+1;repeat++)
         {
            seen_copies=copies;seen_barriers=barriers;names=allocated;
            assert(draw(&v,&c,&t)==(repeat==0));
            assert(copies==seen_copies+(repeat?0:1));
            if(repeat)assert(barriers==seen_barriers);
            assert(allocated==names);
            for(j=2;j;j--)expected[j]=expected[j-1];
            expected[0]=1000+version;
            for(j=0;j<3;j++)
            {
               const d3d12_texture_t *history=(const d3d12_texture_t*)d3d12_chain_history_data(&c,&c.history[j]);
               assert(history->handle->value==expected[j] && c.history[j].handle==owned[j]);
               assert(*(const float*)d3d12_chain_history_data(&c,&c.history[j].size_data.x)==history->size_data.x);
            }
         }
      }
      t.srv_mapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      assert(!draw(&v,&c,&t)); /* zero and explicit default are equivalent */
      t.srv_mapping=23;assert(draw(&v,&c,&t));
      assert(((const d3d12_texture_t*)d3d12_chain_history_data(&c,c.history))->srv_mapping==23);
      assert(((const d3d12_texture_t*)d3d12_chain_history_data(&c,c.history+1))->srv_mapping==0);
      assert(!draw(&v,&c,&t));
   }
   p.history_size=0;before=allocated;
   for(i=0;i<3;i++)assert(!draw(&v,&c,&t));
   assert(allocated==before && c.frame_res==t.handle);
   p.history_size=2;
   for(i=0;i<4;i++)assert(draw(&v,&c,&t)==(i==0));
   assert(allocated==before+3 && c.history_shared);
   t.media_history.copies=0;
   gfx_media_history_ring_init(&c.history_ring,storage,3);
   c.history[gfx_media_history_ring_target(&c.history_ring,false)].handle=NULL;
   assert(!draw(&v,&c,&t) && !c.history_made);
   assert(draw(&v,&c,&t) && c.history_made);
   puts("PASS D3D12 viewer history: pixels, copy counts, uploads, address reuse, HDR format and no-history borrow");
   return 0;
}
'''
d3d_fixture = d3d_fixture.replace("\nHELPER\n", function(d3d, "static const void *d3d12_chain_history_data("))
a = d3d.index("   gfx_media_history_begin(&texture->media_history", d3d.index("bool gfx_display_d3d12_media_shader_draw("))
b = d3d.index("   chain->media_history = NULL;", a)
d3d_fixture = d3d_fixture.replace("\nDRAW_BODY\n", "\n" + d3d[a:b] + "\n")

d3d_fixture = d3d_fixture.replace("\nUPLOAD\n", function(d3d, "static void d3d12_upload_texture(D3D12GraphicsCommandList"))
d3d_fixture = d3d_fixture.replace("\nFRAME\n", function(d3d, "static bool d3d12_views_chain_frame("))

metal = (root / "gfx/drivers/metal.m").read_text(encoding="utf-8")
method = function(metal[metal.index("@implementation FrameView"):],
    "- (void)renderExternalViewChain:")
method = method[method.index("\n{"):]
method = "static void metal_frame(FrameView *self,metal_view_chain_t *c,gpu *external,\n" \
    "      const struct retro_video_view *v,unsigned vp_dims,bool fresh)" + method


def replace_once(pattern, replacement):
    global method
    method, count = re.subn(pattern, replacement, method, flags=re.S)
    assert count == 1, (pattern, count)


# Only Objective-C object operations are stood in for. The shipping branches,
# ring rotation, dimensions, copy decision and render/feedback calls execute.
replace_once(r'\[MTLTextureDescriptor\s+texture2DDescriptorWithPixelFormat:src.pixelFormat\s+'
    r'width:v->width height:v->height mipmapped:NO\]',
    "new_descriptor(src->pixelFormat,v->width,v->height)")
replace_once(r'\[MTLTextureDescriptor\s+texture2DDescriptorWithPixelFormat:MTLPixelFormatR16Uint\s+'
    r'width:v->width height:v->height mipmapped:NO\]',
    "new_descriptor(MTLPixelFormatR16Uint,v->width,v->height)")
method, count = re.subn(r'\[self _initTexture:([^\n]+?) withDescriptor:td\]', r'init_texture(\1,td)', method)
assert count == 2
replace_once(r'\[bce copyFromBuffer:_mem\[_pendingMem\].*?destinationOrigin:MTLOriginMake\(0, 0, 0\)\]',
    "copy_buffer(bce,self->mem[self->pendingMem],dst)")
replace_once(r'\[dst replaceRegion:MTLRegionMake2D\(0, 0, v->width, v->height\).*?bytesPerRow:_pendingPitch\]',
    "replace_texture(dst,(const uint8_t*)self->pendingSrc + at)")
replace_once(r'\[_context convertFormat:_format from:dst\s+to:(destination->view|c->engine.frame.texture\[0\].view)\]',
    lambda m: 'convert_format(self->context,self->format,dst,' + m[1] + ')')
replace_once(r'\[bce copyFromTexture:src.*?toTexture:(destination->view|c->engine.frame.texture\[0\].view).*?destinationOrigin:MTLOriginMake\(0, 0, 0\)\]',
    lambda m: 'copy_texture(bce,src,' + m[1] + ')')
replace_once(r'\[self _chainTargets:&c->engine width:v->width height:v->height\s+'
    r'viewport:vp_dims hdr:\(external && _context.hdrEnabled\)\]',
    "targets(&c->engine,v->width,v->height,vp_dims,external && self->context->hdrEnabled)")
replace_once(r'\[self _chainFeedback:&c->engine\]', "chain_feedback(&c->engine)")
replace_once(r'\[self _chainRender:&c->engine context:_context backBuffer:NO\]',
    "chain_render(&c->engine,self->context,false)")
method = method.replace("[cb blitCommandEncoder]", "encoder(cb)").replace(
    "[_context.blitCommandBuffer blitCommandEncoder]", "encoder(self->context->blitCommandBuffer)")
method = method.replace("[bce endEncoding]", "end_encoding(bce)").replace(
    "[_context hdrMenuNits]", "self->context->menuNits")
for field in ("engine", "shader", "context", "shaderEmitsHDR10", "shaderEmitsHDR16",
              "hdrPaperWhiteNits", "hdrExpandGamut", "pendingSrc", "pendingPitch", "pendingMem",
              "pendingOff", "format", "bpp", "memReader", "mem", "writtenSeen"):
    method = re.sub(r'\b_' + field + r'\b', "self->" + field, method)
method = method.replace("id<MTLTexture>", "gpu *")
method = method.replace("self->context.", "self->context->").replace("td.usage", "td->usage")
for name in ("front", "src", "external", "dst"):
    method = re.sub(r'\b' + name + r'\.(width|height|pixelFormat)\b', name + r'->\1', method)
method = method.replace(".view.width", ".view->width").replace(".view.height", ".view->height")
method = method.replace("id<MTLTexture>", "gpu *").replace("id<MTLBlitCommandEncoder>", "Encoder *")
method = method.replace("id<MTLCommandBuffer>", "Command *").replace("MTLTextureDescriptor", "Descriptor")
assert "[self " not in method and "[bce " not in method

planar = function(metal[metal.index("static enum video_texture_update metal_planar_encode("):],
    "static enum video_texture_update metal_planar_update(")
planar = planar.replace("md.context", "md->context").replace("t.texture", "t->texture").replace("id<MTLTexture>", "gpu *")
writable = function(metal, "static id<MTLTexture> metal_media_writable(")
writable = writable.replace("id<MTLTexture>", "gpu *").replace("md.views", "md->views").replace("t.texture", "t->texture")
update = function(metal, "static enum video_texture_update metal_update_texture_internal(")
update = update.replace("(__bridge MetalDriver *)", "(MetalDriver *)").replace(
    "(__bridge Texture *)", "(Texture *)").replace("@autoreleasepool", "")
update = update.replace("md.context", "md->context").replace("t.texture", "t->texture").replace(
    "t.staging.contents", "t->staging->contents").replace("t.staging.storageMode", "t->staging->storageMode")
update = update.replace("md->context.blitCommandBuffer", "md->context->blitCommandBuffer")
update = re.sub(r'\[bce copyFromBuffer:t.staging.*?destinationOrigin:MTLOriginMake\(0, 0, 0\)\]',
    "copy_staging(bce,t->staging,off,tex)", update, flags=re.S)
update = re.sub(r'\{\s*/\* The slot is free again.*?\[cb addCompletedHandler:.*?\}\];\s*\}',
    "register_completion(cb,t,slot);", update, flags=re.S)
update = re.sub(r'\[t.staging didModifyRange:NSMakeRange\(off, len\)\]',
    "modified_range(t->staging,off,len)", update)
update = update.replace("[cb blitCommandEncoder]", "encoder(cb)").replace("[bce endEncoding]", "end_encoding(bce)")
for objc, native in (("id<MTLTexture>", "gpu *"), ("id<MTLCommandBuffer>", "Command *"),
                     ("id<MTLBlitCommandEncoder>", "Encoder *")):
    update = update.replace(objc, native)
update = re.sub(r'\btex\.(width|height|pixelFormat|mipmapLevelCount)\b', r'tex->\1', update)
assert "[cb " not in update and "[bce " not in update and "addCompletedHandler" not in update

metal_fixture = common + r'''
#define GFX_MAX_FRAME_HISTORY 3
typedef struct Texture Texture;
#include "gfx/common/hdr_viewer_colour.h"
#define nil NULL
#define YES true
#define NO false
#define MTLPixelFormatRGBA16Float 2
#define MTLPixelFormatR16Uint 3
#define RPixelFormatBGRA8Unorm 1
#define RPixelFormatBGRX8Unorm 2
#define RPixelFormatBGR10A2Unorm 3
#define RARCH_STRUCT_ASSIGN(dst,src) ((dst)=(src))
#define RARCH_ASSIGN(dst,src) ((dst)=(src))
#define RARCH_LOG(...) ((void)0)
typedef unsigned NSUInteger;
typedef struct Command { int unused; } Command;
typedef struct Encoder { int unused; } Encoder;
typedef struct { unsigned format,width,height,usage; } Descriptor;
#define MTLTextureUsageShaderRead 1
#define MTLTextureUsageShaderWrite 2
typedef struct { gpu *view;float4 size_data; } texture_t;
typedef struct {
   bool media;
   gfx_media_history_ring_t *history_ring;
   struct { bool valid; texture_t texture[4];float4 output_size; } frame;
   struct { float viewer_colour_rows[12]; } pass[1];
} engine_t;
typedef struct {
   engine_t engine;
   texture_t src16;
   gfx_media_history_t *media_history;
   Texture *media_owner;
   bool history_borrowed;
   uintptr_t media_source;
   gfx_media_history_ring_t history_ring;
   bool history_made;
   unsigned rt_frame_dims,rt_vp_dims;
} metal_view_chain_t;
typedef struct { bool hdrEnabled;float menuNits;Command *blitCommandBuffer; } Context;
typedef struct { unsigned passes,history_size; } preset;
typedef struct {
   engine_t engine;
   preset *shader;
   Context *context;
   bool shaderEmitsHDR10,shaderEmitsHDR16,writtenSeen;
   float hdrPaperWhiteNits,hdrExpandGamut;
   const void *pendingSrc;
   unsigned pendingPitch,pendingOff,bpp,format;
   int pendingMem;
   void *mem[2];Command *memReader[2];
} FrameView;
static bool encoder_available=true;
static unsigned fail_texture;
static Descriptor *new_descriptor(unsigned format,unsigned width,unsigned height)
{ static Descriptor d;d.format=format;d.width=width;d.height=height;return &d; }
static void init_texture(texture_t *t,Descriptor *d)
{ t->view=fail_texture && --fail_texture==0 ? NULL : new_texture(d->width,d->height,d->format);t->size_data.x=(float)d->width;t->size_data.y=(float)d->height; }
static Encoder *encoder(Command *cb)
{ static Encoder e;return cb && encoder_available ? &e : NULL; }
static void copy_texture(Encoder *e,gpu *src,gpu *dst)
{ if(e) { dst->value=src->value;copies++; } }
static void copy_buffer(Encoder *e,void *src,gpu *dst)
{ if(e)dst->value=*(unsigned*)src; }
static void replace_texture(gpu *dst,const void *src) { dst->value=*(const unsigned*)src; }
static void convert_format(Context *ctx,unsigned format,gpu *src,gpu *dst)
{ (void)ctx;(void)format;dst->value=src->value; }
static void end_encoding(Encoder *e) { (void)e; }
static void targets(engine_t *e,unsigned w,unsigned h,unsigned dims,bool hdr)
{ (void)e;(void)w;(void)h;(void)dims;(void)hdr; }
static void chain_feedback(engine_t *e) { (void)e;feedbacks++; }
static void chain_render(engine_t *e,Context *ctx,bool backbuffer)
{ (void)ctx;(void)backbuffer;e->frame.valid=true;renders++; }
typedef struct Staging { void *contents;unsigned storageMode; } Staging;
#define TARGET_OS_OSX 1
#define MTLStorageModeManaged 1
#define METAL_STAGING_SLOTS 3
struct Texture {
   gpu *texture;
   gpu **_mediaTextures;
   unsigned _mediaTextureCount;
   bool _mediaBorrowable;
   gfx_media_history_t _mediaHistory;
   bool _planar;
   Staging *staging;
   unsigned _stagingNext,_stagingLent;
   int _stagingBusy[METAL_STAGING_SLOTS];
};
typedef struct { metal_view_chain_t *media_chain; } Views;
typedef struct { Context *context; Views *views; } MetalDriver;
enum video_texture_update { VIDEO_TEXTURE_UPDATE_DONE,VIDEO_TEXTURE_UPDATE_DROPPED,VIDEO_TEXTURE_UPDATE_REFUSED };
static enum video_texture_update encode_result=VIDEO_TEXTURE_UPDATE_DONE;
static enum video_texture_update metal_planar_encode(Context *ctx,Texture *t,gpu *dst,const void *image)
{ (void)ctx;(void)t;(void)dst;(void)image;return encode_result; }
struct texture_image { unsigned width,height;const void *pixels,*planar; };
static gpu *metal_media_new_texture(gpu *src)
{ return fail_texture && --fail_texture==0 ? NULL : new_texture(src->width,src->height,src->pixelFormat); }
WRITABLE
static bool metal_texture_staging(Texture *t,NSUInteger len)
{
   if(!t->staging) {
      t->staging=(Staging*)calloc(1,sizeof(Staging));assert(t->staging);
      t->staging->contents=calloc(METAL_STAGING_SLOTS,len);assert(t->staging->contents);
      t->staging->storageMode=MTLStorageModeManaged;
   }
   return true;
}
static void copy_staging(Encoder *e,Staging *s,NSUInteger off,gpu *dst)
{ if(e)dst->value=*(unsigned*)((uint8_t*)s->contents+off); }
static void modified_range(Staging *s,NSUInteger off,NSUInteger len)
{ (void)s;(void)off;(void)len; }
static void register_completion(Command *cb,Texture *t,unsigned slot)
{ (void)cb;(void)t;(void)slot; }
METAL_HELPERS
PLANAR
UPDATE
METHOD
static bool draw(FrameView *self,metal_view_chain_t *c,Texture *t)
{
   struct retro_video_view view;
   unsigned before=copies;
   memset(&view,0,sizeof(view));view.width=t->texture->width;view.height=t->texture->height;
   gfx_media_history_begin(&t->_mediaHistory,(uintptr_t)c,
         c->media_source==(uintptr_t)t,c->history_made);
   c->media_source=(uintptr_t)t;c->media_history=&t->_mediaHistory;c->media_owner=t;
   metal_frame(self,c,t->texture,&view,VIDEO_SCALE_PACK(800,600),true);
   c->media_history=NULL;c->media_owner=NULL;
   return copies!=before;
}
static void test_borrowed_pool(void)
{
   FrameView self;
   metal_view_chain_t c;
   Texture t,other;
   Command cb;
   Context ctx={true,203,&cb};
   Views views={&c};
   MetalDriver md={&ctx,&views};
   preset p={1,2};
   struct texture_image image;
   unsigned pixels[8]={100,0,0,0,0,0,0,0},storage[6],expected[3]={0,0,0};
   unsigned frame,repeat,j,names=0,before=copies;
   gpu *old;
   memset(&self,0,sizeof(self));memset(&c,0,sizeof(c));
   memset(&t,0,sizeof(t));memset(&other,0,sizeof(other));
   memset(&image,0,sizeof(image));
   self.shader=&p;self.context=&ctx;self.pendingMem=-1;
   gfx_media_history_ring_init(&c.history_ring,storage,3);
   t.texture=new_texture(2,2,MTLPixelFormatRGBA16Float);
   t.texture->value=100;t._mediaHistory.managed=true;t._mediaBorrowable=true;
   image.width=2;image.height=2;image.pixels=pixels;
   assert(!draw(&self,&c,&t) && copies==before);
   expected[0]=100;
   old=t.texture;fail_texture=1;
   assert(metal_update_texture_internal(&md,(uintptr_t)&t,&image)==VIDEO_TEXTURE_UPDATE_REFUSED);
   assert(t.texture==old && t._mediaTextureCount==1);
   for(frame=1;frame<=64;frame++)
   {
      gpu *readers[3];unsigned values[3];
      for(j=0;j<3;j++) { readers[j]=metal_history_texture(&c.engine,j)->view;values[j]=readers[j]->value; }
      pixels[0]=100+frame;
      assert(metal_update_texture_internal(&md,(uintptr_t)&t,&image)==VIDEO_TEXTURE_UPDATE_DONE);
      for(j=0;j<3;j++)assert(readers[j]->value==values[j]);
      for(j=0;j<METAL_STAGING_SLOTS;j++)t._stagingBusy[j]=0;
      assert(t.texture->pixelFormat==MTLPixelFormatRGBA16Float);
      for(repeat=0;repeat<frame%4+1;repeat++)
      {
         assert(!draw(&self,&c,&t) && copies==before);
         for(j=2;j;j--)expected[j]=expected[j-1];
         expected[0]=pixels[0];
         for(j=0;j<3;j++)assert(metal_history_texture(&c.engine,j)->view->value==expected[j]);
      }
      if(frame==8)names=allocated;
      if(frame>8)assert(names==allocated);
      assert(t._mediaTextureCount<=4);
   }
   /* A lent upload still reads the producer-written staging slot directly. */
   old=t.texture;t._stagingLent=1;
   *(unsigned*)t.staging->contents=888;image.pixels=t.staging->contents;
   assert(metal_update_texture_internal(&md,(uintptr_t)&t,&image)==VIDEO_TEXTURE_UPDATE_DONE);
   assert(old->value==expected[0] && t.texture->value==888);
   assert(!draw(&self,&c,&t));
   t._stagingLent=0;image.pixels=pixels;
   old=t.texture;
   for(j=0;j<METAL_STAGING_SLOTS;j++)t._stagingBusy[j]=1;
   assert(metal_update_texture_internal(&md,(uintptr_t)&t,&image)==VIDEO_TEXTURE_UPDATE_DROPPED);
   assert(t.texture==old && !draw(&self,&c,&t));
   for(j=0;j<METAL_STAGING_SLOTS;j++)t._stagingBusy[j]=0;
   encoder_available=false;
   assert(metal_update_texture_internal(&md,(uintptr_t)&t,&image)==VIDEO_TEXTURE_UPDATE_REFUSED);
   assert(t.texture==old);encoder_available=true;
   t._planar=true;encode_result=VIDEO_TEXTURE_UPDATE_DROPPED;
   assert(metal_planar_update(&md,&t,&image)==VIDEO_TEXTURE_UPDATE_DROPPED && t.texture==old);
   encode_result=VIDEO_TEXTURE_UPDATE_DONE;
   assert(metal_planar_update(&md,&t,&image)==VIDEO_TEXTURE_UPDATE_DONE && t.texture!=old);
   t.texture->value=777;assert(!draw(&self,&c,&t));
   other.texture=new_texture(2,2,1);other.texture->value=900;
   other._mediaHistory.managed=true;other._mediaBorrowable=true;
   assert(!draw(&self,&c,&other));
   old=t.texture;
   assert(metal_update_texture_internal(&md,(uintptr_t)&other,&image)==VIDEO_TEXTURE_UPDATE_DONE);
   assert(t.texture==old && old->value==777);
   assert(!draw(&self,&c,&other));assert(!draw(&self,&c,&t));
   assert(copies==before);
   p.history_size=0;assert(!draw(&self,&c,&t));
   assert(metal_media_writable(&md,&t)==t.texture);
   /* Removing/replacing the chain cannot make the producer pool dangle. */
   views.media_chain=NULL;
   assert(metal_update_texture_internal(&md,(uintptr_t)&t,&image)==VIDEO_TEXTURE_UPDATE_DONE);
   views.media_chain=&c;
   free(t._mediaTextures);free(other._mediaTextures);
   free(t.staging->contents);free(t.staging);
   free(other.staging->contents);free(other.staging);
   puts("PASS Metal borrowed history: zero snapshots, bounded reuse, immutable readers, dropped/failed uploads, HDR, planar and owner changes");
}

int main(void)
{
   FrameView self;
   metal_view_chain_t c;
   Texture t,other;
   Command cb;
   Context ctx={false,80,&cb};
   MetalDriver md={&ctx,NULL};
   preset p={1,2};
   struct texture_image image;
   unsigned *pixels=(unsigned*)calloc(320*240,sizeof(unsigned));
   unsigned i,before,storage[6];
   assert(pixels);
   memset(&self,0,sizeof(self));memset(&c,0,sizeof(c));memset(&t,0,sizeof(t));
   self.shader=&p;self.context=&ctx;self.pendingMem=-1;
   gfx_media_history_ring_init(&c.history_ring,storage,3);
   c.engine.history_ring=&c.history_ring;
   t.texture=new_texture(320,240,1);t.texture->value=19;t._mediaHistory.managed=true;
   for(i=0;i<8;i++)assert(draw(&self,&c,&t)==(i==0));
   assert(copies==1 && renders==8 && feedbacks==8 && allocated==4);
   for(i=0;i<3;i++)assert(metal_history_texture(&c.engine,i)->view->value==19);
   pixels[0]=21;memset(&image,0,sizeof(image));image.width=320;image.height=240;image.pixels=pixels;
   assert(metal_update_texture_internal(&md,(uintptr_t)&t,&image)==VIDEO_TEXTURE_UPDATE_DONE);
   assert(t._mediaHistory.copies==0 && t.texture->value==21);
   for(i=0;i<5;i++)assert(draw(&self,&c,&t)==(i==0));
   for(i=0;i<3;i++)assert(metal_history_texture(&c.engine,i)->view->value==21);
   for(i=0;i<METAL_STAGING_SLOTS;i++)t._stagingBusy[i]=1;
   assert(metal_update_texture_internal(&md,(uintptr_t)&t,&image)==VIDEO_TEXTURE_UPDATE_DROPPED);
   assert(!draw(&self,&c,&t));
   for(i=0;i<METAL_STAGING_SLOTS;i++)t._stagingBusy[i]=0;
   t._planar=true;encode_result=VIDEO_TEXTURE_UPDATE_DROPPED;
   assert(metal_planar_update(&md,&t,NULL)==VIDEO_TEXTURE_UPDATE_DROPPED);
   assert(!draw(&self,&c,&t)); /* a dropped upload leaves the cached pixels */
   encode_result=VIDEO_TEXTURE_UPDATE_DONE;
   assert(metal_planar_update(&md,&t,NULL)==VIDEO_TEXTURE_UPDATE_DONE);
   t.texture->value=20;
   for(i=0;i<5;i++)assert(draw(&self,&c,&t)==(i==0));
   for(i=0;i<3;i++)assert(metal_history_texture(&c.engine,i)->view->value==20);
   other=t;memset(&other._mediaHistory,0,sizeof(other._mediaHistory));
   other._mediaHistory.managed=true;other.texture=new_texture(320,240,1);other.texture->value=29;
   for(i=0;i<4;i++)assert(draw(&self,&c,&other)==(i==0));
   assert(draw(&self,&c,&t));
   memset(&t._mediaHistory,0,sizeof(t._mediaHistory));t._mediaHistory.managed=true;
   assert(draw(&self,&c,&t)); /* same handle address, newly created owner */
   t._mediaHistory.managed=false;
   for(i=0;i<6;i++)assert(draw(&self,&c,&t));
   t._mediaHistory.managed=true;t.texture->width=160;t.texture->pixelFormat=2;ctx.hdrEnabled=true;
   for(i=0;i<5;i++)assert(draw(&self,&c,&t)==(i==0));
   assert(c.engine.frame.texture[0].view->width==160);
   assert(c.engine.frame.texture[0].view->pixelFormat==2);
   ctx.menuNits=203;assert(!draw(&self,&c,&t)); /* raw history needs no colour bake */
   c.history_made=false;encoder_available=false;
   assert(!draw(&self,&c,&t) && t._mediaHistory.copies==0);
   encoder_available=true;
   for(i=0;i<5;i++)assert(draw(&self,&c,&t)==(i==0));
   c.history_made=false;fail_texture=3;
   assert(!draw(&self,&c,&t) && !c.history_made && !c.history_ring.valid);
   for(i=0;i<5;i++)assert(draw(&self,&c,&t)==(i==0));
   {
      unsigned version,repeat,j,expected[3],names,seen_renders,seen_feedbacks;
      gpu *owned[3];
      for(j=0;j<3;j++) { expected[j]=metal_history_texture(&c.engine,j)->view->value;owned[j]=c.engine.frame.texture[j].view; }
      for(version=1;version<=24;version++)
      {
         t.texture->value=1000+version;t._mediaHistory.copies=0;
         for(repeat=0;repeat<version%5+1;repeat++)
         {
            names=allocated;seen_renders=renders;seen_feedbacks=feedbacks;
            assert(draw(&self,&c,&t)==(repeat==0));
            assert(allocated==names && renders==seen_renders+1 && feedbacks==seen_feedbacks+1);
            for(j=2;j;j--)expected[j]=expected[j-1];
            expected[0]=1000+version;
            for(j=0;j<3;j++)
            {
               const texture_t *history=metal_history_texture(&c.engine,j);
               gpu *bound=*(gpu *const*)metal_history_data(&c.engine,&c.engine.frame.texture[j].view);
               assert(bound==history->view && bound->value==expected[j]);
               assert(c.engine.frame.texture[j].view==owned[j]);
               assert(*(const float*)metal_history_data(&c.engine,&c.engine.frame.texture[j].size_data.x)==history->size_data.x);
            }
         }
      }
   }
   p.history_size=0;before=allocated;
   for(i=0;i<3;i++)assert(!draw(&self,&c,&t));
   assert(allocated==before && c.engine.frame.texture[0].view==t.texture);
   p.history_size=2;
   for(i=0;i<4;i++)assert(draw(&self,&c,&t)==(i==0));
   assert(allocated==before+3 && c.engine.history_ring==&c.history_ring);
   assert(metal_history_texture(&c.engine,0)->view!=t.texture);
   free(t.staging->contents);free(t.staging);free(pixels);
   test_borrowed_pool();
   puts("PASS Metal viewer history: pixels, copy counts, dropped uploads, address reuse, HDR, failed encoders and no-history borrow");
   return 0;
}
'''
metal_fixture = metal_fixture.replace("\nMETAL_HELPERS\n", function(metal, "static const void *metal_history_data(") + function(metal, "static texture_t *metal_history_texture("))
metal_fixture = metal_fixture.replace("\nWRITABLE\n", writable)
metal_fixture = metal_fixture.replace("\nPLANAR\n", planar).replace("\nUPDATE\n", update).replace("\nMETHOD\n", method)


d3d11 = (root / "gfx/drivers/d3d11.c").read_text(encoding="utf-8")
d3d11_fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "gfx/common/media_history.h"
#include "gfx/video_defines.h"
typedef struct { float x,y,z,w; } float4;
typedef unsigned DXGI_FORMAT;
typedef unsigned D3D11_USAGE;
#define HAVE_SLANG 1
#define D3D11_USAGE_DEFAULT 0
#define D3D11_USAGE_DYNAMIC 1
#define DXGI_FORMAT_R16G16B16A16_FLOAT 2
#define DXGI_FORMAT_B5G6R5_UNORM 3
#define D3D11_MAP_WRITE_DISCARD 0
#define D3D11_RESOURCE_MISC_GENERATE_MIPS 1
#define VIDEO_TEXTURE_UPDATE_DONE 1
#define SUCCEEDED(x) ((x)>=0)
typedef struct { unsigned Width,Height,Usage,Format,MiscFlags; } D3D11_TEXTURE2D_DESC;
typedef struct { unsigned left,top,front,right,bottom,back; } D3D11_BOX;
typedef struct { void *pData;unsigned RowPitch; } D3D11_MAPPED_SUBRESOURCE;
struct gpu;
typedef struct gpu *D3D11Resource;
typedef struct gpu *D3D11Texture2D;
typedef struct gpu *D3D11ShaderResourceView;
struct gpu_vtbl { void (*GetResource)(struct gpu*,D3D11Resource*);void (*GetDesc)(struct gpu*,D3D11_TEXTURE2D_DESC*); };
typedef struct gpu { struct gpu_vtbl *lpVtbl;D3D11_TEXTURE2D_DESC desc;unsigned value,refs; } gpu;
static void get_resource(gpu *g,gpu **out) {assert(g->refs);g->refs++;*out=g;}
static void get_desc(gpu *g,D3D11_TEXTURE2D_DESC *d) {assert(g->refs);*d=g->desc;}
static struct gpu_vtbl resource_vtbl={get_resource,get_desc};
static gpu pool[512];
static unsigned allocated,copies,renders,feedbacks,failures;
static gpu *new_gpu(D3D11_TEXTURE2D_DESC desc)
{ gpu *g;assert(allocated<512);g=&pool[allocated++];g->lpVtbl=&resource_vtbl;g->desc=desc;g->refs=1;return g; }
#define Release(g) do { if(g) {assert((g)->refs);(g)->refs--;} } while(0)
struct context;
struct ctx_vtbl {
   void (*CopySubresourceRegion)(struct context*,gpu*,unsigned,unsigned,unsigned,unsigned,gpu*,unsigned,const D3D11_BOX*);
   int (*Map)(struct context*,gpu*,unsigned,unsigned,unsigned,D3D11_MAPPED_SUBRESOURCE*);
   void (*Unmap)(struct context*,gpu*,unsigned);
   void (*GenerateMips)(struct context*,gpu*);
};
struct context {struct ctx_vtbl *lpVtbl;};
static void copy_region(struct context *ctx,gpu *dst,unsigned a,unsigned b,unsigned c,unsigned d,gpu *src,unsigned e,const D3D11_BOX *box)
{ (void)ctx;(void)a;(void)b;(void)c;(void)d;(void)e;assert(src->refs && dst->refs && box->right<=src->desc.Width && box->bottom<=src->desc.Height);dst->value=src->value;copies++; }
static int map(struct context *c,gpu *g,unsigned a,unsigned b,unsigned d,D3D11_MAPPED_SUBRESOURCE *m)
{(void)c;(void)g;(void)a;(void)b;(void)d;(void)m;return -1;}
static void unmap(struct context *c,gpu *g,unsigned a) {(void)c;(void)g;(void)a;}
static void mips(struct context *c,gpu *g) {(void)c;(void)g;}
static struct ctx_vtbl context_vtbl={copy_region,map,unmap,mips};
static struct context context={&context_vtbl};
typedef struct {gpu *handle,*view;D3D11_TEXTURE2D_DESC desc;float4 size_data;gfx_media_history_t media_history;} d3d11_texture_t;
typedef struct {unsigned history_size;} preset;
typedef struct {preset *shader_preset;d3d11_texture_t history[4];struct {float values[12];} frame_rect;bool history_made;gfx_media_history_t *media_history;uintptr_t media_source;gpu *frame_view;unsigned rt_frame_dims,rt_vp_dims,frame_format;float4 output_size;} d3d11_chain_t;
typedef struct {struct context *context;void *device;unsigned format;void *media_shader_info;uint64_t media_shader_frame;} d3d11_video_t;
struct retro_video_view {unsigned x,y,width,height;};
static void d3d11_release_texture(d3d11_texture_t *t) {Release(t->handle);t->handle=t->view=NULL;}
static void d3d11_init_texture_ex(void *dev,d3d11_texture_t *t,bool staging)
{(void)dev;(void)staging;if(failures){failures--;return;}t->handle=t->view=new_gpu(t->desc);}
static void d3d11_chain_init_render_targets(d3d11_video_t *v,d3d11_chain_t *c,unsigned w,unsigned h,unsigned dims)
{(void)v;(void)c;(void)w;(void)h;(void)dims;}
static void dxgi_copy(unsigned w,unsigned h,unsigned fmt,unsigned pitch,const void *src,unsigned df,unsigned dp,void *dst)
{(void)w;(void)h;(void)fmt;(void)pitch;(void)src;(void)df;(void)dp;(void)dst;}
static void d3d11_chain_frame(d3d11_video_t *v,d3d11_chain_t *c,void *info,uint64_t frame,bool fresh)
{(void)v;(void)c;(void)info;(void)frame;(void)fresh;renders++;feedbacks++;}
static void slang_rect_values(float *values,unsigned a,unsigned b,unsigned c) { values[0]=(float)(a+b+c); }
CHAIN
static void draw(d3d11_video_t *video,d3d11_chain_t *chain,d3d11_texture_t *texture)
{
   uintptr_t image=(uintptr_t)texture;
   unsigned source_dims=VIDEO_SCALE_PACK(texture->desc.Width,texture->desc.Height);
   unsigned dims=VIDEO_SCALE_PACK(640,480);
   struct retro_video_view view;
   memset(&view,0,sizeof(view));view.width=texture->desc.Width;view.height=texture->desc.Height;
DRAW
}
static int update(d3d11_texture_t *texture,gpu *upload)
{
   struct context *ctx=&context;
   D3D11_BOX box;
   struct {unsigned width,height;} value;
   const void *image=&value;
   /* image is a texture_image in the production fragment. */
UPDATE
}
int main(void)
{
   d3d11_video_t v;
   d3d11_chain_t c;
   d3d11_texture_t t,other;
   preset p={2};
   unsigned i,j,before,version,repeat,expected[3]={0,0,0};
   memset(&v,0,sizeof(v));memset(&c,0,sizeof(c));memset(&t,0,sizeof(t));
   c.shader_preset=&p;v.context=&context;
   t.desc.Width=32;t.desc.Height=24;t.desc.Format=1;
   t.handle=t.view=new_gpu(t.desc);t.handle->value=19;t.media_history.managed=true;
   for(i=0;i<9;i++)draw(&v,&c,&t);
   assert(copies==3 && renders==9 && feedbacks==9 && allocated==4 && t.handle->refs==1);
   for(i=0;i<3;i++)assert(c.history[i].handle->value==19);
   for(i=0;i<3;i++)expected[i]=19;
   for(version=1;version<=24;version++)
   {
      gpu *upload=new_gpu(t.desc);upload->value=100+version;
      assert(update(&t,upload)==VIDEO_TEXTURE_UPDATE_DONE && !t.media_history.copies);
      Release(upload);
      for(repeat=0;repeat<version%5+1;repeat++)
      {
         before=copies;draw(&v,&c,&t);
         assert(copies==before+(repeat<3?1u:0u));
         for(j=2;j;j--)expected[j]=expected[j-1];
         expected[0]=100+version;
         for(j=0;j<3;j++)assert(c.history[j].handle->value==expected[j]);
      }
   }
   other=t;other.handle=other.view=new_gpu(t.desc);other.handle->value=77;
   memset(&other.media_history,0,sizeof(other.media_history));other.media_history.managed=true;
   before=copies;for(i=0;i<5;i++)draw(&v,&c,&other);assert(copies==before+3);
   before=copies;for(i=0;i<5;i++)draw(&v,&c,&t);
   assert(copies==before+3);
   memset(&t.media_history,0,sizeof(t.media_history));t.media_history.managed=true;
   before=copies;for(i=0;i<5;i++)draw(&v,&c,&t);
   assert(copies==before+3);
   t.desc.Width=16;t.handle->desc=t.desc;
   before=copies;for(i=0;i<5;i++)draw(&v,&c,&t);
   assert(copies==before+3 && c.history[0].desc.Width==16);
   t.desc.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;t.handle->desc=t.desc;
   { gpu *upload=new_gpu(t.desc);upload->value=203;
     assert(update(&t,upload)==VIDEO_TEXTURE_UPDATE_DONE);Release(upload); }
   before=copies;for(i=0;i<5;i++)draw(&v,&c,&t);
   assert(copies==before+3 && c.frame_format==DXGI_FORMAT_R16G16B16A16_FLOAT);
   c.history[0].view=NULL;
   before=copies;for(i=0;i<5;i++)draw(&v,&c,&t);
   assert(copies==before+3 && c.history[0].view);
   c.history_made=false;failures=3;
   before=copies;draw(&v,&c,&t);assert(copies==before && !c.history_made);
   for(i=0;i<5;i++)draw(&v,&c,&t);
   assert(copies==before+3);
   t.media_history.managed=false;before=copies;
   for(i=0;i<5;i++)draw(&v,&c,&t);
   assert(copies==before+5);
   c.media_history=NULL;before=copies;
   { struct retro_video_view view={0,0,16,24};
     for(i=0;i<4;i++)d3d11_views_chain_frame(&v,&c,&view,VIDEO_SCALE_PACK(640,480),t.view,VIDEO_SCALE_PACK(16,24),false,true,NULL,0); }
   assert(copies==before+4);
   p.history_size=0;before=copies;draw(&v,&c,&t);assert(copies==before && c.frame_view==t.view);
   puts("PASS D3D11 viewer history: unchanged images, mixed cadence, uploads, owner reuse, allocation recovery and no-history borrow");
   return 0;
}
'''
d3d11_fixture = d3d11_fixture.replace('\nCHAIN\n', function(d3d11, 'static void d3d11_views_chain_frame('))
a = d3d11.index('   gfx_media_history_begin(', d3d11.index('static bool gfx_display_d3d11_media_shader_draw('))
b = d3d11.index('   chain->mvp_last_pass', a)
d3d11_fixture = d3d11_fixture.replace('\nDRAW\n', d3d11[a:b])
a = d3d11.index('upload:\n', d3d11.index('static enum video_texture_update d3d11_gfx_update_texture_internal(', d3d11.index('static void *d3d11_gfx_texture_lend')))
b = d3d11.index('\n}', a)
update = d3d11[a + len('upload:\n'):b].replace('image->width', 'value.width').replace('image->height', 'value.height')
update = '   value.width=texture->desc.Width;value.height=texture->desc.Height;\n   (void)image;\n' + update
d3d11_fixture = d3d11_fixture.replace('\nUPDATE\n', update)


def build(unit, compiler, flags, directory, c99=False):
    path = Path(directory) / "test.c"
    exe = Path(directory) / ("test.exe" if "mingw" in " ".join(compiler) or os.name == "nt" else "test")
    path.write_text(unit, encoding="utf-8")
    # The Metal half runs metal.m's own code, Objective-C on C99 (a loop
    # declares its counter); the rest is held to C89.
    dialect = ["-std=c99"] if c99 else ["-std=c89"]
    dialect += ["-Werror=declaration-after-statement"]
    if any("clang" in word for word in compiler):
        # The frontend's C89_BUILD uses the same allowance for boolean.h.
        dialect += ["-Wno-c99-extensions", "-Wno-c11-extensions"]
    subprocess.run(compiler + flags + dialect + ["-pedantic",
        "-Wall", "-Wextra", "-Werror", "-I" + str(root),
        "-I" + str(root / "libretro-common/include"), str(path), "-o", str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--backend", choices=("d3d11", "d3d12", "metal", "both"), default="both")
args = parser.parse_args()
cc = shlex.split(os.environ.get("CC", "cc"))
flags = shlex.split(os.environ.get("CFLAGS", ""))
with tempfile.TemporaryDirectory(prefix="viewer-history-") as directory:
    if args.backend in ("d3d11", "both"):
        build(d3d11_fixture, cc, flags, directory)
    if args.backend in ("d3d12", "both"):
        build(d3d_fixture, cc, flags, directory)
    if args.backend in ("metal", "both"):
        build(metal_fixture, cc, flags, directory, c99=True)
