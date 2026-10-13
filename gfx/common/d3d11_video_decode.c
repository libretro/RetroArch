/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* rh264's hardware backend on D3D11 video.
 *
 * A decoder is one stream, on a device of its own made on the driver's
 * adapter: its immediate context is used by the decoding thread alone,
 * and the driver's device never sees another thread. The DPB is one
 * NV12 texture array, a slice per rh264 slot, decoded into through one
 * output view each.
 *
 * A picture to be shown is copied, from the visible window, into one
 * of a few shared NV12 textures, and a shared fence raised behind the
 * copy. The driver opens both through their NT handles, waits for the
 * fence on its own queue, converts, and raises the texture's own
 * fence; a texture is copied into again once that has come, or once
 * the frame was abandoned unread.
 *
 * A closed decoder whose textures are still to be read waits in a
 * graveyard until they are, so their handles stay good; deinit empties
 * it. Nothing here takes a lock: the graveyard is an atomic list. */

#ifndef CINTERFACE
#define CINTERFACE
#endif
#ifndef COBJMACROS
#define COBJMACROS
#endif

#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <d3d11.h>
#include <d3d11_4.h>
#include <dxgi1_2.h>

#include <retro_atomic.h>
#include <formats/rh264.h>
#include <formats/video_hw_frame.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "d3d11_video_decode.h"
#include "dxva_h264.h"
#include "../video_defines.h"
#include "../../configuration.h"
#include "../../verbosity.h"

#define D3D11VD_MAX_SLOTS  32
#define D3D11VD_MAX_SLICES 64     /* rh264 gathers no more */
#define D3D11VD_OUTS       4      /* shared output textures per decoder */
/* The scaling lists in coded (zig-zag) order; a sample sets it to try
 * raster order on a GPU */
#ifndef D3D11VD_QM_RASTER
#define D3D11VD_QM_RASTER 0
#endif

#ifndef E_PENDING
#define E_PENDING ((HRESULT)0x8000000AL)
#endif

#define D3D11VD_RELEASE(p) do { if (p) { \
   ((IUnknown*)(p))->lpVtbl->Release((IUnknown*)(p)); (p) = NULL; } } while (0)

/* Kept here, so that nothing needs the IIDs linked in */
static const GUID d3d11vd_h264_vld_nofgt =
   { 0x1b81be68, 0xa0c7, 0x11d3, { 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5 } };
static const GUID d3d11vd_iid_dxgi_device =
   { 0x54ec77fa, 0x1377, 0x44e6, { 0x8c, 0x32, 0x88, 0xfd, 0x5f, 0x44, 0xc8, 0x4c } };
static const GUID d3d11vd_iid_dxgi_resource1 =
   { 0x30961379, 0x4609, 0x4a41, { 0x99, 0x8e, 0x54, 0xfe, 0x56, 0x7e, 0xe0, 0xc1 } };
static const GUID d3d11vd_iid_device5 =
   { 0x8ffde202, 0xa0e7, 0x45df, { 0x9e, 0x01, 0xe8, 0x37, 0x80, 0x1b, 0x5e, 0xa0 } };
static const GUID d3d11vd_iid_context4 =
   { 0x917600da, 0xf58c, 0x4c33, { 0x98, 0xd8, 0x3e, 0x15, 0xb3, 0x90, 0xfa, 0x24 } };
static const GUID d3d11vd_iid_fence =
   { 0xaffde9d1, 0x1df7, 0x4bb7, { 0x8a, 0x34, 0x0f, 0x46, 0x25, 0x1d, 0xab, 0x80 } };
static const GUID d3d11vd_iid_video_device =
   { 0x10ec4d5b, 0x975a, 0x4689, { 0xb9, 0xe4, 0xd0, 0xaa, 0xc3, 0x0f, 0xe3, 0x33 } };
static const GUID d3d11vd_iid_video_context =
   { 0x61f21c45, 0x3c0e, 0x4a74, { 0x9c, 0xea, 0x67, 0x10, 0x0d, 0x9a, 0xd5, 0xe4 } };

typedef struct d3d11vd_dec
{
   struct d3d11vd_dec *grave_next;

   ID3D11Device                 *dev;
   ID3D11DeviceContext          *ctx;
   ID3D11DeviceContext4         *ctx4;
   ID3D11VideoDevice            *vdev;
   ID3D11VideoContext           *vctx;
   ID3D11VideoDecoder           *decoder;
   ID3D11Texture2D              *dpb;
   ID3D11Texture2D              *readback;
   ID3D11VideoDecoderOutputView *view[D3D11VD_MAX_SLOTS];

   unsigned coded_w, coded_h, mbs;
   unsigned visible_w, visible_h;
   unsigned crop_x, crop_y;
   unsigned slots;
   unsigned gen;                  /* names this decoder's handles     */
   uint32_t feedback;

   ID3D11Texture2D *out[D3D11VD_OUTS];
   HANDLE           out_handle[D3D11VD_OUTS];
   ID3D11Fence     *out_done[D3D11VD_OUTS];
   HANDLE           out_done_handle[D3D11VD_OUTS];
   uint64_t         out_value[D3D11VD_OUTS];   /* the read last handed out */
   retro_atomic_int_t out_abandoned[D3D11VD_OUTS];
   unsigned         out_next;

   ID3D11Fence *ready;            /* raised behind each copy          */
   HANDLE       ready_handle;
   uint64_t     ready_value;

   uint8_t pp[DXVA_H264_PP_SIZE];
   uint8_t qm[DXVA_H264_QM_SIZE];
   uint8_t sc[D3D11VD_MAX_SLICES * DXVA_H264_SLICE_SIZE];
} d3d11vd_dec_t;

static struct
{
   IDXGIAdapter *adapter;
   HMODULE dll;
   PFN_D3D11_CREATE_DEVICE create;
   bool up;
   retro_atomic_int_t live;
   retro_atomic_int_t serial;
   retro_atomic_ptr_t grave;
   rh264_hw_ops_t ops;
} d3d11vd;

/* ---- teardown ---- */

static void d3d11vd_destroy(d3d11vd_dec_t *d)
{
   unsigned i;
   for (i = 0; i < D3D11VD_OUTS; i++)
   {
      if (d->out_handle[i])
         CloseHandle(d->out_handle[i]);
      if (d->out_done_handle[i])
         CloseHandle(d->out_done_handle[i]);
      D3D11VD_RELEASE(d->out[i]);
      D3D11VD_RELEASE(d->out_done[i]);
   }
   if (d->ready_handle)
      CloseHandle(d->ready_handle);
   D3D11VD_RELEASE(d->ready);
   for (i = 0; i < D3D11VD_MAX_SLOTS; i++)
      D3D11VD_RELEASE(d->view[i]);
   D3D11VD_RELEASE(d->readback);
   D3D11VD_RELEASE(d->dpb);
   D3D11VD_RELEASE(d->decoder);
   D3D11VD_RELEASE(d->vctx);
   D3D11VD_RELEASE(d->vdev);
   D3D11VD_RELEASE(d->ctx4);
   D3D11VD_RELEASE(d->ctx);
   D3D11VD_RELEASE(d->dev);
   free(d);
}

/* Output texture @o: 0 settled, 1 held, -1 device removed. */
static int d3d11vd_out_status(d3d11vd_dec_t *d, unsigned o)
{
   uint64_t completed;
   if (!d->out_done[o] || (uint64_t)(unsigned)retro_atomic_load_acquire_int(
            &d->out_abandoned[o]) == d->out_value[o])
      return 0;
   completed = d->out_done[o]->lpVtbl->GetCompletedValue(d->out_done[o]);
   if (completed == UINT64_MAX)
      return -1;
   return completed >= d->out_value[o] ? 0 : 1;
}

static bool d3d11vd_settled(d3d11vd_dec_t *d)
{
   unsigned o;
   for (o = 0; o < D3D11VD_OUTS; o++)
      if (d3d11vd_out_status(d, o) != 0)
         return false;
   return true;
}

static void d3d11vd_grave_push(d3d11vd_dec_t *d)
{
   void *head;
   do
   {
      head          = retro_atomic_load_acquire_ptr(&d3d11vd.grave);
      d->grave_next = (d3d11vd_dec_t*)head;
   } while (!retro_atomic_cas_ptr(&d3d11vd.grave, head, d));
}

/* Frees the closed decoders that have settled, or all of them with
 * @all. Whoever takes the list owns what is on it. */
static void d3d11vd_grave_reap(bool all)
{
   d3d11vd_dec_t *d = (d3d11vd_dec_t*)retro_atomic_exchange_ptr(
         &d3d11vd.grave, NULL);
   while (d)
   {
      d3d11vd_dec_t *next = d->grave_next;
      if (all || d3d11vd_settled(d))
         d3d11vd_destroy(d);
      else
         d3d11vd_grave_push(d);
      d = next;
   }
}

/* ---- the frame handed out ---- */

/* The reader will never come: its read, counted as done. A fence is
 * raised only from a queue, so the next copy looks here instead. */
static void d3d11vd_abandon(void *owner, uint64_t done, uint64_t value)
{
   d3d11vd_dec_t *d = (d3d11vd_dec_t*)owner;
   unsigned o;
   if (!d3d11vd.up || !d)
      return;
   for (o = 0; o < D3D11VD_OUTS; o++)
      if ((uint64_t)(uintptr_t)d->out_done_handle[o] == done)
      {
         retro_atomic_store_release_int(&d->out_abandoned[o], (int)value);
         return;
      }
}

/* ---- open ---- */

static bool d3d11vd_make_device(d3d11vd_dec_t *d)
{
   static const D3D_FEATURE_LEVEL levels[2] = {
      D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
   D3D11_FEATURE_DATA_D3D11_OPTIONS4 opt4;

   if (FAILED(d3d11vd.create(d3d11vd.adapter, D3D_DRIVER_TYPE_UNKNOWN,
            NULL, D3D11_CREATE_DEVICE_VIDEO_SUPPORT, levels, 2,
            D3D11_SDK_VERSION, &d->dev, NULL, &d->ctx)))
      return false;
   memset(&opt4, 0, sizeof(opt4));
   return SUCCEEDED(d->ctx->lpVtbl->QueryInterface(d->ctx,
            &d3d11vd_iid_context4, (void**)&d->ctx4))
      && SUCCEEDED(d->dev->lpVtbl->QueryInterface(d->dev,
            &d3d11vd_iid_video_device, (void**)&d->vdev))
      && SUCCEEDED(d->ctx->lpVtbl->QueryInterface(d->ctx,
            &d3d11vd_iid_video_context, (void**)&d->vctx))
      && SUCCEEDED(d->dev->lpVtbl->CheckFeatureSupport(d->dev,
            D3D11_FEATURE_D3D11_OPTIONS4, &opt4, sizeof(opt4)))
      && opt4.ExtendedNV12SharedTextureSupported;
}

/* The decoder, with a configuration that takes short slice control:
 * the bitstream as sent, start codes and all. */
static bool d3d11vd_make_decoder(d3d11vd_dec_t *d, unsigned *min_targets)
{
   D3D11_VIDEO_DECODER_DESC desc;
   D3D11_VIDEO_DECODER_CONFIG cfg;
   BOOL ok = FALSE;
   UINT n  = 0, i;

   if (     FAILED(d->vdev->lpVtbl->CheckVideoDecoderFormat(d->vdev,
               &d3d11vd_h264_vld_nofgt, DXGI_FORMAT_NV12, &ok))
         || !ok)
      return false;
   memset(&desc, 0, sizeof(desc));
   desc.Guid         = d3d11vd_h264_vld_nofgt;
   desc.SampleWidth  = d->coded_w;
   desc.SampleHeight = d->coded_h;
   desc.OutputFormat = DXGI_FORMAT_NV12;
   if (FAILED(d->vdev->lpVtbl->GetVideoDecoderConfigCount(d->vdev,
         &desc, &n)))
      return false;
   for (i = 0; i < n; i++)
   {
      if (FAILED(d->vdev->lpVtbl->GetVideoDecoderConfig(d->vdev, &desc,
            i, &cfg)))
         continue;
      if (cfg.ConfigBitstreamRaw != 2)
         continue;
      *min_targets = cfg.ConfigMinRenderTargetBuffCount;
      return SUCCEEDED(d->vdev->lpVtbl->CreateVideoDecoder(d->vdev,
               &desc, &cfg, &d->decoder));
   }
   return false;
}

/* The slots: slices of one texture array, an output view each. */
static bool d3d11vd_make_dpb(d3d11vd_dec_t *d, unsigned min_targets)
{
   D3D11_TEXTURE2D_DESC desc;
   D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC vd;
   unsigned i;

   memset(&desc, 0, sizeof(desc));
   desc.Width            = d->coded_w;
   desc.Height           = d->coded_h;
   desc.MipLevels        = 1;
   desc.ArraySize        = d->slots > min_targets ? d->slots : min_targets;
   desc.Format           = DXGI_FORMAT_NV12;
   desc.SampleDesc.Count = 1;
   desc.Usage            = D3D11_USAGE_DEFAULT;
   desc.BindFlags        = D3D11_BIND_DECODER;
   if (FAILED(d->dev->lpVtbl->CreateTexture2D(d->dev, &desc, NULL,
         &d->dpb)))
      return false;
   for (i = 0; i < d->slots; i++)
   {
      memset(&vd, 0, sizeof(vd));
      vd.DecodeProfile         = d3d11vd_h264_vld_nofgt;
      vd.ViewDimension         = D3D11_VDOV_DIMENSION_TEXTURE2D;
      vd.Texture2D.ArraySlice  = i;
      if (FAILED(d->vdev->lpVtbl->CreateVideoDecoderOutputView(d->vdev,
            (ID3D11Resource*)d->dpb, &vd, &d->view[i])))
         return false;
   }
   return true;
}

static bool d3d11vd_shared_fence(ID3D11Device5 *dev5, ID3D11Fence **f,
      HANDLE *h)
{
   return SUCCEEDED(dev5->lpVtbl->CreateFence(dev5, 0,
            D3D11_FENCE_FLAG_SHARED, &d3d11vd_iid_fence, (void**)f))
      && SUCCEEDED((*f)->lpVtbl->CreateSharedHandle(*f, NULL, GENERIC_ALL,
            NULL, h));
}

/* The shared textures the driver reads and the fences either side
 * raises. */
static bool d3d11vd_make_outputs(d3d11vd_dec_t *d)
{
   D3D11_TEXTURE2D_DESC desc;
   ID3D11Device5 *dev5 = NULL;
   bool ok             = false;
   unsigned o;

   if (FAILED(d->dev->lpVtbl->QueryInterface(d->dev, &d3d11vd_iid_device5,
         (void**)&dev5)))
      return false;
   memset(&desc, 0, sizeof(desc));
   desc.Width            = d->visible_w;
   desc.Height           = d->visible_h;
   desc.MipLevels        = 1;
   desc.ArraySize        = 1;
   desc.Format           = DXGI_FORMAT_NV12;
   desc.SampleDesc.Count = 1;
   desc.Usage            = D3D11_USAGE_DEFAULT;
   desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
   desc.MiscFlags        = D3D11_RESOURCE_MISC_SHARED
                         | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
   for (o = 0; o < D3D11VD_OUTS; o++)
   {
      IDXGIResource1 *res = NULL;
      HRESULT hr;
      if (FAILED(d->dev->lpVtbl->CreateTexture2D(d->dev, &desc, NULL,
            &d->out[o])))
         goto end;
      if (FAILED(d->out[o]->lpVtbl->QueryInterface(d->out[o],
            &d3d11vd_iid_dxgi_resource1, (void**)&res)))
         goto end;
      hr = res->lpVtbl->CreateSharedHandle(res, NULL,
            DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, NULL,
            &d->out_handle[o]);
      D3D11VD_RELEASE(res);
      if (     FAILED(hr)
            || !d3d11vd_shared_fence(dev5, &d->out_done[o],
                  &d->out_done_handle[o]))
         goto end;
      retro_atomic_store_release_int(&d->out_abandoned[o], 0);
   }
   ok = d3d11vd_shared_fence(dev5, &d->ready, &d->ready_handle);
end:
   D3D11VD_RELEASE(dev5);
   return ok;
}

static void *d3d11vd_open(void *user, const rh264_hw_sps_t *sps,
      unsigned slots)
{
   settings_t *settings = config_get_ptr();
   unsigned min_targets = 0;
   d3d11vd_dec_t *d;
   (void)user;

   if (     !d3d11vd.up || slots > D3D11VD_MAX_SLOTS
         || (settings && settings->uints.video_hw_decode
               == VIDEO_HW_DECODE_OFF))
      return NULL;
   d3d11vd_grave_reap(false);
   if (!(d = (d3d11vd_dec_t*)calloc(1, sizeof(*d))))
      return NULL;
   d->slots     = slots;
   d->coded_w   = (unsigned)sps->width_in_mbs  * 16;
   d->coded_h   = (unsigned)sps->height_in_mbs * 16;
   d->mbs       = (unsigned)(sps->width_in_mbs * sps->height_in_mbs);
   d->visible_w = (unsigned)sps->width;
   d->visible_h = (unsigned)sps->height;
   d->crop_x    = (unsigned)sps->crop_x;
   d->crop_y    = (unsigned)sps->crop_y;
   d->gen       = (unsigned)retro_atomic_fetch_add_int(&d3d11vd.serial, 1)
      + 1;
   /* NV12 copies go by whole chroma samples */
   if (     (d->visible_w | d->visible_h | d->crop_x | d->crop_y) & 1
         || !d->visible_w || !d->visible_h)
      goto fail;

   if (!d3d11vd_make_device(d))
   {
      RARCH_LOG("[D3D11] Video decode: no video device that shares NV12 textures; decoding on the CPU.\n");
      goto fail;
   }
   if (!d3d11vd_make_decoder(d, &min_targets))
   {
      RARCH_LOG("[D3D11] Video decode: no decoder for H.264 %d level %d %ux%u; decoding on the CPU.\n",
            sps->profile_idc, sps->level_idc, d->coded_w, d->coded_h);
      goto fail;
   }
   if (     !d3d11vd_make_dpb(d, min_targets)
         || !d3d11vd_make_outputs(d))
   {
      RARCH_WARN("[D3D11] Video decode: could not set up a %ux%u decoder; decoding on the CPU.\n",
            d->coded_w, d->coded_h);
      goto fail;
   }
   retro_atomic_fetch_add_int(&d3d11vd.live, 1);
   RARCH_LOG("[D3D11] Video decode: H.264 %ux%u on the GPU, %u slots.\n",
         d->visible_w, d->visible_h, slots);
   return d;

fail:
   d3d11vd_destroy(d);
   return NULL;
}

/* ---- decode ---- */

/* @size bytes of @src into the decoder's buffer of @type, the rest of
 * @padded zeroed. */
static bool d3d11vd_fill(d3d11vd_dec_t *d,
      D3D11_VIDEO_DECODER_BUFFER_TYPE type, const void *src, UINT size,
      UINT padded)
{
   UINT cap  = 0;
   void *ptr = NULL;
   if (FAILED(d->vctx->lpVtbl->GetDecoderBuffer(d->vctx, d->decoder, type,
         &cap, &ptr)))
      return false;
   if (cap >= padded && ptr)
   {
      memcpy(ptr, src, size);
      memset((uint8_t*)ptr + size, 0, padded - size);
   }
   d->vctx->lpVtbl->ReleaseDecoderBuffer(d->vctx, d->decoder, type);
   return cap >= padded && ptr;
}

static void d3d11vd_desc(D3D11_VIDEO_DECODER_BUFFER_DESC *b,
      D3D11_VIDEO_DECODER_BUFFER_TYPE type, UINT size, UINT mbs)
{
   memset(b, 0, sizeof(*b));
   b->BufferType     = type;
   b->DataSize       = size;
   b->NumMBsInBuffer = mbs;
}

static int d3d11vd_decode(void *dec, const rh264_hw_pic_t *pic)
{
   d3d11vd_dec_t *d = (d3d11vd_dec_t*)dec;
   D3D11_VIDEO_DECODER_BUFFER_DESC bufs[4];
   UINT cap = 0, padded;
   void *ptr = NULL;
   HRESULT hr;
   unsigned i;
   bool ok;

   if (     pic->slot < 0 || (unsigned)pic->slot >= d->slots
         || !pic->num_slices || pic->num_slices > D3D11VD_MAX_SLICES
         || pic->num_refs > 16 || pic->size > 0x7fffff00u)
      return -1;
   for (i = 0; i < pic->num_refs; i++)
      if (pic->refs[i].slot < 0 || (unsigned)pic->refs[i].slot >= d->slots)
         return -1;

   hr = d->vctx->lpVtbl->DecoderBeginFrame(d->vctx, d->decoder,
         d->view[pic->slot], 0, NULL);
   if (hr == E_PENDING)
      return 1;
   if (FAILED(hr))
      return -1;

   dxva_h264_pic_params(d->pp, pic, ++d->feedback);
   dxva_h264_qmatrix(d->qm, pic, D3D11VD_QM_RASTER);

   /* the bitstream to a multiple of 128 bytes where it fits, the
    * padding the last slice's */
   padded = ((UINT)pic->size + 127u) & ~127u;
   if (     SUCCEEDED(d->vctx->lpVtbl->GetDecoderBuffer(d->vctx, d->decoder,
               D3D11_VIDEO_DECODER_BUFFER_BITSTREAM, &cap, &ptr)))
   {
      if (cap < padded)
         padded = (UINT)pic->size;
      ok = ptr && cap >= padded;
      if (ok)
      {
         memcpy(ptr, pic->data, pic->size);
         memset((uint8_t*)ptr + pic->size, 0, padded - pic->size);
      }
      d->vctx->lpVtbl->ReleaseDecoderBuffer(d->vctx, d->decoder,
            D3D11_VIDEO_DECODER_BUFFER_BITSTREAM);
   }
   else
      ok = false;
   for (i = 0; ok && i < pic->num_slices; i++)
   {
      uint32_t at  = pic->slice_offsets[i];
      uint32_t end = (i + 1 < pic->num_slices)
         ? pic->slice_offsets[i + 1] : (uint32_t)padded;
      dxva_h264_slice(d->sc + i * DXVA_H264_SLICE_SIZE, at, end - at);
   }

   ok = ok
      && d3d11vd_fill(d, D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS,
            d->pp, DXVA_H264_PP_SIZE, DXVA_H264_PP_SIZE)
      && d3d11vd_fill(d, D3D11_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX,
            d->qm, DXVA_H264_QM_SIZE, DXVA_H264_QM_SIZE)
      && d3d11vd_fill(d, D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL,
            d->sc, pic->num_slices * DXVA_H264_SLICE_SIZE,
            pic->num_slices * DXVA_H264_SLICE_SIZE);
   if (ok)
   {
      d3d11vd_desc(&bufs[0], D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS,
            DXVA_H264_PP_SIZE, 0);
      d3d11vd_desc(&bufs[1],
            D3D11_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX,
            DXVA_H264_QM_SIZE, 0);
      d3d11vd_desc(&bufs[2], D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL,
            pic->num_slices * DXVA_H264_SLICE_SIZE, d->mbs);
      d3d11vd_desc(&bufs[3], D3D11_VIDEO_DECODER_BUFFER_BITSTREAM,
            padded, d->mbs);
      ok = SUCCEEDED(d->vctx->lpVtbl->SubmitDecoderBuffers(d->vctx,
               d->decoder, 4, bufs));
   }
   hr = d->vctx->lpVtbl->DecoderEndFrame(d->vctx, d->decoder);
   return (ok && SUCCEEDED(hr)) ? 0 : -1;
}

/* ---- output ---- */

/* The visible window of @slot into a shared texture of its own. */
static int d3d11vd_output(void *dec, unsigned slot, video_hw_frame_t *out)
{
   d3d11vd_dec_t *d = (d3d11vd_dec_t*)dec;
   D3D11_BOX box;
   unsigned o, n;
   int status;

   if (slot >= d->slots)
      return -1;
   for (n = 0; n < D3D11VD_OUTS; n++)
   {
      status = d3d11vd_out_status(d, (d->out_next + n) % D3D11VD_OUTS);
      if (status < 0)
         return -1;
      if (!status)
         break;
   }
   if (n == D3D11VD_OUTS)
      return 1;
   o = (d->out_next + n) % D3D11VD_OUTS;
   box.left   = d->crop_x;
   box.top    = d->crop_y;
   box.front  = 0;
   box.right  = d->crop_x + d->visible_w;
   box.bottom = d->crop_y + d->visible_h;
   box.back   = 1;
   d->ctx->lpVtbl->CopySubresourceRegion(d->ctx, (ID3D11Resource*)d->out[o],
         0, 0, 0, 0, (ID3D11Resource*)d->dpb, slot, &box);
   if (FAILED(d->ctx4->lpVtbl->Signal(d->ctx4, d->ready,
         d->ready_value + 1)))
      return -1;
   d->ready_value++;
   d->ctx->lpVtbl->Flush(d->ctx);
   d->out_next = (o + 1) % D3D11VD_OUTS;

   memset(out, 0, sizeof(*out));
   out->image      = (uint64_t)(uintptr_t)d->out_handle[o];
   out->wait       = (uint64_t)(uintptr_t)d->ready_handle;
   out->wait_value = d->ready_value;
   out->done       = (uint64_t)(uintptr_t)d->out_done_handle[o];
   out->done_value = ++d->out_value[o];
   out->abandon    = d3d11vd_abandon;
   out->owner      = d;
   out->width      = d->visible_w;
   out->height     = d->visible_h;
   out->layout     = d->gen;
   return 0;
}

static int d3d11vd_readback(void *dec, unsigned slot, uint8_t *planes[3])
{
   d3d11vd_dec_t *d = (d3d11vd_dec_t*)dec;
   D3D11_MAPPED_SUBRESOURCE map;
   const uint8_t *uv;
   unsigned x, y, cw = d->visible_w / 2, ch = d->visible_h / 2;
   if (slot >= d->slots)
      return -1;
   if (!d->readback)
   {
      D3D11_TEXTURE2D_DESC desc;
      d->dpb->lpVtbl->GetDesc(d->dpb, &desc);
      desc.ArraySize = 1;
      desc.Usage = D3D11_USAGE_STAGING;
      desc.BindFlags = 0;
      desc.MiscFlags = 0;
      desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      if (FAILED(d->dev->lpVtbl->CreateTexture2D(d->dev, &desc, NULL, &d->readback)))
         return -1;
   }
   d->ctx->lpVtbl->CopySubresourceRegion(d->ctx, (ID3D11Resource*)d->readback,
         0, 0, 0, 0, (ID3D11Resource*)d->dpb, slot, NULL);
   if (FAILED(d->ctx->lpVtbl->Map(d->ctx, (ID3D11Resource*)d->readback,
         0, D3D11_MAP_READ, 0, &map)))
      return -1;
   for (y = 0; y < d->visible_h; y++)
      memcpy(planes[0] + y * d->visible_w,
            (const uint8_t*)map.pData + (y + d->crop_y) * map.RowPitch + d->crop_x,
            d->visible_w);
   uv = (const uint8_t*)map.pData + d->coded_h * map.RowPitch;
   for (y = 0; y < ch; y++)
      for (x = 0; x < cw; x++)
      {
         size_t off = (y + d->crop_y / 2) * map.RowPitch + d->crop_x + x * 2;
         planes[1][y * cw + x] = uv[off];
         planes[2][y * cw + x] = uv[off + 1];
      }
   d->ctx->lpVtbl->Unmap(d->ctx, (ID3D11Resource*)d->readback, 0);
   return 0;
}

static void d3d11vd_close(void *dec)
{
   d3d11vd_dec_t *d = (d3d11vd_dec_t*)dec;
   if (!d)
      return;
   /* the handles stay good until every frame is read */
   if (d3d11vd_settled(d))
      d3d11vd_destroy(d);
   else
      d3d11vd_grave_push(d);
   retro_atomic_fetch_add_int(&d3d11vd.live, -1);
   d3d11vd_grave_reap(false);
}

/* ---- the backend ---- */

bool d3d11_video_decode_init_adapter(void *adapter)
{
   IDXGIAdapter *a = (IDXGIAdapter*)adapter;

   if (d3d11vd.up || !a)
      return false;
   /* the driver may be D3D12's: d3d11.dll is ours to load */
   if (     !(d3d11vd.dll = LoadLibraryA("d3d11.dll"))
         || !(d3d11vd.create = (PFN_D3D11_CREATE_DEVICE)(void (*)(void))
               GetProcAddress(d3d11vd.dll, "D3D11CreateDevice")))
   {
      if (d3d11vd.dll)
         FreeLibrary(d3d11vd.dll);
      d3d11vd.dll = NULL;
      return false;
   }
   a->lpVtbl->AddRef(a);
   d3d11vd.adapter = a;
   retro_atomic_store_release_int(&d3d11vd.live, 0);
   retro_atomic_store_release_int(&d3d11vd.serial, 0);
   retro_atomic_store_release_ptr(&d3d11vd.grave, NULL);
   d3d11vd.ops.user   = NULL;
   d3d11vd.ops.open   = d3d11vd_open;
   d3d11vd.ops.decode = d3d11vd_decode;
   d3d11vd.ops.output = d3d11vd_output;
   d3d11vd.ops.close  = d3d11vd_close;
   d3d11vd.ops.readback = d3d11vd_readback;
   d3d11vd.up         = true;
   rh264_set_hw_backend(&d3d11vd.ops);
   return true;
}

bool d3d11_video_decode_init(void *device)
{
   ID3D11Device *dev     = (ID3D11Device*)device;
   IDXGIDevice  *dxgi    = NULL;
   IDXGIAdapter *adapter = NULL;
   bool ok;

   if (     d3d11vd.up || !dev
         || FAILED(dev->lpVtbl->QueryInterface(dev, &d3d11vd_iid_dxgi_device,
               (void**)&dxgi)))
      return false;
   dxgi->lpVtbl->GetAdapter(dxgi, &adapter);
   D3D11VD_RELEASE(dxgi);
   ok = d3d11_video_decode_init_adapter(adapter);
   D3D11VD_RELEASE(adapter);
   return ok;
}

void d3d11_video_decode_deinit(void)
{
   if (!d3d11vd.up)
      return;
   rh264_set_hw_backend(NULL);
   if (retro_atomic_load_acquire_int(&d3d11vd.live))
      RARCH_ERR("[D3D11] Video decode: %d decoder(s) still open at teardown.\n",
            retro_atomic_load_acquire_int(&d3d11vd.live));
   d3d11vd_grave_reap(true);
   D3D11VD_RELEASE(d3d11vd.adapter);
   if (d3d11vd.dll)
      FreeLibrary(d3d11vd.dll);
   d3d11vd.dll = NULL;
   d3d11vd.up  = false;
}
