/* The D3D11 video backend (gfx/common/d3d11_video_decode.c) against a
 * D3D11 that records what it is told, so that its logic runs where
 * there is no GPU video decoder (CI: mingw-w64 and Wine).
 *
 *   - the decoder: the profile and NV12, the short-slice configuration
 *     passed over a long-slice one, a DPB of the slots or the minimum
 *     the configuration asks, an output view per slot;
 *   - the shared textures at the visible size and their fences;
 *   - each picture of real streams through rh264: decoded into its own
 *     slot's view, the four buffers submitted with their sizes, the
 *     bitstream padded to 128 bytes with zeros the last slice takes, a
 *     slice entry for each NAL unit's start code;
 *   - DecoderBeginFrame's E_PENDING returned for a later retry;
 *   - a bitstream buffer too small refused, one with no room for the
 *     padding taking none;
 *   - each frame shown copied from its slot's visible window, the
 *     ready fence raised behind it, and a texture copied into again
 *     only once read or abandoned - waited for, and given up on;
 *   - a decoder closed with a frame unread kept until it is read, and
 *     everything released by deinit.
 *
 * The mock methods ignore what they are not asked about, which the x64
 * calling convention allows; x86-64 only. */
#ifndef CINTERFACE
#define CINTERFACE
#endif
#ifndef COBJMACROS
#define COBJMACROS
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "../../../gfx/common/d3d11_video_decode.c"
#include <mmsystem.h>

#include <formats/rmp4.h>
#include "frontend_stubs.h"
#include "d3d11_video_decode_test_data.h"
#include "dxva_hevc_access_unit_data.h"

static int fails;

static void check(const char *what, int ok)
{
   printf("  %-64s %s\n", what, ok ? "ok" : "FAIL");
   if (!ok)
      fails++;
}

/* ---- the mock objects ---- */

#define VT_SLOTS 256
#define SLOT(vt, m) (offsetof(vt, m) / sizeof(void*))

enum kind
{
   K_DEVICE = 0, K_CONTEXT, K_CONTEXT4, K_DEVICE5, K_VDEVICE, K_VCONTEXT,
   K_TEXTURE, K_RESOURCE1, K_FENCE, K_DXGI, K_ADAPTER, K_PLAIN, K_COUNT
};

typedef struct mock
{
   void **lpVtbl;
   int kind;
   long refs;
   D3D11_TEXTURE2D_DESC desc;   /* a texture's                         */
   unsigned slice;              /* an output view's array slice        */
   uint64_t completed;          /* a fence's value                     */
   uint64_t on_value;           /* SetEventOnCompletion's              */
   HANDLE on_event;
   HANDLE shared;               /* the handle made for it              */
} mock_t;

static void *vt[K_COUNT][VT_SLOTS];
static int live;                 /* mock objects not released           */

static mock_t *mock_new(int kind)
{
   mock_t *m = (mock_t*)calloc(1, sizeof(*m));
   m->lpVtbl = vt[kind];
   m->kind   = kind;
   m->refs   = 1;
   live++;
   return m;
}

/* the textures and fences made, to find them by handle */
#define MAX_MADE 256
static mock_t *made[MAX_MADE];
static int n_made;

static mock_t *by_handle(uint64_t h)
{
   int i;
   for (i = 0; i < n_made; i++)
      if (made[i] && (uint64_t)(uintptr_t)made[i]->shared == h)
         return made[i];
   return NULL;
}

static ULONG STDMETHODCALLTYPE m_addref(mock_t *m)
{
   return (ULONG)++m->refs;
}

static ULONG STDMETHODCALLTYPE m_release(mock_t *m)
{
   int i;
   if (--m->refs)
      return (ULONG)m->refs;
   for (i = 0; i < n_made; i++)
      if (made[i] == m)
         made[i] = NULL;
   live--;
   free(m);
   return 0;
}

static HRESULT STDMETHODCALLTYPE m_ok(void)
{
   return S_OK;
}

static int iid_is(REFIID a, const GUID *b)
{
   return !memcmp(a, b, sizeof(GUID));
}

static HRESULT STDMETHODCALLTYPE m_qi(mock_t *m, REFIID riid, void **out)
{
   int k = -1;
   *out = NULL;
   if (m->kind == K_DEVICE && iid_is(riid, &d3d11vd_iid_device5))
      k = K_DEVICE5;
   else if (m->kind == K_DEVICE && iid_is(riid, &d3d11vd_iid_video_device))
      k = K_VDEVICE;
   else if (m->kind == K_DEVICE && iid_is(riid, &d3d11vd_iid_dxgi_device))
      k = K_DXGI;
   else if (m->kind == K_CONTEXT && iid_is(riid, &d3d11vd_iid_context4))
      k = K_CONTEXT4;
   else if (m->kind == K_CONTEXT && iid_is(riid, &d3d11vd_iid_video_context))
      k = K_VCONTEXT;
   else if (m->kind == K_TEXTURE && iid_is(riid, &d3d11vd_iid_dxgi_resource1))
   {
      /* the texture itself, as its handle is made on it */
      m->refs++;
      *out = m;
      return S_OK;
   }
   if (k < 0)
      return E_NOINTERFACE;
   *out = mock_new(k);
   return S_OK;
}

static HRESULT STDMETHODCALLTYPE m_get_adapter(mock_t *m, void **out)
{
   *out = mock_new(K_ADAPTER);
   return S_OK;
}

/* ---- device ---- */

static int dpb_array, n_out_tex, out_flags_ok, reject_shared_dpb;

static void STDMETHODCALLTYPE m_texture_desc(mock_t *m, D3D11_TEXTURE2D_DESC *desc)
{
   *desc = m->desc;
}

static HRESULT STDMETHODCALLTYPE m_create_texture(mock_t *m,
      const D3D11_TEXTURE2D_DESC *d, const void *init, void **out)
{
   mock_t *t;
   if (reject_shared_dpb && (d->BindFlags & D3D11_BIND_DECODER)
         && (d->MiscFlags & D3D11_RESOURCE_MISC_SHARED))
   { *out = NULL; return E_FAIL; }
   t = mock_new(K_TEXTURE);
   t->desc   = *d;
   if (d->BindFlags & D3D11_BIND_DECODER)
      dpb_array = (int)d->ArraySize;
   else if (n_made < MAX_MADE)
   {
      n_out_tex++;
      if (     d->Format == DXGI_FORMAT_NV12
            && d->MiscFlags == (D3D11_RESOURCE_MISC_SHARED
               | D3D11_RESOURCE_MISC_SHARED_NTHANDLE)
            && (d->BindFlags & D3D11_BIND_SHADER_RESOURCE))
         out_flags_ok++;
      made[n_made++] = t;
   }
   *out = t;
   return S_OK;
}

static HRESULT STDMETHODCALLTYPE m_feature(mock_t *m, D3D11_FEATURE f,
      void *data, UINT size)
{
   if (f == D3D11_FEATURE_D3D11_OPTIONS4)
      ((D3D11_FEATURE_DATA_D3D11_OPTIONS4*)data)
         ->ExtendedNV12SharedTextureSupported = TRUE;
   return S_OK;
}

static int n_fences;

static HRESULT STDMETHODCALLTYPE m_create_fence(mock_t *m, UINT64 init,
      D3D11_FENCE_FLAG flags, REFIID riid, void **out)
{
   mock_t *f    = mock_new(K_FENCE);
   f->completed = init;
   if (n_made < MAX_MADE)
      made[n_made++] = f;
   n_fences++;
   *out = f;
   return S_OK;
}

static HRESULT STDMETHODCALLTYPE m_shared_handle(mock_t *m,
      const SECURITY_ATTRIBUTES *a, DWORD access, LPCWSTR name, HANDLE *h)
{
   /* a real handle, for the backend to close */
   *h = m->shared = CreateEventA(NULL, FALSE, FALSE, NULL);
   return S_OK;
}

static UINT64 STDMETHODCALLTYPE m_completed(mock_t *m)
{
   return m->completed;
}

static HRESULT STDMETHODCALLTYPE m_on_completion(mock_t *m, UINT64 v,
      HANDLE e)
{
   m->on_value = v;
   m->on_event = e;
   if (m->completed >= v)
      SetEvent(e);
   return S_OK;
}

static void fence_reach(mock_t *f, uint64_t v)
{
   if (f->completed < v)
      f->completed = v;
   if (f->on_event && f->completed >= f->on_value)
      SetEvent(f->on_event);
}

/* ---- context ---- */

static int copies, copy_src_sub, copy_box_ok;
static mock_t *copy_dst;
static unsigned want_crop_x, want_crop_y, want_w, want_h;
static uint64_t ready_signalled;
static HRESULT signal_hr = S_OK;

static void STDMETHODCALLTYPE m_copy_region(mock_t *m, mock_t *dst,
      UINT dst_sub, UINT x, UINT y, UINT z, mock_t *src, UINT src_sub,
      const D3D11_BOX *box)
{
   copies++;
   copy_dst     = dst;
   copy_src_sub = (int)src_sub;
   copy_box_ok  = box && !dst_sub && !x && !y && !z
      && (src->desc.BindFlags & D3D11_BIND_DECODER)
      && box->left == want_crop_x && box->top == want_crop_y
      && box->right == want_crop_x + want_w
      && box->bottom == want_crop_y + want_h
      && box->front == 0 && box->back == 1;
}

/* the decoder's GPU: done as soon as asked */
static HRESULT STDMETHODCALLTYPE m_signal(mock_t *m, mock_t *fence,
      UINT64 v)
{
   if (FAILED(signal_hr))
      return signal_hr;
   ready_signalled = v;
   fence_reach(fence, v);
   return S_OK;
}

/* ---- video device ---- */

static D3D11_VIDEO_DECODER_DESC dec_desc;
static UINT dec_raw;
static int min_targets = 20;

static HRESULT STDMETHODCALLTYPE m_check_format(mock_t *m, const GUID *g,
      DXGI_FORMAT f, BOOL *ok)
{
   *ok = (!memcmp(g, &d3d11vd_h264_vld_nofgt, sizeof(*g))
      || !memcmp(g, &d3d11vd_hevc_main, sizeof(*g))
      || !memcmp(g, &d3d11vd_vp9_profile0, sizeof(*g)))
      && f == DXGI_FORMAT_NV12;
   if (!memcmp(g, &d3d11vd_hevc_main10, sizeof(*g)) && f == DXGI_FORMAT_P010)
      *ok = TRUE;
   if (!memcmp(g, &d3d11vd_vp9_profile2, sizeof(*g)) && f == DXGI_FORMAT_P010)
      *ok = TRUE;
   return S_OK;
}

static int config_retry, decoder_attempts;

static HRESULT STDMETHODCALLTYPE m_config_count(mock_t *m,
      const D3D11_VIDEO_DECODER_DESC *d, UINT *n)
{
   *n = 2;
   return S_OK;
}

static HRESULT STDMETHODCALLTYPE m_config(mock_t *m,
      const D3D11_VIDEO_DECODER_DESC *d, UINT i,
      D3D11_VIDEO_DECODER_CONFIG *c)
{
   memset(c, 0, sizeof(*c));
   c->ConfigBitstreamRaw             = config_retry ?
      (USHORT)(d->OutputFormat == DXGI_FORMAT_P010
         || memcmp(&d->Guid, &d3d11vd_h264_vld_nofgt, sizeof(GUID)) ? 1 : 2)
      : (i ? 2 : 1);
   c->ConfigMinRenderTargetBuffCount = (USHORT)min_targets;
   return S_OK;
}

static HRESULT STDMETHODCALLTYPE m_create_decoder(mock_t *m,
      const D3D11_VIDEO_DECODER_DESC *d,
      const D3D11_VIDEO_DECODER_CONFIG *c, void **out)
{
   decoder_attempts++;
   if (config_retry && (decoder_attempts == 1 || config_retry == 2))
   {
      *out = NULL;
      return E_FAIL;
   }
   dec_desc = *d;
   dec_raw  = c->ConfigBitstreamRaw;
   *out     = mock_new(K_PLAIN);
   return S_OK;
}

static int views, views_ok;

static HRESULT STDMETHODCALLTYPE m_create_view(mock_t *m, mock_t *res,
      const D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC *d, void **out)
{
   mock_t *v = mock_new(K_PLAIN);
   v->slice  = d->Texture2D.ArraySlice;
   if (     d->ViewDimension == D3D11_VDOV_DIMENSION_TEXTURE2D
         && v->slice == (unsigned)views
         && !memcmp(&d->DecodeProfile, &dec_desc.Guid,
               sizeof(GUID)))
      views_ok++;
   views++;
   *out = v;
   return S_OK;
}

/* ---- video context ---- */

#define BUF_TYPES 8
static uint8_t bufs[BUF_TYPES][1 << 17];
static UINT buf_cap[BUF_TYPES];
static int pending_left, begins, ends;
static HRESULT begin_hr = S_OK;
static int begin_slice;

typedef struct
{
   int slice;                   /* the view decoded into */
   int nbufs;
   D3D11_VIDEO_DECODER_BUFFER_DESC desc[4];
   uint8_t pp[DXVA_H264_PP_SIZE];
   uint8_t sc[64 * DXVA_H264_SLICE_SIZE];
   uint8_t *bs;
} submit_t;

#define MAX_SUBMITS 64
static submit_t subs[MAX_SUBMITS];
static int n_subs;
static HRESULT release_hr = S_OK, submit_hr = S_OK, end_hr = S_OK;
static int buffer_gets, releases;

static HRESULT STDMETHODCALLTYPE m_release_buffer(mock_t *m, void *dec,
      D3D11_VIDEO_DECODER_BUFFER_TYPE t)
{
   releases++;
   return release_hr;
}

static HRESULT STDMETHODCALLTYPE m_get_buffer(mock_t *m, void *dec,
      D3D11_VIDEO_DECODER_BUFFER_TYPE t, UINT *size, void **p)
{
   buffer_gets++;
   *size = buf_cap[t];
   *p    = bufs[t];
   return S_OK;
}

static HRESULT STDMETHODCALLTYPE m_begin(mock_t *m, void *dec, mock_t *view,
      UINT n, const void *key)
{
   if (FAILED(begin_hr))
      return begin_hr;
   if (pending_left > 0)
   {
      pending_left--;
      return E_PENDING;
   }
   begins++;
   begin_slice = (int)view->slice;
   return S_OK;
}

static HRESULT STDMETHODCALLTYPE m_end(void)
{
   ends++;
   return end_hr;
}

static HRESULT STDMETHODCALLTYPE m_submit(mock_t *m, void *dec, UINT n,
      const D3D11_VIDEO_DECODER_BUFFER_DESC *d)
{
   submit_t *s;
   UINT i;
   if (FAILED(submit_hr))
      return submit_hr;
   if (n_subs >= MAX_SUBMITS || n > 4)
      return S_OK;
   s        = &subs[n_subs++];
   s->slice = begin_slice;
   s->nbufs = (int)n;
   for (i = 0; i < n; i++)
   {
      s->desc[i] = d[i];
      if (d[i].BufferType == D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS)
         memcpy(s->pp, bufs[d[i].BufferType], DXVA_H264_PP_SIZE);
      else if (d[i].BufferType == D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL
            && d[i].DataSize <= sizeof(s->sc))
         memcpy(s->sc, bufs[d[i].BufferType], d[i].DataSize);
      else if (d[i].BufferType == D3D11_VIDEO_DECODER_BUFFER_BITSTREAM)
      {
         s->bs = (uint8_t*)malloc(d[i].DataSize);
         memcpy(s->bs, bufs[d[i].BufferType], d[i].DataSize);
      }
   }
   return S_OK;
}

static void subs_free(void)
{
   int i;
   for (i = 0; i < n_subs; i++)
      free(subs[i].bs);
   memset(subs, 0, sizeof(subs));
   n_subs = 0;
}

/* ---- device creation ---- */

static mock_t *the_adapter;
static int creates, create_ok;

static HRESULT WINAPI mock_create(IDXGIAdapter *adapter, D3D_DRIVER_TYPE type,
      HMODULE sw, UINT flags, const D3D_FEATURE_LEVEL *levels, UINT n,
      UINT sdk, ID3D11Device **dev, D3D_FEATURE_LEVEL *fl,
      ID3D11DeviceContext **ctx)
{
   creates++;
   if (     (mock_t*)adapter == the_adapter && type == D3D_DRIVER_TYPE_UNKNOWN
         && (flags & D3D11_CREATE_DEVICE_VIDEO_SUPPORT))
      create_ok++;
   *dev = (ID3D11Device*)mock_new(K_DEVICE);
   *ctx = (ID3D11DeviceContext*)mock_new(K_CONTEXT);
   return S_OK;
}

static void vt_fill(int k, size_t n)
{
   size_t i;
   for (i = 0; i < VT_SLOTS; i++)
      vt[k][i] = (void*)m_ok;
   vt[k][0] = (void*)m_qi;
   vt[k][1] = (void*)m_addref;
   vt[k][2] = (void*)m_release;
   (void)n;
}

static void mocks_init(void)
{
   int k;
   for (k = 0; k < K_COUNT; k++)
      vt_fill(k, VT_SLOTS);
   vt[K_DEVICE][SLOT(ID3D11DeviceVtbl, CreateTexture2D)]     = (void*)m_create_texture;
   vt[K_TEXTURE][SLOT(ID3D11Texture2DVtbl, GetDesc)]         = (void*)m_texture_desc;
   vt[K_DEVICE][SLOT(ID3D11DeviceVtbl, CheckFeatureSupport)] = (void*)m_feature;
   vt[K_DEVICE5][SLOT(ID3D11Device5Vtbl, CreateFence)]       = (void*)m_create_fence;
   vt[K_DXGI][SLOT(IDXGIDeviceVtbl, GetAdapter)]             = (void*)m_get_adapter;
   vt[K_FENCE][SLOT(ID3D11FenceVtbl, CreateSharedHandle)]    = (void*)m_shared_handle;
   vt[K_FENCE][SLOT(ID3D11FenceVtbl, GetCompletedValue)]     = (void*)m_completed;
   vt[K_FENCE][SLOT(ID3D11FenceVtbl, SetEventOnCompletion)]  = (void*)m_on_completion;
   vt[K_TEXTURE][SLOT(IDXGIResource1Vtbl, CreateSharedHandle)] = (void*)m_shared_handle;
   vt[K_CONTEXT][SLOT(ID3D11DeviceContextVtbl, CopySubresourceRegion)] = (void*)m_copy_region;
   vt[K_CONTEXT4][SLOT(ID3D11DeviceContext4Vtbl, Signal)]    = (void*)m_signal;
   vt[K_VDEVICE][SLOT(ID3D11VideoDeviceVtbl, CheckVideoDecoderFormat)]    = (void*)m_check_format;
   vt[K_VDEVICE][SLOT(ID3D11VideoDeviceVtbl, GetVideoDecoderConfigCount)] = (void*)m_config_count;
   vt[K_VDEVICE][SLOT(ID3D11VideoDeviceVtbl, GetVideoDecoderConfig)]      = (void*)m_config;
   vt[K_VDEVICE][SLOT(ID3D11VideoDeviceVtbl, CreateVideoDecoder)]         = (void*)m_create_decoder;
   vt[K_VDEVICE][SLOT(ID3D11VideoDeviceVtbl, CreateVideoDecoderOutputView)] = (void*)m_create_view;
   vt[K_VCONTEXT][SLOT(ID3D11VideoContextVtbl, ReleaseDecoderBuffer)] = (void*)m_release_buffer;
   vt[K_VCONTEXT][SLOT(ID3D11VideoContextVtbl, GetDecoderBuffer)]     = (void*)m_get_buffer;
   vt[K_VCONTEXT][SLOT(ID3D11VideoContextVtbl, DecoderBeginFrame)]    = (void*)m_begin;
   vt[K_VCONTEXT][SLOT(ID3D11VideoContextVtbl, DecoderEndFrame)]      = (void*)m_end;
   vt[K_VCONTEXT][SLOT(ID3D11VideoContextVtbl, SubmitDecoderBuffers)] = (void*)m_submit;
   for (k = 0; k < BUF_TYPES; k++)
      buf_cap[k] = sizeof(bufs[k]);
}

/* ---- checks of what was submitted ---- */

static const D3D11_VIDEO_DECODER_BUFFER_DESC *find_desc(const submit_t *s,
      D3D11_VIDEO_DECODER_BUFFER_TYPE t)
{
   int i;
   for (i = 0; i < s->nbufs; i++)
      if (s->desc[i].BufferType == t)
         return &s->desc[i];
   return NULL;
}

static uint32_t get32(const uint8_t *p)
{
   return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16)
      | ((uint32_t)p[3] << 24);
}

/* Each submission whole: the four buffers, the bitstream's slices */
static void check_submits(const char *name, int want_slices, unsigned mbs)
{
   char label[128];
   int i, bufs_ok = 1, pad_ok = 1, slices_ok = 1, view_ok = 1, nsl = 0;
   for (i = 0; i < n_subs; i++)
   {
      const submit_t *s = &subs[i];
      const D3D11_VIDEO_DECODER_BUFFER_DESC *pp = find_desc(s,
            D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS);
      const D3D11_VIDEO_DECODER_BUFFER_DESC *qm = find_desc(s,
            D3D11_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX);
      const D3D11_VIDEO_DECODER_BUFFER_DESC *sc = find_desc(s,
            D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL);
      const D3D11_VIDEO_DECODER_BUFFER_DESC *bs = find_desc(s,
            D3D11_VIDEO_DECODER_BUFFER_BITSTREAM);
      uint32_t k, n, sum = 0;
      if (     s->nbufs != 4 || !pp || !qm || !sc || !bs
            || pp->DataSize != DXVA_H264_PP_SIZE
            || qm->DataSize != DXVA_H264_QM_SIZE
            || sc->DataSize % DXVA_H264_SLICE_SIZE
            || sc->NumMBsInBuffer != mbs || bs->NumMBsInBuffer != mbs)
      {
         bufs_ok = 0;
         continue;
      }
      if (s->pp[DXVA_H264_PP_CURR_PIC] != (uint8_t)s->slice)
         view_ok = 0;
      if (bs->DataSize % 128)
         pad_ok = 0;
      n = sc->DataSize / DXVA_H264_SLICE_SIZE;
      if (i == 0)
         nsl = (int)n;
      for (k = 0; k < n; k++)
      {
         uint32_t at  = get32(s->sc + k * DXVA_H264_SLICE_SIZE);
         uint32_t len = get32(s->sc + k * DXVA_H264_SLICE_SIZE + 4);
         if (     at != sum || at + 4 > bs->DataSize
               || s->bs[at] || s->bs[at + 1] || s->bs[at + 2] != 1)
            slices_ok = 0;
         sum += len;
      }
      if (sum != bs->DataSize)
         slices_ok = 0;
      /* the padding is zeros behind the last slice's data */
      if (bs->DataSize >= 4 && s->bs[bs->DataSize - 1])
         pad_ok = 0;
   }
   snprintf(label, sizeof(label), "%s: %d pictures, four buffers each, sized",
         name, n_subs);
   check(label, n_subs > 0 && bufs_ok);
   snprintf(label, sizeof(label), "%s: each into its own slot's view", name);
   check(label, view_ok);
   snprintf(label, sizeof(label),
         "%s: bitstream to 128 bytes, the last slice's zeros", name);
   check(label, pad_ok);
   snprintf(label, sizeof(label),
         "%s: a slice entry per start code, end to end (%d)", name, nsl);
   check(label, slices_ok && nsl == want_slices);
}

/* ---- a stream through rh264, its frames read as the driver does ---- */

static int frames_read, frames_bad;

static void read_frame(const video_hw_frame_t *fr)
{
   mock_t *tex  = by_handle(fr->image);
   mock_t *done = by_handle(fr->done);
   mock_t *wait = by_handle(fr->wait);
   if (     !tex || !done || !wait || tex->kind != K_TEXTURE
         || done->kind != K_FENCE || wait->kind != K_FENCE
         || tex != copy_dst || wait->completed < fr->wait_value
         || fr->wait_value != ready_signalled
         || fr->width != want_w || fr->height != want_h
         || tex->desc.Width != want_w || tex->desc.Height != want_h
         || !copy_box_ok)
      frames_bad++;
   frames_read++;
   /* the driver's GPU, done reading */
   if (done)
      fence_reach(done, fr->done_value);
}

static int stream(const unsigned char *mp4, size_t len, int *shown)
{
   rmp4_packet pkt;
   video_hw_frame_t fr;
   rmp4_t *m;
   rh264_video *v;
   int i, r, trk = -1, hw;

   *shown = 0;
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
   hw = rh264_video_set_hw(v, 1);
   while (hw && rmp4_read_packet(m, &pkt) == 1)
   {
      if (pkt.track != trk)
         continue;
      if ((r = rh264_video_decode(v, pkt.data, pkt.size)) < 0)
         break;
      if (r == 1 && rh264_video_hw_frame(v, &fr) == 0)
      {
         read_frame(&fr);
         (*shown)++;
      }
   }
   while (hw && rh264_video_drain(v) == 0)
      if (rh264_video_hw_frame(v, &fr) == 0)
      {
         read_frame(&fr);
         (*shown)++;
      }
   rh264_video_close(v);
   rmp4_close(m);
   return hw;
}

static void test_stream(const char *name, const unsigned char *mp4,
      size_t len, unsigned w, unsigned h, unsigned cx, unsigned cy,
      unsigned mbs, int want_slices, int want_frames)
{
   char label[128];
   int shown, live_before = live;

   printf("%s:\n", name);
   subs_free();
   frames_read = frames_bad = 0;
   n_out_tex = out_flags_ok = n_fences = views = views_ok = 0;
   want_w = w; want_h = h; want_crop_x = cx; want_crop_y = cy;
   begins = ends = 0;
   snprintf(label, sizeof(label), "%s: decodes on the backend", name);
   check(label, stream(mp4, len, &shown));
   snprintf(label, sizeof(label),
         "%s: H.264 NV12 %ux%u, short slices, DPB of %d", name,
         dec_desc.SampleWidth, dec_desc.SampleHeight, dpb_array);
   check(label, !memcmp(&dec_desc.Guid, &d3d11vd_h264_vld_nofgt,
            sizeof(GUID)) && dec_desc.OutputFormat == DXGI_FORMAT_NV12
         && dec_desc.SampleWidth == (mbs ? (unsigned)((w + 15) & ~15) : 0)
         && dec_raw == 2 && dpb_array >= min_targets
         && dpb_array >= views);
   snprintf(label, sizeof(label), "%s: an output view per slot (%d)", name,
         views);
   check(label, views > 1 && views_ok == views);
   snprintf(label, sizeof(label),
         "%s: four shared NV12 textures, five fences", name);
   check(label, n_out_tex == 4 && out_flags_ok == 4 && n_fences == 5);
   check_submits(name, want_slices, mbs);
   snprintf(label, sizeof(label), "%s: frames each begun and ended", name);
   check(label, begins == n_subs && ends == n_subs);
   snprintf(label, sizeof(label),
         "%s: %d frames shown, each its own copy, waited for", name, shown);
   check(label, shown == want_frames && frames_read == shown && !frames_bad);
   snprintf(label, sizeof(label), "%s: everything released at close", name);
   check(label, live == live_before);
}

/* ---- the backend driven directly ---- */

static rh264_hw_sps_t sps;
static rh264_hw_pps_t pps;
static uint8_t data[300];
static uint32_t offs[2] = { 0, 100 };
static rh264_hw_pic_t pic;

static void make_pic(int slot)
{
   memset(&sps, 0, sizeof(sps));
   memset(&pps, 0, sizeof(pps));
   memset(&pic, 0, sizeof(pic));
   memset(data, 0x55, sizeof(data));
   data[0] = data[1] = 0; data[2] = 1; data[3] = 0x65;
   data[100] = data[101] = 0; data[102] = 1; data[103] = 0x65;
   sps.profile_idc       = 100;
   sps.level_idc         = 30;
   sps.chroma_format_idc = 1;
   sps.bit_depth         = 8;
   sps.log2_max_frame_num = 4;
   sps.log2_max_poc_lsb  = 4;
   sps.max_num_ref_frames = 1;
   sps.width_in_mbs      = 4;
   sps.height_in_mbs     = 3;
   sps.width             = 64;
   sps.height            = 48;
   pps.num_ref_idx_l0_default = 1;
   pps.num_ref_idx_l1_default = 1;
   pps.pic_init_qp       = 26;
   pps.pic_init_qs       = 26;
   pic.sps           = &sps;
   pic.pps           = &pps;
   pic.data          = data;
   pic.size          = sizeof(data);
   pic.slice_offsets = offs;
   pic.num_slices    = 2;
   pic.slot          = slot;
   pic.idr           = 1;
   pic.intra         = 1;
   pic.reference     = 1;
}

static void test_config_retry(void)
{
   rh265_video *v = rh265_video_open();
   rh265_video_params_t params;
   rh265_hw_ops_t ops;
   void *dec;
   int before = live, parsed;
   make_pic(0);
   config_retry = 1;
   decoder_attempts = 0;
   dec = d3d11vd.ops.open(NULL, &sps, 4);
   check("H.264: second compatible configuration succeeds",
         dec != NULL && decoder_attempts == 2);
   d3d11vd.ops.close(dec);
   config_retry = 2;
   decoder_attempts = 0;
   dec = d3d11vd.ops.open(NULL, &sps, 4);
   check("H.264: exhausted configurations release setup",
         !dec && decoder_attempts == 2 && live == before);
   d3d11vd.ops.close(dec);
   parsed = v
      && !rh265_video_set_extradata(v, hevc_config8, sizeof(hevc_config8))
      && !rh265_video_get_params(v, 0, &params);
   check("HEVC retry: fixture parses", parsed);
   if (parsed)
   {
      config_retry = 1;
      decoder_attempts = 0;
      check("HEVC: second compatible configuration succeeds",
            d3d11_video_decode_hevc_open(&params, 4, &ops)
            && decoder_attempts == 2);
      d3d11_video_decode_hevc_close(&ops);
      config_retry = 2;
      decoder_attempts = 0;
      check("HEVC: exhausted configurations release setup",
            !d3d11_video_decode_hevc_open(&params, 4, &ops)
            && decoder_attempts == 2 && live == before && !ops.decoder);
      d3d11_video_decode_hevc_close(&ops);
   }
   rh265_video_close(v);
   config_retry = 0;
}

static void test_direct(void)
{
   video_hw_frame_t fr[6];
   const rh264_hw_ops_t *ops = &d3d11vd.ops;
   void *dec;
   int r, i, per_dec, live_before = live;
   DWORD t0;
   const D3D11_VIDEO_DECODER_BUFFER_DESC *bs;

   printf("driven directly:\n");
   make_pic(0);
   want_w = 64; want_h = 48; want_crop_x = want_crop_y = 0;
   check("an odd picture size is not opened",
         (sps.width = 63, !ops->open(NULL, &sps, 4)));
   sps.width = 64;
   min_targets = 2;
   dec = ops->open(NULL, &sps, 4);
   check("opened for 4 slots", dec != NULL);
   check("a DPB of the slots when the configuration asks fewer",
         dpb_array == 4);
   if (!dec)
      return;
   per_dec = live - live_before;

   subs_free();
   pending_left = 3;
   for (i = 0; i < 3; i++)
      check("E_PENDING: no submission", ops->decode(dec, &pic) == 1 && !n_subs);
   r = ops->decode(dec, &pic);
   check("E_PENDING three times, then decoded", !r && n_subs == 1
         && !pending_left);
   bs = n_subs ? find_desc(&subs[0], D3D11_VIDEO_DECODER_BUFFER_BITSTREAM)
      : NULL;
   check("300 bytes padded to 384",
         bs && bs->DataSize == 384 && !subs[0].bs[383]
         && subs[0].bs[299] == 0x55
         && get32(subs[0].sc + 14) == 284);
   pending_left = 100000;
   t0 = GetTickCount();
   stub_quiet = 1;
   r = ops->decode(dec, &pic);
   stub_quiet = 0;
   check("E_PENDING for good: retryable at once", r == 1
         && pending_left == 99999 && GetTickCount() - t0 < 100);
   pending_left = 0;

   subs_free();
   buf_cap[D3D11_VIDEO_DECODER_BUFFER_BITSTREAM] = 299;
   check("a bitstream buffer too small: refused", ops->decode(dec, &pic) < 0
         && !n_subs);
   buf_cap[D3D11_VIDEO_DECODER_BUFFER_BITSTREAM] = 320;
   r  = ops->decode(dec, &pic);
   bs = n_subs ? find_desc(&subs[0], D3D11_VIDEO_DECODER_BUFFER_BITSTREAM)
      : NULL;
   check("no room for the padding: none taken",
         !r && bs && bs->DataSize == 300
         && get32(subs[0].sc + 14) == 200);
   buf_cap[D3D11_VIDEO_DECODER_BUFFER_BITSTREAM] = sizeof(bufs[0]);
   check("a slot out of range: refused",
         (pic.slot = 4, ops->decode(dec, &pic) < 0));
   pic.slot = 0;

   /* four textures handed out, none read */
   for (i = 0; i < 4; i++)
      if (ops->output(dec, (unsigned)i, &fr[i]) != 0)
         break;
   check("four frames out, from their slots", i == 4 && copy_src_sub == 3);
   t0 = GetTickCount();
   stub_quiet = 1;
   r = ops->output(dec, 0, &fr[4]);
   stub_quiet = 0;
   check("a fifth with none read: retryable at once",
         r == 1 && GetTickCount() - t0 < 100);
   video_hw_frame_abandon(&fr[1]);
   t0 = GetTickCount();
   r  = ops->output(dec, 2, &fr[4]);
   check("the next texture abandoned: copied into at once",
         !r && fr[4].image == fr[1].image && GetTickCount() - t0 < 500
         && fr[4].done_value == fr[1].done_value + 1);
   fence_reach(by_handle(fr[2].done), fr[2].done_value);
   r = ops->output(dec, 1, &fr[5]);
   check("the next read: copied into", !r && fr[5].image == fr[2].image);

   /* closed with frames unread: kept until they are */
   ops->close(dec);
   check("closed with frames unread: kept", live > live_before);
   fence_reach(by_handle(fr[3].done), fr[3].done_value);
   fence_reach(by_handle(fr[4].done), fr[4].done_value);
   fence_reach(by_handle(fr[5].done), fr[5].done_value);
   fence_reach(by_handle(fr[0].done), fr[0].done_value);
   dec = ops->open(NULL, &sps, 4);
   check("read since: freed by the next open", dec != NULL
         && live == live_before + per_dec);
   if (dec)
      ops->close(dec);
   check("everything released", live == live_before);
   min_targets = 20;
}

static void test_device_loss(void)
{
   const rh264_hw_ops_t *ops = &d3d11vd.ops;
   video_hw_frame_t fr[4], untouched;
   void *dec;
   int i, before = live, retained;
   make_pic(0);
   want_w = 64; want_h = 48; want_crop_x = want_crop_y = 0;
   dec = ops->open(NULL, &sps, 4);
   check("loss: decoder opens", dec != NULL);
   if (!dec)
      return;
   memset(&untouched, 0x5a, sizeof(untouched));
   signal_hr = DXGI_ERROR_DEVICE_REMOVED;
   check("loss: a failed signal takes no lease", ops->output(dec, 0, &untouched) < 0
         && untouched.image == UINT64_C(0x5a5a5a5a5a5a5a5a));
   signal_hr = S_OK;
   for (i = 0; i < 4; i++)
      check("loss: outstanding lease", ops->output(dec, 0, &fr[i]) == 0);
   subs_free();
   begin_hr = DXGI_ERROR_DEVICE_REMOVED;
   check("loss: decode is fatal without submission", ops->decode(dec, &pic) < 0 && n_subs == 0);
   begin_hr = S_OK;
   for (i = 0; i < 4; i++)
      by_handle(fr[i].done)->completed = UINT64_MAX;
   memset(&untouched, 0x5a, sizeof(untouched));
   check("loss: removed fence is fatal without a lease", ops->output(dec, 0, &untouched) < 0
         && untouched.image == UINT64_C(0x5a5a5a5a5a5a5a5a));
   ops->close(dec);
   retained = live > before;
   check("loss: held callbacks keep their owner", retained);
   if (retained)
      for (i = 0; i < 4; i++)
         video_hw_frame_abandon(&fr[i]);
   d3d11vd_grave_reap(false);
   check("loss: all resources retire", live == before);
}

static void test_hevc(void)
{
   static rh265_video_params_t params;
   static rh265_hw_session_t session;
   rh265_hw_ops_t ops;
   rh265_access_unit_t au;
   rh265_nal_t slices[4];
   rh265_video *v = rh265_video_open();
   video_hw_frame_t frame;
   d3d11vd_dec_t *d;
   const D3D11_VIDEO_DECODER_BUFFER_DESC *desc;
   unsigned i;
   int before = live, old_gets, old_begins;
   check("HEVC: parser opens", v != NULL);
   if (!v) return;
   check("HEVC: config", !rh265_video_set_extradata(v, hevc_config8, sizeof(hevc_config8)));
   check("HEVC: borrowed AU", !rh265_video_get_access_unit(v, hevc_sample8,
         sizeof(hevc_sample8), 4, slices, 4, &au));
   check("HEVC: params", !rh265_video_get_params(v, au.first.pps_id, &params));
   params.bit_depth = 12;
   check("HEVC: unsupported depth refused before allocation",
         !d3d11_video_decode_hevc_open(&params, 32, &ops) && live == before && !ops.decoder);
   params.bit_depth = 8;
   check("HEVC: undersized DPB refused", !d3d11_video_decode_hevc_open(&params, 1, &ops));
   views = views_ok = 0;
   if (!d3d11_video_decode_hevc_open(&params, 32, &ops))
   {
      check("HEVC: device opens", 0);
      rh265_video_close(v);
      return;
   }
   d = (d3d11vd_dec_t*)ops.decoder;
   check("HEVC: Main profile, raw 1, matching views", dec_raw == 1
         && !memcmp(&dec_desc.Guid, &d3d11vd_hevc_main, sizeof(GUID))
         && views == 32 && views_ok == 32);
   check("HEVC: session", rh265_hw_session_init(&session, &ops, 32));
   subs_free(); old_gets = buffer_gets; old_begins = begins;
   pending_left = 1;
   check("HEVC: busy neither commits nor acquires buffers",
         rh265_hw_session_submit(&session, hevc_sample8, sizeof(hevc_sample8),
            &params, &au, slices) == 1 && buffer_gets == old_gets
         && begins == old_begins && d->feedback == 0 && session.state.serial == 1);
   check("HEVC: retry submits once", !rh265_hw_session_submit(&session,
         hevc_sample8, sizeof(hevc_sample8), &params, &au, slices)
         && n_subs == 1 && session.state.serial == 2);
   check("HEVC: disabled scaling list omits matrix", subs[0].nbufs == 3
         && !find_desc(&subs[0], D3D11_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX));
   desc = find_desc(&subs[0], D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS);
   check("HEVC: 232-byte picture parameters", desc && desc->DataSize == DXVA_HEVC_PP_SIZE);
   desc = find_desc(&subs[0], D3D11_VIDEO_DECODER_BUFFER_BITSTREAM);
   check("HEVC: borrowed NAL packed directly", desc && !(desc->DataSize & 127)
         && subs[0].bs[2] == 1 && !memcmp(subs[0].bs + 3,
            slices[0].data, slices[0].size));
   if (desc)
      for (i = (unsigned)slices[0].size + 3; i < desc->DataSize; i++)
         if (subs[0].bs[i]) { check("HEVC: zero padding", 0); break; }
   check("HEVC: output gets shared lease", !rh265_hw_session_output(&session, true, &frame)
         && frame.image && frame.width == 64 && frame.height == 64);
   d3d11_video_decode_hevc_close(&ops);
   check("HEVC: close retains unread handles", !ops.decoder && live > before);
   video_hw_frame_abandon(&frame);
   d3d11vd_grave_reap(false);
   check("HEVC: abandon retires resources", live == before);

   params.scaling_list_enabled = 1;
   memset(params.list4, 16, sizeof(params.list4));
   memset(params.list8, 16, sizeof(params.list8));
   memset(params.list16, 16, sizeof(params.list16));
   memset(params.list32, 16, sizeof(params.list32));
   memset(params.dc16, 16, sizeof(params.dc16));
   memset(params.dc32, 16, sizeof(params.dc32));
   views = views_ok = 0;
   check("HEVC: scaling session opens", d3d11_video_decode_hevc_open(&params, 32, &ops));
   check("HEVC: scaling owner initializes", rh265_hw_session_init(&session, &ops, 32));
   subs_free();
   check("HEVC: enabled scaling lists submit", !rh265_hw_session_submit(&session,
         hevc_sample8, sizeof(hevc_sample8), &params, &au, slices) && n_subs == 1);
   desc = find_desc(&subs[0], D3D11_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX);
   check("HEVC: 1000-byte matrix included", subs[0].nbufs == 4
         && desc && desc->DataSize == DXVA_HEVC_QM_SIZE);
   check("HEVC: raster scaling coefficients converted", bufs[
         D3D11_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX][0] == params.list4[0][0]
         && bufs[D3D11_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX][998] == params.dc32[0]);
   d3d11_video_decode_hevc_close(&ops);
   params.scaling_list_enabled = 0;

   for (i = 0; i < 4; i++)
   {
      views = views_ok = 0;
      check("HEVC: fault session opens", d3d11_video_decode_hevc_open(&params, 32, &ops));
      check("HEVC: fault owner initializes", rh265_hw_session_init(&session, &ops, 32));
      old_begins = begins;
      if (i == 0) buf_cap[D3D11_VIDEO_DECODER_BUFFER_BITSTREAM] = 1;
      if (i == 1) release_hr = E_FAIL;
      if (i == 2) submit_hr = E_FAIL;
      if (i == 3) end_hr = E_FAIL;
      check("HEVC: driver failure ends frame without committing",
            rh265_hw_session_submit(&session, hevc_sample8, sizeof(hevc_sample8),
               &params, &au, slices) < 0 && session.state.serial == 1
            && begins == old_begins + 1 && begins == ends && session.broken);
      buf_cap[D3D11_VIDEO_DECODER_BUFFER_BITSTREAM] = sizeof(bufs[0]);
      release_hr = submit_hr = end_hr = S_OK;
      d3d11_video_decode_hevc_close(&ops);
      check("HEVC: failure closes without leaks", live == before);
   }
   check("HEVC: buffers always released", buffer_gets == releases);
   rh265_video_close(v);
   subs_free();
}

static void test_hevc_main10(void)
{
   static rh265_video_params_t params;
   static rh265_hw_session_t session;
   rh265_video *v = rh265_video_open();
   rh265_hw_ops_t ops;
   rh265_access_unit_t au;
   rh265_nal_t slices[4];
   video_hw_frame_t frame;
   D3D11_TEXTURE2D_DESC texture;
   d3d11vd_dec_t *d;
   int before = live;
   check("Main10: parser opens", v != NULL);
   if (!v) return;
   check("Main10: config", !rh265_video_set_extradata(v, hevc_config10, sizeof(hevc_config10)));
   check("Main10: borrowed AU", !rh265_video_get_access_unit(v, hevc_sample10,
         sizeof(hevc_sample10), 4, slices, 4, &au));
   check("Main10: params", !rh265_video_get_params(v, au.first.pps_id, &params));
   views = views_ok = 0;
   if (!d3d11_video_decode_hevc_open(&params, 32, &ops))
   { check("Main10: backend opens", 0); rh265_video_close(v); return; }
   d = (d3d11vd_dec_t*)ops.decoder;
   check("Main10: profile/P010/raw 1", dec_raw == 1 && dec_desc.OutputFormat == DXGI_FORMAT_P010
         && !memcmp(&dec_desc.Guid, &d3d11vd_hevc_main10, sizeof(GUID)));
   d->dpb->lpVtbl->GetDesc(d->dpb, &texture);
   check("Main10: P010 DPB and matching views", texture.Format == DXGI_FORMAT_P010
         && views == 32 && views_ok == 32);
   check("Main10: 8-bit readback helper refuses P010", d3d11vd_readback(d, 0, NULL) < 0);
   d->out[0]->lpVtbl->GetDesc(d->out[0], &texture);
   check("Main10: shared P010 export texture", texture.Format == DXGI_FORMAT_P010
         && (texture.MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE));
   check("Main10: session", rh265_hw_session_init(&session, &ops, 32));
   subs_free(); pending_left = 1;
   check("Main10: busy", rh265_hw_session_submit(&session, hevc_sample10,
         sizeof(hevc_sample10), &params, &au, slices) == 1);
   check("Main10: submission", !rh265_hw_session_submit(&session, hevc_sample10,
         sizeof(hevc_sample10), &params, &au, slices) && n_subs == 1);
   check("Main10: exports format and lease", !rh265_hw_session_output(&session, true, &frame)
         && frame.pixfmt == IMAGE_PIXFMT_P010 && frame.image);
   d3d11_video_decode_hevc_close(&ops);
   check("Main10: close retains lease", live > before);
   video_hw_frame_abandon(&frame); d3d11vd_grave_reap(false);
   check("Main10: retires resources", live == before);
   rh265_video_close(v); subs_free();
}

static void test_hevc_registry(void)
{
   rh265_video *v = rh265_video_open();
   video_hw_frame_t frame;
   int before = live, ret;
   check("HEVC registry: parser opens", v != NULL);
   if (!v) return;
   views = views_ok = 0;
   check("HEVC registry: config", !rh265_video_set_extradata(v,
         hevc_config8, sizeof(hevc_config8)));
   check("HEVC registry: D3D11 selected", rh265_video_set_hw(v, 1)
         && rh265_video_is_hw(v));
   pending_left = 1;
   check("HEVC registry: busy packet retained", rh265_video_decode(v,
         hevc_sample8, sizeof(hevc_sample8)) == 2);
   check("HEVC registry: accepted picture", rh265_video_decode(v,
         hevc_sample8, sizeof(hevc_sample8)) == 1);
   check("HEVC registry: transfers shared export", !rh265_video_hw_frame(v, &frame));
   rh265_video_close(v);
   check("HEVC registry: close retains external lease", live > before);
   video_hw_frame_abandon(&frame);
   d3d11vd_grave_reap(false);
   check("HEVC registry: abandonment retires owner", live == before);

   v = rh265_video_open();
   check("HEVC registry: Main10 config", !rh265_video_set_extradata(v,
         hevc_config10, sizeof(hevc_config10)));
   check("HEVC registry: Main10 stays on software", !rh265_video_set_hw(v, 1)
         && !rh265_video_is_hw(v));
   ret = rh265_video_decode(v, hevc_sample10, sizeof(hevc_sample10));
   if (!ret) ret = rh265_video_drain(v) == 0 ? 1 : -1;
   check("HEVC registry: fallback reconstructs Main10", ret == 1
         && rh265_video_bit_depth(v) == 10 && rh265_video_plane(v, 0, NULL, NULL, NULL));
   rh265_video_close(v);
}

static void test_vp9(int depth, int color_space)
{
   static rvp9_hw_picture_t p;
   static const uint8_t packet[7] = {0x82, 0x49, 0x83, 0x42, 1, 2, 3};
   rvp9_hw_ops_t ops;
   video_hw_frame_t frames[4], extra;
   const D3D11_VIDEO_DECODER_BUFFER_DESC *desc;
   d3d11vd_dec_t *d;
   unsigned i, formats = d3d11vd.import_formats;
   int before = live, old_gets, old_begins, old_copies;
   memset(&p, 0, sizeof(p));
   p.header.profile = depth == 10 ? 2 : 0; p.header.bit_depth = depth;
   p.header.w = 320; p.header.h = 192; p.header.show_frame = 1;
   p.header.subsampling_x = p.header.subsampling_y = 1;
   p.header.color_space = 2; p.header.color_range = 1;
   p.header.uncomp_size = 1; p.header.first_partition_size = 1;
   p.previous_slot = -1;
   for (i = 0; i < 8; i++) p.reference_slots[i] = -1;
   for (i = 0; i < 3; i++) p.named_slots[i] = -1;
   d3d11vd.import_formats = IMAGE_PIXFMT_NV12;
   if (depth == 10)
      check("VP9: P010 import required before allocation",
            !d3d11_video_decode_vp9_open(&p.header, 10, &ops) && live == before);
   d3d11vd.import_formats = IMAGE_PIXFMT_NV12 | IMAGE_PIXFMT_P010;
   check("VP9: insufficient DPB refused", !d3d11_video_decode_vp9_open(&p.header, 9, &ops));
   p.header.color_space = 4;
   check("VP9: unsupported SMPTE240 output matrix refused", !d3d11_video_decode_vp9_open(&p.header, 10, &ops));
   p.header.color_space = 6;
   check("VP9: reserved color space refused", !d3d11_video_decode_vp9_open(&p.header, 10, &ops));
   p.header.color_space = 7;
   check("VP9: RGB color space refused", !d3d11_video_decode_vp9_open(&p.header, 10, &ops));
   p.header.color_space = color_space; views = views_ok = 0;
   if (!d3d11_video_decode_vp9_open(&p.header, 10, &ops))
   { check("VP9: decoder opens", 0); d3d11vd.import_formats = formats; return; }
   d = (d3d11vd_dec_t*)ops.decoder;
   check("VP9: profile, raw configuration and coded size", dec_raw == 1
         && dec_desc.SampleWidth == 320 && dec_desc.SampleHeight == 192
         && !memcmp(&dec_desc.Guid, depth == 10 ? &d3d11vd_vp9_profile2 : &d3d11vd_vp9_profile0, sizeof(GUID))
         && views == 10 && views_ok == 10);
   check("VP9: padded DPB, visible shared output", ((mock_t*)d->dpb)->desc.Width == 384
         && ((mock_t*)d->dpb)->desc.Height == 256 && d->visible_w == 320 && d->visible_h == 192);
   subs_free(); old_gets = buffer_gets; old_begins = begins; pending_left = 1;
   check("VP9: BUSY leaves buffers and feedback untouched", ops.decode(d, packet, sizeof(packet), &p) == 1
         && buffer_gets == old_gets && begins == old_begins && !d->feedback);
   check("VP9: retry submits exactly once", !ops.decode(d, packet, sizeof(packet), &p)
         && n_subs == 1 && subs[0].nbufs == 3 && d->feedback == 1);
   desc = find_desc(&subs[0], D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS);
   check("VP9: packed picture parameters", desc && desc->DataSize == DXVA_VP9_PP_SIZE);
   desc = find_desc(&subs[0], D3D11_VIDEO_DECODER_BUFFER_BITSTREAM);
   check("VP9: packet without repacking, padded to 128 bytes", desc && desc->DataSize == 128
         && !memcmp(subs[0].bs, packet, sizeof(packet)));
   for (i = sizeof(packet); i < 128; i++)
      if (subs[0].bs[i]) { check("VP9: padding zero", 0); break; }
   desc = find_desc(&subs[0], D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL);
   check("VP9: one complete padded frame", desc && desc->DataSize == 10
         && !subs[0].sc[0] && subs[0].sc[4] == 128 && !subs[0].sc[8] && !subs[0].sc[9]);
   for (i = 0; i < 4; i++)
      check("VP9: independent output lease", !ops.output(d, 0, &frames[i])
            && frames[i].width == 320 && frames[i].height == 192
            && frames[i].pixfmt == (unsigned)(depth == 10 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12)
            && frames[i].yuv == ((color_space == 2 ? IMAGE_YUV_FLAG_BT709 : 0) | IMAGE_YUV_FLAG_FULL_RANGE));
   old_copies = copies;
   check("VP9: held leases apply backpressure without a copy", ops.output(d, 0, &extra) == 1 && copies == old_copies);
   d3d11_video_decode_vp9_close(&ops);
   check("VP9: close retains unread handles", !ops.decoder && live > before);
   for (i = 0; i < 4; i++) video_hw_frame_abandon(&frames[i]);
   d3d11vd_grave_reap(false);
   check("VP9: final abandon retires decoder", live == before);
   for (i = 0; i < 4; i++)
   {
      views = views_ok = 0; subs_free();
      check("VP9: fault session opens", d3d11_video_decode_vp9_open(&p.header, 10, &ops));
      old_begins = begins;
      if (i == 0) buf_cap[D3D11_VIDEO_DECODER_BUFFER_BITSTREAM] = 1;
      if (i == 1) release_hr = E_FAIL;
      if (i == 2) submit_hr = E_FAIL;
      if (i == 3) end_hr = E_FAIL;
      check("VP9: failed submission ends the decoder frame", ops.decode(ops.decoder, packet, sizeof(packet), &p) < 0
            && begins == old_begins + 1 && begins == ends);
      buf_cap[D3D11_VIDEO_DECODER_BUFFER_BITSTREAM] = sizeof(bufs[0]);
      release_hr = submit_hr = end_hr = S_OK;
      d3d11_video_decode_vp9_close(&ops);
      check("VP9: fault resources retire", live == before);
   }
   d3d11vd.import_formats = formats;
}


static void test_shared_dpb(int depth)
{
   static rvp9_hw_picture_t p;
   static const uint8_t packet[7] = {0x82, 0x49, 0x83, 0x42, 1, 2, 3};
   rvp9_hw_ops_t ops;
   video_hw_frame_t held;
   d3d11vd_dec_t *d;
   unsigned i, old_slot;
   int before = live, old_copies = copies;
   memset(&p, 0, sizeof(p));
   p.header.profile = depth == 10 ? 2 : 0; p.header.bit_depth = depth;
   p.header.w = 320; p.header.h = 192; p.header.show_frame = 1;
   p.header.subsampling_x = p.header.subsampling_y = 1;
   p.header.color_space = 2; p.header.color_range = 1;
   p.header.uncomp_size = 1; p.header.first_partition_size = 1;
   p.previous_slot = -1;
   for (i = 0; i < 8; i++) p.reference_slots[i] = -1;
   for (i = 0; i < 3; i++) p.named_slots[i] = -1;
   views = views_ok = 0;
   if (!d3d11_video_decode_vp9_open(&p.header, 10, &ops))
   { check("shared DPB opens", 0); return; }
   d = (d3d11vd_dec_t*)ops.decoder;
   check("shared DPB has spare physical slices, no output textures",
         d->direct_dpb && d->physical_slots == 14 && !d->out[0]
         && ((mock_t*)d->dpb)->desc.ArraySize >= 14 && d->dpb_handle);
   check("shared DPB first submission", !ops.decode(d, packet, sizeof(packet), &p));
   check("shared DPB exports without a copy", !ops.output(d, 0, &held)
         && held.image == (uint64_t)(uintptr_t)d->dpb_handle && copies == old_copies);
   old_slot = d->slot_map[0]; pending_left = 1;
   check("shared DPB BUSY does not commit a rename",
         ops.decode(d, packet, sizeof(packet), &p) == 1 && d->slot_map[0] == old_slot);
   check("shared DPB held slice is renamed on retry",
         !ops.decode(d, packet, sizeof(packet), &p) && d->slot_map[0] != held.array_slice
         && !p.slot && copies == old_copies);
   p.slot = 1; p.header.frame_type = 1;
   p.reference_slots[0] = 0;
   for (i = 0; i < 3; i++) p.named_slots[i] = 0;
   check("shared DPB reference maps follow renamed slot",
         !ops.decode(d, packet, sizeof(packet), &p)
         && d->pp[16] == d->slot_map[0] && d->pp[88] == d->slot_map[0]
         && d->pp[0] == d->slot_map[1]);
   p.named_slots[0] = 1000;
   check("shared DPB rejects invalid named reference",
         ops.decode(d, packet, sizeof(packet), &p) < 0);
   d3d11_video_decode_vp9_close(&ops);
   check("shared DPB close retains exported array", live > before);
   video_hw_frame_abandon(&held); d3d11vd_grave_reap(false);
   check("shared DPB abandon retires array", live == before);
   p.header.frame_type = 0;
   reject_shared_dpb = 1; views = views_ok = 0;
   check("unsupported shared DPB falls back",
         d3d11_video_decode_vp9_open(&p.header, 10, &ops));
   d = (d3d11vd_dec_t*)ops.decoder;
   check("fallback keeps copied output textures", d && !d->direct_dpb && d->out[0]);
   reject_shared_dpb = 0;
   d3d11_video_decode_vp9_close(&ops);
   check("fallback retires resources", live == before);
}

static void test_packed_vp9_registry(void)
{
   mock_t *adapter = mock_new(K_ADAPTER);
   rvp9_hw_backend_t replacement;
   memset(&replacement, 0, sizeof(replacement));
   check("packed renderer starts without taking H264",
         d3d11_video_decode_init_packed_adapter(adapter, IMAGE_PIXFMT_YUV8_FP16, false)
         && !d3d11vd.h264_owned);
   check("packed renderer retains VP9 configuration admission",
         d3d11vd.vp9.open_config != NULL);
   check("packed renderer publishes VP9", rvp9_unset_hw_backend(&d3d11vd.vp9));
   rvp9_set_hw_backend(&d3d11vd.vp9);
   d3d11_video_decode_deinit();
   check("packed teardown withdraws VP9", !rvp9_unset_hw_backend(&d3d11vd.vp9));
   check("packed renderer restarts", d3d11_video_decode_init_packed_adapter(
            adapter, IMAGE_PIXFMT_YUV8_FP16, false));
   rvp9_set_hw_backend(&replacement);
   d3d11_video_decode_deinit();
   check("packed teardown preserves a replacement VP9 owner",
         rvp9_unset_hw_backend(&replacement));
   m_release(adapter);
}

int main(void)
{
   ID3D11Device *dev;
   mock_t *m;
   int live0;

   timeBeginPeriod(1);
   setvbuf(stdout, NULL, _IONBF, 0);
   mocks_init();
   /* the backend finds D3D11CreateDevice in d3d11.dll, so it is loaded,
    * and then given the mock's */
   if (!LoadLibraryA("d3d11.dll"))
   {
      printf("d3d11_video_decode_mock_test: SKIP (no d3d11.dll)\n");
      return 0;
   }
   m     = mock_new(K_DEVICE);
   dev   = (ID3D11Device*)m;
   live0 = live;
   check("the backend starts on the device's adapter",
         d3d11_video_decode_init(dev) && d3d11vd.adapter != NULL);
   the_adapter    = (mock_t*)d3d11vd.adapter;
   d3d11vd.create = mock_create;

   test_stream("baseline", stream_baseline, sizeof(stream_baseline),
         176, 144, 0, 0, 99, 1, 16);
   check("a device of its own, on that adapter, for video",
         creates == 1 && create_ok == 1);
   test_stream("main_bpyr", stream_main_bpyr, sizeof(stream_main_bpyr),
         176, 144, 0, 0, 99, 1, 20);
   test_stream("high_slices", stream_high_slices, sizeof(stream_high_slices),
         176, 144, 0, 0, 99, 4, 16);
   test_stream("cropped", stream_cropped, sizeof(stream_cropped),
         170, 122, 0, 0, 88, 1, 12);
   test_config_retry();
   test_direct();
   test_device_loss();
   test_hevc();
   test_hevc_main10();
   test_hevc_registry();
   test_vp9(8, 2);
   test_vp9(10, 2);
   test_vp9(10, 5);

   d3d11_video_decode_deinit();
   check("P010 renderer backend starts", d3d11_video_decode_init_formats(dev,
         IMAGE_PIXFMT_NV12 | IMAGE_PIXFMT_P010));
   the_adapter = (mock_t*)d3d11vd.adapter;
   d3d11vd.create = mock_create;
   {
      rh265_video *v = rh265_video_open();
      video_hw_frame_t frame;
      int before = live;
      check("P010 renderer: config", v && !rh265_video_set_extradata(v,
            hevc_config10, sizeof(hevc_config10)));
      check("P010 renderer: automatic Main10", rh265_video_set_hw(v, 1)
            && rh265_video_is_hw(v));
      pending_left = 1;
      check("P010 renderer: busy", rh265_video_decode(v,
            hevc_sample10, sizeof(hevc_sample10)) == 2);
      check("P010 renderer: accepted", rh265_video_decode(v,
            hevc_sample10, sizeof(hevc_sample10)) == 1);
      check("P010 renderer: lease", !rh265_video_hw_frame(v, &frame)
            && frame.pixfmt == IMAGE_PIXFMT_P010);
      rh265_video_close(v);
      check("P010 renderer: close retains lease", live > before);
      video_hw_frame_abandon(&frame);
      d3d11vd_grave_reap(false);
      check("P010 renderer: abandonment retires owner", live == before);
   }

   d3d11_video_decode_deinit();
   check("shared DPB renderer starts", d3d11_video_decode_init_formats(dev,
         IMAGE_PIXFMT_NV12 | IMAGE_PIXFMT_P010 | D3D11_VIDEO_DECODE_SHARED_DPB));
   the_adapter = (mock_t*)d3d11vd.adapter;
   d3d11vd.create = mock_create;
   test_shared_dpb(8);
   test_shared_dpb(10);
   d3d11_video_decode_deinit();
   check("deinit lets the adapter go", live == live0);
   test_packed_vp9_registry();
   check("packed registry releases its adapter", live == live0);
   m_release(m);
   check("no mock object left", live == 0);
   subs_free();
   printf("d3d11_video_decode_mock_test: %s (%d failure%s)\n",
         fails ? "FAIL" : "PASS", fails, fails == 1 ? "" : "s");
   timeEndPeriod(1);
   return fails ? 1 : 0;
}
