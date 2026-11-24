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
   for (i = 0; i < 3; i++)
      D3D9B_FN(views_target_retire)(d3d, &d3d->blur.tex[i]);
   memset(&d3d->blur, 0, sizeof(d3d->blur));
}

/* The glow's pixel shader and the state block it puts the device back
 * with, which the blur draws with too */
static bool D3D9B_FN(bloom_shader)(d3d9_video_t *d3d)
{
   if (d3d->bloom.ps)
      return d3d->bloom.ps_reg >= 0 && d3d->bloom.state;
   return D3D9B_FN(bloom_ps_make)(d3d) && d3d->bloom.ps_reg >= 0
      && SUCCEEDED(IDirect3DDevice9_CreateStateBlock(d3d->dev,
            D3DSBT_ALL, &d3d->bloom.state));
}

/* Whether the small images can be half float */
static D3DFORMAT D3D9B_FN(bloom_format)(d3d9_video_t *d3d)
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
   return format;
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
   if (!D3D9B_FN(bloom_shader)(d3d))
      goto error;
   if (bloom_dims != d3d->bloom.dims)
   {
      D3DFORMAT format = D3D9B_FN(bloom_format)(d3d);
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

static bool D3D9B_FN(blur_rect)(void *data, unsigned video_dims,
      int x, int y, unsigned dims, float alpha, float dim)
{
   static const float uv[4] = {0, 0, 1, 1};
   d3d9_video_t *d3d = (d3d9_video_t*)data;
   menu_bloom_pass_t plan[MENU_BLUR_PASSES];
   LPDIRECT3DSURFACE9 target = NULL, depth = NULL;
   D3DSURFACE_DESC desc;
   D3DVIEWPORT9 viewport;
   video_views_rect_t rect;
   RECT src;
   float params[4];
   unsigned i, blur_dims;
   int x0, y0, x1, y1, c;
   bool ok = false;
   if (!d3d || d3d->blur.failed)
      return false;
   /* The stretch's filter, and the blend factor the composite needs */
   if (!d3d->blur.checked)
   {
      D3DCAPS9 caps;
      d3d->blur.checked = true;
      if (     FAILED(IDirect3DDevice9_GetDeviceCaps(d3d->dev, &caps))
            || !(caps.DestBlendCaps & D3DPBLENDCAPS_BLENDFACTOR)
            || !(caps.PrimitiveMiscCaps & D3DPMISCCAPS_SEPARATEALPHABLEND))
      {
         d3d->blur.failed = true;
         return false;
      }
      d3d->blur.filter = (caps.StretchRectFilterCaps & D3DPTFILTERCAPS_MINFLINEAR)
         ? D3DTEXF_LINEAR : D3DTEXF_NONE;
   }
   if (     FAILED(IDirect3DDevice9_GetRenderTarget(d3d->dev, 0, &target))
         || !target || FAILED(IDirect3DSurface9_GetDesc(target, &desc))
         || desc.MultiSampleType != D3DMULTISAMPLE_NONE)
      goto end;
   /* Clipped to the output and the target; nothing left, nothing to
    * blur */
   x0 = x < 0 ? 0 : x;
   y0 = y < 0 ? 0 : y;
   x1 = x + (int)VIDEO_SCALE_W(dims);
   y1 = y + (int)VIDEO_SCALE_H(dims);
   if (x1 > (int)VIDEO_SCALE_W(video_dims)) x1 = (int)VIDEO_SCALE_W(video_dims);
   if (y1 > (int)VIDEO_SCALE_H(video_dims)) y1 = (int)VIDEO_SCALE_H(video_dims);
   if (x1 > (int)desc.Width)                x1 = (int)desc.Width;
   if (y1 > (int)desc.Height)               y1 = (int)desc.Height;
   if (x1 - x0 < 8 || y1 - y0 < 8)
      goto end;
   blur_dims = menu_blur_dims(VIDEO_SCALE_PACK(x1 - x0, y1 - y0));
   if (!D3D9B_FN(bloom_shader)(d3d))
   {
      d3d->blur.failed = true;
      goto end;
   }
   if (blur_dims != d3d->blur.dims || desc.Format != d3d->blur.copy_format)
   {
      D3DFORMAT format = D3D9B_FN(bloom_format)(d3d);
      d3d->blur.dims = 0;
      for (i = 0; i < 3; i++)
      {
         D3D9B_FN(views_target_retire)(d3d, &d3d->blur.tex[i]);
         if (!D3D9B_FN(views_target_make_format)(d3d, &d3d->blur.tex[i],
                  i == MENU_BLUR_COPY ? MENU_BLUR_COPY_DIMS(blur_dims)
                     : blur_dims,
                  i == MENU_BLUR_COPY ? desc.Format : format))
         {
            d3d->blur.failed = true;
            goto end;
         }
      }
      d3d->blur.dims        = blur_dims;
      d3d->blur.copy_format = desc.Format;
   }
   if (FAILED(IDirect3DStateBlock9_Capture(d3d->bloom.state)))
      goto end;
   IDirect3DDevice9_GetDepthStencilSurface(d3d->dev, &depth);
   IDirect3DDevice9_GetViewport(d3d->dev, &viewport);

   /* What is drawn so far, stretched to half: the copy and the first
    * downscale in one */
   src.left   = x0;
   src.top    = y0;
   src.right  = x1;
   src.bottom = y1;
   if (FAILED(IDirect3DDevice9_StretchRect(d3d->dev, target, &src,
               d3d->blur.tex[MENU_BLUR_COPY].surf, NULL,
               (D3DTEXTUREFILTERTYPE)d3d->blur.filter)))
      goto end;

   IDirect3DDevice9_SetDepthStencilSurface(d3d->dev, NULL);
   IDirect3DDevice9_SetVertexShader(d3d->dev, NULL);
   D3D9B_BIND(d3d);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_ZENABLE, FALSE);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_CULLMODE, D3DCULL_NONE);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_SCISSORTESTENABLE, FALSE);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_ALPHABLENDENABLE, FALSE);
   menu_blur_plan(plan, blur_dims);
   rect.pos  = 0;
   rect.dims = blur_dims;
   for (i = 0; i < MENU_BLUR_PASSES; i++)
   {
      IDirect3DDevice9_SetTexture(d3d->dev, 0, NULL);
      IDirect3DDevice9_SetRenderTarget(d3d->dev, 0,
            d3d->blur.tex[plan[i].dst].surf);
      IDirect3DDevice9_SetPixelShaderConstantF(d3d->dev, d3d->bloom.ps_reg,
            plan[i].params, 1);
      D3D9B_FN(views_sample)(d3d, d3d->blur.tex[plan[i].src].tex,
            D3DTEXF_LINEAR);
      D3D9B_FN(views_quad)(d3d, &rect, uv, 0);
   }

   /* Back over the rect: the gain darkens the blurred image and fades
    * it in, the blend factor darkens what it is faded over -
    * blurred * (1 - dim) * alpha + drawn * (1 - alpha) * (1 - dim) */
   params[0] = params[1] = params[2] = 0.0f;
   params[3] = (1.0f - dim) * alpha;
   c         = (int)((1.0f - (1.0f - alpha) * (1.0f - dim)) * 255.0f + 0.5f);
   IDirect3DDevice9_SetTexture(d3d->dev, 0, NULL);
   IDirect3DDevice9_SetRenderTarget(d3d->dev, 0, target);
   IDirect3DDevice9_SetPixelShaderConstantF(d3d->dev, d3d->bloom.ps_reg,
         params, 1);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_ALPHABLENDENABLE, TRUE);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_SRCBLEND, D3DBLEND_ONE);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_DESTBLEND, D3DBLEND_INVBLENDFACTOR);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_SRCBLENDALPHA, D3DBLEND_ZERO);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_DESTBLENDALPHA, D3DBLEND_ONE);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_BLENDFACTOR,
         D3DCOLOR_ARGB(c, c, c, c));
   D3D9B_FN(views_sample)(d3d, d3d->blur.tex[0].tex, D3DTEXF_LINEAR);
   rect.pos  = VIDEO_POS_PACK(x0, y0);
   rect.dims = VIDEO_SCALE_PACK(x1 - x0, y1 - y0);
   D3D9B_FN(views_quad)(d3d, &rect, uv, 0);

   IDirect3DDevice9_SetTexture(d3d->dev, 0, NULL);
   IDirect3DDevice9_SetDepthStencilSurface(d3d->dev, depth);
   IDirect3DStateBlock9_Apply(d3d->bloom.state);
   IDirect3DDevice9_SetViewport(d3d->dev, &viewport);
   ok = true;
end:
   if (depth)
      IDirect3DSurface9_Release(depth);
   if (target)
      IDirect3DSurface9_Release(target);
   return ok;
}

#undef D3D9B_FN
#undef D3D9B_FREE
#undef D3D9B_BIND
