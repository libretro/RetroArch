"""Check actual Win32 resolution-switch results without changing a display."""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument("--cc", default=os.environ.get("CC", "cc"))
p.add_argument("--runner", default="")
p.add_argument("--sanitizer", default="")
a = p.parse_args()
root = Path(__file__).resolve().parents[3]
source = (root / "gfx/display_servers/dispserv_win32.c").read_text(encoding="utf-8")
start = source.index("static bool win32_display_server_set_resolution(")
end = source.index("/* Display resolution list qsort", start)
unit = r"""
#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int bool;
#define true 1
#define false 0
typedef unsigned long DWORD;
typedef long LONG;
typedef void *HMONITOR;
#ifdef UNICODE
typedef wchar_t TCHAR;
#else
typedef char TCHAR;
#endif
typedef struct { TCHAR szDevice[32]; } MONITORINFOEX;
typedef struct
{
   unsigned dmPelsWidth, dmPelsHeight, dmBitsPerPel, dmDisplayFrequency;
   unsigned dmDisplayOrientation, dmDisplayFixedOutput, dmFields, dmDisplayFlags;
   struct { long x, y; } dmPosition;
} DEVMODE;
typedef struct win32_display_mode
{
   struct win32_display_mode *next;
   DEVMODE original, applied;
   TCHAR device[32];
} win32_display_mode_t;
typedef struct { win32_display_mode_t *modes; } dispserv_win32_t;
#define VIDEO_SCALE_PACK(w, h) (((w) << 16) | (h))
#define VIDEO_SCALE_W(d) ((d) >> 16)
#define VIDEO_SCALE_H(d) ((d) & 65535)
#define VIDEO_SCALE_PUT_W(d, w) ((d) = VIDEO_SCALE_PACK(w, VIDEO_SCALE_H(d)))
#define VIDEO_SCALE_PUT_H(d, h) ((d) = VIDEO_SCALE_PACK(VIDEO_SCALE_W(d), h))
#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#define DMDFO_DEFAULT 0
#define DM_INTERLACED 2
#define DM_PELSWIDTH 1
#define DM_PELSHEIGHT 2
#define DM_BITSPERPEL 4
#define DM_DISPLAYFREQUENCY 8
#define DM_DISPLAYORIENTATION 16
#define CDS_TEST 2
#define DISP_CHANGE_SUCCESSFUL 0
#define DISP_CHANGE_RESTART 1
#define DISP_CHANGE_FAILED -1
#define DISP_CHANGE_NOTUPDATED -3
static int current_ok = 1, test_result, apply_result;
static unsigned tests, applies, enumerated, monitors;
static int monitor_ok = 1, fail_alloc;
static TCHAR selected = 'D';
static DEVMODE current_modes[2];
static void reset_modes(void)
{
   unsigned i;
   memset(current_modes, 0, sizeof(current_modes));
   for (i = 0; i < 2; i++)
   {
      current_modes[i].dmPelsWidth = 1920;
      current_modes[i].dmPelsHeight = 1080;
      current_modes[i].dmBitsPerPel = 32;
      current_modes[i].dmDisplayFrequency = i ? 75 : 60;
   }
   selected = 'D';
   monitor_ok = current_ok = 1;
   fail_alloc = 0;
}
static bool win32_get_named_video_output(DEVMODE *dm, int mode,
      const TCHAR *device)
{
   memset(dm, 0, sizeof(*dm));
   if (!*device) return false;
   assert(device[0] == 'D' || device[0] == 'E');
   if (mode == -1 && !current_ok) return false;
   if (mode >= 0)
   {
      enumerated++;
      if (mode >= 2) return false;
   }
   *dm = current_modes[device[0] == 'E'];
   if (mode >= 0)
      dm->dmDisplayFrequency = mode == 0 ? 120 : 60;
   return true;
}
static void win32_monitor_info(void *data, void *monitor, unsigned *id)
{
   MONITORINFOEX *info = (MONITORINFOEX *)data;
   memset(info, 0, sizeof(*info));
   monitors++;
   if (monitor_ok)
      info->szDevice[0] = selected;
   selected = 'E';
   *(HMONITOR *)monitor = info;
   *id = 0;
}
static int win32_change_display_settings(const char *name, void *mode,
      unsigned flags)
{
   const DEVMODE *dm = (const DEVMODE *)mode;
   const TCHAR *device = (const TCHAR *)name;
   assert(name && (device[0] == 'D' || device[0] == 'E'));
   if (flags == CDS_TEST)
   {
      tests++;
      return test_result;
   }
   assert(!flags);
   applies++;
   if (apply_result == DISP_CHANGE_SUCCESSFUL)
      current_modes[device[0] == 'E'] = *dm;
   return apply_result;
}
"""
match_start = source.index("static bool win32_display_mode_matches(")
match_end = source.index("static bool win32_get_video_output_info(", match_start)
unit += r"""
static void *record_alloc(size_t n, size_t size)
{
   return fail_alloc ? NULL : calloc(n, size);
}
#define calloc record_alloc
"""
unit += source[match_start:match_end]
display_header = (root / "gfx/video_display_server.h").read_text(encoding="utf-8")
config_start = display_header.index("typedef struct video_display_config\n")
config_end = display_header.index("} video_display_config_t;", config_start) + len("} video_display_config_t;")
unit += display_header[config_start:config_end] + "\n"
selector = (root / "gfx/gfx_media_display.c").read_text(encoding="utf-8")
rate_start = selector.index("float gfx_media_display_rate(")
rate_end = selector.index("\n}", rate_start) + 2
unit += selector[rate_start:rate_end] + "\n"
unit += source[start:end]
unit += r"""
int main(void)
{
   unsigned i;
   for (i = 0; i < 9; i++)
   {
      dispserv_win32_t serv;
      bool result;
      unsigned hz = 120;
      memset(&serv, 0, sizeof(serv));
      reset_modes();
      tests = applies = enumerated = monitors = 0;
      monitor_ok = 1;
      current_ok = 1;
      test_result = apply_result = DISP_CHANGE_SUCCESSFUL;
      switch (i)
      {
         case 1: hz = 75; break;
         case 2: current_ok = 0; break;
         case 3: test_result = DISP_CHANGE_FAILED; break;
         case 4: test_result = DISP_CHANGE_RESTART; break;
         case 5: apply_result = DISP_CHANGE_FAILED; break;
         case 6: apply_result = DISP_CHANGE_NOTUPDATED; break;
         case 7: monitor_ok = 0; break;
         case 8: fail_alloc = 1; break;
      }
      result = win32_display_server_set_resolution(&serv,
            VIDEO_SCALE_PACK(1920, 1080), hz, (float)hz, 0, 0, 0, 0);
      assert(result == (i == 0));
      assert(monitors == 1);
      assert(applies == (i == 0 || i == 5 || i == 6));
      if (i == 2) assert(!tests && !enumerated);
      if (i == 1) assert(!tests && enumerated == 3);
      test_result = apply_result = DISP_CHANGE_SUCCESSFUL;
      win32_display_server_restore_modes(&serv);
      assert(!serv.modes && current_modes[0].dmDisplayFrequency == 60);
   }
   /* Two changed outputs restore independently after the window moves. */
   {
      dispserv_win32_t serv;
      unsigned before;
      memset(&serv, 0, sizeof(serv));
      reset_modes();
      monitors = 0;
      assert(win32_display_server_set_resolution(&serv, 0, 120, 120, 0, 0, 0, 0));
      assert(win32_display_server_set_resolution(&serv, 0, 120, 120, 0, 0, 0, 0));
      assert(serv.modes && serv.modes->next);
      before = monitors;
      win32_display_server_restore_modes(&serv);
      assert(monitors == before && !serv.modes);
      assert(current_modes[0].dmDisplayFrequency == 60);
      assert(current_modes[1].dmDisplayFrequency == 75);
   }
   /* Repeated switches retain the first original mode; external changes
    * and disconnect end ownership rather than being overwritten. */
   for (i = 0; i < 4; i++)
   {
      dispserv_win32_t serv;
      unsigned before;
      memset(&serv, 0, sizeof(serv));
      reset_modes();
      assert(win32_display_server_set_resolution(&serv, 0, 120, 120, 0, 0, 0, 0));
      selected = 'D';
      assert(win32_display_server_set_resolution(&serv, 0, 60, 60, 0, 0, 0, 0));
      assert(serv.modes && !serv.modes->next);
      assert(serv.modes->original.dmDisplayFrequency == 60);
      if (i == 1) current_modes[0].dmDisplayFrequency = 144;
      if (i == 2) current_ok = 0;
      if (i == 3) current_modes[0].dmDisplayFlags = 2;
      before = applies;
      win32_display_server_restore_modes(&serv);
      assert(!serv.modes);
      assert(applies == before + (i == 0));
      if (i == 1) assert(current_modes[0].dmDisplayFrequency == 144);
      current_ok = 1;
   }
   {
      dispserv_win32_t serv;
      memset(&serv, 0, sizeof(serv));
      reset_modes();
      assert(win32_display_server_set_resolution(&serv, 0, 120, 120, 0, 0, 0, 0));
      current_modes[0].dmDisplayFrequency = 144;
      selected = 'D';
      assert(win32_display_server_set_resolution(&serv, 0, 120, 120, 0, 0, 0, 0));
      assert(serv.modes->original.dmDisplayFrequency == 144);
      win32_display_server_restore_modes(&serv);
      assert(!serv.modes && current_modes[0].dmDisplayFrequency == 144);
   }
   assert(!win32_display_server_set_resolution(NULL, 0, 0, 0, 0, 0, 0, 0));
   puts("PASS: mode results and conditional restoration stay bound to each output");

   for (i = 0; i < 9; i++)
   {
      dispserv_win32_t serv;
      void *lease;
      unsigned num = 24;
      memset(&serv, 0, sizeof(serv));
      reset_modes();
      tests = applies = 0;
      test_result = apply_result = DISP_CHANGE_SUCCESSFUL;
      switch (i)
      {
         case 1: num = 0; break;
         case 2: current_ok = 0; break;
         case 3: fail_alloc = 1; break;
         case 4: test_result = DISP_CHANGE_FAILED; break;
         case 5: apply_result = DISP_CHANGE_FAILED; break;
         case 6: monitor_ok = 0; break;
         case 7: current_modes[0].dmDisplayFrequency = 120; break;
         case 8: num = 241; break;
      }
      lease = win32_display_server_begin_media_refresh(&serv, num, 1);
      assert((lease != NULL) == (i == 0));
      assert(applies == (i == 0 || i == 5));
      if (lease)
      {
         selected = 'D';
         assert(win32_display_server_media_refresh_current(&serv, lease, 24, 1));
         selected = 'D';
         assert(!win32_display_server_media_refresh_current(&serv, lease, 25, 1));
         selected = 'E';
         assert(!win32_display_server_media_refresh_current(&serv, lease, 24, 1));
         win32_display_server_end_media_refresh(&serv, lease);
         assert(current_modes[0].dmDisplayFrequency == 60);
         assert(current_modes[1].dmDisplayFrequency == 75);
      }
   }
   for (i = 0; i < 4; i++)
   {
      dispserv_win32_t serv;
      void *lease;
      unsigned before;
      memset(&serv, 0, sizeof(serv));
      reset_modes();
      test_result = apply_result = DISP_CHANGE_SUCCESSFUL;
      lease = win32_display_server_begin_media_refresh(&serv, 24, 1);
      assert(lease);
      if (i == 1) current_modes[0].dmDisplayFrequency = 144;
      if (i == 2) current_ok = 0;
      if (i == 3) current_modes[0].dmDisplayFlags = DM_INTERLACED;
      before = applies;
      win32_display_server_end_media_refresh(&serv, lease);
      assert(applies == before + (i == 0));
      if (i == 1) assert(current_modes[0].dmDisplayFrequency == 144);
   }
   {
      dispserv_win32_t serv;
      void *lease;
      memset(&serv, 0, sizeof(serv));
      reset_modes();
      test_result = apply_result = DISP_CHANGE_SUCCESSFUL;
      current_modes[0].dmDisplayFrequency = 75;
      assert(win32_display_server_set_resolution(&serv, 0, 60, 60, 0, 0, 0, 0));
      selected = 'D';
      lease = win32_display_server_begin_media_refresh(&serv, 24, 1);
      assert(lease && current_modes[0].dmDisplayFrequency == 120);
      win32_display_server_end_media_refresh(&serv, lease);
      assert(current_modes[0].dmDisplayFrequency == 60);
      win32_display_server_restore_modes(&serv);
      assert(!serv.modes && current_modes[0].dmDisplayFrequency == 75);
   }
   return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="win32-resolution-result-") as temp:
    src = Path(temp) / "result.c"
    src.write_text(unit, encoding="utf-8")
    for version in ("0x0400", "0x0601"):
        for unicode in (False, True):
            suffix = ".exe" if os.name == "nt" or a.runner else ""
            exe = Path(temp) / ("result-" + version + str(unicode) + suffix)
            flags = ["-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror",
                     "-Wno-unused-parameter", "-Wframe-larger-than=2048",
                     "-D_WIN32_WINNT=" + version]
            if unicode:
                flags.append("-DUNICODE")
            if a.sanitizer:
                flags += ["-fsanitize=" + a.sanitizer, "-fno-omit-frame-pointer"]
            subprocess.run(shlex.split(a.cc) + flags + [str(src), "-o", str(exe), "-lm"],
                           check=True)
            subprocess.run(shlex.split(a.runner) + [str(exe)], check=True)
