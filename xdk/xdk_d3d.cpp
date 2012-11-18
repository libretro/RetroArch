/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#ifdef _XBOX
#include <xtl.h>
#include <xgraphics.h>
#endif

#include "../driver.h"
#include "xdk_d3d.h"

#ifdef HAVE_HLSL
#include "../gfx/shader_hlsl.h"
#endif

#ifdef _XBOX1
#include "./../gfx/fonts/xdk1_xfonts.h"
#endif

#include "./../gfx/gfx_context.h"
#include "../general.h"
#include "../message.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _XBOX360
#include "../gfx/fonts/xdk360_fonts.h"
#endif

#include "../xdk/xdk_resources.h"

#if defined(_XBOX1)
wchar_t strw_buffer[128];
unsigned font_x, font_y;
FLOAT angle;
#elif defined(_XBOX360)
extern video_console_t video_console;
extern xdk360_video_font_t m_Font;

const DWORD g_MapLinearToSrgbGpuFormat[] = 
{
   GPUTEXTUREFORMAT_1_REVERSE,
   GPUTEXTUREFORMAT_1,
   GPUTEXTUREFORMAT_8,
   GPUTEXTUREFORMAT_1_5_5_5,
   GPUTEXTUREFORMAT_5_6_5,
   GPUTEXTUREFORMAT_6_5_5,
   GPUTEXTUREFORMAT_8_8_8_8_AS_16_16_16_16,
   GPUTEXTUREFORMAT_2_10_10_10_AS_16_16_16_16,
   GPUTEXTUREFORMAT_8_A,
   GPUTEXTUREFORMAT_8_B,
   GPUTEXTUREFORMAT_8_8,
   GPUTEXTUREFORMAT_Cr_Y1_Cb_Y0_REP,     
   GPUTEXTUREFORMAT_Y1_Cr_Y0_Cb_REP,      
   GPUTEXTUREFORMAT_16_16_EDRAM,          
   GPUTEXTUREFORMAT_8_8_8_8_A,
   GPUTEXTUREFORMAT_4_4_4_4,
   GPUTEXTUREFORMAT_10_11_11_AS_16_16_16_16,
   GPUTEXTUREFORMAT_11_11_10_AS_16_16_16_16,
   GPUTEXTUREFORMAT_DXT1_AS_16_16_16_16,
   GPUTEXTUREFORMAT_DXT2_3_AS_16_16_16_16,  
   GPUTEXTUREFORMAT_DXT4_5_AS_16_16_16_16,
   GPUTEXTUREFORMAT_16_16_16_16_EDRAM,
   GPUTEXTUREFORMAT_24_8,
   GPUTEXTUREFORMAT_24_8_FLOAT,
   GPUTEXTUREFORMAT_16,
   GPUTEXTUREFORMAT_16_16,
   GPUTEXTUREFORMAT_16_16_16_16,
   GPUTEXTUREFORMAT_16_EXPAND,
   GPUTEXTUREFORMAT_16_16_EXPAND,
   GPUTEXTUREFORMAT_16_16_16_16_EXPAND,
   GPUTEXTUREFORMAT_16_FLOAT,
   GPUTEXTUREFORMAT_16_16_FLOAT,
   GPUTEXTUREFORMAT_16_16_16_16_FLOAT,
   GPUTEXTUREFORMAT_32,
   GPUTEXTUREFORMAT_32_32,
   GPUTEXTUREFORMAT_32_32_32_32,
   GPUTEXTUREFORMAT_32_FLOAT,
   GPUTEXTUREFORMAT_32_32_FLOAT,
   GPUTEXTUREFORMAT_32_32_32_32_FLOAT,
   GPUTEXTUREFORMAT_32_AS_8,
   GPUTEXTUREFORMAT_32_AS_8_8,
   GPUTEXTUREFORMAT_16_MPEG,
   GPUTEXTUREFORMAT_16_16_MPEG,
   GPUTEXTUREFORMAT_8_INTERLACED,
   GPUTEXTUREFORMAT_32_AS_8_INTERLACED,
   GPUTEXTUREFORMAT_32_AS_8_8_INTERLACED,
   GPUTEXTUREFORMAT_16_INTERLACED,
   GPUTEXTUREFORMAT_16_MPEG_INTERLACED,
   GPUTEXTUREFORMAT_16_16_MPEG_INTERLACED,
   GPUTEXTUREFORMAT_DXN,
   GPUTEXTUREFORMAT_8_8_8_8_AS_16_16_16_16,
   GPUTEXTUREFORMAT_DXT1_AS_16_16_16_16,
   GPUTEXTUREFORMAT_DXT2_3_AS_16_16_16_16,
   GPUTEXTUREFORMAT_DXT4_5_AS_16_16_16_16,
   GPUTEXTUREFORMAT_2_10_10_10_AS_16_16_16_16,
   GPUTEXTUREFORMAT_10_11_11_AS_16_16_16_16,
   GPUTEXTUREFORMAT_11_11_10_AS_16_16_16_16,
   GPUTEXTUREFORMAT_32_32_32_FLOAT,
   GPUTEXTUREFORMAT_DXT3A,
   GPUTEXTUREFORMAT_DXT5A,
   GPUTEXTUREFORMAT_CTX1,
   GPUTEXTUREFORMAT_DXT3A_AS_1_1_1_1,
   GPUTEXTUREFORMAT_8_8_8_8_GAMMA_EDRAM,
   GPUTEXTUREFORMAT_2_10_10_10_FLOAT_EDRAM,
};
#endif

static void check_window(xdk_d3d_video_t *d3d)
{
   bool quit, resize;

   d3d->driver->check_window(&quit,
         &resize, NULL, NULL,
         d3d->frame_count);

   if (quit)
      d3d->quitting = true;
   else if (resize)
      d3d->should_resize = true;
}

#ifdef HAVE_HLSL
static bool hlsl_shader_init(void)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
   const char *shader_path = g_settings.video.cg_shader_path;

   return hlsl_init(g_settings.video.cg_shader_path, d3d->d3d_render_device);
}
#endif

static void xdk_d3d_free(void * data)
{
#ifdef RARCH_CONSOLE
   if (driver.video_data)
	   return;
#endif

   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)data;

   if (!d3d)
      return;

#ifdef HAVE_HLSL
   hlsl_deinit();
#endif
#ifdef HAVE_D3D9
   d3d9_deinit_font();
#endif

   d3d->driver->destroy();

   free(d3d);
}

#ifdef _XBOX360
void xdk_video_font_draw_text(xdk360_video_font_t *font, 
   float fOriginX, float fOriginY, const wchar_t * strText, float fMaxPixelWidth )
{
   if( strText == NULL || strText[0] == L'\0')
      return;

   xdk_d3d_video_t *vid = (xdk_d3d_video_t*)driver.video_data;
   D3DDevice *pd3dDevice = vid->d3d_render_device;

   // Set the color as a vertex shader constant
   float vColor[4];
   vColor[0] = ( ( 0xffffffff & 0x00ff0000 ) >> 16L ) / 255.0F;
   vColor[1] = ( ( 0xffffffff & 0x0000ff00 ) >> 8L ) / 255.0F;
   vColor[2] = ( ( 0xffffffff & 0x000000ff ) >> 0L ) / 255.0F;
   vColor[3] = ( ( 0xffffffff & 0xff000000 ) >> 24L ) / 255.0F;

   d3d9_render_msg_pre(font);

   // Perform the actual storing of the color constant here to prevent
   // a load-hit-store by inserting work between the store and the use of
   // the vColor array.
   pd3dDevice->SetVertexShaderConstantF( 1, vColor, 1 );

   // Set the starting screen position
   if((fOriginX < 0.0f))
      fOriginX += font->m_rcWindow.x2;
   if( fOriginY < 0.0f )
      fOriginY += font->m_rcWindow.y2;

   font->m_fCursorX = floorf( fOriginX );
   font->m_fCursorY = floorf( fOriginY );

   // Adjust for padding
   fOriginY -= font->m_fFontTopPadding;

   // Add window offsets
   float Winx = 0.0f;
   float Winy = 0.0f;
   fOriginX += Winx;
   fOriginY += Winy;
   font->m_fCursorX += Winx;
   font->m_fCursorY += Winy;

   // Begin drawing the vertices

   // Declared as volatile to force writing in ascending
   // address order. It prevents out of sequence writing in write combined
   // memory.

   volatile float * pVertex;

   unsigned long dwNumChars = wcslen(strText);
   HRESULT hr = pd3dDevice->BeginVertices( D3DPT_QUADLIST, 4 * dwNumChars, sizeof( XMFLOAT4 ) ,
      ( VOID** )&pVertex );

   // The ring buffer may run out of space when tiling, doing z-prepasses,
   // or using BeginCommandBuffer. If so, make the buffer larger.
   if( hr < 0 )
      RARCH_ERR( "Ring buffer out of memory.\n" );

   // Draw four vertices for each glyph
   while( *strText )
   {
      wchar_t letter;

      // Get the current letter in the string
      letter = *strText++;

      // Handle the newline character
      if( letter == L'\n' )
      {
         font->m_fCursorX = fOriginX;
         font->m_fCursorY += font->m_fFontYAdvance * font->m_fYScaleFactor;
         continue;
      }

      // Translate unprintable characters
      const GLYPH_ATTR * pGlyph = &font->m_Glyphs[ ( letter <= font->m_cMaxGlyph )
      ? font->m_TranslatorTable[letter] : 0 ];

      float fOffset = font->m_fXScaleFactor * (float)pGlyph->wOffset;
      float fAdvance = font->m_fXScaleFactor * (float)pGlyph->wAdvance;
      float fWidth = font->m_fXScaleFactor * (float)pGlyph->wWidth;
      float fHeight = font->m_fYScaleFactor * font->m_fFontHeight;

      // Setup the screen coordinates
      font->m_fCursorX += fOffset;
      float X4 = font->m_fCursorX;
      float X1 = X4;
      float X3 = X4 + fWidth;
      float X2 = X1 + fWidth;
      float Y1 = font->m_fCursorY;
      float Y3 = Y1 + fHeight;
      float Y2 = Y1;
      float Y4 = Y3;

      font->m_fCursorX += fAdvance;

      // Add the vertices to draw this glyph

      unsigned long tu1 = pGlyph->tu1;        // Convert shorts to 32 bit longs for in register merging
      unsigned long tv1 = pGlyph->tv1;
      unsigned long tu2 = pGlyph->tu2;
      unsigned long tv2 = pGlyph->tv2;

      // NOTE: The vertexs are 2 floats for the screen coordinates,
      // followed by two USHORTS for the u/vs of the character,
      // terminated with the ARGB 32 bit color.
      // This makes for 16 bytes per vertex data (Easier to read)
      // Second NOTE: The uvs are merged and written using a DWORD due
      // to the write combining hardware being only able to handle 32,
      // 64 and 128 writes. Never store to write combined memory with
      // 8 or 16 bit instructions. You've been warned.

      pVertex[0] = X1;
      pVertex[1] = Y1;
      ((volatile unsigned long *)pVertex)[2] = (tu1<<16)|tv1;         // Merged using big endian rules
      pVertex[3] = 0;
      pVertex[4] = X2;
      pVertex[5] = Y2;
      ((volatile unsigned long *)pVertex)[6] = (tu2<<16)|tv1;         // Merged using big endian rules
      pVertex[7] = 0;
      pVertex[8] = X3;
      pVertex[9] = Y3;
      ((volatile unsigned long *)pVertex)[10] = (tu2<<16)|tv2;        // Merged using big endian rules
      pVertex[11] = 0;
      pVertex[12] = X4;
      pVertex[13] = Y4;
      ((volatile unsigned long *)pVertex)[14] = (tu1<<16)|tv2;        // Merged using big endian rules
      pVertex[15] = 0;
      pVertex+=16;

      dwNumChars--;
   }

   // Since we allocated vertex data space based on the string length, we now need to
   // add some dummy verts for any skipped characters (like newlines, etc.)
   while( dwNumChars )
   {
      for(int i = 0; i < 16; i++)
	     pVertex[i] = 0;

      pVertex += 16;
      dwNumChars--;
   }

   // Stop drawing vertices
   D3DDevice_EndVertices(pd3dDevice);

   // Undo window offsets
   font->m_fCursorX -= Winx;
   font->m_fCursorY -= Winy;

   d3d9_render_msg_post(font);
}

void xdk360_console_draw(void)
{
   xdk_d3d_video_t *vid = (xdk_d3d_video_t*)driver.video_data;
   D3DDevice *m_pd3dDevice = vid->d3d_render_device;

   // The top line
   unsigned int nTextLine = ( video_console.m_nCurLine - 
      video_console.m_cScreenHeight + video_console.m_cScreenHeightVirtual - 
      video_console.m_nScrollOffset + 1 )
      % video_console.m_cScreenHeightVirtual;

   d3d9_render_msg_pre(&m_Font);

   for( unsigned int nScreenLine = 0; nScreenLine < video_console.m_cScreenHeight; nScreenLine++ )
   {
      xdk_video_font_draw_text(&m_Font, (float)( video_console.m_cxSafeAreaOffset ),
      (float)( video_console.m_cySafeAreaOffset + video_console.m_fLineHeight * nScreenLine ), 
      video_console.m_Lines[nTextLine], 0.0f );

      nTextLine = ( nTextLine + 1 ) % video_console.m_cScreenHeightVirtual;
   }

   d3d9_render_msg_post(&m_Font);
}

static void xdk_convert_texture_to_as16_srgb( D3DTexture *pTexture )
{
   pTexture->Format.SignX = GPUSIGN_GAMMA;
   pTexture->Format.SignY = GPUSIGN_GAMMA;
   pTexture->Format.SignZ = GPUSIGN_GAMMA;

   XGTEXTURE_DESC desc;
   XGGetTextureDesc( pTexture, 0, &desc );

   //convert to AS_16_16_16_16 format
   pTexture->Format.DataFormat = g_MapLinearToSrgbGpuFormat[ (desc.Format & D3DFORMAT_TEXTUREFORMAT_MASK) >> D3DFORMAT_TEXTUREFORMAT_SHIFT ];
}
#endif

static void xdk_d3d_set_viewport(bool force_full)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
   int width, height;      // Set the viewport based on the current resolution
   int m_viewport_x_temp, m_viewport_y_temp, m_viewport_width_temp, m_viewport_height_temp;
   float m_zNear, m_zFar;

   d3d->d3d_render_device->Clear(0, NULL, D3DCLEAR_TARGET, 0xff000000, 1.0f, 0);
#if defined(_XBOX1)
   // Get the "video mode"
   d3d->video_mode = XGetVideoFlags();

   width  = d3d->d3dpp.BackBufferWidth;
   height = d3d->d3dpp.BackBufferHeight;
#elif defined(_XBOX360)
   width = d3d->video_mode.fIsHiDef ? 1280 : 640;
   height = d3d->video_mode.fIsHiDef ? 720 : 480;
#endif
   m_viewport_x_temp = 0;
   m_viewport_y_temp = 0;
   m_viewport_width_temp = width;
   m_viewport_height_temp = height;
   m_zNear = 0.0f;
   m_zFar = 1.0f;

   if (!force_full)
   {
      float desired_aspect = g_settings.video.aspect_ratio;
      float device_aspect = (float)width / height;
      float delta;

      // If the aspect ratios of screen and desired aspect ratio are sufficiently equal (floating point stuff), 
      if(g_settings.video.aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         delta = (desired_aspect / device_aspect - 1.0) / 2.0 + 0.5;
      	 m_viewport_x_temp = g_extern.console.screen.viewports.custom_vp.x;
      	 m_viewport_y_temp = g_extern.console.screen.viewports.custom_vp.y;
      	 m_viewport_width_temp = g_extern.console.screen.viewports.custom_vp.width;
      	 m_viewport_height_temp = g_extern.console.screen.viewports.custom_vp.height;
      }
      else if (device_aspect > desired_aspect)
      {
         delta = (desired_aspect / device_aspect - 1.0) / 2.0 + 0.5;
         m_viewport_x_temp = (int)(width * (0.5 - delta));
         m_viewport_width_temp = (int)(2.0 * width * delta);
         width = (unsigned)(2.0 * width * delta);
      }
      else
      {
         delta = (device_aspect / desired_aspect - 1.0) / 2.0 + 0.5;
         m_viewport_y_temp = (int)(height * (0.5 - delta));
         m_viewport_height_temp = (int)(2.0 * height * delta);
         height = (unsigned)(2.0 * height * delta);
      }
   }

   D3DVIEWPORT vp = {0};
   vp.Width  = m_viewport_width_temp;
   vp.Height = m_viewport_height_temp;
   vp.X      = m_viewport_x_temp;
   vp.Y      = m_viewport_y_temp;
   vp.MinZ   = m_zNear;
   vp.MaxZ   = m_zFar;
   d3d->d3d_render_device->SetViewport(&vp);

#ifdef _XBOX1
   font_x = vp.X;
   font_y = vp.Y;
#endif

   //if(gl->overscan_enable && !force_full)
   //{
   //	m_left = -gl->overscan_amount/2;
   //	m_right = 1 + gl->overscan_amount/2;
   //	m_bottom = -gl->overscan_amount/2;
   //}
}

static void xdk_d3d_set_rotation(void * data, unsigned orientation)
{
   (void)data;
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)data;
   FLOAT angle;

   switch(orientation)
   {
      case ORIENTATION_NORMAL:
         angle = M_PI * 0 / 180;
	 break;
      case ORIENTATION_VERTICAL:
         angle = M_PI * 270 / 180;
         break;
      case ORIENTATION_FLIPPED:
         angle = M_PI * 180 / 180;
         break;
      case ORIENTATION_FLIPPED_ROTATED:
         angle = M_PI * 90 / 180;
         break;
   }

#ifdef HAVE_HLSL
   /* TODO: Move to D3DXMATRIX here */
   hlsl_set_proj_matrix(XMMatrixRotationZ(angle));
#endif

   d3d->should_resize = TRUE;
}

#ifdef HAVE_FBO
static void xdk_d3d_init_fbo(xdk_d3d_video_t *d3d)
{
   if(!g_settings.video.render_to_texture)
      return;

   if (d3d->lpTexture_ot)
   {
      d3d->lpTexture_ot->Release();
      d3d->lpTexture_ot = NULL;
   }

   if (d3d->lpSurface)
   {
      d3d->lpSurface->Release();
      d3d->lpSurface = NULL;
   }

   d3d->d3d_render_device->CreateTexture(512 * g_settings.video.fbo.scale_x, 512 * g_settings.video.fbo.scale_y,
         1, 0, g_extern.console.screen.gamma_correction ? ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ) : D3DFMT_A8R8G8B8,
         0, &d3d->lpTexture_ot
		 , NULL
		 );

   d3d->d3d_render_device->CreateRenderTarget(512 * g_settings.video.fbo.scale_x, 512 * g_settings.video.fbo.scale_y,
         g_extern.console.screen.gamma_correction ? ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ) : D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 
	      0, 0, &d3d->lpSurface, NULL);

   d3d->lpTexture_ot_as16srgb = *d3d->lpTexture_ot;
   xdk_convert_texture_to_as16_srgb(d3d->lpTexture);
   xdk_convert_texture_to_as16_srgb(&d3d->lpTexture_ot_as16srgb);
   d3d->fbo_enabled = 1;
}
#endif

static void *xdk_d3d_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{
   if (driver.video_data)
      return driver.video_data;

   //we'll just use driver.video_data throughout here because it needs to
   //exist when we delegate initing to the context file
   driver.video_data = (xdk_d3d_video_t*)calloc(1, sizeof(xdk_d3d_video_t));
   if (!driver.video_data)
      return NULL;

   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   d3d->vsync = video->vsync;

#if defined(_XBOX1)
   d3d->driver = gfx_ctx_init_first(GFX_CTX_DIRECT3D8_API);
#elif defined(_XBOX360)
   d3d->driver = gfx_ctx_init_first(GFX_CTX_DIRECT3D9_API);
#endif
   if (!d3d->driver)
   {
      free(d3d);
      return NULL;
   }

   RARCH_LOG("Found D3D context: %s\n", d3d->driver->ident);

   d3d->driver->get_video_size(&d3d->full_x, &d3d->full_y);
   RARCH_LOG("Detecting screen resolution: %ux%u.\n", d3d->full_x, d3d->full_y);

   gfx_ctx_xdk_set_swap_interval(d3d->vsync ? 1 : 0);

#ifdef HAVE_HLSL
   if (!hlsl_shader_init())
   {
      RARCH_ERR("Shader init failed.\n");
	  d3d->driver->destroy();
	  free(d3d);
	  return NULL;
   }

   RARCH_LOG("D3D: Loaded %u program(s).\n", d3d_hlsl_num());
#endif

#ifdef HAVE_FBO
   xdk_d3d_init_fbo(d3d);
#endif

   xdk_d3d_set_rotation(d3d, g_extern.console.screen.orientation);

#if defined(_XBOX1)
   /* load debug fonts */
   XFONT_OpenDefaultFont(&d3d->debug_font);
   d3d->debug_font->SetBkMode(XFONT_TRANSPARENT);
   d3d->debug_font->SetBkColor(D3DCOLOR_ARGB(100,0,0,0));
   d3d->debug_font->SetTextHeight(14);
   d3d->debug_font->SetTextAntialiasLevel(d3d->debug_font->GetTextAntialiasLevel());

   font_x = 0;
   font_y = 0;
#elif defined(_XBOX360)
   HRESULT hr = d3d9_init_font("game:\\media\\Arial_12.xpr");

   if(hr < 0)
   {
      RARCH_ERR("Couldn't initialize HLSL shader fonts.\n");
   }
#endif

   //really returns driver.video_data to driver.video_data - see comment above
   return d3d;
}

static bool xdk_d3d_frame(void *data, const void *frame,
      unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   if (!frame)
      return true;

   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)data;
#ifdef HAVE_FBO
   D3DSurface* pRenderTarget0;
#endif
   bool menu_enabled = g_extern.console.rmenu.state.rmenu.enable;
#ifdef _XBOX1
   bool fps_enable = g_extern.console.rmenu.state.msg_fps.enable;
   unsigned flicker_filter = g_extern.console.screen.state.flicker_filter.value;
   bool soft_filter_enable = g_extern.console.screen.state.soft_filter.enable;
#endif

   if (d3d->last_width != width || d3d->last_height != height)
   {
      D3DLOCKED_RECT d3dlr;

      d3d->lpTexture->LockRect(0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK);
      memset(d3dlr.pBits, 0, 512 * d3dlr.Pitch);
      d3d->lpTexture->UnlockRect(0);

#if defined(_XBOX1)
      float tex_w = width;  // / 512.0f;
      float tex_h = height; // / 512.0f;

      DrawVerticeFormats verts[] = {
         { -1.0f, -1.0f, 1.0f, 0.0f,  tex_h },
         {  1.0f, -1.0f, 1.0f, tex_w, tex_h },
         { -1.0f,  1.0f, 1.0f, 0.0f,  0.0f },
         {  1.0f,  1.0f, 1.0f, tex_w, 0.0f },
      };
#elif defined(_XBOX360)
      float tex_w = width / 512.0f;
      float tex_h = height / 512.0f;

      DrawVerticeFormats verts[] = {
         { -1.0f, -1.0f, 0.0f,  tex_h },
         {  1.0f, -1.0f, tex_w, tex_h },
         { -1.0f,  1.0f, 0.0f,  0.0f },
         {  1.0f,  1.0f, tex_w, 0.0f },
      };
#endif

      // Align texels and vertices (D3D9 quirk).
      for (unsigned i = 0; i < 4; i++)
      {
         verts[i].x -= 0.5f / 512.0f;
         verts[i].y += 0.5f / 512.0f;
      }

#if defined(_XBOX1)
      BYTE *verts_ptr;
#elif defined(_XBOX360)
      void *verts_ptr;
#endif
      d3d->vertex_buf->Lock(0, 0, &verts_ptr, 0);
      memcpy(verts_ptr, verts, sizeof(verts));
      d3d->vertex_buf->Unlock();

      d3d->last_width = width;
      d3d->last_height = height;
   }

#ifdef HAVE_FBO
   if (d3d->fbo_enabled)
   {
      d3d->d3d_render_device->GetRenderTarget(0, &pRenderTarget0);
      d3d->d3d_render_device->SetRenderTarget(0, d3d->lpSurface);
   }
#endif

   if (d3d->should_resize)
      xdk_d3d_set_viewport(false);

   d3d->frame_count++;
#ifdef _XBOX360
   d3d->d3d_render_device->Clear(0, NULL, D3DCLEAR_TARGET,
         0xff000000, 1.0f, 0);
#endif

   d3d->d3d_render_device->SetTexture(0, d3d->lpTexture);

#ifdef HAVE_HLSL
   hlsl_use(1);
#endif

#ifdef HAVE_FBO
   if(d3d->fbo_enabled)
   {
#ifdef HAVE_HLSL
      hlsl_set_params(width, height, 512, 512, g_settings.video.fbo.scale_x * width,
            g_settings.video.fbo.scale_y * height, d3d->frame_count);
#endif
      D3DVIEWPORT vp = {0};
      vp.Width  = g_settings.video.fbo.scale_x * width;
      vp.Height = g_settings.video.fbo.scale_y * height;
      vp.X      = 0;
      vp.Y      = 0;
      vp.MinZ   = 0.0f;
      vp.MaxZ   = 1.0f;
      d3d->d3d_render_device->SetViewport(&vp);
   }
   else
#endif
   {
#ifdef HAVE_HLSL
      hlsl_set_params(width, height, 512, 512, d3d->d3dpp.BackBufferWidth,
            d3d->d3dpp.BackBufferHeight, d3d->frame_count);
#endif
   }

   D3DLOCKED_RECT d3dlr;
   d3d->lpTexture->LockRect(0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK);
   size_t size_screen = g_settings.video.color_format ? sizeof(uint32_t) : sizeof(uint16_t);
   for (unsigned y = 0; y < height; y++)
   {
      const uint8_t *in = (const uint8_t*)frame + y * pitch;
      uint8_t *out = (uint8_t*)d3dlr.pBits + y * d3dlr.Pitch;
      memcpy(out, in, width * size_screen);
   }
   d3d->lpTexture->UnlockRect(0);

   d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_MINFILTER, g_settings.video.smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
   d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_MAGFILTER, g_settings.video.smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
   d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
   d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);

#if defined(_XBOX1)
   d3d->d3d_render_device->SetVertexShader(D3DFVF_XYZ | D3DFVF_TEX1);

   D3DXMATRIX p_out, p_rotate;
   D3DXMatrixIdentity(&p_out);
   D3DXMatrixRotationZ(&p_rotate, angle);

   d3d->d3d_render_device->SetTransform(D3DTS_WORLD, &p_rotate);
   d3d->d3d_render_device->SetTransform(D3DTS_VIEW, &p_out);
   d3d->d3d_render_device->SetTransform(D3DTS_PROJECTION, &p_out);

   d3d->d3d_render_device->SetStreamSource(0, d3d->vertex_buf, sizeof(DrawVerticeFormats));
   d3d->d3d_render_device->Clear(0, NULL, D3DCLEAR_TARGET, 0xff000000, 1.0f, 0);

   d3d->d3d_render_device->BeginScene();
   d3d->d3d_render_device->SetFlickerFilter(flicker_filter);
   d3d->d3d_render_device->SetSoftDisplayFilter(soft_filter_enable);
   d3d->d3d_render_device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
   d3d->d3d_render_device->EndScene();
#elif defined(_XBOX360)

   d3d->d3d_render_device->SetVertexDeclaration(d3d->v_decl);
   d3d->d3d_render_device->SetStreamSource(0, d3d->vertex_buf,
	   0,
	   sizeof(DrawVerticeFormats));

   d3d->d3d_render_device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
#endif

#ifdef HAVE_FBO
   if(d3d->fbo_enabled)
   {
      d3d->d3d_render_device->Resolve(D3DRESOLVE_RENDERTARGET0, NULL, d3d->lpTexture_ot,
         NULL, 0, 0, NULL, 0, 0, NULL);

      d3d->d3d_render_device->SetRenderTarget(0, pRenderTarget0);
      pRenderTarget0->Release();
      d3d->d3d_render_device->SetTexture(0, &d3d->lpTexture_ot_as16srgb);

#ifdef HAVE_HLSL
      hlsl_use(2);
      hlsl_set_params(g_settings.video.fbo.scale_x * width, g_settings.video.fbo.scale_y * height, g_settings.video.fbo.scale_x * 512, g_settings.video.fbo.scale_y * 512, d3d->d3dpp.BackBufferWidth,
            d3d->d3dpp.BackBufferHeight, d3d->frame_count);
#endif
      xdk_d3d_set_viewport(false);

      d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_MINFILTER, g_settings.video.second_pass_smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
      d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_MAGFILTER, g_settings.video.second_pass_smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
      d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
      d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
      d3d->d3d_render_device->SetVertexDeclaration(d3d->v_decl);
      d3d->d3d_render_device->SetStreamSource(0, d3d->vertex_buf,
		  0,
		  sizeof(DrawVerticeFormats));
      d3d->d3d_render_device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
   }
#endif

#if defined(_XBOX1)
   if(fps_enable)
   {
      MEMORYSTATUS stat;
      GlobalMemoryStatus(&stat);

      //Output memory usage

      char fps_txt[128];
      char buf[128];
      bool ret = false;
      snprintf(buf, sizeof(buf), "%.2f MB free / %.2f MB total", stat.dwAvailPhys/(1024.0f*1024.0f), stat.dwTotalPhys/(1024.0f*1024.0f));
      xfonts_render_msg_place(d3d, font_x + 30, font_y + 50, 0 /* scale */, buf);

      gfx_fps_title(fps_txt, sizeof(fps_txt));
      xfonts_render_msg_place(d3d, font_x + 30, font_y + 70, 0 /* scale */, fps_txt);
   }

   if (msg)
      xfonts_render_msg_place(d3d, 60, 365, 0, msg); //TODO: dehardcode x/y here for HD (720p) mode
#elif defined(_XBOX360)
   if (msg && !menu_enabled)
   {
	   xdk360_console_format(msg);
      xdk360_console_draw();
   }
#endif

   if(!d3d->block_swap)
      gfx_ctx_xdk_swap_buffers();

   return true;
}

static void xdk_d3d_set_nonblock_state(void *data, bool state)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)data;

   if(d3d->vsync)
   {
      RARCH_LOG("D3D Vsync => %s\n", state ? "off" : "on");
      gfx_ctx_xdk_set_swap_interval(state ? 0 : 1);
   }
}

static bool xdk_d3d_alive(void *data)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)data;
   check_window(d3d);
   return !d3d->quitting;
}

static bool xdk_d3d_focus(void *data)
{
   (void)data;
   return gfx_ctx_window_has_focus();
}

static void xdk_d3d_start(void)
{
   video_info_t video_info = {0};

   video_info.vsync = g_settings.video.vsync;
   video_info.force_aspect = false;
   video_info.fullscreen = true;
   video_info.smooth = g_settings.video.smooth;
   video_info.input_scale = 2;

   driver.video_data = xdk_d3d_init(&video_info, NULL, NULL);
}

static void xdk_d3d_restart(void)
{
}

static void xdk_d3d_stop(void)
{
   void *data = driver.video_data;

   xdk_d3d_free(data);

   driver.video_data = NULL;
}

static void xdk_d3d_apply_state_changes(void)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
   d3d->should_resize = true;
}

static void xdk_d3d_set_aspect_ratio(void *data, unsigned aspectratio_index)
{
   (void)data;
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   if(g_settings.video.aspect_ratio_idx == ASPECT_RATIO_AUTO)
      rarch_set_auto_viewport(g_extern.frame_cache.width, g_extern.frame_cache.height);
   else if(g_settings.video.aspect_ratio_idx == ASPECT_RATIO_CORE)
      rarch_set_core_viewport();

   g_settings.video.aspect_ratio = aspectratio_lut[g_settings.video.aspect_ratio_idx].value;
   g_settings.video.force_aspect = false;
   d3d->should_resize = true;
}

const video_driver_t video_xdk_d3d = {
   xdk_d3d_init,
   xdk_d3d_frame,
   xdk_d3d_set_nonblock_state,
   xdk_d3d_alive,
   xdk_d3d_focus,
   NULL,
   xdk_d3d_free,
   "xdk_d3d",
   xdk_d3d_start,
   xdk_d3d_stop,
   xdk_d3d_restart,
   xdk_d3d_apply_state_changes,
   xdk_d3d_set_aspect_ratio,
   xdk_d3d_set_rotation,
};
