#!/usr/bin/env python3
"""Execute the shipping D3D12/Metal history paths against counted GPU copies."""
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
#include "gfx/video_defines.h"
typedef struct { float x,y,z,w; } float4;
typedef struct gpu { unsigned width,height,pixelFormat,value,mipmapLevelCount; } gpu;
static gpu pool[128];
static unsigned allocated,copies,renders,feedbacks;
static gpu *new_texture(unsigned width,unsigned height,unsigned format)
{
   gpu *p;
   assert(allocated<128);p=&pool[allocated++];
   p->width=width;p->height=height;p->pixelFormat=format;p->value=0;p->mipmapLevelCount=1;return p;
}
struct retro_video_view { unsigned x,y,width,height; };
'''

d3d = (root / "gfx/drivers/d3d12.c").read_text(encoding="utf-8")
d3d_fixture = common + r'''
#define HAVE_SLANG 1
typedef gpu *D3D12Resource;
typedef void *D3D12GraphicsCommandList;
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
   uintptr_t media_source;
   bool history_made;
   gpu *frame_res;
   unsigned rt_frame_dims,rt_vp_dims;
   float4 output_size;
   struct { float values[16]; } frame_rect;
} d3d12_filter_chain_t;
typedef struct {
   void *device;
   struct { void *srv_heap; } desc;
   d3d12_filter_chain_t *media_shader_chain,filter;
   d3d12_texture_t *media_shader_texture;
} d3d12_video_t;
static void slang_rect_values(float *values,unsigned dims,unsigned pos,unsigned src)
{ (void)values;(void)dims;(void)pos;(void)src; }
static void d3d12_release_texture(d3d12_video_t *v,d3d12_texture_t *t)
{ (void)v;t->handle=NULL; }
static void d3d12_init_texture(void *device,d3d12_texture_t *t)
{ (void)device;t->handle=new_texture(t->desc.Width,t->desc.Height,t->desc.Format); }
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
UPLOAD
FRAME
static bool draw(d3d12_video_t *v,d3d12_filter_chain_t *c,d3d12_texture_t *t)
{
   struct retro_video_view view;
   bool copy;
   memset(&view,0,sizeof(view));view.width=t->desc.Width;view.height=t->desc.Height;
   gfx_media_history_begin(&t->media_history,(uintptr_t)c,
         c->media_source==(uintptr_t)t,c->history_made);
   c->media_source=(uintptr_t)t;c->media_history=&t->media_history;
   v->media_shader_texture=t;
   copy=d3d12_views_chain_frame(v,c,&view,VIDEO_SCALE_PACK(800,600),t->handle,
         VIDEO_SCALE_PACK(view.width,view.height),c->shader_preset->history_size==0,true);
   if(copy) { c->history[0].handle->value=t->handle->value;copies++;
      gfx_media_history_written(c->media_history); }
   c->media_history=NULL;renders++;feedbacks++;
   return copy;
}
int main(void)
{
   d3d12_video_t v;
   d3d12_filter_chain_t c;
   d3d12_texture_t t,other;
   preset p={2};
   unsigned i,before;
   memset(&v,0,sizeof(v));memset(&c,0,sizeof(c));memset(&t,0,sizeof(t));
   c.shader_preset=&p;v.media_shader_chain=&c;
   t.desc.Width=320;t.desc.Height=240;t.desc.Format=1;
   t.handle=new_texture(320,240,1);t.handle->value=19;t.media_history.managed=true;
   for(i=0;i<8;i++) assert(draw(&v,&c,&t)==(i<3));
   assert(copies==3 && renders==8 && feedbacks==8 && allocated==4);
   for(i=0;i<3;i++)assert(c.history[i].handle->value==19);
   before=copies;d3d12_upload_texture(NULL,&t,&v);
   assert(t.media_history.copies==0);
   for(i=0;i<5;i++)assert(draw(&v,&c,&t)==(i<3));
   assert(copies==before+3);
   for(i=0;i<3;i++)assert(c.history[i].handle->value==20);
   t.planar=&t;d3d12_upload_texture(NULL,&t,&v);t.planar=NULL;
   assert(t.media_history.copies==0 && draw(&v,&c,&t));
   other=t;memset(&other.media_history,0,sizeof(other.media_history));
   other.media_history.managed=true;other.handle=new_texture(320,240,1);other.handle->value=29;
   for(i=0;i<4;i++)assert(draw(&v,&c,&other)==(i<3));
   assert(draw(&v,&c,&t)); /* returning to an earlier texture */
   before=copies;
   memset(&t.media_history,0,sizeof(t.media_history));t.media_history.managed=true;
   assert(draw(&v,&c,&t) && copies==before+1); /* same address, new owner */
   t.media_history.managed=false;
   for(i=0;i<6;i++)assert(draw(&v,&c,&t));
   t.media_history.managed=true;t.desc.Width=160;t.desc.Format=2;
   for(i=0;i<5;i++)assert(draw(&v,&c,&t)==(i<3));
   assert(c.history[0].desc.Width==160 && c.history[0].desc.Format==2);
   c.history_made=false;
   assert(draw(&v,&c,&t)); /* a rebuilt chain at the same address */
   c.history[0].handle=NULL;before=allocated;
   assert(draw(&v,&c,&t) && allocated==before+3); /* failed or lost allocation */
   p.history_size=0;before=allocated;
   for(i=0;i<3;i++)assert(!draw(&v,&c,&t));
   assert(allocated==before && c.frame_res==t.handle);
   puts("PASS D3D12 viewer history: pixels, copy counts, uploads, address reuse, HDR format and no-history borrow");
   return 0;
}
'''
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
replace_once(r'\[_context convertFormat:_format from:dst\s+to:c->engine.frame.texture\[0\].view\]',
    "convert_format(self->context,self->format,dst,c->engine.frame.texture[0].view)")
replace_once(r'\[bce copyFromTexture:src.*?destinationOrigin:MTLOriginMake\(0, 0, 0\)\]',
    "copy_texture(bce,src,c->engine.frame.texture[0].view)")
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
method = method.replace("self->context.", "self->context->").replace("td.usage", "td->usage")
for name in ("front", "src", "external", "dst"):
    method = re.sub(r'\b' + name + r'\.(width|height|pixelFormat)\b', name + r'->\1', method)
method = method.replace(".view.width", ".view->width").replace(".view.height", ".view->height")
method = method.replace("id<MTLTexture>", "gpu *").replace("id<MTLBlitCommandEncoder>", "Encoder *")
method = method.replace("id<MTLCommandBuffer>", "Command *").replace("MTLTextureDescriptor", "Descriptor")
assert "[self " not in method and "[bce " not in method

planar = function(metal[metal.index("static enum video_texture_update metal_planar_encode("):],
    "static enum video_texture_update metal_planar_update(")
planar = planar.replace("md.context", "md->context").replace("t.texture", "t->texture")
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
struct Command {};
struct Encoder {};
typedef struct { unsigned format,width,height,usage; } Descriptor;
#define MTLTextureUsageShaderRead 1
#define MTLTextureUsageShaderWrite 2
typedef struct { gpu *view;float4 size_data; } texture_t;
typedef struct {
   bool media;
   struct { bool valid; texture_t texture[4];float4 output_size; } frame;
   struct { float viewer_colour_rows[12]; } pass[1];
} engine_t;
typedef struct {
   engine_t engine;
   texture_t src16;
   gfx_media_history_t *media_history;
   uintptr_t media_source;
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
static Descriptor *new_descriptor(unsigned format,unsigned width,unsigned height)
{ static Descriptor d;d.format=format;d.width=width;d.height=height;return &d; }
static void init_texture(texture_t *t,Descriptor *d)
{ t->view=new_texture(d->width,d->height,d->format); }
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
struct Staging { void *contents;unsigned storageMode; };
#define TARGET_OS_OSX 1
#define MTLStorageModeManaged 1
#define METAL_STAGING_SLOTS 3
typedef struct {
   gpu *texture;
   gfx_media_history_t _mediaHistory;
   bool _planar;
   Staging *staging;
   unsigned _stagingNext,_stagingLent;
   int _stagingBusy[METAL_STAGING_SLOTS];
} Texture;
typedef struct { Context *context; } MetalDriver;
enum video_texture_update { VIDEO_TEXTURE_UPDATE_DONE,VIDEO_TEXTURE_UPDATE_DROPPED,VIDEO_TEXTURE_UPDATE_REFUSED };
static enum video_texture_update encode_result=VIDEO_TEXTURE_UPDATE_DONE;
static enum video_texture_update metal_planar_encode(Context *ctx,Texture *t,gpu *dst,const void *image)
{ (void)ctx;(void)t;(void)dst;(void)image;return encode_result; }
struct texture_image { unsigned width,height;const void *pixels,*planar; };
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
   c->media_source=(uintptr_t)t;c->media_history=&t->_mediaHistory;
   metal_frame(self,c,t->texture,&view,VIDEO_SCALE_PACK(800,600),true);
   c->media_history=NULL;
   return copies!=before;
}
int main(void)
{
   FrameView self;
   metal_view_chain_t c;
   Texture t,other;
   Command cb;
   Context ctx={false,80,&cb};
   MetalDriver md={&ctx};
   preset p={1,2};
   struct texture_image image;
   unsigned *pixels=(unsigned*)calloc(320*240,sizeof(unsigned));assert(pixels);
   unsigned i,before;
   memset(&self,0,sizeof(self));memset(&c,0,sizeof(c));memset(&t,0,sizeof(t));
   self.shader=&p;self.context=&ctx;self.pendingMem=-1;
   t.texture=new_texture(320,240,1);t.texture->value=19;t._mediaHistory.managed=true;
   for(i=0;i<8;i++)assert(draw(&self,&c,&t)==(i<3));
   assert(copies==3 && renders==8 && feedbacks==8 && allocated==4);
   for(i=0;i<3;i++)assert(c.engine.frame.texture[i].view->value==19);
   pixels[0]=21;memset(&image,0,sizeof(image));image.width=320;image.height=240;image.pixels=pixels;
   assert(metal_update_texture_internal(&md,(uintptr_t)&t,&image)==VIDEO_TEXTURE_UPDATE_DONE);
   assert(t._mediaHistory.copies==0 && t.texture->value==21);
   for(i=0;i<5;i++)assert(draw(&self,&c,&t)==(i<3));
   for(i=0;i<3;i++)assert(c.engine.frame.texture[i].view->value==21);
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
   for(i=0;i<5;i++)assert(draw(&self,&c,&t)==(i<3));
   for(i=0;i<3;i++)assert(c.engine.frame.texture[i].view->value==20);
   other=t;memset(&other._mediaHistory,0,sizeof(other._mediaHistory));
   other._mediaHistory.managed=true;other.texture=new_texture(320,240,1);other.texture->value=29;
   for(i=0;i<4;i++)assert(draw(&self,&c,&other)==(i<3));
   assert(draw(&self,&c,&t));
   memset(&t._mediaHistory,0,sizeof(t._mediaHistory));t._mediaHistory.managed=true;
   assert(draw(&self,&c,&t)); /* same handle address, newly created owner */
   t._mediaHistory.managed=false;
   for(i=0;i<6;i++)assert(draw(&self,&c,&t));
   t._mediaHistory.managed=true;t.texture->width=160;t.texture->pixelFormat=2;ctx.hdrEnabled=true;
   for(i=0;i<5;i++)assert(draw(&self,&c,&t)==(i<3));
   assert(c.engine.frame.texture[0].view->width==160);
   assert(c.engine.frame.texture[0].view->pixelFormat==2);
   ctx.menuNits=203;assert(!draw(&self,&c,&t)); /* raw history needs no colour bake */
   c.history_made=false;encoder_available=false;
   assert(!draw(&self,&c,&t) && t._mediaHistory.copies==0);
   encoder_available=true;
   for(i=0;i<5;i++)assert(draw(&self,&c,&t)==(i<3));
   p.history_size=0;before=allocated;
   for(i=0;i<3;i++)assert(!draw(&self,&c,&t));
   assert(allocated==before && c.engine.frame.texture[0].view==t.texture);
   free(t.staging->contents);free(t.staging);free(pixels);
   puts("PASS Metal viewer history: pixels, copy counts, dropped uploads, address reuse, HDR, failed encoders and no-history borrow");
   return 0;
}
'''
metal_fixture = metal_fixture.replace("\nPLANAR\n", planar).replace("\nUPDATE\n", update).replace("\nMETHOD\n", method)


def build(unit, compiler, flags, directory, cpp=False):
    path = Path(directory) / ("test.cpp" if cpp else "test.c")
    exe = Path(directory) / ("test.exe" if "mingw" in " ".join(compiler) or os.name == "nt" else "test")
    path.write_text(unit, encoding="utf-8")
    dialect = ["-std=c++11"] if cpp else ["-std=c89", "-Werror=declaration-after-statement"]
    if not cpp and any("clang" in word for word in compiler):
        # The frontend's C89_BUILD uses the same allowance for boolean.h.
        dialect += ["-Wno-c99-extensions", "-Wno-c11-extensions"]
    subprocess.run(compiler + flags + dialect + ["-pedantic",
        "-Wall", "-Wextra", "-Werror", "-I" + str(root),
        "-I" + str(root / "libretro-common/include"), str(path), "-o", str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--backend", choices=("d3d12", "metal", "both"), default="both")
args = parser.parse_args()
cc = shlex.split(os.environ.get("CC", "cc"))
flags = shlex.split(os.environ.get("CFLAGS", ""))
with tempfile.TemporaryDirectory(prefix="viewer-history-") as directory:
    if args.backend in ("d3d12", "both"):
        build(d3d_fixture, cc, flags, directory)
    cxx = shlex.split(os.environ.get("CXX", "c++"))
    if os.environ.get("CC"):
        cxx = list(cc)
        for i, word in enumerate(cxx):
            suffix = ".exe" if word.endswith(".exe") else ""
            driver = word[:-4] if suffix else word
            for c, cpp in (("clang", "clang++"), ("gcc", "g++"), ("cc", "c++")):
                if driver.endswith(c):
                    cxx[i] = driver[:-len(c)] + cpp + suffix
                    break
    if args.backend in ("metal", "both"):
        build(metal_fixture, cxx, flags, directory, cpp=True)
