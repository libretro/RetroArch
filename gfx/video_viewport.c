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

#include "../general.h"

struct aspect_ratio_elem aspectratio_lut[ASPECT_RATIO_END] = {
   { "4:3",           1.3333f },
   { "16:9",          1.7778f },
   { "16:10",         1.6f },
   { "16:15",         16.0f / 15.0f },
   { "1:1",           1.0f },
   { "2:1",           2.0f },
   { "3:2",           1.5f },
   { "3:4",           0.75f },
   { "4:1",           4.0f },
   { "4:4",           1.0f },
   { "5:4",           1.25f },
   { "6:5",           1.2f },
   { "7:9",           0.7777f },
   { "8:3",           2.6666f },
   { "8:7",           1.1428f },
   { "19:12",         1.5833f },
   { "19:14",         1.3571f },
   { "30:17",         1.7647f },
   { "32:9",          3.5555f },
   { "Config",        0.0f },
   { "Square pixel",  1.0f },
   { "Core provided", 1.0f },
   { "Custom",        0.0f }
};

char rotation_lut[4][32] =
{
   "Normal",
   "90 deg",
   "180 deg",
   "270 deg"
};

static struct retro_system_av_info video_viewport_av_info;

/**
 * video_viewport_set_square_pixel:
 * @width         : Width.
 * @height        : Height.
 *
 * Sets viewport to square pixel aspect ratio based on @width and @height. 
 **/
void video_viewport_set_square_pixel(unsigned width, unsigned height)
{
   unsigned len, highest, i, aspect_x, aspect_y;
   if (width == 0 || height == 0)
      return;

   len      = min(width, height);
   highest  = 1;

   for (i = 1; i < len; i++)
   {
      if ((width % i) == 0 && (height % i) == 0)
         highest = i;
   }

   aspect_x = width / highest;
   aspect_y = height / highest;

   snprintf(aspectratio_lut[ASPECT_RATIO_SQUARE].name,
         sizeof(aspectratio_lut[ASPECT_RATIO_SQUARE].name),
         "%u:%u (1:1 PAR)", aspect_x, aspect_y);

   aspectratio_lut[ASPECT_RATIO_SQUARE].value = (float)aspect_x / aspect_y;
}

/**
 * video_viewport_set_core:
 *
 * Sets viewport to aspect ratio set by core. 
 **/
void video_viewport_set_core(void)
{
   struct retro_system_av_info *av_info = 
      video_viewport_get_system_av_info();
   struct retro_game_geometry *geom = &av_info->geometry;

   if (!geom || geom->base_width <= 0.0f || geom->base_height <= 0.0f)
      return;

   /* Fallback to 1:1 pixel ratio if none provided */
   if (geom->aspect_ratio > 0.0f)
      aspectratio_lut[ASPECT_RATIO_CORE].value = geom->aspect_ratio;
   else
      aspectratio_lut[ASPECT_RATIO_CORE].value = 
         (float)geom->base_width / geom->base_height;
}

/**
 * video_viewport_set_config:
 *
 * Sets viewport to config aspect ratio.
 **/
void video_viewport_set_config(void)
{
   settings_t *settings = config_get_ptr();
   struct retro_system_av_info *av_info = video_viewport_get_system_av_info();

   if (settings->video.aspect_ratio < 0.0f)
   {
      struct retro_game_geometry *geom = &av_info->geometry;

      if (!geom)
         return;

      if (geom->aspect_ratio > 0.0f && settings->video.aspect_ratio_auto)
         aspectratio_lut[ASPECT_RATIO_CONFIG].value = geom->aspect_ratio;
      else
      {
         unsigned base_width  = geom->base_width;
         unsigned base_height = geom->base_height;

         /* Get around division by zero errors */
         if (base_width == 0)
            base_width = 1;
         if (base_height == 0)
            base_height = 1;
         aspectratio_lut[ASPECT_RATIO_CONFIG].value = 
            (float)base_width / base_height; /* 1:1 PAR. */
      }
   }
   else
      aspectratio_lut[ASPECT_RATIO_CONFIG].value = 
         settings->video.aspect_ratio;
}

/**
 * video_viewport_get_scaled_integer:
 * @vp            : Viewport handle
 * @width         : Width.
 * @height        : Height.
 * @aspect_ratio  : Aspect ratio (in float).
 * @keep_aspect   : Preserve aspect ratio?
 *
 * Gets viewport scaling dimensions based on 
 * scaled integer aspect ratio.
 **/
void video_viewport_get_scaled_integer(struct video_viewport *vp,
      unsigned width, unsigned height,
      float aspect_ratio, bool keep_aspect)
{
   int padding_x = 0, padding_y = 0;
   settings_t *settings = config_get_ptr();

   if (!vp)
      return;

   if (settings->video.aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
   {
      struct video_viewport *custom = video_viewport_get_custom();

      if (custom)
      {
         padding_x = width - custom->width;
         padding_y = height - custom->height;
         width = custom->width;
         height = custom->height;
      }
   }
   else
   {
      unsigned base_width;
      /* Use system reported sizes as these define the 
       * geometry for the "normal" case. */
      struct retro_system_av_info *av_info = video_viewport_get_system_av_info();
      unsigned base_height = av_info ? av_info->geometry.base_height : 0;

      if (base_height == 0)
         base_height = 1;

      /* Account for non-square pixels.
       * This is sort of contradictory with the goal of integer scale,
       * but it is desirable in some cases.
       *
       * If square pixels are used, base_height will be equal to 
       * system->av_info.base_height. */
      base_width = (unsigned)roundf(base_height * aspect_ratio);

      /* Make sure that we don't get 0x scale ... */
      if (width >= base_width && height >= base_height)
      {
         if (keep_aspect)
         {
            /* X/Y scale must be same. */
            unsigned max_scale = min(width / base_width, height / base_height);
            padding_x = width - base_width * max_scale;
            padding_y = height - base_height * max_scale;
         }
         else
         {
            /* X/Y can be independent, each scaled as much as possible. */
            padding_x = width % base_width;
            padding_y = height % base_height;
         }
      }

      width     -= padding_x;
      height    -= padding_y;
   }

   vp->width  = width;
   vp->height = height;
   vp->x      = padding_x / 2;
   vp->y      = padding_y / 2;
}

struct retro_system_av_info *video_viewport_get_system_av_info(void)
{
   return (struct retro_system_av_info*)&video_viewport_av_info;
}

struct video_viewport *video_viewport_get_custom(void)
{
   settings_t *settings = config_get_ptr();
   return &settings->video_viewport_custom;
}
