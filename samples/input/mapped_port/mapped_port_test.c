/* Mapped Port in input/input_driver.c: the users mapped to a core port
 * feed it and get its rumble. The real input_driver.c, with the rest
 * of the frontend stood in for. */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <features/features_cpu.h>

#include "../../../configuration.h"
#include "../../../command.h"
#include "../../../retroarch.h"
#include "../../../runloop.h"
#include "../../../verbosity.h"
#include "../../../msg_hash.h"
#include "../../../gfx/video_driver.h"
#include "../../../input/input_driver.h"
#include "../../../input/input_remapping.h"
#include "../../../menu/menu_driver.h"
#include "../../../menu/menu_entries.h"
#include "../../../menu/menu_input.h"

static settings_t        settings;
static runloop_state_t   runloop_st;
static struct menu_state t_menu_st;
static int               failures;

#define CHECK(c, msg) do { if (!(c)) { \
   printf("FAIL line %d: %s\n", __LINE__, msg); failures++; } } while (0)

settings_t *config_get_ptr(void) { return &settings; }
runloop_state_t *runloop_state_get_ptr(void) { return &runloop_st; }
void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
bool command_event(enum event_command cmd, void *data)
{ (void)cmd; (void)data; return false; }
bool retroarch_ctl(enum rarch_ctl_state state, void *data)
{ (void)state; (void)data; return false; }
bool video_driver_has_focus(void) { return true; }
#if defined(_WIN32)
void win32_text_entry(bool active) { (void)active; }
#endif
bool video_driver_headset_focused(void) { return false; }
bool video_driver_get_viewport_info(struct video_viewport *vp)
{ (void)vp; return false; }
bool video_driver_translate_coord_viewport(struct video_viewport *vp,
      int x, int y, uint32_t *pos, uint32_t *screen_pos, bool oob)
{ (void)vp; (void)x; (void)y; (void)pos; (void)screen_pos; (void)oob;
  return false; }
struct menu_state *menu_state_get_ptr(void) { return &t_menu_st; }
bool menu_input_dialog_get_display_kb(void) { return false; }
void menu_entry_get(menu_entry_t *entry, size_t stack_idx,
      size_t i, void *userdata, bool use_representation)
{ (void)entry; (void)stack_idx; (void)i; (void)userdata;
  (void)use_representation; }
const char *msg_hash_to_str(enum msg_hash_enums msg)
{ (void)msg; return ""; }
void runloop_msg_queue_push(const char *msg, size_t len,
      unsigned prio, unsigned duration, bool flush, char *title,
      enum message_queue_icon icon, enum message_queue_category category)
{ (void)msg; (void)len; (void)prio; (void)duration; (void)flush;
  (void)title; (void)icon; (void)category; }

/* Slot n holds t_buttons[n] and pushes axis t_axis_held[n] all the
 * way; every slot's profile has the RetroPad's ids as its buttons and
 * L2 and R2 as axes 4 and 5, unless a test gives it another. */
static uint32_t t_buttons[MAX_USERS];
static uint32_t t_axis_held[MAX_USERS];
/* the button reads of each slot, apart from its RetroPad's */
static unsigned t_button_reads[MAX_USERS];
static unsigned t_rumbled[MAX_USERS];
/* bit n: slot n's motors answer false */
static unsigned t_rumble_refused;
static struct retro_keybind t_auto[RARCH_BIND_LIST_END];
/* An xpad's numbering: Back 6 is Select, Guide 8 none of the RetroPad's
 * buttons, LT axis 2 as well as button 15. */
static struct retro_keybind t_xpad[RARCH_BIND_LIST_END];
/* the RetroPad's ids, as t_auto, with hotkeys of its own */
static struct retro_keybind t_auto_own[RARCH_BIND_LIST_END];
#define T_OWN_MENU 20
#define T_OWN_FF   23
/* t_auto_own's, with a Hotkey Enable of its own */
static struct retro_keybind t_auto_en[RARCH_BIND_LIST_END];
#define T_OWN_EN   25
static bool t_query(unsigned pad) { return pad < MAX_USERS; }
static bool t_held(unsigned port, uint16_t joykey)
{
   return port < MAX_USERS && joykey < 32
      && ((t_buttons[port] >> joykey) & 1);
}
static int32_t t_button(unsigned port, uint16_t joykey)
{
   if (port < MAX_USERS)
      t_button_reads[port]++;
   return t_held(port, joykey);
}
static int16_t t_state(rarch_joypad_info_t *joypad_info,
      const uint16_t *joykeys, const uint32_t *joyaxes, unsigned port)
{
   unsigned i;
   int16_t ret = 0;
   (void)joyaxes;
   (void)port;
   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
      if (joykeys[i] != NO_BTN && t_held(joypad_info->joy_idx, joykeys[i]))
         ret |= (int16_t)(1 << i);
   return ret;
}
static int16_t t_axis(unsigned port, uint32_t joyaxis)
{
   return (port < MAX_USERS && joyaxis != AXIS_NONE
         && t_axis_held[port] == joyaxis) ? 0x7fff : 0;
}
static void t_poll(void) { }
static bool t_rumble(unsigned pad, enum retro_rumble_effect effect,
      uint16_t strength)
{
   (void)effect;
   if (pad < MAX_USERS && strength)
      t_rumbled[pad]++;
   return !((t_rumble_refused >> pad) & 1);
}
static const char *t_name(unsigned pad) { (void)pad; return "Pad"; }
static input_device_driver_t t_joypad = {
   NULL, t_query, NULL, t_button, t_state, NULL, t_axis, t_poll, t_rumble,
   NULL, NULL, NULL, t_name, "test" };

#define T_BIT(id) (1u << (id))
#define T_A       T_BIT(RETRO_DEVICE_ID_JOYPAD_A)
#define T_B       T_BIT(RETRO_DEVICE_ID_JOYPAD_B)

/* Four users, user n reading slot n into core port n; no binds of
 * their own; nothing held. */
static void t_reset(void)
{
   static const uint16_t xpad[RARCH_FIRST_CUSTOM_BIND] = {
      1, 3, 6, 7, 11, 12, 13, 14, 0, 2, 4, 5, 15, 16, 9, 10 };
   unsigned u, i;
   input_driver_state_t *st = input_state_get_ptr();

   t_rumble_refused = 0;

   for (i = 0; i < RARCH_BIND_LIST_END; i++)
   {
      t_auto[i].joykey  = (i < RARCH_FIRST_CUSTOM_BIND) ? (uint16_t)i : NO_BTN;
      t_auto[i].joyaxis = AXIS_NONE;
      t_auto[i].attr    = RETRO_KEYBIND_ATTR(RETROK_UNKNOWN, NO_BTN,
            i < RARCH_FIRST_CUSTOM_BIND);
   }
   t_auto[RETRO_DEVICE_ID_JOYPAD_L2].joyaxis = AXIS_POS(4);
   t_auto[RETRO_DEVICE_ID_JOYPAD_R2].joyaxis = AXIS_POS(5);
   for (i = 0; i < RARCH_BIND_LIST_END; i++)
   {
      t_xpad[i]         = t_auto[i];
      t_xpad[i].joykey  = (i < RARCH_FIRST_CUSTOM_BIND) ? xpad[i] : NO_BTN;
      t_xpad[i].joyaxis = AXIS_NONE;
      t_auto_own[i]     = t_auto[i];
   }
   t_xpad[RETRO_DEVICE_ID_JOYPAD_L2].joyaxis = AXIS_POS(2);
   t_auto_own[RARCH_MENU_TOGGLE].joykey      = T_OWN_MENU;
   t_auto_own[RARCH_FAST_FORWARD_KEY].joykey = T_OWN_FF;
   for (i = 0; i < RARCH_BIND_LIST_END; i++)
      t_auto_en[i] = t_auto_own[i];
   t_auto_en[RARCH_ENABLE_HOTKEY].joykey     = T_OWN_EN;
   for (u = 0; u < MAX_USERS; u++)
   {
      input_config_clear_device_name(u);
      input_config_set_device_vid(u, 0);
      input_config_set_device_pid(u, 0);
      t_axis_held[u]    = AXIS_NONE;
      t_button_reads[u] = 0;
   }
   for (u = 0; u < MAX_USERS; u++)
   {
      for (i = 0; i < RARCH_BIND_LIST_END; i++)
      {
         input_config_binds[u][i].joykey  = NO_BTN;
         input_config_binds[u][i].joyaxis = AXIS_NONE;
         input_config_binds[u][i].attr    =
            RETRO_KEYBIND_ATTR(RETROK_UNKNOWN, NO_BTN, false);
      }
      input_autoconf_binds[u]                 = t_auto;
      settings.uints.input_joypad_index[u]    = u;
      settings.uints.input_remap_ports[u]     = u;
      settings.uints.input_libretro_device[u] = RETRO_DEVICE_JOYPAD;
      t_buttons[u]                            = 0;
      t_rumbled[u]                            = 0;
   }
   settings.uints.input_max_users                 = 4;
   settings.uints.input_rumble_gain               = 100;
   settings.uints.input_menu_toggle_gamepad_combo = INPUT_COMBO_NONE;
   settings.floats.input_axis_threshold           = 0.5f;
   settings.bools.input_hotkey_follows_player1    = false;
   settings.bools.input_all_users_control_menu    = false;
   st->primary_joypad                             = &t_joypad;
   t_menu_st.flags                               &= ~MENU_ST_FLAG_ALIVE;
   input_remapping_update_port_map();
}

/* User @user on core port @port (MAX_USERS: None). */
static void t_map(unsigned user, unsigned port)
{
   settings.uints.input_remap_ports[user] = port;
   input_remapping_update_port_map();
}

/* Core port @port's buttons, read as a new frame's: the poll forgets
 * the frame's view, which would answer what an earlier read saw. */
static unsigned t_read(unsigned port)
{
   input_driver_state_t *st = input_state_get_ptr();
   memset(&st->frame_valid, 0, sizeof(st->frame_valid));
   return (uint16_t)input_driver_state_wrapper(port, RETRO_DEVICE_JOYPAD,
         0, RETRO_DEVICE_ID_JOYPAD_MASK);
}

/* Core port @port's strong motor at @strength, written as after a
 * frame; returns the slots that write reached, as bits. */
static unsigned t_rumble_port(unsigned port, uint16_t strength)
{
   unsigned s;
   unsigned slots = 0;
   memset(t_rumbled, 0, sizeof(t_rumbled));
   input_set_rumble_state(port, RETRO_RUMBLE_STRONG, strength);
   input_driver_flush_rumble();
   for (s = 0; s < MAX_USERS; s++)
      if (t_rumbled[s])
         slots |= 1u << s;
   return slots;
}

/* What core port @port's strong motor answers once written. */
static bool t_rumble_answer(unsigned port)
{
   input_set_rumble_state(port, RETRO_RUMBLE_STRONG, 0);
   input_driver_flush_rumble();
   return input_set_rumble_state(port, RETRO_RUMBLE_STRONG, 0);
}

/* Every user on its own core port: each port reads and rumbles its
 * own user's pad, as before. */
static void test_identity(void)
{
   t_reset();
   t_buttons[0] = T_A;
   t_buttons[3] = T_B;
   CHECK(t_read(0) == T_A && t_read(3) == T_B,
         "the identity map reads another user");
   CHECK(t_rumble_port(0, 1000) == 1u << 0,
         "the identity map: core port 1's rumble reached another slot");
   CHECK(t_rumble_port(3, 1001) == 1u << 3,
         "the identity map: core port 4's rumble reached another slot");
}

/* User 2 on Mapped Port 1: core port 1 reads and rumbles both pads;
 * core port 2, which nobody feeds, rumbles nobody. */
static void test_shared_port(void)
{
   t_reset();
   t_map(1, 0);
   t_buttons[0] = T_A;
   t_buttons[1] = T_B;
   CHECK(t_read(0) == (T_A | T_B), "core port 1 does not read both pads");
   CHECK(t_read(1) == 0, "core port 2 still reads user 2");
   CHECK(t_rumble_port(0, 1010) == ((1u << 0) | (1u << 1)),
         "core port 1's rumble did not reach users 1 and 2");
   CHECK(t_rumble_port(1, 1011) == 0,
         "core port 2's rumble reached user 2, who feeds core port 1");
   CHECK(t_rumble_answer(0), "core port 1's motor answered false");
   CHECK(!t_rumble_answer(1),
         "core port 2's motor, which reaches nobody, answered true");
}

/* A core port's motor answers true when any of its users' does. */
static void test_rumble_answer(void)
{
   t_reset();
   t_rumble_refused = 1u << 0;
   CHECK(!t_rumble_answer(0), "the identity map: a refusing motor answered true");
   t_map(1, 0);
   CHECK(t_rumble_answer(0),
         "core port 1 answered false with user 2's motor answering true");
   t_rumble_refused = (1u << 0) | (1u << 1);
   CHECK(!t_rumble_answer(0),
         "core port 1 answered true with both motors refusing");
}

/* Users 1 and 2 swapped: each core port's rumble follows its user. */
static void test_swapped_ports(void)
{
   t_reset();
   t_map(0, 1);
   t_map(1, 0);
   CHECK(t_rumble_port(0, 1020) == 1u << 1,
         "swapped: core port 1's rumble did not go to user 2 alone");
   CHECK(t_rumble_port(1, 1021) == 1u << 0,
         "swapped: core port 2's rumble did not go to user 1 alone");
}

/* Users 2 and 4 on core port 2, user 3 on None: core port 2's rumble
 * reaches users 2 and 4; core port 3's reaches nobody. */
static void test_port_2_users(void)
{
   t_reset();
   t_map(3, 1);
   t_map(2, MAX_USERS);
   CHECK(t_rumble_port(1, 1030) == ((1u << 1) | (1u << 3)),
         "core port 2's rumble did not reach users 2 and 4 alone");
   CHECK(t_rumble_port(2, 1031) == 0,
         "core port 3's rumble reached user 3, on None");
}

#define T_ENABLE 20
#define T_FF     21
#define T_MENU   22
#define T_Y      T_BIT(RETRO_DEVICE_ID_JOYPAD_Y)

/* A frame's system input, the block flags cleared first as the run
 * loop clears them. */
static void t_collect(input_bits_t *bits)
{
   input_driver_state_t *st = input_state_get_ptr();
   st->flags &= ~(INP_FLAG_BLOCK_LIBRETRO_INPUT | INP_FLAG_BLOCK_HOTKEY);
   memset(bits, 0, sizeof(*bits));
   input_driver_collect_system_input(st, &settings, bits);
}

/* User 1's hotkey @id on button @joykey of any controller that reads
 * it; counted as a change, as the menu's bind settings count it. */
static void t_bind(unsigned id, uint16_t joykey)
{
   input_config_binds[0][id].joykey = joykey;
   input_config_binds[0][id].attr   =
      RETRO_KEYBIND_ATTR(RETROK_UNKNOWN, NO_BTN, true);
   input_config_binds_changed();
}

/* With the menu up: user 1 wherever it is mapped, and the users on
 * its core port; never a user on None. */
static void test_menu_shared(void)
{
   input_bits_t bits;
   t_reset();
   t_menu_st.flags |= MENU_ST_FLAG_ALIVE;
   t_buttons[1] = T_B;
   t_collect(&bits);
   CHECK(!BIT256_GET(bits, RETRO_DEVICE_ID_JOYPAD_B),
         "the identity map: user 2 drives the menu");

   t_map(1, 0);
   t_collect(&bits);
   CHECK(BIT256_GET(bits, RETRO_DEVICE_ID_JOYPAD_B),
         "user 2 on core port 1 does not drive the menu");
   t_buttons[1] = 0;
   t_buttons[0] = T_A;
   t_collect(&bits);
   CHECK(BIT256_GET(bits, RETRO_DEVICE_ID_JOYPAD_A),
         "user 1 no longer drives the menu");
   t_buttons[0] = 0;
   t_buttons[2] = T_Y;
   t_collect(&bits);
   CHECK(!BIT256_GET(bits, RETRO_DEVICE_ID_JOYPAD_Y),
         "user 3, on its own core port, drives the menu");

   /* swapped: user 1 on core port 2, user 2 on core port 1 */
   t_map(0, 1);
   t_buttons[2] = 0;
   t_buttons[0] = T_A;
   t_collect(&bits);
   CHECK(BIT256_GET(bits, RETRO_DEVICE_ID_JOYPAD_A),
         "swapped: user 1 no longer drives the menu");
   t_buttons[0] = 0;
   t_buttons[1] = T_B;
   t_collect(&bits);
   CHECK(!BIT256_GET(bits, RETRO_DEVICE_ID_JOYPAD_B),
         "swapped: user 2, on core port 1, drives the menu");

   /* both on None: nothing is shared */
   t_map(0, MAX_USERS);
   t_map(1, MAX_USERS);
   t_collect(&bits);
   CHECK(!BIT256_GET(bits, RETRO_DEVICE_ID_JOYPAD_B),
         "user 2 on None drives the menu with user 1 on None");
   t_buttons[1] = 0;
   t_buttons[0] = T_A;
   t_collect(&bits);
   CHECK(BIT256_GET(bits, RETRO_DEVICE_ID_JOYPAD_A),
         "user 1 on None no longer drives the menu");
   t_menu_st.flags &= ~MENU_ST_FLAG_ALIVE;
}

/* The users on user 1's core port fire their own controllers' hotkeys,
 * never the set's, which stays user 1's pad's; no other user does, and
 * a user on None shares nothing. */
static void test_hotkeys_shared(void)
{
   input_bits_t bits;
   t_reset();
   input_autoconf_binds[1] = t_auto_own;
   t_bind(RARCH_FAST_FORWARD_KEY, T_FF);
   t_buttons[1] = T_BIT(T_OWN_FF) | T_BIT(T_FF);
   t_collect(&bits);
   CHECK(!BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "the identity map: user 2 fires the hotkeys");
   CHECK(!t_button_reads[1],
         "the identity map: user 2's pad read for the hotkeys");

   t_map(1, 0);
   t_buttons[1] = T_BIT(T_OWN_FF);
   t_collect(&bits);
   CHECK(BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "user 2 on core port 1 does not fire its profile's hotkey");
   t_buttons[1] = T_BIT(T_FF);
   t_collect(&bits);
   CHECK(!BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "user 2 on core port 1 fires the set's hotkey");
   t_buttons[1] = 0;
   t_buttons[0] = T_BIT(T_FF);
   t_collect(&bits);
   CHECK(BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "user 1's pad lost the set to a shared core port");
   t_buttons[0] = T_BIT(T_OWN_FF);
   t_collect(&bits);
   CHECK(!BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "user 2's profile hotkey fires on user 1's pad");
   t_buttons[0] = 0;
   t_buttons[2] = T_BIT(T_FF);
   t_collect(&bits);
   CHECK(!BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "user 3, on its own core port, fires the hotkeys");

   t_map(0, 1);
   t_buttons[2] = 0;
   t_buttons[1] = T_BIT(T_OWN_FF) | T_BIT(T_FF);
   t_collect(&bits);
   CHECK(!BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "swapped: user 2 fires the hotkeys");

   t_map(0, MAX_USERS);
   t_map(1, MAX_USERS);
   t_collect(&bits);
   CHECK(!BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "user 2 on None fires the hotkeys with user 1 on None");
}

/* Hotkey Enable goes with the hotkeys of the controller it is held on,
 * a sharer's its own, not the set's; held on any of them, it holds
 * game input back. */
static void test_hotkey_enable_shared(void)
{
   input_bits_t bits;
   input_driver_state_t *st = input_state_get_ptr();

   t_reset();
   input_autoconf_binds[1] = t_auto_en;
   input_autoconf_binds[3] = t_auto_en;
   t_bind(RARCH_ENABLE_HOTKEY, T_ENABLE);
   t_bind(RARCH_FAST_FORWARD_KEY, T_FF);
   t_buttons[3] = T_BIT(T_OWN_EN) | T_BIT(T_OWN_FF);
   t_collect(&bits);
   CHECK(!BIT256_GET(bits, RARCH_FAST_FORWARD_KEY)
         && !(st->flags & INP_FLAG_BLOCK_LIBRETRO_INPUT),
         "the identity map: user 4's pad fires the hotkeys");

   t_map(1, 0);
   t_buttons[3] = 0;
   t_buttons[1] = T_BIT(T_OWN_EN) | T_BIT(T_OWN_FF);
   t_collect(&bits);
   CHECK(BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "Hotkey Enable and a hotkey on user 2's pad do not fire it");
   CHECK(st->flags & INP_FLAG_BLOCK_LIBRETRO_INPUT,
         "Hotkey Enable on user 2's pad does not hold game input back");

   t_buttons[1] = T_BIT(T_OWN_FF);
   t_collect(&bits);
   CHECK(!BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "a hotkey on user 2's pad fires without its Hotkey Enable");
   t_buttons[1] = T_BIT(T_OWN_EN) | T_BIT(T_FF);
   t_collect(&bits);
   CHECK(!BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "the set's hotkey fires on user 2's pad");

   t_buttons[1] = 0;
   t_buttons[0] = T_BIT(T_ENABLE) | T_BIT(T_FF);
   t_collect(&bits);
   CHECK(BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "Hotkey Enable and a hotkey on user 1's pad do not fire it");
   t_buttons[0] = T_BIT(T_FF);
   t_collect(&bits);
   CHECK(!BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "a hotkey on user 1's pad fires without Hotkey Enable");

   t_buttons[0] = T_BIT(T_ENABLE);
   t_buttons[1] = T_BIT(T_OWN_FF);
   t_collect(&bits);
   CHECK(!BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "Hotkey Enable on user 1's pad fires user 2's hotkey");

   t_buttons[0] = T_BIT(T_FF);
   t_buttons[1] = T_BIT(T_OWN_EN);
   t_collect(&bits);
   CHECK(!BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "Hotkey Enable on user 2's pad fires user 1's hotkey");
}

/* The menu toggle's button, which acts on being let go, and its
 * controller combination work from either pad on core port 1: a
 * sharer's button its own controller's. */
static void test_menu_toggle_shared(void)
{
   input_bits_t bits;
   t_reset();
   input_autoconf_binds[1] = t_auto_own;
   t_bind(RARCH_MENU_TOGGLE, T_MENU);
   settings.uints.input_menu_toggle_gamepad_combo = INPUT_COMBO_START_SELECT;
   t_map(1, 0);

   t_buttons[1] = T_BIT(T_OWN_MENU);
   t_collect(&bits);
   t_buttons[1] = 0;
   t_collect(&bits);
   CHECK(BIT256_GET(bits, RARCH_MENU_TOGGLE),
         "user 2's menu toggle button does not open the menu");
   t_buttons[1] = T_BIT(T_MENU);
   t_collect(&bits);
   t_buttons[1] = 0;
   t_collect(&bits);
   CHECK(!BIT256_GET(bits, RARCH_MENU_TOGGLE),
         "the set's menu toggle button on user 2 opens the menu");
   t_buttons[0] = T_BIT(T_MENU);
   t_collect(&bits);
   t_buttons[0] = 0;
   t_collect(&bits);
   CHECK(BIT256_GET(bits, RARCH_MENU_TOGGLE),
         "user 1's menu toggle button does not open the menu");

   /* a button no RetroPad button shares needs no Hotkey Enable */
   t_bind(RARCH_ENABLE_HOTKEY, T_ENABLE);
   t_buttons[1] = T_BIT(T_OWN_MENU);
   t_collect(&bits);
   t_buttons[1] = 0;
   t_collect(&bits);
   CHECK(BIT256_GET(bits, RARCH_MENU_TOGGLE),
         "user 2's menu toggle button waits for Hotkey Enable");

   t_menu_st.flags |= MENU_ST_FLAG_ALIVE;
   t_buttons[1] = T_BIT(RETRO_DEVICE_ID_JOYPAD_START)
      | T_BIT(RETRO_DEVICE_ID_JOYPAD_SELECT);
   t_collect(&bits);
   CHECK(BIT256_GET(bits, RARCH_MENU_TOGGLE),
         "the menu toggle combination on user 2's pad does not close the menu");
   t_buttons[1] = 0;
   t_buttons[0] = T_BIT(RETRO_DEVICE_ID_JOYPAD_START)
      | T_BIT(RETRO_DEVICE_ID_JOYPAD_SELECT);
   t_collect(&bits);
   CHECK(BIT256_GET(bits, RARCH_MENU_TOGGLE),
         "the menu toggle combination on user 1's pad does not close the menu");
   t_menu_st.flags &= ~MENU_ST_FLAG_ALIVE;
}

/* Hotkeys Follow Player 1: the hotkeys are core port 1's users',
 * wherever user 1 is; the set is the first one's. */
static void test_hotkeys_follow(void)
{
   input_bits_t bits;
   t_reset();
   settings.bools.input_hotkey_follows_player1 = true;
   input_autoconf_binds[1] = t_auto_own;
   t_bind(RARCH_FAST_FORWARD_KEY, T_FF);
   t_map(1, 0);

   t_buttons[1] = T_BIT(T_FF);
   t_collect(&bits);
   CHECK(!BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "Follow Player 1: user 2 on core port 1 fires the set's hotkeys");
   t_buttons[1] = T_BIT(T_OWN_FF);
   t_collect(&bits);
   CHECK(BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "Follow Player 1: user 2 on core port 1 does not fire its own");
   t_buttons[1] = 0;
   t_buttons[0] = T_BIT(T_FF);
   t_collect(&bits);
   CHECK(BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "Follow Player 1: user 1's pad does not fire the hotkeys");

   /* user 1 moves to core port 3 */
   t_map(0, 2);
   t_collect(&bits);
   CHECK(!BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "Follow Player 1: user 1's pad on core port 3 fires the hotkeys");
   t_buttons[0] = 0;
   t_buttons[1] = T_BIT(T_FF);
   t_collect(&bits);
   CHECK(BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "Follow Player 1: user 2 alone on core port 1 does not fire them");
}

/* User 1 on None: core port 1's users drive the menu and fire their
 * own hotkeys, each by its own Hotkey Enable, and user 1 keeps both. */
static void test_none_player1(void)
{
   input_bits_t bits;
   t_reset();
   input_autoconf_binds[1] = t_auto_en;
   t_bind(RARCH_ENABLE_HOTKEY, T_ENABLE);
   t_bind(RARCH_FAST_FORWARD_KEY, T_FF);
   t_map(0, MAX_USERS);
   t_map(1, 0);

   t_menu_st.flags |= MENU_ST_FLAG_ALIVE;
   t_buttons[1] = T_B;
   t_collect(&bits);
   CHECK(BIT256_GET(bits, RETRO_DEVICE_ID_JOYPAD_B),
         "user 1 on None: user 2 on core port 1 does not drive the menu");
   t_buttons[1] = 0;
   t_buttons[2] = T_Y;
   t_collect(&bits);
   CHECK(!BIT256_GET(bits, RETRO_DEVICE_ID_JOYPAD_Y),
         "user 1 on None: user 3, on its own core port, drives the menu");
   t_buttons[2] = 0;
   t_buttons[0] = T_A;
   t_collect(&bits);
   CHECK(BIT256_GET(bits, RETRO_DEVICE_ID_JOYPAD_A),
         "user 1 on None no longer drives the menu");
   t_buttons[0] = 0;
   t_menu_st.flags &= ~MENU_ST_FLAG_ALIVE;

   t_buttons[1] = T_BIT(T_OWN_EN) | T_BIT(T_OWN_FF);
   t_collect(&bits);
   CHECK(BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "user 1 on None: Hotkey Enable and a hotkey on user 2's pad do "
         "not fire it");
   t_buttons[1] = T_BIT(T_OWN_FF);
   t_collect(&bits);
   CHECK(!BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "user 1 on None: a hotkey on user 2's pad fires without Hotkey "
         "Enable");
   t_buttons[1] = T_BIT(T_OWN_EN) | T_BIT(T_FF);
   t_collect(&bits);
   CHECK(!BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "user 1 on None: the set's hotkey fires on user 2's pad");
   t_buttons[1] = 0;
   t_buttons[2] = T_BIT(T_ENABLE) | T_BIT(T_FF);
   t_collect(&bits);
   CHECK(!BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "user 1 on None: user 3, on its own core port, fires user 1's "
         "hotkeys");
   t_buttons[2] = 0;
   t_buttons[0] = T_BIT(T_ENABLE) | T_BIT(T_FF);
   t_collect(&bits);
   CHECK(BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "user 1 on None lost its hotkeys");
}

#define T_X      T_BIT(RETRO_DEVICE_ID_JOYPAD_X)
#define T_SELECT T_BIT(RETRO_DEVICE_ID_JOYPAD_SELECT)
#define T_START  T_BIT(RETRO_DEVICE_ID_JOYPAD_START)

/* A frame with the buttons as set, then one with all let go: whether
 * the menu toggles as they are let go. */
static bool t_menu_tap(void)
{
   input_bits_t bits;
   unsigned u;
   t_collect(&bits);
   for (u = 0; u < MAX_USERS; u++)
      t_buttons[u] = 0;
   t_collect(&bits);
   return BIT256_GET(bits, RARCH_MENU_TOGGLE) != 0;
}

/* Menu toggle on RetroPad X, Hotkey Enable on Select: X is a game
 * button, and opens the menu only with Select on user 1's pad; the
 * set is not a sharer's. */
static void test_menu_toggle_game_button(void)
{
   t_reset();
   t_bind(RARCH_ENABLE_HOTKEY, RETRO_DEVICE_ID_JOYPAD_SELECT);
   t_bind(RARCH_MENU_TOGGLE, RETRO_DEVICE_ID_JOYPAD_X);
   t_buttons[0] = T_X;
   CHECK(!t_menu_tap(), "the identity map: X on user 1's pad opens the menu");
   t_buttons[0] = T_SELECT | T_X;
   CHECK(t_menu_tap(),
         "the identity map: Select+X on user 1's pad does not open the menu");

   t_map(1, 0);
   t_buttons[1] = T_X;
   CHECK(!t_menu_tap(), "X on user 2's pad opens the menu");
   t_buttons[1] = T_SELECT | T_X;
   CHECK(!t_menu_tap(), "Select+X on user 2's pad, the set's, opens the menu");
   t_buttons[0] = T_X;
   CHECK(!t_menu_tap(), "X on user 1's pad opens the menu, user 2 sharing");
   t_buttons[0] = T_SELECT | T_X;
   CHECK(t_menu_tap(),
         "Select+X on user 1's pad does not open the menu, user 2 sharing");
   t_buttons[0] = T_SELECT;
   t_buttons[1] = T_X;
   CHECK(!t_menu_tap(), "Select on user 1's pad and X on user 2's open the menu");
   t_buttons[0] = T_X;
   t_buttons[1] = T_SELECT;
   CHECK(!t_menu_tap(), "Select on user 2's pad and X on user 1's open the menu");
}

/* The menu toggle combination may be held across the pads on core
 * port 1. */
static void test_menu_combo_split(void)
{
   input_bits_t bits;
   t_reset();
   settings.uints.input_menu_toggle_gamepad_combo = INPUT_COMBO_START_SELECT;
   t_map(1, 0);
   t_buttons[0] = T_START;
   t_buttons[1] = T_SELECT;
   t_collect(&bits);
   CHECK(BIT256_GET(bits, RARCH_MENU_TOGGLE),
         "Start on user 1's pad and Select on user 2's do not open the menu");
   t_buttons[0] = 0;
   t_buttons[1] = 0;
   t_collect(&bits);
}

/* A user sharing core port 1 fires its hotkeys with the menu up. */
static void test_hotkeys_shared_menu(void)
{
   input_bits_t bits;
   t_reset();
   input_autoconf_binds[1] = t_auto_own;
   t_bind(RARCH_FAST_FORWARD_KEY, T_FF);
   t_map(1, 0);
   t_menu_st.flags |= MENU_ST_FLAG_ALIVE;
   t_buttons[1] = T_BIT(T_OWN_FF);
   t_collect(&bits);
   CHECK(BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "with the menu up, user 2 on core port 1 does not fire its hotkeys");
   t_buttons[1] = T_BIT(T_FF);
   t_collect(&bits);
   CHECK(!BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "with the menu up, user 2 on core port 1 fires the set's hotkeys");
   t_buttons[1] = 0;
   t_menu_st.flags &= ~MENU_ST_FLAG_ALIVE;
}

/* the key held on the keyboard, or 0 */
static unsigned t_key;
/* A keyboard driver asked for each bind; @binds is input_config_binds,
 * its pointers cast. */
static int16_t t_kb_state(void *data, const input_device_driver_t *joypad,
      rarch_joypad_info_t *joypad_info, const retro_keybind_set *binds,
      unsigned port, unsigned device, unsigned idx, unsigned id)
{
   (void)data;
   (void)joypad;
   (void)joypad_info;
   (void)binds;
   (void)idx;
   if (!t_key || port >= MAX_USERS)
      return 0;
   if (device == RETRO_DEVICE_KEYBOARD)
      return id == t_key;
   return device == RETRO_DEVICE_JOYPAD && id < RARCH_BIND_LIST_END
      && RETRO_KEYBIND_KEY(&input_config_binds[port][id]) == t_key;
}
static input_driver_t t_kb;

/* Hotkey Enable on the keyboard and on the pads: held on the keyboard,
 * it opens the hotkeys of every pad on core port 1. */
static void test_hotkey_enable_keyboard(void)
{
   input_bits_t bits;
   input_driver_state_t *st = input_state_get_ptr();
   t_reset();
   t_kb.input_state   = t_kb_state;
   st->current_driver = &t_kb;
   input_autoconf_binds[1] = t_auto_en;
   t_bind(RARCH_ENABLE_HOTKEY, T_ENABLE);
   input_config_binds[0][RARCH_ENABLE_HOTKEY].attr =
      RETRO_KEYBIND_ATTR(RETROK_TAB, NO_BTN, true);
   t_bind(RARCH_FAST_FORWARD_KEY, T_FF);
   t_map(1, 0);

   t_buttons[1] = T_BIT(T_OWN_FF);
   t_collect(&bits);
   CHECK(!BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "a hotkey on user 2's pad fires with no Hotkey Enable held");
   t_key = RETROK_TAB;
   t_collect(&bits);
   CHECK(BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "Hotkey Enable on the keyboard does not open user 2's hotkeys");
   t_key              = 0;
   st->current_driver = NULL;
}

/* "Hotkey Enable Tap Delay" per controller. User 1's Enable is its
 * Select; user 2, on core port 1 too, has its own on its Start. */
static struct retro_keybind t_auto_tap[RARCH_BIND_LIST_END];

static void t_tap_setup(unsigned tap_delay)
{
   input_bits_t bits;
   unsigned i;
   t_reset();
   for (i = 0; i < RARCH_BIND_LIST_END; i++)
      t_auto_tap[i] = t_auto_en[i];
   t_auto_tap[RARCH_ENABLE_HOTKEY].joykey = RETRO_DEVICE_ID_JOYPAD_START;
   input_autoconf_binds[1] = t_auto_tap;
   t_bind(RARCH_ENABLE_HOTKEY, RETRO_DEVICE_ID_JOYPAD_SELECT);
   t_bind(RARCH_FAST_FORWARD_KEY, T_FF);
   t_map(1, 0);
   /* the default: held, the game sees Enable's game button a few frames */
   settings.uints.input_hotkey_block_delay = 5;
   settings.uints.input_hotkey_tap_delay   = tap_delay;
   /* set from the menu, which forgets what the tap saw before */
   t_menu_st.flags |= MENU_ST_FLAG_ALIVE;
   t_collect(&bits);
   t_menu_st.flags &= ~MENU_ST_FLAG_ALIVE;
}

static void t_tap_done(void)
{
   input_bits_t bits;
   t_buttons[0] = t_buttons[1] = 0;
   t_key        = 0;
   input_state_get_ptr()->current_driver   = NULL;
   settings.uints.input_hotkey_tap_delay   = 0;
   settings.uints.input_hotkey_block_delay = 0;
   t_collect(&bits);
}

/* Slot @slot holds @press, and the keyboard @key, for @held frames,
 * then nothing for five: the frames core port @port saw Start and
 * Select ([0], [1]) while held and after. */
static void t_tap_run(unsigned slot, uint32_t press, unsigned key,
      unsigned held, unsigned port, unsigned during[2], unsigned after[2])
{
   input_bits_t bits;
   unsigned i;
   during[0] = during[1] = after[0] = after[1] = 0;
   for (i = 0; i < held + 5; i++)
   {
      unsigned *n     = (i < held) ? during : after;
      unsigned r;
      t_buttons[slot] = (i < held) ? press : 0;
      t_key           = (i < held) ? key   : 0;
      t_collect(&bits);
      r     = t_read(port);
      n[0] += (r & T_START)  ? 1 : 0;
      n[1] += (r & T_SELECT) ? 1 : 0;
   }
}

/* Slot @slot holds @en for two frames, then @hotkey with it for two,
 * then nothing for five: whether fast-forward fired, and the frames
 * core port 1 saw Start or Select. */
static bool t_tap_combo(unsigned slot, uint32_t en, uint32_t hotkey,
      unsigned *seen)
{
   input_bits_t bits;
   unsigned i;
   bool ff = false;
   *seen   = 0;
   for (i = 0; i < 9; i++)
   {
      t_buttons[slot] = (i < 2) ? en : (i < 4) ? (en | hotkey) : 0;
      t_collect(&bits);
      if (BIT256_GET(bits, RARCH_FAST_FORWARD_KEY))
         ff = true;
      if (t_read(0) & (T_START | T_SELECT))
         (*seen)++;
   }
   return ff;
}

/* A sharer's tap: kept from the game while down, then played, its own
 * Enable button alone, on the core port it plays on. */
static void test_tap_sharer(void)
{
   unsigned during[2], after[2];
   t_tap_setup(100000);
   t_tap_run(1, T_START, 0, 1, 0, during, after);
   CHECK(!during[0], "tap delay: the game saw a sharer's Enable while down");
   CHECK(after[0] == 2,
         "tap delay: a sharer's one-frame tap was not two frames of its Start");
   CHECK(!during[1] && !after[1],
         "tap delay: a sharer's tap played user 1's Enable button");
   t_tap_run(1, T_START, 0, 4, 0, during, after);
   CHECK(!during[0] && after[0] == 4 && !after[1],
         "tap delay: a sharer's four-frame tap was not four frames of its Start alone");

   t_map(0, 1);
   t_map(1, 1);
   t_tap_run(1, T_START, 0, 1, 1, during, after);
   CHECK(!during[0] && after[0] == 2 && !after[1],
         "tap delay: on core port 2, a sharer's tap was not its Start there");
   t_tap_run(1, T_START, 0, 1, 0, during, after);
   CHECK(!after[0] && !after[1],
         "tap delay: on core port 2, a sharer's tap played on core port 1");
   t_tap_done();
}

/* A sharer's Enable held for its own hotkey never reaches the game. */
static void test_tap_sharer_hotkey(void)
{
   unsigned seen;
   bool ff;
   t_tap_setup(100000);
   ff = t_tap_combo(1, T_START, T_BIT(T_OWN_FF), &seen);
   CHECK(ff, "tap delay: a sharer's Enable and its own hotkey did not fire it");
   CHECK(!seen, "tap delay: the game saw a sharer's Enable used for a hotkey");
   t_tap_done();
}

/* User 1's tap with a sharer as with none: Select kept while down and
 * played on release, never the sharer's Start; used for a hotkey,
 * never played. */
static void test_tap_hotkey_user(void)
{
   unsigned during[2], after[2], seen;
   bool ff;
   t_tap_setup(100000);
   t_tap_run(0, T_SELECT, 0, 1, 0, during, after);
   CHECK(!during[1] && after[1] == 2 && !during[0] && !after[0],
         "tap delay: user 1's one-frame tap was not two frames of Select alone");
   t_tap_run(0, T_SELECT, 0, 4, 0, during, after);
   CHECK(!during[1] && after[1] == 4 && !after[0],
         "tap delay: user 1's four-frame tap was not four frames of Select alone");
   ff = t_tap_combo(0, T_SELECT, T_BIT(T_FF), &seen);
   CHECK(ff && !seen,
         "tap delay: user 1's Enable used for a hotkey reached the game");
   t_tap_done();
}

/* User 1's Enable also on the keyboard: a tap of the key plays user 1's
 * Select as with nobody sharing, never a sharer's Enable button. */
static void test_tap_keyboard(void)
{
   unsigned during[2], after[2];
   t_tap_setup(100000);
   t_kb.input_state                       = t_kb_state;
   input_state_get_ptr()->current_driver  = &t_kb;
   input_config_binds[0][RARCH_ENABLE_HOTKEY].attr =
      RETRO_KEYBIND_ATTR(RETROK_TAB, NO_BTN, true);
   input_config_binds_changed();
   t_tap_run(0, 0, RETROK_TAB, 1, 0, during, after);
   CHECK(!during[1] && after[1] == 2 && !during[0] && !after[0],
         "tap delay: a tap of the keyboard's Enable was not user 1's Select alone");
   t_tap_done();
}

/* A sharer with no Enable of its own has no tap: its Start is the
 * game's at once and never played again. */
static void test_tap_sharer_no_enable(void)
{
   unsigned during[2], after[2];
   t_tap_setup(100000);
   input_autoconf_binds[1] = t_auto_own;
   input_config_binds_changed();
   t_tap_run(1, T_START, 0, 1, 0, during, after);
   CHECK(during[0] == 1 && !after[0] && !after[1],
         "tap delay: the Start of a sharer with no Enable was kept or played");
   t_tap_done();
}

/* Taps that overlap, each on its own controller. User 3's Enable is
 * its L. */
static struct retro_keybind t_auto_tap3[RARCH_BIND_LIST_END];
#define T_L          T_BIT(RETRO_DEVICE_ID_JOYPAD_L)
#define T_TAP_FRAMES 12
/* frames @a to @b - 1, as bits */
#define T_FRAMES(a, b) (((1u << (b)) - 1) & ~((1u << (a)) - 1))

struct t_hold
{
   unsigned slot;
   uint32_t buttons;
   unsigned from; /* the first frame held */
   unsigned to;   /* the first frame let go */
};

/* @n holds over twelve frames: what core port @port read each frame,
 * and whether fast-forward fired. */
static bool t_tap_holds(const struct t_hold *h, unsigned n, unsigned port,
      unsigned seen[T_TAP_FRAMES])
{
   input_bits_t bits;
   unsigned f, i;
   bool ff = false;
   for (f = 0; f < T_TAP_FRAMES; f++)
   {
      t_buttons[0] = t_buttons[1] = t_buttons[2] = 0;
      for (i = 0; i < n; i++)
         if (f >= h[i].from && f < h[i].to)
            t_buttons[h[i].slot] |= h[i].buttons;
      t_collect(&bits);
      if (BIT256_GET(bits, RARCH_FAST_FORWARD_KEY))
         ff = true;
      seen[f] = t_read(port);
   }
   t_buttons[2] = 0;
   return ff;
}

/* The frames, as bits, on which @seen has @button. */
static unsigned t_tap_frames(const unsigned seen[T_TAP_FRAMES],
      unsigned button)
{
   unsigned f;
   unsigned frames = 0;
   for (f = 0; f < T_TAP_FRAMES; f++)
      if (seen[f] & button)
         frames |= 1u << f;
   return frames;
}

/* A pad's poll comes after the hotkey pass, so the core sees a press a
 * frame before the pass does. Slot @slot holds @en for @held frames,
 * and @hotkey on frames @hk_from to @hk_to - 1, the pass reading each
 * frame's buttons a frame late; 2 ms pass after frame @wait. Returns
 * the frames core port 1 read @en, as bits; @ff, the frames
 * fast-forward fired. */
static unsigned t_tap_polled(unsigned slot, uint32_t en, unsigned held,
      uint32_t hotkey, unsigned hk_from, unsigned hk_to, unsigned wait,
      unsigned *ff)
{
   input_bits_t bits;
   unsigned f;
   unsigned frames = 0;
   uint32_t prev   = 0;
   *ff             = 0;
   for (f = 0; f < held + 6; f++)
   {
      uint32_t cur = (f < held) ? en : 0;
      if (f >= hk_from && f < hk_to)
         cur |= hotkey;
      t_buttons[slot] = prev;
      t_collect(&bits);
      if (BIT256_GET(bits, RARCH_FAST_FORWARD_KEY))
         (*ff)++;
      t_buttons[slot] = cur;
      if (t_read(0) & en)
         frames |= 1u << f;
      if (f == wait)
      {
         retro_time_t t0 = cpu_features_get_time_usec();
         while (cpu_features_get_time_usec() - t0 < 2000);
      }
      prev = cur;
   }
   t_buttons[slot] = 0;
   return frames;
}

/* User 1's and a sharer's Enable, polled a frame before the hotkey
 * pass sees it: kept from the game from the first frame. */
static void test_tap_polled(void)
{
   unsigned frames, ff;
   t_tap_setup(100000);
   frames = t_tap_polled(0, T_SELECT, 4, 0, 0, 0, ~0u, &ff);
   CHECK(frames == T_FRAMES(5, 9),
         "tap delay, polled: user 1's four-frame tap was not four frames of Select on release");
   frames = t_tap_polled(1, T_START, 4, 0, 0, 0, ~0u, &ff);
   CHECK(frames == T_FRAMES(5, 9),
         "tap delay, polled: a sharer's four-frame tap was not four frames of Start on release");
   frames = t_tap_polled(0, T_SELECT, 8, T_BIT(T_FF), 3, 6, ~0u, &ff);
   CHECK(!frames && ff == 3,
         "tap delay, polled: user 1's Enable used for a hotkey reached the game");
   frames = t_tap_polled(1, T_START, 8, T_BIT(T_OWN_FF), 3, 6, ~0u, &ff);
   CHECK(!frames && ff == 3,
         "tap delay, polled: a sharer's Enable used for its hotkey reached the game");
   /* a sharer's Select is not user 1's Enable */
   frames = t_tap_polled(1, T_SELECT, 4, 0, 0, 0, ~0u, &ff);
   CHECK(frames == T_FRAMES(0, 4),
         "tap delay, polled: a sharer's Select was kept as user 1's Enable");
   t_tap_done();

   t_tap_setup(1);
   frames = t_tap_polled(0, T_SELECT, 6, 0, 0, 0, 1, &ff);
   CHECK(frames == T_FRAMES(2, 6),
         "tap delay, polled: user 1's Enable held past the delay was not the game's from then alone");
   frames = t_tap_polled(1, T_START, 6, 0, 0, 0, 1, &ff);
   CHECK(frames == T_FRAMES(2, 6),
         "tap delay, polled: a sharer's Enable held past the delay was not the game's from then alone");
   t_tap_done();
}

/* Two sharers' taps, one inside the other: each kept while down and
 * played on its own release, for its own length, its own Enable
 * button alone, on the core port they play on. */
static void test_tap_overlap_sharers(void)
{
   static const struct t_hold h[] = {
      { 2, T_L,     0, 4 },
      { 1, T_START, 1, 3 } };
   unsigned seen[T_TAP_FRAMES];
   unsigned i;
   t_tap_setup(100000);
   for (i = 0; i < RARCH_BIND_LIST_END; i++)
      t_auto_tap3[i] = t_auto_en[i];
   t_auto_tap3[RARCH_ENABLE_HOTKEY].joykey = RETRO_DEVICE_ID_JOYPAD_L;
   input_autoconf_binds[2] = t_auto_tap3;
   input_config_binds_changed();
   t_map(2, 0);
   t_tap_holds(h, 2, 0, seen);
   CHECK(t_tap_frames(seen, T_START) == T_FRAMES(3, 5),
         "tap delay: a sharer's tap inside another's was not its Start on its own release");
   CHECK(t_tap_frames(seen, T_L) == T_FRAMES(4, 8),
         "tap delay: a sharer's tap around another's was not its L on its own release");
   CHECK(!t_tap_frames(seen, T_SELECT),
         "tap delay: two sharers' taps played user 1's Select");

   t_map(0, 1);
   t_map(1, 1);
   t_map(2, 1);
   t_tap_holds(h, 2, 1, seen);
   CHECK(   t_tap_frames(seen, T_START) == T_FRAMES(3, 5)
         && t_tap_frames(seen, T_L) == T_FRAMES(4, 8),
         "tap delay: on core port 2, two sharers' taps were not each its own there");
   t_tap_done();
}

/* User 1's tap and a sharer's, one inside the other: each played on
 * its own release, its own Enable button alone. */
static void test_tap_overlap_user(void)
{
   static const struct t_hold h1[] = {
      { 0, T_SELECT, 0, 4 },
      { 1, T_START,  1, 3 } };
   static const struct t_hold h2[] = {
      { 1, T_START,  0, 4 },
      { 0, T_SELECT, 1, 2 } };
   unsigned seen[T_TAP_FRAMES];
   t_tap_setup(100000);
   t_tap_holds(h1, 2, 0, seen);
   CHECK(   t_tap_frames(seen, T_SELECT) == T_FRAMES(4, 8)
         && t_tap_frames(seen, T_START)  == T_FRAMES(3, 5),
         "tap delay: user 1's tap and a sharer's inside it were not each played on its own release");
   t_tap_holds(h2, 2, 0, seen);
   CHECK(   t_tap_frames(seen, T_START)  == T_FRAMES(4, 8)
         && t_tap_frames(seen, T_SELECT) == T_FRAMES(2, 4),
         "tap delay: a sharer's tap and user 1's inside it were not each played on its own release");
   t_tap_done();
}

/* A hotkey swallows the tap of the controller that fired it, and no
 * other controller's. */
static void test_tap_overlap_hotkey(void)
{
   static const struct t_hold h1[] = {
      { 0, T_SELECT,    0, 5 },
      { 0, T_BIT(T_FF), 2, 5 },
      { 1, T_START,     1, 3 } };
   static const struct t_hold h2[] = {
      { 1, T_START,         0, 5 },
      { 1, T_BIT(T_OWN_FF), 2, 5 },
      { 0, T_SELECT,        1, 2 } };
   unsigned seen[T_TAP_FRAMES];
   bool ff;
   t_tap_setup(100000);
   ff = t_tap_holds(h1, 3, 0, seen);
   CHECK(ff && !t_tap_frames(seen, T_SELECT),
         "tap delay: user 1's Enable used for a hotkey reached the game beside a sharer's tap");
   CHECK(t_tap_frames(seen, T_START) == T_FRAMES(3, 5),
         "tap delay: user 1's hotkey swallowed a sharer's tap");
   ff = t_tap_holds(h2, 3, 0, seen);
   CHECK(ff && !t_tap_frames(seen, T_START),
         "tap delay: a sharer's Enable used for its hotkey reached the game beside user 1's tap");
   CHECK(t_tap_frames(seen, T_SELECT) == T_FRAMES(2, 4),
         "tap delay: a sharer's hotkey swallowed user 1's tap");
   t_tap_done();
}

#define T_R T_BIT(RETRO_DEVICE_ID_JOYPAD_R)

/* Slot @slot holds @en past a tap delay of 1 ms and the block delay. */
static void t_tap_pass(unsigned slot, uint32_t en)
{
   input_bits_t bits;
   retro_time_t t0;
   unsigned i;
   t_buttons[slot] = en;
   t_collect(&bits);
   t0 = cpu_features_get_time_usec();
   while (cpu_features_get_time_usec() - t0 < 2000)
      t_collect(&bits);
   for (i = 0; i < 6; i++)
      t_collect(&bits);
}

/* One controller's Enable held past the delay is the game's, but
 * another's Enable down blocks game input, that hold included, as
 * with the delay off. */
static void test_tap_enable_blocks(void)
{
   static const struct t_hold h1[] = {
      { 0, T_SELECT,      0, 10 },
      { 1, T_START | T_R, 2, 5 } };
   static const struct t_hold h2[] = {
      { 1, T_START,        0, 10 },
      { 0, T_SELECT | T_X, 2, 5 } };
   unsigned seen[T_TAP_FRAMES];
   bool ff;
   t_tap_setup(1);
   t_auto_tap[RARCH_FAST_FORWARD_KEY].joykey = RETRO_DEVICE_ID_JOYPAD_R;
   t_bind(RARCH_FAST_FORWARD_KEY, RETRO_DEVICE_ID_JOYPAD_X);
   t_tap_pass(0, T_SELECT);
   ff = t_tap_holds(h1, 2, 0, seen);
   CHECK(t_tap_frames(seen, T_SELECT) == (T_FRAMES(0, 2) | T_FRAMES(5, 10)),
         "tap delay: a sharer's Enable did not block user 1's Enable held past the delay, or it did not come back");
   CHECK(ff && !t_tap_frames(seen, T_START | T_R),
         "tap delay: a sharer's Enable and hotkey reached the game beside user 1's Enable held past the delay");
   t_tap_pass(1, T_START);
   ff = t_tap_holds(h2, 2, 0, seen);
   CHECK(t_tap_frames(seen, T_START) == (T_FRAMES(0, 2) | T_FRAMES(5, 10)),
         "tap delay: user 1's Enable did not block a sharer's Enable held past the delay, or it did not come back");
   CHECK(ff && !t_tap_frames(seen, T_SELECT | T_X),
         "tap delay: user 1's Enable and hotkey reached the game beside a sharer's Enable held past the delay");
   t_tap_done();
}

/* As t_tap_holds(), on core port 1, with user 1's Enable also on Tab
 * and Tab held on frames 0 to 4. */
static bool t_tap_tab_holds(const struct t_hold *h, unsigned n,
      unsigned seen[T_TAP_FRAMES])
{
   input_bits_t bits;
   unsigned f, i;
   bool ff = false;
   t_kb.input_state                      = t_kb_state;
   input_state_get_ptr()->current_driver = &t_kb;
   input_config_binds[0][RARCH_ENABLE_HOTKEY].attr =
      RETRO_KEYBIND_ATTR(RETROK_TAB, NO_BTN, true);
   input_config_binds_changed();
   for (f = 0; f < T_TAP_FRAMES; f++)
   {
      t_buttons[0] = t_buttons[1] = 0;
      t_key        = (f < 5) ? RETROK_TAB : 0;
      for (i = 0; i < n; i++)
         if (f >= h[i].from && f < h[i].to)
            t_buttons[h[i].slot] |= h[i].buttons;
      t_collect(&bits);
      if (BIT256_GET(bits, RARCH_FAST_FORWARD_KEY))
         ff = true;
      seen[f] = t_read(0);
   }
   return ff;
}

/* A sharer's hotkey that only the keyboard's Enable opened swallows
 * the keyboard's tap too. */
static void test_tap_keyboard_opens_sharer(void)
{
   static const struct t_hold h[] = {
      { 1, T_BIT(T_OWN_FF), 2, 5 } };
   unsigned seen[T_TAP_FRAMES];
   bool ff;
   t_tap_setup(100000);
   ff = t_tap_tab_holds(h, 1, seen);
   CHECK(ff, "tap delay: the keyboard's Enable did not open a sharer's hotkey");
   CHECK(!t_tap_frames(seen, T_SELECT | T_START),
         "tap delay: the keyboard's Enable that opened a sharer's hotkey reached the game");
   t_tap_done();
}

/* A sharer's hotkey that its own Enable opened swallows only its tap,
 * the keyboard's Enable held too. */
static void test_tap_sharer_opens_own(void)
{
   static const struct t_hold h[] = {
      { 1, T_START,         0, 5 },
      { 1, T_BIT(T_OWN_FF), 2, 5 } };
   unsigned seen[T_TAP_FRAMES];
   bool ff;
   t_tap_setup(100000);
   ff = t_tap_tab_holds(h, 2, seen);
   CHECK(ff && !t_tap_frames(seen, T_START),
         "tap delay: a sharer's Enable used for its hotkey reached the game beside the keyboard's tap");
   CHECK(t_tap_frames(seen, T_SELECT) == T_FRAMES(5, 10),
         "tap delay: a sharer's hotkey opened by its own Enable swallowed the keyboard's tap");
   t_tap_done();
}

/* Enables pressed with the delay off are not played once it is turned
 * on outside the menu. */
static void test_tap_turned_on(void)
{
   unsigned during[2], after[2];
   t_tap_setup(0);
   t_tap_run(0, T_SELECT, 0, 1, 0, during, after);
   t_tap_run(1, T_START | T_BIT(T_OWN_FF), 0, 1, 0, during, after);
   settings.uints.input_hotkey_tap_delay = 100000;
   t_tap_run(0, 0, 0, 1, 0, during, after);
   CHECK(!during[0] && !during[1] && !after[0] && !after[1],
         "tap delay: turned on outside the menu, it played an Enable pressed with it off");
   t_tap_done();
}

/* With the delay off nothing is kept or played. */
static void test_tap_off(void)
{
   unsigned during[2], after[2];
   t_tap_setup(0);
   t_tap_run(1, T_START, 0, 1, 0, during, after);
   CHECK(during[0] == 1 && !after[0] && !after[1]
         && !input_state_get_ptr()->enabler_tap_active,
         "tap delay off: a sharer's Enable was kept or played");
   t_tap_run(0, T_SELECT, 0, 1, 0, during, after);
   CHECK(during[1] == 1 && !after[1] && !after[0],
         "tap delay off: user 1's Enable was kept or played");
   t_tap_done();
}

#define T_XPAD_SELECT 6
#define T_XPAD_X      2
#define T_GUIDE       8

/* Slot @slot's controller: its profile and its name, counted as a
 * change, as a connect is. */
static void t_pad(unsigned slot, struct retro_keybind *profile,
      const char *name)
{
   input_autoconf_binds[slot] = profile;
   input_config_set_device_name(slot, name);
   input_config_binds_changed();
}

/* A sharer never fires the set's hand-set buttons, Hotkey Enable, a
 * hotkey or the menu toggle: not on a pad of user 1's model, nor on
 * one that numbers the RetroPad alike, nor on one that does not. */
static void test_hotkeys_sharer_not_set(void)
{
   static const char *const names[3] = { "Xpad", "Xpad B", "Headset" };
   unsigned m;
   input_bits_t bits;
   input_driver_state_t *st = input_state_get_ptr();

   for (m = 0; m < 3; m++)
   {
      int before = failures;
      t_reset();
      t_pad(0, t_xpad, "Xpad");
      t_pad(1, (m < 2) ? t_xpad : t_auto, names[m]);
      t_bind(RARCH_ENABLE_HOTKEY, T_XPAD_SELECT);
      t_bind(RARCH_FAST_FORWARD_KEY, T_XPAD_X);
      t_bind(RARCH_MENU_TOGGLE, T_GUIDE);
      t_map(1, 0);

      t_buttons[1] = T_BIT(T_XPAD_SELECT);
      t_collect(&bits);
      CHECK(!(st->flags & INP_FLAG_BLOCK_LIBRETRO_INPUT),
            "the set's Hotkey Enable held game input back on a sharer");
      t_buttons[1] = T_BIT(T_XPAD_SELECT) | T_BIT(T_XPAD_X);
      t_collect(&bits);
      CHECK(!BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
            "the set's Hotkey Enable and hotkey fired on a sharer");
      t_buttons[1] = T_BIT(T_XPAD_X);
      t_collect(&bits);
      CHECK(!BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
            "the set's hotkey fired on a sharer");
      /* the RetroPad buttons they are on user 1's xpad */
      t_buttons[1] = T_SELECT | T_X;
      t_collect(&bits);
      CHECK(!BIT256_GET(bits, RARCH_FAST_FORWARD_KEY)
            && !(st->flags & INP_FLAG_BLOCK_LIBRETRO_INPUT),
            "the set's binds, by RetroPad meaning, fired on a sharer");
      t_buttons[1] = T_BIT(T_GUIDE);
      CHECK(!t_menu_tap(), "the set's menu toggle opened the menu from a sharer");

      t_buttons[0] = T_BIT(T_XPAD_SELECT) | T_BIT(T_XPAD_X);
      t_collect(&bits);
      CHECK(BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
            "a sharer took the set from user 1's pad");
      t_buttons[0] = 0;
      if (failures != before)
         printf("  (user 2's pad: %s)\n", names[m]);
   }
}

/* Game Focus, which lets a held Hotkey Enable through: a sharer's is
 * its own controller's, not the set's. */
static void test_hotkey_game_focus_shared(void)
{
   input_bits_t bits;
   input_driver_state_t *st = input_state_get_ptr();

   t_reset();
   input_autoconf_binds[1] = t_auto_en;
   t_bind(RARCH_ENABLE_HOTKEY, RETRO_DEVICE_ID_JOYPAD_SELECT);
   t_bind(RARCH_FAST_FORWARD_KEY, T_FF);
   t_map(1, 0);
   st->flags |= INP_FLAG_KB_MAPPING_BLOCKED;

   t_buttons[1] = T_BIT(T_OWN_EN) | T_BIT(T_OWN_FF);
   t_collect(&bits);
   CHECK(BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "Game Focus: user 2's Hotkey Enable does not let its hotkey through");
   t_buttons[1] = T_SELECT | T_BIT(T_OWN_FF);
   t_collect(&bits);
   CHECK(!BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "Game Focus: the set's Hotkey Enable lets user 2's hotkey through");
   t_buttons[1] = 0;
   st->flags   &= ~INP_FLAG_KB_MAPPING_BLOCKED;
}

/* a driver that hands the frontend its keys: none down */
static void t_keys_down(void *data, unsigned port, const uint16_t *keys,
      const uint8_t *bind, unsigned count, uint32_t *down)
{ (void)data; (void)port; (void)keys; (void)bind; (void)count; (void)down; }
static int16_t t_kd_state(void *data, const input_device_driver_t *joypad,
      rarch_joypad_info_t *joypad_info, const retro_keybind_set *binds,
      unsigned port, unsigned device, unsigned idx, unsigned id)
{
   (void)data; (void)joypad; (void)joypad_info; (void)binds; (void)port;
   (void)device; (void)idx; (void)id;
   return 0;
}
static input_driver_t t_kd;

/* With the keys the frontend's, only the hotkeys something may hold
 * are gone through: one that only user 2's profile binds is among
 * them while user 2 shares the port. */
static void test_hotkey_sharer_profile(void)
{
   input_bits_t bits;
   input_driver_state_t *st = input_state_get_ptr();

   t_reset();
   t_kd.input_state   = t_kd_state;
   t_kd.keys_down     = t_keys_down;
   st->current_driver = &t_kd;
   t_pad(0, t_xpad, "Xpad");
   t_pad(1, t_auto_own, "Headset");
   input_config_binds[0][RARCH_FAST_FORWARD_KEY].attr =
      RETRO_KEYBIND_ATTR(RETROK_UNKNOWN, NO_BTN, true);
   input_config_binds_changed();
   t_map(1, 0);

   t_buttons[1] = T_BIT(T_OWN_FF);
   t_collect(&bits);
   CHECK(BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "user 2's own profile's fast-forward is passed over");
   t_buttons[1] = 0;
   t_buttons[0] = T_BIT(T_OWN_FF);
   t_collect(&bits);
   CHECK(!BIT256_GET(bits, RARCH_FAST_FORWARD_KEY),
         "user 2's profile's fast-forward fires on user 1's pad");
   t_buttons[0]       = 0;
   st->current_driver = NULL;
}

int main(void)
{
   test_identity();
   test_shared_port();
   test_swapped_ports();
   test_port_2_users();
   test_rumble_answer();
   test_menu_shared();
   test_hotkeys_shared();
   test_hotkey_enable_shared();
   test_menu_toggle_shared();
   test_hotkeys_follow();
   test_menu_toggle_game_button();
   test_menu_combo_split();
   test_hotkeys_shared_menu();
   test_hotkey_enable_keyboard();
   test_tap_sharer();
   test_tap_sharer_hotkey();
   test_tap_hotkey_user();
   test_tap_keyboard();
   test_tap_sharer_no_enable();
   test_tap_polled();
   test_tap_overlap_sharers();
   test_tap_overlap_user();
   test_tap_overlap_hotkey();
   test_tap_enable_blocks();
   test_tap_keyboard_opens_sharer();
   test_tap_sharer_opens_own();
   test_tap_turned_on();
   test_tap_off();
   test_none_player1();
   test_hotkeys_sharer_not_set();
   test_hotkey_game_focus_shared();
   test_hotkey_sharer_profile();
   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("PASS mapped_port_test\n");
   return 0;
}
