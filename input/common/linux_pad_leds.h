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

#ifndef _LINUX_PAD_LEDS_H
#define _LINUX_PAD_LEDS_H

#include <stdint.h>
#include <string.h>

#include <boolean.h>
#include <retro_inline.h>

/* A pad's lights on Linux, as its kernel driver offers them: LED class
 * devices under the pad, each a brightness file. Which a light is, is
 * in its name; what to write for a player is as each console lights
 * its pads. No libudev and nothing of the system's, so
 * samples/input/linux_pad_leds checks it alone. */

enum linux_pad_led
{
   LINUX_PAD_LED_NONE = 0,
   LINUX_PAD_LED_RGB,         /* a light bar: "...:rgb:indicator" (hid-playstation) */
   LINUX_PAD_LED_RED,         /* a light bar's colours: "...:red" (hid-sony) */
   LINUX_PAD_LED_GREEN,
   LINUX_PAD_LED_BLUE,
   LINUX_PAD_LED_DS_PLAYER,   /* a DualSense's five: "...:white:player-N" */
   LINUX_PAD_LED_SW_PLAYER,   /* a Switch pad's four: "...:green:player-N", "...:playerN" */
   LINUX_PAD_LED_PS3_PLAYER,  /* a DualShock 3's four: "...::sonyN" */
   LINUX_PAD_LED_XPAD         /* an Xbox 360 pad's ring: "xpadN" */
};

static INLINE bool linux_pad_led_ends(const char *s, const char *end)
{
   size_t ls = strlen(s), le = strlen(end);
   return ls >= le && !memcmp(s + ls - le, end, le);
}

/* Which light the LED named @name is; @n its number among the player
 * lights, from 1 */
static INLINE enum linux_pad_led linux_pad_led_of(const char *name,
      unsigned *n)
{
   const char *at;
   size_t len;

   *n = 0;
   if (!name)
      return LINUX_PAD_LED_NONE;
   len = strlen(name);
   if (!strncmp(name, "xpad", 4) && len > 4
         && name[4] >= '0' && name[4] <= '9')
      return LINUX_PAD_LED_XPAD;
   if (linux_pad_led_ends(name, ":rgb:indicator"))
      return LINUX_PAD_LED_RGB;
   if (linux_pad_led_ends(name, ":red"))
      return LINUX_PAD_LED_RED;
   if (linux_pad_led_ends(name, ":green"))
      return LINUX_PAD_LED_GREEN;
   if (linux_pad_led_ends(name, ":blue"))
      return LINUX_PAD_LED_BLUE;
   /* the number is the name's last character */
   if (len < 2 || name[len - 1] < '1' || name[len - 1] > '8')
      return LINUX_PAD_LED_NONE;
   *n = (unsigned)(name[len - 1] - '0');
   if ((at = strstr(name, ":white:player-")) && at[14] && !at[15])
      return *n <= 5 ? LINUX_PAD_LED_DS_PLAYER : LINUX_PAD_LED_NONE;
   if (     ((at = strstr(name, "player-")) && at[7] && !at[8])
         || ((at = strstr(name, ":player")) && at[7] && !at[8]))
      return *n <= 4 ? LINUX_PAD_LED_SW_PLAYER : LINUX_PAD_LED_NONE;
   if ((at = strstr(name, "::sony")) && at[6] && !at[7])
      return *n <= 4 ? LINUX_PAD_LED_PS3_PLAYER : LINUX_PAD_LED_NONE;
   *n = 0;
   return LINUX_PAD_LED_NONE;
}

/* What the light @led (number @n) is set to for @player, from 1; less
 * than 1 puts it out. Its brightness, or -1 to leave it. A light bar
 * is a pad's player light only where the pad has no others
 * (@has_players): a DualShock 4's shows the player, a DualSense's is
 * left. @rgb: a light bar's colour, for "...:rgb:indicator"'s
 * multi_intensity. */
static INLINE int linux_pad_led_value(enum linux_pad_led led, unsigned n,
      int player, bool has_players, uint8_t *rgb)
{
   /* as the consoles light them */
   static const uint8_t ds[5]  = { 0x04, 0x0A, 0x15, 0x1B, 0x1F };
   static const uint8_t sw[8]  = { 0x1, 0x3, 0x7, 0xF, 0x9, 0x5, 0xD, 0x6 };
   static const uint8_t ps3[7] = { 0x1, 0x2, 0x4, 0x8, 0x9, 0xA, 0xC };
   /* a DualShock 4's light bar: blue, red, green, pink */
   static const uint8_t bar[4][3] = {
      { 0x00, 0x00, 0x40 }, { 0x40, 0x00, 0x00 },
      { 0x00, 0x40, 0x00 }, { 0x20, 0x00, 0x20 } };
   unsigned bit = n ? 1u << (n - 1) : 0;
   bool on      = player >= 1;
   const uint8_t *col = on ? bar[(player - 1) & 3] : NULL;

   switch (led)
   {
      case LINUX_PAD_LED_RGB:
         if (has_players)
            return -1;
         if (rgb)
         {
            rgb[0] = col ? col[0] : 0;
            rgb[1] = col ? col[1] : 0;
            rgb[2] = col ? col[2] : 0;
         }
         return on ? 255 : 0;
      case LINUX_PAD_LED_RED:
      case LINUX_PAD_LED_GREEN:
      case LINUX_PAD_LED_BLUE:
         if (has_players)
            return -1;
         return col ? col[led - LINUX_PAD_LED_RED] : 0;
      case LINUX_PAD_LED_DS_PLAYER:
         return (on && player <= 5 && (ds[player - 1] & bit)) ? 255 : 0;
      case LINUX_PAD_LED_SW_PLAYER:
         return (on && player <= 8 && (sw[player - 1] & bit)) ? 1 : 0;
      case LINUX_PAD_LED_PS3_PLAYER:
         return (on && player <= 7 && (ps3[player - 1] & bit)) ? 1 : 0;
      case LINUX_PAD_LED_XPAD:
         /* xpad's commands: 6 to 9 light quarter 1 to 4, 0 none */
         return (on && player <= 4) ? 5 + player : 0;
      default:
         break;
   }
   return -1;
}

#endif
