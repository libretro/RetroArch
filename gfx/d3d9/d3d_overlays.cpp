static void d3d_overlay_render(void *data, overlay_t *overlay)
{
   void *verts;
   unsigned i;
   d3d_video_t *d3d = (d3d_video_t*)data;

   if (!overlay || !overlay->tex)
      return;

   struct overlay_vertex
   {
      float x, y, z;
      float u, v;
      float r, g, b, a;
   } vert[4];

   if (!overlay->vert_buf)
   {
      D3DDevice_CreateVertexBuffers(d3d->dev, sizeof(vert), 
         d3d->dev->GetSoftwareVertexProcessing() ? 
         D3DUSAGE_SOFTWAREPROCESSING : 0, 0, D3DPOOL_MANAGED,
         &overlay->vert_buf, NULL);
   }

   for (i = 0; i < 4; i++)
   {
      vert[i].z = 0.5f;
      vert[i].r = vert[i].g = vert[i].b = 1.0f;
      vert[i].a = overlay->alpha_mod;
   }

   float overlay_width = d3d->final_viewport.Width;
   float overlay_height = d3d->final_viewport.Height;

   vert[0].x = overlay->vert_coords.x * overlay_width;
   vert[1].x = (overlay->vert_coords.x + overlay->vert_coords.w)
      * overlay_width;
   vert[2].x = overlay->vert_coords.x * overlay_width;
   vert[3].x = (overlay->vert_coords.x + overlay->vert_coords.w)
      * overlay_width;
   vert[0].y = overlay->vert_coords.y * overlay_height;
   vert[1].y = overlay->vert_coords.y * overlay_height;
   vert[2].y = (overlay->vert_coords.y + overlay->vert_coords.h)
      * overlay_height;
   vert[3].y = (overlay->vert_coords.y + overlay->vert_coords.h)
      * overlay_height;

   vert[0].u = overlay->tex_coords.x;
   vert[1].u = overlay->tex_coords.x + overlay->tex_coords.w;
   vert[2].u = overlay->tex_coords.x;
   vert[3].u = overlay->tex_coords.x + overlay->tex_coords.w;
   vert[0].v = overlay->tex_coords.y;
   vert[1].v = overlay->tex_coords.y;
   vert[2].v = overlay->tex_coords.y + overlay->tex_coords.h;
   vert[3].v = overlay->tex_coords.y + overlay->tex_coords.h;

   /* Align texels and vertices. */
   for (i = 0; i < 4; i++)
   {
      vert[i].x -= 0.5f;
      vert[i].y += 0.5f;
   }

   overlay->vert_buf->Lock(0, sizeof(vert), &verts, 0);
   memcpy(verts, vert, sizeof(vert));
   overlay->vert_buf->Unlock();

   // enable alpha
   d3d->dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
   d3d->dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
   d3d->dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

#ifndef _XBOX1
   // set vertex decl for overlay
   D3DVERTEXELEMENT vElems[4] = {
      {0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,
         D3DDECLUSAGE_POSITION, 0},
      {0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,
         D3DDECLUSAGE_TEXCOORD, 0},
      {0, 20, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT,
         D3DDECLUSAGE_COLOR, 0},
      D3DDECL_END()
   };
   LPDIRECT3DVERTEXDECLARATION vertex_decl;
   d3d->dev->CreateVertexDeclaration(vElems, &vertex_decl);
   d3d->dev->SetVertexDeclaration(vertex_decl);
   vertex_decl->Release();
#endif

   D3DDevice_SetStreamSources(d3d->dev, 0, overlay->vert_buf,
         0, sizeof(overlay_vertex));

   if (overlay->fullscreen)
   {
      /* Set viewport to full window. */
      D3DVIEWPORT vp_full;
      vp_full.X = 0;
      vp_full.Y = 0;
      vp_full.Width = d3d->screen_width;
      vp_full.Height = d3d->screen_height;
      vp_full.MinZ = 0.0f;
      vp_full.MaxZ = 1.0f;
      d3d->dev->SetViewport(&vp_full);
   }

   /* Render overlay. */
   d3d->dev->SetTexture(0, overlay->tex);
   D3DDevice_SetSamplerState_AddressU(d3d->dev, 0, D3DTADDRESS_BORDER);
   D3DDevice_SetSamplerState_AddressV(d3d->dev, 0, D3DTADDRESS_BORDER);
   D3DDevice_SetSamplerState_MinFilter(d3d->dev, 0, D3DTEXF_LINEAR);
   D3DDevice_SetSamplerState_MagFilter(d3d->dev, 0, D3DTEXF_LINEAR);
   D3DDevice_DrawPrimitive(d3d->dev, D3DPT_TRIANGLESTRIP, 0, 2);

   /* Restore previous state. */
   d3d->dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
   d3d->dev->SetViewport(&d3d->final_viewport);
}

static void d3d_free_overlay(void *data, overlay_t *overlay)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   if (overlay->tex)
      overlay->tex->Release();
   if (overlay->vert_buf)
      overlay->vert_buf->Release();
}

static void d3d_free_overlays(void *data)
{
   unsigned i;
   d3d_video_t *d3d = (d3d_video_t*)data;

   for (i = 0; i < d3d->overlays.size(); i++)
      d3d_free_overlay(d3d, &d3d->overlays[i]);
   d3d->overlays.clear();
}

static void d3d_overlay_tex_geom(void *data,
      unsigned index,
      float x, float y,
      float w, float h)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   d3d->overlays[index].tex_coords.x = x;
   d3d->overlays[index].tex_coords.y = y;
   d3d->overlays[index].tex_coords.w = w;
   d3d->overlays[index].tex_coords.h = h;
}

static void d3d_overlay_vertex_geom(void *data,
      unsigned index,
      float x, float y,
      float w, float h)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   y = 1.0f - y;
   h = -h;
   d3d->overlays[index].vert_coords.x = x;
   d3d->overlays[index].vert_coords.y = y;
   d3d->overlays[index].vert_coords.w = w;
   d3d->overlays[index].vert_coords.h = h;
}

static bool d3d_overlay_load(void *data,
      const texture_image *images, unsigned num_images)
{
   unsigned i, y;
   d3d_video_t *d3d = (d3d_video_t*)data;
   d3d_free_overlays(data);
   d3d->overlays.resize(num_images);

   for (i = 0; i < num_images; i++)
   {
      unsigned width = images[i].width;
      unsigned height = images[i].height;
      overlay_t &overlay = d3d->overlays[i];

      if (FAILED(d3d->dev->CreateTexture(
                  width,
                  height,
                  1,
                  0,
                  D3DFMT_A8R8G8B8,
                  D3DPOOL_MANAGED,
                  &overlay.tex, NULL)))
      {
         RARCH_ERR("[D3D]: Failed to create overlay texture\n");
         return false;
      }

      D3DLOCKED_RECT d3dlr;
      if (SUCCEEDED(overlay.tex->LockRect(0, &d3dlr,
                  NULL, D3DLOCK_NOSYSLOCK)))
      {
         uint32_t *dst = static_cast<uint32_t*>(d3dlr.pBits);
         const uint32_t *src = images[i].pixels;
         unsigned pitch = d3dlr.Pitch >> 2;
         for (y = 0; y < height; y++, dst += pitch, src += width)
            memcpy(dst, src, width << 2);
         overlay.tex->UnlockRect(0);
      }

      overlay.tex_w = width;
      overlay.tex_h = height;

      /* Default. Stretch to whole screen. */
      d3d_overlay_tex_geom(d3d, i, 0, 0, 1, 1);

      d3d_overlay_vertex_geom(d3d, i, 0, 0, 1, 1);
   }

   return true;
}

static void d3d_overlay_enable(void *data, bool state)
{
   unsigned i;
   d3d_video_t *d3d = (d3d_video_t*)data;

   for (i = 0; i < d3d->overlays.size(); i++)
      d3d->overlays_enabled = state;

   if (d3d && d3d->ctx_driver && d3d->ctx_driver->show_mouse)
      d3d->ctx_driver->show_mouse(d3d, state);
}

static void d3d_overlay_full_screen(void *data, bool enable)
{
   unsigned i;
   d3d_video_t *d3d = (d3d_video_t*)data;

   for (i = 0; i < d3d->overlays.size(); i++)
      d3d->overlays[i].fullscreen = enable;
}

static void d3d_overlay_set_alpha(void *data, unsigned index, float mod)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   d3d->overlays[index].alpha_mod = mod;
}

static const video_overlay_interface_t d3d_overlay_interface = {
   d3d_overlay_enable,
   d3d_overlay_load,
   d3d_overlay_tex_geom,
   d3d_overlay_vertex_geom,
   d3d_overlay_full_screen,
   d3d_overlay_set_alpha,
};

static void d3d_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface)
{
   (void)data;
   *iface = &d3d_overlay_interface;
}
