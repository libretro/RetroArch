#!/usr/bin/env python3
"""Exercise the shipping viewer PQ conversion with a recording command list."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/drivers/d3d12.c").read_text(encoding="utf-8")
start = source.index("static d3d12_texture_t *d3d12_media_hdr_output(")
function = source[start:source.index("\n}\n", start) + 3]
fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
typedef int LONG;
typedef struct { uintptr_t ptr; } handle_t;
typedef struct { float Width, Height, MinDepth, MaxDepth, TopLeftX, TopLeftY; } D3D12_VIEWPORT;
typedef struct { LONG left, top, right, bottom; } D3D12_RECT;
typedef struct {
   struct { unsigned Width, Height, Format; } desc;
   void *handle, *srv_heap;
   handle_t rt_view, gpu_descriptor[1];
} d3d12_texture_t;
typedef struct {
   unsigned mvp, hdr_mode;
   float inverse_tonemap, hdr10, scanlines;
   struct { float width, height; } source_size, output_size;
} dxgi_hdr_uniform_t;
typedef struct {
   struct { uintptr_t ptr; } cpu;
   unsigned stride;
} heap_t;
typedef struct { unsigned passes; } preset_t;
typedef struct { heap_t rtv_heap; preset_t *shader_preset; } d3d12_filter_chain_t;
struct command;
typedef struct command *D3D12GraphicsCommandList;
typedef struct {
   void (*SetGraphicsRootSignature)(D3D12GraphicsCommandList, unsigned);
   void (*SetPipelineState)(D3D12GraphicsCommandList, unsigned);
   void (*IASetPrimitiveTopology)(D3D12GraphicsCommandList, unsigned);
   void (*IASetVertexBuffers)(D3D12GraphicsCommandList, unsigned, unsigned, unsigned*);
   void (*SetGraphicsRootConstantBufferView)(D3D12GraphicsCommandList, unsigned, unsigned);
   void (*SetGraphicsRootDescriptorTable)(D3D12GraphicsCommandList, unsigned, handle_t);
   void (*OMSetRenderTargets)(D3D12GraphicsCommandList, unsigned, handle_t*, int, void*);
   void (*RSSetViewports)(D3D12GraphicsCommandList, unsigned, D3D12_VIEWPORT*);
   void (*RSSetScissorRects)(D3D12GraphicsCommandList, unsigned, D3D12_RECT*);
   void (*DrawInstanced)(D3D12GraphicsCommandList, unsigned, unsigned, unsigned, unsigned);
} methods_t;
struct command { methods_t *lpVtbl; };
typedef struct {
   unsigned flags, media_hdr_pipe, media_hdr_ubo, media_hdr_ubo_view, mvp_no_rot, device;
   struct { D3D12GraphicsCommandList cmd; unsigned slot; } queue;
   struct { heap_t srv_heap; unsigned rootSignature; } desc;
   struct { unsigned vbo_view; } frame;
   struct { dxgi_hdr_uniform_t ubo_values; } hdr;
   handle_t samplers[2][2];
   d3d12_texture_t media_hdr_texture;
} d3d12_video_t;
#define D3D12_ST_FLAG_HDR_ENABLE 1
#define DXGI_FORMAT_R10G10B10A2_UNORM 24
#define DXGI_FORMAT_R16G16B16A16_FLOAT 10
#define D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE 1
#define D3D12_RESOURCE_STATE_RENDER_TARGET 2
#define D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP 5
#define ROOT_ID_UBO 0
#define ROOT_ID_TEXTURE_T 1
#define ROOT_ID_SAMPLER_T 2
#define RARCH_FILTER_LINEAR 1
#define RARCH_WRAP_EDGE 1
#define FALSE 0
static unsigned allocations, transitions, draws;
static dxgi_hdr_uniform_t written;
static void transition(D3D12GraphicsCommandList cmd, void *resource, int from, int to)
{ (void)cmd; assert(resource && from != to); transitions++; }
#define D3D12_RESOURCE_TRANSITION transition
static void d3d12_release_texture(d3d12_video_t *v, d3d12_texture_t *t)
{ (void)v; t->handle = NULL; }
static void d3d12_init_texture(unsigned device, d3d12_texture_t *t)
{ (void)device; assert(t->desc.Format == 10); t->handle = t; allocations++; }
static void d3d12_cb_write_slot(unsigned cb, unsigned *view, unsigned slot,
      const void *data, size_t size)
{ assert(cb == 7 && *view == 8 && slot == 1 && size == sizeof(written)); memcpy(&written, data, size); }
static unsigned d3d12_cb_slot(unsigned *view, unsigned slot)
{ return *view + slot; }
static void set_uint(D3D12GraphicsCommandList c, unsigned value) { (void)c; (void)value; }
static void vertices(D3D12GraphicsCommandList c, unsigned start, unsigned n, unsigned *v)
{ (void)c; (void)v; assert(!start && n == 1); }
static void constant(D3D12GraphicsCommandList c, unsigned root, unsigned value)
{ (void)c; assert(root == ROOT_ID_UBO && value == 9); }
static void descriptor(D3D12GraphicsCommandList c, unsigned root, handle_t value)
{ (void)c; assert((root == ROOT_ID_TEXTURE_T && value.ptr == 55) || root == ROOT_ID_SAMPLER_T); }
static void target(D3D12GraphicsCommandList c, unsigned n, handle_t *h, int single, void *depth)
{ (void)c; (void)single; (void)depth; assert(n == 1 && h->ptr == 132); }
static void viewport(D3D12GraphicsCommandList c, unsigned n, D3D12_VIEWPORT *v)
{ (void)c; assert(n == 1 && v->Width == 640 && v->Height == 480 && v->MaxDepth == 1); }
static void scissor(D3D12GraphicsCommandList c, unsigned n, D3D12_RECT *r)
{ (void)c; assert(n == 1 && !r->left && !r->top && r->right == 640 && r->bottom == 480); }
static void draw(D3D12GraphicsCommandList c, unsigned n, unsigned instances, unsigned first, unsigned base)
{ (void)c; assert(n == 4 && instances == 1 && !first && !base); draws++; }
FUNCTION
int main(void)
{
   static methods_t methods = {set_uint, set_uint, set_uint, vertices, constant,
      descriptor, target, viewport, scissor, draw};
   struct command command = {&methods};
   d3d12_video_t video;
   d3d12_filter_chain_t chain;
   d3d12_texture_t input;
   preset_t preset = {2};
   memset(&video, 0, sizeof(video)); memset(&chain, 0, sizeof(chain));
   memset(&input, 0, sizeof(input));
   input.desc.Width = 640; input.desc.Height = 480;
   input.desc.Format = 24; input.gpu_descriptor[0].ptr = 55;
   chain.shader_preset = &preset; chain.rtv_heap.cpu.ptr = 100; chain.rtv_heap.stride = 8;
   video.queue.cmd = &command; video.queue.slot = 1;
   video.media_hdr_pipe = 1; video.media_hdr_ubo = 7; video.media_hdr_ubo_view = 8;
   video.mvp_no_rot = 123;
   video.hdr.ubo_values.hdr_mode = 99;
   video.hdr.ubo_values.inverse_tonemap = 1;
   video.hdr.ubo_values.scanlines = 1;
   assert(d3d12_media_hdr_output(&video, &chain, &input) == &input);
   assert(!allocations && !draws);
   video.flags = 1;
   assert(d3d12_media_hdr_output(&video, &chain, &input) == &video.media_hdr_texture);
   assert(allocations == 1 && draws == 1 && transitions == 1);
   assert(written.hdr_mode == 3 && !written.inverse_tonemap && !written.hdr10 && !written.scanlines);
   assert(written.mvp == 123 && written.source_size.width == 640);
   assert(video.hdr.ubo_values.hdr_mode == 99 && video.hdr.ubo_values.scanlines == 1);
   d3d12_media_hdr_output(&video, &chain, &input);
   assert(allocations == 1 && draws == 2 && transitions == 3);
   input.desc.Format = 10;
   assert(d3d12_media_hdr_output(&video, &chain, &input) == &input && draws == 2);
   input.desc.Format = 28;
   assert(d3d12_media_hdr_output(&video, &chain, &input) == &input && draws == 2);
   puts("PASS: viewer PQ decode, FP16/SDR passthrough, separate constants, target format and resource reuse");
   return 0;
}
'''.replace("FUNCTION", function)
with tempfile.TemporaryDirectory(prefix="viewer-hdr-") as directory:
    unit = Path(directory) / "test.c"
    exe = Path(directory) / ("test.exe" if os.name == "nt" or "mingw" in os.environ.get("CC", "") else "test")
    unit.write_bytes(fixture.encode())
    subprocess.run(shlex.split(os.environ.get("CC", "cc")) + [
        *shlex.split(os.environ.get("CFLAGS", "")), "-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror",
        str(unit), "-o", str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
