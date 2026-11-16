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

static unsigned D3D9B_FN(bloom_begin)(void *data, unsigned dims,
      bool defer)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;
   unsigned i, bloom_dims = menu_bloom_dims(dims);
   D3DVIEWPORT9 vp;
   (void)defer;
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
      D3DFORMAT format = D3DFMT_A8R8G8B8;
      D3DDEVICE_CREATION_PARAMETERS creation;
      D3DDISPLAYMODE mode;
      LPDIRECT3D9 api = NULL;
      if (SUCCEEDED(IDirect3DDevice9_GetDirect3D(d3d->dev, &api)))
      {
         if (SUCCEEDED(IDirect3DDevice9_GetCreationParameters(d3d->dev, &creation))
               && SUCCEEDED(IDirect3DDevice9_GetDisplayMode(d3d->dev, 0, &mode))
               && SUCCEEDED(IDirect3D9_CheckDeviceFormat(api, creation.AdapterOrdinal,
                     creation.DeviceType, mode.Format, D3DUSAGE_RENDERTARGET
                        | D3DUSAGE_QUERY_FILTER | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
                     D3DRTYPE_TEXTURE, D3DFMT_A16B16G16R16F)))
            format = D3DFMT_A16B16G16R16F;
         IDirect3D9_Release(api);
      }
      for (i = 0; i < 2; i++)
      {
         D3D9B_FN(views_target_retire)(d3d, &d3d->bloom.tex[i]);
         if (!D3D9B_FN(views_target_make_format)(d3d, &d3d->bloom.tex[i], bloom_dims, format))
            goto error;
      }
      d3d->bloom.passes = menu_bloom_plan(d3d->bloom.plan, bloom_dims,
            format == D3DFMT_A16B16G16R16F);
      d3d->bloom.dims   = bloom_dims;
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

static void D3D9B_FN(bloom_end)(void *data, unsigned dims, bool defer)
{
   static const float uv[4] = {0, 0, 1, 1};
   d3d9_video_t *d3d = (d3d9_video_t*)data;
   unsigned i;
   (void)defer;
   IDirect3DDevice9_SetVertexShader(d3d->dev, NULL);
   D3D9B_BIND(d3d);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_ZENABLE, FALSE);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_CULLMODE, D3DCULL_NONE);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_SRCBLENDALPHA, D3DBLEND_ZERO);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_DESTBLENDALPHA, D3DBLEND_ONE);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_SRCBLEND, D3DBLEND_ONE);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_DESTBLEND, D3DBLEND_ONE);
   for (i = 0; i < d3d->bloom.passes; i++)
   {
      const menu_bloom_pass_t *pass = &d3d->bloom.plan[i];
      bool output                   = pass->dst == MENU_BLOOM_OUTPUT;
      video_views_rect_t rect;
      rect.pos  = 0;
      rect.dims = output ? dims : d3d->bloom.dims;
      IDirect3DDevice9_SetTexture(d3d->dev, 0, NULL);
      IDirect3DDevice9_SetRenderTarget(d3d->dev, 0,
            output ? d3d->bloom.target : d3d->bloom.tex[pass->dst].surf);
      IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_ALPHABLENDENABLE, pass->add);
      IDirect3DDevice9_SetPixelShaderConstantF(d3d->dev, d3d->bloom.ps_reg,
            pass->params, 1);
      D3D9B_FN(views_sample)(d3d, d3d->bloom.tex[pass->src].tex, D3DTEXF_LINEAR);
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
