/* gfx/common/d3d12_ngx.c and gfx/video_upscaler.c against a stand-in
 * NGX core library (fake_ngx.c) on a real Direct3D 12 device: Wine's,
 * over vkd3d and lavapipe, on a runner; the system's on Windows. Then
 * gfx/common/d3d12_ffx.c and d3d12_xess.c behind the dispatch of
 * d3d12_upscaler.c, against the stand-in FidelityFX API and XeSS
 * libraries of libretro-common's samples planted in directories of
 * the test's.
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
#include <gfx/amd/ffx_loader.h>
#include <gfx/intel/xess_loader.h>

#include "../../../configuration.h"
#include "../../../gfx/video_upscaler.h"
#include "../../../gfx/common/d3d12_ngx.h"
#include "../../../gfx/common/d3d12_upscaler.h"
#include <file/file_path.h>
#include "fake_ngx.h"
#include "../../../libretro-common/samples/gfx/amd/fake_ffx.h"
#include "../../../libretro-common/samples/gfx/intel/fake_xess.h"

extern const char *stub_video_ident;
extern float stub_core_hz;

/* The DLSS runtime in @dir: the stand-in feature library built beside
 * the test, under the runtime's name, so its version can be read */
static void plant_dlss_runtime(const char *dir)
{
   char path[256];
   char buf[4096];
   FILE *in, *out;
   size_t n;
   path_mkdir(dir);
#if defined(_WIN32)
   in = fopen("nvngx_dlss.dll", "rb");
   snprintf(path, sizeof(path), "%s/nvngx_dlss.dll", dir);
#else
   in = fopen("libnvidia-ngx-dlss.so.310.2.1", "rb");
   snprintf(path, sizeof(path), "%s/libnvidia-ngx-dlss.so.310.2.1", dir);
#endif
   if (!in)
      return;
   if ((out = fopen(path, "wb")))
   {
      while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
         fwrite(buf, 1, n, out);
      fclose(out);
   }
   fclose(in);
}

/* ...and none there (a run before may have left one) */
static void clear_dlss_runtime(const char *dir)
{
   char path[256];
#if defined(_WIN32)
   snprintf(path, sizeof(path), "%s/nvngx_dlss.dll", dir);
   remove(path);
#else
   snprintf(path, sizeof(path), "%s/libnvidia-ngx-dlss.so.1", dir);
   remove(path);
   snprintf(path, sizeof(path), "%s/libnvidia-ngx-dlss.so.310.2.1", dir);
   remove(path);
#endif
}

/* DLSS Neural Rendering's library: its file version, a copy of the DLSS
 * stand-in's */
static void plant_dlssnr_runtime(const char *dir)
{
   char path[256];
   char buf[4096];
   FILE *in, *out;
   size_t n;
   path_mkdir(dir);
   in = fopen("nvngx_dlss.dll", "rb");
   snprintf(path, sizeof(path), "%s/nvngx_dlssnr.dll", dir);
   if (!in)
      return;
   if ((out = fopen(path, "wb")))
   {
      while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
         fwrite(buf, 1, n, out);
      fclose(out);
   }
   fclose(in);
}

static void clear_dlssnr_runtime(const char *dir)
{
   char path[256];
   snprintf(path, sizeof(path), "%s/nvngx_dlssnr.dll", dir);
   remove(path);
}

static int failures;
static int checks;

#define CHECK(cond) \
   do { \
      checks++; \
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

/* The frontend's mode word, as the driver hands it to the session */
static bool optimal_cb(void *data, unsigned mode, unsigned out_dims,
      unsigned *render_dims, unsigned *min_dims, unsigned *max_dims)
{
   return d3d12_ngx_optimal((d3d12_ngx_t*)data, video_upscaler_mode_ngx(mode),
         out_dims, render_dims, min_dims, max_dims);
}

static bool dispatch_cb(void *data, unsigned mode, unsigned out_dims,
      unsigned *render_dims, unsigned *min_dims, unsigned *max_dims)
{
   return d3d12_upscaler_optimal((d3d12_upscaler_t*)data, mode, out_dims,
         render_dims, min_dims, max_dims);
}

static void *symbol_of(void *handle, const char *name)
{
   void *fn = NULL;
   FARPROC p = GetProcAddress((HMODULE)handle, name);
   memcpy(&fn, &p, sizeof(fn));
   return fn;
}

#define QUALITY(q) VIDEO_UPSCALER_MODE_PACK(VIDEO_TEMPORAL_UPSCALER_QUALITY_##q, 0)

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

/* FSR behind the dispatch: the library from the FSR directory and the
 * version it offers the device, the sizes by its query, a frame's
 * context and dispatch descriptor as the SDK reads them, the inputs
 * back in their states and the output a copy source, a resize retiring
 * the context, a failed dispatch */
static void test_ffx(settings_t *settings, texture_t *color, texture_t *mv,
      texture_t *depth)
{
   d3d12_upscaler_t *up;
   fake_ffx_record_t *(*get)(void);
   fake_ffx_record_t *rec;
   const ffx_lib_t *lib;
   d3d12_upscaler_image_t in, out;
   struct retro_hw_upscaler_inputs inputs;
   struct retro_hw_upscaler_request req;
   video_upscaler_backend_t backend;
   const struct retro_hw_render_upscaler_interface *iface;
   unsigned i, render_dims, min_dims, max_dims;
   char status[256];

   stub_video_ident = "d3d12";
   settings->uints.video_temporal_upscaler = VIDEO_TEMPORAL_UPSCALER_FSR;
   strcpy(settings->paths.directory_fsr, "Zdir_none");
   CHECK(video_upscaler_probe(VIDEO_TEMPORAL_UPSCALER_FSR) == 0);
   video_upscaler_status_string(status, sizeof(status));
   CHECK(strstr(status, "FSR (FSR library: not in FSR Directory)") == status);
   CHECK(d3d12_upscaler_init(VIDEO_TEMPORAL_UPSCALER_FSR, device, "Zdir_none") == NULL);
   strcpy(settings->paths.directory_fsr, "Zdir_fsr");
   CHECK(video_upscaler_probe(VIDEO_TEMPORAL_UPSCALER_FSR) == VIDEO_UPSCALER_READY);
   video_upscaler_status_string(status, sizeof(status));
   CHECK(strstr(status, "FSR (FSR library: found)") == status);
   video_upscaler_log_probe(VIDEO_TEMPORAL_UPSCALER_FSR);
   lib = ffx_lib_get(FFX_API_D3D12, "Zdir_fsr");
   CHECK(lib != NULL);
   if (!lib)
      return;
   get = (fake_ffx_record_t *(*)(void))symbol_of(lib->handle, "fake_ffx_record");
   CHECK(get != NULL);
   if (!get)
      return;
   rec = get();

   /* The session: the versions the library offers the device, the
    * newest taken (none without the device, as SDK 2's loader has it) */
   rec->versions_need_device = 1;
   up = d3d12_upscaler_init(VIDEO_TEMPORAL_UPSCALER_FSR, device, "Zdir_fsr");
   CHECK(up != NULL);
   if (!up)
      return;
   CHECK(rec->creates == 0 && rec->queries >= 2);

   CHECK(d3d12_upscaler_optimal(up, QUALITY(QUALITY), VIDEO_SCALE_PACK(1920, 1080),
            &render_dims, &min_dims, &max_dims));
   CHECK(render_dims == VIDEO_SCALE_PACK(1280, 720)
         && min_dims == VIDEO_SCALE_PACK(640, 360)
         && max_dims == VIDEO_SCALE_PACK(1920, 1080));
   CHECK(d3d12_upscaler_optimal(up, QUALITY(BALANCED), VIDEO_SCALE_PACK(1920, 1080),
            &render_dims, &min_dims, &max_dims) && render_dims == VIDEO_SCALE_PACK(1129, 635));
   CHECK(d3d12_upscaler_optimal(up, QUALITY(NATIVE), VIDEO_SCALE_PACK(1920, 1080),
            &render_dims, &min_dims, &max_dims) && render_dims == VIDEO_SCALE_PACK(1920, 1080));
   CHECK(!d3d12_upscaler_optimal(up, QUALITY(NATIVE), VIDEO_SCALE_PACK(1920, 0),
            &render_dims, &min_dims, &max_dims));

   backend.data    = up;
   backend.optimal = dispatch_cb;
   video_upscaler_attach(&backend);
   iface = video_upscaler_get_interface();
   CHECK(iface != NULL);
   video_upscaler_set_output(VIDEO_SCALE_PACK(1920, 1080));
   settings->uints.video_temporal_upscaler_quality = VIDEO_TEMPORAL_UPSCALER_QUALITY_AUTO;
   CHECK(iface->get_request(iface->handle, &req));
   CHECK(req.render_width == 1280 && req.render_height == 720
         && req.min_render_width == 640 && req.max_render_width == 1920);
   settings->uints.video_temporal_upscaler_quality = VIDEO_TEMPORAL_UPSCALER_QUALITY_ULTRA_PERFORMANCE;
   CHECK(iface->get_request(iface->handle, &req) && req.render_width == 640);
   settings->uints.video_temporal_upscaler_quality = VIDEO_TEMPORAL_UPSCALER_QUALITY_QUALITY;

   /* A frame */
   memset(&inputs, 0, sizeof(inputs));
   set_input(&inputs.motion_vectors, mv);
   set_input(&inputs.depth, depth);
   inputs.flags      = RETRO_HW_UPSCALER_DEPTH_INVERTED | RETRO_HW_UPSCALER_MV_OUTPUT_RES;
   inputs.jitter_x   = 0.25f;
   inputs.jitter_y   = -0.125f;
   inputs.mv_scale_x = 2.0f;
   inputs.mv_scale_y = 0.0f;
   inputs.frame_time_ms = 0.0f;   /* unknown: the core's 50 Hz frame */
   stub_core_hz         = 50.0f;
   in.texture = color->resource;
   in.format  = color->format;
   in.state   = color->state;
   in.dims    = VIDEO_SCALE_PACK(color->width, color->height);
   begin_frame();
   memset(&out, 0, sizeof(out));
   CHECK(d3d12_upscaler_upscale(up, cmd, &in, &inputs, QUALITY(QUALITY),
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   d3d12_upscaler_frame_done(up);
   CHECK(rec->creates == 1 && rec->dispatches == 1);
   CHECK(rec->create[0].flags == (FFX_UPSCALE_ENABLE_DYNAMIC_RESOLUTION
            | FFX_UPSCALE_ENABLE_DISPLAY_RESOLUTION_MVS | FFX_UPSCALE_ENABLE_DEPTH_INVERTED
            | FFX_UPSCALE_ENABLE_NON_LINEAR_COLORSPACE | FFX_UPSCALE_ENABLE_AUTO_EXPOSURE));
   CHECK(rec->create[0].max_render_w == 1920 && rec->create[0].max_upscale_h == 1080);
   CHECK(rec->create[0].backend_type == FFX_TYPE_BACKEND_DX12
         && rec->create[0].device == (void*)device
         && rec->create[0].physical_device == NULL);
   CHECK(rec->create[0].version_id == 0x4002 && rec->create[0].has_message);
   CHECK(rec->dispatch[0].command_list == (void*)cmd);
   CHECK(rec->dispatch[0].color.present && rec->dispatch[0].color.resource == (void*)color->resource
         && rec->dispatch[0].color.format == FFX_FORMAT_R8G8B8A8_UNORM
         && rec->dispatch[0].color.width == 1280 && rec->dispatch[0].color.height == 720
         && rec->dispatch[0].color.usage == FFX_USAGE_READ_ONLY
         && rec->dispatch[0].color.state == FFX_STATE_COMPUTE_READ);
   CHECK(rec->dispatch[0].depth.resource == (void*)depth->resource
         && rec->dispatch[0].depth.format == FFX_FORMAT_R32_FLOAT
         && rec->dispatch[0].depth.state == FFX_STATE_COMPUTE_READ);
   CHECK(rec->dispatch[0].motion_vectors.resource == (void*)mv->resource
         && rec->dispatch[0].motion_vectors.format == FFX_FORMAT_R16G16_FLOAT
         && rec->dispatch[0].motion_vectors.state == FFX_STATE_COMPUTE_READ);
   CHECK(rec->dispatch[0].output.present && rec->dispatch[0].output.resource == (void*)out.texture
         && rec->dispatch[0].output.usage == FFX_USAGE_UAV
         && rec->dispatch[0].output.state == FFX_STATE_UNORDERED_ACCESS
         && rec->dispatch[0].output.width == 1920 && rec->dispatch[0].output.height == 1080);
   CHECK(!rec->dispatch[0].exposure.present && !rec->dispatch[0].transparency.present);
   CHECK(rec->dispatch[0].jitter_x == 0.25f && rec->dispatch[0].jitter_y == -0.125f);
   CHECK(rec->dispatch[0].mv_scale_x == 2.0f && rec->dispatch[0].mv_scale_y == 1.0f);
   CHECK(rec->dispatch[0].render_w == 1280 && rec->dispatch[0].upscale_w == 1920);
   CHECK(rec->dispatch[0].reset == 1 && !rec->dispatch[0].sharpening);
   CHECK(rec->dispatch[0].frame_time == 20.0f);
   stub_core_hz = 0.0f;
   CHECK(rec->dispatch[0].flags == FFX_UPSCALE_FLAG_NON_LINEAR_COLOR_SRGB);
   CHECK(out.dims == VIDEO_SCALE_PACK(1920, 1080) && out.format == DXGI_FORMAT_R8G8B8A8_UNORM
         && out.state == D3D12_RESOURCE_STATE_COPY_SOURCE);
   CHECK(output_desc_ok(out.texture, 1920, 1080));

   /* The next frame: the same context, no reset; a resize: a new one,
    * the old retired after the frames in flight */
   begin_frame();
   CHECK(d3d12_upscaler_upscale(up, cmd, &in, &inputs, QUALITY(QUALITY),
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   d3d12_upscaler_frame_done(up);
   CHECK(rec->creates == 1 && rec->dispatches == 2 && rec->dispatch[1].reset == 0);
   begin_frame();
   CHECK(d3d12_upscaler_upscale(up, cmd, &in, &inputs, QUALITY(QUALITY),
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   d3d12_upscaler_frame_done(up);
   CHECK(rec->creates == 2 && rec->create[1].max_upscale_w == 2560
         && out.dims == VIDEO_SCALE_PACK(2560, 1440) && rec->dispatch[2].reset == 1);
   CHECK(rec->destroys == 0);
   for (i = 0; i < 30 && rec->destroys < 1; i++)
      d3d12_upscaler_frame_done(up);
   CHECK(rec->destroys == 1 && i >= 15);

   /* A dispatch that fails: the frame shown as it is until the output
    * or flags change */
   rec->dispatch_fails = 1;
   begin_frame();
   CHECK(!d3d12_upscaler_upscale(up, cmd, &in, &inputs, QUALITY(QUALITY),
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   d3d12_upscaler_frame_done(up);
   rec->dispatch_fails = 0;
   begin_frame();
   CHECK(!d3d12_upscaler_upscale(up, cmd, &in, &inputs, QUALITY(QUALITY),
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   d3d12_upscaler_frame_done(up);
   CHECK(rec->dispatches == 4);
   inputs.flags |= RETRO_HW_UPSCALER_HDR;
   begin_frame();
   CHECK(d3d12_upscaler_upscale(up, cmd, &in, &inputs,
            VIDEO_UPSCALER_MODE_PACK_SHARP(VIDEO_TEMPORAL_UPSCALER_QUALITY_QUALITY, 0, 30),
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   d3d12_upscaler_frame_done(up);
   CHECK(rec->creates == 3 && rec->dispatches == 5 && rec->dispatch[4].flags == 0
         && (rec->create[2].flags & FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE));
   CHECK(rec->dispatch[4].sharpening && rec->dispatch[4].sharpness == 0.3f);

   /* No inputs: refused */
   CHECK(!d3d12_upscaler_upscale(up, cmd, &in, NULL, QUALITY(QUALITY),
            VIDEO_SCALE_PACK(2560, 1440), &out));

   d3d12_upscaler_free(up);
   CHECK(rec->destroys == 3);
   video_upscaler_detach();
   rec->versions_need_device = 0;
}

/* XeSS behind the dispatch: the library from the XeSS directory, the
 * sizes by its query, a context whose pipelines build in the
 * background before init, the execute parameters as the SDK reads
 * them, a resize retiring the context, a failed execute */
static void test_xess(settings_t *settings, texture_t *color, texture_t *mv,
      texture_t *depth)
{
   d3d12_upscaler_t *up;
   fake_xess_record_t *(*get)(void);
   fake_xess_record_t *rec;
   const xess_lib_t *lib;
   d3d12_upscaler_image_t in, out;
   struct retro_hw_upscaler_inputs inputs;
   struct retro_hw_upscaler_request req;
   video_upscaler_backend_t backend;
   const struct retro_hw_render_upscaler_interface *iface;
   unsigned i, render_dims, min_dims, max_dims;
   char status[256];

   settings->uints.video_temporal_upscaler = VIDEO_TEMPORAL_UPSCALER_XESS;
   strcpy(settings->paths.directory_xess, "Zdir_none");
   CHECK(video_upscaler_probe(VIDEO_TEMPORAL_UPSCALER_XESS) == 0);
   CHECK(d3d12_upscaler_init(VIDEO_TEMPORAL_UPSCALER_XESS, device, "Zdir_none") == NULL);
   strcpy(settings->paths.directory_xess, "Zdir_xess");
   CHECK(video_upscaler_probe(VIDEO_TEMPORAL_UPSCALER_XESS) == VIDEO_UPSCALER_READY);
   video_upscaler_status_string(status, sizeof(status));
   CHECK(strstr(status, "XeSS (XeSS library: found)") == status);
   video_upscaler_log_probe(VIDEO_TEMPORAL_UPSCALER_XESS);
   lib = xess_lib_get("Zdir_xess");
   CHECK(lib != NULL);
   if (!lib)
      return;
   get = (fake_xess_record_t *(*)(void))symbol_of(lib->handle, "fake_xess_record");
   CHECK(get != NULL);
   if (!get)
      return;
   rec = get();

   up = d3d12_upscaler_init(VIDEO_TEMPORAL_UPSCALER_XESS, device, "Zdir_xess");
   CHECK(up != NULL);
   if (!up)
      return;
   CHECK(rec->creates == 1 && rec->create_device == (void*)device && rec->inits == 0);

   CHECK(d3d12_upscaler_optimal(up, QUALITY(QUALITY), VIDEO_SCALE_PACK(1920, 1080),
            &render_dims, &min_dims, &max_dims));
   CHECK(render_dims == VIDEO_SCALE_PACK(1129, 635)
         && min_dims == VIDEO_SCALE_PACK(640, 360)
         && max_dims == VIDEO_SCALE_PACK(1920, 1080));
   CHECK(d3d12_upscaler_optimal(up, QUALITY(BALANCED), VIDEO_SCALE_PACK(1920, 1080),
            &render_dims, &min_dims, &max_dims) && render_dims == VIDEO_SCALE_PACK(960, 540));
   CHECK(d3d12_upscaler_optimal(up, QUALITY(NATIVE), VIDEO_SCALE_PACK(1920, 1080),
            &render_dims, &min_dims, &max_dims) && render_dims == VIDEO_SCALE_PACK(1920, 1080));

   backend.data    = up;
   backend.optimal = dispatch_cb;
   video_upscaler_attach(&backend);
   iface = video_upscaler_get_interface();
   CHECK(iface != NULL);
   video_upscaler_set_output(VIDEO_SCALE_PACK(1920, 1080));
   settings->uints.video_temporal_upscaler_quality = VIDEO_TEMPORAL_UPSCALER_QUALITY_BALANCED;
   CHECK(iface->get_request(iface->handle, &req) && req.render_width == 960);

   /* A frame: the context, its pipelines building, then init and the
    * execute, the inputs back in their states */
   memset(&inputs, 0, sizeof(inputs));
   set_input(&inputs.motion_vectors, mv);
   set_input(&inputs.depth, depth);
   inputs.flags      = RETRO_HW_UPSCALER_DEPTH_INVERTED | RETRO_HW_UPSCALER_MV_JITTERED;
   inputs.jitter_x   = 0.25f;
   inputs.jitter_y   = -0.125f;
   inputs.mv_scale_x = 0.0f;
   inputs.mv_scale_y = -1.0f;
   in.texture = color->resource;
   in.format  = color->format;
   in.state   = color->state;
   in.dims    = VIDEO_SCALE_PACK(color->width, color->height);
   rec->build_pending = 1;
   begin_frame();
   memset(&out, 0, sizeof(out));
   CHECK(!d3d12_upscaler_upscale(up, cmd, &in, &inputs, QUALITY(BALANCED),
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   d3d12_upscaler_frame_done(up);
   CHECK(rec->creates == 2 && rec->pipelines == 1 && !rec->build_blocking
         && rec->inits == 0 && rec->executes == 0);
   begin_frame();
   CHECK(d3d12_upscaler_upscale(up, cmd, &in, &inputs, QUALITY(BALANCED),
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   d3d12_upscaler_frame_done(up);
   CHECK(rec->creates == 2 && rec->inits == 1 && rec->executes == 1);
   CHECK(!rec->init[0].vk && rec->init[0].out_w == 1920 && rec->init[0].out_h == 1080
         && rec->init[0].quality == XESS_QUALITY_BALANCED
         && rec->init[0].flags == (XESS_INIT_FLAG_LDR_INPUT_COLOR | XESS_INIT_FLAG_JITTERED_MV
            | XESS_INIT_FLAG_INVERTED_DEPTH | XESS_INIT_FLAG_ENABLE_AUTOEXPOSURE)
         && rec->init[0].node_mask == 1);
   CHECK(rec->velocity_scale_x == 1.0f && rec->velocity_scale_y == -1.0f);
   CHECK(!rec->execute[0].vk && rec->execute[0].context == rec->init[0].context
         && rec->execute[0].command_list == (void*)cmd);
   CHECK(rec->execute[0].color.present && rec->execute[0].color.image == (uint64_t)(uintptr_t)color->resource);
   CHECK(rec->execute[0].depth.image == (uint64_t)(uintptr_t)depth->resource);
   CHECK(rec->execute[0].velocity.image == (uint64_t)(uintptr_t)mv->resource);
   CHECK(rec->execute[0].output.present && rec->execute[0].output.image == (uint64_t)(uintptr_t)out.texture);
   CHECK(!rec->execute[0].exposure.present && !rec->execute[0].mask.present);
   CHECK(rec->execute[0].jitter_x == 0.25f && rec->execute[0].jitter_y == -0.125f);
   CHECK(rec->execute[0].exposure_scale == 1.0f && rec->execute[0].reset == 1);
   CHECK(rec->execute[0].input_w == 1280 && rec->execute[0].input_h == 720);
   CHECK(out.dims == VIDEO_SCALE_PACK(1920, 1080) && out.format == DXGI_FORMAT_R8G8B8A8_UNORM
         && out.state == D3D12_RESOURCE_STATE_COPY_SOURCE);
   CHECK(output_desc_ok(out.texture, 1920, 1080));

   begin_frame();
   CHECK(d3d12_upscaler_upscale(up, cmd, &in, &inputs, QUALITY(BALANCED),
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   d3d12_upscaler_frame_done(up);
   CHECK(rec->creates == 2 && rec->inits == 1 && rec->executes == 2 && rec->execute[1].reset == 0);

   /* A resize: a new context, the old retired after the frames in flight */
   begin_frame();
   CHECK(d3d12_upscaler_upscale(up, cmd, &in, &inputs, QUALITY(BALANCED),
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   d3d12_upscaler_frame_done(up);
   CHECK(rec->creates == 3 && rec->inits == 2 && rec->init[1].out_w == 2560
         && rec->executes == 3 && out.dims == VIDEO_SCALE_PACK(2560, 1440));
   CHECK(rec->destroys == 0);
   for (i = 0; i < 30 && rec->destroys < 1; i++)
      d3d12_upscaler_frame_done(up);
   CHECK(rec->destroys == 1 && i >= 15);

   /* An execute that fails: the frame shown as it is until the output
    * or mode changes */
   rec->execute_fails = 1;
   begin_frame();
   CHECK(!d3d12_upscaler_upscale(up, cmd, &in, &inputs, QUALITY(BALANCED),
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   d3d12_upscaler_frame_done(up);
   rec->execute_fails = 0;
   begin_frame();
   CHECK(!d3d12_upscaler_upscale(up, cmd, &in, &inputs, QUALITY(BALANCED),
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   d3d12_upscaler_frame_done(up);
   CHECK(rec->executes == 4);
   begin_frame();
   CHECK(d3d12_upscaler_upscale(up, cmd, &in, &inputs, QUALITY(QUALITY),
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   d3d12_upscaler_frame_done(up);
   CHECK(rec->creates == 4 && rec->inits == 3 && rec->executes == 5);

   d3d12_upscaler_free(up);
   CHECK(rec->destroys == 4);   /* every context, the query one too */
   video_upscaler_detach();
}

/* A frame below its mode's range but in another's runs in that mode,
 * the nearest taking it, and back; one no mode takes is refused */
static void test_mode_for(texture_t *color, texture_t *mv, texture_t *depth)
{
   fake_ngx_record_t *rec = record();
   d3d12_ngx_t *ngx;
   d3d12_ngx_image_t in, out;
   struct retro_hw_upscaler_inputs inputs;
   unsigned creates, evals;

   ngx = d3d12_ngx_init(device, "Zdir");
   CHECK(ngx != NULL);
   if (!ngx)
      return;
   memset(&inputs, 0, sizeof(inputs));
   set_input(&inputs.motion_vectors, mv);
   set_input(&inputs.depth, depth);
   in.texture = color->resource;
   in.format  = color->format;
   in.state   = color->state;
   creates    = rec->creates;
   evals      = rec->evals;

   in.dims    = VIDEO_SCALE_PACK(400, 225);
   begin_frame();
   CHECK(d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, 0,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   d3d12_ngx_frame_done(ngx);
   CHECK(rec->creates == creates + 1 && rec->evals == evals + 1);
   CHECK(rec->create[creates % FAKE_NGX_MAX].quality == NGX_PERF_QUALITY_ULTRA_PERFORMANCE
         && rec->create[creates % FAKE_NGX_MAX].width == 1920);
   CHECK(rec->eval[evals % FAKE_NGX_MAX].subrect_w == 400
         && rec->eval[evals % FAKE_NGX_MAX].subrect_h == 225);

   in.dims    = VIDEO_SCALE_PACK(1280, 720);
   begin_frame();
   CHECK(d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, 0,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   d3d12_ngx_frame_done(ngx);
   CHECK(rec->creates == creates + 2
         && rec->create[(creates + 1) % FAKE_NGX_MAX].quality == NGX_PERF_QUALITY_MAX_QUALITY);

   in.dims    = VIDEO_SCALE_PACK(200, 112);
   begin_frame();
   CHECK(!d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, 0,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   d3d12_ngx_frame_done(ngx);
   CHECK(rec->creates == creates + 2 && rec->evals == evals + 2);

   d3d12_ngx_free(ngx);
}

/* DLSS Neural Rendering over Super Resolution's output: offered only
 * with its library in the DLSS directory, the feature made with the
 * model's tuning and evaluated on Super Resolution's output with the
 * core's guides, its output what the frame shows, made again only once
 * a changed setting has held, dropped when turned off, and a core
 * without it (an older driver) leaving Super Resolution alone */
static void test_nr(settings_t *settings, texture_t *color, texture_t *mv,
      texture_t *depth)
{
   fake_ngx_record_t *rec = record();
   d3d12_ngx_t *ngx;
   d3d12_ngx_image_t in, out;
   struct retro_hw_upscaler_inputs inputs;
   char status[256];
   unsigned char px[4];
   unsigned nr, nr2, creates, evals, releases, i, sr, made;

   settings->uints.video_temporal_upscaler = VIDEO_TEMPORAL_UPSCALER_DLSS;
   clear_dlssnr_runtime("Zdir");
   video_upscaler_probe(VIDEO_TEMPORAL_UPSCALER_DLSS);
   CHECK(!video_upscaler_nr_available());
   plant_dlssnr_runtime("Zdir");
   video_upscaler_probe(VIDEO_TEMPORAL_UPSCALER_DLSS);
   CHECK(video_upscaler_nr_available());
   CHECK(video_upscaler_dlssnr_version() == NGX_VERSION_PACK(310, 2, 1));
   video_upscaler_status_string(status, sizeof(status));
   CHECK(!strcmp(status, "DLSS (NGX: found, DLSS runtime 310.2.1, Neural Rendering 310.2.1)"));

   ngx = d3d12_ngx_init(device, "Zdir");
   CHECK(ngx != NULL);
   if (!ngx)
      return;
   memset(&inputs, 0, sizeof(inputs));
   set_input(&inputs.motion_vectors, mv);
   set_input(&inputs.depth, depth);
   inputs.flags      = RETRO_HW_UPSCALER_DEPTH_INVERTED;
   inputs.mv_scale_x = -2.0f;
   inputs.mv_scale_y = 3.0f;
   in.texture = color->resource;
   in.format  = color->format;
   in.state   = color->state;
   in.dims    = VIDEO_SCALE_PACK(color->width, color->height);
   nr = VIDEO_UPSCALER_NR_PACK(VIDEO_UPSCALER_NR_ON | VIDEO_UPSCALER_NR_AUTO_MASK,
         1, 2, 20, 30, 10, 0);

   /* The first frame: Super Resolution, then the model on its output */
   creates = rec->creates;
   evals   = rec->evals;
   begin_frame();
   CHECK(d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, nr,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   d3d12_ngx_frame_done(ngx);
   CHECK(rec->creates == creates + 2 && rec->evals == evals + 2);
   CHECK(rec->create[creates % FAKE_NGX_MAX].feature == 1
         && rec->create[(creates + 1) % FAKE_NGX_MAX].feature == 18);
   i = (creates + 1) % FAKE_NGX_MAX;
   CHECK(rec->create[i].cmd == (void*)cmd
         && rec->create[i].out_width == 1920 && rec->create[i].out_height == 1080
         && rec->create[i].node_mask == 0x101);
   CHECK(rec->create[i].nr_preset == 1 && rec->create[i].nr_style == 2
         && rec->create[i].nr_auto_mask == 1);
   CHECK(rec->create[i].nr_intensity == 1.0f && rec->create[i].nr_structure == 1.5f
         && rec->create[i].nr_tone == 0.5f && rec->create[i].nr_skin == -1.0f);
   sr = evals % FAKE_NGX_MAX;
   i  = (evals + 1) % FAKE_NGX_MAX;
   CHECK(!rec->eval[sr].nr && rec->eval[i].nr);
   CHECK(rec->eval[i].color == rec->eval[sr].output);
   CHECK(rec->eval[i].output == (void*)out.texture && out.texture != rec->eval[sr].output);
   CHECK(rec->eval[i].depth == (void*)depth->resource
         && rec->eval[i].motion_vectors == (void*)mv->resource);
   CHECK(rec->eval[i].subrect_w == 1920 && rec->eval[i].subrect_h == 1080);
   CHECK(rec->eval[i].depth_w == 1280 && rec->eval[i].depth_h == 720
         && rec->eval[i].mv_w == 1280 && rec->eval[i].mv_h == 720);
   CHECK(rec->eval[i].depth_inverted == 1 && rec->eval[i].reset == 1);
   CHECK(rec->eval[i].mv_scale_x == -2.0f && rec->eval[i].mv_scale_y == 3.0f);
   CHECK(out.dims == VIDEO_SCALE_PACK(1920, 1080)
         && out.state == D3D12_RESOURCE_STATE_COPY_SOURCE);
   /* (the stand-in copies its input through: what Super Resolution's
    * copy left reaches the model's output, a copy source) */
   CHECK(read_pixel(out.texture, 1920, 1080, 300, 200, px));
   CHECK(px[0] == (300 & 0xff) && px[1] == 200 && px[2] == 1 && px[3] == 0);

   /* The next frame: the same feature, no reset */
   begin_frame();
   CHECK(d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, nr,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   d3d12_ngx_frame_done(ngx);
   CHECK(rec->creates == creates + 2 && rec->eval[(rec->evals - 1) % FAKE_NGX_MAX].nr
         && rec->eval[(rec->evals - 1) % FAKE_NGX_MAX].reset == 0);

   /* A changed setting: the model made again only once it has held */
   nr2 = VIDEO_UPSCALER_NR_PACK(VIDEO_UPSCALER_NR_ON, 1, 2, 30, 30, 10, 0);
   for (i = 0; i < 40 && rec->creates == creates + 2; i++)
   {
      begin_frame();
      CHECK(d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, nr2,
               VIDEO_SCALE_PACK(1920, 1080), &out));
      end_frame();
      d3d12_ngx_frame_done(ngx);
   }
   CHECK(rec->creates == creates + 3 && i >= 25);
   i = (rec->creates - 1) % FAKE_NGX_MAX;
   CHECK(rec->create[i].feature == 18 && rec->create[i].nr_intensity == 1.5f
         && rec->create[i].nr_auto_mask == 0);

   /* Off: Super Resolution's frame, the model kept; on again, the same
    * model, its history reset once, its output read as before */
   for (i = 0; i < 30; i++)
      d3d12_ngx_frame_done(ngx);
   made     = rec->creates;
   releases = rec->releases;
   evals    = rec->evals;
   begin_frame();
   CHECK(d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, 0,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   CHECK(rec->evals == evals + 1 && !rec->eval[evals % FAKE_NGX_MAX].nr
         && rec->eval[evals % FAKE_NGX_MAX].output == (void*)out.texture);
   for (i = 0; i < 30; i++)
      d3d12_ngx_frame_done(ngx);
   CHECK(rec->releases == releases);
   for (i = 0; i < 2; i++)
   {
      evals = rec->evals;
      begin_frame();
      CHECK(d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, nr2,
               VIDEO_SCALE_PACK(1920, 1080), &out));
      end_frame();
      d3d12_ngx_frame_done(ngx);
      CHECK(rec->creates == made && rec->evals == evals + 2
            && rec->eval[(evals + 1) % FAKE_NGX_MAX].nr
            && rec->eval[(evals + 1) % FAKE_NGX_MAX].reset == (i == 0));
      CHECK(out.state == D3D12_RESOURCE_STATE_COPY_SOURCE
            && read_pixel(out.texture, 1920, 1080, 300, 200, px)
            && px[0] == (300 & 0xff) && px[1] == 200);
   }
   d3d12_ngx_free(ngx);

   /* Off too, the core is asked about the model, so that turning it on
    * needs nothing made again: a core without it is found out */
   video_upscaler_note_nr(true);
   rec->nr_missing = 1;
   settings->bools.video_dlss_nr_enable = false;
   ngx = d3d12_ngx_init(device, "Zdir");
   CHECK(ngx != NULL);
   if (ngx)
   {
      CHECK(!video_upscaler_nr_available());
      d3d12_ngx_free(ngx);
   }

   /* A core without the model, as before its driver: not offered, said
    * so, and Super Resolution goes on */
   settings->bools.video_dlss_nr_enable = true;
   ngx = d3d12_ngx_init(device, "Zdir");
   CHECK(ngx != NULL);
   CHECK(!video_upscaler_nr_available());
   video_upscaler_status_string(status, sizeof(status));
   CHECK(strstr(status, "Neural Rendering: needs a newer driver") != NULL);
   if (ngx)
   {
      creates = rec->creates;
      begin_frame();
      CHECK(d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, nr,
               VIDEO_SCALE_PACK(1920, 1080), &out));
      end_frame();
      d3d12_ngx_frame_done(ngx);
      CHECK(rec->creates == creates + 1
            && rec->create[creates % FAKE_NGX_MAX].feature == 1);
      d3d12_ngx_free(ngx);
   }
   settings->bools.video_dlss_nr_enable = false;
   rec->nr_missing = 0;
   video_upscaler_note_nr(true);
   clear_dlssnr_runtime("Zdir");
   video_upscaler_probe(VIDEO_TEMPORAL_UPSCALER_DLSS);
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

   /* as a core does in retro_load_game */
   video_upscaler_note_wanted();
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
   /* no runtime in the directory: the driver library is never called */
   clear_dlss_runtime("Zdir");
   CHECK(video_upscaler_dlss_version() == 0);
   CHECK(d3d12_ngx_init(device, "Zdir") == NULL);
   CHECK(rec->init_calls == 0);
   plant_dlss_runtime("Zdir");
   CHECK(video_upscaler_probe(VIDEO_TEMPORAL_UPSCALER_DLSS) == VIDEO_UPSCALER_READY);
   CHECK(video_upscaler_dlss_version() == NGX_VERSION_PACK(310, 2, 1));
   {
      char status[256];
      video_upscaler_status_string(status, sizeof(status));
      CHECK(!strcmp(status, "DLSS (NGX: found, DLSS runtime 310.2.1)"));
   }
   CHECK(!video_upscaler_optiscaler());
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

   /* The mode's size and the range over all modes (Ultra Performance's
    * half of 640x360 the floor), through the capability callback,
    * asked once a mode */
   CHECK(d3d12_ngx_optimal(ngx, NGX_PERF_QUALITY_MAX_QUALITY,
            VIDEO_SCALE_PACK(1920, 1080), &render_dims, &min_dims, &max_dims));
   CHECK(render_dims == VIDEO_SCALE_PACK(1280, 720)
         && max_dims == VIDEO_SCALE_PACK(1920, 1080)
         && min_dims == VIDEO_SCALE_PACK(320, 180));
   CHECK(rec->optimal_calls == 6);
   CHECK(d3d12_ngx_optimal(ngx, NGX_PERF_QUALITY_MAX_QUALITY,
            VIDEO_SCALE_PACK(1920, 1080), &render_dims, &min_dims, &max_dims));
   CHECK(rec->optimal_calls == 6);
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
   CHECK(req.min_render_width == 320 && req.max_render_width == 1920);
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
   CHECK(d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, 0,
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
    * honoured when asked; a frame time the core left at 0 is the
    * period of its frame (DLSS weighs its history by it, 0 is not a
    * frame) */
   inputs.frame_time_ms = 0.0f;
   stub_core_hz         = 50.0f;
   begin_frame();
   CHECK(d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, 0,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   d3d12_ngx_frame_done(ngx);
   CHECK(rec->creates == 1 && rec->evals == 2 && rec->eval[1].reset == 0);
   CHECK(rec->eval[1].frame_time == 20.0f);
   inputs.frame_time_ms = 16.6f;
   stub_core_hz         = 0.0f;
   /* (and the core's transparency mask reaches DLSS as the mask it
    * reads, the current color's bias; a stand-in resource) */
   inputs.flags |= RETRO_HW_UPSCALER_RESET;
   set_input(&inputs.transparency_mask, &mv);
   begin_frame();
   CHECK(d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, 0,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   d3d12_ngx_frame_done(ngx);
   CHECK(rec->evals == 3 && rec->eval[2].reset == 1 && rec->creates == 1);
   CHECK(rec->eval[2].transparency == (void*)mv.resource);
   inputs.flags &= ~RETRO_HW_UPSCALER_RESET;
   memset(&inputs.transparency_mask, 0, sizeof(inputs.transparency_mask));

   /* A smaller frame within the range: the same feature, the subrect
    * says the size; one outside: refused */
   in.dims = VIDEO_SCALE_PACK(960, 540);
   begin_frame();
   CHECK(d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, 0,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   d3d12_ngx_frame_done(ngx);
   CHECK(rec->creates == 1 && rec->evals == 4
         && rec->eval[3].subrect_w == 960 && rec->eval[3].subrect_h == 540);
   in.texture = color_big.resource;
   in.dims    = VIDEO_SCALE_PACK(color_big.width, color_big.height);
   begin_frame();
   CHECK(!d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, 0,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   end_frame();
   d3d12_ngx_frame_done(ngx);
   CHECK(rec->evals == 4);
   in.texture = color.resource;
   in.dims    = VIDEO_SCALE_PACK(color.width, color.height);

   /* No inputs, no motion vectors: refused */
   CHECK(!d3d12_ngx_upscale(ngx, cmd, &in, NULL, NGX_PERF_QUALITY_MAX_QUALITY, 0,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   inputs.motion_vectors.image = 0;
   CHECK(!d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, 0,
            VIDEO_SCALE_PACK(1920, 1080), &out));
   set_input(&inputs.motion_vectors, &mv);

   /* A resize: a new feature and output, the old retired and released
    * once the frames that could read them are through */
   begin_frame();
   CHECK(d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, 0,
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
   CHECK(d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_BALANCED, 0,
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   d3d12_ngx_frame_done(ngx);
   CHECK(rec->creates == 3 && rec->create[2].quality == NGX_PERF_QUALITY_BALANCED);
   CHECK(rec->create[2].preset == NGX_DLSS_PRESET_DEFAULT);

   /* The render preset in the mode word: a new feature with the hint set */
   begin_frame();
   CHECK(d3d12_ngx_upscale(ngx, cmd, &in, &inputs, (NGX_MODE_PACK(NGX_PERF_QUALITY_BALANCED, NGX_DLSS_PRESET_K)), 0,
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   d3d12_ngx_frame_done(ngx);
   CHECK(rec->creates == 4 && rec->create[3].quality == NGX_PERF_QUALITY_BALANCED
         && rec->create[3].preset == NGX_DLSS_PRESET_K);
   begin_frame();
   CHECK(d3d12_ngx_upscale(ngx, cmd, &in, &inputs, (NGX_MODE_PACK(NGX_PERF_QUALITY_BALANCED, NGX_DLSS_PRESET_K)), 0,
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   d3d12_ngx_frame_done(ngx);
   CHECK(rec->creates == 4);
   begin_frame();
   CHECK(d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_BALANCED, 0,
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   d3d12_ngx_frame_done(ngx);
   CHECK(rec->creates == 5 && rec->create[4].preset == NGX_DLSS_PRESET_DEFAULT);

   /* An evaluate that fails: the frame shown as it is, the feature left
    * alone until something new is asked of it */
   rec->eval_fails = 1;
   begin_frame();
   CHECK(!d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_BALANCED, 0,
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   d3d12_ngx_frame_done(ngx);
   rec->eval_fails = 0;
   begin_frame();
   CHECK(!d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_BALANCED, 0,
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   d3d12_ngx_frame_done(ngx);
   begin_frame();
   CHECK(d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_QUALITY, 0,
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   d3d12_ngx_frame_done(ngx);
   CHECK(rec->creates == 6);

   /* A feature that will not create: refused, nothing leaks */
   rec->create_fails = 1;
   begin_frame();
   CHECK(!d3d12_ngx_upscale(ngx, cmd, &in, &inputs, NGX_PERF_QUALITY_MAX_PERF, 0,
            VIDEO_SCALE_PACK(2560, 1440), &out));
   end_frame();
   d3d12_ngx_frame_done(ngx);
   rec->create_fails = 0;

   /* Teardown releases everything, retired or current */
   {
      unsigned releases = rec->releases;
      d3d12_ngx_free(ngx);
      CHECK(rec->releases == releases + 5);  /* the five retired */
      CHECK(rec->shutdown_calls == 2 && rec->params_destroyed == 2);
      CHECK(rec->shutdown_path0 == 'Z');
   }
   video_upscaler_detach();
   CHECK(video_upscaler_get_interface() == NULL);

   test_mode_for(&color, &mv, &depth);
   test_nr(settings, &color, &mv, &depth);

   /* DLSS behind the dispatch: the frontend's mode word translated to
    * NGX's quality, preset and sharpness; the session's tables name
    * OptiScaler, so the status does */
   rec->optiscaler = 1;
   {
      d3d12_upscaler_t *up = d3d12_upscaler_init(VIDEO_TEMPORAL_UPSCALER_DLSS, device, "Zdir");
      d3d12_upscaler_image_t din, dout;
      unsigned creates = rec->creates;
      char status[256];
      CHECK(up != NULL);
      CHECK(video_upscaler_optiscaler());
      video_upscaler_status_string(status, sizeof(status));
      CHECK(!strcmp(status, "DLSS (NGX: found, DLSS runtime 310.2.1, via OptiScaler)"));
      if (up)
      {
         CHECK(d3d12_upscaler_optimal(up,
                  VIDEO_UPSCALER_MODE_PACK(VIDEO_TEMPORAL_UPSCALER_QUALITY_PERFORMANCE, 0),
                  VIDEO_SCALE_PACK(1920, 1080), &render_dims, &min_dims, &max_dims)
               && render_dims == VIDEO_SCALE_PACK(960, 540));
         din.texture = color.resource;
         din.format  = color.format;
         din.state   = color.state;
         din.dims    = VIDEO_SCALE_PACK(color.width, color.height);
         begin_frame();
         CHECK(d3d12_upscaler_upscale(up, cmd, &din, &inputs,
                  VIDEO_UPSCALER_MODE_PACK(VIDEO_TEMPORAL_UPSCALER_QUALITY_QUALITY,
                     VIDEO_TEMPORAL_UPSCALER_PRESET_K),
                  VIDEO_SCALE_PACK(1920, 1080), &dout));
         end_frame();
         d3d12_upscaler_frame_done(up);
         CHECK(rec->creates == creates + 1
               && rec->create[creates].quality == NGX_PERF_QUALITY_MAX_QUALITY
               && rec->create[creates].preset == NGX_DLSS_PRESET_K
               && rec->create[creates].out_width == 1920);
         CHECK(dout.dims == VIDEO_SCALE_PACK(1920, 1080)
               && dout.state == D3D12_RESOURCE_STATE_COPY_SOURCE);
         CHECK(rec->eval[(rec->evals - 1) % FAKE_NGX_MAX].sharpness == 0.0f);
         begin_frame();
         CHECK(d3d12_upscaler_upscale(up, cmd, &din, &inputs,
                  VIDEO_UPSCALER_MODE_PACK_SHARP(VIDEO_TEMPORAL_UPSCALER_QUALITY_QUALITY,
                     VIDEO_TEMPORAL_UPSCALER_PRESET_K, 50),
                  VIDEO_SCALE_PACK(1920, 1080), &dout));
         end_frame();
         d3d12_upscaler_frame_done(up);
         CHECK(rec->creates == creates + 1 && rec->eval[(rec->evals - 1) % FAKE_NGX_MAX].sharpness == 0.5f);
         d3d12_upscaler_free(up);
      }
   }

   test_ffx(settings, &color, &mv, &depth);
   test_xess(settings, &color, &mv, &depth);

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
   printf("d3d12_upscaler_test: ok (%d checks)\n", checks);
   return 0;
}
