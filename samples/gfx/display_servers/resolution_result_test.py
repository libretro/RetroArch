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
#include <stddef.h>
#include <stdio.h>
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
   unsigned dmDisplayOrientation, dmDisplayFixedOutput, dmFields;
} DEVMODE;
typedef struct { unsigned orig_dims; float orig_refresh; } dispserv_win32_t;
#define VIDEO_SCALE_PACK(w, h) (((w) << 16) | (h))
#define VIDEO_SCALE_W(d) ((d) >> 16)
#define VIDEO_SCALE_H(d) ((d) & 65535)
#define VIDEO_SCALE_PUT_W(d, w) ((d) = VIDEO_SCALE_PACK(w, VIDEO_SCALE_H(d)))
#define VIDEO_SCALE_PUT_H(d, h) ((d) = VIDEO_SCALE_PACK(VIDEO_SCALE_W(d), h))
#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#define DMDFO_DEFAULT 0
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
static int monitor_ok = 1;
static float video_driver_get_refresh_rate(void) { return 60.0f; }
static int GetSystemMetrics(int which) { return which ? 1080 : 1920; }
static bool win32_get_named_video_output(DEVMODE *dm, int mode,
      const TCHAR *device)
{
   memset(dm, 0, sizeof(*dm));
   if (!*device) return false;
   assert(device[0] == 'D');
   if (mode == -1 && !current_ok) return false;
   if (mode >= 0)
   {
      enumerated++;
      if (mode >= 2) return false;
   }
   dm->dmPelsWidth = 1920;
   dm->dmPelsHeight = 1080;
   dm->dmBitsPerPel = 32;
   dm->dmDisplayFrequency = mode == 0 ? 120 : 60;
   return true;
}
static void win32_monitor_info(void *data, void *monitor, unsigned *id)
{
   MONITORINFOEX *info = (MONITORINFOEX *)data;
   memset(info, 0, sizeof(*info));
   monitors++;
   if (monitor_ok)
      info->szDevice[0] = monitors == 1 ? 'D' : 'E';
   *(HMONITOR *)monitor = info;
   *id = 0;
}
static int win32_change_display_settings(const char *name, void *mode,
      unsigned flags)
{
   const DEVMODE *dm = (const DEVMODE *)mode;
   assert(name && ((const TCHAR *)name)[0] == 'D'
         && dm->dmDisplayFrequency == 120);
   if (flags == CDS_TEST)
   {
      tests++;
      return test_result;
   }
   assert(!flags);
   applies++;
   return apply_result;
}
"""
unit += source[start:end]
unit += r"""
int main(void)
{
   unsigned i;
   for (i = 0; i < 8; i++)
   {
      dispserv_win32_t serv;
      bool result;
      unsigned hz = 120;
      memset(&serv, 0, sizeof(serv));
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
      }
      result = win32_display_server_set_resolution(&serv,
            VIDEO_SCALE_PACK(1920, 1080), hz, (float)hz, 0, 0, 0, 0);
      assert(result == (i == 0));
      assert(monitors == 1);
      assert(applies == (i == 0 || i == 5 || i == 6));
      if (i == 2) assert(!tests && !enumerated);
      if (i == 1) assert(!tests && enumerated == 3);
   }
   assert(!win32_display_server_set_resolution(NULL, 0, 0, 0, 0, 0, 0, 0));
   puts("PASS: resolution success requires a successful Windows mode application");
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
            subprocess.run(shlex.split(a.cc) + flags + [str(src), "-o", str(exe)],
                           check=True)
            subprocess.run(shlex.split(a.runner) + [str(exe)], check=True)
