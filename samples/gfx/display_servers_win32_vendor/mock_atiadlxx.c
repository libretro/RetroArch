/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (mock_atiadlxx.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* A stand-in atiadlxx.dll: the seven ADL2 entry points
 * win32_modeline_adl.c resolves, over one adapter with one display
 * and a timing-override table kept in this module. A Set with the
 * force-update flag does what the real driver does after a table
 * refresh: it re-plugs the monitor, here by sending the two device
 * notifications the resync helper waits for to its window. */

#include <windows.h>
#include <dbt.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "mock_adl.h"

#define MAX_OVERRIDES 64

static ADLDisplayModeInfo s_overrides[MAX_OVERRIDES];
static int s_num_overrides;
static int s_num_sets;
static int s_num_force_sets;
static ADL_MAIN_MALLOC_CALLBACK s_alloc;

static const GUID guid_devinterface_monitor =
   { 0xe6f07b5f, 0xee97, 0x4a90,
      { 0xb0, 0x76, 0x33, 0xf5, 0x7b, 0xf4, 0xea, 0xa7 } };

static void mock_replug_monitor(void)
{
   HWND w = FindWindowA("ra_modeline_resync", NULL);
   DEV_BROADCAST_DEVICEINTERFACE db;
   if (getenv("LIVE_VERBOSE"))
      fprintf(stderr, "[mock] resync window %p\n", (void*)w);
   if (!w)
      return;
   memset(&db, 0, sizeof(db));
   db.dbcc_size       = sizeof(db);
   db.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
   db.dbcc_classguid  = guid_devinterface_monitor;
   SendMessageA(w, WM_DEVICECHANGE, DBT_DEVICEARRIVAL, (LPARAM)&db);
   SendMessageA(w, WM_DEVICECHANGE, DBT_DEVNODES_CHANGED, 0);
}

static ADLDisplayModeInfo *mock_find(int w, int h, int refresh)
{
   int i;
   for (i = 0; i < s_num_overrides; i++)
      if (s_overrides[i].iPelsWidth == w && s_overrides[i].iPelsHeight == h
            && s_overrides[i].iRefreshRate == refresh)
         return &s_overrides[i];
   return NULL;
}

__declspec(dllexport) int ADL2_Main_Control_Create(ADL_MAIN_MALLOC_CALLBACK cb,
      int enumerate_connected, ADL_CONTEXT_HANDLE *ctx)
{
   (void)enumerate_connected;
   s_alloc = cb;
   *ctx    = (ADL_CONTEXT_HANDLE)0x4d4f434b;
   return ADL_OK;
}

__declspec(dllexport) int ADL2_Main_Control_Destroy(ADL_CONTEXT_HANDLE ctx)
{
   (void)ctx;
   return ADL_OK;
}

__declspec(dllexport) int ADL2_Adapter_NumberOfAdapters_Get(ADL_CONTEXT_HANDLE ctx, int *n)
{
   (void)ctx;
   *n = 1;
   return ADL_OK;
}

__declspec(dllexport) int ADL2_Adapter_AdapterInfo_Get(ADL_CONTEXT_HANDLE ctx,
      LPAdapterInfo info, int size)
{
   (void)ctx;
   if (size < (int)sizeof(AdapterInfo))
      return ADL_ERR;
   memset(info, 0, sizeof(*info));
   info->iSize         = sizeof(*info);
   info->iAdapterIndex = 0;
   info->iVendorID     = 0x1002;
   strcpy(info->strAdapterName, "Mock Radeon");
   strcpy(info->strDisplayName, "\\\\.\\DISPLAY1");
   info->iPresent = info->iExist = 1;
   return ADL_OK;
}

__declspec(dllexport) int ADL2_Display_DisplayInfo_Get(ADL_CONTEXT_HANDLE ctx,
      int adapter, int *num, ADLDisplayInfo **list, int force)
{
   ADLDisplayInfo *d;
   (void)ctx; (void)force;
   if (adapter != 0 || !s_alloc)
      return ADL_ERR;
   d = (ADLDisplayInfo*)s_alloc((int)sizeof(*d));
   memset(d, 0, sizeof(*d));
   d->displayID.iDisplayLogicalIndex         = 0;
   d->displayID.iDisplayLogicalAdapterIndex  = 0;
   strcpy(d->strDisplayName, "Mock CRT");
   *num  = 1;
   *list = d;
   return ADL_OK;
}

__declspec(dllexport) int ADL2_Display_ModeTimingOverrideList_Get(ADL_CONTEXT_HANDLE ctx,
      int adapter, int display, int max, ADLDisplayModeInfo *list, int *num)
{
   int n = s_num_overrides < max ? s_num_overrides : max;
   (void)ctx; (void)adapter; (void)display;
   memcpy(list, s_overrides, n * sizeof(*list));
   *num = n;
   return ADL_OK;
}

__declspec(dllexport) int ADL2_Display_ModeTimingOverride_Get(ADL_CONTEXT_HANDLE ctx,
      int adapter, int display, ADLDisplayMode *in, ADLDisplayModeInfo *out)
{
   ADLDisplayModeInfo *o;
   (void)ctx; (void)adapter; (void)display;
   o = mock_find(in->iPelsWidth, in->iPelsHeight, in->iDisplayFrequency);
   if (!o)
      return ADL_ERR;
   *out = *o;
   return ADL_OK;
}

__declspec(dllexport) int ADL2_Display_ModeTimingOverride_Set(ADL_CONTEXT_HANDLE ctx,
      int adapter, int display, ADLDisplayModeInfo *mode, int force_update)
{
   ADLDisplayModeInfo *o;
   (void)ctx; (void)adapter; (void)display;
   s_num_sets++;
   o = mock_find(mode->iPelsWidth, mode->iPelsHeight, mode->iRefreshRate);
   if (mode->iTimingStandard == ADL_DL_MODETIMING_STANDARD_DRIVER_DEFAULT)
   {
      if (o)
      {
         *o = s_overrides[s_num_overrides - 1];
         s_num_overrides--;
      }
   }
   else
   {
      if (!o)
      {
         if (s_num_overrides >= MAX_OVERRIDES)
            return ADL_ERR;
         o = &s_overrides[s_num_overrides++];
      }
      *o = *mode;
   }
   if (force_update)
   {
      s_num_force_sets++;
      mock_replug_monitor();
   }
   return ADL_OK;
}

__declspec(dllexport) int ADL2_Flush_Driver_Data(ADL_CONTEXT_HANDLE ctx, int adapter)
{
   (void)ctx; (void)adapter;
   return ADL_OK;
}

/* For the test to inspect the table the backend built */
__declspec(dllexport) int mock_adl_state(int *num_overrides, int *num_sets,
      int *num_force_sets, ADLDisplayModeInfo *copy, int max)
{
   int n = s_num_overrides < max ? s_num_overrides : max;
   *num_overrides  = s_num_overrides;
   *num_sets       = s_num_sets;
   *num_force_sets = s_num_force_sets;
   if (copy)
      memcpy(copy, s_overrides, n * sizeof(*copy));
   return n;
}
