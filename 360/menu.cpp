/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <xtl.h>
#include "xdkxui.h"

/* Register custom classes */
HRESULT CSSNES::RegisterXuiClasses (void)
{
	return CSSNESMain::Register();
}

/* Unregister custom classes */
HRESULT CSSNES::UnregisterXuiClasses (void)
{
	return CSSNESMain::Unregister();
	return S_OK;
}

HRESULT APIENTRY XuiTextureLoader(IXuiDevice *pDevice, LPCWSTR szFileName, XUIImageInfo *pImageInfo, IDirect3DTexture9 **ppTex)
{
    CONST BYTE  *pbTextureData = 0;
    UINT         cbTextureData = 0;
    HXUIRESOURCE hResource = 0;
    BOOL         bIsMemoryResource = FALSE;
    HRESULT      hr;
    

    hr = XuiResourceOpen(szFileName, &hResource, &bIsMemoryResource);
    if (FAILED(hr))
        return hr;

    if (bIsMemoryResource)
    {
        hr = XuiResourceGetBuffer(hResource, &pbTextureData);
        if (FAILED(hr))
            goto cleanup;
        cbTextureData = XuiResourceGetTotalSize(hResource);
    }
    else
    {
        hr = XuiResourceRead(hResource, NULL, 0, &cbTextureData);
        if (FAILED(hr))
            goto cleanup;

        pbTextureData = (BYTE *)XuiAlloc(cbTextureData);
        if (pbTextureData == 0)
        {
            hr = E_OUTOFMEMORY;
            goto cleanup;
        }

        hr = XuiResourceRead(hResource, (BYTE*)pbTextureData, cbTextureData, &cbTextureData);
        if (FAILED(hr))
            goto cleanup;
        
        XuiResourceClose(hResource);
        hResource = 0;

    }

    //Format specific code should be added here to initialize pImageInfo 
    // and to create an IDirect3DTexture9 interface.


cleanup:

    if (bIsMemoryResource && hResource != 0)
        XuiResourceReleaseBuffer(hResource, pbTextureData);
    else
        XuiFree((LPVOID)pbTextureData);

    if (hResource != 0)
        XuiResourceClose(hResource);

    return hr;
}