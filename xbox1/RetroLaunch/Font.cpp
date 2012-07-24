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


#ifdef _XBOX
#include "Font.h"

#include "../../general.h"
#include "../xdk_d3d8.h"

Font g_font;

Font::Font(void)
{
	m_pFont = NULL;
}

Font::~Font(void)
{
	if (m_pFont)
		m_pFont->Release();
}

bool Font::Create()
{
	if (m_pFont)
		m_pFont->Release();

	HRESULT g_hResult = XFONT_OpenTrueTypeFont(L"D:\\Media\\arial.ttf", 256 * 1024, &m_pFont);

	if (FAILED(g_hResult))
		return false;

	return true;
}

void Font::Render(const string &str, int x, int y, dword height, dword style, D3DXCOLOR color, int maxWidth, bool fade, Align alignment)
{
	CSurface texture;
	RenderToTexture(texture, str, height, style, color, maxWidth, fade);

	if (alignment != Left)
	{
		word *wcBuf = StringToWChar(str);
		dword dwRequiredWidth;
		m_pFont->GetTextExtent(wcBuf, -1, &dwRequiredWidth);
		delete [] wcBuf;

		if (alignment == Center)
			x -= (dwRequiredWidth / 2);
		else if (alignment == Right)
			x -= dwRequiredWidth;
	}

	texture.Render(x, y);
}

void Font::RenderToTexture(CSurface &texture, const string &str, dword height, dword style, D3DXCOLOR color, int maxWidth, bool fade)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
	if (m_pFont == NULL)
		return;

	m_pFont->SetTextHeight(height);
	m_pFont->SetTextStyle(style);
	m_pFont->SetTextColor(color);

	dword dwMaxWidth = (maxWidth <= 0) ? 1000 : maxWidth;

	// get the exact width and height required to display the string
	dword dwRequiredWidth = GetRequiredWidth(str, height, style);
	dword dwRequiredHeight = GetRequiredHeight(str, height, style);;

	// calculate the texture width and height needed to display the font
	dword dwTextureWidth  = dwRequiredWidth * 2;
	dword dwTextureHeight = dwRequiredHeight * 2;
	{
		// because the textures are swizzled we make sure
		// the dimensions are a power of two
		for(dword wmask = 1; dwTextureWidth &(dwTextureWidth - 1); wmask = (wmask << 1 ) + 1)
			dwTextureWidth = (dwTextureWidth + wmask) & ~wmask;

		for(dword hmask = 1; dwTextureHeight &(dwTextureHeight - 1); hmask = (hmask << 1) + 1)
			dwTextureHeight = ( dwTextureHeight + hmask ) & ~hmask;

		// also enforce a minimum pitch of 64 bytes
		dwTextureWidth = max(64 / XGBytesPerPixelFromFormat(D3DFMT_A8R8G8B8), dwTextureWidth);
	}

	// create an temporary image surface to render to
	D3DSurface *pTempSurface;
   d3d->d3d_render_device->CreateImageSurface(dwTextureWidth, dwTextureHeight, D3DFMT_LIN_A8R8G8B8, &pTempSurface);

	// clear the temporary surface
	{
		D3DLOCKED_RECT tmpLr;
		pTempSurface->LockRect(&tmpLr, NULL, 0);
		memset(tmpLr.pBits, 0, dwTextureWidth * dwTextureHeight * XGBytesPerPixelFromFormat(D3DFMT_A8R8G8B8));
		pTempSurface->UnlockRect();
	}

	// render the text to the temporary surface
	word *wcBuf = StringToWChar(str);
	m_pFont->TextOut(pTempSurface, wcBuf, -1, 0, 0);
	delete [] wcBuf;

	// create the texture that will be drawn to the screen
	texture.Destroy();
	texture.Create(dwTextureWidth, dwTextureHeight);

	// copy from the temporary surface to the final texture
	{
		D3DLOCKED_RECT tmpLr;
		D3DLOCKED_RECT txtLr;

		pTempSurface->LockRect(&tmpLr, NULL, 0);
		texture.GetTexture()->LockRect(0, &txtLr, NULL, 0);

		if (fade)
		{
			// draw the last 35 pixels of the string fading out to max width or texture width
			dword dwMinFadeDistance = min(static_cast<dword>(dwTextureWidth * 0.35), 35);
			dword dwFadeStart               = min(dwTextureWidth, dwMaxWidth - dwMinFadeDistance);
			dword dwFadeEnd                 = min(dwTextureWidth, dwMaxWidth);
			dword dwFadeDistance    = dwFadeEnd - dwFadeStart;

			for (dword h = 0; h < dwTextureHeight; h++)
			{
				for (dword w = 0; w < dwFadeDistance; w++)
				{
					dword *pColor = reinterpret_cast<dword *>(tmpLr.pBits);
					dword offset = (h * dwTextureWidth) + (dwFadeStart + w);

					D3DXCOLOR color = D3DXCOLOR(pColor[offset]);
					color.a = color.a * (1.0f - static_cast<float>(w) / static_cast<float>(dwFadeDistance));
					pColor[offset] = color;
				}
			}
		}

		// dont draw anything > than max width
		for (dword h = 0; h < dwTextureHeight; h++)
		{
			for (dword w = min(dwTextureWidth, dwMaxWidth); w < dwTextureWidth; w++)
			{
				dword *pColor = reinterpret_cast<dword *>(tmpLr.pBits);
				dword offset = (h * dwTextureWidth) + w;

				D3DXCOLOR color = D3DXCOLOR(pColor[offset]);
				color.a = 0.0;
				pColor[offset] = color;
			}
		}

		// copy and swizzle the linear surface to the swizzled texture
		XGSwizzleRect(tmpLr.pBits, tmpLr.Pitch, NULL, txtLr.pBits, dwTextureWidth, dwTextureHeight, NULL, 4);

		texture.GetTexture()->UnlockRect(0);
		pTempSurface->UnlockRect();
	}

	pTempSurface->Release();
}

int Font::GetRequiredWidth(const string &str, dword height, dword style)
{
	word *wcBuf = StringToWChar(str);
	dword reqWidth;

	m_pFont->SetTextHeight(height);
	m_pFont->SetTextStyle(style);
	m_pFont->GetTextExtent(wcBuf, -1, &reqWidth);

	delete [] wcBuf;

	return reqWidth;
}

int Font::GetRequiredHeight(const string &str, dword height, dword style)
{
	word *wcBuf = StringToWChar(str);
	dword reqHeight;

	m_pFont->SetTextHeight(height);
	m_pFont->SetTextStyle(style);
	m_pFont->GetFontMetrics(&reqHeight, NULL);

	delete [] wcBuf;

	return reqHeight;
}


word *Font::StringToWChar(const string &str)
{
	word *retVal = new word[(str.length() + 1) * 2];
	memset(retVal, 0, (str.length() + 1) * 2 * sizeof(word));

	if (str.length() > 0)
		mbstowcs(retVal, str.c_str(), str.length());

	return retVal;
}
#endif
