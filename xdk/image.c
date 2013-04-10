/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2013 - Daniel De Matteis
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "../gfx/image.h"
#include "xdk_d3d.h"

bool texture_image_load(const char *path, struct texture_image *out_img)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   D3DXIMAGE_INFO m_imageInfo;

   out_img->pixels      = NULL;
   out_img->vertex_buf  = NULL;

   HRESULT ret = D3DXCreateTextureFromFileExA(d3d->d3d_render_device,
         path, D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0, D3DFMT_A8R8G8B8,
         D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_DEFAULT, 0, &m_imageInfo, NULL,
         &out_img->pixels);

   if(FAILED(ret))
   {
      RARCH_ERR("Error occurred during D3DXCreateTextureFromFileExA().\n");
      return false;
   }

   // create a vertex buffer for the quad that will display the texture
   ret = d3d->d3d_render_device->CreateVertexBuffer(4 * sizeof(DrawVerticeFormats),
         D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &out_img->vertex_buf);

   if (FAILED(ret))
   {
      RARCH_ERR("Error occurred during CreateVertexBuffer().\n");
      out_img->pixels->Release();
      return false;
   }

   out_img->width = m_imageInfo.Width;
   out_img->height = m_imageInfo.Height;

   return true;
}

bool texture_image_render(struct texture_image *out_img)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   if (out_img->pixels == NULL || out_img->vertex_buf == NULL)
      return false;

   int x = out_img->x;
   int y = out_img->y;
   int w = out_img->width;
   int h = out_img->height;

   float fX = static_cast<float>(x);
   float fY = static_cast<float>(y);

   // create the new vertices
   DrawVerticeFormats newVerts[] =
   {
      // x,           y,              z,     color, u ,v
      {fX,            fY,             0.0f,  0,     0, 0},
      {fX + w,        fY,             0.0f,  0,     1, 0},
      {fX + w,        fY + h,         0.0f,  0,     1, 1},
      {fX,            fY + h,         0.0f,  0,     0, 1}
   };

   // load the existing vertices
   DrawVerticeFormats *pCurVerts;

   HRESULT ret = out_img->vertex_buf->Lock(0, 0, (unsigned char**)&pCurVerts, 0);

   if (FAILED(ret))
   {
      RARCH_ERR("Error occurred during m_pVertexBuffer->Lock().\n");
      return false;
   }

   // copy the new verts over the old verts
   memcpy(pCurVerts, newVerts, 4 * sizeof(DrawVerticeFormats));

   out_img->vertex_buf->Unlock();

   d3d->d3d_render_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
   d3d->d3d_render_device->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
   d3d->d3d_render_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

   // also blend the texture with the set alpha value
   d3d->d3d_render_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
   d3d->d3d_render_device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
   d3d->d3d_render_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TEXTURE);

   // draw the quad
   d3d->d3d_render_device->SetTexture(0, out_img->pixels);
   d3d->d3d_render_device->SetStreamSource(0, out_img->vertex_buf, sizeof(DrawVerticeFormats));
   d3d->d3d_render_device->SetVertexShader(D3DFVF_CUSTOMVERTEX);
   d3d->d3d_render_device->DrawPrimitive(D3DPT_QUADLIST, 0, 1);

   return true;
}
