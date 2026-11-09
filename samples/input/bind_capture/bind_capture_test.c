/* Mouse binds from published frames and from drivers that keep their
 * mice; reassigned controllers and a hotkey's sharer during capture. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <features/features_cpu.h>

#include "configuration.h"
#include "runloop.h"
#include "menu/menu_driver.h"
#include "menu/menu_setting.h"
#include "menu/menu_bind_trigger.h"
#include "input/input_remapping.h"
#include "gfx/video_driver.h"

#define MENU_NOINLINE
#define HOTKEYS (RARCH_BIND_LIST_END - RARCH_FIRST_META_KEY)

static unsigned failures;
#define CHECK(cond) do { if (!(cond)) { failures++; \
   fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #cond); } } while (0)

static settings_t settings;
static input_driver_state_t input_driver_st;
static struct menu_state menu_driver_state;
static menu_handle_t menu;
static retro_keybind_set binds[MAX_USERS];
struct retro_keybind *input_config_binds[MAX_USERS];
struct retro_keybind *input_autoconf_binds[MAX_USERS];
const unsigned input_config_bind_order[24] = { 0 };
static input_driver_t driver;
static input_device_driver_t joypad;
static input_pointer_frame_t frames[MAX_USERS];
/* mouse buttons as bits; the driver keeps one mouse per pad, as
 * gx_input does its pointers */
static uint32_t driver_mice[MAX_USERS];
static uint32_t pad_buttons[MAX_USERS];
static int16_t pad_rest[MAX_USERS];
static unsigned pad_high;
static input_bits_t pad_bits;
static unsigned pad_button_reads;
static uint16_t own_hot_key[MAX_USERS][HOTKEYS];
static uint32_t own_hot_axis[MAX_USERS][HOTKEYS];
static unsigned driver_mouse_reads;
static unsigned driver_key;

settings_t *config_get_ptr(void)
{
   return &settings;
}

bool string_is_equal(const char *a, const char *b)
{
   return strcmp(a, b) == 0;
}

retro_time_t cpu_features_get_time_usec(void)
{
   return 1000000;
}

const char *msg_hash_to_str(enum msg_hash_enums msg)
{
   return "";
}

const char *input_config_bind_map_get_desc(unsigned index)
{
   return "";
}

size_t input_config_get_bind_string(void *settings_data,
      char *s, const struct retro_keybind *bind,
      const struct retro_keybind *auto_bind,
      const struct input_bind_label *label,
      const struct input_bind_label *auto_label, size_t len)
{
   *s = '\0';
   return 0;
}

void runloop_msg_queue_push(const char *msg, size_t len,
      unsigned prio, unsigned duration, bool flush, char *title,
      enum message_queue_icon icon, enum message_queue_category category) { }
void input_keyboard_mapping_bits(unsigned mode, unsigned key) { }
void input_driver_set_keyboard_mapping_blocked(bool blocked) { }
void input_driver_hold_held_input(void) { }

/* no controller plays as another player */
unsigned input_config_user_player(unsigned user)
{
   return user;
}

const char *input_config_get_device_name(unsigned port)
{
   return "pad";
}

struct retro_keybind *input_config_bind_edit(unsigned user, unsigned id)
{
   return &input_config_binds[user][id];
}

void input_config_plays_as_hotkey(unsigned slot, unsigned id,
      uint16_t *joykey, uint32_t *joyaxis)
{
   *joykey  = own_hot_key[slot][id - RARCH_FIRST_META_KEY];
   *joyaxis = own_hot_axis[slot][id - RARCH_FIRST_META_KEY];
}

bool input_config_plays_as_set_hotkey(unsigned slot, unsigned id,
      uint16_t joykey, uint32_t joyaxis)
{
   own_hot_key[slot][id - RARCH_FIRST_META_KEY]  = joykey;
   own_hot_axis[slot][id - RARCH_FIRST_META_KEY] = joyaxis;
   return true;
}

bool video_driver_get_viewport_info(struct video_viewport *vp)
{
   return false;
}

bool video_driver_translate_coord_viewport(struct video_viewport *vp,
      int x, int y, uint32_t *pos, uint32_t *screen_pos, bool report_oob)
{
   return false;
}

bool input_driver_pointer_is_offscreen(int16_t x, int16_t y)
{
   return x == -0x8000 || y == -0x8000;
}

/* no mouse pins */
unsigned input_mouse_port_index(unsigned port)
{
   if (port >= MAX_USERS)
      return MAX_INPUT_DEVICES;
   return settings.uints.input_mouse_index[port];
}

#include "input/input_pointer_frame.h"

#undef RARCH_LOG
#define RARCH_LOG(...) do { } while (0)

/* These paths are not used by the menu's mouse and key capture. */
#define input_joypad_from_keys(input, data, blocked, port, id) 0
#define input_analog_from_keys(input, data, port, idx, id) 0

static int16_t driver_state(void *data,
      const input_device_driver_t *pad, rarch_joypad_info_t *info,
      const retro_keybind_set *all_binds,
      unsigned port, unsigned device, unsigned idx, unsigned id)
{
   if (device == RETRO_DEVICE_MOUSE)
   {
      driver_mouse_reads++;
      return id < 32 && ((driver_mice[info->joy_idx] >> id) & 1);
   }
   if (device == RETRO_DEVICE_KEYBOARD)
      return id == driver_key;
   return 0;
}

static void driver_keys_down(void *data, unsigned port,
      const uint16_t *keys, const uint8_t *bind, unsigned count,
      uint32_t *down)
{
   unsigned i;
   for (i = 0; i < count; i++)
      if (keys[i] == driver_key)
         down[i >> 5] |= (1u << (i & 31));
}

static int32_t pad_button(unsigned pad, uint16_t id)
{
   CHECK(pad < MAX_USERS);
   if (id < MENU_MAX_BUTTONS)
      pad_button_reads++;
   if (pad_high && id == pad_high)
      return 1;
   return id < 32 && ((pad_buttons[pad] >> id) & 1);
}

static void pad_get_buttons(unsigned pad, input_bits_t *state)
{
   *state = pad_bits;
}

static int16_t pad_axis(unsigned pad, uint32_t axis)
{
   CHECK(pad < MAX_USERS);
   return axis == AXIS_NEG(3) ? pad_rest[pad] : 0;
}

/* the real one, but for the bind screen it pushes */
static int menu_input_key_bind_set_mode_common(struct menu_state *menu_st,
      struct menu_bind_state *state, enum menu_input_binds_ctl_state mode,
      rarch_setting_t *setting, settings_t *conf)
{
   state->order    = 0;
   state->begin    = setting->bind_type;
   state->last     = setting->bind_type;
   state->out_user = setting->index_offset;
   state->out_id   = setting->bind_type - MENU_SETTINGS_BIND_BEGIN;
   state->buffer   = *input_config_bind(state->out_user, state->out_id);
   state->user     = setting->index_offset;
   return 0;
}

#include "driver_code.inc"

static void reset(void)
{
   unsigned i, j;
   memset(&settings, 0, sizeof(settings));
   memset(&input_driver_st, 0, sizeof(input_driver_st));
   memset(&menu_driver_state, 0, sizeof(menu_driver_state));
   memset(&driver, 0, sizeof(driver));
   memset(&joypad, 0, sizeof(joypad));
   memset(frames, 0, sizeof(frames));
   memset(binds, 0, sizeof(binds));
   memset(driver_mice, 0, sizeof(driver_mice));
   memset(pad_buttons, 0, sizeof(pad_buttons));
   memset(pad_rest, 0, sizeof(pad_rest));
   pad_high = 0;
   memset(&pad_bits, 0, sizeof(pad_bits));
   pad_button_reads = 0;
   for (i = 0; i < MAX_USERS; i++)
   {
      input_config_binds[i]                = binds[i];
      input_autoconf_binds[i]              = binds[i];
      settings.uints.input_joypad_index[i] = i;
      settings.uints.input_mouse_index[i]  = i;
      settings.uints.input_remap_ports[i]  = i;
      for (j = 0; j < RARCH_BIND_LIST_END; j++)
      {
         binds[i][j].joykey  = NO_BTN;
         binds[i][j].joyaxis = AXIS_NONE;
      }
      for (j = 0; j < HOTKEYS; j++)
      {
         own_hot_key[i][j]  = NO_BTN;
         own_hot_axis[i][j] = AXIS_NONE;
      }
   }
   settings.uints.input_max_users       = 2;
   settings.uints.input_bind_timeout    = 3;
   driver.input_state                   = driver_state;
   joypad.button                        = pad_button;
   joypad.axis                          = pad_axis;
   input_driver_st.current_driver       = &driver;
   input_driver_st.primary_joypad       = &joypad;
   menu_driver_state.driver_data        = &menu;
   driver_mouse_reads                   = 0;
   driver_key                           = 0;
   input_pointer_frames_clear();
}

static bool bind_step(void)
{
   char s[256];
   menu_input_ctx_bind_t bind;
   bind.s   = s;
   bind.len = sizeof(s);
   return menu_input_key_bind_iterate(&settings, &bind, 1000000);
}

/* The capture of @user's @id, up to the first frame with everything
 * let go. */
static bool bind_start(unsigned user, unsigned id)
{
   rarch_setting_t setting;
   memset(&setting, 0, sizeof(setting));
   setting.index_offset = user;
   setting.bind_type    = MENU_SETTINGS_BIND_BEGIN + id;
   return menu_input_key_bind_set_mode(MENU_INPUT_BINDS_CTL_BIND_SINGLE,
         &setting) && !bind_step();
}

static void test_mouse_frames(void)
{
   static const unsigned button[] = {
      RETRO_DEVICE_ID_MOUSE_LEFT, RETRO_DEVICE_ID_MOUSE_RIGHT,
      RETRO_DEVICE_ID_MOUSE_MIDDLE, RETRO_DEVICE_ID_MOUSE_BUTTON_4,
      RETRO_DEVICE_ID_MOUSE_BUTTON_5, RETRO_DEVICE_ID_MOUSE_WHEELUP,
      RETRO_DEVICE_ID_MOUSE_WHEELDOWN, RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP,
      RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN };
   static const unsigned bit[] = {
      INPUT_POINTER_LEFT, INPUT_POINTER_RIGHT, INPUT_POINTER_MIDDLE,
      INPUT_POINTER_BUTTON_4, INPUT_POINTER_BUTTON_5, INPUT_POINTER_WHEEL_UP,
      INPUT_POINTER_WHEEL_DOWN, INPUT_POINTER_HWHEEL_UP, INPUT_POINTER_HWHEEL_DOWN };
   unsigned b, other;
   reset();
   for (b = 0; b < sizeof(button) / sizeof(button[0]); b++)
   {
      frames[0].buttons = (uint16_t)bit[b];
      input_pointer_frames_set(frames, NULL, 1, 0);
      CHECK(input_driver_bind_capture_state(0, 0,
               RETRO_DEVICE_MOUSE, 0, button[b]) == 1);
      CHECK(input_driver_bind_capture_state(0, 0,
               RETRO_DEVICE_MOUSE, 0, button[b]) == 1);
      for (other = 0; other < sizeof(button) / sizeof(button[0]); other++)
         if (other != b)
            CHECK(input_driver_bind_capture_state(0, 0,
                     RETRO_DEVICE_MOUSE, 0, button[other]) == 0);
      frames[0].buttons = 0;
      input_pointer_frames_set(frames, NULL, 1, 0);
      CHECK(input_driver_bind_capture_state(0, 0,
               RETRO_DEVICE_MOUSE, 0, button[b]) == 0);
   }
   CHECK(driver_mouse_reads == 0);

   driver_mice[0] = 1 << RETRO_DEVICE_ID_MOUSE_LEFT;
   input_pointer_frames_clear();
   CHECK(input_driver_bind_capture_state(0, 0,
            RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT) == 1);
   frames[0].buttons = 0;
   input_pointer_frames_set(frames, NULL, 1, INPUT_POINTERS_MOUSE_BY_DRIVER);
   CHECK(input_driver_bind_capture_state(0, 0,
            RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT) == 1);
}

/* @user on slot @pad, and user @pad on slot @user */
static void test_reassigned_pad(unsigned user, unsigned pad)
{
   struct menu_bind_state *state = &menu_driver_state.input_binds;
   reset();
   settings.uints.input_joypad_index[user] = pad;
   settings.uints.input_joypad_index[pad]  = user;
   pad_rest[pad]                           = -32767;
   input_pointer_frames_set(frames, NULL, MAX_USERS,
         INPUT_POINTERS_BY_MOUSE_INDEX);
   CHECK(bind_start(user, RETRO_DEVICE_ID_JOYPAD_B));
   CHECK(state->port == pad);
   CHECK(state->axis_state[pad].rested_axes[3] == -32767);
   frames[user].buttons = INPUT_POINTER_RIGHT;
   input_pointer_frames_set(frames, NULL, MAX_USERS,
         INPUT_POINTERS_BY_MOUSE_INDEX);
   CHECK(bind_step());
   CHECK(RETRO_KEYBIND_MBUTTON(&binds[user][RETRO_DEVICE_ID_JOYPAD_B])
         == RETRO_DEVICE_ID_MOUSE_RIGHT);

   frames[user].buttons = 0;
   input_pointer_frames_set(frames, NULL, MAX_USERS,
         INPUT_POINTERS_BY_MOUSE_INDEX);
   CHECK(bind_start(user, RETRO_DEVICE_ID_JOYPAD_A));
   pad_buttons[pad] = 1 << 7;
   CHECK(bind_step());
   CHECK(binds[user][RETRO_DEVICE_ID_JOYPAD_A].joykey == 7);

   /* the mouse the driver keeps on the user's pad */
   pad_buttons[pad] = 0;
   input_pointer_frames_clear();
   CHECK(bind_start(user, RETRO_DEVICE_ID_JOYPAD_Y));
   driver_mice[pad] = 1 << RETRO_DEVICE_ID_MOUSE_MIDDLE;
   CHECK(bind_step());
   CHECK(RETRO_KEYBIND_MBUTTON(&binds[user][RETRO_DEVICE_ID_JOYPAD_Y])
         == RETRO_DEVICE_ID_MOUSE_MIDDLE);
}

/* User 1 on slot 1, User 2 on slot 0, both on core port 1: User 2's
 * controller fires the hotkeys too, and keeps its press as its own. */
static void test_hotkey_sharer(void)
{
   uint16_t key;
   uint32_t axis;
   unsigned id = RARCH_MENU_TOGGLE;
   reset();
   settings.uints.input_joypad_index[0] = 1;
   settings.uints.input_joypad_index[1] = 0;
   settings.uints.input_remap_ports[1]  = 0;

   CHECK(bind_start(0, id));
   pad_buttons[0] = 1 << 5;
   CHECK(bind_step());
   input_config_plays_as_hotkey(0, id, &key, &axis);
   CHECK(key == 5);
   input_config_plays_as_hotkey(1, id, &key, &axis);
   CHECK(key == NO_BTN);
   CHECK(binds[0][id].joykey == NO_BTN);

   pad_buttons[0] = 0;
   CHECK(bind_start(0, id));
   pad_buttons[1] = 1 << 6;
   CHECK(bind_step());
   CHECK(binds[0][id].joykey == 6);
   input_config_plays_as_hotkey(0, id, &key, &axis);
   CHECK(key == 5);
}

/* A key down, read one by one or all at once; a code below the first
 * key is none. */
static void test_keys(bool at_once)
{
   reset();
   if (at_once)
      driver.keys_down = driver_keys_down;
   CHECK(bind_start(0, RETRO_DEVICE_ID_JOYPAD_B));
   driver_key = RETROK_BACKSPACE - 1;
   CHECK(!bind_step());
   driver_key = RETROK_x;
   CHECK(bind_step());
   CHECK(RETRO_KEYBIND_KEY(&binds[0][RETRO_DEVICE_ID_JOYPAD_B]) == RETROK_x);
   driver_key = 0;
   CHECK(bind_start(0, RETRO_DEVICE_ID_JOYPAD_A));
   driver_key = RETROK_LAST - 1;
   CHECK(bind_step());
   CHECK(RETRO_KEYBIND_KEY(&binds[0][RETRO_DEVICE_ID_JOYPAD_A])
         == RETROK_LAST - 1);
}

/* the highest button there is, among the bits */
static void test_last_button(void)
{
   reset();
   CHECK(bind_start(0, RETRO_DEVICE_ID_JOYPAD_B));
   pad_high = MENU_MAX_BUTTONS - 1;
   CHECK(bind_step());
   CHECK(binds[0][RETRO_DEVICE_ID_JOYPAD_B].joykey == MENU_MAX_BUTTONS - 1);
}

/* A driver checked to answer get_buttons() as button() is read at
 * once, and only as far as the buttons bound go; any other is read a
 * button at a time, whatever get_buttons() it has. */
static void test_get_buttons(void)
{
   static input_device_driver_t checked, other;

   reset();
   checked                        = joypad;
   checked.ident                  = "udev";
   checked.get_buttons            = pad_get_buttons;
   input_driver_st.primary_joypad = &checked;
   CHECK(bind_start(0, RETRO_DEVICE_ID_JOYPAD_B));
   pad_button_reads = 0;
   BIT256_SET(pad_bits, MENU_MAX_BUTTONS);
   BIT256_SET(pad_bits, 255);
   CHECK(!bind_step());
   BIT256_SET(pad_bits, MENU_MAX_BUTTONS - 1);
   CHECK(bind_step());
   CHECK(binds[0][RETRO_DEVICE_ID_JOYPAD_B].joykey == MENU_MAX_BUTTONS - 1);
   CHECK(pad_button_reads == 0);

   reset();
   other                          = joypad;
   other.ident                    = "unchecked";
   other.get_buttons              = pad_get_buttons;
   input_driver_st.primary_joypad = &other;
   CHECK(bind_start(0, RETRO_DEVICE_ID_JOYPAD_B));
   BIT256_SET(pad_bits, 9);
   pad_buttons[0] = 1 << 7;
   CHECK(bind_step());
   CHECK(binds[0][RETRO_DEVICE_ID_JOYPAD_B].joykey == 7);
   CHECK(pad_button_reads > 0);
}

int main(void)
{
   test_mouse_frames();
   test_reassigned_pad(0, 1);
   test_reassigned_pad(1, 0);
   test_reassigned_pad(0, MAX_USERS - 1);
   test_hotkey_sharer();
   test_keys(false);
   test_keys(true);
   test_last_button();
   test_get_buttons();
   if (failures)
      return 1;
   puts("PASS bind_capture_test");
   return 0;
}
