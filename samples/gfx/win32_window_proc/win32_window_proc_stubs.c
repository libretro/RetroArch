/* Everything gfx/common/win32_common.c calls outside itself, as
 * stand-ins that record what they were asked. */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <windows.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <dynamic/dylib.h>

#include "configuration.h"
#include "verbosity.h"
#include "msg_hash.h"
#include "runloop.h"
#include "audio/audio_driver.h"
#include "gfx/video_driver.h"
#include "gfx/common/win32_common.h"
#include "gfx/common/vulkan_common.h"
#include "input/input_driver.h"
#include "menu/menu_driver.h"
#include "frontend/frontend_driver.h"
#include "ui/ui_companion_driver.h"
#include "ui/drivers/ui_win32.h"
#include "content.h"
#include "tasks/tasks_internal.h"

#include "win32_window_proc_trace.h"

/* ---- the trace ---- */
char   trace_buf[1 << 20];
size_t trace_len;

bool     trace_on;
unsigned stub_create_steps;

void trace(const char *fmt, ...)
{
   va_list ap;
   if (!trace_on || trace_len >= sizeof(trace_buf) - 512)
      return;
   va_start(ap, fmt);
   trace_len += vsnprintf(trace_buf + trace_len,
         sizeof(trace_buf) - trace_len, fmt, ap);
   va_end(ap);
}

/* what the input drivers' handlers answer, set by the script */
bool stub_input_takes;

/* ---- state the window code reads ---- */
static settings_t           stub_settings;
static video_driver_state_t stub_video_st;
static input_driver_state_t stub_input_st;
static struct menu_state    stub_menu_st;
static runloop_state_t      stub_runloop_st;
static unsigned             stub_language;

settings_t *config_get_ptr(void)                 { return &stub_settings; }
video_driver_state_t *video_state_get_ptr(void)  { return &stub_video_st; }
uint32_t input_driver_get_flags(void)            { return stub_input_st.flags; }
struct menu_state *menu_state_get_ptr(void)      { return &stub_menu_st; }
runloop_state_t *runloop_state_get_ptr(void)     { return &stub_runloop_st; }
frontend_ctx_driver_t *frontend_get_ptr(void)    { return NULL; }
unsigned *msg_hash_get_uint(enum msg_hash_action type) { return &stub_language; }
const char *msg_hash_to_str(enum msg_hash_enums msg)   { return ""; }

void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)  { (void)fmt; }
void RARCH_DBG(const char *fmt, ...)  { (void)fmt; }

/* ---- input ---- */
volatile bool g_dinput_enum_inflight = false;

void input_keyboard_event(bool down, unsigned code,
      uint32_t character, uint16_t mod, unsigned device)
{
   /* mod is the live modifier state of the machine the test runs on;
    * it is the same call either way, so it stays out of the trace */
   trace("  keyboard_event down=%d code=%u char=0x%x device=%u\n",
         (int)down, code, (unsigned)character, device);
}

unsigned input_keymaps_translate_keysym_to_rk(unsigned sym)
{
   return 1000 + sym;
}

bool dinput_handle_message(void *dinput, UINT message,
      WPARAM wParam, LPARAM lParam)
{
   trace("  dinput_handle_message msg=0x%04x wparam=0x%llx -> %d\n",
         (unsigned)message, (unsigned long long)wParam,
         (int)stub_input_takes);
   return stub_input_takes;
}

bool winraw_handle_message(UINT message, WPARAM wParam, LPARAM lParam)
{
   trace("  winraw_handle_message msg=0x%04x wparam=0x%llx -> %d\n",
         (unsigned)message, (unsigned long long)wParam,
         (int)stub_input_takes);
   return stub_input_takes;
}

/* ---- audio, video ---- */
bool audio_driver_start(bool is_shutdown) { trace("  audio_driver_start\n"); return true; }
bool audio_driver_stop(void)              { trace("  audio_driver_stop\n");  return true; }

void video_driver_cached_frame(void)                         { trace("  video_driver_cached_frame\n"); }
void video_driver_display_set(uintptr_t idx)                 { }
void video_driver_display_type_set(enum rarch_display_type t){ }
void video_driver_display_userdata_set(uintptr_t idx)        { }
void video_driver_window_set(uintptr_t idx)                  { }
void video_driver_window_output_changed(void)                { trace("  window_output_changed\n"); }
bool video_driver_is_threaded(void)                          { return false; }
/* A window kept for the next video driver (win32_window_keep): this
 * test drives the window procedure on the one thread that made the
 * window, so there is no video thread to hold or to stop. */
uint32_t runloop_get_flags(void)                             { return stub_runloop_st.flags; }
bool task_is_on_main_thread(void)                            { return true; }
bool video_thread_host_hold(void (*on_exit)(void))           { return false; }
bool video_thread_host_is_held(void)                         { return false; }
void video_thread_host_stop(void)                            { }
void *video_driver_get_ptr(void)                             { return NULL; }
bool dxgi_display_hdr_active(HWND hwnd)                      { return false; }

/* the video families' own creation steps */
enum gfx_ctx_api      win32_api = GFX_CTX_OPENGL_API;
HDC                   win32_gdi_hdc;
gfx_ctx_vulkan_data_t win32_vk;
int                   win32_vk_interval;

void create_gl_context(HWND hwnd, bool *quit)
{
   stub_create_steps |= 1;
}

bool vulkan_surface_create(gfx_ctx_vulkan_data_t *vk,
      enum vulkan_wsi_type type, void *display, void *surface,
      unsigned dims, int8_t swap_interval)
{
   stub_create_steps |= 2;
   return true;
}

/* ---- the Win32 UI ---- */
VOID (WINAPI *DragAcceptFiles_func)(HWND, BOOL) = NULL;

static void stub_window_destroy(void *data)
{
   ui_window_win32_t *window = (ui_window_win32_t*)data;
   DestroyWindow(window->hwnd);
}

/* as ui/drivers/ui_win32.c has them */
static void stub_window_set_focused(void *data)
{
   ui_window_win32_t *window = (ui_window_win32_t*)data;
   SetFocus(window->hwnd);
}

static void stub_window_set_visible(void *data, bool visible)
{
   ui_window_win32_t *window = (ui_window_win32_t*)data;
   ShowWindow(window->hwnd, visible ? SW_SHOWNORMAL : SW_HIDE);
}

static void stub_window_set_title(void *data, char *buf)     { }
static void stub_window_set_droppable(void *data, bool drop) { }
static bool stub_window_focused(void *data)
{
   ui_window_win32_t *window = (ui_window_win32_t*)data;
   return GetForegroundWindow() == window->hwnd;
}
static void *stub_window_init(void) { return NULL; }

static ui_window_t stub_window = {
   stub_window_init, stub_window_destroy, stub_window_set_focused,
   stub_window_set_visible, stub_window_set_title,
   stub_window_set_droppable, stub_window_focused, "stub"
};

static void stub_process_events(void)
{
   MSG msg;
   while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
   {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
   }
}

static ui_application_t stub_application;

const ui_window_t *ui_companion_driver_get_window_ptr(void) { return &stub_window; }
ui_companion_driver_t ui_companion_win32;

/* the core's picture: the window code will not let its window be
 * smaller */
void stub_set_geometry(unsigned width, unsigned height)
{
   stub_video_st.av_info.geometry.base_width  = width;
   stub_video_st.av_info.geometry.base_height = height;
}

void stubs_init(void)
{
   /* a core's geometry: the window limits its size to twenty times
    * it, and with none the limit is too small to hold a window */
   stub_video_st.av_info.geometry.base_width  = 320;
   stub_video_st.av_info.geometry.base_height = 240;
   stub_application.process_events = stub_process_events;
   ui_companion_win32.application  = &stub_application;
   ui_companion_win32.window       = &stub_window;
}

bool win32_load_content_from_gui(const char *szFilename)   { return false; }
bool win32_drag_query_file(HWND hwnd, WPARAM wparam)       { trace("  drag_query_file\n"); return false; }
LRESULT win32_menu_loop(HWND owner, WPARAM wparam)
{
   trace("  menu_loop wparam=0x%llx\n", (unsigned long long)wparam);
   return 0;
}
void win32_localize_menu(HMENU menu)                        { }
HMENU win32_resources_create_menu(void)
{
   HMENU menu = CreateMenu();
   AppendMenuA(menu, MF_STRING, 1, "File");
   return menu;
}
unsigned short win32_get_langid_from_retro_lang(enum retro_language lang) { return 0x0409; }

bool task_push_load_new_core(const char *core_path, const char *fullpath,
      content_ctx_info_t *content_info, enum rarch_core_type type,
      retro_task_callback_t cb, void *cb_data) { return false; }

dylib_t dylib_load(const char *path)                    { return NULL; }
function_t dylib_proc(dylib_t lib, const char *proc)    { return NULL; }
void dylib_close(dylib_t lib)                           { }
