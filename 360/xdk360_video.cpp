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

#include "../driver.h"

#include "xdk360_video.h"
#include "../general.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xdk360_video_debugfonts_.h"

static const char* g_strPixelShaderProgram =
    " sampler2D tex : register(s0);       "
    " struct PS_IN                        "
    " {                                   "
    "     float2 coord : TEXCOORD0;       "
    " };                                  "
    "                                     "
    " float4 main(PS_IN input) : COLOR    "
    " {                                   "
    "     return tex2D(tex, input.coord); "
    " }                                   ";

static const char* g_strVertexShaderProgram =
    " struct VS_IN                                  "
    "                                               "
    " {                                             "
    "     float2 pos : POSITION;                    "
    "     float2 coord : TEXCOORD0;                 "
    " };                                            "
    "                                               "
    " struct VS_OUT                                 "
    " {                                             "
    "     float4 pos : POSITION;                    "
    "     float2 coord : TEXCOORD0;                 "
    " };                                            "
    "                                               "
    " VS_OUT main(VS_IN input)                      "
    " {                                             "
    "     VS_OUT output;                            "
    "     output.pos = float4(input.pos, 0.0, 1.0); "
    "     output.coord = input.coord;               "
    "     return output;                            "
    " }                                             ";

typedef struct DrawVerticeFormats
{
   float x, y;
   float u, v;
} DrawVerticeFormats;

static bool g_quitting;
unsigned g_frame_count;
void *g_d3d;

static void xdk360_debugfonts_dprint( int x, int y, unsigned char str )
{
	xdk360_video_t *vid = (xdk360_video_t*)g_d3d;

	int posw = (str - ' ')%16;
	int posh = (str - ' ')/16;
	primitive_t * pBuf = &vid->font_buffer[vid->font_num * 4];
	int addx = 6;

	if ( vid->font_num >= DFONT_MAX )
		return;

	if ( str == 0x9E )
		addx = 3;

	pBuf[0].x = pBuf[2].x = x;
	pBuf[1].x = pBuf[3].x = x+addx;
	pBuf[0].y = pBuf[1].y = y;
	pBuf[2].y = pBuf[3].y = y+7;
	pBuf[0].u = pBuf[2].u = ((float)(posw*16 +  1)/256.0f);
	pBuf[1].u = pBuf[3].u = ((float)(posw*16 + 13)/256.0f);
	pBuf[0].v = pBuf[1].v = ((float)(posh*16 +  1)/256.0f);
	pBuf[2].v = pBuf[3].v = ((float)(posh*16 + 15)/256.0f);
	vid->font_num++;
}

void xdk360_debugfonts_printf(int x, int y, const char *arg, ... )
{
	char tmp[1024];
	char *p = tmp;
	int  tmpx = x;

	va_list ap;
	va_start( ap, arg );
	vsprintf( tmp, arg, ap );
	while ( *p != '\0' )
	{
		unsigned char str = (unsigned char)*p;
		xdk360_debugfonts_dprint( x, y, str );
		if ( str == 0x9E )
			x += 3;
		else if ( str == '\n' )
		{
			xdk360_debugfonts_dprint( x, y, 0x9F );
			x = tmpx;
			y += 7;
		}
		else
			x += 6;

		p++;
	}
	xdk360_debugfonts_dprint( x, y, 0x9F );
	va_end( ap );
}

static void xdk360_debugfonts_init (void)
{
	HRESULT hr;
	D3DLOCKED_RECT lock_rect;
	xdk360_video_t *vid = (xdk360_video_t*)g_d3d;
	vid->font_num = 0;

	IDirect3DDevice9 * d3d = vid->xdk360_render_device;

	hr = d3d->CreateTexture(128, 128, 1, 0, D3DFMT_A8R8G8B8,
	D3DPOOL_MANAGED, &vid->font_texture, NULL);

	if (FAILED(hr))
	{
		SSNES_ERR("Could't create debug font texture.\n");
		return;
	}

	vid->font_texture->LockRect(0, &lock_rect, NULL, D3DLOCK_NOSYSLOCK );

	DWORD * src = FontData_5x5;
	DWORD * dst = (DWORD *)lock_rect.pBits;

	for (int y = 0; y < 64; y++)
	{
		memcpy(dst, src, sizeof(DWORD)*128 );
		src += 128;
		dst += lock_rect.Pitch/sizeof(DWORD);
	}

	vid->font_texture->UnlockRect(0);

	for (int i = 0; i < DFONT_MAX; i++)
	{
		vid->font_buffer[i * 4].z		= 0;
		vid->font_buffer[i * 4].rhw		= 1.0f;

		vid->font_buffer[i * 4 + 1].z	= 0;
		vid->font_buffer[i * 4 + 1].rhw	= 1.0f;

		vid->font_buffer[i * 4 + 2].z	= 0;
		vid->font_buffer[i * 4 + 2].rhw	= 1.0f;

		vid->font_buffer[i * 4 + 3].z	= 0;
		vid->font_buffer[i * 4 + 3].rhw	= 1.0f;

		vid->font_index[i * 6]			= i * 4;
		vid->font_index[i * 6 + 1]		= i * 4 + 1;
		vid->font_index[i * 6 + 2]		= i * 4 + 2;
		vid->font_index[i * 6 + 3]		= i * 4 + 3;
		vid->font_index[i * 6 + 4]		= i * 4 + 2;
		vid->font_index[i * 6 + 5]		= i * 4 + 1;
	}
}

static void xdk360_debugfonts_deinit (void)
{
	xdk360_video_t *vid = (xdk360_video_t*)g_d3d;

	IDirect3DDevice9 * d3d = vid->xdk360_render_device;

	if (vid->font_texture != NULL)
		vid->font_texture->Release();
}

static void xdk360_debugfonts_draw (void)
{
	xdk360_video_t *vid = (xdk360_video_t*)g_d3d;
	IDirect3DDevice9 * d3d = vid->xdk360_render_device;

	d3d->SetTexture(0, vid->font_texture);
	d3d->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	d3d->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	d3d->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	//d3d->SetFVF( PRIM_FVF);
	d3d->SetVertexDeclaration(vid->pVertexDecl);
	d3d->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, vid->font_num * 4,
	vid->font_num * 2, vid->font_index, D3DFMT_INDEX16, 
	vid->font_buffer, sizeof(primitive_t));

	vid->font_num = 0;
}

static void xdk360_gfx_free(void * data)
{
   if (g_d3d)
	   return;

   xdk360_video_t *vid = (xdk360_video_t*)data;

   if (!vid)
      return;

   vid->lpTexture->Release();
   vid->vertex_buf->Release();
   vid->pVertexDecl->Release();
   vid->pPixelShader->Release();
   vid->pVertexShader->Release();
   vid->xdk360_render_device->Release();
   vid->xdk360_device->Release();

   free(vid);
}

static void *xdk360_gfx_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{
   if (g_d3d)
      return g_d3d;

   xdk360_video_t *vid = (xdk360_video_t*)calloc(1, sizeof(xdk360_video_t));
   if (!vid)
      return NULL;

   vid->xdk360_device = Direct3DCreate9(D3D_SDK_VERSION);
   if (!vid->xdk360_device)
   {
      free(vid);
      return NULL;
   }

   // Get video settings
   XVIDEO_MODE video_mode;

   memset(&video_mode, 0, sizeof(video_mode));

   XGetVideoMode(&video_mode);

   memset(&vid->d3dpp, 0, sizeof(vid->d3dpp));
   
   vid->d3dpp.BackBufferWidth		= video_mode.fIsHiDef ? 1280 : 640;
   vid->d3dpp.BackBufferHeight      = video_mode.fIsHiDef ? 720 : 480;
   vid->d3dpp.BackBufferFormat        = (D3DFORMAT)MAKESRGBFMT(D3DFMT_A8R8G8B8);
   vid->d3dpp.FrontBufferFormat       = (D3DFORMAT)MAKESRGBFMT(D3DFMT_LE_X8R8G8B8);
   vid->d3dpp.MultiSampleType         = D3DMULTISAMPLE_NONE;
   vid->d3dpp.MultiSampleQuality      = 0;
   vid->d3dpp.BackBufferCount         = 2;
   vid->d3dpp.EnableAutoDepthStencil  = TRUE;
   vid->d3dpp.AutoDepthStencilFormat  = D3DFMT_D24S8;
   vid->d3dpp.SwapEffect              = D3DSWAPEFFECT_DISCARD;
   vid->d3dpp.PresentationInterval    = D3DPRESENT_INTERVAL_ONE;

   vid->xdk360_device->CreateDevice(0, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &vid->d3dpp, 
         &vid->xdk360_render_device);

   ID3DXBuffer* pShaderCodeV = NULL;
   ID3DXBuffer* pShaderCodeP = NULL;
   ID3DXBuffer* pErrorMsg = NULL;

   HRESULT hr = D3DXCompileShader(g_strVertexShaderProgram, (UINT)strlen(g_strVertexShaderProgram),
         NULL, NULL, "main", "vs_2_0", 0, &pShaderCodeV, &pErrorMsg, NULL);

   if (SUCCEEDED(hr))
   {
      hr = D3DXCompileShader(g_strPixelShaderProgram, (UINT)strlen(g_strPixelShaderProgram),
            NULL, NULL, "main", "ps_2_0", 0, &pShaderCodeP, &pErrorMsg, NULL);
   }

   if (FAILED(hr))
   {
      OutputDebugString(pErrorMsg ? (char*)pErrorMsg->GetBufferPointer() : "");
      vid->xdk360_render_device->Release();
      vid->xdk360_device->Release();
      free(vid);
      return NULL;
   }

   vid->xdk360_render_device->CreateVertexShader((const DWORD*)pShaderCodeV->GetBufferPointer(), &vid->pVertexShader);
   vid->xdk360_render_device->CreatePixelShader((const DWORD*)pShaderCodeP->GetBufferPointer(), &vid->pPixelShader);
   pShaderCodeV->Release();
   pShaderCodeP->Release();

   vid->xdk360_render_device->CreateTexture(512, 512, 1, 0, D3DFMT_LIN_X1R5G5B5,
               0, &vid->lpTexture, NULL);

   D3DLOCKED_RECT d3dlr;
   if (SUCCEEDED(vid->lpTexture->LockRect(0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK)))
   {
      memset(d3dlr.pBits, 0, 512 * d3dlr.Pitch);
      vid->lpTexture->UnlockRect(0);
   }

   vid->last_width = 512;
   vid->last_height = 512;

   vid->xdk360_render_device->CreateVertexBuffer(4 * sizeof(DrawVerticeFormats), 0, 
               0, 0, &vid->vertex_buf, NULL);

   static const DrawVerticeFormats init_verts[] = {
      { -1.0f, -1.0f, 0.0f, 1.0f },
      {  1.0f, -1.0f, 1.0f, 1.0f },
      { -1.0f,  1.0f, 0.0f, 0.0f },
      {  1.0f,  1.0f, 1.0f, 0.0f },
   };

   void *verts_ptr;
   vid->vertex_buf->Lock(0, 0, &verts_ptr, 0);
   memcpy(verts_ptr, init_verts, sizeof(init_verts));
   vid->vertex_buf->Unlock();

   static const D3DVERTEXELEMENT9 VertexElements[] =
   {
      { 0, 0 * sizeof(float), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
      { 0, 2 * sizeof(float), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
      D3DDECL_END()
   };

   vid->xdk360_render_device->CreateVertexDeclaration(VertexElements, &vid->pVertexDecl);

   vid->xdk360_render_device->Clear(0, NULL, D3DCLEAR_TARGET,
         0xff000000, 1.0f, 0);

   vid->xdk360_render_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
   vid->xdk360_render_device->SetRenderState(D3DRS_ZENABLE, FALSE);

   D3DVIEWPORT9 vp = {0};
   vp.Width  = video_mode.fIsHiDef ? 1280 : 640;
   vp.Height = video_mode.fIsHiDef ? 720 : 480;
   vp.MinZ   = 0.0f;
   vp.MaxZ   = 1.0f;
   vid->xdk360_render_device->SetViewport(&vp);

   return vid;
}

static bool xdk360_gfx_frame(void *data, const void *frame,
      unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   xdk360_video_t *vid = (xdk360_video_t*)data;
   g_frame_count++;

   vid->xdk360_render_device->Clear(0, NULL, D3DCLEAR_TARGET,
         0xff000000, 1.0f, 0);

   if (vid->last_width != width || vid->last_height != height)
   {
      D3DLOCKED_RECT d3dlr;
      if (SUCCEEDED(vid->lpTexture->LockRect(0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK)))
      {
         memset(d3dlr.pBits, 0, 512 * d3dlr.Pitch);
         vid->lpTexture->UnlockRect(0);
      }

      float tex_w = width / 512.0f;
      float tex_h = height / 512.0f;
      const DrawVerticeFormats verts[] = {
         { -1.0f, -1.0f, 0.0f,  tex_h },
         {  1.0f, -1.0f, tex_w, tex_h },
         { -1.0f,  1.0f, 0.0f,  0.0f },
         {  1.0f,  1.0f, tex_w, 0.0f },
      };

      void *verts_ptr;
      vid->vertex_buf->Lock(0, 0, &verts_ptr, 0);
      memcpy(verts_ptr, verts, sizeof(verts));
      vid->vertex_buf->Unlock();

      vid->last_width = width;
      vid->last_height = height;
   }

   D3DLOCKED_RECT d3dlr;
   if (SUCCEEDED(vid->lpTexture->LockRect(0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK)))
   {
      for (unsigned y = 0; y < height; y++)
      {
         const uint8_t *in = (const uint8_t*)frame + y * pitch;
         uint8_t *out = (uint8_t*)d3dlr.pBits + y * d3dlr.Pitch;
         memcpy(out, in, width * sizeof(uint16_t));
      }
      vid->lpTexture->UnlockRect(0);
   }

   if (msg)
   {
	   xdk360_debugfonts_printf(0, 0, msg);
	   xdk360_debugfonts_draw();
   }

   vid->xdk360_render_device->SetTexture(0, vid->lpTexture);
   vid->xdk360_render_device->SetSamplerState(0, D3DSAMP_MINFILTER, g_console.filter_type);
   vid->xdk360_render_device->SetSamplerState(0, D3DSAMP_MAGFILTER, g_console.filter_type);
   vid->xdk360_render_device->SetSamplerState(0, D3DSAMP_ADDRESSU,  D3DTADDRESS_BORDER);
   vid->xdk360_render_device->SetSamplerState(0, D3DSAMP_ADDRESSV,  D3DTADDRESS_BORDER);

   vid->xdk360_render_device->SetVertexShader(vid->pVertexShader);
   vid->xdk360_render_device->SetPixelShader(vid->pPixelShader);

   vid->xdk360_render_device->SetVertexDeclaration(vid->pVertexDecl);
   vid->xdk360_render_device->SetStreamSource(0, vid->vertex_buf, 0, sizeof(DrawVerticeFormats));

   vid->xdk360_render_device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
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

// 360 needs a working graphics stack before SSNESeven starts.
// To deal with this main.c,
// the top level module owns the instance, and is created beforehand.
// When SSNES gets around to init it, it is already allocated.
// When SSNES wants to free it, it is ignored.
void xdk360_video_init(void)
{
	video_info_t video_info = {0};
	// Might have to supply correct values here.
	video_info.vsync = true;
	video_info.force_aspect = false;
	video_info.smooth = true;
	video_info.input_scale = 2;

	g_d3d = xdk360_gfx_init(&video_info, NULL, NULL);
	xdk360_debugfonts_init();
}

void xdk360_video_deinit(void)
{
	void *data = g_d3d;
	g_d3d = NULL;
	xdk360_gfx_free(data);
}

const video_driver_t video_xdk360 = {
   xdk360_gfx_init,
   xdk360_gfx_frame,
   xdk360_gfx_set_nonblock_state,
   xdk360_gfx_alive,
   xdk360_gfx_focus,
   NULL,
   xdk360_gfx_free,
   "xdk360"
};

