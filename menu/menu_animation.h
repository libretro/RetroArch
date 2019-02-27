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

#ifndef _MENU_ANIMATION_H
#define _MENU_ANIMATION_H

#include <stdint.h>
#include <stdlib.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define TICKER_SPACER_DEFAULT "   |   "

typedef float (*easing_cb) (float, float, float, float);
typedef void  (*tween_cb)  (void*);

enum menu_animation_ctl_state
{
   MENU_ANIMATION_CTL_NONE = 0,
   MENU_ANIMATION_CTL_DEINIT,
   MENU_ANIMATION_CTL_CLEAR_ACTIVE,
   MENU_ANIMATION_CTL_SET_ACTIVE
};

enum menu_animation_easing_type
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
enum menu_animation_ticker_type
{
   TICKER_TYPE_BOUNCE = 0,
   TICKER_TYPE_LOOP,
   TICKER_TYPE_LAST
};

typedef uintptr_t menu_animation_ctx_tag;

typedef struct menu_animation_ctx_subject
{
   size_t count;
   const void *data;
} menu_animation_ctx_subject_t;

typedef struct menu_animation_ctx_entry
{
   enum menu_animation_easing_type easing_enum;
   uintptr_t tag;
   float duration;
   float target_value;
   float *subject;
   tween_cb cb;
   void *userdata;
} menu_animation_ctx_entry_t;

typedef struct menu_animation_ctx_ticker
{
   bool selected;
   size_t len;
   uint64_t idx;
   enum menu_animation_ticker_type type_enum;
   char *s;
   const char *str;
   const char *spacer;
} menu_animation_ctx_ticker_t;

typedef float menu_timer_t;

typedef struct menu_timer_ctx_entry
{
   float duration;
   tween_cb cb;
   void *userdata;
} menu_timer_ctx_entry_t;

typedef struct menu_delayed_animation
{
   menu_timer_t timer;
   menu_animation_ctx_entry_t entry;
} menu_delayed_animation_t;

void menu_timer_start(menu_timer_t *timer, menu_timer_ctx_entry_t *timer_entry);

void menu_timer_kill(menu_timer_t *timer);

void menu_animation_init(void);

void menu_animation_free(void);

bool menu_animation_update(void);

bool menu_animation_ticker(menu_animation_ctx_ticker_t *ticker);

float menu_animation_get_delta_time(void);

bool menu_animation_is_active(void);

bool menu_animation_kill_by_tag(menu_animation_ctx_tag *tag);

void menu_animation_kill_by_subject(menu_animation_ctx_subject_t *subject);

bool menu_animation_push(menu_animation_ctx_entry_t *entry);

void menu_animation_push_delayed(unsigned delay, menu_animation_ctx_entry_t *entry);

bool menu_animation_ctl(enum menu_animation_ctl_state state, void *data);

uint64_t menu_animation_get_ticker_idx(void);

uint64_t menu_animation_get_ticker_slow_idx(void);

RETRO_END_DECLS

#endif
