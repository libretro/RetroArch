/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2026 - Daniel De Matteis
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

/* A DualShock 3 moved to Windows' own WinUSB driver - the
 * Microsoft-signed "WinUsb Device" of winusb.inf - so that
 * winusb_hid.c can start it, and given back to Windows' choice. No
 * driver of RetroArch's own is installed.
 *
 * Changing a device's driver needs administrator rights, which
 * RetroArch does not run with: the menu starts a copy of it with them
 * (one prompt) that does the change and leaves; the pad then comes
 * back on its new driver as if plugged in again. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
#include <setupapi.h>

#include <boolean.h>
#include <compat/strl.h>
#include <string/stdstring.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#include <features/features_cpu.h>
#include <queues/task_queue.h>

#include "../connect/joypad_connection.h"
#include "../../msg_hash.h"
#include "../../verbosity.h"
#include "winusb_bind.h"

#ifdef HAVE_WINUSB_BIND

/* the USBDevice setup class, where "WinUsb Device" is offered */
static const char winusb_bind_usbdevice[] =
   "{88BAE032-5A81-49F0-BC3D-A4FF138216D6}";

/* newdev.dll's: the wide driver record only - an ANSI one is refused */
typedef BOOL (WINAPI *winusb_di_install_t)(HWND, HDEVINFO,
      PSP_DEVINFO_DATA, PSP_DRVINFO_DATA_W, DWORD, PBOOL);
typedef BOOL (WINAPI *winusb_di_uninstall_t)(HWND, HDEVINFO,
      PSP_DEVINFO_DATA, DWORD, PBOOL);
/* cfgmgr32.dll's */
typedef DWORD (WINAPI *winusb_cm_locate_t)(DWORD*, const char*, ULONG);
typedef DWORD (WINAPI *winusb_cm_reenumerate_t)(DWORD, ULONG);

bool winusb_bind_is_ds3(unsigned vid, unsigned pid)
{
   joypad_connection_entry_t *entry = find_connection_entry(
         (uint16_t)vid, (uint16_t)pid, "");
   return entry && entry->iface == &pad_connection_ps3;
}

/* winusb.inf's "WinUsb Device" for @dev, from the list Windows offers
 * when asked to look in that file only, excluded drivers included. */
static bool winusb_bind_find(HDEVINFO devs, SP_DEVINFO_DATA *dev,
      DWORD type, const char *inf, SP_DRVINFO_DATA_W *out)
{
   DWORD i;
   SP_DEVINSTALL_PARAMS_A params;

   params.cbSize = sizeof(params);
   if (!SetupDiGetDeviceInstallParamsA(devs, dev, &params))
      return false;
   params.Flags   |= DI_ENUMSINGLEINF;
   params.FlagsEx |= DI_FLAGSEX_ALLOWEXCLUDEDDRVS;
   strlcpy(params.DriverPath, inf, sizeof(params.DriverPath));
   if (     !SetupDiSetDeviceInstallParamsA(devs, dev, &params)
         || !SetupDiBuildDriverInfoList(devs, dev, type))
      return false;
   for (i = 0; ; i++)
   {
      out->cbSize = sizeof(*out);
      if (!SetupDiEnumDriverInfoW(devs, dev, type, i, out))
         return false;
      if (wcsstr(out->Description, L"WinUsb"))
         return true;
   }
}

static bool winusb_bind_one(HDEVINFO devs, SP_DEVINFO_DATA *dev,
      winusb_di_install_t di_install)
{
   char inf[MAX_PATH];
   char old_class[64];
   SP_DRVINFO_DATA_W drv;
   BOOL reboot = FALSE;
   UINT n      = GetWindowsDirectoryA(inf, sizeof(inf));

   if (!n || n >= sizeof(inf))
      return false;
   strlcat(inf, "\\INF\\winusb.inf", sizeof(inf));

   if (     !winusb_bind_find(devs, dev, SPDIT_CLASSDRIVER, inf, &drv)
         && !winusb_bind_find(devs, dev, SPDIT_COMPATDRIVER, inf, &drv))
   {
      /* only drivers of the device's own class (HIDClass) are offered:
       * moved to USBDevice, as Device Manager's "Show all devices"
       * does, it is offered; put back if even then it is not */
      old_class[0] = '\0';
      SetupDiGetDeviceRegistryPropertyA(devs, dev, SPDRP_CLASSGUID, NULL,
            (BYTE*)old_class, sizeof(old_class), NULL);
      if (!SetupDiSetDeviceRegistryPropertyA(devs, dev, SPDRP_CLASSGUID,
               (const BYTE*)winusb_bind_usbdevice,
               sizeof(winusb_bind_usbdevice)))
         return false;
      SetupDiDestroyDriverInfoList(devs, dev, SPDIT_CLASSDRIVER);
      if (!winusb_bind_find(devs, dev, SPDIT_CLASSDRIVER, inf, &drv))
      {
         if (*old_class)
            SetupDiSetDeviceRegistryPropertyA(devs, dev, SPDRP_CLASSGUID,
                  (const BYTE*)old_class, (DWORD)strlen(old_class) + 1);
         return false;
      }
   }
   SetupDiSetSelectedDriverW(devs, dev, &drv);
   return di_install(NULL, devs, dev, &drv, 0, &reboot) != FALSE;
}

/* Every DualShock 3 plugged in: to WinUSB with @bind, else back to
 * Windows' own choice. How many were changed; -1 when it cannot be
 * done here. */
static int winusb_bind_all(bool bind)
{
   DWORD i;
   int changed = 0;
   HDEVINFO devs;
   HMODULE newdev               = LoadLibraryA("newdev.dll");
   winusb_di_install_t install  = newdev ? (winusb_di_install_t)
      GetProcAddress(newdev, "DiInstallDevice") : NULL;
   winusb_di_uninstall_t remove = newdev ? (winusb_di_uninstall_t)
      GetProcAddress(newdev, "DiUninstallDevice") : NULL;

   if (!install || !remove)
   {
      if (newdev)
         FreeLibrary(newdev);
      return -1;
   }
   if ((devs = SetupDiGetClassDevsA(NULL, "USB", NULL,
               DIGCF_ALLCLASSES | DIGCF_PRESENT)) == INVALID_HANDLE_VALUE)
   {
      FreeLibrary(newdev);
      return -1;
   }
   for (i = 0; ; i++)
   {
      SP_DEVINFO_DATA dev;
      char instance[MAX_PATH];
      char service[64];
      const char *ids;
      unsigned vid, pid;
      bool on_winusb;
      BOOL reboot = FALSE;

      dev.cbSize = sizeof(dev);
      if (!SetupDiEnumDeviceInfo(devs, i, &dev))
         break;
      if (     !SetupDiGetDeviceInstanceIdA(devs, &dev, instance,
                  sizeof(instance), NULL)
            || !(ids = strstr(instance, "VID_"))
            || sscanf(ids, "VID_%4x&PID_%4x", &vid, &pid) != 2
            || !winusb_bind_is_ds3(vid, pid))
         continue;
      if (!SetupDiGetDeviceRegistryPropertyA(devs, &dev, SPDRP_SERVICE,
               NULL, (BYTE*)service, sizeof(service), NULL))
         service[0] = '\0';
      on_winusb = string_is_equal_noncase(service, "WinUSB");
      if (bind && !on_winusb)
      {
         if (winusb_bind_one(devs, &dev, install))
            changed++;
      }
      else if (!bind && on_winusb)
      {
         if (remove(NULL, devs, &dev, 0, &reboot))
            changed++;
      }
   }
   SetupDiDestroyDeviceInfoList(devs);
   FreeLibrary(newdev);

   /* uninstalled, Windows looks again and chooses as at first plug-in */
   if (!bind && changed)
   {
      HMODULE cfgmgr = LoadLibraryA("cfgmgr32.dll");
      if (cfgmgr)
      {
         winusb_cm_locate_t locate = (winusb_cm_locate_t)
            GetProcAddress(cfgmgr, "CM_Locate_DevNodeA");
         winusb_cm_reenumerate_t reenumerate = (winusb_cm_reenumerate_t)
            GetProcAddress(cfgmgr, "CM_Reenumerate_DevNode");
         DWORD root;
         if (     locate && reenumerate
               && locate(&root, NULL, 0 /* CM_LOCATE_DEVNODE_NORMAL */) == 0)
            reenumerate(root, 1 /* CM_REENUMERATE_SYNCHRONOUS */);
         FreeLibrary(cfgmgr);
      }
   }
   return changed;
}

bool winusb_bind_command(const char *arg, int *ret)
{
   int changed;
   bool bind;

   if (string_is_equal(arg, WINUSB_BIND_ARG))
      bind = true;
   else if (string_is_equal(arg, WINUSB_RESTORE_ARG))
      bind = false;
   else
      return false;
   changed = winusb_bind_all(bind);
   *ret    = (changed < 0) ? 255 : changed;
   return true;
}

/* Asked for from the menu. The prompt for administrator rights is
 * answered on a thread of its own, which waits for the copy started
 * with them; a task looks at it now and then and says how it went. */
enum
{
   WINUSB_BIND_IDLE = 0,
   WINUSB_BIND_RUNNING,
   WINUSB_BIND_DONE
};

#define WINUSB_BIND_POLL_USEC 100000

typedef BOOL (WINAPI *winusb_shell_execute_t)(SHELLEXECUTEINFOA*);
typedef HRESULT (WINAPI *winusb_co_init_t)(void*, DWORD);
typedef void (WINAPI *winusb_co_uninit_t)(void);

static struct
{
   retro_atomic_int_t state;
   int  result;   /* the copy's exit code; -1 not started, -2 declined */
   bool bind;
} winusb_bind_job;

static void winusb_bind_thread(void *data)
{
   char self[MAX_PATH];
   SHELLEXECUTEINFOA sei;
   DWORD code                  = 255;
   int result                  = -1;
   HMODULE shell               = LoadLibraryA("shell32.dll");
   HMODULE ole                 = LoadLibraryA("ole32.dll");
   winusb_shell_execute_t exec = shell ? (winusb_shell_execute_t)
      GetProcAddress(shell, "ShellExecuteExA") : NULL;
   winusb_co_init_t co_init    = ole ? (winusb_co_init_t)
      GetProcAddress(ole, "CoInitializeEx") : NULL;
   winusb_co_uninit_t co_done  = ole ? (winusb_co_uninit_t)
      GetProcAddress(ole, "CoUninitialize") : NULL;
   bool com                    = co_init && co_done && SUCCEEDED(co_init(
            NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE));
   DWORD n                     = GetModuleFileNameA(NULL, self, sizeof(self));

   (void)data;
   if (exec && n && n < sizeof(self))
   {
      memset(&sei, 0, sizeof(sei));
      sei.cbSize       = sizeof(sei);
      sei.fMask        = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
      sei.lpVerb       = "runas";
      sei.lpFile       = self;
      sei.lpParameters = winusb_bind_job.bind
         ? WINUSB_BIND_ARG : WINUSB_RESTORE_ARG;
      sei.nShow        = SW_HIDE;
      if (!exec(&sei))
         result = (GetLastError() == ERROR_CANCELLED) ? -2 : -1;
      else if (sei.hProcess)
      {
         WaitForSingleObject(sei.hProcess, INFINITE);
         if (GetExitCodeProcess(sei.hProcess, &code))
            result = (int)code;
         CloseHandle(sei.hProcess);
      }
   }
   if (com)
      co_done();
   if (ole)
      FreeLibrary(ole);
   if (shell)
      FreeLibrary(shell);
   winusb_bind_job.result = result;
   retro_atomic_store_release_int(&winusb_bind_job.state, WINUSB_BIND_DONE);
}

static void winusb_bind_task_handler(retro_task_t *task)
{
   enum msg_hash_enums msg;
   int result;

   if (retro_atomic_load_acquire_int(&winusb_bind_job.state)
         != WINUSB_BIND_DONE)
   {
      task->when = cpu_features_get_time_usec() + WINUSB_BIND_POLL_USEC;
      return;
   }
   result = winusb_bind_job.result;
   if (result == -2)
      msg = MSG_INPUT_DS3_DECLINED;
   else if (result <= 0 || result == 255)
      msg = MSG_INPUT_DS3_NOT_CHANGED;
   else
      msg = winusb_bind_job.bind ? MSG_INPUT_DS3_ENABLED
                                 : MSG_INPUT_DS3_RESTORED;
   RARCH_LOG("[WinUSB] %s (%d).\n", msg_hash_to_str(msg), result);
   task_free_title(task);
   task_set_title(task, strdup(msg_hash_to_str(msg)));
   retro_atomic_store_release_int(&winusb_bind_job.state, WINUSB_BIND_IDLE);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

bool winusb_bind_request(bool bind)
{
   retro_task_t *task;
   sthread_t *thread;

   if (!retro_atomic_cas_int(&winusb_bind_job.state,
            WINUSB_BIND_IDLE, WINUSB_BIND_RUNNING))
      return false;
   winusb_bind_job.bind   = bind;
   winusb_bind_job.result = -1;
   if (!(task = task_init()))
   {
      retro_atomic_store_release_int(&winusb_bind_job.state, WINUSB_BIND_IDLE);
      return false;
   }
   if (!(thread = sthread_create(winusb_bind_thread, NULL)))
   {
      free(task);
      retro_atomic_store_release_int(&winusb_bind_job.state, WINUSB_BIND_IDLE);
      return false;
   }
   sthread_detach(thread);
   task->handler = winusb_bind_task_handler;
   task->title   = strdup(msg_hash_to_str(MSG_INPUT_DS3_ASKING));
   task->flags  &= ~RETRO_TASK_FLG_MUTE;
   task_queue_push(task);
   return true;
}
#endif
