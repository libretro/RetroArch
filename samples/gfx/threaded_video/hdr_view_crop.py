#!/usr/bin/env python3
"""Exercise shipping HDR software-view crop addressing across backends."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/drivers/d3d12.c").read_text()
source11 = (root / "gfx/drivers/d3d11.c").read_text()
source_gl = (root / "gfx/drivers/gl3.c").read_text()


def function(name):
    start = source.index("static ", source.index(name) - 32)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define true 1
typedef unsigned DXGI_FORMAT;
typedef void *D3D12Resource;
typedef void *D3D12GraphicsCommandList;
typedef struct { unsigned left, top, front, right, bottom, back; } D3D12_BOX;
typedef struct { unsigned Offset; } D3D12_PLACED_SUBRESOURCE_FOOTPRINT;
typedef struct { void *upload_buffer; } d3d12_texture_t;
typedef struct { DXGI_FORMAT format; struct { void *cmd; } queue; } d3d12_video_t;
#define DXGI_FORMAT_B5G6R5_UNORM 1
#define DXGI_FORMAT_B8G8R8X8_UNORM 2
#define DXGI_FORMAT_R10G10B10A2_UNORM 3
#define DXGI_FORMAT_R16G16B16A16_FLOAT 4
struct d3d12_sw_src
{
   D3D12Resource buffer;
   D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
   D3D12_BOX window;
   const void *frame;
   unsigned pitch;
};
BPP
static unsigned copies, uploads, gpu_copies;
static unsigned char output[3 * 2 * 8];
static D3D12_BOX last_box;
static void d3d12_update_texture(unsigned w, unsigned h, unsigned pitch,
      DXGI_FORMAT format, const void *frame, d3d12_texture_t *texture)
{
   unsigned y;
   unsigned row = w * d3d12_frame_bpp(format);
   (void)texture;
   assert(w == 3 && h == 2);
   copies++;
   for (y = 0; y < h; y++)
      memcpy(output + y * row, (const unsigned char*)frame + y * pitch, row);
}
/* The upload buffer is always free here: one frame in flight */
static int d3d12_texture_upload_turn(d3d12_video_t *video,
      d3d12_texture_t *texture, int wait)
{
   (void)video;
   (void)texture;
   (void)wait;
   return 1;
}
static void d3d12_upload_texture(D3D12GraphicsCommandList cmd,
      d3d12_texture_t *texture, d3d12_video_t *video)
{
   (void)cmd;
   (void)texture;
   (void)video;
   uploads++;
}
static void d3d12_upload_texture_from(D3D12GraphicsCommandList cmd,
      d3d12_texture_t *texture, d3d12_video_t *video, D3D12Resource buffer,
      const D3D12_PLACED_SUBRESOURCE_FOOTPRINT *layout, const D3D12_BOX *box)
{
   (void)cmd;
   (void)texture;
   (void)video;
   assert(buffer && layout->Offset == 512);
   last_box = *box;
   gpu_copies++;
}
UPLOAD
struct retro_video_view { unsigned x, y, width, height; };
typedef struct
{
   struct { unsigned num_views; struct retro_video_view views[1]; } views;
} video_frame_info_t;
static void dxgi_copy(unsigned w, unsigned h, DXGI_FORMAT format,
      unsigned pitch, const void *frame, DXGI_FORMAT target,
      unsigned target_pitch, void *target_data)
{
   (void)target;
   (void)target_pitch;
   (void)target_data;
   d3d12_update_texture(w, h, pitch, format, frame, NULL);
}
static void crop_d3d11(DXGI_FORMAT format, const void *sw, unsigned pitch,
      const struct retro_video_view *v)
{
   d3d12_video_t state, *d3d11 = &state;
   struct { struct { struct { DXGI_FORMAT Format; } desc; } history[1]; } chain, *c = &chain;
   struct { unsigned RowPitch; void *pData; } mapped;
   state.format = format;
   {
D3D11_BPP
   d3d11->format = format;
   c->history[0].desc.Format = format;
   mapped.RowPitch = 128;
   mapped.pData = output;
D3D11_COPY
   }
}
typedef struct
{
   struct { unsigned rgb32, source_10bit, source_fp16; } video_info;
   struct { unsigned tex[1], copied, written_seen; } views;
} gl3_t;
struct gl3_filter_chain_texture { unsigned format; };
#define GL_TEXTURE_2D 1
#define RARCH_LOG(message) ((void)0)
static void gl3_views_tex(gl3_t *gl, unsigned i,
      const struct retro_video_view *v, unsigned format, unsigned swizzle)
{
   (void)gl;
   (void)i;
   (void)v;
   (void)format;
   (void)swizzle;
}
static void glBindTexture(unsigned target, unsigned texture)
{
   (void)target;
   (void)texture;
}
static void gl3_write_cpu_texture(gl3_t *gl, const void *frame,
      unsigned w, unsigned h, unsigned pitch)
{
   DXGI_FORMAT format = gl->video_info.source_fp16 ? 4
      : gl->video_info.source_10bit ? 3 : gl->video_info.rgb32 ? 2 : 1;
   d3d12_update_texture(w, h, pitch, format, frame, NULL);
}
GL3_UPLOAD
int main(void)
{
   unsigned char frame[5 * 128], saved[sizeof(frame)];
   d3d12_video_t video;
   d3d12_texture_t texture;
   struct d3d12_sw_src src;
   unsigned format, x, y, c, backend;
   memset(&video, 0, sizeof(video));
   memset(&src, 0, sizeof(src));
   texture.upload_buffer = &texture;
   src.frame = frame;
   src.pitch = 128;
   for (backend = 0; backend < 3; backend++)
   for (format = 1; format <= 4; format++)
   {
      unsigned bpp = format == 1 ? 2 : format == 4 ? 8 : 4;
      video.format = format;
      memset(frame, 0xCD, sizeof(frame));
      for (y = 0; y < 5; y++)
         for (x = 0; x < 9; x++)
            for (c = 0; c < bpp; c++)
               frame[y * 128 + x * bpp + c] = (unsigned char)(y * 43 + x * 9 + c);
      memcpy(saved, frame, sizeof(frame));
      for (x = 0; x <= 6; x++)
      {
         unsigned row;
         struct retro_video_view view;
         memset(output, 0xEF, sizeof(output));
         view.x = x;
         view.y = 2;
         view.width = 3;
         view.height = 2;
         if (!backend)
            d3d12_sw_upload(&video, &texture, &src, x, 2, 3, 2);
         else if (backend == 1)
            crop_d3d11(format, frame, src.pitch, &view);
         else
         {
            gl3_t gl;
            video_frame_info_t info;
            struct gl3_filter_chain_texture source_texture;
            memset(&gl, 0, sizeof(gl));
            gl.video_info.rgb32 = format == 2;
            gl.video_info.source_10bit = format == 3;
            gl.video_info.source_fp16 = format == 4;
            info.views.num_views = 1;
            info.views.views[0] = view;
            source_texture.format = format;
            gl3_views_write(&gl, &info, 1, &source_texture, frame, src.pitch);
            assert(gl.views.copied == 1);
         }
         for (row = 0; row < 2; row++)
            assert(!memcmp(output + row * 3 * bpp,
                     frame + (row + 2) * 128 + x * bpp, 3 * bpp));
         assert(!memcmp(saved, frame, sizeof(frame)));
      }
   }
   assert(copies == 84 && uploads == 28 && !gpu_copies);
   src.buffer = &src;
   src.frame = NULL;
   src.layout.Offset = 512;
   src.window.left = 4;
   src.window.top = 7;
   src.window.front = 0;
   src.window.back = 1;
   d3d12_sw_upload(&video, &texture, &src, 2, 3, 3, 2);
   assert(copies == 84 && uploads == 28 && gpu_copies == 1);
   assert(last_box.left == 6 && last_box.top == 10);
   assert(last_box.right == 9 && last_box.bottom == 12);
   assert(last_box.front == 0 && last_box.back == 1);
   puts("PASS: D3D11/D3D12/GLCore RGB565, XRGB8888, packed 10-bit and FP16 view crops; D3D12 GPU boxes");
   return 0;
}
'''
fixture = fixture.replace("\nBPP\n", "\n" + function("d3d12_frame_bpp(") + "\n")
fixture = fixture.replace("\nUPLOAD\n", "\n" + function("d3d12_sw_upload(") + "\n")
start = source11.index("            unsigned bpp =", source11.index("static void d3d11_views_chain_frame("))
end = source11.index(";", start) + 1
fixture = fixture.replace("D3D11_BPP", source11[start:end])
start = source11.index("               dxgi_copy(", end)
end = source11.index(";", start) + 1
fixture = fixture.replace("D3D11_COPY", source11[start:end])
source = source_gl
fixture = fixture.replace("GL3_UPLOAD", function("gl3_views_write("))
with tempfile.TemporaryDirectory(prefix="retroarch-d3d12-view-") as directory:
    path = Path(directory)
    src = path / "view.c"
    exe = path / "view.exe"
    src.write_text(fixture)
    command = shlex.split(os.environ.get("CC", "cc"))
    command += ["-std=gnu89", "-Wall", "-Wextra", "-Werror"]
    command += shlex.split(os.environ.get("CFLAGS", ""))
    subprocess.run(command + [str(src), "-o", str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
