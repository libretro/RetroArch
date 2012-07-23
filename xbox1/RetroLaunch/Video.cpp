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


#include "Video.h"
#include "IniFile.h"
#include "Debug.h"

CVideo g_video;
HRESULT g_hResult;

CVideo::CVideo(void)
{
}

CVideo::~CVideo(void)
{
}

bool CVideo::Create(HWND hDeviceWindow, bool bWindowed)
{
	// Create the Direct3D object (leave it DX8 or should we try DX9 for WIN32 ?)
	m_pD3D = Direct3DCreate8(D3D_SDK_VERSION);

	if (m_pD3D == NULL)
		return false;

	// set up the structure used to create the d3d device
	D3DPRESENT_PARAMETERS d3dpp;
	ZeroMemory(&d3dpp, sizeof(d3dpp));

	d3dpp.BackBufferWidth                                       = 640;
	d3dpp.BackBufferHeight                                      = 480;
	d3dpp.BackBufferFormat                                      = D3DFMT_A8R8G8B8;
	d3dpp.BackBufferCount                                       = 1;
	//d3dpp.AutoDepthStencilFormat			= D3DFMT_D16;
	//d3dpp.EnableAutoDepthStencil			= false;
	d3dpp.SwapEffect                                            = D3DSWAPEFFECT_DISCARD;

	//Fullscreen only
	if(!bWindowed)
	{
		if(!g_iniFile.m_currentIniEntry.bVSync) {
			d3dpp.FullScreen_PresentationInterval   = D3DPRESENT_INTERVAL_IMMEDIATE;
		}else{
			d3dpp.FullScreen_PresentationInterval   = D3DPRESENT_INTERVAL_ONE;
		}
	}

	g_hResult = m_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, NULL,
	                                 D3DCREATE_HARDWARE_VERTEXPROCESSING,
	                                 &d3dpp, &m_pD3DDevice);

	if (FAILED(g_hResult))
	{
		g_debug.Print("Error: D3DCreate(), CreateDevice()");
		return false;
	}
	// use an orthogonal matrix for the projection matrix
	D3DXMATRIX mat;

	D3DXMatrixOrthoOffCenterLH(&mat, 0.0f, 640.0f, 480.0f, 0.0f, 0.0f, 1.0f);

	m_pD3DDevice->SetTransform(D3DTS_PROJECTION, &mat);

	// use an identity matrix for the world and view matrices
	D3DXMatrixIdentity(&mat);
	m_pD3DDevice->SetTransform(D3DTS_WORLD, &mat);
	m_pD3DDevice->SetTransform(D3DTS_VIEW, &mat);

	// disable lighting
	m_pD3DDevice->SetRenderState(D3DRS_LIGHTING, FALSE);

	// disable z-buffer (see autodepthstencil)
	m_pD3DDevice->SetRenderState(D3DRS_ZENABLE, FALSE );

	return true;
}


void CVideo::BeginRender()
{
	m_pD3DDevice->Clear(0, NULL, D3DCLEAR_TARGET,
	                    D3DCOLOR_XRGB(0, 0, 0),
	                    1.0f, 0);

	m_pD3DDevice->BeginScene();
	m_pD3DDevice->SetFlickerFilter(g_iniFile.m_currentIniEntry.dwFlickerFilter);
	m_pD3DDevice->SetSoftDisplayFilter(g_iniFile.m_currentIniEntry.bSoftDisplayFilter);
}

void CVideo::EndRender()
{
	m_pD3DDevice->EndScene();

	m_pD3DDevice->Present(NULL, NULL, NULL, NULL);

}

void CVideo::CleanUp()
{
	if( m_pD3DDevice != NULL)
		m_pD3DDevice->Release();

	if( m_pD3D != NULL)
		m_pD3D->Release();
}
