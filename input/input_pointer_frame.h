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

#ifndef __INPUT_POINTER_FRAME_H
#define __INPUT_POINTER_FRAME_H

/* What the frontend keeps of the mice and touches an input driver
 * publishes at its poll, and how it answers for the mouse, the pointer
 * and the lightgun's aim from them. input_driver.c is its one user; it
 * is a header so that a test can drive the same code
 * (samples/input/pointer_frame). It needs input_driver.h,
 * gfx/video_driver.h's viewport calls and <string.h> before it. */

#define INPUT_TOUCHES_MAX 16

/* The pointing devices an input driver published at this poll, and
 * their places in the viewport, worked out the first time one is asked
 * for after the poll and kept until the next. */
static struct
{
   input_pointer_frame_t frame[MAX_USERS];
   uint32_t confined_pos[MAX_USERS];  /* in the viewport, held to its edges */
   uint32_t viewport_pos[MAX_USERS];  /* in the viewport, -0x8000 outside it */
   uint32_t screen_pos[MAX_USERS];
   uint32_t read_as[MAX_USERS];       /* the device kind last read as */
   uint32_t touch_pos[INPUT_TOUCHES_MAX];      /* in the window; 0: none */
   uint32_t touch_confined_pos[INPUT_TOUCHES_MAX];
   uint32_t touch_screen_pos[INPUT_TOUCHES_MAX];
   struct video_viewport vp;
   uint16_t have_confined;            /* a bit a device: done this poll */
   uint16_t have_viewport;
   uint16_t have_touch;               /* a bit a touch: done this poll */
   uint16_t touch_down;
   uint8_t  count;
   uint8_t  touch_count;
   uint8_t  flags;                    /* enum input_pointers_flags */
   bool     vp_asked;
   bool     vp_ok;
} input_pointers;

static void input_pointer_frames_set(const input_pointer_frame_t *frames,
      unsigned count, unsigned flags)
{
   if (count > MAX_USERS)
      count = MAX_USERS;
   memcpy(input_pointers.frame, frames, count * sizeof(*frames));
   input_pointers.count         = (uint8_t)count;
   input_pointers.flags         = (uint8_t)flags;
   input_pointers.have_confined = 0;
   input_pointers.have_viewport = 0;
   input_pointers.vp_asked      = false;
}

static void input_pointer_touches_set(const uint32_t *pos, unsigned count,
      unsigned down)
{
   if (count > INPUT_TOUCHES_MAX)
      count = INPUT_TOUCHES_MAX;
   memcpy(input_pointers.touch_pos, pos, count * sizeof(*pos));
   input_pointers.touch_count = (uint8_t)count;
   input_pointers.touch_down  = (uint16_t)down;
   input_pointers.have_touch  = 0;
}

static bool input_pointer_viewport(void)
{
   if (!input_pointers.vp_asked)
   {
      memset(&input_pointers.vp, 0, sizeof(input_pointers.vp));
      input_pointers.vp_ok    = video_driver_get_viewport_info(&input_pointers.vp);
      input_pointers.vp_asked = true;
   }
   return input_pointers.vp_ok;
}

/* The published device a port reads. */
static unsigned input_pointer_of_port(unsigned port)
{
   if (input_pointers.flags & INPUT_POINTERS_BY_MOUSE_INDEX)
      return input_mouse_port_index(port);
   return (input_pointers.count == 1) ? 0 : port;
}

/* Device @i's place in the viewport and on the screen; false when there
 * is no viewport to place it in. */
static bool input_pointer_place(unsigned i, bool confined,
      uint32_t *pos, uint32_t *screen_pos)
{
   uint16_t bit   = (uint16_t)(1 << i);
   uint16_t *have = confined
      ? &input_pointers.have_confined : &input_pointers.have_viewport;

   if (!input_pointer_viewport())
      return false;
   if (!(*have & bit))
   {
      uint32_t place = 0;
      if (!video_driver_translate_coord_viewport(&input_pointers.vp,
               VIDEO_POS_X(input_pointers.frame[i].pos),
               VIDEO_POS_Y(input_pointers.frame[i].pos),
               &place, &input_pointers.screen_pos[i], !confined))
      {
         input_pointers.vp_ok = false;
         return false;
      }
      if (confined)
         input_pointers.confined_pos[i] = place;
      else
         input_pointers.viewport_pos[i] = place;
      *have |= bit;
   }
   *pos        = confined
      ? input_pointers.confined_pos[i] : input_pointers.viewport_pos[i];
   *screen_pos = input_pointers.screen_pos[i];
   return true;
}

/* Touch @t's place in the viewport, held to its edges, and on the
 * screen; false when there is no viewport. */
static bool input_touch_place(unsigned t, uint32_t *pos, uint32_t *screen_pos)
{
   uint16_t bit = (uint16_t)(1 << t);
   if (!input_pointer_viewport())
      return false;
   if (!(input_pointers.have_touch & bit))
   {
      if (!video_driver_translate_coord_viewport(&input_pointers.vp,
               VIDEO_POS_X(input_pointers.touch_pos[t]),
               VIDEO_POS_Y(input_pointers.touch_pos[t]),
               &input_pointers.touch_confined_pos[t],
               &input_pointers.touch_screen_pos[t], false))
      {
         input_pointers.vp_ok = false;
         return false;
      }
      input_pointers.have_touch |= bit;
   }
   *pos        = input_pointers.touch_confined_pos[t];
   *screen_pos = input_pointers.touch_screen_pos[t];
   return true;
}

/* Pointer @idx of mouse @i: a real touch in that place if there is one,
 * else the mouse standing for it - if the port has a mouse. False if
 * there is neither, or no viewport to place it in. */
static bool input_pointer_touch(unsigned i, unsigned idx,
      uint32_t *pos, uint32_t *screen_pos, bool *down)
{
   const input_pointer_frame_t *f = &input_pointers.frame[
      i < input_pointers.count ? i : 0];
   bool real    = idx < input_pointers.touch_count
               && input_pointers.touch_pos[idx] != 0;
   bool three   = (input_pointers.flags & INPUT_POINTERS_MOUSE_3_TOUCHES) != 0;
   bool mouse   = i < input_pointers.count && idx < (three ? 3u : 1u);

   if (!real && !mouse)
      return false;
   if (real
         ? !input_touch_place(idx, pos, screen_pos)
         : !input_pointer_place(i, true, pos, screen_pos))
      return false;
   *down = real && (input_pointers.touch_down & (1 << idx));
   if (!*down && mouse)
   {
      if (!three)
         *down = (f->buttons & INPUT_POINTER_LEFT) != 0;
      else if (idx == 0)
         *down = (f->buttons & (INPUT_POINTER_LEFT
                  | INPUT_POINTER_RIGHT | INPUT_POINTER_MIDDLE)) != 0;
      else if (idx == 1)
         *down = (f->buttons & (INPUT_POINTER_RIGHT
                  | INPUT_POINTER_MIDDLE)) != 0;
      else
         *down = (f->buttons & INPUT_POINTER_MIDDLE) != 0;
   }
   return true;
}

/* A mouse's, a pointer's or a lightgun aim's position, packed. False if
 * the frontend does not hold it and the driver is to be asked. */
static bool input_pointer_frame_pos(unsigned port, unsigned device,
      unsigned idx, uint32_t *pos)
{
   unsigned i;
   uint32_t screen_pos;

   if (!input_pointers.count)
      return false;
   *pos = 0;
   i    = input_pointer_of_port(port);
   if (i < input_pointers.count)
      input_pointers.read_as[i] = device;
   else if (  device != RETRO_DEVICE_POINTER
           && device != RARCH_DEVICE_POINTER_SCREEN)
      return true;   /* no mouse here; a pointer may still be a touch */
   switch (device)
   {
      case RETRO_DEVICE_MOUSE:
         *pos = input_pointers.frame[i].rel;
         return true;
      case RARCH_DEVICE_MOUSE_SCREEN:
         *pos = input_pointers.frame[i].pos;
         return true;
      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
         {
            uint32_t vp_pos = 0;
            bool down;
            screen_pos      = 0;
            input_pointer_touch(i, idx, &vp_pos, &screen_pos, &down);
            *pos = (device == RETRO_DEVICE_POINTER) ? vp_pos : screen_pos;
         }
         return true;
      case RETRO_DEVICE_LIGHTGUN:
         input_pointer_place(i, false, pos, &screen_pos);
         return true;
      default:
         break;
   }
   return false;
}

/* One value of a mouse, a pointer or a lightgun's aim. False if the
 * frontend does not hold it and the driver is to be asked. */
static bool input_pointer_frame_read(unsigned port, unsigned device,
      unsigned idx, unsigned id, int16_t *out)
{
   const input_pointer_frame_t *f;
   unsigned i;
   uint32_t pos;

   if (!input_pointers.count)
      return false;
   switch (device)
   {
      case RETRO_DEVICE_MOUSE:
      case RARCH_DEVICE_MOUSE_SCREEN:
      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
         break;
      case RETRO_DEVICE_LIGHTGUN:
         /* its aim; its buttons are binds, and the driver's */
         if (     id == RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X
               || id == RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y
               || id == RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN
               || id == RETRO_DEVICE_ID_LIGHTGUN_X
               || id == RETRO_DEVICE_ID_LIGHTGUN_Y)
            break;
         return false;
      default:
         return false;
   }
   *out = 0;
   i    = input_pointer_of_port(port);
   if (i < input_pointers.count)
      input_pointers.read_as[i] = device;
   else if (  device != RETRO_DEVICE_POINTER
           && device != RARCH_DEVICE_POINTER_SCREEN)
      return true;   /* no mouse here; a pointer may still be a touch */
   f = &input_pointers.frame[i < input_pointers.count ? i : 0];

   switch (device)
   {
      case RETRO_DEVICE_MOUSE:
      case RARCH_DEVICE_MOUSE_SCREEN:
         pos = (device == RETRO_DEVICE_MOUSE) ? f->rel : f->pos;
         switch (id)
         {
            case RETRO_DEVICE_ID_MOUSE_X:
               *out = VIDEO_POS_X(pos);
               break;
            case RETRO_DEVICE_ID_MOUSE_Y:
               *out = VIDEO_POS_Y(pos);
               break;
            case RETRO_DEVICE_ID_MOUSE_LEFT:
               *out = (f->buttons & INPUT_POINTER_LEFT) != 0;
               break;
            case RETRO_DEVICE_ID_MOUSE_RIGHT:
               *out = (f->buttons & INPUT_POINTER_RIGHT) != 0;
               break;
            case RETRO_DEVICE_ID_MOUSE_MIDDLE:
               *out = (f->buttons & INPUT_POINTER_MIDDLE) != 0;
               break;
            case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
               *out = (f->buttons & INPUT_POINTER_BUTTON_4) != 0;
               break;
            case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
               *out = (f->buttons & INPUT_POINTER_BUTTON_5) != 0;
               break;
            case RETRO_DEVICE_ID_MOUSE_WHEELUP:
               *out = (f->buttons & INPUT_POINTER_WHEEL_UP) != 0;
               break;
            case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
               *out = (f->buttons & INPUT_POINTER_WHEEL_DOWN) != 0;
               break;
            case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
               *out = (f->buttons & INPUT_POINTER_HWHEEL_UP) != 0;
               break;
            case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
               *out = (f->buttons & INPUT_POINTER_HWHEEL_DOWN) != 0;
               break;
            default:
               break;
         }
         break;
      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
         {
            uint32_t screen_pos;
            bool down;
            if (!input_pointer_touch(i, idx, &pos, &screen_pos, &down))
               break;
            if (device == RARCH_DEVICE_POINTER_SCREEN)
               pos = screen_pos;
            switch (id)
            {
               case RETRO_DEVICE_ID_POINTER_X:
                  *out = VIDEO_POS_X(pos);
                  break;
               case RETRO_DEVICE_ID_POINTER_Y:
                  *out = VIDEO_POS_Y(pos);
                  break;
               case RETRO_DEVICE_ID_POINTER_PRESSED:
                  *out = down;
                  break;
               case RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN:
                  *out = input_driver_pointer_is_offscreen(
                        VIDEO_POS_X(pos), VIDEO_POS_Y(pos));
                  break;
               default:
                  break;
            }
         }
         break;
      case RETRO_DEVICE_LIGHTGUN:
         if (     id == RETRO_DEVICE_ID_LIGHTGUN_X
               || id == RETRO_DEVICE_ID_LIGHTGUN_Y)
         {
            /* deprecated: the mouse's motion */
            *out = (id == RETRO_DEVICE_ID_LIGHTGUN_X)
               ? VIDEO_POS_X(f->rel) : VIDEO_POS_Y(f->rel);
         }
         else
         {
            uint32_t screen_pos;
            if (!input_pointer_place(i, false, &pos, &screen_pos))
               break;
            if (id == RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X)
               *out = VIDEO_POS_X(pos);
            else if (id == RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y)
               *out = VIDEO_POS_Y(pos);
            else
               *out = input_driver_pointer_is_offscreen(
                     VIDEO_POS_X(pos), VIDEO_POS_Y(pos));
         }
         break;
      default:
         break;
   }
   return true;
}

/* Before a driver's poll: one that publishes nothing is asked itself. */
static void input_pointer_frames_clear(void)
{
   input_pointers.count       = 0;
   input_pointers.touch_count = 0;
}

#endif
