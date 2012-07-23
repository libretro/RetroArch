/**
 * RetroLaunch 2012
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version. This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details. You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. To contact the
 * authors: Surreal64 CE Team (http://www.emuxtras.net)
 */

#pragma once

#include "Global.h"

#undef D3DFVF_CUSTOMVERTEX
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1)

typedef struct CustomVertex
{
    float x, y, z;
    dword color;
	float u, v;
	//float rhw;
}CustomVertex;


class CVideo
{
public:
	CVideo(void);
	~CVideo(void);

	bool Create(HWND hDeviceWindow, bool bWindowed); //Device creation

	void BeginRender();
	void EndRender();
	void CleanUp();

public:
	/*Direct3D*/IDirect3D8	*m_pD3D; //D3D object
	/*D3DDevice*/IDirect3DDevice8	*m_pD3DDevice; //D3D device

private:
	
	//nothing
};

extern CVideo g_video;
extern HRESULT g_hResult;
