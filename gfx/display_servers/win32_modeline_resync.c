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

#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <dbt.h>

#include <retro_timers.h>
#include <features/features_cpu.h>
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#include "win32_modeline.h"
#include "../../verbosity.h"

/* Waiting for the driver to re-plug the monitor after a timing table
 * refresh takes two notifications: the monitor interface arrival and
 * the device-node change. A hidden window on its own thread receives
 * them and signals the waiter; the wait itself blocks on a condition
 * variable and gives up after RESYNC_TIMEOUT_MS so a driver that
 * never re-plugs cannot hang the mode switch. */
#define RESYNC_TIMEOUT_MS 5000

static const GUID guid_devinterface_monitor =
   { 0xe6f07b5f, 0xee97, 0x4a90,
      { 0xb0, 0x76, 0x33, 0xf5, 0x7b, 0xf4, 0xea, 0xa7 } };

struct win32_modeline_resync
{
#ifdef HAVE_THREADS
   sthread_t *thread;
   slock_t   *lock;
   scond_t   *cond;
#endif
   HWND hwnd;
   bool notified_arrival;
   bool notified_nodes;
   bool ready;
};

/* The window procedure has no user pointer before CreateWindowEx
 * returns, and only one resync helper exists per process. */
static win32_modeline_resync_t *resync_instance = NULL;

#ifdef HAVE_THREADS
static void resync_signal(win32_modeline_resync_t *r, bool arrival)
{
   slock_lock(r->lock);
   if (arrival)
      r->notified_arrival = true;
   else
      r->notified_nodes   = true;
   scond_signal(r->cond);
   slock_unlock(r->lock);
}

static LRESULT CALLBACK resync_wnd_proc(HWND hwnd, UINT msg,
      WPARAM wparam, LPARAM lparam)
{
   win32_modeline_resync_t *r = resync_instance;

   switch (msg)
   {
      case WM_DEVICECHANGE:
         if (!r)
            return 0;
         switch (wparam)
         {
            case DBT_DEVICEARRIVAL:
               {
                  PDEV_BROADCAST_DEVICEINTERFACE db =
                     (PDEV_BROADCAST_DEVICEINTERFACE)lparam;
                  RARCH_DBG("[Resync] DBT_DEVICEARRIVAL\n");
                  if (db && IsEqualGUID(&db->dbcc_classguid,
                           &guid_devinterface_monitor))
                     resync_signal(r, true);
               }
               break;
            case DBT_DEVICEREMOVECOMPLETE:
               RARCH_DBG("[Resync] DBT_DEVICEREMOVECOMPLETE\n");
               break;
            case DBT_DEVNODES_CHANGED:
               RARCH_DBG("[Resync] DBT_DEVNODES_CHANGED\n");
               resync_signal(r, false);
               break;
            default:
               RARCH_DBG("[Resync] WM_DEVICECHANGE %x unhandled\n",
                     (unsigned)wparam);
               break;
         }
         return 0;
      case WM_CLOSE:
         PostQuitMessage(0);
         return 0;
      default:
         break;
   }
   return DefWindowProc(hwnd, msg, wparam, lparam);
}

static void resync_thread(void *data)
{
   MSG msg;
   WNDCLASSEXA wc;
   DEV_BROADCAST_DEVICEINTERFACE filter;
   HDEVNOTIFY notify;
   win32_modeline_resync_t *r = (win32_modeline_resync_t*)data;
   HINSTANCE hinst            = GetModuleHandle(NULL);

   memset(&wc, 0, sizeof(wc));
   wc.cbSize        = sizeof(wc);
   wc.lpfnWndProc   = resync_wnd_proc;
   wc.style         = CS_HREDRAW | CS_VREDRAW;
   wc.hInstance     = hinst;
   wc.lpszClassName = "ra_modeline_resync";
   wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
   RegisterClassExA(&wc);

   r->hwnd = CreateWindowExA(0, "ra_modeline_resync", NULL, WS_POPUP,
         CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, NULL, NULL, hinst, NULL);
   if (!r->hwnd)
      RARCH_ERR("[Resync] Could not create the notification window\n");

   memset(&filter, 0, sizeof(filter));
   filter.dbcc_size       = sizeof(filter);
   filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
   filter.dbcc_classguid  = guid_devinterface_monitor;
   notify = RegisterDeviceNotification(r->hwnd, &filter,
         DEVICE_NOTIFY_WINDOW_HANDLE);
   if (!notify)
      RARCH_ERR("[Resync] Error registering device notification\n");

   slock_lock(r->lock);
   r->ready = true;
   scond_signal(r->cond);
   slock_unlock(r->lock);

   while (GetMessage(&msg, NULL, 0, 0))
   {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
   }

   if (notify)
      UnregisterDeviceNotification(notify);
   DestroyWindow(r->hwnd);
   UnregisterClassA("ra_modeline_resync", hinst);
}
#endif

win32_modeline_resync_t *win32_modeline_resync_new(void)
{
   win32_modeline_resync_t *r = (win32_modeline_resync_t*)calloc(1, sizeof(*r));
   if (!r)
      return NULL;
#ifdef HAVE_THREADS
   r->lock = slock_new();
   r->cond = scond_new();
   if (!r->lock || !r->cond)
   {
      win32_modeline_resync_free(r);
      return NULL;
   }
   resync_instance = r;
   r->thread       = sthread_create(resync_thread, r);
   if (!r->thread)
   {
      win32_modeline_resync_free(r);
      return NULL;
   }
   /* The window must exist before the first wait can be signalled */
   slock_lock(r->lock);
   while (!r->ready)
      scond_wait(r->cond, r->lock);
   slock_unlock(r->lock);
#endif
   return r;
}

void win32_modeline_resync_free(win32_modeline_resync_t *r)
{
   if (!r)
      return;
#ifdef HAVE_THREADS
   if (r->thread)
   {
      if (r->hwnd)
         SendMessage(r->hwnd, WM_CLOSE, 0, 0);
      sthread_join(r->thread);
   }
   if (resync_instance == r)
      resync_instance = NULL;
   if (r->cond)
      scond_free(r->cond);
   if (r->lock)
      slock_free(r->lock);
#endif
   free(r);
}

void win32_modeline_resync_arm(win32_modeline_resync_t *r)
{
#ifdef HAVE_THREADS
   if (!r || !r->thread)
      return;
   slock_lock(r->lock);
   r->notified_arrival = false;
   r->notified_nodes   = false;
   slock_unlock(r->lock);
#else
   (void)r;
#endif
}

void win32_modeline_resync_wait(win32_modeline_resync_t *r)
{
#ifdef HAVE_THREADS
   int64_t start, now;
   const int64_t limit_us = (int64_t)RESYNC_TIMEOUT_MS * 1000;

   if (!r || !r->thread)
      return;

   start = cpu_features_get_time_usec();
   now   = start;
   slock_lock(r->lock);
   while (!(r->notified_arrival && r->notified_nodes))
   {
      int64_t left = limit_us - (now - start);
      if (left <= 0)
      {
         RARCH_WARN("[Resync] No monitor re-plug notification within %d ms\n",
               RESYNC_TIMEOUT_MS);
         break;
      }
      scond_wait_timeout(r->cond, r->lock, left);
      now = cpu_features_get_time_usec();
   }
   /* Consumed; the next arm/wait pair starts clean */
   r->notified_arrival = false;
   r->notified_nodes   = false;
   slock_unlock(r->lock);
   RARCH_DBG("[Resync] Resync time elapsed %d ms\n",
         (int)((now - start) / 1000));
#else
   /* Without a message thread nothing can deliver the notification;
    * give the driver the time it usually takes. */
   retro_sleep(RESYNC_TIMEOUT_MS / 10);
#endif
}
