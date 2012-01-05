/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
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
#include <xboxmath.h>

#include "driver.h"
#include "general.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// pixel shader
const CHAR*         g_strPixelShaderProgram =
    " struct PS_IN                                 "
    " {                                            "
    "     float4 Color : COLOR;                    "  // Interpolated color from                      
    " };                                           "  // the vertex shader
    "                                              "
    " float4 main( PS_IN In ) : COLOR              "
    " {                                            "
    "     return In.Color;                         "  // Output color
    " }                                            ";

// vertex shader
const CHAR*         g_strVertexShaderProgram =
    " float4x4 matWVP : register(c0);              "
    "                                              "
    " struct VS_IN                                 "
    "                                              "
    " {                                            "
    "     float4 ObjPos   : POSITION;              "  // Object space position 
    "     float4 Color    : COLOR;                 "  // Vertex color                 
    " };                                           "
    "                                              "
    " struct VS_OUT                                "
    " {                                            "
    "     float4 ProjPos  : POSITION;              "  // Projected space position 
    "     float4 Color    : COLOR;                 "
    " };                                           "
    "                                              "
    " VS_OUT main( VS_IN In )                      "
    " {                                            "
    "     VS_OUT Out;                              "
    "     Out.ProjPos = mul( matWVP, In.ObjPos );  "  // Transform vertex into
    "     Out.Color = In.Color;                    "  // Projected space and 
    "     return Out;                              "  // Transfer color
    " }                                            ";


typedef struct DrawVerticeFormats
{
	float x, y, z, w;
	unsigned int color;
	float u, v;
} DrawVerticeFormats;

typedef struct xdk360_video xdk360_video_t;

static bool g_quitting;

typedef struct gl
{
   IDirect3D9* xdk360_device;
   IDirect3DDevice9* xdk360_render_device;
   IDirect3DVertexShader9 *pVertexShader;
   IDirect3DPixelShader9* pPixelShader;
   XMMATRIX matWVP;
   unsigned frame_count;
} gl_t;

static void xdk360_gfx_free(void *data)
{
   gl_t *vid = data;
   if (!vid)
      return;

   free(vid);
}

static void *xdk360_gfx_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{
  gl_t * gl = calloc(1, sizeof(gl_t));
  if (!gl)
      return NULL;

  gl->xdk360_device = Direct3DCreate9( D3D_SDK_VERSION );

  /* Set up the structure used to create the Direct3D device */
  D3DPRESENT_PARAMETERS d3dpp;
  ZeroMemory( &d3dpp, sizeof( d3dpp ) );
  d3dpp.BackBufferWidth = 1280;
  d3dpp.BackBufferHeight = 720;
  d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
  d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );
  d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
  d3dpp.MultiSampleQuality = 0;
  d3dpp.BackBufferCount = 1;
  d3dpp.EnableAutoDepthStencil = TRUE;
  d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
  d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
  d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

   /* Create the Direct3D device */
   gl->xdk360_device->CreateDevice(0, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dpp, 
   &gl->xdk360_render_device);

   /* Buffers to hold compiled shaders and possible error messages */
   ID3DXBuffer* pShaderCode = NULL;
   ID3DXBuffer* pErrorMsg = NULL;

   /* Compile vertex shader */
   HRESULT hr = D3DXCompileShader(g_strVertexShaderProgram, ( UINT )strlen(g_strVertexShaderProgram),
   NULL, NULL, "main", "vs_2_0", 0, &pShaderCode, &pErrorMsg, NULL);

   if (FAILED(hr))
   {
   	OutputDebugStringA( pErrorMsg ? (CHAR *)pErrorMsg->GetBufferPointer() : "");
	exit(1);
   }

   /* Create vertex shader */
   gl->xdk360_render_device->CreateVertexShader((DWORD*)pShaderCode->GetBufferPointer(), &gl->pVertexShader);

   /* Shader code is no longer required */
   pShaderCode->Release();
   pShaderCode = NULL;

   /* Compile pixel shader */
   hr = D3DXCompileShader(g_strPixelShaderProgram, (UINT)strlen(g_strPixelShaderProgram),
   NULL, NULL, "main", "ps_2_0", 0, &pShaderCode, &pErrorMsg, NULL);

   if (FAILED(hr))
   {
   	OutputDebugStringA( pErrorMsg ? (CHAR *)pErrorMsg->GetBufferPointer() : "");
	exit(1);
   }

  /* Create pixel shader */
  gl->xdk360_render_device->CreatePixelShader((DWORD*)pShaderCode->GetBufferPointer(), &gl->pPixelShader);

  /* Shader code no longer required */
  pShaderCode->Release();
  pShaderCode = NULL;

  /* Structure to hold vertex data.*/
  struct COLORVERTEX
  {
      FLOAT Position[3];
      DWORD Color;
  };

  COLORVERTEX Vertices[3] =
  {
      { -1.0f, -1.0f, 0.0f, 0x00FF0000 },
      {  0.0f,  1.0f, 0.0f, 0x0000FF00 },
      {  1.0f, -1.0f, 0.0f, 0x000000FF }
  };

  /* Define the vertex elements.*/
  static const D3DVERTEXELEMENT9 VertexElements[3] =
  {
      { 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
      { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0 },
      D3DDECL_END()
  };

  /* Create a vertex declaration from the element descriptions.*/
  IDirect3DVertexDeclaration9* pVertexDecl;
  g_pd3dDevice->CreateVertexDeclaration( VertexElements, &pVertexDecl );

  /* World matrix (identity in this sample)*/
  XMMATRIX matWorld = XMMatrixIdentity();

  /* View matrix*/
  XMVECTOR vEyePt = XMVectorSet( 0.0f, 0.0f, -4.0f, 0.0f );
  XMVECTOR vLookatPt = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
  XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
  XMMATRIX matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUp );

  /* Determine the aspect ratio*/
  FLOAT fAspectRatio = ( FLOAT )d3dpp.BackBufferWidth / ( FLOAT )d3dpp.BackBufferHeight;

  /* Projection matrix*/
  XMMATRIX matProj = XMMatrixPerspectiveFovLH( XM_PI / 4, fAspectRatio, 1.0f, 200.0f );

  /* World*view*projection*/
  gl->matWVP = matWorld * matView * matProj;

   /* Clear the backbuffer.*/
   gl->xdk360_render_device->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
   0xff000000, 1.0f, 0L );

   return gl;
}

static bool xdk360_gfx_frame(void *data, const void *frame, unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   gl_t *vid = data;

   vid->frame_count++;

   /* Clear the backbuffer.*/
   vid->xdk360_render_device->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
   0xff000000, 1.0f, 0L );

   /* Set shaders. */
   vid->xdk360_render_device->SetVertexShader( gl->pVertexShader );
   vid->xdk360_render_device->SetPixelShader( gl->pPixelShader );

   // Set shader constants.
   vid->xdk360_render_device->SetVertexShaderConstantF( 0, ( FLOAT* )&vid->matWVP, 4 );

   // Set the vertex declaration.
   vid->xdk360_render_device->SetVertexDeclaration( pVertexDecl );

   // Draw
   vid->xdk360_render_device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2 );

   // Resolve
   vid->xdk360_render_device->Present(NULL, NULL, NULL, NULL);

   return true;
}

static void xdk360_gfx_set_nonblock_state(void *data, bool state)
{
   (void)data;
   (void)state;
}

static bool xdk360_gfx_alive(void *data)
{
   (void)data;
   return !g_quitting;
}

static bool xdk360_gfx_focus(void *data)
{
   (void)data;
   return true;
}


const video_driver_t video_xdk360 = {
   .init = xdk360_gfx_init,
   .frame = xdk360_gfx_frame,
   .alive = xdk360_gfx_alive,
   .set_nonblock_state = xdk360_gfx_set_nonblock_state,
   .focus = xdk360_gfx_focus,
   .free = xdk360_gfx_free,
   .ident = "xdk360"
};

