#!/usr/bin/env python3
"""The shipping viewer clone borrows the prebuilt shader variant and owns its bindings."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/drivers/d3d11.c").read_text(encoding="utf-8")
start = source.index("static d3d11_chain_t *d3d11_chain_clone(")
end = source.index("\n}\n", start) + 3
def function(name):
    begin = source.index("static void " + name + "(")
    return source[begin:source.index("\n}\n", begin) + 3]

fixture = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SLANG_CBUFFER_MAX 2
#define FAILED(x) ((x) != 0)
typedef struct object object;
typedef struct { unsigned ByteWidth; } D3D11_BUFFER_DESC;
struct vtable { void (*GetDesc)(object *,D3D11_BUFFER_DESC *); };
struct object { struct vtable *lpVtbl; unsigned refs,bytes; bool owned; };
typedef struct { int (*CreateBuffer)(void *,const D3D11_BUFFER_DESC *,void *,object **); } device_vtable;
typedef struct { device_vtable *lpVtbl; } device;
typedef struct { device *device; } d3d11_video_t;
typedef struct { object *vs,*ps,*gs,*layout; } shader;
typedef struct { object *handle,*staging,*view,*rt_view; } texture;
typedef struct { void *data; unsigned size; } uniform_sem_t;
typedef struct { void *texture_data; } texture_sem_t;
typedef struct { uniform_sem_t *uniforms; int uniform_count; } cbuffer_sem_t;
typedef struct { texture_sem_t *textures; int texture_count; cbuffer_sem_t cbuffers[2]; } pass_semantics_t;
typedef struct {
   shader shader,viewer_shader;
   pass_semantics_t semantics,viewer_semantics;
   object *buffers[2];
   float viewer_colour_rows[12];
} d3d11_pass_t;
struct preset { unsigned passes,luts; };
typedef struct {
   struct preset *shader_preset;
   float frame_rect,mvp_last_pass;
   texture luts[1];
   d3d11_pass_t pass[1];
} d3d11_chain_t;
static unsigned buffers,creates,fail_create;
static void d3d11_add_ref(object *o) { if (o) o->refs++; }
static void release(object *o)
{
   if (o && !--o->refs && o->owned) { buffers--; free(o); }
}
#define Release(o) release(o)
static void d3d11_release_shader(shader *s)
{ release(s->vs);release(s->ps);release(s->gs);release(s->layout); }
SEMANTICS_FREE
RELEASE_PROGRAMS
static void get_desc(object *o,D3D11_BUFFER_DESC *d) { d->ByteWidth=o->bytes; }
static struct vtable object_vtable={get_desc};
static int create_buffer(void *d,const D3D11_BUFFER_DESC *desc,void *data,object **out)
{
   object *o;
   (void)d;(void)data;
   if (++creates==fail_create) return -1;
   o=(object*)calloc(1,sizeof(*o));assert(o);
   o->lpVtbl=&object_vtable;o->refs=1;o->bytes=desc->ByteWidth;o->owned=true;
   *out=o;buffers++;return 0;
}
static void d3d11_chain_free(d3d11_chain_t *c,bool shared)
{
   unsigned i;
   (void)shared;
   for(i=0;i<c->shader_preset->luts;i++)
   {
      release(c->luts[i].handle);release(c->luts[i].staging);
      release(c->luts[i].view);release(c->luts[i].rt_view);
   }
   for(i=0;i<c->shader_preset->passes;i++)
   {
      d3d11_pass_release_programs(&c->pass[i]);
   }
}
FUNCTION
int main(void)
{
   struct preset preset={1,1};
   device_vtable dv={create_buffer};
   device dev={&dv};
   d3d11_video_t video={&dev};
   d3d11_chain_t src;
   object objects[8];
   uniform_sem_t normal[2],viewer[4];
   texture_sem_t textures[2];
   unsigned i,mode;
   memset(&src,0,sizeof(src));memset(objects,0,sizeof(objects));
   memset(normal,0,sizeof(normal));memset(viewer,0,sizeof(viewer));
   for(i=0;i<8;i++) { objects[i].refs=1;objects[i].lpVtbl=&object_vtable;objects[i].bytes=128; }
   src.shader_preset=&preset;src.luts[0].view=&objects[0];
   src.pass[0].shader.vs=&objects[1];src.pass[0].shader.ps=&objects[2];
   src.pass[0].shader.layout=&objects[3];
   src.pass[0].viewer_shader.vs=&objects[4];src.pass[0].viewer_shader.ps=&objects[5];
   src.pass[0].viewer_shader.layout=&objects[3];
   src.pass[0].buffers[0]=&objects[6];src.pass[0].buffers[1]=&objects[7];
   for(i=0;i<3;i++) { viewer[i].data=src.pass[0].viewer_colour_rows+4*i;viewer[i].size=16; }
   normal[0].data=&src.frame_rect;normal[0].size=sizeof(float);
   src.pass[0].semantics.cbuffers[0].uniforms=normal;
   src.pass[0].semantics.cbuffers[0].uniform_count=1;
   src.pass[0].viewer_semantics.cbuffers[0].uniforms=viewer;
   src.pass[0].viewer_semantics.cbuffers[0].uniform_count=3;
   textures[0].texture_data=&src.luts[0].view;textures[1].texture_data=NULL;
   src.pass[0].semantics.textures=textures;src.pass[0].semantics.texture_count=1;
   src.pass[0].viewer_semantics.textures=textures;src.pass[0].viewer_semantics.texture_count=1;
   for(mode=0;mode<2;mode++)
   {
      d3d11_chain_t *c=d3d11_chain_clone(&video,&src,mode!=0);
      assert(c && creates==2*(mode+1) && buffers==2);
      assert(c->pass[0].shader.ps==(mode ? &objects[5]:&objects[2]));
      assert(c->pass[0].semantics.textures[0].texture_data==&c->luts[0].view);
      if(mode)
         for(i=0;i<3;i++) assert(c->pass[0].semantics.cbuffers[0].uniforms[i].data==c->pass[0].viewer_colour_rows+4*i);
      else assert(c->pass[0].semantics.cbuffers[0].uniforms[0].data==&c->frame_rect);
      assert(src.pass[0].semantics.cbuffers[0].uniforms==normal);
      assert(src.pass[0].viewer_semantics.cbuffers[0].uniforms==viewer);
      d3d11_chain_free(c,true);free(c);assert(!buffers);
      for(i=0;i<8;i++) assert(objects[i].refs==1);
   }
   for(mode=1;mode<=2;mode++)
   {
      d3d11_chain_t *c;
      fail_create=creates+mode;
      c=d3d11_chain_clone(&video,&src,true);
      assert(!c && !buffers);
      for(i=0;i<8;i++) assert(objects[i].refs==1);
   }
   {
      d3d11_pass_t dual;
      memset(&dual,0,sizeof(dual));
      dual.shader=src.pass[0].shader;dual.viewer_shader=src.pass[0].viewer_shader;
      d3d11_add_ref(dual.shader.vs);d3d11_add_ref(dual.shader.ps);d3d11_add_ref(dual.shader.layout);
      d3d11_add_ref(dual.viewer_shader.vs);d3d11_add_ref(dual.viewer_shader.ps);d3d11_add_ref(dual.viewer_shader.layout);
      dual.semantics.textures=(texture_sem_t*)calloc(1,sizeof(texture_sem_t));
      dual.viewer_semantics.textures=(texture_sem_t*)calloc(1,sizeof(texture_sem_t));
      dual.semantics.cbuffers[0].uniforms=(uniform_sem_t*)calloc(1,sizeof(uniform_sem_t));
      dual.viewer_semantics.cbuffers[0].uniforms=(uniform_sem_t*)calloc(1,sizeof(uniform_sem_t));
      d3d11_pass_release_programs(&dual);d3d11_pass_release_programs(&dual);
      assert(!dual.shader.vs && !dual.viewer_shader.ps);
      for(i=0;i<8;i++) assert(objects[i].refs==1);
   }
   puts("PASS: precompiled viewer shader borrowed, bindings rebased, core unchanged, failed allocations and repeated cleanup released");
   return 0;
}
'''.replace("FUNCTION", source[start:end]).replace(
    "SEMANTICS_FREE", function("d3d11_semantics_free")).replace(
    "RELEASE_PROGRAMS", function("d3d11_pass_release_programs"))
cc = shlex.split(os.environ.get("CC", "cc"))
with tempfile.TemporaryDirectory(prefix="d3d11-viewer-colour-") as directory:
    unit = Path(directory) / "test.c"
    exe = Path(directory) / ("test.exe" if os.name == "nt" or "mingw" in " ".join(cc) else "test")
    unit.write_bytes(fixture.encode("utf-8"))
    subprocess.run(cc + shlex.split(os.environ.get("CFLAGS", "")) + [
        "-std=gnu89", "-Wall", "-Wextra", "-Werror", "-Werror=declaration-after-statement",
        str(unit), "-o", str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
