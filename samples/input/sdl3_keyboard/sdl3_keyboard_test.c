/* input/drivers/sdl3_input.c's key events, through its own poll.
 *
 * SDL is given a press and a release for every scancode the layout
 * gives a key RetroArch knows, and what reaches input_keyboard_event()
 * is checked against the shared table's answer for the unmodified key:
 *
 *  - a plain press and release name that key;
 *  - a press with shift held, released after shift, names the same
 *    key on both edges, so no key is left down.
 *
 * Built where SDL3 is (see the Makefile). */
#include <stdio.h>

#include "../../../input/drivers/sdl3_input.c"

/* What the driver reaches outside itself, none of it under test */
bool input_config_get_sdl3_system_keyboard(void) { return false; }
bool input_driver_keyboard_line_enabled(void) { return false; }
bool input_driver_poll_viewport(struct video_viewport *vp) { (void)vp; return false; }
void input_driver_publish_pointers(const input_pointer_frame_t *frames,
      unsigned count, unsigned flags) { (void)frames; (void)count; (void)flags; }
void input_driver_publish_touches(const uint32_t *pos, unsigned count,
      unsigned present, unsigned down) { (void)pos; (void)count; (void)present; (void)down; }
void input_driver_set_native_keyboard_available(bool available) { (void)available; }
void input_driver_set_native_keyboard_shown(bool shown) { (void)shown; }
enum input_text_type input_driver_text_entry_type(void) { return (enum input_text_type)0; }
SDL_Window *sdl3_get_window(void) { return NULL; }
void sdl3_pump_input_events(void) { SDL_PumpEvents(); }
bool sdl3_uses_screen_keyboard(void) { return false; }
uint32_t utf8_walk(const char **string) { return (uint32_t)(unsigned char)*(*string)++; }

#define MAX_EDGES 8
static unsigned edges;
static bool     edge_down[MAX_EDGES];
static unsigned edge_code[MAX_EDGES];

void input_keyboard_event(bool down, unsigned code, uint32_t character,
      uint16_t mod, unsigned device)
{
   (void)character; (void)mod; (void)device;
   if (!code)
      return;
   if (edges < MAX_EDGES)
   {
      edge_down[edges] = down;
      edge_code[edges] = code;
   }
   edges++;
}

static void push_key(bool down, SDL_Scancode sc, SDL_Keymod mod)
{
   SDL_Event e;
   SDL_zero(e);
   e.type         = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
   e.key.scancode = sc;
   e.key.key      = SDL_GetKeyFromScancode(sc, mod, true);
   e.key.mod      = mod;
   e.key.down     = down;
   SDL_PushEvent(&e);
}

/* One press and release; the two edges must name want. */
static unsigned run(sdl3_input_t *sdl, SDL_Scancode sc,
      SDL_Keymod down_mod, unsigned want, const char *what)
{
   edges = 0;
   push_key(true, sc, down_mod);
   push_key(false, sc, SDL_KMOD_NONE);
   sdl3_input_poll(sdl);
   if (     edges != 2
         || !edge_down[0] || edge_code[0] != want
         ||  edge_down[1] || edge_code[1] != want)
   {
      printf("FAIL %s %s: want RETROK %u down and up, got %u edge(s): %s %u, %s %u\n",
            what, SDL_GetScancodeName(sc), want, edges,
            edge_down[0] ? "down" : "up", edge_code[0],
            edge_down[1] ? "down" : "up", edge_code[1]);
      return 1;
   }
   return 0;
}

int main(void)
{
   sdl3_input_t *sdl;
   unsigned bad = 0, keys = 0;
   int sc;

   SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
   if (!SDL_Init(SDL_INIT_VIDEO) || !(sdl = (sdl3_input_t*)sdl3_input_init(NULL)))
   {
      printf("FAIL sdl3_keyboard_test: SDL did not start (%s)\n", SDL_GetError());
      return 1;
   }

   for (sc = SDL_SCANCODE_UNKNOWN + 1; sc < SDL_SCANCODE_COUNT; sc++)
   {
      unsigned want = input_keymaps_translate_keysym_to_rk(
            SDL_GetKeyFromScancode((SDL_Scancode)sc, SDL_KMOD_NONE, true));
      if (!want)
         continue;
      keys++;
      bad += run(sdl, (SDL_Scancode)sc, SDL_KMOD_NONE,   want, "press");
      bad += run(sdl, (SDL_Scancode)sc, SDL_KMOD_LSHIFT, want, "shift press");
   }

   sdl3_input_free(sdl);
   SDL_Quit();

   if (!keys)
   {
      printf("FAIL sdl3_keyboard_test: the layout placed no keys\n");
      return 1;
   }
   if (bad)
   {
      printf("FAIL sdl3_keyboard_test: %u of %u keys\n", bad, keys * 2);
      return 1;
   }
   printf("PASS sdl3_keyboard_test: %u keys, plain and shifted, name their"
         " unmodified key on both edges\n", keys);
   return 0;
}
