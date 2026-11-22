#!/usr/bin/env python3
"""Exercise the shipping D3D12 viewer clone and output encoding selection."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/drivers/d3d12.c").read_text(encoding="utf-8")

def function(signature):
    start = source.index(signature)
    return source[start:source.index("\n}\n", start) + 3]

fixture = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gfx/common/media_history_ring.h"
#define SLANG_CBUFFER_MAX 2
#define SLANG_NUM_BINDINGS 4
#define D3D12_FRAMES 2
#define D3D12_DESCRIPTOR_HEAP_TYPE_RTV 1
#define D3D12_DESCRIPTOR_HEAP_FLAG_NONE 0
typedef struct { size_t ptr; } handle;
typedef struct object object;
struct vtable { void (*AddRef)(object *); };
struct object { struct vtable *lpVtbl; unsigned refs; };
typedef struct {
   struct { unsigned Type,NumDescriptors,Flags; } desc;
   object *handle;
   void *map;
   handle cpu,gpu;
   unsigned stride;
} heap;
typedef struct { unsigned SizeInBytes; } buffer_view;
typedef struct { void *data; unsigned size; } uniform_sem_t;
typedef struct { void *texture_data; } texture_sem_t;
typedef struct { uniform_sem_t *uniforms; int uniform_count; } cbuffer_sem_t;
enum glslang_format { SLANG_FORMAT_UNKNOWN,SLANG_FORMAT_A2B10G10R10_UNORM_PACK32,
   SLANG_FORMAT_R16G16B16A16_SFLOAT };
typedef struct {
   texture_sem_t *textures;
   int texture_count;
   cbuffer_sem_t cbuffers[2];
   bool explicit_format;
   enum glslang_format format;
} pass_semantics_t;
typedef struct { handle rt_view; } texture;
typedef struct {
   object *pipe,*viewer_pipe;
   pass_semantics_t semantics,viewer_semantics;
   object *buffers[2];
   buffer_view buffer_view[2];
   float viewer_colour_rows[12];
   handle samplers,textures,textures_alt;
   texture rt,feedback;
} d3d12_pass_t;
struct preset { unsigned passes; int hdr_mode; unsigned history_size; };
typedef struct {
   struct preset *shader_preset;
   float frame_rect,output_size;
   texture luts[1];
   d3d12_pass_t pass[1];
   heap rtv_heap;
   handle tables;
   unsigned num_tables;
   gfx_media_history_ring_t history_ring;
} d3d12_filter_chain_t;
typedef struct { void *device; unsigned cpu_write_heap; struct { heap srv_heap; } desc; } d3d12_video_t;
static unsigned live_buffers,live_heaps,live_tables,creates,fail_create;
static void addref(object *o) { o->refs++; }
static struct vtable vt={addref};
static enum glslang_format glslang_hdr_output_encoding(struct preset *p,enum glslang_format f)
{ return p->hdr_mode==0 ? SLANG_FORMAT_UNKNOWN : p->hdr_mode==1 ?
   SLANG_FORMAT_A2B10G10R10_UNORM_PACK32 : p->hdr_mode==2 ? SLANG_FORMAT_R16G16B16A16_SFLOAT : f; }
static void d3d12_init_descriptor_heap(void *d,heap *h)
{ (void)d; h->handle=(object*)calloc(1,sizeof(object));assert(h->handle);h->map=h;h->cpu.ptr=64;h->stride=4;live_heaps++; }
static handle d3d12_descriptor_heap_range_alloc(heap *h,unsigned n)
{ handle r={128};(void)h;assert(n==8);live_tables++;return r; }
static void d3d12_cb_create(void *d,unsigned kind,buffer_view *v,unsigned bytes,object **out)
{
   (void)d;(void)kind;
   if (++creates==fail_create) return;
   *out=(object*)calloc(1,sizeof(object));assert(*out);v->SizeInBytes=bytes;live_buffers++;
}
static void d3d12_views_chain_destroy(d3d12_video_t *v,d3d12_filter_chain_t *c)
{
   unsigned i,j;(void)v;
   for(i=0;i<c->shader_preset->passes;i++)
   {
      if(c->pass[i].pipe) c->pass[i].pipe->refs--;
      free(c->pass[i].semantics.textures);
      for(j=0;j<2;j++)
      {
         free(c->pass[i].semantics.cbuffers[j].uniforms);
         if(c->pass[i].buffers[j]) { free(c->pass[i].buffers[j]);live_buffers--; }
      }
   }
   if(c->rtv_heap.handle) { free(c->rtv_heap.handle);live_heaps--; }
   if(c->tables.ptr) live_tables--;
   free(c);
}
CLONE
OUTPUT_MODE
int main(void)
{
   d3d12_video_t v;
   d3d12_filter_chain_t src;
   struct preset preset={1,-1,2};
   object core={&vt,1},viewer={&vt,1},buffer={&vt,1};
   uniform_sem_t uniforms[4];texture_sem_t textures[2];
   unsigned i,mode;
   memset(&v,0,sizeof(v));memset(&src,0,sizeof(src));memset(uniforms,0,sizeof(uniforms));
   src.shader_preset=&preset;src.pass[0].pipe=&core;src.pass[0].viewer_pipe=&viewer;
   for(i=0;i<3;i++) { uniforms[i].data=src.pass[0].viewer_colour_rows+4*i;uniforms[i].size=16; }
   textures[0].texture_data=&src.luts[0];textures[1].texture_data=NULL;
   src.pass[0].viewer_semantics.textures=textures;src.pass[0].viewer_semantics.texture_count=1;
   src.pass[0].viewer_semantics.cbuffers[0].uniforms=uniforms;
   src.pass[0].viewer_semantics.cbuffers[0].uniform_count=3;
   src.pass[0].buffers[0]=src.pass[0].buffers[1]=&buffer;
   src.pass[0].buffer_view[0].SizeInBytes=256;src.pass[0].buffer_view[1].SizeInBytes=512;
   for(mode=0;mode<2;mode++)
   {
      d3d12_filter_chain_t *c=d3d12_chain_clone(&v,&src,mode!=0);
      assert(c && live_buffers==2 && live_tables==1 && live_heaps==1);
      assert(c->history_ring.count==(mode ? 3u : 0u));
      assert(c->history_ring.order==(unsigned*)(c+1));
      assert(c->pass[0].pipe==(mode ? &viewer : &core));
      assert(c->pass[0].buffer_view[1].SizeInBytes==512);
      if(mode)
      {
         assert(c->pass[0].semantics.textures[0].texture_data==&c->luts[0]);
         for(i=0;i<3;i++) assert(c->pass[0].semantics.cbuffers[0].uniforms[i].data==c->pass[0].viewer_colour_rows+4*i);
      }
      assert(src.pass[0].viewer_semantics.cbuffers[0].uniforms==uniforms);
      d3d12_views_chain_destroy(&v,c);
      assert(core.refs==1 && viewer.refs==1 && !live_buffers && !live_tables && !live_heaps);
   }
   for(mode=1;mode<=2;mode++)
   {
      fail_create=creates+mode;
      assert(!d3d12_chain_clone(&v,&src,true));
      assert(core.refs==1 && viewer.refs==1 && !live_buffers && !live_tables && !live_heaps);
   }
   src.pass[0].viewer_pipe=NULL;fail_create=0;
   { d3d12_filter_chain_t *c=d3d12_chain_clone(&v,&src,true);
     assert(c && c->pass[0].pipe==&core);d3d12_views_chain_destroy(&v,c); }
   src.pass[0].semantics.format=SLANG_FORMAT_R16G16B16A16_SFLOAT;
   assert(d3d12_viewer_output_mode(&src)==0);
   src.pass[0].semantics.explicit_format=true;
   assert(d3d12_viewer_output_mode(&src)==2);
   src.pass[0].semantics.format=SLANG_FORMAT_A2B10G10R10_UNORM_PACK32;
   assert(d3d12_viewer_output_mode(&src)==1);
   preset.hdr_mode=0;assert(d3d12_viewer_output_mode(&src)==0);
   preset.hdr_mode=2;assert(d3d12_viewer_output_mode(&src)==2);
   preset.hdr_mode=1;assert(d3d12_viewer_output_mode(&src)==1);
   puts("PASS D3D12 viewer: prebuilt pipeline, rebased bindings, allocation failures, SDR/linear/PQ output");
   return 0;
}
'''.replace("CLONE", function("static d3d12_filter_chain_t *d3d12_chain_clone(")).replace(
    "OUTPUT_MODE", function("static unsigned d3d12_viewer_output_mode("))
cc = shlex.split(os.environ.get("CC", "cc"))
with tempfile.TemporaryDirectory(prefix="d3d12-viewer-colour-") as directory:
    unit = Path(directory) / "test.c"
    exe = Path(directory) / ("test.exe" if os.name == "nt" or "mingw" in " ".join(cc) else "test")
    unit.write_bytes(fixture.encode("utf-8"))
    subprocess.run(cc + shlex.split(os.environ.get("CFLAGS", "")) + [
        "-std=gnu89", "-Wall", "-Wextra", "-Werror", "-Werror=declaration-after-statement",
        "-I" + str(root), "-I" + str(root / "libretro-common/include"),
        str(unit), "-o", str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
