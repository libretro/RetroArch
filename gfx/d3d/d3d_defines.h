/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#ifndef D3DVIDEO_DEFINES_H
#define D3DVIDEO_DEFINES_H

#if defined(HAVE_D3D9)
/* Direct3D 9 */
#include <d3d9.h>
#include <d3dx9.h>
#include <d3dx9core.h>

#define LPDIRECT3D                     LPDIRECT3D9
#define LPDIRECT3DDEVICE               LPDIRECT3DDEVICE9
#define LPDIRECT3DTEXTURE              LPDIRECT3DTEXTURE9
#define LPDIRECT3DCUBETEXTURE          LPDIRECT3DCUBETEXTURE9
#define LPDIRECT3DVERTEXBUFFER         LPDIRECT3DVERTEXBUFFER9
#define LPDIRECT3DVERTEXSHADER         LPDIRECT3DVERTEXSHADER9
#define LPDIRECT3DPIXELSHADER          LPDIRECT3DPIXELSHADER9
#define LPDIRECT3DSURFACE              LPDIRECT3DSURFACE9
#define LPDIRECT3DVERTEXDECLARATION    LPDIRECT3DVERTEXDECLARATION9
#define LPDIRECT3DVOLUMETEXTURE        LPDIRECT3DVOLUMETEXTURE9
#define LPDIRECT3DRESOURCE             LPDIRECT3DRESOURCE9
#define D3DVERTEXELEMENT               D3DVERTEXELEMENT9
#define D3DVIEWPORT                    D3DVIEWPORT9

#define D3DCREATE_CTX                  Direct3DCreate9

#ifndef D3DCREATE_SOFTWARE_VERTEXPROCESSING
#define D3DCREATE_SOFTWARE_VERTEXPROCESSING 0
#endif

#elif defined(_XBOX1)
#include <xtl.h>

/* Direct3D 8 */
#define LPDIRECT3D                     LPDIRECT3D8
#define LPDIRECT3DDEVICE               LPDIRECT3DDEVICE8
#define LPDIRECT3DTEXTURE              LPDIRECT3DTEXTURE8
#define LPDIRECT3DCUBETEXTURE          LPDIRECT3DCUBETEXTURE8
#define LPDIRECT3DVOLUMETEXTURE        LPDIRECT3DVOLUMETEXTURE8
#define LPDIRECT3DVERTEXBUFFER         LPDIRECT3DVERTEXBUFFER8
#define LPDIRECT3DSURFACE              LPDIRECT3DSURFACE8
#define LPDIRECT3DRESOURCE             LPDIRECT3DRESOURCE8
#define D3DVERTEXELEMENT               D3DVERTEXELEMENT8
#define D3DVIEWPORT                    D3DVIEWPORT8

#define D3DCREATE_CTX                  Direct3DCreate8

#define D3DLOCK_NOSYSLOCK (0)
#define D3DSAMP_ADDRESSU D3DTSS_ADDRESSU
#define D3DSAMP_ADDRESSV D3DTSS_ADDRESSV
#define D3DSAMP_MAGFILTER D3DTSS_MAGFILTER
#define D3DSAMP_MINFILTER D3DTSS_MINFILTER
#endif

#if defined(_XBOX360)
#define D3DFVF_CUSTOMVERTEX 0
#elif defined(_XBOX1)
#define D3DFVF_CUSTOMVERTEX	(D3DFVF_XYZRHW | D3DFVF_TEX1)
#endif

#endif
