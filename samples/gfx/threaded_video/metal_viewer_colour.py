#!/usr/bin/env python3
"""Run the shipping Metal clone with Objective-C object operations stood in for."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/drivers/metal.m").read_text(encoding="utf-8")

def method(signature):
    start = source.index(signature)
    return source[start:source.index("\n}\n", start) + 3]

clone = method("- (metal_view_chain_t *)makeViewChainForMedia:(BOOL)media\n{")
clone = clone.replace("- (metal_view_chain_t *)makeViewChainForMedia:(BOOL)media", "static metal_view_chain_t *clone(FrameView *self, bool media)")
clone = clone.replace("_engine", "self->engine").replace("_shader", "self->shader")
clone = clone.replace("[self freeViewChain:c]", "free_chain(c)")
clone = clone.replace("id<MTLBuffer>", "object *").replace("src.length", "src->length")
clone, n = re.subn(r'RARCH_AUTORELEASE_R\(\[_context.device\s+newBufferWithLength:src->length\s+options:PLATFORM_METAL_RESOURCE_STORAGE_MODE\]\)', "new_buffer(src->length)", clone)
assert n == 1
release = method("- (void)freeViewChain:(metal_view_chain_t *)c\n{")
release = release.replace("- (void)freeViewChain:(metal_view_chain_t *)c", "static void free_chain(metal_view_chain_t *c)")
fixture = r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gfx/common/media_history_ring.h"
#define nil NULL
#define SLANG_CBUFFER_MAX 2
#define GFX_MAX_SHADERS 1
#define GFX_MAX_TEXTURES 1
#define GFX_MAX_FRAME_HISTORY 1
typedef struct object { unsigned refs,length; bool owned; } object;
typedef struct { void *data; unsigned size; } uniform_sem_t;
typedef struct { void *texture_data; } texture_sem_t;
typedef struct { uniform_sem_t *uniforms; int uniform_count; } cbuffer_sem_t;
typedef struct { texture_sem_t *textures; int texture_count; cbuffer_sem_t cbuffers[2]; } pass_semantics_t;
typedef struct { object *view; float size_data; } texture_t;
typedef struct {
   float mvp;
   gfx_media_history_ring_t *history_ring;
   struct { texture_t texture[2]; } frame;
   struct {
      object *buffers[2],*_state,*_viewerState;
      texture_t rt,feedback;
      pass_semantics_t semantics,viewer_semantics;
      float viewer_colour_rows[12];
   } pass[1];
   texture_t luts[1];
} engine_t;
typedef struct { unsigned passes,luts,history_size; } preset;
typedef struct { engine_t engine;texture_t src16;gfx_media_history_ring_t history_ring; } metal_view_chain_t;
typedef struct { engine_t engine; preset *shader; } FrameView;
static unsigned buffers,creates,fail_create;
static void drop(object *o) { if(o && !--o->refs && o->owned) { buffers--;free(o); } }
static void assign(object **dst,object *src)
{ if(src)src->refs++;drop(*dst);*dst=src; }
#define RARCH_STRUCT_ASSIGN(dst,src) assign(&(dst),(src))
static object *new_buffer(unsigned length)
{
   object *o;
   if(++creates==fail_create)return NULL;
   o=(object*)calloc(1,sizeof(*o));assert(o);o->owned=true;o->length=length;buffers++;return o;
}
RELEASE
CLONE
int main(void)
{
   FrameView self;
   preset shader={1,1,1};
   object objects[4];
   uniform_sem_t uniforms[4];texture_sem_t textures[2];
   unsigned i,mode;
   memset(&self,0,sizeof(self));memset(objects,0,sizeof(objects));memset(uniforms,0,sizeof(uniforms));
   for(i=0;i<4;i++) { objects[i].refs=1;objects[i].length=128; }
   self.shader=&shader;self.engine.pass[0]._state=&objects[0];self.engine.pass[0]._viewerState=&objects[1];
   self.engine.pass[0].buffers[0]=self.engine.pass[0].buffers[1]=&objects[2];
   self.engine.luts[0].view=&objects[3];
   for(i=0;i<3;i++) { uniforms[i].data=self.engine.pass[0].viewer_colour_rows+4*i;uniforms[i].size=16; }
   self.engine.pass[0].viewer_semantics.cbuffers[0].uniforms=uniforms;
   self.engine.pass[0].viewer_semantics.cbuffers[0].uniform_count=3;
   textures[0].texture_data=&self.engine.luts[0].view;textures[1].texture_data=NULL;
   self.engine.pass[0].viewer_semantics.textures=textures;self.engine.pass[0].viewer_semantics.texture_count=1;
   for(mode=0;mode<2;mode++)
   {
      metal_view_chain_t *c=clone(&self,mode!=0);
      assert(c && buffers==2 && c->engine.pass[0]._state==&objects[mode]);
      assert(c->history_ring.count==(mode?2u:0u));
      assert(c->engine.history_ring==(mode?&c->history_ring:NULL));
      if(mode)
      {
         assert(c->engine.pass[0].semantics.textures[0].texture_data==&c->engine.luts[0].view);
         for(i=0;i<3;i++) assert(c->engine.pass[0].semantics.cbuffers[0].uniforms[i].data==c->engine.pass[0].viewer_colour_rows+4*i);
      }
      assert(self.engine.pass[0].viewer_semantics.cbuffers[0].uniforms==uniforms);
      free_chain(c);assert(!buffers);
      for(i=0;i<4;i++) assert(objects[i].refs==1);
   }
   for(mode=1;mode<=2;mode++)
   {
      fail_create=creates+mode;
      assert(!clone(&self,true));assert(!buffers);
      for(i=0;i<4;i++) assert(objects[i].refs==1);
   }
   puts("PASS Metal viewer: borrowed pipeline, rebased uniforms/LUTs, failed buffer allocations released");
   return 0;
}
'''.replace("RELEASE", release).replace("CLONE", clone)
cc = shlex.split(os.environ.get("CC", "cc"))
with tempfile.TemporaryDirectory(prefix="metal-viewer-colour-") as directory:
    unit = Path(directory) / "test.c"
    exe = Path(directory) / ("test.exe" if os.name == "nt" or "mingw" in " ".join(cc) else "test")
    unit.write_bytes(fixture.encode("utf-8"))
    subprocess.run(cc + shlex.split(os.environ.get("CFLAGS", "")) + [
        "-I"+str(root), "-I"+str(root / "libretro-common/include"),
        "-std=gnu89", "-Wall", "-Wextra", "-Werror", "-Werror=declaration-after-statement",
        str(unit), "-o", str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
