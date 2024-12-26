/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef _GFX_ANIMATION_H
#define _GFX_ANIMATION_H

#include <stdint.h>
#include <stdlib.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "font_driver.h"

#define TICKER_SPACER_DEFAULT "   |   "
#define TICKER_SPEED       333333

/* Pixel ticker nominally increases by one after each
 * TICKER_PIXEL_PERIOD ms (actual increase depends upon
 * ticker speed setting and display resolution)
 *
 * Formula is: (1.0f / 60.0f) * 1000.0f
 * */
#define TICKER_PIXEL_PERIOD (16.666666666666668f)

#define ANIM_IS_ACTIVE(_p) (((_p)->flags & (GFX_ANIM_FLAG_IS_ACTIVE)) || ((_p)->flags & GFX_ANIM_FLAG_TICKER_IS_ACTIVE))

#define GFX_ANIMATION_CLEAR_ACTIVE(anim) ((anim)->flags &= ~(GFX_ANIM_FLAG_IS_ACTIVE | GFX_ANIM_FLAG_TICKER_IS_ACTIVE))
#define GFX_ANIMATION_SET_ACTIVE(anim) ((anim)->flags |= (GFX_ANIM_FLAG_IS_ACTIVE | GFX_ANIM_FLAG_TICKER_IS_ACTIVE))

RETRO_BEGIN_DECLS

enum gfx_animation_easing_type
{
   /* Linear */
   EASING_LINEAR    = 0,
   /* Quad */
   EASING_IN_QUAD,
   EASING_OUT_QUAD,
   EASING_IN_OUT_QUAD,
   EASING_OUT_IN_QUAD,
   /* Cubic */
   EASING_IN_CUBIC,
   EASING_OUT_CUBIC,
   EASING_IN_OUT_CUBIC,
   EASING_OUT_IN_CUBIC,
   /* Quart */
   EASING_IN_QUART,
   EASING_OUT_QUART,
   EASING_IN_OUT_QUART,
   EASING_OUT_IN_QUART,
   /* Quint */
   EASING_IN_QUINT,
   EASING_OUT_QUINT,
   EASING_IN_OUT_QUINT,
   EASING_OUT_IN_QUINT,
   /* Sine */
   EASING_IN_SINE,
   EASING_OUT_SINE,
   EASING_IN_OUT_SINE,
   EASING_OUT_IN_SINE,
   /* Expo */
   EASING_IN_EXPO,
   EASING_OUT_EXPO,
   EASING_IN_OUT_EXPO,
   EASING_OUT_IN_EXPO,
   /* Circ */
   EASING_IN_CIRC,
   EASING_OUT_CIRC,
   EASING_IN_OUT_CIRC,
   EASING_OUT_IN_CIRC,
   /* Bounce */
   EASING_IN_BOUNCE,
   EASING_OUT_BOUNCE,
   EASING_IN_OUT_BOUNCE,
   EASING_OUT_IN_BOUNCE,

   EASING_LAST
};

/* TODO:
 * Add a reverse loop ticker for languages
 * that read right to left */
enum gfx_animation_ticker_type
{
   TICKER_TYPE_BOUNCE = 0,
   TICKER_TYPE_LOOP,
   TICKER_TYPE_LAST
};

enum gfx_animation_flags
{
   GFX_ANIM_FLAG_PENDING_DELETES    = (1 << 0),
   GFX_ANIM_FLAG_IN_UPDATE          = (1 << 1),
   GFX_ANIM_FLAG_IS_ACTIVE          = (1 << 2),
   GFX_ANIM_FLAG_TICKER_IS_ACTIVE   = (1 << 3)
};

typedef void  (*tween_cb)  (void*);

typedef void (*update_time_cb) (float *ticker_pixel_increment,
      unsigned width, unsigned height);

typedef struct gfx_animation_ctx_entry
{
   float *subject;
   void *userdata;
   tween_cb cb;
   uintptr_t tag;
   float duration;
   float target_value;
   enum gfx_animation_easing_type easing_enum;
} gfx_animation_ctx_entry_t;

typedef struct gfx_animation_ctx_ticker
{
   uint64_t idx;
   char *s;
   const char *str;
   const char *spacer;
   size_t len;
   enum gfx_animation_ticker_type type_enum;
   bool selected;
} gfx_animation_ctx_ticker_t;

typedef struct gfx_animation_ctx_ticker_smooth
{
   uint64_t idx;
   const char *src_str;
   const char *spacer;
   char *dst_str;
   unsigned *dst_str_width; /* May be set to NULL
                               (RGUI + XMB do not require this info) */
   unsigned *x_offset;
   font_data_t *font;
   size_t dst_str_len;
   unsigned glyph_width; /* Fallback if font == NULL */
   unsigned field_width;
   float font_scale;
   enum gfx_animation_ticker_type type_enum;
   bool selected;
} gfx_animation_ctx_ticker_smooth_t;

typedef struct gfx_animation_ctx_line_ticker
{
   uint64_t idx;
   const char *str;
   char *s;
   size_t len;
   size_t line_len;
   size_t max_lines;
   enum gfx_animation_ticker_type type_enum;
} gfx_animation_ctx_line_ticker_t;

typedef struct gfx_animation_ctx_line_ticker_smooth
{
   uint64_t idx;
   font_data_t *font;
   const char *src_str;
   float *y_offset;
   char *dst_str;
   size_t dst_str_len;
   char *top_fade_str;
   size_t top_fade_str_len;
   float *top_fade_y_offset;
   float *top_fade_alpha;
   float *bottom_fade_y_offset;
   float *bottom_fade_alpha;
   char *bottom_fade_str;
   size_t bottom_fade_str_len;
   unsigned field_width;
   unsigned field_height;
   float font_scale;
   enum gfx_animation_ticker_type type_enum;
   bool fade_enabled;
} gfx_animation_ctx_line_ticker_smooth_t;

typedef struct gfx_timer_ctx_entry
{
   tween_cb cb;
   void *userdata;
   float duration;
} gfx_timer_ctx_entry_t;

typedef struct gfx_delayed_animation
{
   gfx_animation_ctx_entry_t entry; /* pointer alignment */
   float timer;
} gfx_delayed_animation_t;

typedef float (*easing_cb) (float, float, float, float);

struct tween
{
   easing_cb   easing;
   tween_cb    cb;
   void        *userdata;
   uintptr_t   tag;
   float       duration;
   float       running_since;
   float       initial_value;
   float       target_value;
   float       *subject;
   bool        deleted;
};

struct gfx_animation
{
   uint64_t ticker_idx;            /* updated every TICKER_SPEED us */
   uint64_t ticker_slow_idx;       /* updated every TICKER_SLOW_SPEED us */
   uint64_t ticker_pixel_idx;      /* updated every frame */
   uint64_t ticker_pixel_line_idx; /* updated every frame */
   retro_time_t cur_time;
   retro_time_t old_time;
   update_time_cb updatetime_cb;   /* ptr alignment */
                                   /* By default, this should be a NOOP */
   struct tween* list;
   struct tween* pending;

   float delta_time;

   uint8_t flags;
};

typedef struct gfx_animation gfx_animation_t;

void gfx_animation_timer_start(float *timer,
      gfx_timer_ctx_entry_t *timer_entry);

bool gfx_animation_update(
      retro_time_t current_time,
      bool timedate_enable,
      float ticker_speed,
      unsigned video_width,
      unsigned video_height);

bool gfx_animation_ticker(gfx_animation_ctx_ticker_t *ticker);

bool gfx_animation_ticker_smooth(gfx_animation_ctx_ticker_smooth_t *ticker);

bool gfx_animation_kill_by_tag(uintptr_t *tag);

bool gfx_animation_push(gfx_animation_ctx_entry_t *entry);

void gfx_animation_push_delayed(unsigned delay, gfx_animation_ctx_entry_t *entry);

void gfx_animation_deinit(void);

gfx_animation_t *anim_get_ptr(void);

RETRO_END_DECLS

#endif
