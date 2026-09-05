/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2021 - Chris Kennedy, Antonio Giner,
 *                            Alexandre Wodarczyk, Gil Delescluse
 *  Copyright (C) 2026 - The RetroArch team
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include <compat/strl.h>

#include "win32_modeline.h"
#include "win32_modeline_ati_ids.h"
#include "../../verbosity.h"

/* Pre-Cedar Radeons keep one DALDTMCRTBCD<w>x<h>x0x<r> value per
 * listed mode under the adapter's registry key: 68 bytes of
 * big-endian BCD-coded timing plus a checksum. Writing it and then
 * enumerating display settings makes the driver reload it. Needs an
 * elevated process on Vista and later. */

#define CRTC_DOUBLE_SCAN      0x0001
#define CRTC_INTERLACED       0x0002
#define CRTC_H_SYNC_POLARITY  0x0004
#define CRTC_V_SYNC_POLARITY  0x0008

typedef struct
{
   int win_version;
   char m_device_name[32];
   char m_device_key[256];
} ati_ctx_t;

static int ati_family(int vendor, int device)
{
   size_t i;
   if (vendor != 0x1002)
      return 0;
   for (i = 0; i < sizeof(radeon_pci_ids) / sizeof(radeon_pci_ids[0]); i++)
      if (radeon_pci_ids[i].device == device)
         return radeon_pci_ids[i].family;
   /* Not listed: newer than the table */
   return CHIP_LAST;
}

bool win32_modeline_ati_is_legacy(int vendor, int device)
{
   return ati_family(vendor, device) < CHIP_CEDAR;
}

static int ati_get_dword(int i, const char *lp_data)
{
   return (int)(((unsigned)(lp_data[i]     & 0xFF) << 24)
              | ((unsigned)(lp_data[i + 1] & 0xFF) << 16)
              | ((unsigned)(lp_data[i + 2] & 0xFF) << 8)
              |  (unsigned)(lp_data[i + 3] & 0xFF));
}

/* Four BCD bytes read as decimal digits */
static int ati_get_dword_bcd(int i, const char *lp_data)
{
   char out[32];
   unsigned x = 0;
   snprintf(out, sizeof(out), "%02X%02X%02X%02X",
         lp_data[i] & 0xFF, lp_data[i + 1] & 0xFF,
         lp_data[i + 2] & 0xFF, lp_data[i + 3] & 0xFF);
   sscanf(out, "%u", &x);
   return (int)x;
}

static void ati_set_dword(char *data_string, unsigned data_dword, int offset)
{
   data_string[offset]     = (char)((data_dword >> 24) & 0xFF);
   data_string[offset + 1] = (char)((data_dword >> 16) & 0xFF);
   data_string[offset + 2] = (char)((data_dword >> 8) & 0xFF);
   data_string[offset + 3] = (char)(data_dword & 0xFF);
}

static void ati_set_dword_bcd(char *data_string, unsigned data_dword, int offset)
{
   if (data_dword < 100000000)
   {
      unsigned low_word, high_word;
      unsigned a, b, c, d;
      char out[32];
      low_word  = data_dword % 10000;
      high_word = data_dword / 10000;
      snprintf(out, sizeof(out), "%u %u %u %u",
            high_word / 100, high_word % 100, low_word / 100, low_word % 100);
      sscanf(out, "%02X %02X %02X %02X", &a, &b, &c, &d);
      data_string[offset]     = (char)a;
      data_string[offset + 1] = (char)b;
      data_string[offset + 2] = (char)c;
      data_string[offset + 3] = (char)d;
   }
}

static int ati_os_version(void)
{
   OSVERSIONINFOA vi;
   memset(&vi, 0, sizeof(vi));
   vi.dwOSVersionInfoSize = sizeof(vi);
   GetVersionExA(&vi);
   return vi.dwMajorVersion;
}

static bool ati_is_elevated(void)
{
   HANDLE htoken;
   TOKEN_ELEVATION te;
   DWORD len;
   bool result = false;

   if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &htoken))
      return false;
   memset(&te, 0, sizeof(te));
   if (GetTokenInformation(htoken, TokenElevation, &te, sizeof(te), &len))
      result = te.TokenIsElevated ? true : false;
   CloseHandle(htoken);
   return result;
}

static int ati_win_interlace_factor(ati_ctx_t *c, const video_modeline_t *mode)
{
   if (c->win_version > 5 && mode->interlace)
      return 2;
   return 1;
}

static unsigned ati_caps(void *ctx)
{
   return MODELINE_CAPS_UPDATE | MODELINE_CAPS_SCAN_EDITABLE;
}

static bool ati_get_timing(void *ctx, video_modeline_t *mode)
{
   HKEY hkey;
   char lp_name[64];
   char lp_data[68];
   DWORD length;
   bool found        = false;
   ati_ctx_t *c      = (ati_ctx_t*)ctx;
   int refresh_label = mode->refresh_label ? mode->refresh_label
      : mode->refresh * ati_win_interlace_factor(c, mode);

   if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, c->m_device_key, 0, KEY_ALL_ACCESS, &hkey)
         != ERROR_SUCCESS)
   {
      RARCH_DBG("[ATI] Failed opening registry entry for mode\n");
      return false;
   }

   snprintf(lp_name, sizeof(lp_name), "DALDTMCRTBCD%dx%dx0x%d",
         mode->width, mode->height, refresh_label);
   length = sizeof(lp_data);
   if (RegQueryValueExA(hkey, lp_name, NULL, NULL, (LPBYTE)lp_data, &length)
         == ERROR_SUCCESS && length == sizeof(lp_data))
      found = true;
   else if (c->win_version > 5 && mode->interlace)
   {
      snprintf(lp_name, sizeof(lp_name), "DALDTMCRTBCD%dx%dx0x%d",
            mode->width, mode->height, refresh_label + 1);
      length = sizeof(lp_data);
      if (RegQueryValueExA(hkey, lp_name, NULL, NULL, (LPBYTE)lp_data, &length)
            == ERROR_SUCCESS && length == sizeof(lp_data))
         found = true;
   }

   if (found)
   {
      int checksum;
      mode->pclock    = (uint64_t)ati_get_dword_bcd(36, lp_data) * 10000;
      mode->hactive   = ati_get_dword_bcd(8, lp_data);
      mode->hbegin    = ati_get_dword_bcd(12, lp_data);
      mode->hend      = ati_get_dword_bcd(16, lp_data) + mode->hbegin;
      mode->htotal    = ati_get_dword_bcd(4, lp_data);
      mode->vactive   = ati_get_dword_bcd(24, lp_data);
      mode->vbegin    = ati_get_dword_bcd(28, lp_data);
      mode->vend      = ati_get_dword_bcd(32, lp_data) + mode->vbegin;
      mode->vtotal    = ati_get_dword_bcd(20, lp_data);
      mode->interlace = (ati_get_dword(0, lp_data) & CRTC_INTERLACED) ? 1 : 0;
      mode->hsync     = (ati_get_dword(0, lp_data) & CRTC_H_SYNC_POLARITY) ? 0 : 1;
      mode->vsync     = (ati_get_dword(0, lp_data) & CRTC_V_SYNC_POLARITY) ? 0 : 1;
      /* Whole hertz for the line rate, as the driver lists it */
      mode->hfreq     = (double)(mode->pclock / (uint64_t)mode->htotal);
      mode->vfreq     = mode->hfreq / mode->vtotal * (mode->interlace ? 2 : 1);
      mode->refresh_label = refresh_label;
      mode->type     |= MODELINE_TIMING_ATI_LEGACY;

      checksum = 65535 - ati_get_dword(0, lp_data) - mode->htotal - mode->hactive
         - mode->hend - mode->vtotal - mode->vactive - mode->vend
         - (int)(mode->pclock / 10000);
      if (checksum != ati_get_dword(64, lp_data))
         RARCH_DBG("[ATI] Bad checksum on %s\n", lp_name);
   }
   RegCloseKey(hkey);
   return found;
}

static bool ati_set_timing(ati_ctx_t *c, video_modeline_t *mode)
{
   HKEY hkey;
   char lp_name[64];
   char lp_data[68];
   long checksum;
   bool found        = false;
   int refresh_label = mode->refresh_label ? mode->refresh_label
      : mode->refresh * ati_win_interlace_factor(c, mode);

   memset(lp_data, 0, sizeof(lp_data));
   ati_set_dword_bcd(lp_data, (unsigned)(mode->pclock / 10000), 36);
   ati_set_dword_bcd(lp_data, mode->hactive, 8);
   ati_set_dword_bcd(lp_data, mode->hbegin, 12);
   ati_set_dword_bcd(lp_data, mode->hend - mode->hbegin, 16);
   ati_set_dword_bcd(lp_data, mode->htotal, 4);
   ati_set_dword_bcd(lp_data, mode->vactive, 24);
   ati_set_dword_bcd(lp_data, mode->vbegin, 28);
   ati_set_dword_bcd(lp_data, mode->vend - mode->vbegin, 32);
   ati_set_dword_bcd(lp_data, mode->vtotal, 20);
   ati_set_dword(lp_data, (mode->interlace ? CRTC_INTERLACED : 0)
         | (mode->hsync ? 0 : CRTC_H_SYNC_POLARITY)
         | (mode->vsync ? 0 : CRTC_V_SYNC_POLARITY), 0);
   checksum = 65535 - ati_get_dword(0, lp_data) - mode->htotal - mode->hactive
      - mode->hend - mode->vtotal - mode->vactive - mode->vend
      - (long)(mode->pclock / 10000);
   ati_set_dword(lp_data, (unsigned)checksum, 64);

   if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, c->m_device_key, 0, KEY_ALL_ACCESS, &hkey)
         != ERROR_SUCCESS)
   {
      RARCH_LOG("[ATI] Failed updating registry entry for mode\n");
      return false;
   }

   snprintf(lp_name, sizeof(lp_name), "DALDTMCRTBCD%dx%dx0x%d",
         mode->width, mode->height, refresh_label);
   if (RegQueryValueExA(hkey, lp_name, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
      found = true;
   else if (c->win_version > 5 && mode->interlace)
   {
      snprintf(lp_name, sizeof(lp_name), "DALDTMCRTBCD%dx%dx0x%d",
            mode->width, mode->height, refresh_label + 1);
      if (RegQueryValueExA(hkey, lp_name, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
         found = true;
   }
   if (!(found && RegSetValueExA(hkey, lp_name, 0, REG_BINARY,
               (LPBYTE)lp_data, 68) == ERROR_SUCCESS))
      RARCH_LOG("[ATI] Failed saving registry entry %s\n", lp_name);
   RegCloseKey(hkey);
   return found;
}

static bool ati_update_mode(void *ctx, video_modeline_t *mode)
{
   ati_ctx_t *c = (ati_ctx_t*)ctx;
   if (!ati_set_timing(c, mode))
      return false;
   mode->type |= MODELINE_TIMING_ATI_LEGACY;
   return true;
}

/* An EnumDisplaySettings pass makes the driver reread the timings */
static bool ati_flush(void *ctx)
{
   int i = 0;
   DEVMODEA dm;
   ati_ctx_t *c = (ati_ctx_t*)ctx;
   memset(&dm, 0, sizeof(dm));
   dm.dmSize = sizeof(dm);
   while (EnumDisplaySettingsExA(c->m_device_name, i, &dm, 0) != 0)
      i++;
   return true;
}

static void ati_close(void *ctx)
{
   free(ctx);
}

bool win32_modeline_ati_create(win32_modeline_backend_t *b,
      const char *device_name, const char *device_key,
      const video_modeline_disp_t *ds)
{
   ati_ctx_t *c = (ati_ctx_t*)calloc(1, sizeof(*c));
   if (!c)
      return false;

   strlcpy(c->m_device_name, device_name, sizeof(c->m_device_name));
   strlcpy(c->m_device_key, device_key, sizeof(c->m_device_key));

   RARCH_DBG("[ATI] Legacy init\n");
   c->win_version = ati_os_version();
   if (c->win_version > 5 && !ati_is_elevated())
   {
      RARCH_ERR("[ATI] The legacy timing path needs administrator rights\n");
      free(c);
      return false;
   }

   b->ctx         = c;
   b->name        = "ATI Legacy";
   b->caps        = ati_caps;
   b->get_timing  = ati_get_timing;
   b->add_mode    = NULL;
   b->update_mode = ati_update_mode;
   b->delete_mode = ati_update_mode;  /* restores the backed-up timing */
   b->flush       = ati_flush;
   b->close       = ati_close;
   return true;
}
