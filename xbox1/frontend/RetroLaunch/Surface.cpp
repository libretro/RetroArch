/* RetroArch - A frontend for libretro.
* Copyright (C) 2010-2012 - Hans-Kristian Arntzen
* Copyright (C) 2011-2012 - Daniel De Matteis
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

#include "Surface.h"

#include "../../xdk_d3d8.h"

bool d3d_surface_new(d3d_surface_t *surface, const char *filename)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

	surface->m_pTexture              = NULL;
	surface->m_pVertexBuffer         = NULL;

	HRESULT ret = D3DXCreateTextureFromFileExA(d3d->d3d_render_device,                // d3d device
	                                         filename,                                // filename
	                                         D3DX_DEFAULT,                            // width
                                            D3DX_DEFAULT,                            // height
	                                         D3DX_DEFAULT,                            // mipmaps
	                                         0,                                       // usage
	                                         D3DFMT_A8R8G8B8,                         // format
	                                         D3DPOOL_MANAGED,                         // memory class
	                                         D3DX_DEFAULT,                            // texture filter
	                                         D3DX_DEFAULT,                            // mipmapping
	                                         0,                                       // colorkey
	                                         &surface->m_imageInfo,                   // image info
	                                         NULL,                                    // pallete
	                                         &surface->m_pTexture);                   // texture

	if (FAILED(ret))
	{
		RARCH_ERR("Error occurred during D3DXCreateTextureFromFileExA().\n");
		return false;
	}

	// create a vertex buffer for the quad that will display the texture
   ret = d3d->d3d_render_device->CreateVertexBuffer(4 * sizeof(DrawVerticeFormats),
      D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &surface->m_pVertexBuffer);

	if (FAILED(ret))
	{
		RARCH_ERR("Error occurred during CreateVertexBuffer().\n");
		surface->m_pTexture->Release();
		return false;
	}

	return true;
}

void d3d_surface_free(d3d_surface_t *surface)
{
	// free the vertex buffer
	if (surface->m_pVertexBuffer)
	{
		surface->m_pVertexBuffer->Release();
		surface->m_pVertexBuffer = NULL;
	}

	// free the texture
	if (surface->m_pTexture)
	{
		surface->m_pTexture->Release();
		surface->m_pTexture = NULL;
	}
}

bool d3d_surface_render(d3d_surface_t *surface, int x, int y, int32_t w, int32_t h)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

	if (surface->m_pTexture == NULL || surface->m_pVertexBuffer == NULL)
		return false;

	float fX = static_cast<float>(x);
	float fY = static_cast<float>(y);

	// create the new vertices
   DrawVerticeFormats newVerts[] =
	{
		// x,           y,              z,	   color, u ,v
		{fX,            fY,             0.0f,  0,     0, 0},
		{fX + w,        fY,             0.0f,  0,     1, 0},
		{fX + w,        fY + h,         0.0f,  0,     1, 1},
		{fX,            fY + h,         0.0f,  0,     0, 1}
	};

	// load the existing vertices
   DrawVerticeFormats *pCurVerts;

	HRESULT ret = surface->m_pVertexBuffer->Lock(0, 0, (unsigned char**)&pCurVerts, 0);

	if (FAILED(ret))
	{
		RARCH_ERR("Error occurred during m_pVertexBuffer->Lock().\n");
		return false;
	}

	// copy the new verts over the old verts
	memcpy(pCurVerts, newVerts, 4 * sizeof(DrawVerticeFormats));

	surface->m_pVertexBuffer->Unlock();

   d3d->d3d_render_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	d3d->d3d_render_device->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
	d3d->d3d_render_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

	// also blend the texture with the set alpha value
	d3d->d3d_render_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	d3d->d3d_render_device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
	d3d->d3d_render_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TEXTURE);

	// draw the quad
	d3d->d3d_render_device->SetTexture(0, surface->m_pTexture);
	d3d->d3d_render_device->SetStreamSource(0, surface->m_pVertexBuffer, sizeof(DrawVerticeFormats));
	d3d->d3d_render_device->SetVertexShader(D3DFVF_CUSTOMVERTEX);
	d3d->d3d_render_device->DrawPrimitive(D3DPT_QUADLIST, 0, 1);

	return true;
}
