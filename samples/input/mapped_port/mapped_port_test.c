/* Mapped Port in input/input_driver.c: the users mapped to a core port
 * feed it and get its rumble. The real input_driver.c, with the rest
 * of the frontend stood in for. */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

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

/* Slot n holds t_buttons[n]; every slot's profile has the RetroPad's
 * ids as its buttons. */
static uint32_t t_buttons[MAX_USERS];
static unsigned t_rumbled[MAX_USERS];
/* bit n: slot n's motors answer false */
static unsigned t_rumble_refused;
static struct retro_keybind t_auto[RARCH_BIND_LIST_END];
static bool t_query(unsigned pad) { return pad < MAX_USERS; }
static int32_t t_button(unsigned port, uint16_t joykey)
{
   return port < MAX_USERS && joykey < 32
      && ((t_buttons[port] >> joykey) & 1);
}
static int16_t t_state(rarch_joypad_info_t *joypad_info,
      const uint16_t *joykeys, const uint32_t *joyaxes, unsigned port)
{
   unsigned i;
   int16_t ret = 0;
   (void)joyaxes;
   (void)port;
   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
      if (joykeys[i] != NO_BTN && t_button(joypad_info->joy_idx, joykeys[i]))
         ret |= (int16_t)(1 << i);
   return ret;
}
static int16_t t_axis(unsigned port, uint32_t joyaxis)
{ (void)port; (void)joyaxis; return 0; }
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
   settings.floats.input_axis_threshold           = 0.5f;
   st->primary_joypad                             = &t_joypad;
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

int main(void)
{
   test_identity();
   test_shared_port();
   test_swapped_ports();
   test_port_2_users();
   test_rumble_answer();
   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("PASS mapped_port_test\n");
   return 0;
}
