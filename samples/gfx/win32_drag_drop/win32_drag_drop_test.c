/* Drag and drop on every Windows generation RetroArch runs on, through
 * the setup in frontend/drivers/platform_win32.c and the drop handler
 * in ui/drivers/ui_win32.c, both compiled from their shipping text.
 *
 *   win7   dwmapi, ChangeWindowMessageFilterEx, DragQueryFileW
 *   vista  dwmapi, process-wide ChangeWindowMessageFilter only
 *   xp     no dwmapi, no message filter
 *   win9x  no dwmapi, no message filter, no DragQueryFileW
 *   wstub  as win9x, but DragQueryFileW is exported and returns 0
 *
 * Drag and drop must be switched on whether or not DWM exists, DWM
 * setup must run once however often it is asked, an elevated window
 * must let the drop messages through where Windows can filter them,
 * and the dropped files must reach the menu - or, if the menu does not
 * take them, the first one must be loaded - through whichever query
 * function the system has. */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "fake_win32.h"

enum system
{
   SYS_WIN7 = 0,
   SYS_VISTA,
   SYS_XP,
   SYS_WIN9X,
   SYS_WSTUB
};

static enum system sys;
static int  shell32_token, user32_token, dwm_token, window_token;
static int  atexit_calls;
static int  mmcss_calls;
static int  accept_calls;
static BOOL accept_last;
static HWND accept_hwnd;
static int  filter_ex_calls, filter_calls;
static UINT filter_msgs[8];
static int  query_w_calls, query_a_calls;
static bool menu_takes;
static char menu_got[2][64];
static int  menu_got_count;
static char loaded[64];
static int  failures;

static const char *drop_files[2] = { "C:\\roms\\a.zip", "C:\\roms\\b.zip" };

VOID (WINAPI *DragAcceptFiles_func)(HWND, BOOL);

static void expect(int ok, const char *what)
{
   if (!ok)
   {
      printf("FAIL: %s\n", what);
      failures++;
   }
}

/* --- fake system ---------------------------------------------------- */

static HRESULT WINAPI fake_dwm_enable_mmcss(BOOL on)
{
   (void)on;
   mmcss_calls++;
   return 0;
}

static VOID WINAPI fake_drag_accept_files(HWND hwnd, BOOL accept)
{
   accept_calls++;
   accept_hwnd = hwnd;
   accept_last = accept;
}

static BOOL WINAPI fake_filter_ex(HWND hwnd, UINT msg, DWORD action, void *info)
{
   if (hwnd != &window_token || action != 1 || info)
      return 0;
   if (filter_ex_calls + filter_calls < 8)
      filter_msgs[filter_ex_calls + filter_calls] = msg;
   filter_ex_calls++;
   return 1;
}

static BOOL WINAPI fake_filter(UINT msg, DWORD action)
{
   if (action != 1)
      return 0;
   if (filter_ex_calls + filter_calls < 8)
      filter_msgs[filter_ex_calls + filter_calls] = msg;
   filter_calls++;
   return 1;
}

static UINT WINAPI fake_query_w(HDROP drop, UINT index, LPWSTR buf, UINT len)
{
   size_t i;
   query_w_calls++;
   if (sys == SYS_WSTUB)
      return 0;
   if (index == 0xFFFFFFFF)
      return 2;
   if (index > 1 || !buf || !len)
      return 0;
   for (i = 0; drop_files[index][i] && i + 1 < len; i++)
      buf[i] = (wchar_t)drop_files[index][i];
   buf[i] = 0;
   return (UINT)i;
}

UINT DragQueryFileA(HDROP drop, UINT index, char *buf, UINT len)
{
   query_a_calls++;
   if (index == 0xFFFFFFFF)
      return 2;
   if (index > 1 || !buf || !len)
      return 0;
   strncpy(buf, drop_files[index], len - 1);
   buf[len - 1] = '\0';
   return (UINT)strlen(buf);
}

HMODULE GetModuleHandleA(const char *name)
{
   if (!strcmp(name, "shell32.dll"))
      return &shell32_token;
   if (!strcmp(name, "user32.dll"))
      return &user32_token;
   return NULL;
}

FARPROC GetProcAddress(HMODULE module, const char *name)
{
   if (module == &shell32_token)
   {
      if (!strcmp(name, "DragAcceptFiles"))
         return (FARPROC)fake_drag_accept_files;
      if (!strcmp(name, "DragQueryFileW") && sys != SYS_WIN9X)
         return (FARPROC)fake_query_w;
   }
   if (module == &user32_token)
   {
      if (!strcmp(name, "ChangeWindowMessageFilterEx") && sys == SYS_WIN7)
         return (FARPROC)fake_filter_ex;
      if (!strcmp(name, "ChangeWindowMessageFilter")
            && (sys == SYS_WIN7 || sys == SYS_VISTA))
         return (FARPROC)fake_filter;
   }
   return NULL;
}

dylib_t dylib_load(const char *path)
{
   if (!strcmp(path, "dwmapi.dll") && (sys == SYS_WIN7 || sys == SYS_VISTA))
      return &dwm_token;
   return NULL;
}

function_t dylib_proc(dylib_t lib, const char *proc)
{
   if (lib == &dwm_token && !strcmp(proc, "DwmEnableMMCSS"))
      return (function_t)fake_dwm_enable_mmcss;
   return NULL;
}

void dylib_close(dylib_t lib) { (void)lib; }

int fake_atexit(void (*fn)(void))
{
   (void)fn;
   atexit_calls++;
   return 0;
}

void fake_log(const char *fmt, ...) { (void)fmt; }

bool local_to_utf8_string(const char *in, char *s, size_t len)
{
   if (!len || strlen(in) >= len)
      return false;
   strcpy(s, in);
   return true;
}

char *utf16_to_utf8_string_alloc(const wchar_t *str)
{
   size_t i, n = 0;
   char *out;
   while (str[n])
      n++;
   if (!(out = (char*)malloc(n + 1)))
      return NULL;
   for (i = 0; i < n; i++)
      out[i] = (char)str[i];
   out[n] = '\0';
   return out;
}

bool menu_driver_drop(struct string_list *files)
{
   size_t i;
   menu_got_count = (int)files->size;
   for (i = 0; i < files->size && i < 2; i++)
   {
      strncpy(menu_got[i], files->elems[i].data, sizeof(menu_got[i]) - 1);
      menu_got[i][sizeof(menu_got[i]) - 1] = '\0';
   }
   string_list_free(files);
   return menu_takes;
}

bool win32_load_content_from_gui(const char *path)
{
   strncpy(loaded, path ? path : "", sizeof(loaded) - 1);
   loaded[sizeof(loaded) - 1] = '\0';
   return true;
}

/* --- the shipping code ---------------------------------------------- */

static dylib_t dwm_lib;
#include "platform_sim.c"
#include "ui_sim.c"

/* --- checks ---------------------------------------------------------- */

static void check_setup(void)
{
   bool has_dwm   = (sys == SYS_WIN7 || sys == SYS_VISTA);
   bool first     = gfx_init_dwm();
   bool second    = gfx_init_dwm();
   int filters;

   expect(first == has_dwm, "gfx_init_dwm reports whether DWM exists");
   expect(second == first, "a second gfx_init_dwm reports the same");
   expect(atexit_calls == 1, "DWM teardown is registered once");
   expect(mmcss_calls == (has_dwm ? 1 : 0), "MMCSS is asked for once, with DWM");
   expect(DragAcceptFiles_func != NULL,
         "drag and drop is switched on with or without DWM");
   if (!DragAcceptFiles_func)
      return;

   DragAcceptFiles_func(&window_token, 1);
   expect(accept_calls == 1 && accept_last && accept_hwnd == &window_token,
         "shell32 accepts files on the window");

   filters = filter_ex_calls + filter_calls;
   if (sys == SYS_WIN7)
      expect(filter_ex_calls == 3 && filter_calls == 0,
            "Windows 7 lets the drop messages through to the window");
   else if (sys == SYS_VISTA)
      expect(filter_calls == 3 && filter_ex_calls == 0,
            "Vista lets the drop messages through to the process");
   else
      expect(filters == 0, "no message filter where Windows has none");
   if (filters == 3)
      expect(filter_msgs[0] == 0x0233 && filter_msgs[1] == 0x004A
            && filter_msgs[2] == 0x0049,
            "the filtered messages are WM_DROPFILES, WM_COPYDATA, WM_COPYGLOBALDATA");

   DragAcceptFiles_func(&window_token, 0);
   expect(accept_calls == 2 && !accept_last, "shell32 stops accepting files");
   expect(filter_ex_calls + filter_calls == filters,
         "turning drops off does not touch the filter");
}

static void check_drop(void)
{
   bool ansi = (sys == SYS_WIN9X || sys == SYS_WSTUB);

   menu_takes = true;
   expect(win32_drag_query_file(&window_token, 1), "the menu takes the drop");
   expect(menu_got_count == 2, "both dropped files reach the menu");
   expect(!strcmp(menu_got[0], drop_files[0])
         && !strcmp(menu_got[1], drop_files[1]),
         "the menu gets the dropped paths in order");
   expect(!loaded[0], "nothing is loaded when the menu takes the drop");
   if (ansi)
      expect(query_a_calls > 0, "the ANSI query reads the drop");
   else
      expect(query_w_calls > 0 && query_a_calls == 0,
            "the Unicode query reads the drop");

   menu_takes = false;
   expect(win32_drag_query_file(&window_token, 1),
         "content loads when the menu declines");
   expect(!strcmp(loaded, drop_files[0]), "the first dropped file is loaded");
}

int main(int argc, char **argv)
{
   static const char *names[] = { "win7", "vista", "xp", "win9x", "wstub" };
   size_t i;

   if (argc < 2)
      return 2;
   for (i = 0; i < ARRAY_SIZE(names); i++)
      if (!strcmp(argv[1], names[i]))
         break;
   if (i == ARRAY_SIZE(names))
      return 2;
   sys = (enum system)i;

   check_setup();
   check_drop();

   printf("%s: %s\n", argv[1], failures ? "FAILED" : "ok");
   return failures ? 1 : 0;
}
