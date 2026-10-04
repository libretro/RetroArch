/*  RetroArch - A frontend for libretro.
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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef CXX_BUILD
extern "C" {
#endif

#include <hidsdi.h>

#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0500 /* 2K */
#include <dbt.h>
#include <cfgmgr32.h>
#endif

#ifdef CXX_BUILD
}
#endif

#include <compat/strl.h>
#include <string/stdstring.h>
#include <retro_atomic.h>

#ifndef _XBOX
#include "../../gfx/common/win32_common.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include <queues/task_queue.h>

#include "../input_keymaps.h"
#include "../common/input_keyboard_devices.h"

/* Threading model
 * ---------------
 * (This is how the driver was, and is with RETROARCH_RAWINPUT_POLL=0
 * in the environment. By default the window is the polling thread's
 * and the reports are read in bulk by the poll: see "Read by the
 * poll" further down.)
 *
 * On every Windows video driver that can actually use this input
 * driver, it is built by the video context driver's input_driver
 * callback - gfx_ctx_wgl_input_driver() and the w_vk / d3d_common /
 * gdi equivalents - rather than by the runloop. Under video_threaded
 * the video driver's init() runs on the video thread
 * (video_thread_wrapper.c, CMD_INIT), so winraw_init() and therefore
 * winraw_create_window() run there too.
 *
 * There is a second path. A video driver may leave *input NULL, and
 * video_driver_init_input() then picks the configured driver itself -
 * on the main thread, since video_driver_init_internal() calls it
 * after the video thread has been spun up. A winraw window created
 * that way would not share a thread with the pump and would never see
 * WM_INPUT. The only Windows driver that takes that path is sdl2,
 * which sets *input = NULL at sdl2_gfx.c:470, and that combination is
 * already unusable and warned about for an unrelated reason - see the
 * note at the end of this comment. Worth knowing if another driver
 * ever stops providing one.
 *
 * That matters because RegisterRawInputDevices() targets wr->window,
 * a HWND_MESSAGE window, and WM_INPUT is delivered to the queue of the
 * thread that created it. The pump,
 * ui_application_win32_process_events(), uses
 * PeekMessage(&msg, 0, 0, 0, PM_REMOVE), which retrieves messages for
 * every window belonging to the calling thread - and it runs on the
 * main thread when video is not threaded (runloop.c) or on the video
 * thread when it is (win32_check_window()). Either way the pump and
 * wr->window are on the same thread, so WM_INPUT is dispatched in both
 * configurations.
 *
 * The split that does exist: winraw_callback() runs on whichever
 * thread owns the window, while winraw_poll() is called from
 * input_driver_poll() in the runloop, always on the main thread. Every
 * field the two share crosses a thread boundary. dlt_x/dlt_y,
 * whl_u/whl_d, pos_pending, abs_pending and abs_pos are
 * retro_atomic_int_t for that reason.
 *
 * x and y are plain LONG because they have exactly one writer:
 * winraw_poll(). The wndproc never reads or writes them. It publishes
 * what it knows - an accumulated delta, a request to sample the system
 * cursor, or an absolute position - and poll derives the position from
 * whichever applies, once per frame. abs_ref_x/abs_ref_y are the
 * wndproc's own copy of the last absolute report, used to turn
 * successive absolute positions into deltas without reading the
 * position it does not own.
 *
 * It used to be two writers both doing read-modify-write, which was a
 * lost-update race that atomics would not have fixed - only made to
 * look synchronised.
 *
 * Note also that whoever drains raw input owns it process-wide: the
 * SDL2 video driver calls RegisterRawInputDevices() and
 * GetRawInputBuffer() internally, which takes the WM_INPUT stream away
 * from this driver entirely. See the warning in gfx/drivers/sdl2_gfx.c.
 *
 * That cuts both ways, and it is the reason GetRawInputBuffer() is not
 * used here. It drains the queue for the whole thread, not for one
 * window, so a drain in winraw_callback() also takes the joystick and
 * gamepad records that winraw_joypad registered against its own
 * HWND_MESSAGE window - created on this same thread by
 * input_driver_init_joypads(), immediately after winraw_init(). This
 * was tried in d087a820cd and reverted in 7858994d45: forwarding the
 * RIM_TYPEHID records back to the joypad driver restored pad input, but
 * keyboard and mouse buttons stayed broken for a reason that was never
 * established, while mouse coordinates kept working. Anyone trying
 * again needs to be the single drain point for the entire process,
 * which means winraw_joypad giving up its own window and its own
 * GetRawInputData() call, and should know the prize is small: measured
 * at 8.32 reports per frame with a 1000 Hz mouse at 120 fps, the drain
 * removes about seven of the roughly seventeen syscalls per frame,
 * since WM_INPUT is still posted per report and PeekMessage() with
 * PM_REMOVE has no user-mode fast path.
 */

#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

enum winraw_mouse_flags
{
   WRAW_MOUSE_FLG_BTN_L  = (1 << 0),
   WRAW_MOUSE_FLG_BTN_M  = (1 << 1),
   WRAW_MOUSE_FLG_BTN_R  = (1 << 2),
   WRAW_MOUSE_FLG_BTN_B4 = (1 << 3),
   WRAW_MOUSE_FLG_BTN_B5 = (1 << 4)
};

typedef struct
{
   HANDLE hnd;
   /* Position. Written only by winraw_poll(), on the main thread. The
    * wndproc never touches these - it publishes what it knows through
    * the atomics below and poll derives the position from them once
    * per frame. Two writers doing read-modify-write here was a
    * lost-update race that no amount of atomicity would have fixed. */
   LONG x, y;
   /* Absolute-position reference, touched only by the wndproc. Used to
    * turn successive MOUSE_MOVE_ABSOLUTE reports into deltas. */
   LONG abs_ref_x, abs_ref_y;
   /* Produced by the wndproc, drained once per frame by winraw_poll().
    * The snapshot in winraw_input_t::mice is single-threaded; only the
    * g_mice originals are ever accessed concurrently. */
   retro_atomic_int_t dlt_x, dlt_y;
   retro_atomic_int_t whl_u, whl_d;
   /* Set by the wndproc when this mouse needs its position taken from
    * the system cursor, drained once per frame by winraw_poll(). */
   retro_atomic_int_t pos_pending;
   /* Set by the wndproc for a MOUSE_MOVE_ABSOLUTE report, with the
    * scaled position alongside as one VIDEO_POS_PACK word, so poll
    * never pairs one report's x with another's y. Takes precedence
    * over the accumulated delta, and yields to pos_pending. */
   retro_atomic_int_t abs_pending;
   retro_atomic_int_t abs_pos;
   int device;
   uint8_t flags;
} winraw_mouse_t;

struct winraw_pointer_status
{
   struct winraw_pointer_status *next;
   int pointer_id;
   int pointer_x;
   int pointer_y;
};

/* Key events held until the end of a poll. A frame's worth is a
 * handful. */
#define WINRAW_KEV_SIZE 256

/* One bulk read's worth of reports: over a thousand mouse reports. */
#define WINRAW_DRAIN_BYTES (64 * 1024)

enum winraw_input_flags
{
   WRAW_INP_FLG_MOUSE_GRAB             = (1 << 0),
   WRAW_INP_FLG_MOUSE_XY_MAPPING_READY = (1 << 1),
   WRAW_INP_FLG_KB_PAUSE               = (1 << 2)
};

/* keyboards told apart, at most; more than these are not listed */
#define WINRAW_KEYBOARDS_MAX MAX_INPUT_DEVICES
/* What raw input calls a keyboard is anything that can send keys, and
 * one keyboard on the desk is often several of those; this many of
 * them are kept track of. */
#define WINRAW_KB_RAW_MAX    32
/* a raw input keyboard that is part of no listed keyboard */
#define WINRAW_KB_NONE       0xFF

/* One keyboard's own keys: a bit a scancode. The scancodes the driver
 * takes are a make code alone, one with the E0 or the E1 prefix, and
 * SC_PAUSE; that is three runs of 256 and one more. */
#define WINRAW_KB_BITS  (3 * 256 + 1)
#define WINRAW_KB_BYTES ((WINRAW_KB_BITS + 7) / 8)

/* The bit of a scancode, or WINRAW_KB_BITS for one that has none. */
static INLINE unsigned winraw_kb_bit(unsigned mcode)
{
   switch (mcode >> 8)
   {
      case 0x00:
         return mcode;
      case 0xE0:
         return 256 + (mcode & 0xFF);
      case 0xE1:
         return 512 + (mcode & 0xFF);
   }
   return (mcode == SC_PAUSE) ? 768 : WINRAW_KB_BITS;
}

/* and the scancode of a bit */
static INLINE unsigned winraw_kb_mcode(unsigned bit)
{
   if (bit < 256)
      return bit;
   if (bit < 512)
      return 0xE000 | (bit - 256);
   if (bit < 768)
      return 0xE100 | (bit - 512);
   return SC_PAUSE;
}

typedef struct
{
   double view_abs_ratio_x;
   double view_abs_ratio_y;
   /* Raised by the wndproc when an absolute report arrives before the
    * xy mapping exists. winraw_poll() builds it, because
    * winraw_init_mouse_xy_mapping() writes the position and the wndproc
    * no longer owns that. */
   retro_atomic_int_t map_pending;
   HWND window;
   /* Dummy head for easier iteration */
   struct winraw_pointer_status pointer_head;
   RECT active_rect; /* Needed for checking for a windows size change */
   RECT prev_rect;   /* Needed for checking for a windows size change */
   int rect_delay;   /* Needed to delay resize of window */
   winraw_mouse_t *mice;
   unsigned mouse_cnt;
   uint8_t kb_keys[SC_LAST];
   uint8_t flags;
   bool last_focus;
   bool kb_clear_pending;
   /* "Background Keyboard Input": key reports are taken while the
    * window is not the active one too (as a sink only); kb_taking is
    * whether they were at the last poll. */
   bool kb_background;
   bool kb_taking;

   /* Read in bulk by the poll: see "Read by the poll" below. All of
    * this is unused, and zero, unless that is switched on. */
   bool poll_drain;
   /* Registered as a sink: input arrives whether or not the
    * application is in the foreground, and the main window's focus
    * decides what is taken. */
   bool sink;
   bool window_failed;
   DWORD window_tid;   /* the thread that made the window */
   /* the "disable Windows keys" setting the keyboard is registered
    * with; registered again when the setting changes */
   bool nowinkey;
   /* A report came from a mouse that is not in the list: the list is
    * made again at the end of the poll (winraw_mice_refresh()). */
   bool   mouse_unknown_seen;
   HANDLE mouse_unknown;
   /* a handle that was still not a listed mouse after a refresh */
   HANDLE mouse_not_listed;
   DWORD  mouse_refresh_tick;
   /* The keyboards as raw input has them - anything that can send
    * keys: their handles, oldest first. All of them feed the one key
    * state above. */
   HANDLE   kbs[WINRAW_KB_RAW_MAX];
   unsigned kb_cnt;
   /* each one's own keys, beside the one key state they all feed */
   uint8_t  kb_down[WINRAW_KB_RAW_MAX][WINRAW_KB_BYTES];
   /* The keyboards as they are listed and numbered for the menu: the
    * raw input keyboards that are one device on the desk are one
    * keyboard here, and some are part of none (kb_grp[] says which;
    * see winraw_keyboards_list()). kg_down[] is each listed
    * keyboard's keys, whichever of its parts they came from: what a
    * port that was given the keyboard reads (winraw_port_keys()). */
   uint8_t  kb_grp[WINRAW_KB_RAW_MAX];
   unsigned kg_cnt;
   uint8_t  kg_down[WINRAW_KEYBOARDS_MAX][WINRAW_KB_BYTES];
   /* A key came from a keyboard that is not in the list: the list is
    * made again at the end of the poll. */
   bool     kb_unknown_seen;
   HANDLE   kb_unknown;
   /* a handle that was still not a listed keyboard after that */
   HANDLE   kb_not_listed;
   DWORD    kb_refresh_tick;
   /* Key events of the reports read so far, handed on at the end of
    * the poll. One thread's. */
   unsigned kev_n;
   uint32_t kev[WINRAW_KEV_SIZE];
   /* Counts, logged when the driver is freed. */
   unsigned long drained;        /* reports read in bulk */
   unsigned long drained_hid;    /* controllers' reports among the reads */
   unsigned long drain_reads;    /* in this many reads */
   unsigned long drain_empty;    /* reads that found nothing waiting */
   unsigned long by_message;     /* reports that came as a message */
   unsigned long kev_dropped;
   /* Taken by the window's thread: how old, at each poll, what that
    * thread had taken was (see winraw_pump_done()). */
   uint64_t      stale_sum;      /* microseconds, summed */
   uint32_t      stale_max;
   unsigned long stale_polls;
} winraw_input_t;

/* TODO/FIXME - static globals */
static winraw_mouse_t *g_mice        = NULL;
static bool winraw_focus             = false;

/* Sync internal mouse coordinates with the OS cursor position.
 * Used after events such as window mode, size, and focus changes */
static bool winraw_sync_mouse_to_cursor(winraw_input_t *wr)
{
   HWND wnd;
   POINT p;
   unsigned i;

   if (!wr || !wr->mouse_cnt)
      return false;

   wnd = (HWND)video_driver_window_get();
   if (!wnd || !GetCursorPos(&p))
      return false;

   ScreenToClient(wnd, &p);

   for (i = 0; i < wr->mouse_cnt; ++i)
   {
      g_mice[i].x = (LONG)p.x;
      g_mice[i].y = (LONG)p.y;
   }

   return true;
}

#define WINRAW_KEYBOARD_PRESSED(wr, key) (wr->kb_keys[rarch_keysym_lut[(enum retro_key)(key)]])

/* A key as a port sees it: on the one keyboard the port was given
 * (@own, from winraw_port_keys()), or with no such keyboard on any. */
#define WINRAW_PORT_KEY_PRESSED(wr, own, key) ((own) \
      ? winraw_kb_own_pressed((own), rarch_keysym_lut[(enum retro_key)(key)]) \
      : WINRAW_KEYBOARD_PRESSED(wr, key))

static HWND winraw_create_window(WNDPROC wnd_proc)
{
   HWND wnd;
   WNDCLASSA wc     = {0};
   if (!(wc.hInstance = GetModuleHandleA(NULL)))
      return NULL;
   wc.lpfnWndProc   = wnd_proc;
   wc.lpszClassName = "winraw-input";
   if (     !RegisterClassA(&wc)
         &&  GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
      return NULL;
   if (!(wnd = CreateWindowExA(0, wc.lpszClassName,
               NULL, 0, 0, 0, 0, 0,
               HWND_MESSAGE, NULL, NULL, NULL)))
   {
      UnregisterClassA(wc.lpszClassName, NULL);
      return NULL;
   }
   return wnd;
}

/* Deferred mouse display-name resolution.
 *
 * Resolving a friendly mouse name requires
 * GetRawInputDeviceInfoA(RIDI_DEVICENAME) followed by CreateFile() +
 * HidD_GetProductString() on the HID interface. Opening the HID
 * interface of e.g. a Bluetooth mouse right after a fresh boot can
 * block for seconds while the device stack is still coming up, and
 * the result is purely cosmetic (menu display names / log lines), so
 * this work runs on the task queue instead of the input driver init
 * path. Win32 queries happen in the task handler; publishing the
 * names to global input config state happens in the task callback,
 * which runs on the main thread (same split as the joypad
 * autoconfig task). */

/* ---- which device on the desk a raw input device is part of -------- */

/* What is known of one raw input device beyond its handle. */
typedef struct
{
   /* What tells one device on the desk from another: Windows' own id
    * for the physical device (its "container"), which every part of
    * a device shares; failing that its USB ids; "" if neither is
    * known. */
   char     key[48];
   char     desc[80];   /* what Windows calls it, as a name of last resort */
   uint16_t vid;
   uint16_t pid;
   /* Nothing on the desk is behind it: the terminal server's keyboard
    * or mouse, which every Windows has, or a device a program made -
    * a vendor's "virtual input device", a remapper's. Windows hangs
    * those off its ROOT (or SWD) enumerator, where no hardware is. */
   bool     remote;
   /* Whether its USB interface says "boot keyboard" - what a keyboard
    * has so that it works before an operating system is up, and what
    * the key-sending part of a mouse or a headset has no reason to
    * have. 1 yes, 0 no, -1 not known (not USB, or not found out). */
   int8_t   boot;
   /* and the same of "boot mouse", which a mouse has and the
    * pointer-sending part of a keyboard has no reason to */
   int8_t   boot_mouse;
} winraw_dev_ident_t;

static const char *winraw_stristr(const char *s, const char *sub)
{
   size_t n = strlen(sub);
   for (; *s; s++)
   {
      size_t i;
      for (i = 0; i < n; i++)
      {
         char a = s[i], b = sub[i];
         if (a >= 'a' && a <= 'z')
            a -= 'a' - 'A';
         if (b >= 'a' && b <= 'z')
            b -= 'a' - 'A';
         if (a != b)
            break;
      }
      if (i == n)
         return s;
   }
   return NULL;
}

/* the hex digits at @p, at most @max of them; *@digits says how many */
static unsigned winraw_hex(const char *p, unsigned max, unsigned *digits)
{
   unsigned v = 0, n = 0;
   for (; n < max; n++, p++)
   {
      if (*p >= '0' && *p <= '9')
         v = (v << 4) | (unsigned)(*p - '0');
      else if (*p >= 'a' && *p <= 'f')
         v = (v << 4) | (unsigned)(*p - 'a' + 10);
      else if (*p >= 'A' && *p <= 'F')
         v = (v << 4) | (unsigned)(*p - 'A' + 10);
      else
         break;
   }
   if (digits)
      *digits = n;
   return v;
}

/* From what Windows says of a device - its path, its container id,
 * the compatible ids of what it hangs off, that parent's description,
 * and the instance ids of its parent and its parent's parent joined
 * with ';', any of which may be "" - what the driver goes by. No
 * Windows call in here: samples/input/winraw_keyboards runs it on
 * strings of its own. */
static void winraw_dev_ident(const char *path, const char *container,
      const char *compat, const char *desc, const char *parents,
      winraw_dev_ident_t *id)
{
   const char *p;
   unsigned digits = 0;

   memset(id, 0, sizeof(*id));
   id->boot       = -1;
   id->boot_mouse = -1;
   id->remote     = winraw_stristr(path, "RDP_KBD") || winraw_stristr(path, "RDP_MOU");
   /* made by a program: it, or what it hangs off, is on the ROOT or
    * the SWD enumerator */
   if (     strlen(path) > 4
         && (  winraw_stristr(path + 4, "ROOT#") == path + 4
            || winraw_stristr(path + 4, "SWD#")  == path + 4))
      id->remote  = true;
   for (p = parents; p && *p; )
   {
      if (     winraw_stristr(p, "ROOT\\") == p
            || winraw_stristr(p, "SWD\\")  == p)
         id->remote = true;
      if (!(p = strchr(p, ';')))
         break;
      p++;
   }

   /* USB: VID_045E&PID_07A5. Bluetooth: VID&0002054c_PID&0df2, the
    * vendor id after four digits that say whose list it is from. */
   if ((p = winraw_stristr(path, "VID_")))
      id->vid = (uint16_t)winraw_hex(p + 4, 4, NULL);
   else if ((p = winraw_stristr(path, "VID&")))
   {
      unsigned v = winraw_hex(p + 4, 8, &digits);
      id->vid    = (uint16_t)v;
   }
   if ((p = winraw_stristr(path, "PID_")))
      id->pid = (uint16_t)winraw_hex(p + 4, 4, NULL);
   else if ((p = winraw_stristr(path, "PID&")))
      id->pid = (uint16_t)winraw_hex(p + 4, 4, NULL);

   /* Everything built into the machine shares one container, the
    * machine's own; that one tells nothing apart. */
   if (     container && *container
         && !winraw_stristr(container, "00000000-0000-0000-FFFF-FFFFFFFFFFFF"))
      strlcpy(id->key, container, sizeof(id->key));
   else if (id->vid || id->pid)
      snprintf(id->key, sizeof(id->key), "%04x:%04x", id->vid, id->pid);

   if (compat && winraw_stristr(compat, "USB\\Class_03"))
   {
      id->boot       = winraw_stristr(compat, "SubClass_01&Prot_01") ? 1 : 0;
      id->boot_mouse = winraw_stristr(compat, "SubClass_01&Prot_02") ? 1 : 0;
   }

   if (desc)
      strlcpy(id->desc, desc, sizeof(id->desc));
}

#ifndef WINRAW_DEVICE_STRINGS
#ifndef CM_DRP_BASE_CONTAINERID
#define CM_DRP_BASE_CONTAINERID 0x00000025
#endif

static void winraw_narrow(char *s, size_t len, const WCHAR *w)
{
   size_t i;
   for (i = 0; i + 1 < len && w[i]; i++)
      s[i] = (w[i] < 0x80) ? (char)w[i] : '?';
   s[i] = '\0';
}

/* What Windows says of a raw input device: its path; the container id
 * of the device on the desk it is part of; the compatible ids of what
 * it hangs off, joined with ';'; what Windows calls that parent; and
 * the instance ids of the parent and of its parent, joined with ';'.
 * Each is "" where Windows does not say. @path holds 256, @container
 * 48, @compat 256, @desc 80, @parents 256. */
static void winraw_device_strings(HANDLE hnd, char *path,
      char *container, char *compat, char *desc, char *parents)
{
   WCHAR id[256];
   WCHAR w[256];
   DEVINST inst, parent;
   ULONG len;
   unsigned i, n   = 0;
   UINT size       = 256;
   const char *end = NULL;
   UINT r;

   path[0] = container[0] = compat[0] = desc[0] = parents[0] = '\0';

   r = GetRawInputDeviceInfoA(hnd, RIDI_DEVICENAME, path, &size);
   if (r == (UINT)-1 || r == 0)
   {
      path[0] = '\0';
      return;
   }
   path[255] = '\0';

   /* The path is the device's instance id with '#' for '\', a prefix
    * before it and an interface class after it:
    *   \\?\HID#VID_0951&PID_16E5&MI_00#8&2d7f0f1&0&0000#{884b96c3-...} */
   if (strlen(path) < 5 || !(end = strrchr(path, '#')))
      return;
   for (i = 4; path + i < end && n + 1 < ARRAY_SIZE(id); i++)
      id[n++] = (path[i] == '#') ? L'\\' : (WCHAR)(unsigned char)path[i];
   id[n] = 0;

   if (CM_Locate_DevNodeW(&inst, id, CM_LOCATE_DEVNODE_NORMAL) != CR_SUCCESS)
      return;

   len = sizeof(w);
   memset(w, 0, sizeof(w));
   if (CM_Get_DevNode_Registry_PropertyW(inst, CM_DRP_BASE_CONTAINERID,
            NULL, w, &len, 0) == CR_SUCCESS)
      winraw_narrow(container, 48, w);

   if (CM_Get_Parent(&parent, inst, 0) == CR_SUCCESS)
   {
      len = sizeof(w) - 2 * sizeof(WCHAR);
      memset(w, 0, sizeof(w));
      if (CM_Get_DevNode_Registry_PropertyW(parent, CM_DRP_COMPATIBLEIDS,
               NULL, w, &len, 0) == CR_SUCCESS)
      {
         /* a list of strings, each ended by a 0, the list by another */
         for (i = 0; i + 1 < ARRAY_SIZE(w) && (w[i] || w[i + 1]); i++)
            if (!w[i])
               w[i] = L';';
         winraw_narrow(compat, 256, w);
      }
      len = sizeof(w);
      memset(w, 0, sizeof(w));
      if (CM_Get_DevNode_Registry_PropertyW(parent, CM_DRP_DEVICEDESC,
               NULL, w, &len, 0) == CR_SUCCESS)
         winraw_narrow(desc, 80, w);

      /* where it hangs: its parent, and that one's */
      memset(w, 0, sizeof(w));
      if (CM_Get_Device_IDW(parent, w, ARRAY_SIZE(w) - 1, 0) == CR_SUCCESS)
      {
         DEVINST grand;
         size_t at;
         winraw_narrow(parents, 120, w);
         at = strlen(parents);
         memset(w, 0, sizeof(w));
         if (     CM_Get_Parent(&grand, parent, 0) == CR_SUCCESS
               && CM_Get_Device_IDW(grand, w, ARRAY_SIZE(w) - 1, 0) == CR_SUCCESS)
         {
            parents[at++] = ';';
            winraw_narrow(parents + at, 120, w);
         }
      }
   }
   if (!desc[0])
   {
      len = sizeof(w);
      memset(w, 0, sizeof(w));
      if (CM_Get_DevNode_Registry_PropertyW(inst, CM_DRP_DEVICEDESC,
               NULL, w, &len, 0) == CR_SUCCESS)
         winraw_narrow(desc, 80, w);
   }
}
#define WINRAW_DEVICE_STRINGS winraw_device_strings
#endif

typedef struct
{
   HANDLE hnd;      /* raw input device handle; used for queries only */
   char name[256];
   /* the name to go by if the device gives none, and its USB ids for
    * the menu */
   char fallback[80];
   uint16_t vid;
   uint16_t pid;
   /* mice: which device on the desk it is part of, and whether it is
    * one to leave out of the list */
   char device[48];
   bool remote;
} winraw_mouse_name_entry_t;

/* a listed keyboard, as the names task is asked about it */
typedef struct
{
   HANDLE hnd;
   char fallback[80];
   uint16_t vid;
   uint16_t pid;
} winraw_kb_name_req_t;

typedef struct
{
   winraw_mouse_name_entry_t *entries;
   unsigned count;
   bool keyboards; /* the names are keyboards', not mice's */
} winraw_mouse_names_handle_t;

static void winraw_mouse_names_free(retro_task_t *task)
{
   winraw_mouse_names_handle_t *h = NULL;
   if (!task)
      return;
   if ((h = (winraw_mouse_names_handle_t*)task->state))
   {
      free(h->entries);
      free(h);
   }
   task->state = NULL;
}

static void winraw_mouse_names_handler(retro_task_t *task)
{
   unsigned i;
   winraw_mouse_names_handle_t *h = NULL;

   if (!task)
      return;

   if ((h = (winraw_mouse_names_handle_t*)task->state))
   {
      for (i = 0; i < h->count; ++i)
      {
         char *name     = h->entries[i].name;
         /* Reset the in/out size argument every iteration -
          * GetRawInputDeviceInfoA() may modify it. */
         UINT name_size = sizeof(h->entries[i].name);
         UINT r         = GetRawInputDeviceInfoA(h->entries[i].hnd,
               RIDI_DEVICENAME, name, &name_size);
         if (r == (UINT)-1 || r == 0)
            name[0] = '\0';

         if (name[0])
         {
            HANDLE hhid = CreateFile(name,
                  0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

            if (hhid != INVALID_HANDLE_VALUE)
            {
               wchar_t prod_buf[128];
               prod_buf[0] = '\0';
               if (HidD_GetProductString(hhid, prod_buf, sizeof(prod_buf)))
                  wcstombs(name, prod_buf, sizeof(h->entries[i].name));
               /* a device goes by its product name or by what
                * Windows calls it, not by its path */
               else if (h->entries[i].fallback[0])
                  name[0] = '\0';
               CloseHandle(hhid);
            }
            else if (h->entries[i].fallback[0])
               name[0] = '\0';
         }

         if (!name[0] && h->entries[i].fallback[0])
            strlcpy(name, h->entries[i].fallback,
                  sizeof(h->entries[i].name));
         if (!name[0])
            strlcpy_lit(name, "<name not found>",
                  sizeof(h->entries[i].name));
      }
   }

   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void winraw_mouse_names_cb(retro_task_t *task,
      void *task_data, void *user_data, const char *err)
{
   unsigned i;
   winraw_mouse_names_handle_t *h = NULL;

   if (!task)
      return;
   if (!(h = (winraw_mouse_names_handle_t*)task->state))
      return;

   /* input_config_set_mouse_display_name() writes global input
    * config state, so it must run here on the main thread. */
   if (h->keyboards)
   {
      /* the list as it is now, whole: a keyboard that has gone is
       * not left in it */
      input_config_clear_keyboard_display_names();
      for (i = 0; i < h->count; ++i)
      {
         input_config_set_keyboard_display_name(i, h->entries[i].name);
         input_config_set_keyboard_ids(i,
               h->entries[i].vid, h->entries[i].pid);
         RARCH_LOG("[WinRaw] Found keyboard #%u: \"%s\".\n",
               i + 1, h->entries[i].name);
      }
      return;
   }
   /* the list as it is now, whole: a mouse that has gone is not left
    * in it */
   input_config_clear_mouse_info();
   for (i = 0; i < h->count; ++i)
   {
      input_config_set_mouse_display_name(i, h->entries[i].name);
      /* for Input Information: which of these are one mouse on the
       * desk, and which has nothing behind it */
      input_config_set_mouse_device(i, h->entries[i].device,
            h->entries[i].vid, h->entries[i].pid, h->entries[i].remote);
      RARCH_LOG("[WinRaw] Found mouse #%u: \"%s\".\n",
            i + 1, h->entries[i].name);
   }
}

/* The names of mice (from @mice) or, with @mice NULL, of keyboards
 * (from @kbs), looked up off the main thread and set on it. */
static void winraw_push_names_task(
      winraw_mouse_t *mice, const winraw_dev_ident_t *mouse_ids,
      const bool *mouse_unlisted,
      const winraw_kb_name_req_t *kbs, unsigned mouse_cnt)
{
   unsigned i;
   retro_task_t *task             = NULL;
   winraw_mouse_names_handle_t *h = NULL;

   if (!mouse_cnt)
   {
      /* none is a list too: what was listed goes */
      if (!mice)
         input_config_clear_keyboard_display_names();
      else
         input_config_clear_mouse_info();
      return;
   }

   if (!(h = (winraw_mouse_names_handle_t*)calloc(1, sizeof(*h))))
      return;
   if (!(h->entries = (winraw_mouse_name_entry_t*)calloc(
         mouse_cnt, sizeof(*h->entries))))
   {
      free(h);
      return;
   }
   h->count     = mouse_cnt;
   h->keyboards = (mice == NULL);
   for (i = 0; i < mouse_cnt; ++i)
   {
      if (mice)
      {
         /* what Windows says of it, for its name of last resort and
          * for telling which mice are one mouse on the desk */
         h->entries[i].hnd    = mice[i].hnd;
         h->entries[i].vid    = mouse_ids[i].vid;
         h->entries[i].pid    = mouse_ids[i].pid;
         h->entries[i].remote = mouse_unlisted[i];
         strlcpy(h->entries[i].device, mouse_ids[i].key,
               sizeof(h->entries[i].device));
         strlcpy(h->entries[i].fallback, mouse_ids[i].desc,
               sizeof(h->entries[i].fallback));
      }
      else
      {
         h->entries[i].hnd = kbs[i].hnd;
         h->entries[i].vid = kbs[i].vid;
         h->entries[i].pid = kbs[i].pid;
         strlcpy(h->entries[i].fallback, kbs[i].fallback,
               sizeof(h->entries[i].fallback));
      }
   }

   if (!(task = task_init()))
   {
      free(h->entries);
      free(h);
      return;
   }

   task->handler  = winraw_mouse_names_handler;
   task->state    = h;
   task->title    = NULL;
   task->callback = winraw_mouse_names_cb;
   task->cleanup  = winraw_mouse_names_free;
   task->flags   |= RETRO_TASK_FLG_MUTE;

   task_queue_push(task);
}

/* The mice, numbered as a port is given one (Mouse Index).
 *
 * What raw input calls a mouse is anything that can send pointer
 * events: a keyboard whose macros can move the pointer is one, a
 * device a program made is one, the terminal server has one. They
 * were numbered as raw input lists them, oldest first, so that on a
 * desk with a keyboard and a mouse "mouse 1" - the one the first port
 * reads unless told otherwise - could be the keyboard.
 *
 * The mice on the desk now come first, in the order they had; then
 * the rest, which can still be chosen by their number and are left
 * out of the menu's Input Information:
 *
 * - one nothing on the desk is behind;
 * - one that is part of a device with a USB boot keyboard on it and
 *   no boot mouse: a keyboard that can send pointer events. Where it
 *   is not known what a part is, it is taken for a mouse. */
static bool winraw_init_devices(winraw_mouse_t **mice, unsigned *mouse_cnt)
{
   UINT i;
   POINT crs_pos;
   unsigned k, n;
   winraw_dev_ident_t kb;
   char path[256], container[48], compat[256], desc[80], parents[256];
   winraw_dev_ident_t *ids  = NULL;
   bool *unlisted           = NULL;
   winraw_mouse_t *mice_r   = NULL;
   unsigned mouse_cnt_r     = 0;
   RAWINPUTDEVICELIST *devs = NULL;
   UINT dev_cnt             = 0;
   UINT r                   = GetRawInputDeviceList(
         NULL, &dev_cnt, sizeof(RAWINPUTDEVICELIST));

   if (r == (UINT)-1)
      goto error;

   if (!(devs = (RAWINPUTDEVICELIST*)malloc(
         dev_cnt * sizeof(RAWINPUTDEVICELIST))))
      goto error;

   if ((dev_cnt = GetRawInputDeviceList(devs,
         &dev_cnt, sizeof(RAWINPUTDEVICELIST))) == (UINT)-1)
      goto error;

   for (i = 0; i < dev_cnt; ++i)
      mouse_cnt_r += devs[i].dwType == RIM_TYPEMOUSE ? 1 : 0;

   if (mouse_cnt_r)
   {
      if (!(mice_r = (winraw_mouse_t*)calloc(
            1, mouse_cnt_r * sizeof(winraw_mouse_t))))
         goto error;

      if (!GetCursorPos(&crs_pos))
         goto error;

      for (i = 0; i < mouse_cnt_r; ++i)
      {
         mice_r[i].x = crs_pos.x;
         mice_r[i].y = crs_pos.y;
      }
   }

   *mouse_cnt = mouse_cnt_r;

   /* count is already checked, so this is safe */
   for (i = mouse_cnt_r = 0; i < dev_cnt; ++i)
   {
      if (devs[i].dwType == RIM_TYPEMOUSE)
      {
         mouse_cnt_r++;
         mice_r[*mouse_cnt - mouse_cnt_r].hnd = devs[i].hDevice;
      }
   }

   if (mouse_cnt_r)
   {
      if (     !(ids      = (winraw_dev_ident_t*)calloc(mouse_cnt_r, sizeof(*ids)))
            || !(unlisted = (bool*)calloc(mouse_cnt_r, sizeof(*unlisted))))
         goto error;

      for (k = 0; k < mouse_cnt_r; k++)
      {
         WINRAW_DEVICE_STRINGS(mice_r[k].hnd, path, container, compat,
               desc, parents);
         winraw_dev_ident(path, container, compat, desc, parents, &ids[k]);
         RARCH_DBG("[WinRaw] Raw mouse: \"%s\", device \"%s\", ids %04x:%04x,"
               " boot mouse: %s, made by a program: %s, \"%s\".\n",
               path, ids[k].key, ids[k].vid, ids[k].pid,
               ids[k].boot_mouse > 0 ? "yes"
               : ids[k].boot_mouse == 0 ? "no" : "not known",
               ids[k].remote ? "yes" : "no", ids[k].desc);
      }

      /* which of them are mice on the desk
       * (input_keyboard_devices.h): told of the mice, in the order
       * they are in, and then of the keyboards */
      {
         unsigned total     = mouse_cnt_r;
         input_kbdev_t *all = (input_kbdev_t*)calloc(
               mouse_cnt_r + dev_cnt, sizeof(*all));
         if (!all)
            goto error;
         for (k = 0; k < mouse_cnt_r; k++)
         {
            strlcpy(all[k].key, ids[k].key, sizeof(all[k].key));
            all[k].pointer    = true;
            all[k].remote     = ids[k].remote;
            all[k].boot       = -1;
            all[k].boot_mouse = ids[k].boot_mouse;
         }
         for (i = 0; i < dev_cnt; i++)
         {
            if (devs[i].dwType != RIM_TYPEKEYBOARD)
               continue;
            WINRAW_DEVICE_STRINGS(devs[i].hDevice, path, container, compat,
                  desc, parents);
            winraw_dev_ident(path, container, compat, desc, parents, &kb);
            strlcpy(all[total].key, kb.key, sizeof(all[total].key));
            all[total].keyboard   = true;
            all[total].remote     = kb.remote;
            all[total].boot       = kb.boot;
            all[total].boot_mouse = -1;
            total++;
         }
         input_kbdev_mice(all, total);
         for (k = 0; k < mouse_cnt_r; k++)
         {
            unlisted[k] = !all[k].mouse;
            if (unlisted[k] && !ids[k].remote)
               RARCH_LOG("[WinRaw] Not counted among the mice: \"%s\""
                     " (%04x:%04x), a keyboard that can send pointer"
                     " events.\n", ids[k].desc, ids[k].vid, ids[k].pid);
         }
         free(all);
      }

      /* the mice first, each kind in the order it had */
      for (n = 0, k = 0; k < mouse_cnt_r; k++)
      {
         unsigned j;
         winraw_mouse_t     m;
         winraw_dev_ident_t id;
         if (unlisted[k])
            continue;
         m  = mice_r[k];
         id = ids[k];
         for (j = k; j > n; j--)
         {
            mice_r[j]   = mice_r[j - 1];
            ids[j]      = ids[j - 1];
            unlisted[j] = unlisted[j - 1];
         }
         mice_r[n]   = m;
         ids[n]      = id;
         unlisted[n] = false;
         n++;
      }
   }

   *mice      = mice_r;

   /* what a port is pinned to each by, in the order they are in now:
    * its USB ids, or with none what Windows calls it */
   {
      char (*pins)[64] = (char (*)[64])calloc(
            mouse_cnt_r ? mouse_cnt_r : 1, sizeof(*pins));
      if (pins)
      {
         unsigned k;
         for (k = 0; k < mouse_cnt_r && ids; k++)
         {
            if (ids[k].vid || ids[k].pid)
               snprintf(pins[k], sizeof(pins[k]), "%04x:%04x",
                     ids[k].vid, ids[k].pid);
            else
               strlcpy(pins[k], ids[k].desc, sizeof(pins[k]));
         }
         input_mouse_pins_set_devices((const char (*)[64])pins, mouse_cnt_r);
         free(pins);
      }
   }

   winraw_push_names_task(mice_r, ids, unlisted, NULL, mouse_cnt_r);
   free(ids);
   free(unlisted);
   free(devs);

   return true;

error:
   free(ids);
   free(unlisted);
   free(devs);
   free(mice_r);
   *mice      = NULL;
   *mouse_cnt = 0;
   return false;
}

/* scancode in the low 16 bits, down in bit 16, modifiers above */
#define WINRAW_KEV_PACK(mcode, down, mod) \
   ((uint32_t)(mcode) | ((uint32_t)((down) ? 1 : 0) << 16) | ((uint32_t)(mod) << 17))

static uint16_t winraw_held_mods(const winraw_input_t *wr);

/* Set when Windows has said devices came or went (the hotplug timer,
 * on the window's thread); the next poll makes the keyboard list
 * again. That is how a keyboard that was unplugged leaves the list:
 * it sends no last key to notice it by. */
static retro_atomic_int_t winraw_devices_changed;

/* Which keyboards there are.
 *
 * What raw input calls a keyboard is anything that can send keys. One
 * keyboard on the desk is often two or three of those - its media
 * keys, its extra-key interface - a mouse or a headset with buttons
 * that send keys is one too, and every Windows has the terminal
 * server's, with nothing behind it. Listed as raw input has them, a
 * desk with one keyboard and one mouse showed four keyboards, and a
 * port given "a keyboard" got one part of one.
 *
 * So the raw input keyboards are kept, oldest first as the mice are
 * (kbs[]), and from them the keyboards that are listed and numbered
 * for the menu are made:
 *
 * (which, input/common/input_keyboard_devices.h says, as it does for
 * the udev driver; this driver tells it what Windows says of each
 * device)
 *
 * - the raw input keyboards that are parts of one device on the desk
 *   are one keyboard. Windows gives every part of a physical device
 *   the same container id; where it gives none, the same USB ids are
 *   taken for the same device;
 * - a keyboard nothing on the desk is behind is not listed: the
 *   terminal server's, or one a program made (a vendor's "virtual
 *   input device");
 * - a device that is also a mouse, and none of whose keyboard parts
 *   is a USB boot keyboard, is not listed: that is a mouse whose
 *   buttons can send keys. Where it is not known whether a part is a
 *   boot keyboard - it is not USB, or Windows did not say - the
 *   device is listed.
 *
 * Every raw input keyboard still feeds the one key state, listed or
 * not. The listed keyboards' names are asked for here and reach the
 * menu's Information > Input Information when the task is done; a
 * device that gives no name goes by what Windows calls it. */
static void winraw_keyboards_list(winraw_input_t *wr)
{
   UINT i;
   unsigned k, o, g, bit;
   unsigned n               = 0, total = 0;
   RAWINPUTDEVICELIST *devs = NULL;
   UINT dev_cnt             = 0;
   /* the list as it was, for what carries over and what does not */
   unsigned old_cnt         = wr->kb_cnt;
   HANDLE   old_kbs[WINRAW_KB_RAW_MAX];
   uint8_t  old_down[WINRAW_KB_RAW_MAX][WINRAW_KB_BYTES];
   winraw_dev_ident_t ident[WINRAW_KB_RAW_MAX];
   input_kbdev_t devices[WINRAW_KB_RAW_MAX * 2];
   char     pins[WINRAW_KEYBOARDS_MAX][64];
   winraw_kb_name_req_t req[WINRAW_KEYBOARDS_MAX];
   char path[256], container[48], compat[256], desc[80], parents[256];

   memset(pins, 0, sizeof(pins));

   memcpy(old_kbs,  wr->kbs,     sizeof(old_kbs));
   memcpy(old_down, wr->kb_down, sizeof(old_down));
   memset(wr->kb_down, 0, sizeof(wr->kb_down));
   memset(wr->kg_down, 0, sizeof(wr->kg_down));
   memset(wr->kb_grp, WINRAW_KB_NONE, sizeof(wr->kb_grp));
   memset(req, 0, sizeof(req));

   wr->kb_cnt = 0;
   wr->kg_cnt = 0;

   if (GetRawInputDeviceList(NULL, &dev_cnt,
            sizeof(RAWINPUTDEVICELIST)) == (UINT)-1 || !dev_cnt)
      goto done;
   if (!(devs = (RAWINPUTDEVICELIST*)malloc(
         dev_cnt * sizeof(RAWINPUTDEVICELIST))))
      goto done;
   if ((dev_cnt = GetRawInputDeviceList(devs,
         &dev_cnt, sizeof(RAWINPUTDEVICELIST))) == (UINT)-1)
      goto done;

   for (i = 0; i < dev_cnt; i++)
      if (devs[i].dwType == RIM_TYPEKEYBOARD)
         total++;
   if (total > WINRAW_KB_RAW_MAX)
      total = WINRAW_KB_RAW_MAX;
   /* the list has the newest first */
   for (i = 0; i < dev_cnt && n < total; i++)
      if (devs[i].dwType == RIM_TYPEKEYBOARD)
         wr->kbs[total - ++n] = devs[i].hDevice;
   wr->kb_cnt = total;

   /* What each is, for input_keyboard_devices.h to say which are the
    * keyboards on the desk: the raw input keyboards first, in the
    * order they are kept in, then the mice. */
   for (k = 0; k < wr->kb_cnt; k++)
   {
      WINRAW_DEVICE_STRINGS(wr->kbs[k], path, container, compat,
            desc, parents);
      winraw_dev_ident(path, container, compat, desc, parents, &ident[k]);
      RARCH_DBG("[WinRaw] Raw keyboard: \"%s\", device \"%s\", ids %04x:%04x,"
            " boot keyboard: %s, \"%s\".\n",
            path, ident[k].key, ident[k].vid, ident[k].pid,
            ident[k].boot > 0 ? "yes" : ident[k].boot == 0 ? "no" : "not known",
            ident[k].desc);
      memset(&devices[k], 0, sizeof(devices[k]));
      strlcpy(devices[k].key, ident[k].key, sizeof(devices[k].key));
      devices[k].keyboard   = true;
      devices[k].remote     = ident[k].remote;
      devices[k].boot       = ident[k].boot;
      devices[k].boot_mouse = -1;
   }
   n = wr->kb_cnt;
   for (i = 0; i < dev_cnt && n < ARRAY_SIZE(devices); i++)
   {
      winraw_dev_ident_t m;
      if (devs[i].dwType != RIM_TYPEMOUSE)
         continue;
      WINRAW_DEVICE_STRINGS(devs[i].hDevice, path, container, compat,
            desc, parents);
      winraw_dev_ident(path, container, compat, desc, parents, &m);
      memset(&devices[n], 0, sizeof(devices[n]));
      strlcpy(devices[n].key, m.key, sizeof(devices[n].key));
      devices[n].pointer    = true;
      devices[n].remote     = m.remote;
      devices[n].boot       = -1;
      devices[n].boot_mouse = m.boot_mouse;
      n++;
   }

   wr->kg_cnt = input_kbdev_group(devices, n, WINRAW_KEYBOARDS_MAX);

   for (k = 0; k < wr->kb_cnt; k++)
   {
      g             = devices[k].group;
      wr->kb_grp[k] = (uint8_t)g;
      if (g == INPUT_KBDEV_NONE)
      {
         /* said once of a device, not of each of its parts */
         for (o = 0; o < k; o++)
            if (     devices[o].group == INPUT_KBDEV_NONE
                  && ident[k].key[0]
                  && string_is_equal(ident[k].key, ident[o].key))
               break;
         if (o < k)
            continue;
         if (ident[k].remote)
            RARCH_LOG("[WinRaw] Not listed as a keyboard: \"%s\", which"
                  " nothing on the desk is behind.\n",
                  ident[k].desc[0] ? ident[k].desc : "a device");
         else
            RARCH_LOG("[WinRaw] Not listed as a keyboard: \"%s\" (%04x:%04x),"
                  " a mouse that can send keys.\n",
                  ident[k].desc, ident[k].vid, ident[k].pid);
         continue;
      }
      /* a listed keyboard's name is asked of the oldest of its parts */
      if (g < WINRAW_KEYBOARDS_MAX && !req[g].hnd)
      {
         req[g].hnd = wr->kbs[k];
         req[g].vid = ident[k].vid;
         req[g].pid = ident[k].pid;
         strlcpy(req[g].fallback, ident[k].desc, sizeof(req[g].fallback));
         /* and what a port is pinned to it by: its USB ids, or with
          * none what Windows calls it */
         if (ident[k].vid || ident[k].pid)
            snprintf(pins[g], sizeof(pins[g]), "%04x:%04x",
                  ident[k].vid, ident[k].pid);
         else
            strlcpy(pins[g], ident[k].desc, sizeof(pins[g]));
      }
   }

done:
   free(devs);
   /* the ports' keyboards are looked up in the new list */
   input_keyboard_pins_set_devices((const char (*)[64])pins, wr->kg_cnt);

   /* A raw input keyboard that is still there keeps its keys, and the
    * listed keyboards' keys are made again from their parts'. One
    * that has gone sent no key-up for what was held on it: each such
    * key that no keyboard still here holds is let go, in the one key
    * state and for whoever follows key events - or it would stay
    * down. */
   for (o = 0; o < old_cnt; o++)
   {
      for (k = 0; k < wr->kb_cnt; k++)
         if (wr->kbs[k] == old_kbs[o])
            break;
      if (k < wr->kb_cnt)
         memcpy(wr->kb_down[k], old_down[o], WINRAW_KB_BYTES);
   }
   for (k = 0; k < wr->kb_cnt; k++)
   {
      unsigned b;
      if (wr->kb_grp[k] == WINRAW_KB_NONE)
         continue;
      for (b = 0; b < WINRAW_KB_BYTES; b++)
         wr->kg_down[wr->kb_grp[k]][b] |= wr->kb_down[k][b];
   }
   for (o = 0; o < old_cnt; o++)
   {
      for (k = 0; k < wr->kb_cnt; k++)
         if (wr->kbs[k] == old_kbs[o])
            break;
      if (k < wr->kb_cnt)
         continue;
      for (bit = 0; bit < WINRAW_KB_BITS; bit++)
      {
         unsigned mcode;
         if (!(old_down[o][bit >> 3] & (1 << (bit & 7))))
            continue;
         for (k = 0; k < wr->kb_cnt; k++)
            if (wr->kb_down[k][bit >> 3] & (1 << (bit & 7)))
               break;
         if (k < wr->kb_cnt)
            continue;
         mcode = winraw_kb_mcode(bit);
         if (!wr->kb_keys[mcode])
            continue;
         wr->kb_keys[mcode] = 0;
         if (wr->poll_drain && wr->kev_n < WINRAW_KEV_SIZE)
            wr->kev[wr->kev_n++] =
               WINRAW_KEV_PACK(mcode, 0, winraw_held_mods(wr));
         else if (!wr->poll_drain)
            input_keyboard_event(0,
                  input_keymaps_translate_keysym_to_rk(mcode),
                  0, win32_get_keyboard_mods(), RETRO_DEVICE_KEYBOARD);
      }
   }

   winraw_push_names_task(NULL, NULL, NULL, req, wr->kg_cnt);
}

/* Whether a key is down on one keyboard's own state. */
static INLINE bool winraw_kb_own_pressed(const uint8_t *own, unsigned mcode)
{
   unsigned bit = winraw_kb_bit(mcode);
   return bit < WINRAW_KB_BITS && (own[bit >> 3] & (1 << (bit & 7)));
}

/* The keyboard a port reads its key binds and its keyboard from: the
 * one the port's Keyboard Index names, or NULL for all of them as one
 * - the setting's default, and what a port falls back to while the
 * keyboard it names is not there and no other port has its own (the
 * frontend says which, from the keyboard each port is pinned to:
 * input_keyboard_port_choice()). NULL as well while the menu is
 * open: the menu is worked with the first port's binds and has to
 * answer to every keyboard. Hotkeys never go through this. */
static const uint8_t *winraw_port_keys(const winraw_input_t *wr,
      unsigned port)
{
   /* no key is down on this one: what a port reads whose own keyboard
    * is away while another port has its own */
   static const uint8_t none[WINRAW_KB_BYTES];
   int idx = input_keyboard_port_choice(port);
   if (!idx)
      return NULL;
#ifdef HAVE_MENU
   if (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE)
      return NULL;
#endif
   if (idx < 0 || (unsigned)idx > wr->kg_cnt)
      return none;
   return wr->kg_down[idx - 1];
}

/* A key came from a keyboard the list does not have - one plugged in
 * since the list was made: the list is made again, at most once a
 * second. Called at the end of the poll. */
static void winraw_keyboards_refresh(winraw_input_t *wr)
{
   unsigned i;
   bool found = false;
   DWORD now  = GetTickCount();

   wr->kb_unknown_seen = false;
   if (wr->kb_refresh_tick && now - wr->kb_refresh_tick < 1000)
      return;
   wr->kb_refresh_tick = now ? now : 1;

   winraw_keyboards_list(wr);
   for (i = 0; i < wr->kb_cnt; i++)
      if (wr->kbs[i] == wr->kb_unknown)
         found = true;
   /* not a listed keyboard even now: not asked about again */
   if (!found)
      wr->kb_not_listed = wr->kb_unknown;
   RARCH_LOG("[WinRaw] Keyboard list made again: %u.\n", wr->kg_cnt);
}

static int16_t winraw_lightgun_aiming_state(winraw_input_t *wr,
      winraw_mouse_t *mouse,
      unsigned port, unsigned id)
{
   struct video_viewport vp = {0};
   int16_t res_x         = 0;
   int16_t res_y         = 0;
   int16_t res_screen_x  = 0;
   int16_t res_screen_y  = 0;

   if ((video_driver_translate_coord_viewport_wrap(
               &vp, mouse->x, mouse->y,
               &res_x, &res_y, &res_screen_x, &res_screen_y)))
   {
      switch (id)
      {
         case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
            return res_x;
         case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
            return res_y;
         case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
            return input_driver_pointer_is_offscreen(res_x, res_y);
         default:
            break;
      }
   }

   return 0;
}

static bool winraw_mouse_button_pressed(
      winraw_input_t *wr,
      winraw_mouse_t *mouse,
      unsigned port, unsigned key)
{
   switch (key)
   {
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         return ((mouse->flags & WRAW_MOUSE_FLG_BTN_L) > 0);
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         return ((mouse->flags & WRAW_MOUSE_FLG_BTN_R) > 0);
      case RETRO_DEVICE_ID_MOUSE_MIDDLE:
         return ((mouse->flags & WRAW_MOUSE_FLG_BTN_M) > 0);
      case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
         return ((mouse->flags & WRAW_MOUSE_FLG_BTN_B4) > 0);
      case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
         return ((mouse->flags & WRAW_MOUSE_FLG_BTN_B5) > 0);
      case RETRO_DEVICE_ID_MOUSE_WHEELUP:
         return mouse->whl_u;
      case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
         return mouse->whl_d;
   }

   return false;
}

static void winraw_init_mouse_xy_mapping(winraw_input_t *wr)
{
   struct video_viewport viewport;
   int mouse_x;
   int mouse_y;
   unsigned i;

   if (!video_driver_get_viewport_info(&viewport))
      return;

   /* Default fallback: center of the viewport */
   mouse_x = VIDEO_POS_X(viewport.pos) + VIDEO_SCALE_W(viewport.dims)  / 2;
   mouse_y = VIDEO_POS_Y(viewport.pos) + VIDEO_SCALE_H(viewport.dims) / 2;

   /* Sync to OS cursor position; fall back to center if it fails */
   if (!winraw_sync_mouse_to_cursor(wr))
   {
      for (i = 0; i < wr->mouse_cnt; i++)
      {
         g_mice[i].x      = mouse_x;
         g_mice[i].y      = mouse_y;
      }
   }

   wr->view_abs_ratio_x   = (double)VIDEO_SCALE_W(viewport.full_dims)  / 65535.0;
   wr->view_abs_ratio_y   = (double)VIDEO_SCALE_H(viewport.full_dims) / 65535.0;

   wr->flags             |= WRAW_INP_FLG_MOUSE_XY_MAPPING_READY;
}

static void winraw_update_mouse_state(winraw_input_t *wr,
      winraw_mouse_t *mouse, RAWMOUSE *state)
{
   bool swap_mouse_buttons = (g_win32_flags & WIN32_CMN_FLAG_SWAP_MOUSE_BTNS) ? true : false;

   if (state->usFlags & MOUSE_MOVE_ABSOLUTE)
   {
      if ((wr->flags & WRAW_INP_FLG_MOUSE_XY_MAPPING_READY) > 0)
      {
         state->lLastX = (LONG)(wr->view_abs_ratio_x * state->lLastX);
         state->lLastY = (LONG)(wr->view_abs_ratio_y * state->lLastY);
         /* abs_ref_* is this thread's own copy of the last absolute
          * position, so the delta can be derived without reading the
          * position that winraw_poll() owns. */
         retro_atomic_fetch_add_int(&mouse->dlt_x,
               state->lLastX - mouse->abs_ref_x);
         retro_atomic_fetch_add_int(&mouse->dlt_y,
               state->lLastY - mouse->abs_ref_y);
         mouse->abs_ref_x = state->lLastX;
         mouse->abs_ref_y = state->lLastY;
         retro_atomic_store_release_int(&mouse->abs_pos,
               (int)VIDEO_POS_PACK(state->lLastX, state->lLastY));
         retro_atomic_store_release_int(&mouse->abs_pending, 1);
      }
      else
         retro_atomic_store_release_int(&wr->map_pending, 1);
   }
   else if (state->lLastX || state->lLastY)
   {
      /* Menu and pointer require GetCursorPos() for
       * positioning, but using that always will
       * break multiple mice positions */
      bool getcursorpos = (mouse->device == RETRO_DEVICE_POINTER) ? true : false;
#ifdef HAVE_MENU
      if (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE)
         getcursorpos = true;
#endif
      /* Input overlay with mouse cursor must also use GetCursorPos() */
      if (!getcursorpos)
      {
         settings_t *settings = config_get_ptr();
         if (     settings->bools.input_overlay_enable
               && *settings->paths.path_overlay)
            getcursorpos = true;
      }

      if (getcursorpos)
      {
         retro_atomic_fetch_add_int(&mouse->dlt_x, state->lLastX);
         retro_atomic_fetch_add_int(&mouse->dlt_y, state->lLastY);

         /* Defer the cursor query to winraw_poll(). GetCursorPos() is an
          * unconditional kernel transition on every Windows version, and
          * this runs once per raw input report - about sixteen times per
          * frame with a 1000 Hz mouse at 60 fps. Only the value in place
          * at the frame snapshot is ever read, so resolving it once per
          * frame gives the same result from a fresher sample. */
         retro_atomic_store_release_int(&mouse->pos_pending, 1);
      }
      else
      {
         /* This branch moves by delta, so any deferred cursor query or
          * absolute report from earlier in this frame is superseded. */
         retro_atomic_store_release_int(&mouse->pos_pending, 0);
         retro_atomic_store_release_int(&mouse->abs_pending, 0);

         /* Handle different sensitivity for lightguns */
         if (mouse->device == RETRO_DEVICE_LIGHTGUN)
         {
            retro_atomic_store_release_int(&mouse->dlt_x, state->lLastX);
            retro_atomic_store_release_int(&mouse->dlt_y, state->lLastY);
         }
         else
         {
            retro_atomic_fetch_add_int(&mouse->dlt_x, state->lLastX);
            retro_atomic_fetch_add_int(&mouse->dlt_y, state->lLastY);
         }
      }
   }

   if (swap_mouse_buttons)
   {
      if (state->usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
         mouse->flags |=  (WRAW_MOUSE_FLG_BTN_R);
      else if (state->usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
         mouse->flags &= ~(WRAW_MOUSE_FLG_BTN_R);

      if (state->usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
         mouse->flags |=  (WRAW_MOUSE_FLG_BTN_L);
      else if (state->usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
         mouse->flags &= ~(WRAW_MOUSE_FLG_BTN_L);
   }
   else
   {
      if (state->usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
         mouse->flags |=  (WRAW_MOUSE_FLG_BTN_L);
      else if (state->usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
         mouse->flags &= ~(WRAW_MOUSE_FLG_BTN_L);

      if (state->usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
         mouse->flags |=  (WRAW_MOUSE_FLG_BTN_R);
      else if (state->usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
         mouse->flags &= ~(WRAW_MOUSE_FLG_BTN_R);
   }

   if (state->usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
      mouse->flags |=  (WRAW_MOUSE_FLG_BTN_M);
   else if (state->usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)
      mouse->flags &= ~(WRAW_MOUSE_FLG_BTN_M);

   if (state->usButtonFlags & RI_MOUSE_BUTTON_4_DOWN)
      mouse->flags |=  (WRAW_MOUSE_FLG_BTN_B4);
   else if (state->usButtonFlags & RI_MOUSE_BUTTON_4_UP)
      mouse->flags &= ~(WRAW_MOUSE_FLG_BTN_B4);

   if (state->usButtonFlags & RI_MOUSE_BUTTON_5_DOWN)
      mouse->flags |=  (WRAW_MOUSE_FLG_BTN_B5);
   else if (state->usButtonFlags & RI_MOUSE_BUTTON_5_UP)
      mouse->flags &= ~(WRAW_MOUSE_FLG_BTN_B5);

   if (state->usButtonFlags & RI_MOUSE_WHEEL)
   {
      if ((SHORT)state->usButtonData > 0)
         retro_atomic_store_release_int(&mouse->whl_u, 1);
      else if ((SHORT)state->usButtonData < 0)
         retro_atomic_store_release_int(&mouse->whl_d, 1);
   }
}

/* Read by the poll
 * ----------------
 * The keyboard and mouse window is made by the first winraw_poll(),
 * on the thread that polls, and the reports waiting for it are read
 * in bulk - one GetRawInputBuffer() for all of them - at the start of
 * every poll. RETROARCH_RAWINPUT_POLL=0 in the environment turns this
 * off and leaves the driver as the threading note at the top has it,
 * should something need telling apart from it.
 *
 * As it was (the threading note at the top), the window is
 * made on whichever thread runs the video driver's init, and each
 * report is a WM_INPUT message taken when that thread pumps: a
 * PeekMessage() and a GetRawInputData() per report, once per video
 * frame, and not at all while that thread waits in a present. Two
 * things follow.
 *
 * Latency. The poll, on the main thread, reads what the last pump
 * left. A key or a mouse report that arrived since is in the queue,
 * not in the state, and the core gets it a frame later. Read by the
 * poll itself, the state is whatever the devices had sent at the
 * moment of the poll - with late polling, the moment the core asks.
 *
 * Cost. Two system calls a report: at 60 fps a 1000 Hz mouse is some
 * 33 a frame and an 8000 Hz one some 270. A bulk read is one call for
 * all of them, and there is no message to dispatch. No thread is
 * added for this and nothing waits or wakes: the reads happen where
 * the frontend already is.
 *
 * What it costs when nothing is happening is the poll's one read a
 * frame, which then finds nothing: knowing what the devices have sent
 * at the moment of the poll means asking at the moment of the poll.
 * That is the only read made unasked.
 *
 * The thread's pump leaves raw input in the queue for that read
 * (winraw_poll_owns_thread()): it asks for the messages below WM_INPUT
 * and the ones above it, which is how Microsoft's own sample of a
 * buffered read keeps its pump off the reports. Before it did, a
 * session with the mouse in use had the pump take 1398 of 4578
 * reports out one message at a time, each with a read of its own and
 * a bulk read behind it, and the calls saved over the old path came
 * to a few per cent. Some other pump can still come across a report
 * as a message - a modal loop Windows runs for a menu or a window
 * being dragged - and then the callback takes that one and reads the
 * rest in bulk (below).
 *
 * And the window is no longer the video driver's thread's, which has
 * to be true before the input driver can stop being restarted with
 * the video driver. (It still is restarted with it: a later step.)
 *
 * What else differs when this is on:
 *
 * - Key events. A report's key is written to the key table when the
 *   report is read. The call to input_keyboard_event() - into the
 *   menu's state and the core's keyboard callback - is held until the
 *   end of the poll, and so is made on the main thread; as it is
 *   otherwise, under threaded video it is made from the video thread.
 *
 * - Modifiers. win32_get_keyboard_mods() is what the main window's
 *   thread last published from its own key messages, which can be a
 *   frame behind a report just read. Which modifier keys are held is
 *   read from the key table instead; only the lock keys' toggles are
 *   taken from the published value.
 *
 * - A bulk read takes every raw input report waiting on the thread,
 *   whichever window it was for. If winraw_joypad's window is on this
 *   thread too (video not threaded), its reports come out here and
 *   are passed to it (winraw_joypad_take_hid()). An attempt at bulk
 *   reads before this one, d087a820cd, read from inside the WM_INPUT
 *   handler and lost keys and buttons; the report a WM_INPUT message
 *   carries has already left the queue by the time its handler runs,
 *   so a bulk read there never sees it, and a key press is usually
 *   the only report waiting. Here a report that arrives as a message
 *   - the pump got to it before a poll did - is read from its message
 *   first, as it always was, and only then is the rest read in bulk.
 *
 * Raw input goes to a window only while its application is in the
 * foreground, and under threaded video this window's thread is not
 * the one with the focus. Windows judges the foreground by process,
 * not by thread: tried on Windows with threaded video, the reports
 * arrive. RETROARCH_RAWINPUT_POLL=2 is kept for a system where they
 * do not: it registers as a sink, so that everything arrives, and
 * the main window's focus (winraw_focus) decides whether it is taken.
 *
 * Measured there, a minute of play with a 1000 Hz mouse in use: 9714
 * reports in 7679 reads, one read an iteration, none taken by the
 * pump. The old path's two calls a report would have been 19428. */

extern void winraw_joypad_take_hid(HANDLE device, const BYTE *data,
      DWORD report_size, DWORD count);

/* The thread whose raw input queue the poll reads in bulk, and how
 * many windows on it are read that way: this driver's, and
 * winraw_joypad's (winraw_queue_claim_thread()). */
static DWORD              winraw_drain_tid;
static retro_atomic_int_t winraw_drain_claims;
/* This driver, for a bulk read that winraw_joypad's poll starts: one
 * thread's, the polling thread's. */
static void              *winraw_drain_wr;
/* Set by a bulk read made on winraw_joypad's behalf, which comes just
 * before this driver's poll: that poll does not then read again. */
static bool               winraw_queue_was_read;

void winraw_queue_claim_thread(bool claim);

/* 8 when this is a 32-bit process on 64-bit Windows: a bulk read's
 * records are then laid out for 64 bits, the payload eight bytes
 * further on and the records aligned to eight. (GetRawInputData()
 * corrects its one record itself.) */
static unsigned winraw_wow64_shift(void)
{
#ifdef _WIN64
   return 0;
#else
   static int shift = -1;
   if (shift < 0)
   {
      typedef BOOL (WINAPI *is_wow64_t)(HANDLE, PBOOL);
      BOOL wow          = FALSE;
      is_wow64_t is_wow = (is_wow64_t)GetProcAddress(
            GetModuleHandleA("kernel32"), "IsWow64Process");
      if (is_wow && is_wow(GetCurrentProcess(), &wow) && wow)
         shift = 8;
      else
         shift = 0;
   }
   return (unsigned)shift;
#endif
}

static uint16_t winraw_held_mods(const winraw_input_t *wr)
{
   uint16_t mod = win32_get_keyboard_mods()
      & (RETROKMOD_CAPSLOCK | RETROKMOD_NUMLOCK | RETROKMOD_SCROLLOCK);

   if (wr->kb_keys[SC_LSHIFT] || wr->kb_keys[SC_RSHIFT])
      mod |= RETROKMOD_SHIFT;
   if (wr->kb_keys[SC_LCTRL]  || wr->kb_keys[SC_RCTRL])
      mod |= RETROKMOD_CTRL;
   if (wr->kb_keys[SC_LALT]   || wr->kb_keys[SC_RALT])
      mod |= RETROKMOD_ALT;
   if (wr->kb_keys[SC_LSUPER] || wr->kb_keys[SC_RSUPER])
      mod |= RETROKMOD_META;
   return mod;
}

/* Hands the held key events on. Called last in winraw_poll(), from a
 * copy, and touches the driver no more once the first is delivered:
 * what an event sets off is not this function's to know. */
static void winraw_kev_deliver(winraw_input_t *wr)
{
   uint32_t ev[WINRAW_KEV_SIZE];
   unsigned i;
   unsigned n = wr->kev_n;

   if (!n)
      return;
#ifdef HAVE_MENU
   /* keys typed elsewhere are not typed into the menu */
   if (     !winraw_focus
         && (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE))
   {
      wr->kev_n = 0;
      return;
   }
#endif
   memcpy(ev, wr->kev, n * sizeof(ev[0]));
   wr->kev_n = 0;

   for (i = 0; i < n; i++)
      input_keyboard_event((ev[i] >> 16) & 1,
            input_keymaps_translate_keysym_to_rk(ev[i] & 0xFFFF),
            0, (uint16_t)(ev[i] >> 17), RETRO_DEVICE_KEYBOARD);
}

/* One keyboard or mouse report, however it was read. False for a
 * scancode that is ignored. */
static bool winraw_take(winraw_input_t *wr, DWORD type, HANDLE device,
      void *payload)
{
   unsigned i;
   unsigned mcode, flags, down, mod;

   switch (type)
   {
      case RIM_TYPEKEYBOARD:
         {
            RAWKEYBOARD *kb = (RAWKEYBOARD*)payload;
            mcode = kb->MakeCode;
            flags = kb->Flags;
            down  = (flags & RI_KEY_BREAK) ? 0 : 1;
            mod   = 0;

            /* Extended scancodes */
            if (flags & RI_KEY_E0)
               mcode |= 0xE000;
            else if (flags & RI_KEY_E1)
               mcode |= 0xE100;

            /* Special pause-key handling due to
             * scancode 0xE11D45 incoming separately */
            if ((wr->flags & WRAW_INP_FLG_KB_PAUSE) > 0)
            {
               wr->flags &= ~(WRAW_INP_FLG_KB_PAUSE);
               if (mcode == SC_NUMLOCK)
                  mcode = SC_PAUSE;
            }
            else if (mcode == 0xE11D)
               wr->flags |=  (WRAW_INP_FLG_KB_PAUSE);

            /* Ignored scancodes */
            switch (mcode)
            {
               case RETROK_UNKNOWN:
               case 0xE11D:
               case 0xE02A:
               case 0xE036:
               case 0xE0AA:
               case 0xE0B6:
                  return false;
            }

            mod = win32_get_keyboard_mods();

            /* From a keyboard the list does not have: one plugged in
             * since the list was made. (No handle at all is a key
             * something injected, not a device.) */
            if (device)
            {
               for (i = 0; i < wr->kb_cnt; i++)
                  if (wr->kbs[i] == device)
                     break;
               if (i < wr->kb_cnt)
               {
                  /* the keyboard's own keys, beside the one state
                   * below that every keyboard feeds */
                  unsigned bit = winraw_kb_bit(mcode);
                  if (bit < WINRAW_KB_BITS)
                  {
                     unsigned g = wr->kb_grp[i];
                     if (down)
                     {
                        wr->kb_down[i][bit >> 3] |=  (uint8_t)(1 << (bit & 7));
                        if (g != WINRAW_KB_NONE)
                           wr->kg_down[g][bit >> 3] |= (uint8_t)(1 << (bit & 7));
                     }
                     else
                     {
                        unsigned k;
                        wr->kb_down[i][bit >> 3] &= (uint8_t)~(1 << (bit & 7));
                        /* the listed keyboard this is part of lets go
                         * when no part of it holds the key */
                        if (g != WINRAW_KB_NONE)
                        {
                           for (k = 0; k < wr->kb_cnt; k++)
                              if (     wr->kb_grp[k] == g
                                    && (wr->kb_down[k][bit >> 3] & (1 << (bit & 7))))
                                 break;
                           if (k == wr->kb_cnt)
                              wr->kg_down[g][bit >> 3] &= (uint8_t)~(1 << (bit & 7));
                        }
                        /* Let go on this keyboard and still held on
                         * another: for the one state every keyboard
                         * feeds, the key is still down, and there is
                         * no key-up to tell of. (It used to go up
                         * with the first keyboard to let go.) */
                        for (k = 0; k < wr->kb_cnt; k++)
                           if (wr->kb_down[k][bit >> 3] & (1 << (bit & 7)))
                              return true;
                     }
                  }
               }
               else if (  wr->poll_drain
                       && device != wr->kb_not_listed
                       && wr->kb_cnt < WINRAW_KB_RAW_MAX)
               {
                  wr->kb_unknown      = device;
                  wr->kb_unknown_seen = true;
               }
            }

            wr->kb_keys[mcode] = down;
            if (wr->poll_drain)
            {
               /* held until the end of the poll */
               if (wr->kev_n < WINRAW_KEV_SIZE)
                  wr->kev[wr->kev_n++] =
                     WINRAW_KEV_PACK(mcode, down, winraw_held_mods(wr));
               else
                  wr->kev_dropped++;
            }
            else
               input_keyboard_event(down,
                     input_keymaps_translate_keysym_to_rk(mcode),
                     0, mod, RETRO_DEVICE_KEYBOARD);
         }
         break;
      case RIM_TYPEMOUSE:
         for (i = 0; i < wr->mouse_cnt; ++i)
         {
            if (g_mice[i].hnd == device)
            {
               winraw_update_mouse_state(wr,
                     &g_mice[i], (RAWMOUSE*)payload);
               break;
            }
         }
         /* From a mouse the list does not have: one plugged in since
          * the list was made. (No handle at all is input something
          * injected, not a device.) */
         if (     i == wr->mouse_cnt
               && wr->poll_drain
               && device
               && device != wr->mouse_not_listed)
         {
            wr->mouse_unknown      = device;
            wr->mouse_unknown_seen = true;
         }
         break;
   }
   return true;
}

/* Everything waiting in this thread's raw input queue, in as few
 * reads as it takes: one, unless there is more than a buffer's worth.
 * Only for the thread the window is on. */
static void winraw_drain(winraw_input_t *wr)
{
   static uint64_t buf[WINRAW_DRAIN_BYTES / sizeof(uint64_t)];
   const unsigned shift = winraw_wow64_shift();
   const size_t align   = shift ? 8 : sizeof(void*);

   for (;;)
   {
      UINT i;
      UINT size = sizeof(buf);
      BYTE *p   = (BYTE*)buf;
      UINT n    = GetRawInputBuffer((PRAWINPUT)buf, &size,
            sizeof(RAWINPUTHEADER));

      /* nothing waiting; or a report bigger than the buffer, which is
       * left to arrive as a message */
      if (n == 0 || n == (UINT)-1)
      {
         if (wr)
            wr->drain_empty++;
         break;
      }
      if (wr)
         wr->drain_reads++;

      for (i = 0; i < n; i++)
      {
         RAWINPUT *ri  = (RAWINPUT*)p;
         BYTE *payload = (BYTE*)&ri->data + shift;

         if (ri->header.dwType == RIM_TYPEHID)
         {
            /* a controller's, for winraw_joypad's window on this
             * thread */
            RAWHID *hid = (RAWHID*)payload;
            if (wr)
               wr->drained_hid++;
            winraw_joypad_take_hid(ri->header.hDevice, hid->bRawData,
                  hid->dwSizeHid, hid->dwCount);
         }
         /* no keyboard and mouse driver: nothing is registered for
          * them, and there is nothing to take them */
         else if (wr)
         {
            wr->drained++;
            /* as a sink, background input arrives as well */
            if (     !wr->sink || winraw_focus
                  || (   wr->kb_background
                      && ri->header.dwType == RIM_TYPEKEYBOARD))
               winraw_take(wr, ri->header.dwType, ri->header.hDevice,
                     payload);
         }

         p += (ri->header.dwSize + align - 1) & ~(align - 1);
      }

      /* It had room for as much again: the queue is empty, and the
       * read that would say so is saved. */
      if ((size_t)(p - (BYTE*)buf) < sizeof(buf) / 2)
         break;
   }
}

/* For winraw_joypad, whose window is on the polling thread too when
 * raw input is read by the poll. A bulk read takes every report
 * waiting on the thread, its controllers' and this driver's alike, so
 * there is one read for both: winraw_joypad's poll, which
 * input_driver_poll() calls first, makes it through here, and
 * winraw_poll() then finds it made. With an input driver that is not
 * this one there is no winraw_poll(), and this is the only read. */
void winraw_queue_read(void)
{
   winraw_drain((winraw_input_t*)winraw_drain_wr);
   winraw_queue_was_read = true;
}

/* A window on the calling thread is to have its raw input read by the
 * poll, or no longer: the thread's pump leaves raw input in the queue
 * while any is (winraw_poll_owns_thread()). */
void winraw_queue_claim_thread(bool claim)
{
   if (claim)
   {
      winraw_drain_tid = GetCurrentThreadId();
      retro_atomic_fetch_add_int(&winraw_drain_claims, 1);
   }
   else
      retro_atomic_fetch_add_int(&winraw_drain_claims, -1);
}

/* Whether raw input is read by the poll at all: on, unless
 * RETROARCH_RAWINPUT_POLL=0. */
bool winraw_raw_input_polled(void)
{
   const char *env = getenv("RETROARCH_RAWINPUT_POLL");
   return !(env && env[0] == '0');
}

/* Taken by the window's thread - the driver as it was, and as it is
 * with RETROARCH_RAWINPUT_POLL=0 - a report gets into the state the
 * poll reads when that thread pumps its messages, which the video
 * thread does once a frame. So what the poll reads is as old as that
 * thread's last pump: whatever a device has sent since waits for the
 * poll after the next one. Read by the poll that wait is not there,
 * which is the point of reading by the poll; this measures it where it
 * still exists, so that there is a number for it and not an argument.
 *
 * The thread that made the window stamps the clock each time its pump
 * has run dry; the poll, in winraw_poll(), takes the stamp's age. The
 * figures are logged when the driver is freed. Nothing here runs when
 * the poll reads. */
static DWORD              winraw_legacy_tid;
static retro_atomic_int_t winraw_legacy_pumped;
static LARGE_INTEGER      winraw_legacy_freq;

/* microseconds, the low 32 bits: for differences */
static uint32_t winraw_legacy_now(void)
{
   LARGE_INTEGER now;
   QueryPerformanceCounter(&now);
   return (uint32_t)(
           (now.QuadPart / winraw_legacy_freq.QuadPart) * 1000000
         + (now.QuadPart % winraw_legacy_freq.QuadPart) * 1000000
               / winraw_legacy_freq.QuadPart);
}

void winraw_pump_done(void)
{
   if (     winraw_legacy_tid
         && GetCurrentThreadId() == winraw_legacy_tid)
      retro_atomic_store_release_int(&winraw_legacy_pumped,
            (int)(winraw_legacy_now() | 1));
}

/* For a thread's pump (ui_application_win32_process_events()): true
 * on the thread whose raw input the poll reads in bulk. The pump then
 * leaves raw input where it is - in the queue, for the poll - instead
 * of taking it out one message at a time. */
bool winraw_poll_owns_thread(void)
{
   return retro_atomic_load_relaxed_int(&winraw_drain_claims) > 0
      && GetCurrentThreadId() == winraw_drain_tid;
}

/* Input that came while the application was in the background is not
 * taken - unless the driver registered as a sink, where everything
 * that is not foreground input comes marked that way and the main
 * window's focus decides. */
static bool winraw_in_background(const winraw_input_t *wr, WPARAM wpar)
{
   if (GET_RAWINPUT_CODE_WPARAM(wpar) == RIM_INPUT)
      return false;
   return !(wr->sink && winraw_focus);
}

static LRESULT CALLBACK winraw_callback(
      HWND wnd, UINT msg, WPARAM wpar, LPARAM lpar)
{
   static uint8_t data[1024];
   RAWINPUT       *ri = (RAWINPUT*)data;
   UINT size          = sizeof(data);
   winraw_input_t *wr;

   if (msg != WM_INPUT)
      return DefWindowProcA(wnd, msg, wpar, lpar);

   /* none once the driver has let go of the window */
   if (!(wr = (winraw_input_t*)(LONG_PTR)
            GetWindowLongPtr(wnd, GWLP_USERDATA)))
      return DefWindowProcA(wnd, msg, wpar, lpar);

   if (!(
          winraw_in_background(wr, wpar)
       || GetRawInputData((HRAWINPUT)lpar, RID_INPUT,
         data, &size, sizeof(RAWINPUTHEADER)) == (UINT)-1))
   {
      bool taken;
      wr->by_message++;
      taken = winraw_take(wr, ri->header.dwType, ri->header.hDevice,
            &ri->data);
      /* The pump came across a report before a poll read it. This one
       * has left the queue with its message and had to be read on its
       * own; whatever is waiting behind it is read in bulk now, rather
       * than dispatched by the pump one message at a time. Nothing is
       * read here when no report is waiting, so a pump that finds none
       * costs nothing extra. */
      if (wr->poll_drain)
         winraw_drain(wr);
      if (!taken)
         return 0;
   }

   DefWindowProcA(wnd, msg, wpar, lpar);
   return 0;
}

/* The window for the bulk reads: made by the first poll, so that it
 * is the polling thread's, and registered for the keyboard and the
 * mouse. */
static bool winraw_poll_window_up(winraw_input_t *wr)
{
   RAWINPUTDEVICE rid[2];
   DWORD base = wr->sink ? RIDEV_INPUTSINK : 0;

   if (!(wr->window = winraw_create_window(winraw_callback)))
      return false;
   SetWindowLongPtr(wr->window, GWLP_USERDATA, (LONG_PTR)wr);

   rid[0].dwFlags     = base;
   rid[0].hwndTarget  = wr->window;
   rid[0].usUsagePage = 0x01; /* Generic desktop */
   rid[0].usUsage     = 0x06; /* Keyboard */
   wr->nowinkey       = config_get_ptr()->bools.input_nowinkey_enable;
   if (wr->nowinkey)
      rid[0].dwFlags |= RIDEV_NOHOTKEYS; /* Disable win keys while focused */
   /* Background Keyboard Input: Windows sends the keys while another
    * application is active only to a sink */
   wr->kb_background  = config_get_ptr()->bools.input_keyboard_background;
   if (wr->kb_background)
      rid[0].dwFlags |= RIDEV_INPUTSINK;

   rid[1].dwFlags     = base;
   rid[1].hwndTarget  = wr->window;
   rid[1].usUsagePage = 0x01; /* generic desktop */
   rid[1].usUsage     = 0x02; /* mouse */

   if (     RegisterRawInputDevices(&rid[0], 1, sizeof(RAWINPUTDEVICE))
         && RegisterRawInputDevices(&rid[1], 1, sizeof(RAWINPUTDEVICE)))
   {
      wr->window_tid  = GetCurrentThreadId();
      winraw_drain_wr = wr;
      winraw_queue_claim_thread(true);
      RARCH_LOG("[WinRaw] Keyboard and mouse are read in bulk by the poll"
            " (thread %lu)%s.\n", (unsigned long)wr->window_tid,
            wr->sink ? ", registered as a sink" : "");
      return true;
   }

   SetWindowLongPtr(wr->window, GWLP_USERDATA, 0);
   DestroyWindow(wr->window);
   wr->window = NULL;
   return false;
}

/* On, unless RETROARCH_RAWINPUT_POLL=0. 2 registers as a sink as
 * well. */
static bool winraw_poll_wanted(bool *sink)
{
   const char *env = getenv("RETROARCH_RAWINPUT_POLL");
   *sink           = (env && env[0] == '2');
   return winraw_raw_input_polled();
}

/* Two things the driver used to pick up only by being started again,
 * which it was at every video driver restart - a content load, a
 * change of video setting. Read by the poll it has no need of the
 * restart, and these are done when they come up instead.
 *
 * The list of mice is made when the driver starts, and a report from
 * a mouse that is not on it was dropped: a mouse plugged in while
 * RetroArch ran did nothing until something restarted the drivers.
 * Such a report now has the list made again at the end of the poll,
 * as a restart would make it - same order, same names task - with the
 * mice that were already there keeping their position, buttons and
 * device type. Not more than once a second, and a handle that is
 * still not a listed mouse afterwards is not asked about again. */
static void winraw_mice_refresh(winraw_input_t *wr)
{
   unsigned i, j;
   bool found               = false;
   winraw_mouse_t *new_all  = NULL;
   winraw_mouse_t *new_snap = NULL;
   unsigned new_cnt         = 0;
   DWORD now                = GetTickCount();

   wr->mouse_unknown_seen   = false;
   if (wr->mouse_refresh_tick && now - wr->mouse_refresh_tick < 1000)
      return;
   wr->mouse_refresh_tick   = now ? now : 1;

   if (!winraw_init_devices(&new_all, &new_cnt))
      return;
   if (new_cnt && !(new_snap = (winraw_mouse_t*)
            malloc(new_cnt * sizeof(winraw_mouse_t))))
   {
      free(new_all);
      return;
   }

   for (i = 0; i < new_cnt; i++)
   {
      for (j = 0; j < wr->mouse_cnt; j++)
      {
         if (new_all[i].hnd == g_mice[j].hnd)
         {
            new_all[i] = g_mice[j];
            break;
         }
      }
      if (new_all[i].hnd == wr->mouse_unknown)
         found = true;
   }
   if (new_cnt)
      memcpy(new_snap, new_all, new_cnt * sizeof(winraw_mouse_t));

   free(g_mice);
   free(wr->mice);
   g_mice        = new_all;
   wr->mice      = new_snap;
   wr->mouse_cnt = new_cnt;

   if (!found)
      wr->mouse_not_listed = wr->mouse_unknown;
   RARCH_LOG("[WinRaw] Mouse list made again: %u.\n", new_cnt);
}

/* The "disable Windows keys" setting is part of how the keyboard is
 * registered. It took effect at the driver's next start; now the
 * keyboard is registered again when the setting is seen to have
 * changed. */
static void winraw_nowinkey_apply(winraw_input_t *wr, bool enable,
      bool background)
{
   RAWINPUTDEVICE rid;
   rid.dwFlags     = ((wr->sink || background) ? RIDEV_INPUTSINK : 0)
                   | (enable   ? RIDEV_NOHOTKEYS : 0);
   rid.hwndTarget  = wr->window;
   rid.usUsagePage = 0x01; /* Generic desktop */
   rid.usUsage     = 0x06; /* Keyboard */
   if (RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE)))
   {
      wr->nowinkey      = enable;
      wr->kb_background = background;
   }
}

static void *winraw_init(const char *joypad_driver)
{
   RAWINPUTDEVICE rid;
   bool input_nowinkey_enable = config_get_ptr()->bools.input_nowinkey_enable;
   winraw_input_t *wr   = (winraw_input_t *)
      calloc(1, sizeof(winraw_input_t));

   if (!wr)
      return NULL;

   input_keymaps_init_keyboard_lut(rarch_key_map_winraw);

   /* Read in bulk by the poll, if asked for: the window is then made
    * by the first poll, on the thread that polls, not here. */
   if (winraw_poll_wanted(&wr->sink))
   {
      wr->poll_drain = true;
      if (!winraw_init_devices(&g_mice, &wr->mouse_cnt))
         goto error;
      winraw_keyboards_list(wr);
      if (wr->mouse_cnt)
      {
         if (!(wr->mice = (winraw_mouse_t*)
            malloc(wr->mouse_cnt * sizeof(winraw_mouse_t))))
            goto error;
         memcpy(wr->mice, g_mice, wr->mouse_cnt * sizeof(winraw_mouse_t));
      }
      return wr;
   }

   if (!(wr->window = winraw_create_window(winraw_callback)))
      goto error;
   if (!winraw_init_devices(&g_mice, &wr->mouse_cnt))
      goto error;
   winraw_keyboards_list(wr);

   if (wr->mouse_cnt)
   {
      if (!(wr->mice = (winraw_mouse_t*)
         malloc(wr->mouse_cnt * sizeof(winraw_mouse_t))))
         goto error;

      memcpy(wr->mice, g_mice, wr->mouse_cnt * sizeof(winraw_mouse_t));
   }

   rid.dwFlags     = (wr->window) ? 0 : RIDEV_REMOVE;
   rid.hwndTarget  = wr->window;
   rid.usUsagePage = 0x01; /* Generic desktop */
   rid.usUsage     = 0x06; /* Keyboard */
   if (input_nowinkey_enable)
      rid.dwFlags |= RIDEV_NOHOTKEYS; /* Disable win keys while focused */

   if (!RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE)))
      goto error;

   rid.dwFlags     = wr->window ? 0 : RIDEV_REMOVE;
   rid.hwndTarget  = wr->window;
   rid.usUsagePage = 0x01; /* generic desktop */
   rid.usUsage     = 0x02; /* mouse */

   if (!RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE)))
      goto error;

   SetWindowLongPtr(wr->window, GWLP_USERDATA, (LONG_PTR)wr);
   retro_atomic_store_release_int(&winraw_legacy_pumped, 0);
   if (QueryPerformanceFrequency(&winraw_legacy_freq)
         && winraw_legacy_freq.QuadPart > 0)
      winraw_legacy_tid = GetCurrentThreadId();

#ifndef _XBOX
   /* wr->window was created on this thread, so WM_INPUT is posted to
    * this thread's queue and only a pump running here will dispatch it.
    * The pump belongs to whichever thread owns the main window - the
    * video thread under video_threaded. See the threading note at the
    * top of this file: every Windows video driver that supports this
    * input driver builds it from its own context callback, so the two
    * match, but the video_driver_init_input() fallback runs on the main
    * thread after the video thread already exists. Say so rather than
    * leaving the user with silently dead input. */
   {
      DWORD self_tid = GetCurrentThreadId();
      DWORD wnd_tid  = main_window.hwnd
         ? GetWindowThreadProcessId(main_window.hwnd, NULL) : 0;

      if (wnd_tid && wnd_tid != self_tid)
         RARCH_ERR("[WinRaw] Raw input window is on thread %lu but the "
               "message pump runs on thread %lu - WM_INPUT will not be "
               "dispatched and keyboard/mouse input will not work.\n",
               (unsigned long)self_tid, (unsigned long)wnd_tid);
   }
#endif

   return wr;

error:
   if (wr && wr->window)
   {
      rid.dwFlags     = RIDEV_REMOVE;
      rid.hwndTarget  = NULL;
      rid.usUsagePage = 0x01; /* generic desktop */
      rid.usUsage     = 0x02; /* mouse */

      RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE));

      rid.dwFlags     = RIDEV_REMOVE;
      rid.hwndTarget  = NULL;
      rid.usUsagePage = 0x01; /* Generic desktop */
      rid.usUsage     = 0x06; /* Keyboard */
      if (input_nowinkey_enable)
         rid.dwFlags |= RIDEV_NOHOTKEYS; /* Disable win keys while focused */

      RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE));
      DestroyWindow(wr->window);
      UnregisterClassA("winraw-input", NULL);
   }
   free(g_mice);
   if (wr)
      free(wr->mice);
   free(wr);
   return NULL;
}

static void winraw_poll(void *data)
{
   unsigned i;
   POINT crs_pos          = {0, 0};
   bool crs_pos_valid     = false;
   winraw_input_t *wr     = (winraw_input_t*)data;

   /* Everything the devices have sent up to now, before any of it is
    * looked at below. */
   if (wr->poll_drain)
   {
      if (     !wr->window && !wr->window_failed
            && !winraw_poll_window_up(wr))
      {
         wr->window_failed = true;
         RARCH_ERR("[WinRaw] Could not make the keyboard and mouse window.\n");
      }
      /* winraw_joypad's poll, just before this one, has already made
       * the read for both when it is the joypad driver */
      if (wr->window && !winraw_queue_was_read)
         winraw_drain(wr);
      winraw_queue_was_read = false;
   }
   else
   {
      /* how old what the window's thread has taken is, now that it is
       * read (winraw_pump_done()) */
      int at = retro_atomic_load_acquire_int(&winraw_legacy_pumped);
      if (at)
      {
         uint32_t age = winraw_legacy_now() - (uint32_t)at;
         /* a pause, a dragged window: not a frame's wait */
         if (age < 250000)
         {
            wr->stale_sum += age;
            if (age > wr->stale_max)
               wr->stale_max = age;
            wr->stale_polls++;
         }
      }
   }

   /* Fix coordinates after a resolution change. Runs here rather than
    * in the wndproc so that active_rect, view_abs_ratio_* and the
    * position all belong to this thread. */
   GetClientRect((HWND)video_driver_window_get(), &wr->prev_rect);

   if (!EqualRect(&wr->active_rect, &wr->prev_rect))
   {
      if (wr->rect_delay < 10)
      {
         winraw_init_mouse_xy_mapping(wr); /* Triggering fewer times seems to fix the issue. Forcing resize while resolution is changing */
         wr->rect_delay++;
      }
      else
      {
         wr->active_rect = wr->prev_rect;
         winraw_init_mouse_xy_mapping(wr);
         wr->rect_delay  = 0;
      }
   }
   else if (retro_atomic_exchange_int(&wr->map_pending, 0))
      winraw_init_mouse_xy_mapping(wr);

   /* Sync coordinates when window regains focus */
   if (winraw_focus && !wr->last_focus)
      winraw_sync_mouse_to_cursor(wr);

   /* Release any keys still held at the moment focus is lost.
    *
    * The wndproc rejects background input (it returns early when
    * GET_RAWINPUT_CODE_WPARAM() is not RIM_INPUT), so no key-up
    * ever arrives for a key that was down when the window was
    * deactivated, and it would stay latched indefinitely.
    *
    * The clear is deferred while Alt is still physically down,
    * which is the case throughout an Alt-Tab. Latch it instead of
    * testing the focus-loss edge directly, otherwise the edge is
    * missed on exactly the Alt-Tab case that needs it and the
    * keys stay latched until focus returns. */
   {
      bool taking       = winraw_focus || wr->kb_background;
      if (!taking && wr->kb_taking)
         wr->kb_clear_pending = true;
      wr->kb_taking     = taking;
   }

   if (wr->kb_clear_pending && !(GetKeyState(VK_MENU) & 0x8000))
   {
      /* LAlt is released through input_keyboard_event() rather
       * than just zeroed, so the keyboard layer observes the
       * transition and does not treat Alt as held afterwards. */
      if (wr->kb_keys[SC_LALT])
         input_keyboard_event(0,
               input_keymaps_translate_keysym_to_rk(SC_LALT),
               0, 0, RETRO_DEVICE_KEYBOARD);

      memset(wr->kb_keys, 0, SC_LAST);
      memset(wr->kb_down, 0, sizeof(wr->kb_down));
      memset(wr->kg_down, 0, sizeof(wr->kg_down));
      wr->kb_clear_pending = false;
   }

   wr->last_focus = winraw_focus;

   for (i = 0; i < wr->mouse_cnt; ++i)
   {
      /* Derive the position. This is the only writer of g_mice[i].x/y.
       *
       * The three sources are mutually exclusive and the wndproc marks
       * which one applies: a deferred cursor query wins, then an
       * absolute report, then the delta accumulated over the frame.
       * The cursor query happens at most once per frame however many
       * reports asked for it or however many mice are attached. */
      LONG dx;
      LONG dy;

      /* Clear buttons when not focused */
      if (!winraw_focus)
         g_mice[i].flags = 0;

      dx = (LONG)retro_atomic_exchange_int(&g_mice[i].dlt_x, 0);
      dy = (LONG)retro_atomic_exchange_int(&g_mice[i].dlt_y, 0);

      if (retro_atomic_exchange_int(&g_mice[i].pos_pending, 0))
      {
         if (!crs_pos_valid)
         {
            HWND wnd = (HWND)video_driver_window_get();
            if (!GetCursorPos(&crs_pos))
               RARCH_DBG("[WinRaw] GetCursorPos failed with error %lu.\n", GetLastError());
            else if (!wnd || !ScreenToClient(wnd, &crs_pos))
               RARCH_DBG("[WinRaw] ScreenToClient failed with error %lu.\n", GetLastError());
            else
               crs_pos_valid = true;
         }
         if (crs_pos_valid)
         {
            g_mice[i].x = crs_pos.x;
            g_mice[i].y = crs_pos.y;
         }
      }
      else if (retro_atomic_exchange_int(&g_mice[i].abs_pending, 0))
      {
         unsigned pos = (unsigned)retro_atomic_load_acquire_int(
               &g_mice[i].abs_pos);
         g_mice[i].x  = (LONG)VIDEO_POS_X(pos);
         g_mice[i].y  = (LONG)VIDEO_POS_Y(pos);
      }
      else if (dx || dy)
      {
         LONG nx = g_mice[i].x + dx;
         LONG ny = g_mice[i].y + dy;
         /* Prevent travel outside active window */
         if (nx < wr->active_rect.left)
            nx = wr->active_rect.left;
         else if (nx > wr->active_rect.right)
            nx = wr->active_rect.right;

         if (ny < wr->active_rect.top)
            ny = wr->active_rect.top;
         else if (ny > wr->active_rect.bottom)
            ny = wr->active_rect.bottom;

         g_mice[i].x = nx;
         g_mice[i].y = ny;
      }

      wr->mice[i].x       = g_mice[i].x;
      wr->mice[i].y       = g_mice[i].y;
      wr->mice[i].dlt_x   = dx;
      wr->mice[i].dlt_y   = dy;
      wr->mice[i].whl_u   = retro_atomic_exchange_int(&g_mice[i].whl_u, 0);
      wr->mice[i].whl_d   = retro_atomic_exchange_int(&g_mice[i].whl_d, 0);
      wr->mice[i].flags   = g_mice[i].flags;
   }

   /* devices came or went: the keyboards are listed again */
   if (     retro_atomic_load_relaxed_int(&winraw_devices_changed)
         && retro_atomic_exchange_int(&winraw_devices_changed, 0))
   {
      winraw_keyboards_list(wr);
      wr->kb_refresh_tick = GetTickCount();
      wr->kb_unknown_seen = false;
      wr->kb_not_listed   = NULL;
      RARCH_LOG("[WinRaw] Keyboard list made again: %u.\n", wr->kg_cnt);
   }

   if (wr->poll_drain)
   {
      if (wr->mouse_unknown_seen)
         winraw_mice_refresh(wr);
      if (wr->kb_unknown_seen)
         winraw_keyboards_refresh(wr);
      if (wr->window)
      {
         settings_t *settings = config_get_ptr();
         bool nowinkey   = settings->bools.input_nowinkey_enable;
         bool background = settings->bools.input_keyboard_background;
         if (nowinkey != wr->nowinkey || background != wr->kb_background)
            winraw_nowinkey_apply(wr, nowinkey, background);
      }
      winraw_kev_deliver(wr);
   }
}

static int16_t winraw_input_state(
      void *data,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      rarch_joypad_info_t *joypad_info,
      const retro_keybind_set *binds,
      bool keyboard_mapping_blocked,
      unsigned port,
      unsigned device,
      unsigned idx,
      unsigned id)
{
   winraw_mouse_t *mouse = NULL;

   if (port < MAX_USERS)
   {
      int16_t ret           = 0;
      winraw_input_t *wr    = (winraw_input_t*)data;
      /* the one keyboard this port was given, if it was given one */
      const uint8_t *own    = winraw_port_keys(wr, port);
      /* In the background, keys play the game only: they work no
       * hotkey, and nothing while the menu is up. */
      bool keys_ok          = winraw_focus;
      bool meta_ok          = winraw_focus;
      bool process_mouse    =
         (device == RETRO_DEVICE_JOYPAD)
         || (device == RETRO_DEVICE_MOUSE)
         || (device == RARCH_DEVICE_MOUSE_SCREEN)
         || (device == RETRO_DEVICE_LIGHTGUN)
         || (device == RETRO_DEVICE_POINTER)
         || (device == RARCH_DEVICE_POINTER_SCREEN);

      if (process_mouse)
      {
         unsigned i;
         for (i = 0; i < wr->mouse_cnt; ++i)
         {
            /* (the port's Mouse Index, or where its pinned mouse is) */
            if (i == input_mouse_port_index(port))
            {
               mouse = &wr->mice[i];
               if (mouse && device > RETRO_DEVICE_JOYPAD)
                  g_mice[i].device = device;
               break;
            }
         }
      }

#ifdef HAVE_MENU
      if (!keys_ok && wr->kb_background)
         keys_ok = !(menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE);
#else
      if (wr->kb_background)
         keys_ok = true;
#endif

      switch (device)
      {
         case RETRO_DEVICE_JOYPAD:
            if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
            {
               unsigned i;

               if (mouse)
               {
                  for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
                  {
                     if (RETRO_KEYBIND_VALID(&binds[port][i]))
                     {
                        if (winraw_mouse_button_pressed(wr, mouse, port, binds[port][i].mbutton))
                           ret |= (1 << i);
                     }
                  }
               }

               if (!keyboard_mapping_blocked && keys_ok)
               {
                  for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
                  {
                     if (RETRO_KEYBIND_VALID(&binds[port][i]))
                     {
                        if (     (RETRO_KEYBIND_KEY(&binds[port][i]) && RETRO_KEYBIND_KEY(&binds[port][i]) < RETROK_LAST)
                              && WINRAW_PORT_KEY_PRESSED(wr, own, RETRO_KEYBIND_KEY(&binds[port][i])))
                           ret |= (1 << i);
                     }
                  }
               }

               return ret;
            }

            if (id < RARCH_BIND_LIST_END)
            {
               if (RETRO_KEYBIND_VALID(&binds[port][id]))
               {
                  if (     (RETRO_KEYBIND_KEY(&binds[port][id]) && RETRO_KEYBIND_KEY(&binds[port][id]) < RETROK_LAST)
                        /* a hotkey answers to every keyboard */
                        && ((id >= RARCH_FIRST_META_KEY)
                           ? WINRAW_KEYBOARD_PRESSED(wr, RETRO_KEYBIND_KEY(&binds[port][id]))
                           : WINRAW_PORT_KEY_PRESSED(wr, own, RETRO_KEYBIND_KEY(&binds[port][id])))
                        && (id == RARCH_GAME_FOCUS_TOGGLE || !keyboard_mapping_blocked)
                        && ((id >= RARCH_FIRST_META_KEY) ? meta_ok : keys_ok)
                     )
                     return 1;
                  else if (mouse && winraw_mouse_button_pressed(wr, mouse, port, binds[port][id].mbutton))
                     return 1;
               }
            }
            break;
         case RETRO_DEVICE_ANALOG:
            {
               int id_minus_key      = 0;
               int id_plus_key       = 0;
               unsigned id_minus     = 0;
               unsigned id_plus      = 0;
               bool id_plus_valid    = false;
               bool id_minus_valid   = false;

               input_conv_analog_id_to_bind_id(idx, id, id_minus, id_plus);

               id_minus_valid        = RETRO_KEYBIND_VALID(&binds[port][id_minus]);
               id_plus_valid         = RETRO_KEYBIND_VALID(&binds[port][id_plus]);
               id_minus_key          = RETRO_KEYBIND_KEY(&binds[port][id_minus]);
               id_plus_key           = RETRO_KEYBIND_KEY(&binds[port][id_plus]);

               if (keys_ok && id_plus_valid && id_plus_key && id_plus_key < RETROK_LAST)
               {
                  if (WINRAW_PORT_KEY_PRESSED(wr, own, id_plus_key))
                     ret = 0x7fff;
               }
               if (keys_ok && id_minus_valid && id_minus_key && id_minus_key < RETROK_LAST)
               {
                  if (WINRAW_PORT_KEY_PRESSED(wr, own, id_minus_key))
                     ret += -0x7fff;
               }
            }
            return ret;
         case RETRO_DEVICE_KEYBOARD:
            return keys_ok && (id && id < RETROK_LAST) && WINRAW_PORT_KEY_PRESSED(wr, own, id);
         case RETRO_DEVICE_MOUSE:
         case RARCH_DEVICE_MOUSE_SCREEN:
            if (mouse)
            {
               bool abs = (device == RARCH_DEVICE_MOUSE_SCREEN);
               switch (id)
               {
                  case RETRO_DEVICE_ID_MOUSE_X:
                     return abs ? mouse->x : mouse->dlt_x;
                  case RETRO_DEVICE_ID_MOUSE_Y:
                     return abs ? mouse->y : mouse->dlt_y;
                  case RETRO_DEVICE_ID_MOUSE_LEFT:
                     if ((mouse->flags & WRAW_MOUSE_FLG_BTN_L) > 0)
                        return 1;
                     break;
                  case RETRO_DEVICE_ID_MOUSE_RIGHT:
                     if ((mouse->flags & WRAW_MOUSE_FLG_BTN_R) > 0)
                        return 1;
                     break;
                  case RETRO_DEVICE_ID_MOUSE_WHEELUP:
                     if (mouse->whl_u)
                        return 1;
                     break;
                  case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
                     if (mouse->whl_d)
                        return 1;
                     break;
                  case RETRO_DEVICE_ID_MOUSE_MIDDLE:
                     if ((mouse->flags & WRAW_MOUSE_FLG_BTN_M) > 0)
                        return 1;
                     break;
                  case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
                     if ((mouse->flags & WRAW_MOUSE_FLG_BTN_B4) > 0)
                        return 1;
                     break;
                  case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
                     if ((mouse->flags & WRAW_MOUSE_FLG_BTN_B5) > 0)
                        return 1;
                     break;
               }
            }
            break;
         case RETRO_DEVICE_POINTER:
         case RARCH_DEVICE_POINTER_SCREEN:
            {
               struct video_viewport vp    = {0};
               bool pointer_down           = false;
               int x                       = 0;
               int y                       = 0;
               int16_t res_x               = 0;
               int16_t res_y               = 0;
               int16_t res_screen_x        = 0;
               int16_t res_screen_y        = 0;
               unsigned num                = 0;
               struct winraw_pointer_status *
                  check_pos                = wr->pointer_head.next;

               while (check_pos && num < idx)
               {
                  num++;
                  check_pos    = check_pos->next;
               }
               if (!check_pos && idx > 0) /* idx = 0 has mouse fallback. */
                  return 0;

               if (mouse)
               {
                  x            = mouse->x;
                  y            = mouse->y;
                  pointer_down = (mouse->flags & (WRAW_MOUSE_FLG_BTN_L)) > 0;
               }

               if (check_pos)
               {
                  x            = check_pos->pointer_x;
                  y            = check_pos->pointer_y;
                  pointer_down = true;
               }

               if (!(video_driver_translate_coord_viewport_confined_wrap(&vp, x, y,
                           &res_x, &res_y, &res_screen_x, &res_screen_y)))
                  return 0;

               if (device == RARCH_DEVICE_POINTER_SCREEN)
               {
                  res_x        = res_screen_x;
                  res_y        = res_screen_y;
               }

               switch (id)
               {
                  case RETRO_DEVICE_ID_POINTER_X:
                     return res_x;
                  case RETRO_DEVICE_ID_POINTER_Y:
                     return res_y;
                  case RETRO_DEVICE_ID_POINTER_PRESSED:
                     return pointer_down;
                  case RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN:
                     return input_driver_pointer_is_offscreen(res_x, res_y);
                  default:
                     break;
               }
            }
            break;
         case RETRO_DEVICE_LIGHTGUN:
            switch (id)
            {
               case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
               case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
               case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
                  if (mouse)
                     return winraw_lightgun_aiming_state(wr, mouse, port, id);
                  break;
               case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
               case RETRO_DEVICE_ID_LIGHTGUN_RELOAD:
               case RETRO_DEVICE_ID_LIGHTGUN_AUX_A:
               case RETRO_DEVICE_ID_LIGHTGUN_AUX_B:
               case RETRO_DEVICE_ID_LIGHTGUN_AUX_C:
               case RETRO_DEVICE_ID_LIGHTGUN_START:
               case RETRO_DEVICE_ID_LIGHTGUN_SELECT:
               case RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP:
               case RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN:
               case RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT:
               case RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT:
               case RETRO_DEVICE_ID_LIGHTGUN_PAUSE: /* deprecated */
                  {
                     unsigned new_id                = input_driver_lightgun_id_convert(id);
                     const uint64_t bind_joykey     = input_config_binds[port][new_id].joykey;
                     const uint64_t bind_joyaxis    = input_config_binds[port][new_id].joyaxis;
                     const uint64_t autobind_joykey = input_autoconf_binds[port][new_id].joykey;
                     const uint64_t autobind_joyaxis= input_autoconf_binds[port][new_id].joyaxis;
                     uint16_t joyport               = joypad_info->joy_idx;
                     float axis_threshold           = joypad_info->axis_threshold;
                     const uint64_t joykey          = (bind_joykey != NO_BTN)
                        ? bind_joykey  : autobind_joykey;
                     const uint32_t joyaxis         = (bind_joyaxis != AXIS_NONE)
                        ? bind_joyaxis : autobind_joyaxis;

                     if (RETRO_KEYBIND_VALID(&binds[port][new_id]))
                     {
                        if ((uint16_t)joykey != NO_BTN && joypad->button(
                                 joyport, (uint16_t)joykey))
                           return 1;
                        if (joyaxis != AXIS_NONE &&
                              ((float)abs(joypad->axis(joyport, joyaxis))
                               / 0x8000) > axis_threshold)
                           return 1;
                        else if ((RETRO_KEYBIND_KEY(&binds[port][new_id]) && RETRO_KEYBIND_KEY(&binds[port][new_id]) < RETROK_LAST)
                              && !keyboard_mapping_blocked
                              && WINRAW_PORT_KEY_PRESSED(wr, own, RETRO_KEYBIND_KEY(&binds[port][new_id]))
                           )
                           return 1;
                        else if (mouse)
                        {
                           if (winraw_mouse_button_pressed(wr, mouse, port, binds[port][new_id].mbutton))
                              return 1;
                        }
                     }
                  }
                  break;
                  /*deprecated*/
               case RETRO_DEVICE_ID_LIGHTGUN_X:
                  if (mouse)
                     return mouse->dlt_x;
                  break;
               case RETRO_DEVICE_ID_LIGHTGUN_Y:
                  if (mouse)
                     return mouse->dlt_y;
                  break;
            }
            break;
      }
   }

   return 0;
}

#if !defined(_XBOX)
bool winraw_handle_message(UINT msg,
      WPARAM wpar, LPARAM lpar)
{
   switch (msg)
   {
      case WM_SETFOCUS:
         winraw_focus = true;
         break;
      case WM_KILLFOCUS:
         winraw_focus = false;
         break;

      case WM_DEVICECHANGE:
#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0500 /* 2K */
         if (   (wpar == DBT_DEVICEARRIVAL)
             || (wpar == DBT_DEVICEREMOVECOMPLETE))
         {
            PDEV_BROADCAST_HDR pHdr = (PDEV_BROADCAST_HDR)lpar;
            if (pHdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
               win32_hotplug_arm();
         }
#endif
         break;
      case WM_TIMER:
         if (wpar != WIN32_HOTPLUG_TIMER_ID)
            break;
         if (win32_hotplug_due())
         {
            settings_t *settings = config_get_ptr();
            /* and the input driver's own list of keyboards, at its
             * next poll */
            retro_atomic_store_release_int(&winraw_devices_changed, 1);
            /* Name the joypad driver to reinitialise. Passing NULL
             * here makes input_joypad_init_driver() skip the branch
             * that honours the configured driver - it is guarded by
             * 'if (ident && *ident)' - and fall through to
             * input_joypad_init_first(), which takes whichever entry
             * of joypad_drivers[] initialises first. The configured
             * driver is never tried, so a device change silently
             * swaps it for one earlier in that list. */
            joypad_driver_reinit(NULL,
                  settings ? settings->arrays.input_joypad_driver : NULL);
         }
         return true;
   }
   return false;
}
#endif

static void winraw_free(void *data)
{
   RAWINPUTDEVICE rid;
   winraw_input_t *wr         = (winraw_input_t*)data;
   bool input_nowinkey_enable = config_get_ptr()->bools.input_nowinkey_enable;

   if (wr->poll_drain)
   {
      if (wr->window)
      {
         rid.dwFlags     = RIDEV_REMOVE;
         rid.hwndTarget  = NULL;
         rid.usUsagePage = 0x01; /* generic desktop */
         rid.usUsage     = 0x02; /* mouse */
         RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE));
         rid.usUsage     = 0x06; /* Keyboard */
         RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE));

         SetWindowLongPtr(wr->window, GWLP_USERDATA, 0);
         /* A window is destroyed by the thread that made it. That is
          * the polling thread, which is also the one that frees the
          * driver; if it ever is not, the window is asked to close
          * and goes when its own thread next pumps. */
         if (GetCurrentThreadId() == wr->window_tid)
         {
            DestroyWindow(wr->window);
            UnregisterClassA("winraw-input", NULL);
         }
         else
            PostMessageA(wr->window, WM_CLOSE, 0, 0);
         winraw_queue_claim_thread(false);
      }
      if (winraw_drain_wr == wr)
         winraw_drain_wr = NULL;
      RARCH_DBG("[WinRaw] Read by the poll: %lu keyboard and mouse reports"
            " and %lu of controllers in %lu bulk reads, %lu as messages;"
            " %lu reads found nothing waiting (%lu key events dropped).\n",
            wr->drained, wr->drained_hid, wr->drain_reads, wr->by_message,
            wr->drain_empty, wr->kev_dropped);
      free(g_mice);
      free(wr->mice);
      free(data);
      return;
   }

   rid.dwFlags          = RIDEV_REMOVE;
   rid.hwndTarget       = NULL;
   rid.usUsagePage      = 0x01; /* generic desktop */
   rid.usUsage          = 0x02; /* mouse */

   RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE));

   rid.dwFlags          = RIDEV_REMOVE;
   rid.hwndTarget       = NULL;
   rid.usUsagePage      = 0x01; /* Generic desktop */
   rid.usUsage          = 0x06; /* Keyboard */
   if (input_nowinkey_enable)
      rid.dwFlags      |= RIDEV_NOHOTKEYS; /* Disable win keys while focused */

   RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE));

   SetWindowLongPtr(wr->window, GWLP_USERDATA, 0);
   if (wr->window)
   {
      DestroyWindow(wr->window);
      UnregisterClassA("winraw-input", NULL);
   }
   winraw_legacy_tid = 0;
   if (wr->stale_polls)
      RARCH_DBG("[WinRaw] Taken by the window's thread: at the poll,"
            " what that thread had taken was %.2f ms old on average and"
            " %.2f ms at the most, over %lu polls. Read by the poll,"
            " which is the default, that wait is not there.\n",
            (double)wr->stale_sum / (double)wr->stale_polls / 1000.0,
            (double)wr->stale_max / 1000.0, wr->stale_polls);
   free(g_mice);
   free(wr->mice);

   free(data);
}

static uint64_t winraw_get_capabilities(void *u)
{
   return   (1 << RETRO_DEVICE_KEYBOARD)
          | (1 << RETRO_DEVICE_MOUSE)
          | (1 << RETRO_DEVICE_JOYPAD)
          | (1 << RETRO_DEVICE_ANALOG)
          | (1 << RETRO_DEVICE_POINTER)
          | (1 << RETRO_DEVICE_LIGHTGUN);
}

static void winraw_grab_mouse(void *d, bool state)
{
   RAWINPUTDEVICE rid;
   winraw_input_t *wr = (winraw_input_t*)d;
   bool curr_state    = (wr->flags & WRAW_INP_FLG_MOUSE_GRAB) > 0;

   if (curr_state == state)
      return;

   /* read by the poll, and its first poll has not made the window
    * yet: there is nothing to register again */
   if (wr->poll_drain && !wr->window)
   {
      if (state)
         wr->flags |=  WRAW_INP_FLG_MOUSE_GRAB;
      else
         wr->flags &= ~WRAW_INP_FLG_MOUSE_GRAB;
#ifndef _XBOX
      win32_clip_window(state);
#endif
      return;
   }

   rid.dwFlags        = (wr->window)
      ? (wr->sink ? RIDEV_INPUTSINK : 0) : RIDEV_REMOVE;
   rid.hwndTarget     = wr->window;
   rid.usUsagePage    = 0x01; /* generic desktop */
   rid.usUsage        = 0x02; /* mouse */

   if (RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE)))
   {
      if (state)
         wr->flags |=  WRAW_INP_FLG_MOUSE_GRAB;
      else
         wr->flags &= ~WRAW_INP_FLG_MOUSE_GRAB;
#ifndef _XBOX
      win32_clip_window(state);
#endif
   }
}

/* Left running across a video driver restart
 * (input_driver_t::survives_video) when it is read by the poll. Its
 * window is then the polling thread's, not the video driver's
 * thread's; it looks the main window up afresh at every poll; and the
 * two things it used to pick up only by being restarted - a mouse
 * plugged in, the Windows-key setting - it does itself
 * (winraw_mice_refresh(), winraw_nowinkey_apply()). The joypad driver
 * is kept with it, so it has to be one that can stay too: winraw_joypad
 * read by the poll. With any other joypad driver, or with
 * RETROARCH_RAWINPUT_POLL=0, the driver is restarted with the video
 * driver as it always was. */
extern bool winraw_joypad_survives_video(void);

static bool winraw_survives_video(void *data)
{
   winraw_input_t *wr = (winraw_input_t*)data;
   return wr && wr->poll_drain && winraw_joypad_survives_video();
}

input_driver_t input_winraw = {
   winraw_init,
   winraw_poll,
   winraw_input_state,
   winraw_free,
   NULL,
   NULL,
   winraw_get_capabilities,
   "raw",
   winraw_grab_mouse,
   NULL,
   NULL,
   winraw_survives_video
};
