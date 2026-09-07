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
#include "../../verbosity.h"

/* Constants and structures from the AMD ADL SDK, the subset the
 * timing-override calls use. The dll is loaded at runtime. */
#define ADL_MAX_PATH   256
#define ADL_OK           0
#define ADL_ERR         -1

#define ADL_DL_TIMINGFLAG_DOUBLE_SCAN               0x0001
#define ADL_DL_TIMINGFLAG_INTERLACED                0x0002
#define ADL_DL_TIMINGFLAG_H_SYNC_POLARITY           0x0004
#define ADL_DL_TIMINGFLAG_V_SYNC_POLARITY           0x0008

#define ADL_DL_MODETIMING_STANDARD_CVT              0x00000001
#define ADL_DL_MODETIMING_STANDARD_GTF              0x00000002
#define ADL_DL_MODETIMING_STANDARD_DMT              0x00000004
#define ADL_DL_MODETIMING_STANDARD_CUSTOM           0x00000008
#define ADL_DL_MODETIMING_STANDARD_DRIVER_DEFAULT   0x00000010
#define ADL_DL_MODETIMING_STANDARD_CVT_RB           0x00000020

/* Timing commands */
#define TIMING_DELETE      0x001
#define TIMING_CREATE      0x002
#define TIMING_UPDATE      0x004
#define TIMING_UPDATE_LIST 0x008

typedef struct AdapterInfo
{
   int iSize;
   int iAdapterIndex;
   char strUDID[ADL_MAX_PATH];
   int iBusNumber;
   int iDeviceNumber;
   int iFunctionNumber;
   int iVendorID;
   char strAdapterName[ADL_MAX_PATH];
   char strDisplayName[ADL_MAX_PATH];
   int iPresent;
   int iExist;
   char strDriverPath[ADL_MAX_PATH];
   char strDriverPathExt[ADL_MAX_PATH];
   char strPNPString[ADL_MAX_PATH];
   int iOSDisplayIndex;
} AdapterInfo, *LPAdapterInfo;

typedef struct ADLDisplayID
{
   int iDisplayLogicalIndex;
   int iDisplayPhysicalIndex;
   int iDisplayLogicalAdapterIndex;
   int iDisplayPhysicalAdapterIndex;
} ADLDisplayID;

typedef struct ADLDisplayInfo
{
   ADLDisplayID displayID;
   int iDisplayControllerIndex;
   char strDisplayName[ADL_MAX_PATH];
   char strDisplayManufacturerName[ADL_MAX_PATH];
   int iDisplayType;
   int iDisplayOutputType;
   int iDisplayConnector;
   int iDisplayInfoMask;
   int iDisplayInfoValue;
} ADLDisplayInfo;

typedef struct ADLDisplayMode
{
   int iPelsHeight;
   int iPelsWidth;
   int iBitsPerPel;
   int iDisplayFrequency;
} ADLDisplayMode;

typedef struct ADLDetailedTiming
{
   int   iSize;
   short sTimingFlags;
   short sHTotal;
   short sHDisplay;
   short sHSyncStart;
   short sHSyncWidth;
   short sVTotal;
   short sVDisplay;
   short sVSyncStart;
   short sVSyncWidth;
   unsigned short sPixelClock;
   short sHOverscanRight;
   short sHOverscanLeft;
   short sVOverscanBottom;
   short sVOverscanTop;
   short sOverscan8B;
   short sOverscanGR;
} ADLDetailedTiming;

typedef struct ADLDisplayModeInfo
{
   int iTimingStandard;
   int iPossibleStandard;
   int iRefreshRate;
   int iPelsWidth;
   int iPelsHeight;
   ADLDetailedTiming sDetailedTiming;
} ADLDisplayModeInfo;

typedef struct AdapterList
{
   ADLDisplayInfo *m_display_list;
   int m_index;
   int m_bus;
   int m_num_of_displays;
   char m_name[ADL_MAX_PATH];
   char m_display_name[ADL_MAX_PATH];
} AdapterList;

typedef void *ADL_CONTEXT_HANDLE;
typedef void *(__stdcall *ADL_MAIN_MALLOC_CALLBACK)(int);
typedef int (*ADL2_MAIN_CONTROL_CREATE)(ADL_MAIN_MALLOC_CALLBACK, int, ADL_CONTEXT_HANDLE *);
typedef int (*ADL2_MAIN_CONTROL_DESTROY)(ADL_CONTEXT_HANDLE);
typedef int (*ADL2_ADAPTER_NUMBEROFADAPTERS_GET)(ADL_CONTEXT_HANDLE, int*);
typedef int (*ADL2_ADAPTER_ADAPTERINFO_GET)(ADL_CONTEXT_HANDLE, LPAdapterInfo, int);
typedef int (*ADL2_DISPLAY_DISPLAYINFO_GET)(ADL_CONTEXT_HANDLE, int, int *, ADLDisplayInfo **, int);
typedef int (*ADL2_DISPLAY_MODETIMINGOVERRIDE_GET)(ADL_CONTEXT_HANDLE, int, int, ADLDisplayMode *, ADLDisplayModeInfo *);
typedef int (*ADL2_DISPLAY_MODETIMINGOVERRIDE_SET)(ADL_CONTEXT_HANDLE, int, int, ADLDisplayModeInfo *, int);
typedef int (*ADL2_DISPLAY_MODETIMINGOVERRIDELIST_GET)(ADL_CONTEXT_HANDLE, int, int, int, ADLDisplayModeInfo *, int *);
typedef int (*ADL2_FLUSH_DRIVER_DATA)(ADL_CONTEXT_HANDLE, int);

typedef struct
{
   ADL2_ADAPTER_NUMBEROFADAPTERS_GET        ADL2_Adapter_NumberOfAdapters_Get;
   ADL2_ADAPTER_ADAPTERINFO_GET             ADL2_Adapter_AdapterInfo_Get;
   ADL2_DISPLAY_DISPLAYINFO_GET             ADL2_Display_DisplayInfo_Get;
   ADL2_DISPLAY_MODETIMINGOVERRIDE_GET      ADL2_Display_ModeTimingOverride_Get;
   ADL2_DISPLAY_MODETIMINGOVERRIDE_SET      ADL2_Display_ModeTimingOverride_Set;
   ADL2_DISPLAY_MODETIMINGOVERRIDELIST_GET  ADL2_Display_ModeTimingOverrideList_Get;
   ADL2_FLUSH_DRIVER_DATA                   ADL2_Flush_Driver_Data;
   HINSTANCE hDLL;
   LPAdapterInfo lpAdapterInfo;
   AdapterList *lpAdapter;
   ADL_CONTEXT_HANDLE m_adl;
   win32_modeline_resync_t *resync;
   ADLDisplayModeInfo *adl_mode;         /* MODELINE_MAX_MODES cache */
   ADLDisplayModeInfo last_override;     /* last staged Set, for flush */
   int iNumberAdapters;
   int m_adapter_index;
   int m_display_index;
   int m_num_of_adl_modes;
   int cat_version;
   int sub_version;
   int last_update_mode;
   bool is_patched;
   bool allow_hardware_refresh;
   bool refresh_required;
   bool have_last;
   char m_display_name[32];
   char m_device_key[128];
} adl_ctx_t;

static void *__stdcall adl_memory_alloc(int size)
{
   return malloc(size);
}

static void adl_memory_free(void **buf)
{
   if (buf && *buf)
   {
      free(*buf);
      *buf = NULL;
   }
}

/* Catalyst 12 and earlier, and 15 and later when reading, invert
 * the polarity bits; interlaced refresh labels follow the same rule. */
static int adl_invert_pol(adl_ctx_t *c, bool on_read)
{
   return ((c->cat_version <= 12) || (c->cat_version >= 15 && on_read));
}

static int adl_interlace_factor(adl_ctx_t *c, bool interlace, bool on_read)
{
   return interlace && ((c->cat_version <= 12)
         || (c->cat_version >= 15 && on_read)) ? 2 : 1;
}

static unsigned adl_caps(void *ctx)
{
   adl_ctx_t *c = (adl_ctx_t*)ctx;
   if (c->allow_hardware_refresh)
      return MODELINE_CAPS_UPDATE | MODELINE_CAPS_ADD | MODELINE_CAPS_DESKTOP_EDITABLE;
   return c->is_patched ? MODELINE_CAPS_UPDATE : 0;
}

static int adl_open(adl_ctx_t *c)
{
   ADL2_MAIN_CONTROL_CREATE ADL2_Main_Control_Create;
   int err = ADL_ERR;

   c->hDLL = LoadLibraryA("atiadlxx.dll");
   if (!c->hDLL)
      c->hDLL = LoadLibraryA("atiadlxy.dll");
   if (!c->hDLL)
   {
      RARCH_DBG("[ADL] Library not found\n");
      return err;
   }

   ADL2_Main_Control_Create = (ADL2_MAIN_CONTROL_CREATE)
      GetProcAddress(c->hDLL, "ADL2_Main_Control_Create");
   if (ADL2_Main_Control_Create)
      err = ADL2_Main_Control_Create(adl_memory_alloc, 1, &c->m_adl);
   return err;
}

static void adl_close(void *ctx)
{
   int i;
   adl_ctx_t *c = (adl_ctx_t*)ctx;
   ADL2_MAIN_CONTROL_DESTROY ADL2_Main_Control_Destroy;

   if (!c)
      return;

   RARCH_DBG("[ADL] Close\n");
   if (c->lpAdapter)
   {
      for (i = 0; i < c->iNumberAdapters; i++)
         adl_memory_free((void**)&c->lpAdapter[i].m_display_list);
   }
   adl_memory_free((void**)&c->lpAdapterInfo);
   adl_memory_free((void**)&c->lpAdapter);

   if (c->hDLL)
   {
      ADL2_Main_Control_Destroy = (ADL2_MAIN_CONTROL_DESTROY)
         GetProcAddress(c->hDLL, "ADL2_Main_Control_Destroy");
      if (ADL2_Main_Control_Destroy && c->m_adl)
         ADL2_Main_Control_Destroy(c->m_adl);
      FreeLibrary(c->hDLL);
   }
   win32_modeline_resync_free(c->resync);
   free(c->adl_mode);
   free(c);
}

static bool adl_get_driver_version(adl_ctx_t *c)
{
   HKEY hkey;
   bool found = false;

   if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, c->m_device_key, 0, KEY_READ, &hkey)
         == ERROR_SUCCESS)
   {
      BYTE cat_ver[32];
      DWORD length = sizeof(cat_ver);
      if ((RegQueryValueExA(hkey, "Catalyst_Version", NULL, NULL, cat_ver, &length) == ERROR_SUCCESS)
            || (RegQueryValueExA(hkey, "RadeonSoftwareVersion", NULL, NULL, cat_ver, &length) == ERROR_SUCCESS)
            || (RegQueryValueExA(hkey, "DriverVersion", NULL, NULL, cat_ver, &length) == ERROR_SUCCESS))
      {
         found         = true;
         c->is_patched = (RegQueryValueExA(hkey, "CalamityRelease", NULL, NULL, NULL, NULL) == ERROR_SUCCESS);
         sscanf((char*)cat_ver, "%d.%d", &c->cat_version, &c->sub_version);
         RARCH_DBG("[ADL] AMD driver version %d.%d%s\n", c->cat_version,
               c->sub_version, c->is_patched ? " (patched)" : "");
      }
      RegCloseKey(hkey);
   }
   return found;
}

static bool adl_enum_displays(adl_ctx_t *c)
{
   int i;

   c->ADL2_Adapter_NumberOfAdapters_Get(c->m_adl, &c->iNumberAdapters);
   if (c->iNumberAdapters <= 0)
      return false;

   c->lpAdapterInfo = (LPAdapterInfo)calloc(c->iNumberAdapters, sizeof(AdapterInfo));
   c->lpAdapter     = (AdapterList*)calloc(c->iNumberAdapters, sizeof(AdapterList));
   if (!c->lpAdapterInfo || !c->lpAdapter)
      return false;

   c->ADL2_Adapter_AdapterInfo_Get(c->m_adl, c->lpAdapterInfo,
         sizeof(AdapterInfo) * c->iNumberAdapters);

   for (i = 0; i < c->iNumberAdapters; i++)
   {
      c->lpAdapter[i].m_index = c->lpAdapterInfo[i].iAdapterIndex;
      c->lpAdapter[i].m_bus   = c->lpAdapterInfo[i].iBusNumber;
      memcpy(c->lpAdapter[i].m_name, c->lpAdapterInfo[i].strAdapterName, ADL_MAX_PATH);
      memcpy(c->lpAdapter[i].m_display_name, c->lpAdapterInfo[i].strDisplayName, ADL_MAX_PATH);
      c->lpAdapter[i].m_num_of_displays = 0;
      c->lpAdapter[i].m_display_list    = NULL;
      /* Display info only for the target adapter: the call is slow */
      if (!strcmp(c->lpAdapter[i].m_display_name, c->m_display_name))
         c->ADL2_Display_DisplayInfo_Get(c->m_adl, c->lpAdapter[i].m_index,
               &c->lpAdapter[i].m_num_of_displays,
               &c->lpAdapter[i].m_display_list, 1);
   }
   return true;
}

static bool adl_get_device_mapping(adl_ctx_t *c)
{
   int i, j;
   for (i = 0; i < c->iNumberAdapters; i++)
   {
      if (strcmp(c->m_display_name, c->lpAdapter[i].m_display_name))
         continue;
      for (j = 0; j < c->lpAdapter[i].m_num_of_displays; j++)
      {
         ADLDisplayInfo *d = &c->lpAdapter[i].m_display_list[j];
         if (c->lpAdapter[i].m_index == d->displayID.iDisplayLogicalAdapterIndex)
         {
            c->m_adapter_index = c->lpAdapter[i].m_index;
            c->m_display_index = d->displayID.iDisplayLogicalIndex;
            return true;
         }
      }
   }
   return false;
}

static bool adl_mode_info_to_modeline(adl_ctx_t *c,
      ADLDisplayModeInfo *dmi, video_modeline_t *m)
{
   ADLDetailedTiming dt;
   if (dmi->sDetailedTiming.sHTotal == 0)
      return false;
   dt = dmi->sDetailedTiming;

   m->htotal     = dt.sHTotal;
   m->hactive    = dt.sHDisplay;
   m->hbegin     = dt.sHSyncStart;
   m->hend       = dt.sHSyncWidth + m->hbegin;
   m->vtotal     = dt.sVTotal;
   m->vactive    = dt.sVDisplay;
   m->vbegin     = dt.sVSyncStart;
   m->vend       = dt.sVSyncWidth + m->vbegin;
   m->interlace  = (dt.sTimingFlags & ADL_DL_TIMINGFLAG_INTERLACED) ? 1 : 0;
   m->doublescan = (dt.sTimingFlags & ADL_DL_TIMINGFLAG_DOUBLE_SCAN) ? 1 : 0;
   m->hsync      = ((dt.sTimingFlags & ADL_DL_TIMINGFLAG_H_SYNC_POLARITY) ? 1 : 0) ^ adl_invert_pol(c, true);
   m->vsync      = ((dt.sTimingFlags & ADL_DL_TIMINGFLAG_V_SYNC_POLARITY) ? 1 : 0) ^ adl_invert_pol(c, true);
   m->pclock     = (uint64_t)dt.sPixelClock * 10000;
   m->height     = m->height  ? m->height  : dmi->iPelsHeight;
   m->width      = m->width   ? m->width   : dmi->iPelsWidth;
   m->refresh    = m->refresh ? m->refresh
      : dmi->iRefreshRate / adl_interlace_factor(c, m->interlace ? true : false, true);
   /* Whole hertz for the line rate, as the driver lists it */
   m->hfreq      = (double)(float)(m->pclock / (uint64_t)m->htotal);
   m->vfreq      = (double)((float)(m->hfreq / m->vtotal) * (m->interlace ? 2 : 1));
   return true;
}

static bool adl_get_timing_list(adl_ctx_t *c)
{
   return c->ADL2_Display_ModeTimingOverrideList_Get(c->m_adl,
         c->m_adapter_index, c->m_display_index, MODELINE_MAX_MODES,
         c->adl_mode, &c->m_num_of_adl_modes) == ADL_OK;
}

static bool adl_get_timing_from_cache(adl_ctx_t *c, video_modeline_t *m)
{
   int i;
   for (i = 0; i < c->m_num_of_adl_modes; i++)
   {
      ADLDisplayModeInfo *mode = &c->adl_mode[i];
      if (mode->iPelsWidth == m->width && mode->iPelsHeight == m->height
            && mode->iRefreshRate == m->refresh)
      {
         if (m->interlace
               && !(mode->sDetailedTiming.sTimingFlags & ADL_DL_TIMINGFLAG_INTERLACED))
            continue;
         return adl_mode_info_to_modeline(c, mode, m);
      }
   }
   return false;
}

static bool adl_get_timing(void *ctx, video_modeline_t *m)
{
   adl_ctx_t *c = (adl_ctx_t*)ctx;
   ADLDisplayMode mode_in;
   ADLDisplayModeInfo mode_info_out;
   video_modeline_t m_temp = *m;

   mode_in.iPelsHeight       = m->height;
   mode_in.iPelsWidth        = m->width;
   mode_in.iBitsPerPel       = 32;
   mode_in.iDisplayFrequency = m->refresh
      * adl_interlace_factor(c, m->interlace ? true : false, true);

   memset(&mode_info_out, 0, sizeof(mode_info_out));
   if (c->ADL2_Display_ModeTimingOverride_Get(c->m_adl, c->m_adapter_index,
            c->m_display_index, &mode_in, &mode_info_out) == ADL_OK)
   {
      if (adl_mode_info_to_modeline(c, &mode_info_out, &m_temp)
            && m_temp.interlace == m->interlace)
      {
         *m       = m_temp;
         m->type |= MODELINE_TIMING_ATI_ADL;
         return true;
      }
   }

   /* Interlaced modes are not returned by the override query; the
    * cached override list has them. */
   if (adl_get_timing_from_cache(c, m))
   {
      m->type |= MODELINE_TIMING_ATI_ADL;
      return true;
   }
   return false;
}

static bool adl_set_timing_override(adl_ctx_t *c, video_modeline_t *m,
      int update_mode)
{
   ADLDisplayModeInfo mode_info;
   ADLDetailedTiming *dt;
   video_modeline_t m_temp;

   memset(&mode_info, 0, sizeof(mode_info));
   mode_info.iTimingStandard   = (update_mode & TIMING_DELETE)
      ? ADL_DL_MODETIMING_STANDARD_DRIVER_DEFAULT : ADL_DL_MODETIMING_STANDARD_CUSTOM;
   mode_info.iPossibleStandard = 0;
   mode_info.iRefreshRate      = m->refresh
      * adl_interlace_factor(c, m->interlace ? true : false, false);
   mode_info.iPelsWidth        = m->width;
   mode_info.iPelsHeight       = m->height;

   dt                   = &mode_info.sDetailedTiming;
   dt->sTimingFlags     = (short)((m->interlace ? ADL_DL_TIMINGFLAG_INTERLACED : 0)
      | (m->doublescan ? ADL_DL_TIMINGFLAG_DOUBLE_SCAN : 0)
      | ((m->hsync ^ adl_invert_pol(c, false)) ? ADL_DL_TIMINGFLAG_H_SYNC_POLARITY : 0)
      | ((m->vsync ^ adl_invert_pol(c, false)) ? ADL_DL_TIMINGFLAG_V_SYNC_POLARITY : 0));
   dt->sHTotal          = (short)m->htotal;
   dt->sHDisplay        = (short)m->hactive;
   dt->sHSyncStart      = (short)m->hbegin;
   dt->sHSyncWidth      = (short)(m->hend - m->hbegin);
   dt->sVTotal          = (short)m->vtotal;
   dt->sVDisplay        = (short)m->vactive;
   dt->sVSyncStart      = (short)m->vbegin;
   dt->sVSyncWidth      = (short)(m->vend - m->vbegin);
   dt->sPixelClock      = (unsigned short)(m->pclock / 10000);
   dt->sHOverscanRight  = 0;
   dt->sHOverscanLeft   = 0;
   dt->sVOverscanBottom = 0;
   dt->sVOverscanTop    = 0;

   if (c->ADL2_Display_ModeTimingOverride_Set(c->m_adl, c->m_adapter_index,
            c->m_display_index, &mode_info,
            (update_mode & TIMING_UPDATE_LIST) ? 1 : 0) != ADL_OK)
      return false;

   c->last_override    = mode_info;
   c->last_update_mode = update_mode;
   c->have_last        = true;

   /* Reading the mode back triggers the timing refresh on modded drivers */
   m_temp = *m;
   if (update_mode & TIMING_UPDATE)
      adl_get_timing(c, &m_temp);
   return true;
}

/* Staging: each op writes its override without the list refresh; the
 * refresh and the monitor resync happen once in flush, after the
 * last staged mode, when any of them needs it. */
static bool adl_add_mode(void *ctx, video_modeline_t *m)
{
   adl_ctx_t *c = (adl_ctx_t*)ctx;
   c->refresh_required = true;
   if (!adl_set_timing_override(c, m, TIMING_UPDATE))
      return false;
   m->type |= MODELINE_TIMING_ATI_ADL;
   return true;
}

static bool adl_delete_mode(void *ctx, video_modeline_t *m)
{
   adl_ctx_t *c = (adl_ctx_t*)ctx;
   c->refresh_required = true;
   return adl_set_timing_override(c, m, TIMING_DELETE);
}

static bool adl_update_mode(void *ctx, video_modeline_t *m)
{
   adl_ctx_t *c = (adl_ctx_t*)ctx;
   if (!c->is_patched || (m->type & MODELINE_DESKTOP))
      c->refresh_required = true;
   if (!adl_set_timing_override(c, m, TIMING_UPDATE))
      return false;
   m->type |= MODELINE_TIMING_ATI_ADL;
   return true;
}

static bool adl_flush(void *ctx)
{
   adl_ctx_t *c = (adl_ctx_t*)ctx;
   bool ok      = true;

   if (c->refresh_required && c->have_last)
   {
      /* Re-issue the last override with the list refresh bit; the
       * driver re-plugs the monitor in response, so the resync is
       * armed before the call and waited for after it */
      win32_modeline_resync_arm(c->resync);
      if (c->ADL2_Display_ModeTimingOverride_Set(c->m_adl, c->m_adapter_index,
               c->m_display_index, &c->last_override, 1) != ADL_OK)
         ok = false;
      win32_modeline_resync_wait(c->resync);
      adl_get_timing_list(c);
   }
   c->refresh_required = false;
   c->have_last        = false;
   return ok;
}

bool win32_modeline_adl_create(win32_modeline_backend_t *b,
      const char *device_name, const char *device_key,
      const video_modeline_disp_t *ds)
{
   adl_ctx_t *c = (adl_ctx_t*)calloc(1, sizeof(*c));
   if (!c)
      return false;

   strlcpy(c->m_display_name, device_name, sizeof(c->m_display_name));
   strlcpy(c->m_device_key, device_key, sizeof(c->m_device_key));
   c->allow_hardware_refresh = ds->allow_hardware_refresh;
   c->adl_mode = (ADLDisplayModeInfo*)calloc(MODELINE_MAX_MODES, sizeof(ADLDisplayModeInfo));
   if (!c->adl_mode)
   {
      free(c);
      return false;
   }

   RARCH_DBG("[ADL] Init\n");
   if (adl_open(c) != ADL_OK)
   {
      RARCH_DBG("[ADL] Initialization error\n");
      adl_close(c);
      return false;
   }

#define ADL_GET(field, type, name) \
   c->field = (type)GetProcAddress(c->hDLL, name); \
   if (!c->field) \
   { \
      RARCH_ERR("[ADL] %s not available\n", name); \
      adl_close(c); \
      return false; \
   }
   ADL_GET(ADL2_Adapter_NumberOfAdapters_Get, ADL2_ADAPTER_NUMBEROFADAPTERS_GET, "ADL2_Adapter_NumberOfAdapters_Get")
   ADL_GET(ADL2_Adapter_AdapterInfo_Get, ADL2_ADAPTER_ADAPTERINFO_GET, "ADL2_Adapter_AdapterInfo_Get")
   ADL_GET(ADL2_Display_DisplayInfo_Get, ADL2_DISPLAY_DISPLAYINFO_GET, "ADL2_Display_DisplayInfo_Get")
   ADL_GET(ADL2_Display_ModeTimingOverride_Get, ADL2_DISPLAY_MODETIMINGOVERRIDE_GET, "ADL2_Display_ModeTimingOverride_Get")
   ADL_GET(ADL2_Display_ModeTimingOverride_Set, ADL2_DISPLAY_MODETIMINGOVERRIDE_SET, "ADL2_Display_ModeTimingOverride_Set")
   ADL_GET(ADL2_Display_ModeTimingOverrideList_Get, ADL2_DISPLAY_MODETIMINGOVERRIDELIST_GET, "ADL2_Display_ModeTimingOverrideList_Get")
   ADL_GET(ADL2_Flush_Driver_Data, ADL2_FLUSH_DRIVER_DATA, "ADL2_Flush_Driver_Data")
#undef ADL_GET

   if (!adl_enum_displays(c))
   {
      RARCH_ERR("[ADL] Error enumerating displays\n");
      adl_close(c);
      return false;
   }
   if (!adl_get_device_mapping(c))
   {
      RARCH_ERR("[ADL] Error mapping display\n");
      adl_close(c);
      return false;
   }
   if (!adl_get_driver_version(c))
      RARCH_ERR("[ADL] Driver version unknown\n");
   if (!adl_get_timing_list(c))
      RARCH_ERR("[ADL] Error getting the timing override list\n");

   c->resync = win32_modeline_resync_new();

   b->ctx         = c;
   b->name        = "AMD ADL";
   b->caps        = adl_caps;
   b->get_timing  = adl_get_timing;
   b->add_mode    = adl_add_mode;
   b->update_mode = adl_update_mode;
   b->delete_mode = adl_delete_mode;
   b->flush       = adl_flush;
   b->close       = adl_close;
   return true;
}
