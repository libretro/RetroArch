/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018 - Francisco Javier Trujillo Mata - fjtrujy
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

#include <kernel.h>
#include <gsKit.h>
#include <gsInline.h>

#include "../../driver.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

#include "../../libretro-common/include/libretro_gskit_ps2.h"

/* Generic tint color */
#define GS_TEXT GS_SETREG_RGBA(0x80, 0x80, 0x80, 0x80)
/* turn black GS Screen */
#define GS_BLACK GS_SETREG_RGBA(0x00, 0x00, 0x00, 0x80)

#define NUM_RM_VMODES 7
#define PS2_RESOLUTION_LAST NUM_RM_VMODES - 1
#define RM_VMODE_AUTO 0

/* RM Vmode -> GS Vmode conversion table */
struct rm_mode
{
   char mode;
   short int width;
   short int height;
   short int VCK;
   short int interlace;
   short int field;
   short int PAR1; /* Pixel Aspect Ratio 1 (For video modes with non-square pixels, like PAL/NTSC) */
   short int PAR2; /* Pixel Aspect Ratio 2 (For video modes with non-square pixels, like PAL/NTSC) */
   char *desc;
};

static struct rm_mode rm_mode_table[NUM_RM_VMODES] = {
    /* SDTV modes */
    {-1, 704, -1, 4, GS_INTERLACED, GS_FIELD, -1, 11, "AUTO"},
    {GS_MODE_PAL, 704, 576, 4, GS_INTERLACED, GS_FIELD, 12, 11, "PAL@50Hz"},
    {GS_MODE_NTSC, 704, 480, 4, GS_INTERLACED, GS_FIELD, 10, 11, "NTSC@60Hz"},
    /* SDTV special modes */
    {GS_MODE_PAL, 704, 288, 4, GS_NONINTERLACED, GS_FRAME, 12, 22, "PAL@50Hz-288p"},
    {GS_MODE_NTSC, 704, 240, 4, GS_NONINTERLACED, GS_FRAME, 10, 22, "NTSC@60Hz-240p"},
    /* EDTV modes (component cable!) */
    {GS_MODE_DTV_480P, 704, 480, 2, GS_NONINTERLACED, GS_FRAME, 10, 11, "DTV480P@60Hz"},
    {GS_MODE_DTV_576P, 704, 576, 2, GS_NONINTERLACED, GS_FRAME, 12, 11, "DTV576P@50Hz"},
};

typedef struct ps2_video
{
   /* I need to create this additional field
    * to be used in the font driver*/
   bool clearVRAM_font;
   bool menuVisible;
   bool vsync;
   int vsync_callback_id;
   bool force_aspect;

   int8_t vmode;
   int video_window_offset_x;
   int video_window_offset_y;

   int PSM;
   int tex_filter;
   int menu_filter;

   video_viewport_t vp;

   /* Palette in the cores */
   struct retro_hw_render_interface_gskit_ps2 iface;

   GSGLOBAL *gsGlobal;
   GSTEXTURE *menuTexture;
   GSTEXTURE *coreTexture;

   /* Last scaling state, for detecting changes */
   int iTextureWidth;
   int iTextureHeight;
   float fDAR;
   bool bScaleInteger;
   struct retro_hw_ps2_insets padding;

   /* Current scaling calculation result */
   int iDisplayWidth;
   int iDisplayHeight;
} ps2_video_t;

static int vsync_sema_id;

/* PRIVATE METHODS */
static int vsync_handler()
{
   iSignalSema(vsync_sema_id);

   ExitHandler();
   return 0;
}

static void rmEnd(ps2_video_t *ps2)
{
   if (!ps2->gsGlobal)
      return;

   gsKit_deinit_global(ps2->gsGlobal);
   gsKit_remove_vsync_handler(ps2->vsync_callback_id);
}

static void updateOffSetsIfNeeded(ps2_video_t *ps2)
{
   bool shouldUpdate = false;
   settings_t *settings = config_get_ptr();
   int video_window_offset_x = settings->ints.video_window_offset_x;
   int video_window_offset_y = settings->ints.video_window_offset_y;

   if (video_window_offset_x != ps2->video_window_offset_x || video_window_offset_y != ps2->video_window_offset_y)
      shouldUpdate = true;

   if (!shouldUpdate)
      return;

   ps2->video_window_offset_x = video_window_offset_x;
   ps2->video_window_offset_y = video_window_offset_y;

   gsKit_set_display_offset(ps2->gsGlobal, ps2->video_window_offset_x * rm_mode_table[ps2->vmode].VCK, ps2->video_window_offset_y);
   RARCH_LOG("PS2_GFX Change offset: %d, %d\n", ps2->video_window_offset_x, ps2->video_window_offset_y);
}

/* Copy of gsKit_sync_flip, but without the 'flip' */
static void gsKit_sync(GSGLOBAL *gsGlobal)
{
   if (!gsGlobal->FirstFrame)
      WaitSema(vsync_sema_id);

   while (PollSema(vsync_sema_id) >= 0)
      ;
}

/* Copy of gsKit_sync_flip, but without the 'sync' */
static void gsKit_flip(GSGLOBAL *gsGlobal)
{
   if (!gsGlobal->FirstFrame)
   {
      if (gsGlobal->DoubleBuffering == GS_SETTING_ON)
      {
         GS_SET_DISPFB2(gsGlobal->ScreenBuffer[gsGlobal->ActiveBuffer & 1] / 8192,
                        gsGlobal->Width / 64, gsGlobal->PSM, 0, 0);

         gsGlobal->ActiveBuffer ^= 1;
      }
   }

   gsKit_setactive(gsGlobal);
}

static void rmSetMode(ps2_video_t *ps2, int force)
{
   struct rm_mode *mode;
   global_t *global = global_get_ptr();

   /* we don't want to set the vmode without a reason... */
   if (ps2->vmode == global->console.screen.resolutions.current.id && force == 0)
      return;

   /* Cleanup previous gsKit instance */
   if (ps2->vmode >= 0)
   {
      rmEnd(ps2);
      /* Set new mode */
      global->console.screen.resolutions.current.id = ps2->vmode;
   }
   else
      /* first driver init */
      ps2->vmode = global->console.screen.resolutions.current.id;

   mode = &rm_mode_table[ps2->vmode];

   /* Invalidate scaling state */
   ps2->iTextureWidth = 0;
   ps2->iTextureHeight = 0;

   ps2->gsGlobal = gsKit_init_global();
   gsKit_TexManager_setmode(ps2->gsGlobal, ETM_DIRECT);
   ps2->vsync_callback_id = gsKit_add_vsync_handler(vsync_handler);
   ps2->gsGlobal->Mode = mode->mode;
   ps2->gsGlobal->Width = mode->width;
   ps2->gsGlobal->Height = mode->height;
   ps2->gsGlobal->Interlace = mode->interlace;
   ps2->gsGlobal->Field = mode->field;
   ps2->gsGlobal->PSM = GS_PSM_CT16;
   ps2->gsGlobal->PSMZ = GS_PSMZ_16S;
   ps2->gsGlobal->DoubleBuffering = GS_SETTING_ON;
   ps2->gsGlobal->ZBuffering = GS_SETTING_OFF;
   ps2->gsGlobal->PrimAlphaEnable = GS_SETTING_OFF;

   if ((ps2->gsGlobal->Interlace == GS_INTERLACED) && (ps2->gsGlobal->Field == GS_FRAME))
      ps2->gsGlobal->Height /= 2;

   /* Coordinate space ranges from 0 to 4096 pixels
   * Center the buffer in the coordinate space */
   ps2->gsGlobal->OffsetX = ((4096 - ps2->gsGlobal->Width) / 2) * 16;
   ps2->gsGlobal->OffsetY = ((4096 - ps2->gsGlobal->Height) / 2) * 16;

   gsKit_init_screen(ps2->gsGlobal);
   gsKit_mode_switch(ps2->gsGlobal, GS_ONESHOT);

   gsKit_set_test(ps2->gsGlobal, GS_ZTEST_OFF);
   gsKit_set_test(ps2->gsGlobal, GS_ATEST_OFF);

   /* reset the contents of the screen to avoid garbage being displayed */
   gsKit_clear(ps2->gsGlobal, GS_BLACK);
   gsKit_sync(ps2->gsGlobal);
   gsKit_flip(ps2->gsGlobal);

   RARCH_LOG("PS2_GFX New vmode: %d, %d x %d\n", ps2->vmode, ps2->gsGlobal->Width, ps2->gsGlobal->Height);

   updateOffSetsIfNeeded(ps2);
}

static void rmInit(ps2_video_t *ps2)
{
   ee_sema_t sema;
   sema.init_count = 0;
   sema.max_count = 1;
   sema.option = 0;
   vsync_sema_id = CreateSema(&sema);

   short int mode = gsKit_check_rom();

   rm_mode_table[RM_VMODE_AUTO].mode = mode;
   rm_mode_table[RM_VMODE_AUTO].height = (mode == GS_MODE_PAL) ? 576 : 480;
   rm_mode_table[RM_VMODE_AUTO].PAR1 = (mode == GS_MODE_PAL) ? 12 : 10;

   dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
               D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);

   /* Initialize the DMAC */
   dmaKit_chan_init(DMA_CHANNEL_GIF);

   rmSetMode(ps2, 1);
}

static GSTEXTURE *prepare_new_texture(void)
{
   GSTEXTURE *texture = (GSTEXTURE *)calloc(1, sizeof(GSTEXTURE));
   return texture;
}

static void init_ps2_video(ps2_video_t *ps2)
{
   ps2->vmode = -1;
   rmInit(ps2);

   ps2->vp.x = 0;
   ps2->vp.y = 0;
   ps2->vp.width = ps2->gsGlobal->Width;
   ps2->vp.height = ps2->gsGlobal->Height;
   ps2->vp.full_width = ps2->gsGlobal->Width;
   ps2->vp.full_height = ps2->gsGlobal->Height;

   ps2->menuTexture = prepare_new_texture();
   ps2->coreTexture = prepare_new_texture();

   ps2->video_window_offset_x = 0;
   ps2->video_window_offset_y = 0;

   /* Used for cores that supports palette */
   ps2->iface.interface_type = RETRO_HW_RENDER_INTERFACE_GSKIT_PS2;
   ps2->iface.interface_version = RETRO_HW_RENDER_INTERFACE_GSKIT_PS2_VERSION;
   ps2->iface.coreTexture = ps2->coreTexture;
}

static void ps2_gfx_deinit_texture(GSTEXTURE *texture)
{
   texture->Mem = NULL;
   texture->Clut = NULL;
}

static void set_texture(GSTEXTURE *texture, const void *frame,
                        int width, int height, int PSM, int filter)
{
   texture->Width = width;
   texture->Height = height;
   texture->PSM = PSM;
   texture->Filter = filter;
   texture->Mem = (void *)frame;
}

static int ABS(int v)
{
   return (v >= 0) ? v : -v;
}

static void setupScalingMode(ps2_video_t *ps2, int iWidth, int iHeight, float fDAR, bool bScaleInteger)
{
   GSGLOBAL *gsGlobal = ps2->gsGlobal;
   struct rm_mode *currMode = &rm_mode_table[ps2->vmode];
   int iBestFBWidth = currMode->width;
   int iBestMagH = currMode->VCK - 1;
   float fPAR;

   /* Calculate the pixel aspect ratio (PAR) */
   fPAR = (float)currMode->PAR2 / (float)currMode->PAR1;

#if defined(DEBUG)
   printf("Aspect ratio: %.4f x %.4f = %.4f\n", fDAR, fPAR, fDAR * fPAR);
#endif

   if (bScaleInteger == false)
   {
      /* Assume black bars left/right */
      ps2->iDisplayHeight = currMode->height;
      ps2->iDisplayWidth = (int)((float)ps2->iDisplayHeight * fDAR * fPAR + 0.5f);
      if (ps2->iDisplayWidth > currMode->width)
      {
         /* Really wide screen, black bars top/bottom */
         ps2->iDisplayWidth = currMode->width;
         ps2->iDisplayHeight = (int)((float)ps2->iDisplayWidth / (fDAR * fPAR) + 0.5f);
      }
   }
   else
   {
      /* Best match the framebuffer width/height to a multiple of the texture
       * Width, rounded down so it always fits */
      int iHeightScale = MAX(1, currMode->height / iHeight);
      ps2->iDisplayHeight = iHeight * iHeightScale;
      /* Height, rounded */
      int iWidthScale = MAX(1, (int)((float)ps2->iDisplayHeight * fDAR * fPAR + (float)(iWidth / 2)) / iWidth);
      ps2->iDisplayWidth = iWidth * iWidthScale;

#if defined(DEBUG)
      printf("Integer scaling:\n");
      printf("- Width  = %d x %d = %d\n", iWidth, iWidthScale, ps2->iDisplayWidth);
      printf("- Height = %d x %d = %d\n", iHeight, iHeightScale, ps2->iDisplayHeight);
#endif

      if (currMode->VCK > 1 && ps2->iDisplayWidth < currMode->width)
      {
         /* We try to best match the number of "VCK" units, for the best output
          * For 576i/480i: 1 pixel = 4 VCK (4x super-resolution)
          * For 576p/480p: 1 pixel = 2 VCK (2x super-resolution) */
         int iTargetVCK = (int)((float)ps2->iDisplayHeight * fDAR * fPAR * (float)currMode->VCK + 0.5f);
         int iBestVCK = ps2->iDisplayWidth * currMode->VCK;
         /* Try all possible framebuffer widths */
#if defined(DEBUG)
         printf("Find match for %d * SCALE * MagH = %d VCK (current = %d VCK)\n", iWidth, iTargetVCK, iBestVCK);
#endif
         int iFBWidth;
         for (iFBWidth = 64; iFBWidth <= currMode->width; iFBWidth += 64)
         {
            /* Ignore too small framebuffers */
            if (iFBWidth < iWidth)
               continue;

            iWidthScale = iFBWidth / iWidth;

            /* Try all possible magnifications */
            int iMagH;
            for (iMagH = 0; iMagH < 15; iMagH++)
            {
               int iVCK = iWidth * iWidthScale * (iMagH + 1);
               if (ABS(iTargetVCK - iVCK) < ABS(iTargetVCK - iBestVCK))
               {
#if defined(DEBUG)
                  printf("- found %d * %d * %d = %d\n", iWidth, iWidthScale, iMagH + 1, iVCK);
#endif

                  /* Better match */
                  iBestVCK = iVCK;
                  iBestFBWidth = iFBWidth;
                  iBestMagH = iMagH;
                  ps2->iDisplayWidth = iWidth * iWidthScale;
               }
            }
         }
      }
   }

   if ((gsGlobal->Interlace == GS_INTERLACED) && (gsGlobal->Field == GS_FRAME))
      ps2->iDisplayHeight /= 2;

#if defined(DEBUG)
   printf("Texture resolution:\n");
   printf("- Width  = %d x %.2f = %d\n", iWidth, (float)ps2->iDisplayWidth / (float)iWidth, ps2->iDisplayWidth);
   printf("- Height = %d x %.2f = %d\n", iHeight, (float)ps2->iDisplayHeight / (float)iHeight, ps2->iDisplayHeight);
   printf("Setting custom framebuffer:\n");
   printf("- Width  = %d x %d / %d = %d\n", iBestFBWidth, iBestMagH + 1, currMode->VCK, iBestFBWidth * (iBestMagH + 1) / currMode->VCK);
   printf("- Height = %d x %d     = %d\n", currMode->height, gsGlobal->MagV + 1, currMode->height * (gsGlobal->MagV + 1));
#endif

   /* Center on screen by adding the difference (in VCK units). */
   gsGlobal->StartX += (gsGlobal->Width * (gsGlobal->MagH + 1) - iBestFBWidth * (iBestMagH + 1)) / 2;
   /* Calculate the actual display width and height, again */
   gsGlobal->DW = (iBestMagH + 1) * iBestFBWidth;
   /* Override magh */
   gsGlobal->MagH = iBestMagH;

   /* Reset VRAM allocation */
   gsGlobal->CurrentPointer = 0;
   /* Allocate new framebuffer(s) */
   gsGlobal->ScreenBuffer[0] = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(iBestFBWidth, currMode->height, gsGlobal->PSM), GSKIT_ALLOC_SYSBUFFER);
   if (gsGlobal->DoubleBuffering == GS_SETTING_ON)
      gsGlobal->ScreenBuffer[1] = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(iBestFBWidth, currMode->height, gsGlobal->PSM), GSKIT_ALLOC_SYSBUFFER);
   /* Set the new framebuffer as display buffer */
   gsGlobal->Width = iBestFBWidth;
   gsKit_display_buffer(gsGlobal);

   /* Update DISPLAY1/2 register (code from gsKit) */
   GS_SET_DISPLAY1(
       gsGlobal->StartX + gsGlobal->StartXOffset,
       gsGlobal->StartY + gsGlobal->StartYOffset,
       gsGlobal->MagH,
       gsGlobal->MagV,
       gsGlobal->DW - 1,
       gsGlobal->DH - 1);
   GS_SET_DISPLAY2(
       gsGlobal->StartX + gsGlobal->StartXOffset,
       gsGlobal->StartY + gsGlobal->StartYOffset,
       gsGlobal->MagH,
       gsGlobal->MagV,
       gsGlobal->DW - 1,
       gsGlobal->DH - 1);
}

static void *ps2_gfx_init(const video_info_t *video,
                          input_driver_t **input, void **input_data)
{
   void *ps2input = NULL;
   ps2_video_t *ps2 = (ps2_video_t *)calloc(1, sizeof(ps2_video_t));

   *input_data = NULL;

   if (!ps2)
      return NULL;

   init_ps2_video(ps2);
   if (video->font_enable)
      font_driver_init_osd(ps2,
                           video,
                           false,
                           video->is_threaded,
                           FONT_DRIVER_RENDER_PS2);

   ps2->PSM = (video->rgb32 ? GS_PSM_CT32 : GS_PSM_CT16);
   ps2->tex_filter = video->smooth ? GS_FILTER_LINEAR : GS_FILTER_NEAREST;
   ps2->force_aspect = video->force_aspect;
   ps2->vsync = video->vsync;

   if (input && input_data)
   {
      settings_t *settings = config_get_ptr();
      ps2input = input_driver_init_wrap(&input_ps2,
                                        settings->arrays.input_joypad_driver);
      *input = ps2input ? &input_ps2 : NULL;
      *input_data = ps2input;
   }

   return ps2;
}

static bool ps2_gfx_frame(void *data, const void *frame,
                          unsigned width, unsigned height, uint64_t frame_count,
                          unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   ps2_video_t *ps2 = (ps2_video_t *)data;
   GSGLOBAL *gsGlobal = ps2->gsGlobal;
   struct font_params *osd_params = (struct font_params *)&video_info->osd_stat_params;
   bool statistics_show = video_info->statistics_show;
   settings_t *settings = config_get_ptr();
   GSTEXTURE *tex = ps2->coreTexture;

   if (!width || !height)
      return false;

#if defined(DEBUG)
   if (frame_count % 180 == 0)
      printf("ps2_gfx_frame %llu\n", frame_count);
#endif

   /* Check if user change offset values */
   updateOffSetsIfNeeded(ps2);

   if (frame)
   {
      /* New frame from core, update */
      float fDAR = ps2->force_aspect ? video_driver_get_aspect_ratio() : 0;
      bool bScaleInteger = settings->bools.video_scale_integer;

      /* Checking if the transfer is done in the core */
      if (frame != RETRO_HW_FRAME_BUFFER_VALID)
      {
         /* SW rendered texture */
         int shifh_per_bytes = (ps2->PSM == GS_PSM_CT32) ? 2 : 1;
         int real_width = pitch >> shifh_per_bytes;
         set_texture(tex, frame, real_width, height, ps2->PSM, ps2->tex_filter);

         /* Padding */
         ps2->padding = empty_ps2_insets;
         ps2->padding.right = real_width - width;
      }
      else
      {
         /* "HW" rendered texture */
         /* Set current filter mode */
         tex->Filter = ps2->tex_filter;

         /* Padding */
         ps2->padding = ps2->iface.padding;
      }

      /* Texture dimensions */
      int iTextureWidth = tex->Width - ps2->padding.left - ps2->padding.right;
      int iTextureHeight = tex->Height - ps2->padding.top - ps2->padding.bottom;
      if (ps2->iTextureWidth != iTextureWidth ||
          ps2->iTextureHeight != iTextureHeight ||
          ps2->fDAR != fDAR ||
          ps2->bScaleInteger != bScaleInteger)
      {
         /* Scaling changed, try to find best matching output mode */
         ps2->iTextureWidth = iTextureWidth;
         ps2->iTextureHeight = iTextureHeight;
         ps2->fDAR = fDAR;
         ps2->bScaleInteger = bScaleInteger;
         setupScalingMode(ps2, iTextureWidth, iTextureHeight, fDAR, bScaleInteger);
      }

      gsKit_TexManager_invalidate(ps2->gsGlobal, tex);
   }

   /* Center texture on framebuffer */
   float fDisplayOffsetX = (gsGlobal->Width - ps2->iDisplayWidth + 1) / 2 - 0.5f;
   float fDisplayOffsetY = (gsGlobal->Height - ps2->iDisplayHeight + 1) / 2 - 0.5f;
   /* Draw */
   gsGlobal->PrimAlphaEnable = GS_SETTING_OFF;
   gsKit_TexManager_bind(gsGlobal, tex);
   gsKit_prim_sprite_texture(gsGlobal, tex,
                             fDisplayOffsetX,                        /* X1 */
                             fDisplayOffsetY,                        /* Y1 */
                             ps2->padding.left,                      /* U1 */
                             ps2->padding.top,                       /* V1 */
                             fDisplayOffsetX + ps2->iDisplayWidth,   /* X2 */
                             fDisplayOffsetY + ps2->iDisplayHeight,  /* Y2 */
                             ps2->padding.left + ps2->iTextureWidth, /* U2 */
                             ps2->padding.top + ps2->iTextureHeight, /* V2 */
                             1,
                             GS_TEXT);

   if (ps2->menuVisible)
   {
      bool texture_empty = !ps2->menuTexture->Width || !ps2->menuTexture->Height;
      if (!texture_empty)
      {
#define A_COLOR_SOURCE 0
#define A_COLOR_DEST 1
#define A_COLOR_NULL 2
#define A_ALPHA_SOURCE 0
#define A_ALPHA_DEST 1
#define A_ALPHA_FIX 2
         /* (A - B) * C + D */
         gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(A_COLOR_DEST, A_COLOR_NULL, A_ALPHA_FIX, A_COLOR_SOURCE, 0x20), 0);
         gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
         gsKit_prim_sprite_texture(gsGlobal, ps2->menuTexture,
                                   -0.5f,                           /* X1 */
                                   -0.5f,                           /* Y1 */
                                   0,                               /* U1 */
                                   0,                               /* V1 */
                                   -0.5f + (float)gsGlobal->Width,  /* X2 */
                                   -0.5f + (float)gsGlobal->Height, /* Y2 */
                                   ps2->menuTexture->Width,         /* U2 */
                                   ps2->menuTexture->Height,        /* V2 */
                                   2,
                                   GS_TEXT);
      }
   }
   else if (statistics_show)
   {
      if (osd_params)
         font_driver_render_msg(ps2, video_info->stat_text, osd_params, NULL);
   }

   if (!string_is_empty(msg))
      font_driver_render_msg(ps2, msg, NULL, NULL);

   if (gsGlobal->DoubleBuffering == GS_SETTING_OFF)
   {
      /* Without double buffering:
       * - Wait for VSync
       * - Draw to front buffer (during VSync, so it's not visible to the user) */
      if (ps2->vsync)
         gsKit_sync(gsGlobal);
      gsKit_queue_exec(gsGlobal);
   }
   else
   {
      /* With double buffering:
       * - Draw to back buffer (invisible to user)
       * - Make sure drawing is completed (gsKit_finish)
       * - Wait for VSync
       * - Flip (back and front) buffers */
      gsKit_queue_exec(gsGlobal);
      gsKit_finish();
      if (ps2->vsync)
         gsKit_sync(gsGlobal);
      gsKit_flip(gsGlobal);
   }

   gsKit_TexManager_nextFrame(gsGlobal);
   gsKit_clear(gsGlobal, GS_BLACK);

   return true;
}

static void ps2_gfx_set_nonblock_state(void *data, bool toggle,
                                       bool adaptive_vsync_enabled, unsigned swap_interval)
{
   ps2_video_t *ps2 = (ps2_video_t *)data;

   if (ps2)
      ps2->vsync = !toggle;
}

static bool ps2_gfx_alive(void *data) { return true; }
static bool ps2_gfx_focus(void *data) { return true; }
static bool ps2_gfx_suppress_screensaver(void *data, bool enable) { return false; }
static bool ps2_gfx_has_windowed(void *data) { return false; }

static void ps2_gfx_free(void *data)
{
   ps2_video_t *ps2 = (ps2_video_t *)data;

   gsKit_clear(ps2->gsGlobal, GS_BLACK);
   gsKit_vram_clear(ps2->gsGlobal);

   font_driver_free_osd();

   ps2_gfx_deinit_texture(ps2->menuTexture);
   ps2_gfx_deinit_texture(ps2->coreTexture);

   free(ps2->menuTexture);
   free(ps2->coreTexture);

   rmEnd(ps2);

   if (vsync_sema_id >= 0)
      DeleteSema(vsync_sema_id);

   free(data);
}

static bool ps2_gfx_set_shader(void *data,
                               enum rarch_shader_type type, const char *path) { return false; }

static void ps2_set_video_mode(void *data, unsigned fbWidth, unsigned lines,
                               bool fullscreen)
{
   ps2_video_t *ps2 = (ps2_video_t *)data;
   if (!ps2)
      return;

   rmSetMode(ps2, 0);
}

static void ps2_set_filtering(void *data, unsigned index, bool smooth, bool ctx_scaling)
{
   ps2_video_t *ps2 = (ps2_video_t *)data;

   ps2->menu_filter = smooth ? GS_FILTER_LINEAR : GS_FILTER_NEAREST;
}

static void ps2_get_video_output_size(void *data,
                                      unsigned *width, unsigned *height, char *desc, size_t desc_len)
{
   ps2_video_t *ps2 = (ps2_video_t *)data;
   if (!ps2)
      return;

   /* If the current index is out of bound default it to zero */
   if (ps2->vmode > PS2_RESOLUTION_LAST || ps2->vmode < 0)
      ps2->vmode = 0;

   *width = rm_mode_table[ps2->vmode].width;
   *height = rm_mode_table[ps2->vmode].height;

   strlcpy(desc, rm_mode_table[ps2->vmode].desc, desc_len);
}

static void ps2_get_video_output_prev(void *data)
{
   ps2_video_t *ps2 = (ps2_video_t *)data;
   if (!ps2)
      return;

   if (ps2->vmode == 0)
      ps2->vmode = PS2_RESOLUTION_LAST;
   else
      ps2->vmode--;
}

static void ps2_get_video_output_next(void *data)
{
   ps2_video_t *ps2 = (ps2_video_t *)data;
   if (!ps2)
      return;

   if (ps2->vmode >= PS2_RESOLUTION_LAST)
      ps2->vmode = 0;
   else
      ps2->vmode++;
}

static void ps2_set_texture_frame(void *data, const void *frame, bool rgb32,
                                  unsigned width, unsigned height, float alpha)
{
   ps2_video_t *ps2 = (ps2_video_t *)data;

   int PSM = (rgb32 ? GS_PSM_CT32 : GS_PSM_CT16);

   set_texture(ps2->menuTexture, frame, width, height, PSM, ps2->menu_filter);
   gsKit_TexManager_invalidate(ps2->gsGlobal, ps2->menuTexture);
   gsKit_TexManager_bind(ps2->gsGlobal, ps2->menuTexture);
}

static void ps2_set_texture_enable(void *data, bool enable, bool fullscreen)
{
   ps2_video_t *ps2 = (ps2_video_t *)data;

   ps2->menuVisible = enable;
}

static void ps2_set_osd_msg(void *data,
                            const char *msg,
                            const void *params, void *font)
{
   ps2_video_t *ps2 = (ps2_video_t *)data;

   if (ps2)
      font_driver_render_msg(data, msg, params, font);
}

static bool ps2_get_hw_render_interface(void *data,
                                        const struct retro_hw_render_interface **iface)
{
   ps2_video_t *ps2 = (ps2_video_t *)data;
   ps2->iface.padding = empty_ps2_insets;
   *iface =
       (const struct retro_hw_render_interface *)&ps2->iface;
   return true;
}

static const video_poke_interface_t ps2_poke_interface = {
    NULL, /* get_flags  */
    NULL, /* load_texture */
    NULL, /* unload_texture */
    ps2_set_video_mode,
    NULL, /* get_refresh_rate */
    ps2_set_filtering,
    ps2_get_video_output_size,
    ps2_get_video_output_prev,
    ps2_get_video_output_next,
    NULL, /* get_current_framebuffer */
    NULL, /* get_proc_address */
    NULL, /* set_aspect_ratio */
    NULL, /* apply_state_changes */
    ps2_set_texture_frame,
    ps2_set_texture_enable,
    ps2_set_osd_msg,
    NULL, /* show_mouse  */
    NULL, /* grab_mouse_toggle */
    NULL, /* get_current_shader */
    NULL, /* get_current_software_framebuffer */
    ps2_get_hw_render_interface,
    NULL, /* set_hdr_max_nits */
    NULL, /* set_hdr_paper_white_nits */
    NULL, /* set_hdr_contrast */
    NULL  /* set_hdr_expand_gamut */
};

static void ps2_gfx_get_poke_interface(void *data,
                                       const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &ps2_poke_interface;
}

video_driver_t video_ps2 = {
    ps2_gfx_init,
    ps2_gfx_frame,
    ps2_gfx_set_nonblock_state,
    ps2_gfx_alive,
    ps2_gfx_focus,
    ps2_gfx_suppress_screensaver,
    ps2_gfx_has_windowed,
    ps2_gfx_set_shader,
    ps2_gfx_free,
    "ps2",
    NULL, /* set_viewport */
    NULL, /* set_rotation */
    NULL, /* viewport_info */
    NULL, /* read_viewport  */
    NULL, /* read_frame_raw */

#ifdef HAVE_OVERLAY
    NULL, /* overlay_interface */
#endif
#ifdef HAVE_VIDEO_LAYOUT
    NULL,
#endif
    ps2_gfx_get_poke_interface,
};
