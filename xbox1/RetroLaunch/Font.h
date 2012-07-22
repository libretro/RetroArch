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
#ifdef _XBOX
#include "Global.h"
#include "Surface.h"

#define XFONT_TRUETYPE // use true type fonts
#include <xfont.h>

enum Align
{
	Left,
	Center,
	Right
};

class Font
{
public:
	Font(void);
	~Font(void);

	bool Create();
	bool Create(const string &szTTFFilename);

	void Render(const string &str, int x, int y, dword height, dword style = XFONT_NORMAL, D3DXCOLOR color = D3DCOLOR_XRGB(0, 0, 0), int dwMaxWidth = -1, bool fade = false, Align alignment = Left);
	void RenderToTexture(CSurface &texture, const string &str, dword height, dword style = XFONT_NORMAL, D3DXCOLOR color = D3DCOLOR_XRGB(0, 0, 0), int maxWidth = -1, bool fade = false);
	
	int GetRequiredWidth(const string &str, dword height, dword style);
	int GetRequiredHeight(const string &str, dword height, dword style);

	word *StringToWChar(const string &str);

private:
	XFONT *m_pFont;
};

extern Font g_font;
#endif