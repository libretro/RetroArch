/* The D3D11 video backend (gfx/common/d3d11_video_decode.c) on a real
 * GPU, against rh264 in software.
 *
 * Each stream of d3d11_video_decode_test_data.h decodes twice: in
 * software, and on the backend, whose frames this reads back the way
 * the driver imports them - the shared texture and fences opened on a
 * device of its own, the copy waited for on the GPU, the frame given
 * back by raising its fence. H.264 decodes exactly, so every frame
 * must match the software one to the sample.
 *
 * Where there is a D3D12 device, every stream is read once more through
 * it, as the D3D12 driver opens the same handles.
 *
 * The streams with scaling matrices also decode with the lists in
 * raster order, and the run says which order the GPU took: the
 * backend sends them in coded (zig-zag) order.
 *
 * Windows 10 1703 or later with a GPU that decodes H.264; anything
 * less (Wine, a remote session, no video decoder) skips with exit 0.
 * run.sh builds it with mingw-w64. */
#ifndef CINTERFACE
#define CINTERFACE
#endif
#ifndef COBJMACROS
#define COBJMACROS
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The backend under test, built in so that its list order can be
 * switched */
static int test_qm_raster;
#define D3D11VD_QM_RASTER test_qm_raster
#include "../../../gfx/common/d3d11_video_decode.c"

#include <d3d12.h>
#include <formats/rmp4.h>
#include "d3d11_video_decode_test_data.h"
#include "dxva_hevc_access_unit_data.h"

#include "frontend_stubs.h"

/* ---- the reader ---- */

#define MAX_FRAMES 32

static const GUID test_iid_texture2d =
   { 0x6f15aaf2, 0xd208, 0x4e89, { 0x9a, 0xb4, 0x48, 0x95, 0x35, 0xd3, 0x4f, 0x9c } };

static int fails;
static ID3D11Device         *dev;
static ID3D11DeviceContext  *ctx;
static ID3D11Device5        *dev5;
static ID3D11DeviceContext4 *ctx4;

typedef struct
{
   int n;
   unsigned w, h;
   uint8_t *y[MAX_FRAMES], *uv[MAX_FRAMES];   /* uv interleaved, NV12 */
} frames_t;

static void check(const char *what, int ok)
{
   printf("  %-64s %s\n", what, ok ? "ok" : "FAIL");
   if (!ok)
      fails++;
}

static void frames_free(frames_t *f)
{
   int i;
   for (i = 0; i < f->n; i++)
   {
      free(f->y[i]);
      free(f->uv[i]);
   }
   memset(f, 0, sizeof(*f));
}

static uint8_t *frame_alloc(frames_t *f, size_t n)
{
   (void)f;
   return (uint8_t*)calloc(1, n);
}

/* A software picture, as NV12 */
static void take_sw(frames_t *f, rh264_video *v)
{
   const uint8_t *p[3];
   int s[3], w, h, cw, ch, i, x, y;
   if (f->n >= MAX_FRAMES)
      return;
   for (i = 0; i < 3; i++)
      p[i] = rh264_video_plane(v, i, &s[i], i ? NULL : &w, i ? NULL : &h);
   if (!p[0] || !p[1] || !p[2])
      return;
   cw = (w + 1) / 2;
   ch = (h + 1) / 2;
   f->w = (unsigned)w;
   f->h = (unsigned)h;
   i    = f->n++;
   f->y[i]  = frame_alloc(f, (size_t)w * h);
   f->uv[i] = frame_alloc(f, (size_t)cw * 2 * ch);
   if (!f->y[i] || !f->uv[i])
      return;
   for (y = 0; y < h; y++)
      memcpy(f->y[i] + (size_t)y * w, p[0] + (size_t)y * s[0], (size_t)w);
   for (y = 0; y < ch; y++)
      for (x = 0; x < cw; x++)
      {
         f->uv[i][(size_t)y * cw * 2 + x * 2]     = p[1][(size_t)y * s[1] + x];
         f->uv[i][(size_t)y * cw * 2 + x * 2 + 1] = p[2][(size_t)y * s[2] + x];
      }
}

/* What was opened of one decoder's handles */
typedef struct
{
   unsigned gen;
   int n_tex, n_fence;
   uint64_t tex_h[8], fence_h[8];
   ID3D11Texture2D *tex[8];
   ID3D11Fence *fence[8];
   ID3D11Texture2D *staging;
} opened_t;

static void opened_clear(opened_t *o)
{
   int i;
   for (i = 0; i < o->n_tex; i++)
      D3D11VD_RELEASE(o->tex[i]);
   for (i = 0; i < o->n_fence; i++)
      D3D11VD_RELEASE(o->fence[i]);
   D3D11VD_RELEASE(o->staging);
   memset(o, 0, sizeof(*o));
}

static ID3D11Texture2D *open_tex(opened_t *o, uint64_t h)
{
   ID3D11Texture2D *t = NULL;
   int i;
   for (i = 0; i < o->n_tex; i++)
      if (o->tex_h[i] == h)
         return o->tex[i];
   if (     o->n_tex >= 8
         || FAILED(dev5->lpVtbl->OpenSharedResource1(dev5,
               (HANDLE)(uintptr_t)h, &test_iid_texture2d, (void**)&t)))
      return NULL;
   o->tex_h[o->n_tex] = h;
   o->tex[o->n_tex++] = t;
   return t;
}

static ID3D11Fence *open_fence(opened_t *o, uint64_t h)
{
   ID3D11Fence *f = NULL;
   int i;
   for (i = 0; i < o->n_fence; i++)
      if (o->fence_h[i] == h)
         return o->fence[i];
   if (     o->n_fence >= 8
         || FAILED(dev5->lpVtbl->OpenSharedFence(dev5, (HANDLE)(uintptr_t)h,
               &d3d11vd_iid_fence, (void**)&f)))
      return NULL;
   o->fence_h[o->n_fence] = h;
   o->fence[o->n_fence++] = f;
   return f;
}

/* The frame the backend handed out, read back as the driver reads it,
 * then given back */
static int take_hw(frames_t *f, opened_t *o, const video_hw_frame_t *fr)
{
   D3D11_MAPPED_SUBRESOURCE m;
   ID3D11Texture2D *tex;
   ID3D11Fence *wait, *done;
   unsigned w = fr->width, h = fr->height, cw = (w + 1) / 2, ch = (h + 1) / 2;
   unsigned y, bps = fr->pixfmt == IMAGE_PIXFMT_P010 ? 2 : 1;
   int i;

   if (o->gen != fr->layout)
   {
      opened_clear(o);
      o->gen = fr->layout;
   }
   if (     !(tex  = open_tex(o, fr->image))
         || !(wait = open_fence(o, fr->wait))
         || !(done = open_fence(o, fr->done)))
   {
      video_hw_frame_abandon(fr);
      return 0;
   }
   if (!o->staging)
   {
      D3D11_TEXTURE2D_DESC d;
      memset(&d, 0, sizeof(d));
      d.Width            = w;
      d.Height           = h;
      d.MipLevels        = 1;
      d.ArraySize        = 1;
      d.Format           = bps == 2 ? DXGI_FORMAT_P010 : DXGI_FORMAT_NV12;
      d.SampleDesc.Count = 1;
      d.Usage            = D3D11_USAGE_STAGING;
      d.CPUAccessFlags   = D3D11_CPU_ACCESS_READ;
      if (FAILED(dev->lpVtbl->CreateTexture2D(dev, &d, NULL, &o->staging)))
      {
         video_hw_frame_abandon(fr);
         return 0;
      }
   }
   ctx4->lpVtbl->Wait(ctx4, wait, fr->wait_value);
   ctx->lpVtbl->CopyResource(ctx, (ID3D11Resource*)o->staging,
         (ID3D11Resource*)tex);
   ctx4->lpVtbl->Signal(ctx4, done, fr->done_value);
   ctx->lpVtbl->Flush(ctx);
   if (FAILED(ctx->lpVtbl->Map(ctx, (ID3D11Resource*)o->staging, 0,
         D3D11_MAP_READ, 0, &m)))
      return 0;
   if (f->n < MAX_FRAMES)
   {
      i        = f->n++;
      f->w     = w;
      f->h     = h;
      f->y[i]  = frame_alloc(f, (size_t)w * h * bps);
      f->uv[i] = frame_alloc(f, (size_t)cw * 2 * ch * bps);
      if (f->y[i] && f->uv[i])
      {
         const uint8_t *p = (const uint8_t*)m.pData;
         for (y = 0; y < h; y++)
            memcpy(f->y[i] + (size_t)y * w * bps, p + (size_t)y * m.RowPitch, w * bps);
         /* the chroma plane follows the luma rows */
         p += (size_t)m.RowPitch * h;
         for (y = 0; y < ch; y++)
            memcpy(f->uv[i] + (size_t)y * cw * 2 * bps, p + (size_t)y * m.RowPitch,
                  cw * 2 * bps);
      }
   }
   ctx->lpVtbl->Unmap(ctx, (ID3D11Resource*)o->staging, 0);
   return 1;
}

/* ---- the same frames read through D3D12 ---- */

static const GUID test_iid_d12_device =
   { 0x189819f1, 0x1db6, 0x4b57, { 0xbe, 0x54, 0x18, 0x21, 0x33, 0x9b, 0x85, 0xf7 } };
static const GUID test_iid_d12_queue =
   { 0x0ec870a6, 0x5d7e, 0x4c22, { 0x8c, 0xfc, 0x5b, 0xaa, 0xe0, 0x76, 0x16, 0xed } };
static const GUID test_iid_d12_alloc =
   { 0x6102dee4, 0xaf59, 0x4b09, { 0xb9, 0x99, 0xb4, 0x4d, 0x73, 0xf0, 0x9b, 0x24 } };
static const GUID test_iid_d12_list =
   { 0x5b160d0f, 0xac1b, 0x4185, { 0x8b, 0xa8, 0xb3, 0xae, 0x42, 0xa5, 0xa4, 0x55 } };
static const GUID test_iid_d12_fence =
   { 0x0a753dcf, 0xc4d8, 0x4b91, { 0xad, 0xf6, 0xbe, 0x5a, 0x60, 0xd9, 0x5a, 0x76 } };
static const GUID test_iid_d12_resource =
   { 0x696442be, 0xa72e, 0x4059, { 0xbc, 0x79, 0x5b, 0x5c, 0x98, 0x04, 0x0f, 0xad } };

typedef HRESULT (WINAPI *d3d12_create_device_t)(IUnknown*, D3D_FEATURE_LEVEL,
      REFIID, void**);

static int use_d3d12;
static ID3D12Device              *d12;
static ID3D12CommandQueue        *q12;
static ID3D12CommandAllocator    *a12;
static ID3D12GraphicsCommandList *l12;
static ID3D12Fence               *f12;
static UINT64                     f12_value;
static HANDLE                     ev12;
static ID3D12Resource            *rb12;
static UINT64                     rb12_size;

typedef struct
{
   unsigned gen;
   int n_tex, n_fence;
   uint64_t tex_h[8], fence_h[8];
   ID3D12Resource *tex[8];
   ID3D12Fence *fence[8];
} opened12_t;

static opened12_t o12;

static void opened12_clear(opened12_t *o)
{
   int i;
   for (i = 0; i < o->n_tex; i++)
      D3D11VD_RELEASE(o->tex[i]);
   for (i = 0; i < o->n_fence; i++)
      D3D11VD_RELEASE(o->fence[i]);
   memset(o, 0, sizeof(*o));
}

static void *open12(uint64_t h, const GUID *iid, uint64_t *hs, void **objs,
      int *n)
{
   void *p = NULL;
   int i;
   for (i = 0; i < *n; i++)
      if (hs[i] == h)
         return objs[i];
   if (     *n >= 8
         || FAILED(d12->lpVtbl->OpenSharedHandle(d12, (HANDLE)(uintptr_t)h,
               iid, &p)))
      return NULL;
   hs[*n]     = h;
   objs[(*n)++] = p;
   return p;
}

/* ID3D12Resource::GetDesc returns a struct; called as what it is */
static void d12_desc(ID3D12Resource *r, D3D12_RESOURCE_DESC *d)
{
   D3D12_RESOURCE_DESC *(STDMETHODCALLTYPE *get)(ID3D12Resource*,
         D3D12_RESOURCE_DESC*) = (D3D12_RESOURCE_DESC *(STDMETHODCALLTYPE *)(
         ID3D12Resource*, D3D12_RESOURCE_DESC*))(void (*)(void))
         r->lpVtbl->GetDesc;
   get(r, d);
}

static void d12_transition(ID3D12Resource *r, D3D12_RESOURCE_STATES a,
      D3D12_RESOURCE_STATES b)
{
   D3D12_RESOURCE_BARRIER bar;
   memset(&bar, 0, sizeof(bar));
   bar.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
   bar.Transition.pResource   = r;
   bar.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
   bar.Transition.StateBefore = a;
   bar.Transition.StateAfter  = b;
   l12->lpVtbl->ResourceBarrier(l12, 1, &bar);
}

static int d12_init(void)
{
   static const D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0;
   D3D12_COMMAND_QUEUE_DESC qd;
   d3d12_create_device_t create;
   HMODULE mod = LoadLibraryA("d3d12.dll");
   if (     !mod
         || !(create = (d3d12_create_device_t)(void (*)(void))GetProcAddress(
               mod, "D3D12CreateDevice"))
         || FAILED(create(NULL, fl, &test_iid_d12_device, (void**)&d12)))
      return 0;
   memset(&qd, 0, sizeof(qd));
   qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
   return SUCCEEDED(d12->lpVtbl->CreateCommandQueue(d12, &qd,
            &test_iid_d12_queue, (void**)&q12))
      && SUCCEEDED(d12->lpVtbl->CreateCommandAllocator(d12,
            D3D12_COMMAND_LIST_TYPE_DIRECT, &test_iid_d12_alloc,
            (void**)&a12))
      && SUCCEEDED(d12->lpVtbl->CreateCommandList(d12, 0,
            D3D12_COMMAND_LIST_TYPE_DIRECT, a12, NULL, &test_iid_d12_list,
            (void**)&l12))
      && SUCCEEDED(l12->lpVtbl->Close(l12))
      && SUCCEEDED(d12->lpVtbl->CreateFence(d12, 0, D3D12_FENCE_FLAG_NONE,
            &test_iid_d12_fence, (void**)&f12))
      && (ev12 = CreateEventA(NULL, FALSE, FALSE, NULL)) != NULL;
}

static void d12_deinit(void)
{
   opened12_clear(&o12);
   D3D11VD_RELEASE(rb12);
   D3D11VD_RELEASE(f12);
   D3D11VD_RELEASE(l12);
   D3D11VD_RELEASE(a12);
   D3D11VD_RELEASE(q12);
   D3D11VD_RELEASE(d12);
   if (ev12)
      CloseHandle(ev12);
   ev12 = NULL;
}

/* The frame read through D3D12: the producer's fence waited for on the
 * queue, both planes copied out, the frame given back behind them */
static int take_hw12(frames_t *f, const video_hw_frame_t *fr)
{
   D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp[2];
   D3D12_TEXTURE_COPY_LOCATION src, dst;
   D3D12_RESOURCE_DESC desc;
   ID3D12Resource *tex;
   ID3D12Fence *wait, *done;
   UINT64 total = 0;
   unsigned w = fr->width, h = fr->height, cw = (w + 1) / 2, ch = (h + 1) / 2;
   unsigned y, p;
   uint8_t *m;
   int i;

   if (o12.gen != fr->layout)
   {
      opened12_clear(&o12);
      o12.gen = fr->layout;
   }
   if (     !(tex  = (ID3D12Resource*)open12(fr->image,
               &test_iid_d12_resource, o12.tex_h, (void**)o12.tex,
               &o12.n_tex))
         || !(wait = (ID3D12Fence*)open12(fr->wait, &test_iid_d12_fence,
               o12.fence_h, (void**)o12.fence, &o12.n_fence))
         || !(done = (ID3D12Fence*)open12(fr->done, &test_iid_d12_fence,
               o12.fence_h, (void**)o12.fence, &o12.n_fence)))
   {
      video_hw_frame_abandon(fr);
      return 0;
   }
   d12_desc(tex, &desc);
   d12->lpVtbl->GetCopyableFootprints(d12, &desc, 0, 2, 0, fp, NULL, NULL,
         &total);
   if (!rb12 || rb12_size < total)
   {
      D3D12_HEAP_PROPERTIES hp;
      D3D12_RESOURCE_DESC bd;
      D3D11VD_RELEASE(rb12);
      memset(&hp, 0, sizeof(hp));
      memset(&bd, 0, sizeof(bd));
      hp.Type             = D3D12_HEAP_TYPE_READBACK;
      bd.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
      bd.Width            = total;
      bd.Height           = 1;
      bd.DepthOrArraySize = 1;
      bd.MipLevels        = 1;
      bd.SampleDesc.Count = 1;
      bd.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
      if (FAILED(d12->lpVtbl->CreateCommittedResource(d12, &hp,
            D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COPY_DEST, NULL,
            &test_iid_d12_resource, (void**)&rb12)))
      {
         rb12 = NULL;
         video_hw_frame_abandon(fr);
         return 0;
      }
      rb12_size = total;
   }
   a12->lpVtbl->Reset(a12);
   l12->lpVtbl->Reset(l12, a12, NULL);
   d12_transition(tex, D3D12_RESOURCE_STATE_COMMON,
         D3D12_RESOURCE_STATE_COPY_SOURCE);
   for (p = 0; p < 2; p++)
   {
      src.pResource        = tex;
      src.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
      src.SubresourceIndex = p;
      dst.pResource        = rb12;
      dst.Type             = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
      dst.PlacedFootprint  = fp[p];
      l12->lpVtbl->CopyTextureRegion(l12, &dst, 0, 0, 0, &src, NULL);
   }
   d12_transition(tex, D3D12_RESOURCE_STATE_COPY_SOURCE,
         D3D12_RESOURCE_STATE_COMMON);
   l12->lpVtbl->Close(l12);
   q12->lpVtbl->Wait(q12, wait, fr->wait_value);
   q12->lpVtbl->ExecuteCommandLists(q12, 1, (ID3D12CommandList**)&l12);
   q12->lpVtbl->Signal(q12, done, fr->done_value);
   q12->lpVtbl->Signal(q12, f12, ++f12_value);
   f12->lpVtbl->SetEventOnCompletion(f12, f12_value, ev12);
   if (WaitForSingleObject(ev12, 5000) != WAIT_OBJECT_0)
      return 0;
   if (FAILED(rb12->lpVtbl->Map(rb12, 0, NULL, (void**)&m)))
      return 0;
   if (f->n < MAX_FRAMES)
   {
      i        = f->n++;
      f->w     = w;
      f->h     = h;
      f->y[i]  = frame_alloc(f, (size_t)w * h);
      f->uv[i] = frame_alloc(f, (size_t)cw * 2 * ch);
      if (f->y[i] && f->uv[i])
      {
         for (y = 0; y < h; y++)
            memcpy(f->y[i] + (size_t)y * w,
                  m + fp[0].Offset + (size_t)y * fp[0].Footprint.RowPitch, w);
         for (y = 0; y < ch; y++)
            memcpy(f->uv[i] + (size_t)y * cw * 2,
                  m + fp[1].Offset + (size_t)y * fp[1].Footprint.RowPitch,
                  cw * 2);
      }
   }
   rb12->lpVtbl->Unmap(rb12, 0, NULL);
   return 1;
}

/* @mp4 decoded on the backend when @hw, else in software; 1 when it
 * decoded as asked */
static int decode(const unsigned char *mp4, size_t len, int hw,
      frames_t *f)
{
   static opened_t o;
   video_hw_frame_t fr;
   rmp4_packet pkt;
   rmp4_t *m;
   rh264_video *v;
   int i, r, trk = -1, on_hw = 0, ok = 1;

   memset(f, 0, sizeof(*f));
   if (!(m = rmp4_open_memory(mp4, len)))
      return 0;
   for (i = 0; i < rmp4_num_tracks(m); i++)
      if (rmp4_get_track(m, i)->type == RMP4_TRACK_VIDEO)
      { trk = i; break; }
   if (trk < 0 || !(v = rh264_video_open()))
   {
      rmp4_close(m);
      return 0;
   }
   rh264_video_set_extradata(v, rmp4_get_track(m, trk)->codec_private,
         rmp4_get_track(m, trk)->codec_private_size);
   if (hw == 2)
      rh264_video_set_hw_readback(v, 1);
   else if (hw)
      on_hw = rh264_video_set_hw(v, 1);
   if (hw == 1 && !on_hw)
      ok = 0;
   while (ok && rmp4_read_packet(m, &pkt) == 1)
   {
      if (pkt.track != trk)
         continue;
      {
         unsigned tries = 0;
         do
         {
            r = rh264_video_decode(v, pkt.data, pkt.size);
            if (r == 2)
               Sleep(1);
         } while (r == 2 && ++tries < 1000);
         if (r == 2)
            r = -1;
      }
      if (r < 0)
         ok = 0;
      else if (r == 1)
      {
         if (!on_hw || hw == 2)
            take_sw(f, v);
         else if (rh264_video_hw_frame(v, &fr) != 0
               || !(use_d3d12 ? take_hw12(f, &fr) : take_hw(f, &o, &fr)))
            ok = 0;
      }
   }
   while (ok)
   {
      r = rh264_video_drain(v);
      if (r == 2)
      {
         Sleep(1);
         continue;
      }
      if (r != 0)
         break;
      if (!on_hw || hw == 2)
         take_sw(f, v);
      else if (rh264_video_hw_frame(v, &fr) != 0
            || !(use_d3d12 ? take_hw12(f, &fr) : take_hw(f, &o, &fr)))
         ok = 0;
   }
   if (hw == 2 && !rh264_video_is_hw(v))
      ok = 0;
   rh264_video_close(v);
   rmp4_close(m);
   opened_clear(&o);
   opened12_clear(&o12);
   return ok;
}

/* Frames that differ: the first one's index and its worst sample */
static int compare(const frames_t *a, const frames_t *b, int *first,
      int *worst)
{
   int i, bad = 0;
   *first = -1;
   *worst = 0;
   if (a->n != b->n || a->w != b->w || a->h != b->h)
      return -1;
   for (i = 0; i < a->n; i++)
   {
      size_t ny = (size_t)a->w * a->h;
      size_t nc = (size_t)((a->w + 1) / 2) * 2 * ((a->h + 1) / 2);
      size_t k;
      int diff = 0;
      for (k = 0; k < ny; k++)
         if (abs(a->y[i][k] - b->y[i][k]) > diff)
            diff = abs(a->y[i][k] - b->y[i][k]);
      for (k = 0; k < nc; k++)
         if (abs(a->uv[i][k] - b->uv[i][k]) > diff)
            diff = abs(a->uv[i][k] - b->uv[i][k]);
      if (diff)
      {
         if (*first < 0)
         {
            *first = i;
            *worst = diff;
         }
         bad++;
      }
   }
   return bad;
}

typedef struct
{
   const char *name;
   const unsigned char *data;
   size_t len;
   int lists;   /* sends scaling matrices */
} stream_t;

#define STREAM(n, l) { #n, stream_##n, sizeof(stream_##n), l }

static const stream_t streams[] = {
   STREAM(baseline,    0),
   STREAM(main_bpyr,   0),
   STREAM(high_slices, 0),
   STREAM(high_jvt,    1),
   STREAM(high_cqm,    1),
   STREAM(cropped,     0)
};

static void test_hevc_main(int depth)
{
   static rh265_video_params_t params;
   static rh265_hw_session_t session;
   rh265_hw_ops_t ops;
   rh265_access_unit_t au;
   rh265_nal_t slices[4];
   rh265_video *v = rh265_video_open();
   video_hw_frame_t frame;
   frames_t hw;
   opened_t opened;
   int ret, stride, w, h, plane, matches = 1;
   unsigned attempt, x, y;
   const uint8_t *sw;
   const uint8_t *config = depth == 10 ? hevc_config10 : hevc_config8;
   const uint8_t *sample = depth == 10 ? hevc_sample10 : hevc_sample8;
   size_t config_size = depth == 10 ? sizeof(hevc_config10) : sizeof(hevc_config8);
   size_t sample_size = depth == 10 ? sizeof(hevc_sample10) : sizeof(hevc_sample8);
   printf("HEVC %d-bit:\n", depth);
   memset(&hw, 0, sizeof(hw));
   memset(&opened, 0, sizeof(opened));
   memset(&ops, 0, sizeof(ops));
   check("HEVC Main: parser opens", v != NULL);
   if (!v) return;
   if (rh265_video_set_extradata(v, config, config_size)
         || rh265_video_get_access_unit(v, sample, sample_size,
            4, slices, 4, &au)
         || rh265_video_get_params(v, au.first.pps_id, &params))
   {
      check("HEVC Main: fixture parses", 0);
      goto done;
   }
   if (!d3d11_video_decode_hevc_open(&params, 32, &ops))
   {
      printf("HEVC %d-bit: SKIP (decoder/shared output setup unavailable)\n", depth);
      goto done;
   }
   check("HEVC Main: owner initializes", rh265_hw_session_init(&session, &ops, 32));
   ret = 1;
   for (attempt = 0; ret == 1 && attempt < 2048; attempt++)
      ret = rh265_hw_session_submit(&session, sample, sample_size, &params, &au, slices);
   check("HEVC Main: hardware submission accepts", ret == 0);
   if (ret) goto done;
   check("HEVC Main: exports shared frame", !rh265_hw_session_output(&session, true, &frame));
   if (!frame.image) goto done;
   check("HEVC Main: shared handles and fences import", take_hw(&hw, &opened, &frame));
   ret = rh265_video_decode(v, sample, sample_size);
   if (ret == 0) ret = rh265_video_drain(v) == 0 ? 1 : -1;
   check("HEVC Main: software picture", ret == 1);
   if (ret != 1 || hw.n != 1) goto done;
   for (plane = 0; plane < 3; plane++)
   {
      sw = rh265_video_plane(v, plane, &stride, &w, &h);
      if (!sw || w != (int)(plane ? hw.w / 2 : hw.w)
            || h != (int)(plane ? hw.h / 2 : hw.h))
      { matches = 0; break; }
      for (y = 0; y < (unsigned)h; y++)
         for (x = 0; x < (unsigned)w; x++)
         {
            unsigned at = plane ? y * hw.w + 2 * x + plane - 1 : y * hw.w + x;
            const uint8_t *data = plane ? hw.uv[0] : hw.y[0];
            unsigned value = depth == 10 ? ((const uint16_t*)data)[at] >> 6 : data[at];
            unsigned expected = depth == 10 ? ((const uint16_t*)sw)[y * stride + x]
               : sw[y * stride + x];
            if (expected != value) matches = 0;
         }
   }
   check("HEVC Main: every imported pixel equals software", matches);
done:
   opened_clear(&opened);
   frames_free(&hw);
   d3d11_video_decode_hevc_close(&ops);
   rh265_video_close(v);
}

int main(void)
{
   static const D3D_FEATURE_LEVEL levels[2] = {
      D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
   frames_t sw, hw;
   unsigned i;
   int zigzag_ok = 0, raster_ok = 0, lists_seen = 0;

   setvbuf(stdout, NULL, _IONBF, 0);
   if (     FAILED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
               levels, 2, D3D11_SDK_VERSION, &dev, NULL, &ctx))
         || FAILED(dev->lpVtbl->QueryInterface(dev, &d3d11vd_iid_device5,
               (void**)&dev5))
         || FAILED(ctx->lpVtbl->QueryInterface(ctx, &d3d11vd_iid_context4,
               (void**)&ctx4)))
   {
      printf("d3d11_video_decode_test: SKIP (no D3D11.4 device)\n");
      return 0;
   }
   if (!d3d11_video_decode_init(dev))
   {
      printf("d3d11_video_decode_test: SKIP (the backend did not start)\n");
      return 0;
   }

   test_hevc_main(8);
   test_hevc_main(10);

   for (i = 0; i < sizeof(streams) / sizeof(streams[0]); i++)
   {
      const stream_t *s = &streams[i];
      char label[128];
      int bad, first, worst, on;

      printf("%s:\n", s->name);
      test_qm_raster = 0;
      decode(s->data, s->len, 0, &sw);
      on = decode(s->data, s->len, 1, &hw);
      if (!on && i == 0)
      {
         frames_free(&sw);
         frames_free(&hw);
         d3d11_video_decode_deinit();
         printf("d3d11_video_decode_test: SKIP (the GPU decodes no H.264 here)\n");
         return 0;
      }
      snprintf(label, sizeof(label), "%s: decodes on the GPU", s->name);
      check(label, on);
      bad = compare(&sw, &hw, &first, &worst);
      printf("      %d frames %ux%u in software, %d from the GPU\n",
            sw.n, sw.w, sw.h, hw.n);
      if (bad > 0)
         printf("      %d differ, the first frame %d by up to %d\n", bad,
               first, worst);
      snprintf(label, sizeof(label), "%s: every frame as in software",
            s->name);
      check(label, on && bad == 0 && sw.n > 0);
      frames_free(&hw);
      on = decode(s->data, s->len, 2, &hw);
      bad = compare(&sw, &hw, &first, &worst);
      snprintf(label, sizeof(label), "%s: CPU readback matches software", s->name);
      check(label, on && bad == 0 && sw.n > 0);
      if (s->lists)
      {
         lists_seen = 1;
         if (on && bad == 0)
            zigzag_ok++;
         frames_free(&hw);
         test_qm_raster = 1;
         if (decode(s->data, s->len, 1, &hw) && compare(&sw, &hw, &first,
               &worst) == 0)
            raster_ok++;
         test_qm_raster = 0;
      }
      frames_free(&sw);
      frames_free(&hw);
   }
   /* the D3D12 driver's reading of the same handles */
   if (d12_init())
   {
      printf("read through D3D12:\n");
      use_d3d12 = 1;
      for (i = 0; i < sizeof(streams) / sizeof(streams[0]); i++)
      {
         const stream_t *s = &streams[i];
         char label[128];
         int first, worst, on;
         decode(s->data, s->len, 0, &sw);
         on = decode(s->data, s->len, 1, &hw);
         snprintf(label, sizeof(label),
               "%s: through D3D12, every frame as in software", s->name);
         check(label, on && sw.n > 0
               && compare(&sw, &hw, &first, &worst) == 0);
         frames_free(&sw);
         frames_free(&hw);
      }
      use_d3d12 = 0;
   }
   else
      printf("no D3D12 device: the D3D12 reading is not run\n");
   d12_deinit();

   if (lists_seen)
      printf("scaling lists: the GPU took them in %s order\n",
            (zigzag_ok && !raster_ok) ? "zig-zag (as sent)"
            : (raster_ok && !zigzag_ok) ? "raster (NOT as sent)"
            : (zigzag_ok && raster_ok) ? "either" : "neither");

   d3d11_video_decode_deinit();
   check("every decoder closed", !retro_atomic_load_acquire_int(&d3d11vd.live));
   D3D11VD_RELEASE(ctx4);
   D3D11VD_RELEASE(dev5);
   D3D11VD_RELEASE(ctx);
   D3D11VD_RELEASE(dev);
   printf("d3d11_video_decode_test: %s (%d failure%s)\n",
         fails ? "FAIL" : "PASS", fails, fails == 1 ? "" : "s");
   return fails ? 1 : 0;
}
