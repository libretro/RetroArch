//forward decls
static void renderchain_set_mvp(void *data, unsigned vp_width, unsigned vp_height, unsigned rotation);
static void renderchain_blit_to_texture(void *data, const void *frame,
   unsigned width, unsigned height, unsigned pitch);
static void renderchain_set_mvp(void *data, unsigned vp_width, unsigned vp_height, unsigned rotation);
static void renderchain_set_vertices(void *data, unsigned pass, unsigned width, unsigned height);

#if defined(_XBOX360)
#define D3DTexture_Blit(d3d, desc, d3dlr, frame, width, height, pitch) \
   d3d->tex->GetLevelDesc(0, &desc); \
   XGCopySurface(d3dlr.pBits, d3dlr.Pitch, width, height, desc.Format, NULL, frame, pitch, desc.Format, NULL, 0, 0)
#else
#define D3DTexture_Blit(d3d, desc, d3dlr, frame, width, height, pitch) \
   for (unsigned y = 0; y < height; y++) \
   { \
      const uint8_t *in = (const uint8_t*)frame + y * pitch; \
      uint8_t *out = (uint8_t*)d3dlr.pBits + y * d3dlr.Pitch; \
      memcpy(out, in, width * d3d->pixel_size); \
   }
#endif

static void renderchain_clear(void *data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   if (d3d->tex)
      d3d->tex->Release();
   d3d->tex = NULL;
   if (d3d->vertex_buf)
      d3d->vertex_buf->Release();
   d3d->vertex_buf = NULL;

#ifdef _XBOX360
   if (d3d->vertex_decl)
      d3d->vertex_decl->Release();
   d3d->vertex_decl = NULL;
#endif
}

static void renderchain_free(void *data)
{
   d3d_video_t *chain = (d3d_video_t*)data;

   renderchain_clear(chain);
   //renderchain_destroy_stock_shader(chain);
#ifndef DONT_HAVE_STATE_TRACKER
   if (chain->tracker)
      state_tracker_free(chain->tracker);
#endif
}

bool renderchain_init_shader_fvf(void *data, void *pass_)
{
   d3d_video_t *chain = (d3d_video_t*)data;
   d3d_video_t *pass = (d3d_video_t*)data;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)chain->dev;

#if defined(_XBOX360)
   static const D3DVERTEXELEMENT VertexElements[] =
   {
      { 0, 0 * sizeof(float), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
      { 0, 2 * sizeof(float), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
      D3DDECL_END()
   };

   if (FAILED(d3dr->CreateVertexDeclaration(VertexElements, &pass->vertex_decl)))
      return false;
#endif

   return true;
}

static bool renderchain_create_first_pass(void *data, const video_info_t *info)
{
   HRESULT ret;
   d3d_video_t *chain = (d3d_video_t*)data;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)chain->dev;

   ret = D3DDevice_CreateVertexBuffers(d3dr, 4 * sizeof(Vertex), 
         D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &chain->vertex_buf, NULL);

   if (FAILED(ret))
      return false;

   ret = d3dr->CreateTexture(chain->tex_w, chain->tex_h, 1, 0, info->rgb32 ? D3DFMT_LIN_X8R8G8B8 : D3DFMT_LIN_R5G6B5,
         0, &chain->tex
#ifdef _XBOX360
         , NULL
#endif
         );

   if (FAILED(ret))
      return false;

   D3DDevice_SetSamplerState_AddressU(d3dr, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
   D3DDevice_SetSamplerState_AddressV(d3dr, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
#ifdef _XBOX1
   d3dr->SetRenderState(D3DRS_LIGHTING, FALSE);
#endif
   d3dr->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
   d3dr->SetRenderState(D3DRS_ZENABLE, FALSE);

   if (!renderchain_init_shader_fvf(chain, chain))
      return false;

   return true;
}


static bool renderchain_init(void *data, const video_info_t *info)
{
   d3d_video_t *chain = (d3d_video_t*)data;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)chain->dev;

   chain->pixel_size   = info->rgb32 ? sizeof(uint32_t) : sizeof(uint16_t);

   if (!renderchain_create_first_pass(chain, info))
      return false;

   if (g_extern.console.screen.viewports.custom_vp.width == 0)
      g_extern.console.screen.viewports.custom_vp.width = chain->screen_width;

   if (g_extern.console.screen.viewports.custom_vp.height == 0)
      g_extern.console.screen.viewports.custom_vp.height = chain->screen_height;

   return true;
}

static void renderchain_render_pass(void *data, const void *frame, unsigned width, unsigned height, unsigned pitch, unsigned rotation)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)d3d->dev;

#if defined(_XBOX1)
   d3dr->SetFlickerFilter(g_extern.console.screen.flicker_filter_index);
   d3dr->SetSoftDisplayFilter(g_extern.console.softfilter_enable);
#elif defined(_XBOX360)
   DWORD fetchConstant;
   UINT64 pendingMask3;
#endif

   renderchain_blit_to_texture(d3d, frame, width, height, pitch);
   renderchain_set_vertices(d3d, 1, width, height);

   RD3DDevice_SetTexture(d3dr, 0, d3d->tex);
   RD3DDevice_SetViewport(d3d->dev, &d3d->final_viewport);
   D3DDevice_SetSamplerState_MinFilter(d3dr, 0, g_settings.video.smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
   D3DDevice_SetSamplerState_MagFilter(d3dr, 0, g_settings.video.smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);

#if defined(_XBOX1)
   RD3DDevice_SetVertexShader(d3dr, D3DFVF_XYZ | D3DFVF_TEX1);
#elif defined(_XBOX360)
   D3DDevice_SetVertexDeclaration(d3dr, d3d->vertex_decl);
#endif
   for (unsigned i = 0; i < 4; i++)
   {
      D3DDevice_SetStreamSources(d3dr, i, d3d->vertex_buf, 0, sizeof(Vertex));
   }

   D3DDevice_DrawPrimitive(d3dr, D3DPT_TRIANGLESTRIP, 0, 2);

   g_extern.frame_count++;

   renderchain_set_mvp(d3d, d3d->screen_width, d3d->screen_height, d3d->dev_rotation);
}

static void renderchain_set_vertices(void *data, unsigned pass, unsigned width, unsigned height)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   if (d3d->last_width != width || d3d->last_height != height)
   {
      d3d->last_width = width;
      d3d->last_height = height;

      Vertex vert[4];
      float tex_w = width;
      float tex_h = height;
#ifdef _XBOX360
      tex_w /= ((float)d3d->tex_w);
      tex_h /= ((float)d3d->tex_h);
#endif

      vert[0].x = -1.0f;
      vert[1].x =  1.0f;
      vert[2].x = -1.0f;
      vert[3].x =  1.0f;

      vert[0].y = -1.0f;
      vert[1].y = -1.0f;
      vert[2].y =  1.0f;
      vert[3].y =  1.0f;
#if defined(_XBOX1)
      vert[0].z =  1.0f;
      vert[1].z =  1.0f;
      vert[2].z =  1.0f;
      vert[3].z =  1.0f;

      vert[0].rhw = 0.0f;
      vert[1].rhw = tex_w;
      vert[2].rhw = 0.0f;
      vert[3].rhw = tex_w;

      vert[0].u =  tex_h;
      vert[1].u =  tex_h;
      vert[2].u =  0.0f;
      vert[3].u =  0.0f;

      vert[0].v =  0.0f;
      vert[1].v =  0.0f;
      vert[2].v =  0.0f;
      vert[3].v =  0.0f;
#elif defined(_XBOX360)
      vert[0].u =  0.0f;
      vert[1].u =  tex_w;
      vert[2].u =  0.0f;
      vert[3].u =  tex_w;

      vert[0].v =  tex_h;
      vert[1].v =  tex_h;
      vert[2].v =  0.0f;
      vert[3].v =  0.0f;
#endif

      // Align texels and vertices.
      for (unsigned i = 0; i < 4; i++)
      {
         vert[i].x -= 0.5f / ((float)d3d->tex_w);
         vert[i].y += 0.5f / ((float)d3d->tex_h);
      }

#if defined(_XBOX1)
      BYTE *verts;
#elif defined(_XBOX360)
      void *verts;
#endif
      RD3DVertexBuffer_Lock(d3d->vertex_buf, 0, 0, &verts, 0);
      memcpy(verts, vert, sizeof(vert));
      RD3DVertexBuffer_Unlock(d3d->vertex_buf);
   }

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
#ifdef _XBOX
   if (d3d->shader)
   {
      renderchain_set_mvp(d3d, d3d->screen_width, d3d->screen_height, d3d->dev_rotation);
      if (d3d->shader->use)
         d3d->shader->use(d3d, pass);
      if (d3d->shader->set_params)
         d3d->shader->set_params(d3d, width, height, d3d->tex_w, d3d->tex_h, d3d->screen_width,
               d3d->screen_height, g_extern.frame_count,
               NULL, NULL, NULL, 0);
   }
#endif
#endif
}

static void renderchain_set_mvp(void *data, unsigned vp_width, unsigned vp_height, unsigned rotation)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)d3d->dev;
#if defined(_XBOX360) && defined(HAVE_HLSL)
   hlsl_set_proj_matrix(XMMatrixRotationZ(rotation * (M_PI / 2.0)));
   if (d3d->shader && d3d->shader->set_mvp)
      d3d->shader->set_mvp(d3d, NULL);
#elif defined(_XBOX1)
   D3DXMATRIX p_out, p_rotate, mat;
   D3DXMatrixOrthoOffCenterLH(&mat, 0, vp_width,  vp_height, 0, 0.0f, 1.0f);
   D3DXMatrixIdentity(&p_out);
   D3DXMatrixRotationZ(&p_rotate, rotation * (M_PI / 2.0));

   RD3DDevice_SetTransform(d3dr, D3DTS_WORLD, &p_rotate);
   RD3DDevice_SetTransform(d3dr, D3DTS_VIEW, &p_out);
   RD3DDevice_SetTransform(d3dr, D3DTS_PROJECTION, &p_out);
#endif
}

static void renderchain_blit_to_texture(void *data, const void *frame,
   unsigned width, unsigned height, unsigned pitch)
{
   D3DSURFACE_DESC desc;
   D3DLOCKED_RECT d3dlr;
   d3d_video_t *d3d = (d3d_video_t*)data;

   (void)desc;

   if (d3d->last_width != width || d3d->last_height != height)
   {
      D3DTexture_LockRectClear(d3d, d3d->tex, 0, d3dlr, NULL, D3DLOCK_NOSYSLOCK);
   }

   D3DTexture_LockRect(d3d->tex, 0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK);
   D3DTexture_Blit(d3d, desc, d3dlr, frame, width, height, pitch);
}
