#ifndef PS2_COMMON_H__
#define PS2_COMMON_H__

#include <stdint.h>
#include <stdbool.h>

#include <gsKit.h>
#include <gsInline.h>

#include "../video_defines.h"
#include "../../libretro-common/include/libretro_gskit_ps2.h"

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

#endif
