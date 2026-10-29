/*  RetroArch - A frontend for libretro.
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

#ifndef _ANDROID_POINTERS_H
#define _ANDROID_POINTERS_H

#include <stdint.h>
#include <boolean.h>
#include <retro_inline.h>

/* Android's touches and mouse as the input driver keeps them
 * (input/drivers/android_input.c), and what it publishes of them to the
 * frontend each poll. No NDK types, so that
 * samples/input/pointer_frame can drive it; it needs
 * input/input_driver.h, with HAVE_VIDEO_WINDOWS, and
 * gfx/video_driver.h before it. */

/* The main window's 16 slots and the second display's 16 fingers */
#define ANDROID_POINTERS_MAX 32

/* A touch slot: index n is the n-th finger down. A lifted finger's
 * slot keeps its place and the slots above it shift down, so an index
 * that is not down still reads a place. */
struct input_pointer
{
   uint32_t pos;    /* in its window, VIDEO_POS_PACK */
   bool placed;     /* a place was ever written here */
};

typedef struct android_pointers
{
   const struct input_pointer *main_slots;
   const struct input_pointer *second;
   unsigned slots;                      /* main_slots */
   unsigned main_count;                 /* fingers down on the main window */
   unsigned second_count;               /* on the second display, all down */
   input_video_window_t second_window;
   uint32_t mouse_pos;                  /* in the window, VIDEO_POS_PACK */
   uint16_t mouse_buttons;              /* enum input_pointer_buttons */
   bool mouse_used;                     /* a mouse has moved: it aims the lightgun */
} android_pointers_t;

/* The publication's flags that keep Android's answers */
static INLINE unsigned android_pointers_flags(bool mouse_used)
{
   unsigned flags = INPUT_POINTERS_PRESSED_INSIDE
                  | INPUT_POINTERS_WINDOWS_SECOND
                  | INPUT_POINTERS_MOUSE_BY_DRIVER
                  | INPUT_POINTERS_TOUCH_SLOTS
                  | INPUT_POINTERS_POINTER_IDS_BY_DRIVER;
   if (!mouse_used)
      flags |= INPUT_POINTERS_GUN_AT_TOUCH;
   return flags;
}

/* The fingers the driver's own lightgun buttons count (TRIGGER one,
 * START two, RELOAD three): none on a main window whose views layout
 * @layout shows no screen, where a tap is on nothing. A tap in the
 * letterbox still counts. */
static INLINE unsigned android_pointers_gun_fingers(unsigned main_count,
      const video_views_layout_t *layout)
{
   if (layout && !layout->num_placements)
      return 0;
   return main_count;
}

/* One poll's mouse and touches to the frontend; app thread */
static INLINE void android_pointers_publish(const android_pointers_t *p)
{
   input_pointer_frame_t frame;
   input_video_window_t windows[ANDROID_POINTERS_MAX];
   uint32_t pos[ANDROID_POINTERS_MAX];
   unsigned present = 0;
   unsigned down    = 0;
   unsigned count   = 0;
   unsigned i;

   frame.pos     = p->mouse_pos;
   frame.rel     = 0;   /* the driver's own (MOUSE_BY_DRIVER) */
   frame.buttons = p->mouse_buttons;
   input_driver_publish_window_pointers(&frame, NULL, 1,
         android_pointers_flags(p->mouse_used));

   for (i = 0; i < p->slots && count < ANDROID_POINTERS_MAX; i++, count++)
   {
      pos[count]                = p->main_slots[i].pos;
      windows[count].window     = 0;
      windows[count].generation = 0;
      if (p->main_slots[i].placed)
         present |= 1u << count;
      if (i < p->main_count)
         down    |= 1u << count;
   }
   for (i = 0; i < p->second_count && count < ANDROID_POINTERS_MAX;
         i++, count++)
   {
      pos[count]     = p->second[i].pos;
      windows[count] = p->second_window;
      present       |= 1u << count;
      down          |= 1u << count;
   }
   input_driver_publish_window_touches(pos, windows, count, present, down);
}

#endif
