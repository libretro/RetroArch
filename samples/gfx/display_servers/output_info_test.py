"""Exercise the shipping Win32 output snapshot without a Windows dependency."""
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


def function(signature):
    start = source.index(signature)
    end = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


unit = r'''
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
typedef int bool;
#define true 1
#define false 0
typedef unsigned long DWORD;
typedef void *HMONITOR;
#ifdef UNICODE
typedef wchar_t TCHAR;
#define LABEL(x) L##x
#else
typedef char TCHAR;
#define LABEL(x) x
#endif
typedef struct { TCHAR szDevice[32]; } MONITORINFOEX;
typedef struct { unsigned dmSize, dmPelsWidth, dmPelsHeight; } DEVMODE;
#define ENUM_CURRENT_SETTINGS ((DWORD)-1)
#define EDS_ROTATEDMODE 4
#define CP_UTF8 65001
#define VIDEO_SCALE_PACK(w, h) (((w) << 16) | (h))
static const TCHAR first[] = LABEL("\\\\.\\DISPLAY1");
static const TCHAR second[] = LABEL("\\\\.\\DISPLAY2");
static const TCHAR *selected = first;
static unsigned width = 1920, height = 1080, queries, monitors;
static int monitor_ok = 1, query_ok = 1, move_after_query;
static size_t strlcpy(char *dst, const char *src, size_t len)
{
   size_t size = strlen(src);
   if (len)
   {
      size_t n = size < len - 1 ? size : len - 1;
      memcpy(dst, src, n);
      dst[n] = '\0';
   }
   return size;
}
static void win32_monitor_info(void *data, void *hm, unsigned *id)
{
   MONITORINFOEX *mon = (MONITORINFOEX *)data;
   const TCHAR *p = selected;
   size_t i = 0;
   monitors++;
   assert(!mon->szDevice[0]);
   if (!monitor_ok) return;
   do { mon->szDevice[i++] = *p; } while (*p++);
   *(HMONITOR *)hm = mon;
   *id = 0;
}
static int settings(const TCHAR *name, DWORD mode, DEVMODE *dm)
{
   queries++;
   assert(name[0] && (mode == ENUM_CURRENT_SETTINGS || mode == 3));
   assert(dm->dmSize == sizeof(*dm));
   if (!query_ok) return 0;
   dm->dmPelsWidth = width;
   dm->dmPelsHeight = height;
   if (move_after_query)
   {
      selected = second;
      width = 2560;
      height = 1440;
      move_after_query = 0;
   }
   return 1;
}
#if _WIN32_WINNT >= 0x0500
static int EnumDisplaySettingsEx(const TCHAR *name, DWORD mode,
      DEVMODE *dm, unsigned flags)
{
   assert(flags == EDS_ROTATEDMODE);
   return settings(name, mode, dm);
}
#else
static int EnumDisplaySettings(const TCHAR *name, DWORD mode, DEVMODE *dm)
{
   return settings(name, mode, dm);
}
#endif
#ifdef UNICODE
static int WideCharToMultiByte(unsigned page, unsigned flags,
      const wchar_t *src, int n, char *dst, int capacity, void *a, void *b)
{
   int i;
   assert(page == CP_UTF8 && !flags && n == -1 && !a && !b);
   for (i = 0; i < capacity; i++)
   {
      assert(src[i] >= 0 && src[i] <= 127);
      dst[i] = (char)src[i];
      if (!src[i]) return i + 1;
   }
   return 0;
}
#endif
'''
unit += "\n".join(function(signature) for signature in (
    "static bool win32_get_named_video_output(",
    "static bool win32_get_video_output_info(",
    "static bool win32_get_video_output(",
    "static void win32_display_server_get_video_output_size("))
unit += r'''
int main(void)
{
   char name[64] = {0}, small[4] = "xyz";
   unsigned dims = 1, before;
   DEVMODE dm;
   move_after_query = 1;
   win32_display_server_get_video_output_size(NULL, &dims, name, sizeof(name));
   assert(dims == VIDEO_SCALE_PACK(1920, 1080));
   assert(!strcmp(name, "\\\\.\\DISPLAY1") && monitors == 1 && queries == 1);
   win32_display_server_get_video_output_size(NULL, &dims, name, sizeof(name));
   assert(dims == VIDEO_SCALE_PACK(2560, 1440));
   assert(!strcmp(name, "\\\\.\\DISPLAY2") && monitors == 2 && queries == 2);
   win32_display_server_get_video_output_size(NULL, NULL, small, sizeof(small));
   assert(!strncmp(name, small, 3) && !small[3]);
   small[0] = 'x';
   win32_display_server_get_video_output_size(NULL, NULL, small, 0);
   assert(small[0] == 'x');
   assert(win32_get_video_output(&dm, 3));
   query_ok = 0;
   win32_display_server_get_video_output_size(NULL, &dims, name, sizeof(name));
   assert(!dims && !name[0]);
   monitor_ok = 0;
   before = queries;
   dims = 1;
   name[0] = 'x';
   win32_display_server_get_video_output_size(NULL, &dims, name, sizeof(name));
   assert(!dims && !name[0] && queries == before);
   puts("PASS: output identity and dimensions share one monitor snapshot");
   return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="win32-output-info-") as temp:
    src = Path(temp) / "output.c"
    src.write_text(unit, encoding="utf-8")
    for version in ("0x0400", "0x0601"):
        for unicode in (False, True):
            suffix = ".exe" if os.name == "nt" or a.runner else ""
            exe = Path(temp) / ("output-" + version + str(unicode) + suffix)
            flags = ["-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror",
                     "-Wframe-larger-than=2048", "-D_WIN32_WINNT=" + version]
            if unicode:
                flags.append("-DUNICODE")
            if a.sanitizer:
                flags += ["-fsanitize=" + a.sanitizer, "-fno-omit-frame-pointer"]
            subprocess.run(shlex.split(a.cc) + flags + [str(src), "-o", str(exe)],
                           check=True)
            subprocess.run(shlex.split(a.runner) + [str(exe)], check=True)
