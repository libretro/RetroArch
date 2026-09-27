/* winraw: an absolute mouse report reaches poll as one position.
 *
 * The report handler runs on the thread that owns the window and
 * winraw_poll() on the main thread. A report's x and y are published
 * together, so poll reads a position one report gave - never one
 * report's x with the next report's y.
 *
 * Deterministic rather than a race: the driver's acquire loads are
 * routed through a hook here, and the first one poll makes in the
 * absolute branch delivers a second report before poll's next load.
 * Whatever poll then settles on must be a position that was reported.
 *
 * Also checked: positions round-trip, 0 and the edge of an 8K
 * viewport included. */
#include <stdio.h>
#include <stdlib.h>

#include <retro_atomic.h>

static void (*load_hook)(void);

static int hooked_load_acquire_int(retro_atomic_int_t *p)
{
   int v = retro_atomic_load_acquire_int(p);
   if (load_hook)
   {
      void (*hook)(void) = load_hook;
      load_hook = NULL;
      hook();
   }
   return v;
}
#undef  retro_atomic_load_acquire_int
#define retro_atomic_load_acquire_int(p) hooked_load_acquire_int(p)

#include "input/drivers/winraw_input.c"

/* The frontend, as far as the driver links against it. */
uint8_t g_win32_flags;
ui_window_win32_t main_window;
retro_keybind_set input_config_binds[MAX_USERS];
retro_keybind_set input_autoconf_binds[MAX_USERS];
enum retro_key rarch_keysym_lut[RETROK_LAST];
const struct rarch_key_map rarch_key_map_winraw[] = { { 0, RETROK_UNKNOWN } };
static settings_t stub_settings;
static struct menu_state stub_menu;
settings_t *config_get_ptr(void) { return &stub_settings; }
struct menu_state *menu_state_get_ptr(void) { return &stub_menu; }
void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }
void input_config_set_mouse_display_name(unsigned port, const char *name)
{ (void)port; (void)name; }
unsigned input_driver_lightgun_id_convert(unsigned id) { return id; }
bool input_driver_pointer_is_offscreen(int16_t x, int16_t y)
{ (void)x; (void)y; return false; }
void input_keyboard_event(bool down, unsigned code, uint32_t character,
      uint16_t mod, unsigned device)
{ (void)down; (void)code; (void)character; (void)mod; (void)device; }
void input_keymaps_init_keyboard_lut(const struct rarch_key_map *map)
{ (void)map; }
enum retro_key input_keymaps_translate_keysym_to_rk(unsigned sym)
{ (void)sym; return RETROK_UNKNOWN; }
void joypad_driver_reinit(void *data, const char *name)
{ (void)data; (void)name; }
retro_task_t *task_init(void) { return NULL; }
bool task_queue_push(retro_task_t *task) { (void)task; return false; }
void task_set_flags(retro_task_t *task, uint8_t flags, bool set)
{ (void)task; (void)flags; (void)set; }
bool video_driver_get_viewport_info(struct video_viewport *vp)
{ (void)vp; return false; }
bool video_driver_translate_coord_viewport(struct video_viewport *vp,
      int mouse_x, int mouse_y, int16_t *res_x, int16_t *res_y,
      int16_t *res_screen_x, int16_t *res_screen_y, bool report_oob)
{
   (void)vp; (void)mouse_x; (void)mouse_y; (void)res_x; (void)res_y;
   (void)res_screen_x; (void)res_screen_y; (void)report_oob;
   return false;
}
uintptr_t video_driver_window_get(void) { return 0; }
void win32_clip_window(bool grab) { (void)grab; }
uint16_t win32_get_keyboard_mods(void) { return 0; }
void win32_hotplug_arm(void) { }
bool win32_hotplug_due(void) { return false; }

static unsigned failures;
static winraw_input_t *wr;

static void report(LONG x, LONG y)
{
   RAWMOUSE m;
   memset(&m, 0, sizeof(m));
   m.usFlags = MOUSE_MOVE_ABSOLUTE;
   m.lLastX  = x;
   m.lLastY  = y;
   winraw_update_mouse_state(wr, &g_mice[0], &m);
}

static void second_report(void) { report(3000, 4000); }

static void expect(LONG x, LONG y, const char *what)
{
   if (wr->mice[0].x != x || wr->mice[0].y != y)
   {
      printf("   FAIL %s: poll read (%ld, %ld), want (%ld, %ld)\n", what,
            (long)wr->mice[0].x, (long)wr->mice[0].y, (long)x, (long)y);
      failures++;
   }
   else
      printf("   ok   %s: (%ld, %ld)\n", what, (long)x, (long)y);
}

int main(void)
{
   static const LONG pos[][2] = {
      { 0, 0 }, { 1, 2 }, { 640, 360 }, { 7679, 4319 }, { 12, 4319 } };
   unsigned i;

   printf("winraw absolute position:\n");
   wr        = (winraw_input_t*)calloc(1, sizeof(*wr));
   g_mice    = (winraw_mouse_t*)calloc(1, sizeof(*g_mice));
   wr->mice  = (winraw_mouse_t*)calloc(1, sizeof(*wr->mice));
   wr->mouse_cnt        = 1;
   wr->flags           |= WRAW_INP_FLG_MOUSE_XY_MAPPING_READY;
   wr->view_abs_ratio_x = 1.0;
   wr->view_abs_ratio_y = 1.0;
   winraw_focus         = true;
   wr->last_focus       = true;

   for (i = 0; i < sizeof(pos) / sizeof(pos[0]); i++)
   {
      report(pos[i][0], pos[i][1]);
      winraw_poll(wr);
      expect(pos[i][0], pos[i][1], "round trip");
   }

   /* A report landing between poll's reads. */
   report(1000, 2000);
   load_hook = second_report;
   winraw_poll(wr);
   load_hook = NULL;
   if (   !(wr->mice[0].x == 1000 && wr->mice[0].y == 2000)
       && !(wr->mice[0].x == 3000 && wr->mice[0].y == 4000))
   {
      printf("   FAIL a report during poll: read (%ld, %ld), which no "
            "report gave\n", (long)wr->mice[0].x, (long)wr->mice[0].y);
      failures++;
   }
   else
      printf("   ok   a report during poll: read (%ld, %ld), as reported\n",
            (long)wr->mice[0].x, (long)wr->mice[0].y);

   /* And the next poll takes the report that landed. */
   winraw_poll(wr);
   expect(3000, 4000, "the next poll");

   if (failures)
   {
      printf("winraw absolute position: %u failure(s)\n", failures);
      return 1;
   }
   printf("winraw absolute position: every read is a reported position\n");
   return 0;
}
