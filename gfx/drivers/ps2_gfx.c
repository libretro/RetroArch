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
#define GS_TEXT GS_SETREG_RGBA(0x80,0x80,0x80,0x80)
/* turn black GS Screen */
#define GS_BLACK GS_SETREG_RGBA(0x00,0x00,0x00,0x80)
/* default alpha logic */
#define GS_DEFAULT_ALPHA GS_SETREG_ALPHA(0, 1, 0, 1, 0)

#define NUM_RM_VMODES 8
#define PS2_RESOLUTION_LAST NUM_RM_VMODES - 1
#define RM_VMODE_AUTO 0

enum rm_aratio {
    RM_ARATIO_4_3 = 0,
    RM_ARATIO_16_9,
};

// RM Vmode -> GS Vmode conversion table
struct rm_mode
{
    char mode;
    char hsync; // In KHz
    short int width;
    short int height;
    short int passes;
    short int VCK;
    short int interlace;
    short int field;
    short int aratio;
    short int PAR1; // Pixel Aspect Ratio 1 (For video modes with non-square pixels, like PAL/NTSC)
    short int PAR2; // Pixel Aspect Ratio 2 (For video modes with non-square pixels, like PAL/NTSC)
    char desc[64];
};

static struct rm_mode rm_mode_table[NUM_RM_VMODES] = {
    // 24 bit color mode with black borders
    {-1,                 16,  640,   -1,  1, 4, GS_INTERLACED,    GS_FIELD, RM_ARATIO_4_3, -1, 15, "AUTO"}, // AUTO
    {GS_MODE_PAL,        16,  640,  512,  1, 4, GS_INTERLACED,    GS_FIELD, RM_ARATIO_4_3, 16, 15, "PAL@50Hz"}, // PAL@50Hz
    {GS_MODE_NTSC,       16,  640,  448,  1, 4, GS_INTERLACED,    GS_FIELD, RM_ARATIO_4_3, 14, 15, "NTSC@60Hz"}, // NTSC@60Hz
    {GS_MODE_PAL,        16,  640,  256,  1, 2, GS_NONINTERLACED, GS_FIELD, RM_ARATIO_4_3, 16, 15, "PAL@50Hz"}, // PAL@50Hz
    {GS_MODE_NTSC,       16,  640,  224,  1, 2, GS_NONINTERLACED, GS_FIELD, RM_ARATIO_4_3, 14, 15, "NTSC@60Hz"}, // NTSC@60Hz
    {GS_MODE_DTV_480P,   31,  640,  448,  1, 2, GS_NONINTERLACED, GS_FRAME, RM_ARATIO_4_3, 14, 15, "DTV480P@60Hz"}, // DTV480P@60Hz
    {GS_MODE_DTV_576P,   31,  640,  512,  1, 2, GS_NONINTERLACED, GS_FRAME, RM_ARATIO_4_3, 16, 15, "DTV576P@50Hz"}, // DTV576P@50Hz
    {GS_MODE_VGA_640_60, 31,  640,  480,  1, 2, GS_NONINTERLACED, GS_FRAME, RM_ARATIO_4_3,  1,  1, "VGA640x480@60Hz"}, // VGA640x480@60Hz
    // 24 bit color mode full screen, multi-pass (2 passes, HIRES)
   //  {GS_MODE_PAL,        16,  704,  576,  2, 4, GS_INTERLACED,    GS_FIELD, RM_ARATIO_4_3, 12, 11, "PAL@50Hz"}, // PAL@50Hz
   //  {GS_MODE_NTSC,       16,  704,  480,  2, 4, GS_INTERLACED,    GS_FIELD, RM_ARATIO_4_3, 10, 11, "NTSC@60Hz"}, // NTSC@60Hz
   //  {GS_MODE_DTV_480P,   31,  704,  480,  2, 2, GS_NONINTERLACED, GS_FRAME, RM_ARATIO_4_3, 10, 11, "DTV480P@60Hz"}, // DTV480P@60Hz
   //  {GS_MODE_DTV_576P,   31,  704,  576,  2, 2, GS_NONINTERLACED, GS_FRAME, RM_ARATIO_4_3, 12, 11, "DTV576P@50Hz"}, // DTV576P@50Hz
    // 16 bit color mode full screen, multi-pass (3 passes, HIRES)
   //  {GS_MODE_DTV_720P,   31, 1280,  720,  3, 1, GS_NONINTERLACED, GS_FRAME, RM_ARATIO_16_9, 1,  1, "HDTV720P@60Hz"}, // HDTV720P@60Hz
   //  {GS_MODE_DTV_1080I,  31, 1920, 1080,  3, 1, GS_INTERLACED,    GS_FRAME, RM_ARATIO_16_9, 1,  1, "HDTV1080I@60Hz"}, // HDTV1080I@60Hz
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
   bool hires;
   int video_window_offset_x;
   int video_window_offset_y;

   int PSM;
   int menu_filter;
   int core_filter;

   video_viewport_t vp;

   /* Palette in the cores */
   struct retro_hw_render_interface_gskit_ps2 iface;

   GSGLOBAL *gsGlobal;
   GSTEXTURE *menuTexture;
   GSTEXTURE *coreTexture;
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

   if (ps2->hires)
   {
      gsKit_hires_deinit_global(ps2->gsGlobal);
   }
   else
   {
      gsKit_deinit_global(ps2->gsGlobal);
   }
   ps2->vmode = -1;
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
   RARCH_LOG("RENDERMAN Change offset: %d, %d\n", ps2->video_window_offset_x, ps2->video_window_offset_y);
}

static int rmSetMode(ps2_video_t *ps2, int force)
{
   global_t *global = global_get_ptr();

   // we don't want to set the vmode without a reason...
    int changed = (ps2->vmode != global->console.screen.resolutions.current.id || force);
    if (changed) {
        // Cleanup previous gsKit instance
        if (ps2->vmode >= 0)
            rmEnd(ps2);

      ps2->vmode = global->console.screen.resolutions.current.id;
      ps2->hires = (rm_mode_table[ps2->vmode].passes > 1) ? 1 : 0;

      if (ps2->hires) {
         ps2->gsGlobal = gsKit_hires_init_global();
      } else {
         ps2->gsGlobal = gsKit_init_global();
      }
      ps2->gsGlobal->Mode = rm_mode_table[ps2->vmode].mode;
      ps2->gsGlobal->Width = rm_mode_table[ps2->vmode].width;
      ps2->gsGlobal->Height = rm_mode_table[ps2->vmode].height;
      ps2->gsGlobal->Interlace = rm_mode_table[ps2->vmode].interlace;
      ps2->gsGlobal->Field = rm_mode_table[ps2->vmode].field;
      ps2->gsGlobal->PSM = GS_PSM_CT16;
      ps2->gsGlobal->PSMZ = GS_PSMZ_16S;
      ps2->gsGlobal->DoubleBuffering = GS_SETTING_OFF;
      ps2->gsGlobal->ZBuffering = GS_SETTING_OFF;
      ps2->gsGlobal->PrimAlphaEnable = GS_SETTING_OFF;

      if ((ps2->gsGlobal->Interlace == GS_INTERLACED) && (ps2->gsGlobal->Field == GS_FRAME))
         ps2->gsGlobal->Height /= 2;

      // Coordinate space ranges from 0 to 4096 pixels
      // Center the buffer in the coordinate space
      ps2->gsGlobal->OffsetX = ((4096 - ps2->gsGlobal->Width) / 2) * 16;
      ps2->gsGlobal->OffsetY = ((4096 - ps2->gsGlobal->Height) / 2) * 16;

      if (ps2->hires) {
         gsKit_hires_init_screen(ps2->gsGlobal, rm_mode_table[ps2->vmode].passes);
      } else {
         gsKit_init_screen(ps2->gsGlobal);
         gsKit_mode_switch(ps2->gsGlobal, GS_ONESHOT);
      }

      gsKit_set_test(ps2->gsGlobal, GS_ZTEST_OFF);
      gsKit_set_primalpha(ps2->gsGlobal, GS_DEFAULT_ALPHA, 0);

      // reset the contents of the screen to avoid garbage being displayed
      if (ps2->hires) {
         gsKit_hires_sync(ps2->gsGlobal);
         gsKit_hires_flip(ps2->gsGlobal);
      } else {
         gsKit_clear(ps2->gsGlobal, GS_BLACK);
         gsKit_sync_flip(ps2->gsGlobal);
      }

      RARCH_LOG("RENDERMAN New vmode: %d, %d x %d\n", ps2->vmode, ps2->gsGlobal->Width, ps2->gsGlobal->Height);
   }

   updateOffSetsIfNeeded(ps2);

    return changed;
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
   rm_mode_table[RM_VMODE_AUTO].height = (mode == GS_MODE_PAL) ? 512 : 448;
   rm_mode_table[RM_VMODE_AUTO].PAR1 = (mode == GS_MODE_PAL) ? 16 : 14;

   dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
               D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);

   // Initialize the DMAC
   dmaKit_chan_init(DMA_CHANNEL_GIF);

   rmSetMode(ps2, 1);
}

/* Copy of gsKit_sync_flip, but without the 'flip' */
static void gsKit_sync(GSGLOBAL *gsGlobal)
{
   if (!gsGlobal->FirstFrame)
      WaitSema(vsync_sema_id);

   while (PollSema(vsync_sema_id) >= 0);
}

/* Copy of gsKit_sync_flip, but without the 'sync' */
static void gsKit_flip(GSGLOBAL *gsGlobal)
{
   if (!gsGlobal->FirstFrame)
   {
      if (gsGlobal->DoubleBuffering == GS_SETTING_ON)
      {
         GS_SET_DISPFB2( gsGlobal->ScreenBuffer[
               gsGlobal->ActiveBuffer & 1] / 8192,
               gsGlobal->Width / 64, gsGlobal->PSM, 0, 0 );

         gsGlobal->ActiveBuffer ^= 1;
      }

   }

   gsKit_setactive(gsGlobal);
}

static GSTEXTURE *prepare_new_texture(void)
{
   GSTEXTURE *texture = (GSTEXTURE*)calloc(1, sizeof(GSTEXTURE));
   return texture;
}

static void init_ps2_video(ps2_video_t *ps2)
{
   rmInit(ps2);
   gsKit_TexManager_init(ps2->gsGlobal);

   ps2->vp.x                = 0;
   ps2->vp.y                = 0;
   ps2->vp.width            = ps2->gsGlobal->Width;
   ps2->vp.height           = ps2->gsGlobal->Height;
   ps2->vp.full_width       = ps2->gsGlobal->Width;
   ps2->vp.full_height      = ps2->gsGlobal->Height;

   ps2->vsync_callback_id = gsKit_add_vsync_handler(vsync_handler);
   ps2->menuTexture = prepare_new_texture();
   ps2->coreTexture = prepare_new_texture();

   ps2->vmode = 0;
   ps2->hires = false;
   ps2->video_window_offset_x = 0;
   ps2->video_window_offset_y = 0;

   /* Used for cores that supports palette */
   ps2->iface.interface_type    = RETRO_HW_RENDER_INTERFACE_GSKIT_PS2;
   ps2->iface.interface_version = RETRO_HW_RENDER_INTERFACE_GSKIT_PS2_VERSION;
   ps2->iface.coreTexture       = ps2->coreTexture;
}

static void ps2_gfx_deinit_texture(GSTEXTURE *texture)
{
   texture->Mem  = NULL;
   texture->Clut = NULL;
}

static void set_texture(GSTEXTURE *texture, const void *frame,
      int width, int height, int PSM, int filter)
{
   texture->Width  = width;
   texture->Height = height;
   texture->PSM    = PSM;
   texture->Filter = filter;
   texture->Mem    = (void *)frame;
}

static void prim_texture(GSGLOBAL *gsGlobal, GSTEXTURE *texture, int zPosition, float aspect_ratio, bool scale_integer, struct retro_hw_ps2_insets padding)
{
   float x1, y1, x2, y2;
   float visible_width  =  texture->Width - padding.left - padding.right;
   float visible_height =  texture->Height - padding.top - padding.bottom;

   if (scale_integer)
   {
      float width_proportion  = (float)gsGlobal->Width / (float)visible_width;
      float height_proportion = (float)gsGlobal->Height / (float)visible_height;
      int delta               = MIN(width_proportion, height_proportion);
      float newWidth          = visible_width * delta;
      float newHeight         = visible_height * delta;

      x1 = (gsGlobal->Width - newWidth) / 2.0f;
      y1 = (gsGlobal->Height - newHeight) / 2.0f;
      x2 = newWidth + x1;
      y2 = newHeight + y1;
   }
   else if (aspect_ratio > 0)
   {
      float gs_aspect_ratio = (float)gsGlobal->Width / (float)gsGlobal->Height;
      float newWidth = (gs_aspect_ratio > aspect_ratio) ? gsGlobal->Height * aspect_ratio : gsGlobal->Width;
      float newHeight = (gs_aspect_ratio > aspect_ratio) ? gsGlobal->Height : gsGlobal->Width / aspect_ratio;

      x1 = (gsGlobal->Width - newWidth) / 2.0f;
      y1 = (gsGlobal->Height - newHeight) / 2.0f;
      x2 = newWidth + x1;
      y2 = newHeight + y1;
   }
   else
   {
      x1 = 0.0f;
      y1 = 0.0f;
      x2 = gsGlobal->Width;
      y2 = gsGlobal->Height;
   }

   gsKit_prim_sprite_texture( gsGlobal, texture,
         x1,            /* X1 */
         y1,            /* Y1 */
         padding.left,  /* U1 */
         padding.top,   /* V1 */
         x2,            /* X2 */
         y2,            /* Y2 */
         texture->Width - padding.right,   /* U2 */
         texture->Height - padding.bottom, /* V2 */
         zPosition,
         GS_TEXT);
}

static void refreshScreen(ps2_video_t *ps2)
{
   if (ps2->vsync)
   {
      gsKit_sync(ps2->gsGlobal);
      gsKit_flip(ps2->gsGlobal);
   }
   gsKit_queue_exec(ps2->gsGlobal);
   gsKit_TexManager_nextFrame(ps2->gsGlobal);
}

static void *ps2_gfx_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   void *ps2input   = NULL;
   ps2_video_t *ps2 = (ps2_video_t*)calloc(1, sizeof(ps2_video_t));

   *input_data      = NULL;

   if (!ps2)
      return NULL;

   init_ps2_video(ps2);
   if (video->font_enable)
      font_driver_init_osd(ps2,
            video,
            false,
            video->is_threaded,
            FONT_DRIVER_RENDER_PS2);

   ps2->PSM          = (video->rgb32 ? GS_PSM_CT32 : GS_PSM_CT16);
   ps2->core_filter  = video->smooth ? GS_FILTER_LINEAR : GS_FILTER_NEAREST;
   ps2->force_aspect = video->force_aspect;
   ps2->vsync        = video->vsync;

   if (input && input_data)
   {
      settings_t *settings = config_get_ptr();
      ps2input             = input_driver_init_wrap(&input_ps2,
            settings->arrays.input_joypad_driver);
      *input               = ps2input ? &input_ps2 : NULL;
      *input_data          = ps2input;
   }

   return ps2;
}

static bool ps2_gfx_frame(void *data, const void *frame,
      unsigned width, unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   ps2_video_t               *ps2 = (ps2_video_t*)data;
   struct font_params *osd_params = (struct font_params*)
      &video_info->osd_stat_params;
   bool statistics_show           = video_info->statistics_show;
   settings_t *settings      = config_get_ptr();

   if (!width || !height)
      return false;

#if defined(DEBUG)
   if (frame_count % 180 == 0)
      printf("ps2_gfx_frame %llu\n", frame_count);
#endif

   // Check if user change offset values
   updateOffSetsIfNeeded(ps2);

   if (frame)
   {
      struct retro_hw_ps2_insets padding = empty_ps2_insets;
      /* Checking if the transfer is done in the core */
      if (frame != RETRO_HW_FRAME_BUFFER_VALID)
      {
         /* calculate proper width based in the pitch */
         int shifh_per_bytes = (ps2->PSM == GS_PSM_CT32) ? 2 : 1;
         int real_width      = pitch >> shifh_per_bytes;
         set_texture(ps2->coreTexture, frame, real_width, height, ps2->PSM, ps2->core_filter);

         padding.right       = real_width - width;
      }
      else
      {
         padding = ps2->iface.padding;
      }

      float aspect_ratio = ps2->force_aspect ? video_driver_get_aspect_ratio() : 0;
      bool scale_integer = settings->bools.video_scale_integer;

      /* Disable Alpha for cores */
      ps2->gsGlobal->PrimAlphaEnable = GS_SETTING_OFF;
      gsKit_set_test(ps2->gsGlobal, GS_ATEST_OFF);

      gsKit_TexManager_invalidate(ps2->gsGlobal, ps2->coreTexture);
      gsKit_TexManager_bind(ps2->gsGlobal, ps2->coreTexture);
      prim_texture(ps2->gsGlobal, ps2->coreTexture, 1, aspect_ratio, scale_integer, padding);
   }

   if (ps2->menuVisible)
   {
      bool texture_empty = !ps2->menuTexture->Width || !ps2->menuTexture->Height;
      if (!texture_empty)
      {
         prim_texture(ps2->gsGlobal, ps2->menuTexture, 2, 0, 0, empty_ps2_insets);
      }
   }
   else if (statistics_show)
   {
      if (osd_params)
         font_driver_render_msg(ps2, video_info->stat_text, osd_params, NULL);
   }

   if (!string_is_empty(msg))
      font_driver_render_msg(ps2, msg, NULL, NULL);

   refreshScreen(ps2);

   return true;
}

static void ps2_gfx_set_nonblock_state(void *data, bool toggle,
      bool adaptive_vsync_enabled, unsigned swap_interval)
{
   ps2_video_t *ps2 = (ps2_video_t*)data;

   if (ps2)
      ps2->vsync = !toggle;
}

static bool ps2_gfx_alive(void *data) { return true; }
static bool ps2_gfx_focus(void *data) { return true; }
static bool ps2_gfx_suppress_screensaver(void *data, bool enable) { return false; }
static bool ps2_gfx_has_windowed(void *data) { return false; }

static void ps2_gfx_free(void *data)
{
   ps2_video_t *ps2 = (ps2_video_t*)data;

   gsKit_clear(ps2->gsGlobal, GS_BLACK);
   gsKit_vram_clear(ps2->gsGlobal);

   font_driver_free_osd();

   ps2_gfx_deinit_texture(ps2->menuTexture);
   ps2_gfx_deinit_texture(ps2->coreTexture);

   free(ps2->menuTexture);
   free(ps2->coreTexture);

   gsKit_remove_vsync_handler(ps2->vsync_callback_id);
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
   ps2_video_t *ps2 = (ps2_video_t*)data;
   rmSetMode(ps2, 0);
}

static void ps2_set_filtering(void *data, unsigned index, bool smooth, bool ctx_scaling)
{
   ps2_video_t *ps2 = (ps2_video_t*)data;

   ps2->menu_filter = smooth ? GS_FILTER_LINEAR : GS_FILTER_NEAREST;
}

static void ps2_get_video_output_size(void *data,
      unsigned *width, unsigned *height, char *desc, size_t desc_len)
{
   global_t *global = global_get_ptr();
   if (!global)
      return;

   /* If the current index is out of bound default it to zero */
   if (global->console.screen.resolutions.current.id > PS2_RESOLUTION_LAST)
      global->console.screen.resolutions.current.id = 0;

   *width  = rm_mode_table[
      global->console.screen.resolutions.current.id].width;
   *height = rm_mode_table[
      global->console.screen.resolutions.current.id].height;

   strlcpy(desc, rm_mode_table[global->console.screen.resolutions.current.id].desc, desc_len);
}

static void ps2_get_video_output_prev(void *data)
{
   global_t *global = global_get_ptr();

   if (global->console.screen.resolutions.current.id == 0)
   {
      global->console.screen.resolutions.current.id = PS2_RESOLUTION_LAST;
      return;
   }

   global->console.screen.resolutions.current.id--;
}

static void ps2_get_video_output_next(void *data)
{
   global_t *global = global_get_ptr();

   if (global->console.screen.resolutions.current.id >= PS2_RESOLUTION_LAST)
   {
      global->console.screen.resolutions.current.id = 0;
      return;
   }

   global->console.screen.resolutions.current.id++;
}

static void ps2_set_texture_frame(void *data, const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   ps2_video_t *ps2      = (ps2_video_t*)data;

   int PSM               = (rgb32 ? GS_PSM_CT32 : GS_PSM_CT16);

   set_texture(ps2->menuTexture, frame, width, height, PSM, ps2->menu_filter);
   gsKit_TexManager_invalidate(ps2->gsGlobal, ps2->menuTexture);
   gsKit_TexManager_bind(ps2->gsGlobal, ps2->menuTexture);
}

static void ps2_set_texture_enable(void *data, bool enable, bool fullscreen)
{
   ps2_video_t *ps2 = (ps2_video_t*)data;

   if (ps2->menuVisible != enable)
   {
      /* If Menu change status, CLEAR SCREEN */
      gsKit_clear(ps2->gsGlobal, GS_BLACK);
   }
   ps2->menuVisible = enable;
}

static void ps2_set_osd_msg(void *data,
      const char *msg,
      const void *params, void *font)
{
   ps2_video_t *ps2 = (ps2_video_t*)data;

   if (ps2)
      font_driver_render_msg(data, msg, params, font);
}

static bool ps2_get_hw_render_interface(void* data,
      const struct retro_hw_render_interface** iface)
{
   ps2_video_t          *ps2 = (ps2_video_t*)data;
   ps2->iface.padding        = empty_ps2_insets;
   *iface                    =
      (const struct retro_hw_render_interface*)&ps2->iface;
   return true;
}

static const video_poke_interface_t ps2_poke_interface = {
   NULL,          /* get_flags  */
   NULL,
   NULL,
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
   ps2_set_osd_msg,             /* set_osd_msg */
   NULL,                        /* show_mouse  */
   NULL,                        /* grab_mouse_toggle */
   NULL,                        /* get_current_shader */
   NULL,                        /* get_current_software_framebuffer */
   ps2_get_hw_render_interface, /* get_hw_render_interface */
   NULL,                        /* set_hdr_max_nits */
   NULL,                        /* set_hdr_paper_white_nits */
   NULL,                        /* set_hdr_contrast */
   NULL                         /* set_hdr_expand_gamut */
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
