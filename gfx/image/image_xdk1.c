/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2014 - Daniel De Matteis
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

#include "image.h"
#include "../../xdk/xdk_d3d.h"

bool texture_image_load(struct texture_image *out_img, const char *path)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   D3DXIMAGE_INFO m_imageInfo;

   out_img->pixels      = NULL;
   out_img->vertex_buf  = NULL;

   if (FAILED(D3DXCreateTextureFromFileExA(d3d->dev,
         path, D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0, D3DFMT_A8R8G8B8,
         D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_DEFAULT, 0, &m_imageInfo, NULL,
         &out_img->pixels)))
   {
      RARCH_ERR("Error occurred during D3DXCreateTextureFromFileExA().\n");
      return false;
   }

   // create a vertex buffer for the quad that will display the texture
   if (FAILED(D3DDevice_CreateVertexBuffers(d3d->dev, 4 * sizeof(Vertex), D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &out_img->vertex_buf, NULL)))
   {
      RARCH_ERR("Error occurred during CreateVertexBuffer().\n");
      out_img->pixels->Release();
      return false;
   }

   out_img->width = m_imageInfo.Width;
   out_img->height = m_imageInfo.Height;

   return true;
}

void texture_image_free(struct texture_image *img)
{
   if (!img)
      return;

   if (img->vertex_buf)
      img->vertex_buf->Release();
   if (img->pixels)
      img->pixels->Release();
   memset(img, 0, sizeof(*img));
}
