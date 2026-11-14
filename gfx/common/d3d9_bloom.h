#include "menu_bloom.h"

static void D3D9B_FN(bloom_free)(d3d9_video_t *d3d)
{
   unsigned i;
   for (i = 0; i < 2; i++)
      D3D9B_FN(views_target_retire)(d3d, &d3d->bloom.tex[i]);
   if (d3d->bloom.state)
      IDirect3DStateBlock9_Release(d3d->bloom.state);
   if (d3d->bloom.ps)
      D3D9B_FREE(d3d);
   memset(&d3d->bloom, 0, sizeof(d3d->bloom));
}

static unsigned D3D9B_FN(bloom_begin)(void *data, unsigned dims)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;
   unsigned i, bloom_dims = menu_bloom_dims(dims);
   D3DVIEWPORT9 vp;
   if (!d3d || !bloom_dims || d3d->bloom.failed)
      return 0;
   if (!d3d->bloom.ps)
   {
      if (!D3D9B_FN(bloom_ps_make)(d3d) || d3d->bloom.ps_reg < 0
            || FAILED(IDirect3DDevice9_CreateStateBlock(d3d->dev,
                  D3DSBT_ALL, &d3d->bloom.state)))
         goto error;
   }
   if (bloom_dims != d3d->bloom.dims)
   {
      for (i = 0; i < 2; i++)
      {
         D3D9B_FN(views_target_retire)(d3d, &d3d->bloom.tex[i]);
         if (!D3D9B_FN(views_target_make)(d3d, &d3d->bloom.tex[i], bloom_dims))
            goto error;
      }
      d3d->bloom.dims = bloom_dims;
   }
   if (FAILED(IDirect3DStateBlock9_Capture(d3d->bloom.state)))
      return 0;
   IDirect3DDevice9_GetRenderTarget(d3d->dev, 0, &d3d->bloom.target);
   IDirect3DDevice9_GetDepthStencilSurface(d3d->dev, &d3d->bloom.depth);
   IDirect3DDevice9_GetViewport(d3d->dev, &d3d->bloom.viewport);
   IDirect3DDevice9_SetDepthStencilSurface(d3d->dev, NULL);
   IDirect3DDevice9_SetRenderTarget(d3d->dev, 0, d3d->bloom.tex[0].surf);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_SCISSORTESTENABLE, FALSE);
   vp.X = vp.Y = 0;
   vp.Width = VIDEO_SCALE_W(bloom_dims);
   vp.Height = VIDEO_SCALE_H(bloom_dims);
   vp.MinZ = 0.0f;
   vp.MaxZ = 1.0f;
   IDirect3DDevice9_SetViewport(d3d->dev, &vp);
   IDirect3DDevice9_Clear(d3d->dev, 0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
   return bloom_dims;
error:
   d3d->bloom.failed = true;
   return 0;
}

static void D3D9B_FN(bloom_end)(void *data, unsigned dims)
{
   static const float uv[4] = {0, 0, 1, 1};
   d3d9_video_t *d3d = (d3d9_video_t*)data;
   unsigned i;
   IDirect3DDevice9_SetVertexShader(d3d->dev, NULL);
   D3D9B_BIND(d3d);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_ZENABLE, FALSE);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_CULLMODE, D3DCULL_NONE);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_SRCBLENDALPHA, D3DBLEND_ZERO);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_DESTBLENDALPHA, D3DBLEND_ONE);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_SRCBLEND, D3DBLEND_ONE);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_DESTBLEND, D3DBLEND_ONE);
   for (i = 0; i < 3; i++)
   {
      float params[4];
      video_views_rect_t rect;
      rect.pos = 0;
      rect.dims = i == 2 ? dims : d3d->bloom.dims;
      params[0] = i == 0 ? 2.0f / VIDEO_SCALE_W(d3d->bloom.dims) : 0.0f;
      params[1] = i == 1 ? 2.0f / VIDEO_SCALE_H(d3d->bloom.dims) : 0.0f;
      params[2] = i == 0 ? 0.20f : 0.0f;
      params[3] = i == 2 ? 0.75f : 1.0f;
      IDirect3DDevice9_SetTexture(d3d->dev, 0, NULL);
      IDirect3DDevice9_SetRenderTarget(d3d->dev, 0,
            i == 2 ? d3d->bloom.target : d3d->bloom.tex[1 - i].surf);
      IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_ALPHABLENDENABLE, i == 2);
      IDirect3DDevice9_SetPixelShaderConstantF(d3d->dev, d3d->bloom.ps_reg, params, 1);
      D3D9B_FN(views_sample)(d3d, d3d->bloom.tex[i == 1 ? 1 : 0].tex, D3DTEXF_LINEAR);
      D3D9B_FN(views_quad)(d3d, &rect, uv, 0);
   }
   IDirect3DDevice9_SetTexture(d3d->dev, 0, NULL);
   IDirect3DDevice9_SetDepthStencilSurface(d3d->dev, d3d->bloom.depth);
   IDirect3DStateBlock9_Apply(d3d->bloom.state);
   IDirect3DDevice9_SetViewport(d3d->dev, &d3d->bloom.viewport);
   IDirect3DSurface9_Release(d3d->bloom.target);
   d3d->bloom.target = NULL;
   if (d3d->bloom.depth)
      IDirect3DSurface9_Release(d3d->bloom.depth);
   d3d->bloom.depth = NULL;
}

#undef D3D9B_FN
#undef D3D9B_FREE
#undef D3D9B_BIND
