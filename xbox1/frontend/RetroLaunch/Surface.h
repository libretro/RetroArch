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

#ifndef _D3D_SURFACE_H_
#define _D3D_SURFACE_H_

#include "../../../xdk/xdk_defines.h"

typedef struct
{
   LPDIRECT3DTEXTURE m_pTexture;
	LPDIRECT3DVERTEXBUFFER m_pVertexBuffer;
	int m_x;
   int m_y;
	D3DXIMAGE_INFO m_imageInfo;
	unsigned char m_byR;
   unsigned char m_byG;
   unsigned char m_byB;
   bool m_bLoaded;
} d3d_surface_t;

bool d3d_surface_new(d3d_surface_t *surface, const char *filename);
void d3d_surface_free(d3d_surface_t *surface);
bool d3d_surface_render(d3d_surface_t *surface, int x, int y, int32_t w, int32_t h);

#endif