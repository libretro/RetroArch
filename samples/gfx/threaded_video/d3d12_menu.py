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
for name in ("static void *d3d12_get_menu_framebuffer(",
             "static void d3d12_set_menu_texture_frame("):
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
   struct { unsigned Width, Height, Format; } desc;
   struct { struct { unsigned RowPitch; } Footprint; } layout;
   uint8_t *lend_mapped[2];
   unsigned lent, pending;
   UINT64 lend_fence[2];
   bool dirty;
   unsigned sampler;
} d3d12_texture_t;
typedef struct {
   struct { d3d12_texture_t texture; DXGI_FORMAT source_format; void *vbo; float alpha; } menu;
   struct { struct fence *fence; UINT64 frameSignal; } queue;
   struct { int srv_heap; } desc;
   void *device;
   unsigned samplers[2][1];
} d3d12_video_t;
typedef struct { struct { bool menu_linear_filter; } bools; } settings_t;
static settings_t settings;
static UINT64 completed;
static unsigned copies, maps, releases, waits;
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
static void d3d12_release_texture(d3d12_texture_t *t)
{
   releases++;
   t->lent = 0;
   t->lend_mapped[0] = t->lend_mapped[1] = NULL;
}
static void d3d12_init_texture(void *device, d3d12_texture_t *t)
{
   (void)device;
   if (t->desc.Format == DXGI_FORMAT_EX_A4R4G4B4_UNORM)
      t->desc.Format = DXGI_FORMAT_B4G4R4A4_UNORM;
   t->handle = t->upload_buffer = t;
   t->layout.Footprint.RowPitch = 256;
}
static void d3d12_update_texture(unsigned w, unsigned h, int pitch,
      DXGI_FORMAT format, const void *frame, d3d12_texture_t *t)
{
   (void)w; (void)h; (void)pitch; (void)format; (void)frame;
   copies++;
   t->dirty = true;
}
static void dxgi_copy(unsigned w, unsigned h, DXGI_FORMAT src_format,
      int pitch, const void *frame, DXGI_FORMAT dst_format,
      unsigned dst_pitch, void *dst)
{
   (void)w; (void)h; (void)src_format; (void)pitch; (void)frame;
   (void)dst_format; (void)dst_pitch; (void)dst;
   copies++;
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
FUNCTIONS
int main(void)
{
   struct fence_vtable vtable = { get_completed };
   struct fence fence = { &vtable };
   d3d12_video_t d;
   d3d12_texture_t *t = &d.menu.texture;
   unsigned mode;
   const unsigned dims = VIDEO_SCALE_PACK(33, 4);
   size_t pitch;
   void *loan;
   uint8_t fallback[33 * 4 * 4];
   memset(fallback, 0x5a, sizeof(fallback));
   for (mode = 0; mode < 2; mode++)
   {
      memset(&d, 0, sizeof(d));
      d.queue.fence = &fence;
      completed = 5;
      copies = maps = releases = waits = refuse = 0;
      d3d12_set_menu_texture_frame(&d, fallback, mode, dims, 1.0f);
      assert(copies == 1 && releases == 1);
      /* Pending normal uploads cannot be overwritten by a loan. */
      pitch = 33 * (mode ? 4 : 2);
      loan = d3d12_get_menu_framebuffer(&d, mode, dims, &pitch);
      assert(loan == uploads[1] && pitch == 256);
      memset(loan, 0x71, 256 * 4);
      d3d12_set_menu_texture_frame(&d, loan, mode, dims, 0.5f);
      assert(copies == 1 && t->pending == 1 && t->dirty);
      assert(vertices[0].color[3] == 0.5f);
      /* Recording a copy stamps the selected slot's retirement fence. */
      t->dirty = false;
      t->lend_fence[1] = 6;
      t->lend_fence[0] = 7;
      maps = 0;
      pitch = 123;
      assert(!d3d12_get_menu_framebuffer(&d, mode, dims, &pitch));
      assert(!maps && pitch == 123 && ((uint8_t*)loan)[0] == 0x71);
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
      assert(!d3d12_get_menu_framebuffer(&d, mode, VIDEO_SCALE_PACK(34, 4), &pitch));
      assert(!d3d12_get_menu_framebuffer(NULL, mode, dims, &pitch));
      assert(!d3d12_get_menu_framebuffer(&d, mode, dims, NULL));
      assert(!d3d12_get_menu_framebuffer(&d, mode, 0, &pitch));
      /* A GPU conversion fallback cannot accept the native source bytes. */
      t->desc.Format = 99;
      assert(!d3d12_get_menu_framebuffer(&d, mode, dims, &pitch));
      /* Mismatched publication of a loan is rejected before recreation. */
      d3d12_set_menu_texture_frame(&d, uploads[0], mode,
            VIDEO_SCALE_PACK(34, 4), 1.0f);
      assert(releases == 1 && copies == 2);
   }
   puts("PASS: D3D12 padded menu loans, pending uploads, fences and copy fallback");
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
