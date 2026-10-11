#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "input/input_driver.h"

#define INV_0x8000 (1.0f / 32768.0f)
#define INPUT_CONFIG_BIND(port, i) (&input_config_binds[0][i])
#define input_state_wrap(input, data, pad, info, binds, blocked, port, device, idx, id) \
   source_down(device, id)

static retro_keybind_set binds;
struct retro_keybind *input_config_binds[MAX_USERS] = { binds };
static struct retro_keybind auto_binds[RARCH_BIND_LIST_END];
static uint32_t buttons;
static uint32_t axis_down;
static unsigned key_down, mouse_down;
static unsigned expected_pad = 3;
static unsigned button_reads, axis_reads, key_reads, mouse_reads;

#define CHECK(c) do { if (!(c)) { \
   fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); exit(1); } } while (0)

static int32_t button(unsigned port, uint16_t id)
{
   CHECK(port == expected_pad);
   button_reads++;
   return id < 32 && (buttons & (1u << id)) != 0;
}

static int16_t axis(unsigned port, uint32_t id)
{
   CHECK(port == expected_pad);
   axis_reads++;
   return id == axis_down ? 32767 : 0;
}

static int32_t source_down(unsigned device, unsigned id)
{
   if (device == RETRO_DEVICE_KEYBOARD)
   {
      key_reads++;
      return id == key_down;
   }
   if (device == RETRO_DEVICE_MOUSE)
   {
      mouse_reads++;
      return id == mouse_down;
   }
   return 0;
}

#include "driver_code.inc"

static input_driver_state_t state;
static input_device_driver_t pad;
static rarch_joypad_info_t info;
static input_hotkey_block_t context;

static void reset(void)
{
   unsigned i;
   struct retro_keybind empty;
   memset(&empty, 0, sizeof(empty));
   empty.joykey = NO_BTN;
   empty.joyaxis = AXIS_NONE;
   RETRO_KEYBIND_SET_KEY(&empty, RETROK_UNKNOWN);
   RETRO_KEYBIND_SET_MBUTTON(&empty, NO_BTN);
   RETRO_KEYBIND_SET_VALID(&empty, true);
   for (i = 0; i < RARCH_BIND_LIST_END; i++)
      input_config_binds[0][i] = auto_binds[i] = empty;
   memset(&state, 0, sizeof(state));
   buttons = 0;
   axis_down = AXIS_NONE;
   key_down = RETROK_UNKNOWN;
   mouse_down = NO_BTN;
   button_reads = axis_reads = key_reads = mouse_reads = 0;
   pad.button = button;
   pad.axis = axis;
   info.auto_binds = auto_binds;
   info.joy_idx = expected_pad;
   info.axis_threshold = 0.5f;
   memset(&context, 0, sizeof(context));
   context.input_st = &state;
   context.joypad = &pad;
   context.joypad_info = &info;
}

/* Menu first in the scan; the hold hotkey follows it every frame. */
static void chord(bool open, bool other, bool blocked)
{
   input_bits_t bits;
   unsigned frame;
   memset(&bits, 0, sizeof(bits));
   for (frame = 0; frame < 3; frame++)
   {
      press(context, RARCH_MENU_TOGGLE, false, blocked, &bits);
      press(context, RARCH_FAST_FORWARD_HOLD_KEY, other, blocked, &bits);
      CHECK(BIT256_GET_PTR(&bits, RARCH_FAST_FORWARD_HOLD_KEY));
      CHECK(!BIT256_GET_PTR(&bits, RARCH_MENU_TOGGLE));
   }
   buttons = 0;
   axis_down = AXIS_NONE;
   key_down = RETROK_UNKNOWN;
   mouse_down = NO_BTN;
   release(&state, &bits);
   CHECK(!!BIT256_GET_PTR(&bits, RARCH_MENU_TOGGLE) == open);
   CHECK(!(state.flags & (INP_FLAG_MENU_PRESS_PENDING | INP_FLAG_MENU_PRESS_CANCEL)));
   memset(&bits, 0, sizeof(bits));
   release(&state, &bits);
   CHECK(!BIT256_GET_PTR(&bits, RARCH_MENU_TOGGLE));
}

int main(void)
{
   struct retro_keybind *en = &input_config_binds[0][RARCH_ENABLE_HOTKEY];
   struct retro_keybind *ff = &input_config_binds[0][RARCH_FAST_FORWARD_HOLD_KEY];

   reset();
   en->joykey = ff->joykey = 23;
   buttons = 1u << 23;
   chord(true, false, false);
   CHECK(button_reads == 0 && axis_reads == 0 && key_reads == 0 && mouse_reads == 0);

   reset();
   auto_binds[RARCH_ENABLE_HOTKEY].joykey = ff->joykey = 23;
   buttons = 1u << 23;
   chord(true, false, false);

   reset();
   en->joykey = auto_binds[RARCH_FAST_FORWARD_HOLD_KEY].joykey = 23;
   buttons = 1u << 23;
   chord(true, false, false);

   reset();
   auto_binds[RARCH_ENABLE_HOTKEY].joykey = 23;
   auto_binds[RARCH_FAST_FORWARD_HOLD_KEY].joykey = 23;
   buttons = 1u << 23;
   chord(true, false, false);

   reset();
   en->joykey = 23;
   ff->joykey = 20;
   buttons = (1u << 23) | (1u << 20);
   chord(false, false, false);
   CHECK(button_reads == 0 && axis_reads == 0 && key_reads == 0 && mouse_reads == 0);

   reset();
   en->joykey = ff->joykey = 23;
   RETRO_KEYBIND_SET_KEY(ff, RETROK_f);
   buttons = 1u << 23;
   key_down = RETROK_f;
   chord(false, false, false);
   CHECK(button_reads == 0 && axis_reads == 0 && key_reads == 1 && mouse_reads == 0);

   reset();
   en->joykey = ff->joykey = 23;
   RETRO_KEYBIND_SET_KEY(ff, RETROK_f);
   buttons = 1u << 23;
   key_down = RETROK_f;
   chord(true, false, true);

   reset();
   RETRO_KEYBIND_SET_KEY(en, RETROK_f);
   RETRO_KEYBIND_SET_KEY(ff, RETROK_f);
   key_down = RETROK_f;
   chord(true, false, false);

   reset();
   en->joyaxis = ff->joyaxis = AXIS_POS(1);
   axis_down = AXIS_POS(1);
   chord(true, false, false);
   CHECK(axis_reads == 0);

   reset();
   auto_binds[RARCH_ENABLE_HOTKEY].joyaxis = AXIS_NEG(1);
   auto_binds[RARCH_FAST_FORWARD_HOLD_KEY].joyaxis = AXIS_NEG(1);
   axis_down = AXIS_NEG(1);
   chord(true, false, false);

   reset();
   en->joyaxis = AXIS_NEG(1);
   ff->joyaxis = AXIS_POS(1);
   axis_down = AXIS_POS(1);
   chord(false, false, false);

   reset();
   en->joykey = ff->joykey = 23;
   ff->joyaxis = AXIS_POS(1);
   buttons = 1u << 23;
   axis_down = AXIS_POS(1);
   chord(false, false, false);

   reset();
   RETRO_KEYBIND_SET_MBUTTON(en, RETRO_DEVICE_ID_MOUSE_LEFT);
   RETRO_KEYBIND_SET_MBUTTON(ff, RETRO_DEVICE_ID_MOUSE_LEFT);
   mouse_down = RETRO_DEVICE_ID_MOUSE_LEFT;
   chord(true, false, false);

   reset();
   en->joykey = ff->joykey = 23;
   RETRO_KEYBIND_SET_MBUTTON(ff, RETRO_DEVICE_ID_MOUSE_LEFT);
   buttons = 1u << 23;
   mouse_down = RETRO_DEVICE_ID_MOUSE_LEFT;
   chord(false, false, false);

   reset();
   en->joykey = ff->joykey = 23;
   buttons = 1u << 23;
   chord(false, true, false);

   reset();
   en->joykey = ff->joykey = 23;
   RETRO_KEYBIND_SET_VALID(en, false);
   buttons = 1u << 23;
   chord(false, false, false);

   reset();
   {
      input_bits_t bits;
      memset(&bits, 0, sizeof(bits));
      press(context, RARCH_MENU_TOGGLE, false, false, &bits);
      press(context, RARCH_PAUSE_TOGGLE, true, false, &bits);
      CHECK(state.flags & INP_FLAG_MENU_PRESS_CANCEL);
      press(context, RARCH_GAME_FOCUS_TOGGLE, true, false, &bits);
      release(&state, &bits);
      CHECK(BIT256_GET_PTR(&bits, RARCH_MENU_TOGGLE));
   }

   puts("hotkey menu: PASS");
   return 0;
}
