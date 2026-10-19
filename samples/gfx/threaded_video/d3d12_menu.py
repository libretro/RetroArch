#!/usr/bin/env python3
"""Run shipping D3D12 menu loans against held fences and padded uploads."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/drivers/d3d12.c").read_text(encoding="utf-8")
functions = ""
for name in ("static void d3d12_menu_mapping(",
             "static void *d3d12_get_menu_framebuffer(",
             "static void d3d12_set_menu_texture_frame(",
             "static bool d3d12_frame_mem_ready(",
             "static void d3d12_menu_upload(",
             "static void d3d12_frame_mem_free(void *data, uintptr_t id)\n{"):
    start = source.index(name)
    functions += source[start:source.index("\n}\n", start) + 3]
fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int bool;
#define true 1
#define false 0
typedef unsigned DXGI_FORMAT;
typedef uint64_t UINT64;
typedef struct { size_t Begin, End; } D3D12_RANGE;
#define D3D12_FRAME_MEM_MAX 20
#define D3D12_TEXTURE_DATA_PITCH_ALIGNMENT 256
typedef void *D3D12GraphicsCommandList;
typedef void *D3D12Resource;
typedef struct { size_t Offset; struct { unsigned RowPitch; } Footprint; } D3D12_PLACED_SUBRESOURCE_FOOTPRINT;
#define DXGI_FORMAT_B8G8R8A8_UNORM 32
#define DXGI_FORMAT_B4G4R4A4_UNORM 16
#define DXGI_FORMAT_EX_A4R4G4B4_UNORM 116
#define VIDEO_SCALE_PACK(w,h) (((w) << 16) | (h))
#define VIDEO_SCALE_W(d) ((d) >> 16)
#define VIDEO_SCALE_H(d) ((d) & 65535)
#define RARCH_FILTER_LINEAR 1
#define RARCH_FILTER_NEAREST 0
#define RARCH_WRAP_DEFAULT 0
typedef struct { float color[4]; } d3d12_vertex_t;
struct fence;
struct fence_vtable { UINT64 (*GetCompletedValue)(struct fence*); };
struct fence { struct fence_vtable *lpVtbl; };
typedef struct {
   void *handle, *upload_buffer, *srv_heap;
   struct { unsigned Width, Height, Format, MipLevels; } desc;
   D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
   UINT64 upload_fence;
   uint8_t *lend_mapped[2];
   unsigned lent, pending;
   UINT64 lend_fence[2];
   bool dirty;
   unsigned sampler, srv_mapping;
   unsigned cpu_descriptor[1];
} d3d12_texture_t;
#define D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(a,b,c,d) (4096u | (a) | ((b)<<3) | ((c)<<6) | ((d)<<9))
#define D3D12_SRV_DIMENSION_TEXTURE2D 3
typedef struct { unsigned Format, Shader4ComponentMapping, ViewDimension; struct { unsigned MipLevels; } Texture2D; } D3D12_SHADER_RESOURCE_VIEW_DESC;
struct device;
struct device_vtable { void (*CreateShaderResourceView)(struct device*, void*, const D3D12_SHADER_RESOURCE_VIEW_DESC*, unsigned); };
struct device { struct device_vtable *lpVtbl; };
typedef struct device *D3D12Device;
static unsigned mappings;
static void create_srv(struct device *device, void *resource,
      const D3D12_SHADER_RESOURCE_VIEW_DESC *desc, unsigned handle)
{
   (void)device; (void)handle;
   assert(resource && desc->Format == 16 && desc->Texture2D.MipLevels == 1);
   assert(desc->Shader4ComponentMapping == D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(1,0,3,2));
   mappings++;
}
typedef struct {
   struct { d3d12_texture_t texture; DXGI_FORMAT source_format; void *vbo; float alpha; unsigned upload; } menu;
   struct { struct fence *fence; UINT64 frameSignal, fenceValue; } queue;
   struct { D3D12Resource buffer; void *mapped; UINT64 size, fence; bool menu, pending; } frame_mem[D3D12_FRAME_MEM_MAX];
   struct { int srv_heap; } desc;
   void *device;
   unsigned samplers[2][1];
} d3d12_video_t;
typedef struct { struct { bool menu_linear_filter; } bools; } settings_t;
static settings_t settings;
static UINT64 completed;
static unsigned copies, maps, releases, waits;
static int copied_src_pitch;
static unsigned copied_format;
static unsigned refuse;
static uint8_t uploads[2][512 * 4];
static d3d12_vertex_t vertices[4];
static settings_t *config_get_ptr(void) { return &settings; }
static UINT64 get_completed(struct fence *fence) { (void)fence; return completed; }
static void d3d12_fence_wait(d3d12_video_t *d, UINT64 value)
{
   (void)d;
   (void)value;
   waits++;
}
static void d3d12_release_texture(d3d12_video_t *d, d3d12_texture_t *t)
{
   (void)d;
   releases++;
   t->lent = 0;
   t->lend_mapped[0] = t->lend_mapped[1] = NULL;
}
static void d3d12_init_texture(void *device, d3d12_texture_t *t)
{
   (void)device;
   assert(t->srv_mapping == 0);
   if (t->desc.Format == DXGI_FORMAT_EX_A4R4G4B4_UNORM)
      t->desc.Format = DXGI_FORMAT_B4G4R4A4_UNORM;
   t->handle = t->upload_buffer = t;
   t->desc.MipLevels = 1;
   t->layout.Footprint.RowPitch = (t->desc.Width * (t->desc.Format == 16 ? 2 : 4) + 255) & ~255u;
}
static void d3d12_update_texture(unsigned w, unsigned h, int pitch,
      DXGI_FORMAT format, const void *frame, d3d12_texture_t *t)
{
   (void)w; (void)h; (void)pitch; (void)format; (void)frame;
   copies++;
   copied_src_pitch = pitch;
   copied_format = format;
   t->dirty = true;
}
static void dxgi_copy(unsigned w, unsigned h, DXGI_FORMAT src_format,
      int pitch, const void *frame, DXGI_FORMAT dst_format,
      unsigned dst_pitch, void *dst)
{
   (void)w; (void)h; (void)src_format; (void)pitch; (void)frame;
   (void)dst_format; (void)dst_pitch; (void)dst;
   assert(frame != dst);
   copies++;
   copied_src_pitch = pitch;
   copied_format = src_format;
}
#define D3D12Map(vbo, sub, range, out) ((void)(vbo), (void)(sub), (void)(range), *(out) = vertices)
#define D3D12Unmap(vbo, sub, range) ((void)(vbo), (void)(sub), (void)(range))
static void *d3d12_texture_lend(void *data, uintptr_t id, unsigned slot,
      size_t tight_pitch, size_t *out_pitch)
{
   d3d12_texture_t *t = (d3d12_texture_t*)id;
   (void)data;
   assert(slot < 2 && tight_pitch <= t->layout.Footprint.RowPitch);
   maps++;
   *out_pitch = t->layout.Footprint.RowPitch;
   if (refuse & (1u << slot))
      return NULL;
   t->lent |= 1u << slot;
   t->lend_mapped[slot] = uploads[slot];
   return uploads[slot];
}
typedef void IUnknown;
static unsigned freed, retired;
static void release_resource(void *resource) { assert(resource); freed++; }
#define Release(resource) release_resource(resource)
static void d3d12_retire(d3d12_video_t *d, void *texture, IUnknown *resource, UINT64 fence)
{
   (void)d; (void)texture; (void)resource; (void)fence;
   retired++;
}
static D3D12Resource copied_buffer;
static unsigned copied_pitch;
static void d3d12_upload_texture_from(D3D12GraphicsCommandList cmd,
      d3d12_texture_t *t, void *data, D3D12Resource buffer,
      const D3D12_PLACED_SUBRESOURCE_FOOTPRINT *layout, const void *box)
{
   d3d12_video_t *d = (d3d12_video_t*)data;
   (void)cmd; (void)box;
   assert(!layout->Offset);
   copied_buffer = buffer;
   copied_pitch = layout->Footprint.RowPitch;
   t->upload_fence = d->queue.fenceValue + 1;
   t->dirty = false;
}
static void d3d12_upload_texture(D3D12GraphicsCommandList cmd,
      d3d12_texture_t *t, void *data)
{
   (void)cmd; (void)t; (void)data;
}
FUNCTIONS
int main(void)
{
   struct device_vtable api = { create_srv };
   struct device device = { &api };
   struct fence_vtable vtable = { get_completed };
   struct fence fence = { &vtable };
   d3d12_video_t d;
   d3d12_texture_t *t = &d.menu.texture;
   unsigned mode;
   const unsigned dims = VIDEO_SCALE_PACK(65, 4);
   size_t pitch;
   void *loan;
   uint8_t fallback[65 * 4 * 4];
   memset(fallback, 0x5a, sizeof(fallback));
   for (mode = 0; mode < 2; mode++)
   {
      memset(&d, 0, sizeof(d));
      d.queue.fence = &fence;
      d.device = &device;
      completed = 5;
      copies = maps = releases = waits = refuse = mappings = 0;
      d3d12_set_menu_texture_frame(&d, fallback, mode, dims, 1.0f);
      assert(copies == 1 && releases == 1);
      assert(copied_format == (mode ? 32 : 16));
      assert(mappings == (mode ? 0 : 1));
      /* Pending normal uploads cannot be overwritten by a loan. */
      pitch = 65 * (mode ? 4 : 2);
      loan = d3d12_get_menu_framebuffer(&d, mode, dims, &pitch);
      assert(loan == uploads[1] && pitch == (mode ? 512 : 256));
      memset(loan, 0x71, pitch * 4);
      if (!mode)
      {
         unsigned x, y;
         for (y = 0; y < 4; y++)
            for (x = 0; x < 65; x++)
               ((uint16_t*)((uint8_t*)loan + y * pitch))[x] = 0x1234;
      }
      d3d12_set_menu_texture_frame(&d, loan, mode, dims, 0.5f);
      assert(copies == 1 && t->pending == 1 && t->dirty);
      assert(vertices[0].color[3] == 0.5f);
      assert(mappings == (mode ? 0 : 1));
      if (!mode)
      {
         unsigned x, y;
         for (y = 0; y < 4; y++)
         {
            for (x = 0; x < 65; x++)
               assert(((uint16_t*)((uint8_t*)loan + y * pitch))[x] == 0x1234);
            assert(((uint8_t*)loan)[y * pitch + 130] == 0x71);
         }
      }
      /* Recording a copy stamps the selected slot's retirement fence. */
      t->dirty = false;
      t->lend_fence[1] = 6;
      t->lend_fence[0] = 7;
      maps = 0;
      pitch = 123;
      assert(!d3d12_get_menu_framebuffer(&d, mode, dims, &pitch));
      assert(!maps && pitch == 123 && ((uint8_t*)loan)[0] == (mode ? 0x71 : 0x34));
      completed = 6;
      assert(d3d12_get_menu_framebuffer(&d, mode, dims, &pitch) == uploads[1]);
      d3d12_set_menu_texture_frame(&d, uploads[1], mode, dims, 1.0f);
      assert(copies == 1);
      /* A disabled menu keeps its unsubmitted upload pending. */
      assert(!d3d12_get_menu_framebuffer(&d, mode, dims, &pitch));
      completed = 7;
      assert(d3d12_get_menu_framebuffer(&d, mode, dims, &pitch) == uploads[0]);
      maps = waits;
      d3d12_set_menu_texture_frame(&d, uploads[0], mode, dims, 1.0f);
      assert(copies == 1 && t->pending == 0 && waits == maps);
      d3d12_set_menu_texture_frame(&d, fallback, mode, dims, 1.0f);
      assert(copies == 2 && t->pending == 0);
      d3d12_set_menu_texture_frame(&d, NULL, mode, dims, 1.0f);
      assert(copies == 2 && t->pending == 0);
      t->dirty = false;
      refuse = 3;
      pitch = 123;
      assert(!d3d12_get_menu_framebuffer(&d, mode, dims, &pitch));
      assert(pitch == 123);
      refuse = 0;
      assert(!d3d12_get_menu_framebuffer(&d, !mode, dims, &pitch));
      assert(!d3d12_get_menu_framebuffer(&d, mode, VIDEO_SCALE_PACK(66, 4), &pitch));
      assert(!d3d12_get_menu_framebuffer(NULL, mode, dims, &pitch));
      assert(!d3d12_get_menu_framebuffer(&d, mode, dims, NULL));
      assert(!d3d12_get_menu_framebuffer(&d, mode, 0, &pitch));
      /* The threaded pool holds unsubmitted sources, then stamps the recorded copy. */
      d.frame_mem[0].buffer = uploads[0];
      d.frame_mem[0].mapped = uploads[0];
      d.frame_mem[0].size = sizeof(uploads[0]);
      d.frame_mem[0].menu = true;
      d.frame_mem[1].buffer = uploads[1];
      d.frame_mem[1].mapped = uploads[1];
      d.frame_mem[1].size = sizeof(uploads[1]);
      d.frame_mem[1].menu = true;
      t->lent = 0;
      maps = waits;
      memset(uploads[0], 0x12, sizeof(uploads[0]));
      d3d12_set_menu_texture_frame(&d, uploads[0], mode, dims, 1.0f);
      assert(((uint16_t*)uploads[0])[0] == 0x1212);
      assert(((uint16_t*)uploads[0])[128] == 0x1212);
      assert(copies == 2 && waits == maps && d.menu.upload == 1);
      assert(d.frame_mem[0].pending && !d3d12_frame_mem_ready(&d, 1));
      d3d12_set_menu_texture_frame(&d, uploads[1], mode, dims, 1.0f);
      assert(!d.frame_mem[0].pending && d3d12_frame_mem_ready(&d, 1));
      assert(d.frame_mem[1].pending && !d3d12_frame_mem_ready(&d, 2));
      d.queue.fenceValue = completed;
      d3d12_menu_upload(NULL, &d);
      assert(!d.menu.upload && !d.frame_mem[1].pending && !t->dirty);
      assert(copied_buffer == uploads[1] && copied_pitch == (mode ? 512 : 256));
      assert(d.frame_mem[1].fence == completed + 1);
      assert(!d3d12_frame_mem_ready(&d, 2));
      completed++;
      assert(d3d12_frame_mem_ready(&d, 2));
      /* Converted sources copy with the pooled pitch and are immediately reusable. */
      t->desc.Format = 99;
      d3d12_set_menu_texture_frame(&d, uploads[0], mode, dims, 1.0f);
      assert(copies == 3 && !d.menu.upload && !d.frame_mem[0].pending);
      assert(copied_src_pitch == (mode ? 512 : 256));
      assert(copied_format == (mode ? 32 : 116));
      d3d12_set_menu_texture_frame(&d, uploads[0], mode,
            VIDEO_SCALE_PACK(65535, 4), 1.0f);
      assert(copies == 3 && releases == 1);
      t->desc.Format = mode ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_B4G4R4A4_UNORM;
      d3d12_set_menu_texture_frame(&d, uploads[0], mode, dims, 1.0f);
      assert(d.frame_mem[0].pending && d.menu.upload == 1);
      freed = retired = 0;
      d3d12_frame_mem_free(&d, 1);
      assert(freed == 1 && !retired && !d.menu.upload && !t->dirty);
      assert(!d.frame_mem[0].buffer && !d.frame_mem[0].mapped);
      d.frame_mem[1].fence = completed + 1;
      d3d12_frame_mem_free(&d, 2);
      assert(freed == 1 && retired == 1);
      t->desc.Format = 99;

      t->lent = 3;
      t->lend_mapped[0] = uploads[0];
      t->lend_mapped[1] = uploads[1];
      /* A GPU conversion fallback cannot accept the native source bytes. */
      t->desc.Format = 99;
      assert(!d3d12_get_menu_framebuffer(&d, mode, dims, &pitch));
      /* Mismatched publication of a loan is rejected before recreation. */
      d3d12_set_menu_texture_frame(&d, uploads[0], mode,
            VIDEO_SCALE_PACK(66, 4), 1.0f);
      assert(releases == 1 && copies == 3);
   }
   puts("PASS: D3D12 padded menu loans, SRV mapping, pending uploads, fences and copy fallback");
   return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="retroarch-d3d12-menu-") as directory:
    src = Path(directory) / "menu.c"
    exe = Path(directory) / "menu.exe"
    src.write_text(fixture.replace("FUNCTIONS", functions), encoding="utf-8")
    command = shlex.split(os.environ.get("CC", "cc"))
    command += ["-std=gnu89", "-Wall", "-Wextra", "-Werror"]
    command += shlex.split(os.environ.get("CFLAGS", ""))
    subprocess.run(command + [str(src), "-o", str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
