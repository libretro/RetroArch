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

#ifndef _WIN_PAD_IDENT_H
#define _WIN_PAD_IDENT_H

#include <stddef.h>
#include <stdint.h>

#include <boolean.h>
#include <retro_inline.h>

/* Which of Windows' views of a controller reads it, so that one pad is
 * one port. Windows shows a pad to raw input, to DirectInput and, an
 * Xbox pad, to XInput; a pad off its HID driver (WinUSB, an original
 * Xbox pad) only to the HID driver running beside the joypad driver.
 *
 * Read where a controller connects, never on a frame. Nothing of
 * Windows', so samples/input/win_pad_ident checks it on Linux. */

/* @needle, upper case, in @s in either case. Device paths come in
 * both: raw input gives "\\?\HID#VID_045E&...&IG_00" from some
 * systems and "\\?\hid#vid_045e&...&ig_00" from others. */
static INLINE bool win_pad_path_has(const char *s, const char *needle)
{
   size_t n = 0;
   if (!s)
      return false;
   while (needle[n])
      n++;
   for (; *s; s++)
   {
      size_t i;
      for (i = 0; i < n; i++)
      {
         char c = s[i];
         if (c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');
         if (c != needle[i])
            break;
      }
      if (i == n)
         return true;
   }
   return false;
}

/* A pad Windows drives through XInput: "IG_" in its path */
static INLINE bool win_pad_path_is_xinput(const char *path)
{
   return win_pad_path_has(path, "IG_");
}

/* A pad on Bluetooth: the HID service's UUID in its path - classic,
 * or HID over GATT for a Bluetooth LE pad */
static INLINE bool win_pad_path_is_bluetooth(const char *path)
{
   return win_pad_path_has(path, "00001124-0000-1000-8000-00805F9B34FB")
       || win_pad_path_has(path, "00001812-0000-1000-8000-00805F9B34FB");
}

enum win_pad_driver
{
   WIN_PAD_DRV_WINRAW = 0,     /* raw input */
   WIN_PAD_DRV_WINRAW_XINPUT,  /* raw input, Xbox pads through XInput */
   WIN_PAD_DRV_XINPUT_HYBRID,  /* XInput, the rest through DirectInput */
   WIN_PAD_DRV_DINPUT          /* DirectInput alone */
};

enum win_pad_view
{
   WIN_PAD_VIEW_NONE = 0,
   WIN_PAD_VIEW_XINPUT,
   WIN_PAD_VIEW_RAW,
   WIN_PAD_VIEW_DINPUT,
   WIN_PAD_VIEW_HID            /* the HID driver beside the joypad driver */
};

/* The one view that reads the pad at @path under @drv. @hid_class: it
 * is on Windows' HID driver, where raw input and DirectInput see it.
 * @ignored: another program stands in for it (input_ignore_devices.h). */
static INLINE enum win_pad_view win_pad_owner(enum win_pad_driver drv,
      const char *path, bool hid_class, bool ignored)
{
   if (ignored)
      return WIN_PAD_VIEW_NONE;
   if (!hid_class)
      return WIN_PAD_VIEW_HID;
   if (win_pad_path_is_xinput(path))
      switch (drv)
      {
         case WIN_PAD_DRV_WINRAW:
            return WIN_PAD_VIEW_RAW;
         case WIN_PAD_DRV_DINPUT:
            return WIN_PAD_VIEW_DINPUT;
         default:
            return WIN_PAD_VIEW_XINPUT;
      }
   if (drv == WIN_PAD_DRV_WINRAW || drv == WIN_PAD_DRV_WINRAW_XINPUT)
      return WIN_PAD_VIEW_RAW;
   return WIN_PAD_VIEW_DINPUT;
}

/* A DualShock 4 or DualSense on cable and Bluetooth at once is two
 * devices to Windows and one pad: the same Bluetooth address, read
 * from either link. As one number, 0 for none. */

/* Over USB: feature report 0x12 of a DualShock 4, 0x09 of a DualSense,
 * @buf as read, the report id first; the address in bytes 1 to 6,
 * least significant first */
static INLINE uint64_t win_pad_sony_addr_feature(const uint8_t *buf,
      size_t len)
{
   uint64_t addr = 0;
   int i;
   if (!buf || len < 7)
      return 0;
   for (i = 6; i >= 1; i--)
      addr = (addr << 8) | buf[i];
   return addr;
}

/* Over Bluetooth: the HID serial number string, the address in twelve
 * hex digits, most significant first, separators allowed */
static INLINE uint64_t win_pad_sony_addr_serial(const char *s)
{
   uint64_t addr = 0;
   unsigned digits = 0;
   if (!s)
      return 0;
   for (; *s; s++)
   {
      unsigned v;
      if (*s >= '0' && *s <= '9')
         v = (unsigned)(*s - '0');
      else if (*s >= 'a' && *s <= 'f')
         v = (unsigned)(*s - 'a' + 10);
      else if (*s >= 'A' && *s <= 'F')
         v = (unsigned)(*s - 'A' + 10);
      else if (*s == ':' || *s == '-')
         continue;
      else
         return 0;
      if (++digits > 12)
         return 0;
      addr = (addr << 4) | v;
   }
   return digits == 12 ? addr : 0;
}

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#include <windows.h>

/* The Bluetooth address of the DualShock 4 or DualSense at @path, read
 * from that link; 0 for any other pad, or none given. The device is
 * opened asking no access - HID's requests need none, and a pad
 * another program holds is still read - and HID's two calls are found
 * in hid.dll, so a build that links no HID library has this too. Where
 * a pad connects, never on a frame: the pad answers. */
static INLINE uint64_t win_pad_sony_addr(const char *path,
      uint16_t vid, uint16_t pid)
{
   typedef BOOLEAN (WINAPI *get_serial_t)(HANDLE, PVOID, ULONG);
   typedef BOOLEAN (WINAPI *get_feature_t)(HANDLE, PVOID, ULONG);
   get_serial_t  get_serial;
   get_feature_t get_feature;
   HMODULE hid;
   HANDLE h;
   uint64_t addr = 0;
   bool ds4      = vid == 0x054C && (pid == 0x05C4 || pid == 0x09CC);
   bool ds       = vid == 0x054C && (pid == 0x0CE6 || pid == 0x0DF2);

   if ((!ds4 && !ds) || !path || !*path)
      return 0;
   if (!(hid = LoadLibraryA("hid.dll")))
      return 0;
   get_serial  = (get_serial_t)GetProcAddress(hid,
         "HidD_GetSerialNumberString");
   get_feature = (get_feature_t)GetProcAddress(hid, "HidD_GetFeature");
   h = CreateFileA(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
         OPEN_EXISTING, 0, NULL);
   if (h != INVALID_HANDLE_VALUE)
   {
      if (win_pad_path_is_bluetooth(path))
      {
         WCHAR wide[64];
         char serial[64];
         memset(wide, 0, sizeof(wide));
         if (     get_serial && get_serial(h, wide, sizeof(wide))
               && WideCharToMultiByte(CP_UTF8, 0, wide, -1, serial,
                  sizeof(serial), NULL, NULL) > 0)
            addr = win_pad_sony_addr_serial(serial);
      }
      else
      {
         uint8_t feature[64];
         memset(feature, 0, sizeof(feature));
         feature[0] = ds4 ? 0x12 : 0x09;
         if (get_feature && get_feature(h, feature, sizeof(feature)))
            addr = win_pad_sony_addr_feature(feature, sizeof(feature));
      }
      CloseHandle(h);
   }
   FreeLibrary(hid);
   return addr;
}
#endif

#endif
