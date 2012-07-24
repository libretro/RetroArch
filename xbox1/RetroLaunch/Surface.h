/**
 * Surreal 64 Launcher (C) 2003
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
 * authors: email: buttza@hotmail.com, lantus@lantus-x.com
 *
 * Additional code and cleanups: Surreal64 CE Team (http://www.emuxtras.net)
 */

#pragma once

#include "Global.h"

class CSurface
{
public:
	CSurface();
	CSurface(const string &szFilename);
	~CSurface();

	/**
	 * Do functions
	 */
	bool Create(const string &szFilename);
	bool Create(dword width, dword height);
	void Destroy();

	bool IsLoaded();

	bool Render();
	bool Render(int x, int y);
	bool Render(int x, int y, dword w, dword h);

	/**
	 * Set functions
	 */
	void SetOpacity(byte opacity);

	void MoveTo(int x, int y);

	/**
	 * Get functions
	 */
	dword GetWidth();
	dword GetHeight();

	byte GetOpacity();

	IDirect3DTexture8 *GetTexture();

private:
	/**
	 * A d3d texture object that will contain the loaded texture
	 * and a d3d vertex buffer object that will contain the vertex
	 * buffer for the quad which will display the texture
	 */
	IDirect3DTexture8 *m_pTexture;
	IDirect3DVertexBuffer8 *m_pVertexBuffer;

	/**
	 * The default render position of the texture
	 */
	int m_x, m_y;

	/**
	 * The width and height of the texture
	 */
	D3DXIMAGE_INFO m_imageInfo;

	/**
	 * The opacity of the texture
	 */
	byte m_byOpacity;
	byte m_byR, m_byG, m_byB;

	/**
	 * Whether the texture has been created or not
	 */
	bool m_bLoaded;
};
