/* gfx/common/d3d12_ngx.c and gfx/video_upscaler.c against a stand-in
 * NGX core library (fake_ngx.c) on a real Direct3D 12 device: Wine's,
 * over vkd3d and lavapipe, on a runner; the system's on Windows.
 *
 * What is pinned: the session comes up from the loaded library with
 * the project id and path list; the mode's render sizes reach the
 * core's request with the mip bias; a frame records a feature made for
 * the mode's full range and an evaluate on the frame's command list
 * whose resources are the core's, with the frame's size as the render
 * subrect, the jitter, scales and reset; the inputs come back in the
 * states the core left them and the output is a copy source holding
 * what was evaluated into it; a resize makes a new feature and output
 * and retires the old ones, which go after the frames in flight;
 * frames outside the range, without inputs or after a failed evaluate
 * are refused; nothing of it is reachable with the setting off. */
#define COBJMACROS
#include <windows.h>
#include <d3d12.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <gfx/nvidia/ngx_loader.h>

#include "../../../configuration.h"
#include "../../../gfx/video_upscaler.h"
#include "../../../gfx/common/d3d12_ngx.h"
#include "fake_ngx.h"

static int failures;

#define CHECK(cond) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
         failures++; \
      } \
   } while (0)

typedef struct texture
{
   ID3D12Resource *resource;
   DXGI_FORMAT format;
   D3D12_RESOURCE_STATES state;
   unsigned width, height;
} texture_t;

typedef HRESULT (WINAPI *pfn_create_device)(IUnknown*, D3D_FEATURE_LEVEL,
      REFIID, void**);

static ID3D12Device *device;
static ID3D12CommandQueue *queue;
static ID3D12CommandAllocator *allocator;
static ID3D12GraphicsCommandList *cmd;
static ID3D12Fence *fence;
static UINT64 fence_value;
static HANDLE fence_event;

static bool setup_d3d12(void)
{
   HMODULE m = LoadLibraryA("d3d12.dll");
   FARPROC p = m ? GetProcAddress(m, "D3D12CreateDevice") : NULL;
   pfn_create_device create;
   D3D12_COMMAND_QUEUE_DESC qdesc;
   if (!p)
      return false;
   memcpy(&create, &p, sizeof(create));
   if (FAILED(create(NULL, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device,
               (void**)&device)))
      return false;
   memset(&qdesc, 0, sizeof(qdesc));
   if (FAILED(ID3D12Device_CreateCommandQueue(device, &qdesc,
               &IID_ID3D12CommandQueue, (void**)&queue)))
      return false;
   if (FAILED(ID3D12Device_CreateCommandAllocator(device,
               D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator,
               (void**)&allocator)))
      return false;
   if (FAILED(ID3D12Device_CreateCommandList(device, 0,
               D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, NULL,
               &IID_ID3D12GraphicsCommandList, (void**)&cmd)))
      return false;
   ID3D12GraphicsCommandList_Close(cmd);
   if (FAILED(ID3D12Device_CreateFence(device, 0, D3D12_FENCE_FLAG_NONE,
               &IID_ID3D12Fence, (void**)&fence)))
      return false;
   fence_event = CreateEventA(NULL, FALSE, FALSE, NULL);
   return fence_event != NULL;
}

static bool make_buffer(ID3D12Resource **out, D3D12_HEAP_TYPE type,
      UINT64 size, D3D12_RESOURCE_STATES state)
{
   D3D12_HEAP_PROPERTIES heap;
   D3D12_RESOURCE_DESC desc;
   memset(&heap, 0, sizeof(heap));
   heap.Type               = type;
   heap.CreationNodeMask   = 1;
   heap.VisibleNodeMask    = 1;
   memset(&desc, 0, sizeof(desc));
   desc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
   desc.Width              = size;
   desc.Height             = 1;
   desc.DepthOrArraySize   = 1;
   desc.MipLevels          = 1;
   desc.Format             = DXGI_FORMAT_UNKNOWN;
   desc.SampleDesc.Count   = 1;
   desc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
   return SUCCEEDED(ID3D12Device_CreateCommittedResource(device, &heap,
            D3D12_HEAP_FLAG_NONE, &desc, state, NULL, &IID_ID3D12Resource,
            (void**)out));
}

static bool make_texture(texture_t *t, DXGI_FORMAT format, unsigned w,
      unsigned h, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state)
{
   D3D12_HEAP_PROPERTIES heap;
   D3D12_RESOURCE_DESC desc;
   memset(&heap, 0, sizeof(heap));
   heap.Type               = D3D12_HEAP_TYPE_DEFAULT;
   heap.CreationNodeMask   = 1;
   heap.VisibleNodeMask    = 1;
   memset(&desc, 0, sizeof(desc));
   desc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
   desc.Width              = w;
   desc.Height             = h;
   desc.DepthOrArraySize   = 1;
   desc.MipLevels          = 1;
   desc.Format             = format;
   desc.SampleDesc.Count   = 1;
   desc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
   desc.Flags              = flags;
   t->format = format;
   t->state  = state;
   t->width  = w;
   t->height = h;
   return SUCCEEDED(ID3D12Device_CreateCommittedResource(device, &heap,
            D3D12_HEAP_FLAG_NONE, &desc, state, NULL, &IID_ID3D12Resource,
            (void**)&t->resource));
}

static void free_texture(texture_t *t)
{
   if (t->resource)
      ID3D12Resource_Release(t->resource);
   t->resource = NULL;
}

static void begin_frame(void)
{
   ID3D12CommandAllocator_Reset(allocator);
   ID3D12GraphicsCommandList_Reset(cmd, allocator, NULL);
}

/* A test: the frame is over before the next is recorded */
static void end_frame(void)
{
   ID3D12CommandList *lists[1];
   lists[0] = (ID3D12CommandList*)cmd;
   ID3D12GraphicsCommandList_Close(cmd);
   ID3D12CommandQueue_ExecuteCommandLists(queue, 1, lists);
   ID3D12CommandQueue_Signal(queue, fence, ++fence_value);
   if (ID3D12Fence_GetCompletedValue(fence) < fence_value)
   {
      ID3D12Fence_SetEventOnCompletion(fence, fence_value, fence_event);
      WaitForSingleObject(fence_event, INFINITE);
   }
}

static void transition(ID3D12Resource *r, D3D12_RESOURCE_STATES before,
      D3D12_RESOURCE_STATES after)
{
   D3D12_RESOURCE_BARRIER b;
   memset(&b, 0, sizeof(b));
   b.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
   b.Transition.pResource   = r;
   b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
   b.Transition.StateBefore = before;
   b.Transition.StateAfter  = after;
   ID3D12GraphicsCommandList_ResourceBarrier(cmd, 1, &b);
}

/* Fills an RGBA8 texture with a pattern of its own coordinates, left in
 * @t->state as a core's set_texture would leave it */
static bool fill_texture(texture_t *t)
{
   D3D12_TEXTURE_COPY_LOCATION src, dst;
   D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
   UINT64 size = 0;
   D3D12_RESOURCE_DESC desc;
   ID3D12Resource *upload = NULL;
   unsigned char *map = NULL;
   unsigned x, y;
   memset(&desc, 0, sizeof(desc));
   desc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
   desc.Width              = t->width;
   desc.Height             = t->height;
   desc.DepthOrArraySize   = 1;
   desc.MipLevels          = 1;
   desc.Format             = t->format;
   desc.SampleDesc.Count   = 1;
   ID3D12Device_GetCopyableFootprints(device, &desc, 0, 1, 0, &layout,
         NULL, NULL, &size);
   if (!make_buffer(&upload, D3D12_HEAP_TYPE_UPLOAD, size,
            D3D12_RESOURCE_STATE_GENERIC_READ))
      return false;
   if (FAILED(ID3D12Resource_Map(upload, 0, NULL, (void**)&map)))
   {
      ID3D12Resource_Release(upload);
      return false;
   }
   for (y = 0; y < t->height; y++)
      for (x = 0; x < t->width; x++)
      {
         unsigned char *px = map + layout.Footprint.RowPitch * y + x * 4;
         px[0] = (unsigned char)x;
         px[1] = (unsigned char)y;
         px[2] = (unsigned char)(x >> 8);
         px[3] = (unsigned char)(y >> 8);
      }
   ID3D12Resource_Unmap(upload, 0, NULL);
   src.pResource        = upload;
   src.Type             = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
   src.PlacedFootprint  = layout;
   dst.pResource        = t->resource;
   dst.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
   dst.SubresourceIndex = 0;
   begin_frame();
   transition(t->resource, t->state, D3D12_RESOURCE_STATE_COPY_DEST);
   ID3D12GraphicsCommandList_CopyTextureRegion(cmd, &dst, 0, 0, 0, &src, NULL);
   transition(t->resource, D3D12_RESOURCE_STATE_COPY_DEST, t->state);
   end_frame();
   ID3D12Resource_Release(upload);
   return true;
}

/* Reads one pixel of an RGBA8 output, a copy source, as the driver
 * copies it */
static bool read_pixel(ID3D12Resource *r, unsigned w, unsigned h,
      unsigned x, unsigned y, unsigned char px[4])
{
   D3D12_TEXTURE_COPY_LOCATION src, dst;
   D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
   UINT64 size = 0;
   D3D12_RESOURCE_DESC desc;
   D3D12_RANGE range;
   ID3D12Resource *readback = NULL;
   unsigned char *map = NULL;
   memset(&desc, 0, sizeof(desc));
   desc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
   desc.Width              = w;
   desc.Height             = h;
   desc.DepthOrArraySize   = 1;
   desc.MipLevels          = 1;
   desc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
   desc.SampleDesc.Count   = 1;
   ID3D12Device_GetCopyableFootprints(device, &desc, 0, 1, 0, &layout,
         NULL, NULL, &size);
   if (!make_buffer(&readback, D3D12_HEAP_TYPE_READBACK, size,
            D3D12_RESOURCE_STATE_COPY_DEST))
      return false;
   src.pResource        = r;
   src.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
   src.SubresourceIndex = 0;
   dst.pResource        = readback;
   dst.Type             = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
   dst.PlacedFootprint  = layout;
   begin_frame();
   ID3D12GraphicsCommandList_CopyTextureRegion(cmd, &dst, 0, 0, 0, &src, NULL);
   end_frame();
   range.Begin = 0;
   range.End   = (SIZE_T)size;
   if (FAILED(ID3D12Resource_Map(readback, 0, &range, (void**)&map)))
   {
      ID3D12Resource_Release(readback);
      return false;
   }
   memcpy(px, map + layout.Footprint.RowPitch * y + x * 4, 4);
   range.End = 0;
   ID3D12Resource_Unmap(readback, 0, &range);
   ID3D12Resource_Release(readback);
   return true;
}

static fake_ngx_record_t *record(void)
{
   const ngx_lib_t *lib = ngx_lib_get();
   fake_ngx_record_t *(*get)(void) = NULL;
   FARPROC p;
   if (!lib)
      return NULL;
   p = GetProcAddress((HMODULE)lib->handle, "fake_ngx_record");
   memcpy(&get, &p, sizeof(get));
   return get ? get() : NULL;
}

static void set_input(struct retro_hw_upscaler_image *dst, const texture_t *src)
{
   dst->image  = (uint64_t)(uintptr_t)src->resource;
   dst->view   = 0;
   dst->format = (unsigned)src->format;
   dst->state  = (unsigned)src->state;
   dst->width  = src->width;
   dst->height = src->height;
}

static bool optimal_cb(void *data, unsigned mode, unsigned out_dims,
      unsigned *render_dims, unsigned *min_dims, unsigned *max_dims)
{
   return d3d12_ngx_optimal((d3d12_ngx_t*)data, mode, out_dims,
         render_dims, min_dims, max_dims);
}

static bool output_desc_ok(ID3D12Resource *r, unsigned w, unsigned h)
{
   D3D12_RESOURCE_DESC desc;
   /* GetDesc returns a struct; mingw's C vtable has it with a hidden
    * first argument, as d3d12.c calls it */
   void (STDMETHODCALLTYPE *get_desc)(ID3D12Resource*, D3D12_RESOURCE_DESC*);
   memcpy(&get_desc, &r->lpVtbl->GetDesc, sizeof(get_desc));
   get_desc(r, &desc);
   return desc.Width == w && desc.Height == h
      && desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM
      && (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
}

int main(void)
{
   settings_t *settings = (settings_t*)config_get_ptr();
   fake_ngx_record_t *rec;
   d3d12_ngx_t *ngx;
   texture_t color, mv, depth, color_big;
   d3d12_ngx_image_t in, out;
   struct retro_hw_upscaler_inputs inputs;
   struct retro_hw_upscaler_request req;
   video_upscaler_backend_t backend;
   const struct retro_hw_render_upscaler_interface *iface;
   unsigned render_dims, min_dims, max_dims;
   unsigned char px[4];
   unsigned i;

   if (!setup_d3d12())
   {
      printf("d3d12_upscaler_test: no Direct3D 12 device, skipped\n");
      return 0;
   }
   rec = record();
   CHECK(rec != NULL);
   if (!rec)
      return 1;

   /* Off: no interface for the core, whatever the backend */
   settings->uints.video_temporal_upscaler = 0;
   CHECK(video_upscaler_get_interface() == NULL);
   strcpy(settings->paths.directory_dlss, "Zdir");

   /* The session: init with the project, the path list carrying the
    * DLSS directory, the capability set read */
   settings->uints.video_temporal_upscaler = VIDEO_TEMPORAL_UPSCALER_DLSS;
   rec->unavailable = 1;
   CHECK(d3d12_ngx_init(device, "Zdir") == NULL);
   CHECK(rec->init_calls == 1 && rec->shutdown_calls == 1);
   rec->unavailable = 0;
   ngx = d3d12_ngx_init(device, "Zdir");
   CHECK(ngx != NULL);
   if (!ngx)
      return 1;
   CHECK(rec->init_calls == 2 && rec->init_device == (void*)device);
   CHECK(rec->init_path0 == 'Z');

   /* The mode's sizes, through the capability callback, cached */
   CHECK(d3d12_ngx_optimal(ngx, NGX_PERF_QUALITY_MAX_QUALITY,
            VIDEO_SCALE_PACK(1920, 1080), &render_dims, &min_dims, &max_dims));
   CHECK(render_dims == VIDEO_SCALE_PACK(1280, 720)
         && max_dims == VIDEO_SCALE_PACK(1920, 1080)
         && min_dims == VIDEO_SCALE_PACK(640, 360));
   CHECK(d3d12_ngx_optimal(ngx, NGX_PERF_QUALITY_MAX_QUALITY,
            VIDEO_SCALE_PACK(1920, 1080), &render_dims, &min_dims, &max_dims));
   CHECK(rec->optimal_calls == 1);
   CHECK(!d3d12_ngx_optimal(ngx, NGX_PERF_QUALITY_ULTRA_QUALITY,
            VIDEO_SCALE_PACK(1920, 1080), &render_dims, &min_dims, &max_dims));
   CHECK(!d3d12_ngx_optimal(ngx, NGX_PERF_QUALITY_MAX_QUALITY,
            VIDEO_SCALE_PACK(0, 1080), &render_dims, &min_dims, &max_dims));

   /* The core's request, through the frontend module */
   backend.data    = ngx;
   backend.optimal = optimal_cb;
   video_upscaler_attach(&backend);
   iface = video_upscaler_get_interface();
   CHECK(iface && iface->interface_version == RETRO_HW_RENDER_UPSCALER_INTERFACE_VERSION);
   memset(&req, 0, sizeof(req));
   CHECK(!iface->get_request(iface->handle, &req));   /* no output size yet */
   video_upscaler_set_output(VIDEO_SCALE_PACK(1920, 1080));
   settings->uints.video_temporal_upscaler_quality = VIDEO_TEMPORAL_UPSCALER_QUALITY_AUTO;
   CHECK(iface->get_request(iface->handle, &req));
   CHECK(req.output_width == 1920 && req.output_height == 1080);
   CHECK(req.render_width == 1280 && req.render_height == 720);
   CHECK(req.min_render_width == 640 && req.max_render_width == 1920);
   CHECK(fabs(req.mip_bias - (log(1280.0 / 1920.0) / log(2.0) - 1.0)) < 1e-5);
   settings->uints.video_temporal_upscaler_quality = VIDEO_TEMPORAL_UPSCALER_QUALITY_PERFORMANCE;
   CHECK(iface->get_request(iface->handle, &req) && req.render_width == 960);
   settings->uints.video_temporal_upscaler = 0;
   CHECK(!iface->get_request(iface->handle, &req));
   settings->uints.video_temporal_upscaler = VIDEO_TEMPORAL_UPSCALER_DLSS;
   settings->uints.video_temporal_upscaler_quality = VIDEO_TEMPORAL_UPSCALER_QUALITY_QUALITY;

   /* The inputs: set before the frame, taken in it, once */
   memset(&inputs, 0, sizeof(inputs));
   CHECK(video_upscaler_take_inputs() == NULL);
   iface->set_inputs(iface->handle, &inputs);
   CHECK(video_upscaler_take_inputs() == &inputs);
   CHECK(video_upscaler_take_inputs() == NULL);

   /* A frame: colour at the mode's render size in the state set_texture
    * hands over, the depth and motion vectors left as a core would */
   CHECK(make_texture(&color, DXGI_FORMAT_R8G8B8A8_UNORM, 1280, 720,
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            D3D12_RESOURCE_STATE_COPY_SOURCE));
   CHECK(make_texture(&mv, DXGI_FORMAT_R16G16_FLOAT, 1280, 720,
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            D3D12_RESOURCE_STATE_RENDER_TARGET));
   CHECK(make_texture(&depth, DXGI_FORMAT_R32_FLOAT, 1280, 720,
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
   CHECK(make_texture(&color_big, DXGI_FORMAT_R8G8B8A8_UNORM, 2048, 1152,
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            D3D12_RESOURCE_STATE_COPY_SOURCE));
   CHECK(fill_texture(&color));

   in.texture = color.resource;
   in.format  = color.format;
   in.state   = color.state;
   in.dims    = VIDEO_SCALE_PACK(color.width, color.height);
   set_input(&inputs.motion_vectors, &mv);
   set_input(&inputs.depth, &depth);
   inputs.flags      = RETRO_HW_UPSCALER_DEPTH_INVERTED | RETRO_HW_UPSCALER_MV_JITTERED;
   inputs.jitter_x   = 0.25f;
   inputs.jitter_y   = -0.125f;
   inputs.mv_scale_x = 0.0f;       /* reads as 1 */
   inputs.mv_scale_y = -1.0f;
   inputs.frame_time_ms = 16.6f;

   begin_frame();
   memset(&out, 0, sizeof(out));
   CHECK(d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   d3d12_ngx_frame_done(ngx);
   CHECK(rec->creates == 1 && rec->evals == 1);
   /* The feature: the mode's full range in, the output size out, the
    * flags from the inputs with low-res MVs and automatic exposure, on
    * the frame's list */
   CHECK(rec->create[0].cmd == (void*)cmd);
   CHECK(rec->create[0].width == 1920 && rec->create[0].height == 1080);
   CHECK(rec->create[0].out_width == 1920 && rec->create[0].out_height == 1080);
   CHECK(rec->create[0].quality == NGX_PERF_QUALITY_MAX_QUALITY);
   CHECK(rec->create[0].flags == (NGX_DLSS_FLAG_MV_LOW_RES | NGX_DLSS_FLAG_MV_JITTERED
            | NGX_DLSS_FLAG_DEPTH_INVERTED | NGX_DLSS_FLAG_AUTO_EXPOSURE));
   CHECK(rec->create[0].node_mask == 0x101);
   /* The evaluate: the resources as given, the output ours */
   CHECK(rec->eval[0].cmd == (void*)cmd);
   CHECK(rec->eval[0].color == (void*)color.resource);
   CHECK(rec->eval[0].depth == (void*)depth.resource);
   CHECK(rec->eval[0].motion_vectors == (void*)mv.resource);
   CHECK(rec->eval[0].output && rec->eval[0].output == (void*)out.texture);
   CHECK(!rec->eval[0].exposure && !rec->eval[0].transparency);
   CHECK(rec->eval[0].jitter_x == 0.25f && rec->eval[0].jitter_y == -0.125f);
   CHECK(rec->eval[0].mv_scale_x == 1.0f && rec->eval[0].mv_scale_y == -1.0f);
   CHECK(rec->eval[0].pre_exposure == 1.0f && rec->eval[0].exposure_scale == 1.0f);
   CHECK(rec->eval[0].frame_time == 16.6f);
   CHECK(rec->eval[0].reset == 1);   /* a new feature starts over */
   CHECK(rec->eval[0].subrect_w == 1280 && rec->eval[0].subrect_h == 720);
   CHECK(out.dims == VIDEO_SCALE_PACK(1920, 1080) && out.format == DXGI_FORMAT_R8G8B8A8_UNORM
         && out.state == D3D12_RESOURCE_STATE_COPY_SOURCE);
   CHECK(output_desc_ok(out.texture, 1920, 1080));
   /* What the evaluate wrote is there to copy out, as the driver does */
   CHECK(read_pixel(out.texture, 1920, 1080, 300, 200, px));
   CHECK(px[0] == (300 & 0xff) && px[1] == 200 && px[2] == 1 && px[3] == 0);

   /* The next frame: the same feature, no reset, the core's reset
    * honoured when asked */
   begin_frame();
   CHECK(d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   d3d12_ngx_frame_done(ngx);
   CHECK(rec->creates == 1 && rec->evals == 2 && rec->eval[1].reset == 0);
   inputs.flags |= RETRO_HW_UPSCALER_RESET;
   begin_frame();
   CHECK(d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   d3d12_ngx_frame_done(ngx);
   CHECK(rec->evals == 3 && rec->eval[2].reset == 1 && rec->creates == 1);
   inputs.flags &= ~RETRO_HW_UPSCALER_RESET;

   /* A smaller frame within the range: the same feature, the subrect
    * says the size; one outside: refused */
   in.dims = VIDEO_SCALE_PACK(960, 540);
   begin_frame();
   CHECK(d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   d3d12_ngx_frame_done(ngx);
   CHECK(rec->creates == 1 && rec->evals == 4
         && rec->eval[3].subrect_w == 960 && rec->eval[3].subrect_h == 540);
   in.texture = color_big.resource;
   in.dims    = VIDEO_SCALE_PACK(color_big.width, color_big.height);
   begin_frame();
   CHECK(!d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   d3d12_ngx_frame_done(ngx);
   CHECK(rec->evals == 4);
   in.texture = color.resource;
   in.dims    = VIDEO_SCALE_PACK(color.width, color.height);

   /* No inputs, no motion vectors: refused */
   CHECK(!d3d12_ngx_upscale(ngx, cmd, &in, NULL, NGX_PERF_QUALITY_MAX_QUALITY,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   inputs.motion_vectors.image = 0;
   CHECK(!d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   set_input(&inputs.motion_vectors, &mv);

   /* A resize: a new feature and output, the old retired and released
    * once the frames that could read them are through */
   begin_frame();
   CHECK(d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY,
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   d3d12_ngx_frame_done(ngx);
   CHECK(rec->creates == 2 && rec->create[1].width == 2560 && rec->create[1].out_width == 2560);
   CHECK(out.dims == VIDEO_SCALE_PACK(2560, 1440));
   CHECK(output_desc_ok(out.texture, 2560, 1440));
   CHECK(rec->releases == 0);
   CHECK(rec->eval[4].reset == 1);
   for (i = 0; i < 30 && rec->releases == 0; i++)
      d3d12_ngx_frame_done(ngx);
   CHECK(rec->releases == 1 && rec->last_released == 1);
   CHECK(i >= 15);

   /* A mode change makes a new feature too */
   begin_frame();
   CHECK(d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_BALANCED,
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   d3d12_ngx_frame_done(ngx);
   CHECK(rec->creates == 3 && rec->create[2].quality == NGX_PERF_QUALITY_BALANCED);

   /* An evaluate that fails: the frame shown as it is, the feature left
    * alone until something new is asked of it */
   rec->eval_fails = 1;
   begin_frame();
   CHECK(!d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_BALANCED,
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   d3d12_ngx_frame_done(ngx);
   rec->eval_fails = 0;
   begin_frame();
   CHECK(!d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_BALANCED,
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   d3d12_ngx_frame_done(ngx);
   begin_frame();
   CHECK(d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY,
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   d3d12_ngx_frame_done(ngx);
   CHECK(rec->creates == 4);

   /* A feature that will not create: refused, nothing leaks */
   rec->create_fails = 1;
   begin_frame();
   CHECK(!d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_PERF,
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   d3d12_ngx_frame_done(ngx);
   rec->create_fails = 0;

   /* Teardown releases everything, retired or current */
   {
      unsigned releases = rec->releases;
      d3d12_ngx_free(ngx);
      CHECK(rec->releases == releases + 3);  /* the three retired */
      CHECK(rec->shutdown_calls == 2 && rec->params_destroyed == 1);
   }
   video_upscaler_detach();
   CHECK(video_upscaler_get_interface() == NULL);

   free_texture(&color);
   free_texture(&mv);
   free_texture(&depth);
   free_texture(&color_big);
   ID3D12Fence_Release(fence);
   ID3D12GraphicsCommandList_Release(cmd);
   ID3D12CommandAllocator_Release(allocator);
   ID3D12CommandQueue_Release(queue);
   ID3D12Device_Release(device);

   if (failures)
   {
      printf("d3d12_upscaler_test: %d failure(s)\n", failures);
      return 1;
   }
   printf("d3d12_upscaler_test: ok\n");
   return 0;
}
