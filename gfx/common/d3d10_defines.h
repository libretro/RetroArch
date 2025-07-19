/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2018 - Ali Bouhlel
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

#ifndef _D3D10_DEFINES_H_
#define _D3D10_DEFINES_H_

#include <retro_inline.h>

#include "dxgi_common.h"
#include <d3d10.h>

#include <boolean.h>

#include <retro_math.h>
#include <gfx/math/matrix_4x4.h>

#include "../drivers_shader/slang_process.h"

#define D3D10_MAX_GPU_COUNT 16

enum d3d10_video_flags
{
   D3D10_ST_FLAG_VSYNC               = (1 << 0),
   D3D10_ST_FLAG_RESIZE_CHAIN        = (1 << 1),
   D3D10_ST_FLAG_KEEP_ASPECT         = (1 << 2),
   D3D10_ST_FLAG_RESIZE_VIEWPORT     = (1 << 3),
   D3D10_ST_FLAG_RESIZE_RTS          = (1 << 4), /* RT = Render Target */
   D3D10_ST_FLAG_INIT_HISTORY        = (1 << 5),
   D3D10_ST_FLAG_SPRITES_ENABLE      = (1 << 6),
   D3D10_ST_FLAG_OVERLAYS_ENABLE     = (1 << 7),
   D3D10_ST_FLAG_OVERLAYS_FULLSCREEN = (1 << 8),
   D3D10_ST_FLAG_MENU_ENABLE         = (1 << 9),
   D3D10_ST_FLAG_MENU_FULLSCREEN     = (1 << 10),
   D3D10_ST_FLAG_FRAME_DUPE_LOCK     = (1 << 11)
};


typedef const ID3D10SamplerState*       D3D10SamplerStateRef;

typedef ID3D10InputLayout*       D3D10InputLayout;
typedef ID3D10RasterizerState*   D3D10RasterizerState;
typedef ID3D10DepthStencilState* D3D10DepthStencilState;
typedef ID3D10BlendState*        D3D10BlendState;
typedef ID3D10PixelShader*       D3D10PixelShader;
typedef ID3D10SamplerState*      D3D10SamplerState;
typedef ID3D10VertexShader*      D3D10VertexShader;
typedef ID3D10GeometryShader*    D3D10GeometryShader;

/* auto-generated */
typedef ID3DDestructionNotifier*  D3DDestructionNotifier;
typedef ID3D10Resource*           D3D10Resource;
typedef ID3D10Buffer*             D3D10Buffer;
typedef ID3D10Texture1D*          D3D10Texture1D;
typedef ID3D10Texture2D*          D3D10Texture2D;
typedef ID3D10Texture3D*          D3D10Texture3D;
typedef ID3D10View*               D3D10View;
typedef ID3D10ShaderResourceView* D3D10ShaderResourceView;
typedef ID3D10RenderTargetView*   D3D10RenderTargetView;
typedef ID3D10DepthStencilView*   D3D10DepthStencilView;
typedef ID3D10Asynchronous*       D3D10Asynchronous;
typedef ID3D10Query*              D3D10Query;
typedef ID3D10Predicate*          D3D10Predicate;
typedef ID3D10Counter*            D3D10Counter;
typedef ID3D10Device*             D3D10Device;
typedef ID3D10Multithread*        D3D10Multithread;
#ifdef DEBUG
typedef ID3D10Debug*              D3D10Debug;
#endif
typedef ID3D10SwitchToRef*        D3D10SwitchToRef;
typedef ID3D10InfoQueue*          D3D10InfoQueue;

#ifndef ALIGN
#ifdef _MSC_VER
#define ALIGN(x) __declspec(align(x))
#else
#define ALIGN(x) __attribute__((aligned(x)))
#endif
#endif

#endif
