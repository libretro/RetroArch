/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _XDK_DEFINES_H
#define _XDK_DEFINES_H


#if defined(_XBOX1)
/* XBox 1*/

#define RD3DDevice_SetTransform(device, State, pMatrix) D3DDevice_SetTransform(State, pMatrix)

#define RD3DVertexBuffer_Lock(device, OffsetToLock, SizeToLock, ppbData, Flags) *ppbData = D3DVertexBuffer_Lock2(device, Flags) + OffsetToLock
#define RD3DVertexBuffer_Unlock(device)
#define RD3DDevice_Clear(device, Count, pRects, Flags, Color, Z, Stencil) D3DDevice_Clear(Count, pRects, Flags, Color, Z, Stencil)

#elif defined(_XBOX360)
/* XBox 360*/

#define RD3DVertexBuffer_Lock(device, OffsetToLock, SizeToLock, ppbData, Flags) *ppbData = D3DVertexBuffer_Lock(device, OffsetToLock, SizeToLock, Flags)
#define RD3DVertexBuffer_Unlock(device) D3DVertexBuffer_Unlock(device)

#define RD3DDevice_Clear(device, Count, pRects, Flags, Color, Z, Stencil) D3DDevice_Clear(device, Count, pRects, Flags, Color, Z, Stencil, false)
#endif

#endif
