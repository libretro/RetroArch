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

#include <formats/rmp4.h>
#include "d3d11_video_decode_test_data.h"

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
   unsigned y;
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
      d.Format           = DXGI_FORMAT_NV12;
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
      f->y[i]  = frame_alloc(f, (size_t)w * h);
      f->uv[i] = frame_alloc(f, (size_t)cw * 2 * ch);
      if (f->y[i] && f->uv[i])
      {
         const uint8_t *p = (const uint8_t*)m.pData;
         for (y = 0; y < h; y++)
            memcpy(f->y[i] + (size_t)y * w, p + (size_t)y * m.RowPitch, w);
         /* the chroma plane follows the luma rows */
         p += (size_t)m.RowPitch * h;
         for (y = 0; y < ch; y++)
            memcpy(f->uv[i] + (size_t)y * cw * 2, p + (size_t)y * m.RowPitch,
                  cw * 2);
      }
   }
   ctx->lpVtbl->Unmap(ctx, (ID3D11Resource*)o->staging, 0);
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
   if (hw)
      on_hw = rh264_video_set_hw(v, 1);
   if (hw && !on_hw)
      ok = 0;
   while (ok && rmp4_read_packet(m, &pkt) == 1)
   {
      if (pkt.track != trk)
         continue;
      if ((r = rh264_video_decode(v, pkt.data, pkt.size)) < 0)
         ok = 0;
      else if (r == 1)
      {
         if (!on_hw)
            take_sw(f, v);
         else if (rh264_video_hw_frame(v, &fr) != 0 || !take_hw(f, &o, &fr))
            ok = 0;
      }
   }
   while (ok && rh264_video_drain(v) == 0)
   {
      if (!on_hw)
         take_sw(f, v);
      else if (rh264_video_hw_frame(v, &fr) != 0 || !take_hw(f, &o, &fr))
         ok = 0;
   }
   rh264_video_close(v);
   rmp4_close(m);
   opened_clear(&o);
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
