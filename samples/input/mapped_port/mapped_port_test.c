/* Mapped Port in input/input_driver.c: the users mapped to a core port
 * feed it and get its rumble. Plays As: a controller's own player
 * places it, and one playing as another user's player feeds that
 * user's core port; the remap ports stay as they were and the core is
 * told its controllers again when that changes. The real
 * input_driver.c and command.c, with the rest of the frontend stood in
 * for. */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <features/features_cpu.h>

#include <compat/strl.h>
#include <file/config_file.h>
#include <file/file_path.h>

#include "../../../configuration.h"
#include "../../../command.h"
#include "../../../core.h"
#include "../../../dynamic.h"
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
#include "../../../network/netplay/netplay.h"

static settings_t        settings;
static runloop_state_t   runloop_st;
static struct menu_state t_menu_st;
static int               failures;

#define CHECK(c, msg) do { if (!(c)) { \
   printf("FAIL line %d: %s\n", __LINE__, msg); failures++; } } while (0)

/* The core's four ports: how often the core was told its controllers,
 * and what each port was told last (T_UNTOLD: never). */
#define T_UNTOLD 0xffffffffu
static struct retro_controller_info t_core_ports[4];
static unsigned t_inits;
static unsigned t_device[4];

settings_t *config_get_ptr(void) { return &settings; }
runloop_state_t *runloop_state_get_ptr(void) { return &runloop_st; }
void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
#if defined(__linux) && !defined(ANDROID)
/* joypad_drivers[]'s, which no test starts */
input_device_driver_t linuxraw_joypad;
#endif
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
/* as retroarch.c's: command.c tells the core */
bool command_event(enum event_command cmd, void *data)
{
   (void)data;
   if (cmd == CMD_EVENT_CONTROLLER_INIT)
   {
      t_inits++;
      command_event_init_controllers(&runloop_st.system, &settings,
            settings.uints.input_max_users);
   }
   return false;
}
const struct retro_controller_description *
libretro_find_controller_description(
      const struct retro_controller_info *info, unsigned id)
{ (void)info; (void)id; return NULL; }
bool core_set_controller_port_device(retro_ctx_controller_info_t *pad)
{
   if (pad->port < 4)
      t_device[pad->port] = pad->device;
   return true;
}
bool retroarch_ctl(enum rarch_ctl_state state, void *data)
{ (void)state; (void)data; return false; }
/* config_file.c's #include and #reference, which no test reaches */
void path_linked_list_free(struct path_linked_list *l) { (void)l; }
void path_linked_list_add_path(struct path_linked_list *l, char *path)
{ (void)l; (void)path; }
void fill_pathname_resolve_relative(char *s, const char *ref,
      const char *in, size_t len)
{ (void)ref; strlcpy(s, in, len); }
size_t fill_pathname_abbreviated_or_relative(char *s, const char *ref,
      const char *in, size_t len)
{ (void)ref; return strlcpy(s, in, len); }
bool video_driver_has_focus(void) { return true; }
/* netplay on: the ports are the session's */
static bool t_netplay;
bool netplay_driver_ctl(enum rarch_netplay_ctl_state state, void *data)
{
   (void)data;
   return state == RARCH_NETPLAY_CTL_IS_ENABLED && t_netplay;
}
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
 * their own; no reservations or Plays As entries; nothing held. */
static void t_reset(void)
{
   static const uint16_t xpad[RARCH_FIRST_CUSTOM_BIND] = {
      1, 3, 6, 7, 11, 12, 13, 14, 0, 2, 4, 5, 15, 16, 9, 10 };
   unsigned u, i;
   input_driver_state_t *st = input_state_get_ptr();

   memset(&st->mapper, 0, sizeof(st->mapper));
   st->remap_worked = 0;
   t_rumble_refused = 0;
   t_inits          = 0;
   runloop_st.system.ports.data = t_core_ports;
   runloop_st.system.ports.size = 4;
   for (i = 0; i < 4; i++)
      t_device[i] = T_UNTOLD;

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
      input_config_clear_device_display_name(u);
      input_config_plays_as_release(u);
      input_config_set_device_vid(u, 0);
      input_config_set_device_pid(u, 0);
      settings.uints.input_device_reservation_type[u] =
         INPUT_DEVICE_RESERVATION_NONE;
      settings.arrays.input_reserved_devices[u][0]    = '\0';
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
      for (i = 0; i < RARCH_FIRST_CUSTOM_BIND + 8; i++)
         settings.uints.input_remap_ids[u][i] = i;
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
   settings.bools.input_remap_binds_enable        = false;
   st->primary_joypad                             = &t_joypad;
   st->first_press_on                             = false;
   t_netplay                                      = false;
   t_menu_st.flags                               &= ~MENU_ST_FLAG_ALIVE;
   input_plays_as_read(NULL);
   input_remapping_update_port_map();
   st->first_press_pending                        = 0;
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

/* Slot @slot holds a controller called @name with @vid and @pid. */
static void t_dev(unsigned slot, const char *name, uint16_t vid,
      uint16_t pid)
{
   input_config_set_device_name(slot, name);
   input_config_set_device_display_name(slot, name);
   input_config_set_device_vid(slot, vid);
   input_config_set_device_pid(slot, pid);
}

/* Slot @slot's controller gone, as the frontend lets one go. */
static void t_disconnect(unsigned slot)
{
   input_config_clear_device_name(slot);
   input_config_clear_device_display_name(slot);
   input_config_set_device_vid(slot, 0);
   input_config_set_device_pid(slot, 0);
   input_config_plays_as_release(slot);
}

/* The Plays As entries a configuration's @text gives. */
static void t_entries(const char *text)
{
   char buf[1024];
   config_file_t *conf;
   strlcpy(buf, text, sizeof(buf));
   conf = config_file_new_from_string(buf, NULL);
   input_plays_as_read(conf);
   config_file_free(conf);
}

/* Whether @conf has @key, and as @value where that is not NULL. */
static bool t_conf_is(config_file_t *conf, const char *key,
      const char *value)
{
   char got[256];
   if (!config_get_array(conf, key, got, sizeof(got)))
      return false;
   return !value || !strcmp(got, value);
}

/* A reservation's vid:pid, as the reservations have always read it:
 * hex, a colon, hex, then a space or the end; anything else is a
 * name. */
static void test_vid_pid_parse(void)
{
   unsigned long v = 7;
   unsigned long p = 7;
   const char *n   = NULL;
   CHECK(input_config_parse_vid_pid("045e:028e Pad", &v, &p, &n)
         && v == 0x045e && p == 0x028e && n && !strcmp(n, "Pad"),
         "vid:pid name was not read as one");
   CHECK(input_config_parse_vid_pid("0000:0000", &v, &p, &n)
         && v == 0 && p == 0 && n && !*n,
         "a bare 0000:0000 was not read as one");
   v = 7;
   CHECK(!input_config_parse_vid_pid("Pad", &v, &p, NULL)
         && !input_config_parse_vid_pid("045e:Pad", &v, &p, NULL)
         && !input_config_parse_vid_pid("045e:028eX", &v, &p, NULL)
         && !input_config_parse_vid_pid("Pad 2: Turbo", &v, &p, NULL)
         && v == 7,
         "a name was read as a vid:pid");
}

/* An entry names its controller by vid:pid, or by name where it has
 * 0000:0000 or none; an entry is one controller's. */
static void test_plays_as_match(void)
{
   t_reset();
   t_dev(0, "Other Name", 0x045e, 0x028e);
   t_dev(1, "Headset", 0, 0);
   t_dev(2, "Other", 0, 0);
   t_dev(3, "Stick", 0x1234, 0x5678);
   t_entries(
         "input_plays_as_device_1 = \"045e:028e Pad\"\n"
         "input_plays_as_player_1 = \"2\"\n"
         "input_plays_as_device_2 = \"0000:0000 Headset\"\n"
         "input_plays_as_player_2 = \"1\"\n"
         "input_plays_as_device_3 = \"Stick\"\n"
         "input_plays_as_player_3 = \"3\"\n");
   CHECK(input_config_plays_as_entry(0) == 0,
         "vid:pid does not name its controller");
   CHECK(input_config_plays_as_entry(1) == 1,
         "0000:0000 does not name its controller by name");
   CHECK(input_config_plays_as_entry(2) == -1,
         "0000:0000 names every controller with no vid:pid");
   CHECK(input_config_plays_as_entry(3) == 2
         && input_config_plays_as_player(3) == 3,
         "a name alone does not name its controller");

   /* two of one model, one entry: the second plays Auto */
   t_reset();
   t_dev(0, "Pad", 0x045e, 0x028e);
   t_dev(1, "Pad", 0x045e, 0x028e);
   t_entries(
         "input_plays_as_device_1 = \"045e:028e Pad\"\n"
         "input_plays_as_player_1 = \"2\"\n");
   CHECK(input_config_plays_as_entry(0) == 0
         && input_config_plays_as_entry(1) == -1,
         "one entry was taken by two controllers");
}

/* An entry's name is what follows its vid:pid, however that is
 * written; a name alone is all name. */
static void test_plays_as_entry_name(void)
{
   t_reset();
   t_entries(
         "input_plays_as_device_1 = \"45e:28e Pad\"\n"
         "input_plays_as_device_2 = \"Pads:Mini Pro\"\n"
         "input_plays_as_device_3 = \"0000:0000\"\n");
   CHECK(input_config_plays_as_entry_name(0)
         && !strcmp(input_config_plays_as_entry_name(0), "Pad"),
         "a short vid:pid is shown with the name");
   CHECK(input_config_plays_as_entry_name(1)
         && !strcmp(input_config_plays_as_entry_name(1), "Pads:Mini Pro"),
         "a name alone was cut");
   CHECK(input_config_plays_as_entry_name(2)
         && !strcmp(input_config_plays_as_entry_name(2), "0000:0000"),
         "a vid:pid with no name is not shown");
}

/* The entries are written and read back; a free entry's keys go. */
static void test_plays_as_round_trip(void)
{
   config_file_t *conf;
   t_reset();
   t_dev(0, "Pad", 0x045e, 0x028e);
   CHECK(input_config_plays_as_set(0, 2), "Player 2 took no entry");
   conf = config_file_new_alloc();
   config_set_string(conf, "input_plays_as_device_5", "045e:0000 Gone");
   config_set_string(conf, "input_plays_as_player_5", "3");
   input_plays_as_write(conf);
   CHECK(t_conf_is(conf, "input_plays_as_device_1", "045e:028e Pad")
         && t_conf_is(conf, "input_plays_as_player_1", "2"),
         "the entry was not written as vid:pid name and player");
   CHECK(!t_conf_is(conf, "input_plays_as_device_5", NULL)
         && !t_conf_is(conf, "input_plays_as_player_5", NULL),
         "a free entry's keys were kept");
   CHECK(!t_conf_is(conf, "input_plays_as_device_2", NULL),
         "a free entry was written");

   input_plays_as_read(NULL);
   CHECK(input_config_plays_as_player(0) == 0, "the entries outlived a read");
   input_plays_as_read(conf);
   CHECK(input_config_plays_as_entry(0) == 0
         && input_config_plays_as_player(0) == 2,
         "the entry did not come back");
   config_file_free(conf);
}

/* An entry is made for a player and let go on Auto; with all sixteen
 * held by controllers that are gone, another is not made. */
static void test_plays_as_entry_life(void)
{
   unsigned e;
   char name[32];
   t_reset();
   t_dev(0, "Pad", 1, 1);
   CHECK(input_config_plays_as_set(0, 0)
         && input_config_plays_as_entry(0) < 0,
         "Auto took an entry");
   CHECK(input_config_plays_as_set(0, 2)
         && input_config_plays_as_entry(0) == 0
         && input_config_plays_as_active(),
         "Player 2 took no entry");
   CHECK(input_config_plays_as_set(0, 0)
         && input_config_plays_as_entry(0) < 0
         && !input_config_plays_as_active(),
         "an entry set back to Auto was kept");

   for (e = 0; e < MAX_USERS; e++)
   {
      snprintf(name, sizeof(name), "Gone %u", e);
      t_dev(1, name, (uint16_t)(e + 2), 1);
      input_config_plays_as_set(1, 1);
      t_disconnect(1);
   }
   CHECK(!input_config_plays_as_set(0, 3)
         && input_config_plays_as_entry(0) < 0,
         "a seventeenth entry was made");
   CHECK(input_config_plays_as_set(0, 0), "Auto needed an entry");
   CHECK(input_config_plays_as_entry_name(15)
         && !strcmp(input_config_plays_as_entry_name(15), "Gone 15")
         && !input_config_plays_as_entry_taken(15),
         "an entry of a controller that is gone is not listed by name");
   input_config_plays_as_set_entry(15, 0);
   CHECK(!input_config_plays_as_entry_name(15),
         "a gone controller's entry set to Auto was kept");
}

/* A controller in slot @slot, connected as the frontend connects one:
 * named, then placed. */
static void t_connect(unsigned slot, const char *name, uint16_t vid,
      uint16_t pid)
{
   t_dev(slot, name, vid, pid);
   input_config_plays_as_place(slot);
}

/* The user reading @slot below Max Users, or MAX_USERS. */
static unsigned t_user_of(unsigned slot)
{
   unsigned u;
   for (u = 0; u < settings.uints.input_max_users; u++)
      if (settings.uints.input_joypad_index[u] == slot)
         return u;
   return MAX_USERS;
}

/* Auto: with no entries the reservations place it; with entries, the
 * first free port. */
static void test_place_auto(void)
{
   t_reset();
   t_dev(2, "Pad", 1, 1);
   CHECK(!input_config_plays_as_place(2) && t_user_of(2) == 2,
         "with no entries Plays As placed an Auto controller");
   t_entries(
         "input_plays_as_device_1 = \"Away\"\n"
         "input_plays_as_player_1 = \"4\"\n");
   CHECK(input_config_plays_as_place(2) && t_user_of(2) == 0,
         "an Auto controller is not on the first free port");
}

/* Player N: a free port N is its; an Auto controller there moves to
 * the first free port; another of player N's makes it share, from the
 * highest free port below Max Users. */
static void test_place_player(void)
{
   t_reset();
   t_entries(
         "input_plays_as_device_1 = \"0001:0001 A\"\n"
         "input_plays_as_player_1 = \"2\"\n");
   t_connect(0, "A", 1, 1);
   CHECK(t_user_of(0) == 1, "Player 2 did not take a free port 2");

   t_reset();
   t_entries(
         "input_plays_as_device_1 = \"0002:0002 B\"\n"
         "input_plays_as_player_1 = \"1\"\n");
   t_connect(0, "Auto", 9, 9);
   CHECK(t_user_of(0) == 0, "the Auto controller is not on port 1");
   t_connect(1, "B", 2, 2);
   CHECK(t_user_of(1) == 0 && t_user_of(0) == 1,
         "Player 1 did not take port 1 from an Auto controller");

   t_reset();
   t_entries(
         "input_plays_as_device_1 = \"0001:0001 A\"\n"
         "input_plays_as_player_1 = \"1\"\n"
         "input_plays_as_device_2 = \"0002:0002 B\"\n"
         "input_plays_as_player_2 = \"1\"\n");
   t_connect(0, "A", 1, 1);
   t_connect(1, "B", 2, 2);
   CHECK(t_user_of(0) == 0 && t_user_of(1) == 3,
         "a second Player 1 does not share from the highest free port");
   CHECK(settings.uints.input_joypad_index[1] == 3,
         "the port the sharer left does not read the slot it came from");
}

/* A controller a reservation put on port N is player N's; an explicit
 * player above Max Users plays Auto; with no free port a sharer gets
 * none. */
static void test_place_limits(void)
{
   t_reset();
   settings.uints.input_device_reservation_type[0] =
      INPUT_DEVICE_RESERVATION_PREFERRED;
   strlcpy(settings.arrays.input_reserved_devices[0], "0005:0005 Res",
         sizeof(settings.arrays.input_reserved_devices[0]));
   t_entries(
         "input_plays_as_device_1 = \"0002:0002 B\"\n"
         "input_plays_as_player_1 = \"1\"\n");
   t_dev(0, "Res", 5, 5);
   CHECK(!input_config_plays_as_place(0),
         "an Auto controller was placed with a reservation set");
   t_connect(1, "B", 2, 2);
   CHECK(t_user_of(0) == 0 && t_user_of(1) == 3,
         "Player 1 did not share with the reserved controller on port 1");

   t_reset();
   t_entries(
         "input_plays_as_device_1 = \"0002:0002 B\"\n"
         "input_plays_as_player_1 = \"5\"\n");
   t_connect(0, "A", 1, 1);
   t_connect(1, "B", 2, 2);
   CHECK(t_user_of(0) == 0 && t_user_of(1) == 1,
         "Player 5 with Max Users 4 did not play Auto");

   t_reset();
   settings.uints.input_max_users = 2;
   t_entries(
         "input_plays_as_device_1 = \"0001:0001 A\"\n"
         "input_plays_as_player_1 = \"1\"\n"
         "input_plays_as_device_2 = \"0002:0002 B\"\n"
         "input_plays_as_player_2 = \"1\"\n");
   t_connect(0, "A", 1, 1);
   t_connect(1, "C", 3, 3);
   t_connect(2, "B", 2, 2);
   CHECK(t_user_of(0) == 0 && t_user_of(1) == 1 && t_user_of(2) == MAX_USERS,
         "a sharer with no free port took one");
}

/* An explicit player beats a reservation naming the controller; a
 * reservation names as it places, a 0000:0000 one every controller
 * with no vid:pid. */
static void test_place_reservation(void)
{
   t_reset();
   settings.uints.input_device_reservation_type[0] =
      INPUT_DEVICE_RESERVATION_PREFERRED;
   strlcpy(settings.arrays.input_reserved_devices[0], "0005:0005 Res",
         sizeof(settings.arrays.input_reserved_devices[0]));
   t_dev(0, "Res", 5, 5);
   t_entries(
         "input_plays_as_device_1 = \"0005:0005 Res\"\n"
         "input_plays_as_player_1 = \"3\"\n"
         "input_plays_as_device_2 = \"0002:0002 B\"\n"
         "input_plays_as_player_2 = \"1\"\n");
   t_connect(1, "B", 2, 2);
   CHECK(t_user_of(1) == 0 && t_user_of(0) == 1,
         "a Player 3 controller that port 1's reservation names kept "
         "port 1 from Player 1");

   t_reset();
   settings.uints.input_device_reservation_type[0] =
      INPUT_DEVICE_RESERVATION_PREFERRED;
   strlcpy(settings.arrays.input_reserved_devices[0], "0000:0000 Res",
         sizeof(settings.arrays.input_reserved_devices[0]));
   t_entries(
         "input_plays_as_device_1 = \"0002:0002 B\"\n"
         "input_plays_as_player_1 = \"1\"\n");
   t_dev(0, "Other", 0, 0);
   t_connect(1, "B", 2, 2);
   CHECK(t_user_of(0) == 0 && t_user_of(1) == 3,
         "a 0000:0000 reservation did not name an unnumbered controller "
         "it places");
}

/* Port 1 reserved for one controller, another set to Player 1: either
 * order, the first has port 1 and the second shares. */
static void test_place_reservation_order(void)
{
   t_reset();
   settings.uints.input_device_reservation_type[0] =
      INPUT_DEVICE_RESERVATION_PREFERRED;
   strlcpy(settings.arrays.input_reserved_devices[0], "0005:0005 Res",
         sizeof(settings.arrays.input_reserved_devices[0]));
   t_entries(
         "input_plays_as_device_1 = \"0002:0002 B\"\n"
         "input_plays_as_player_1 = \"1\"\n");
   t_dev(0, "Res", 5, 5);
   CHECK(!input_config_plays_as_place(0), "the reservation did not place");
   t_connect(1, "B", 2, 2);
   CHECK(t_user_of(0) == 0 && t_user_of(1) == 3,
         "reserved first: Player 1 did not share");

   t_reset();
   settings.uints.input_device_reservation_type[0] =
      INPUT_DEVICE_RESERVATION_PREFERRED;
   strlcpy(settings.arrays.input_reserved_devices[0], "0005:0005 Res",
         sizeof(settings.arrays.input_reserved_devices[0]));
   t_entries(
         "input_plays_as_device_1 = \"0002:0002 B\"\n"
         "input_plays_as_player_1 = \"1\"\n");
   t_connect(0, "B", 2, 2);
   t_dev(1, "Res", 5, 5);
   CHECK(input_config_plays_as_place(1)
         && t_user_of(0) == 0 && t_user_of(1) == 3,
         "Player 1 first: the reserved controller did not share");

   /* twins reserved for ports 1 and 2, one set to Player 1: the other
    * goes to port 2 as the reservations give it */
   t_reset();
   settings.uints.input_device_reservation_type[0] =
      INPUT_DEVICE_RESERVATION_PREFERRED;
   settings.uints.input_device_reservation_type[1] =
      INPUT_DEVICE_RESERVATION_PREFERRED;
   strlcpy(settings.arrays.input_reserved_devices[0], "0005:0005 Res",
         sizeof(settings.arrays.input_reserved_devices[0]));
   strlcpy(settings.arrays.input_reserved_devices[1], "0005:0005 Res",
         sizeof(settings.arrays.input_reserved_devices[1]));
   t_entries(
         "input_plays_as_device_1 = \"0005:0005 Res\"\n"
         "input_plays_as_player_1 = \"1\"\n");
   t_connect(0, "Res", 5, 5);
   t_dev(1, "Res", 5, 5);
   CHECK(!input_config_plays_as_place(1)
         && t_user_of(0) == 0 && t_user_of(1) == 1,
         "a twin reserved for port 2 shared Player 1");
}

/* The controller on port N leaving: its sharers stay; the next Auto
 * controller takes port N. A player set and the controllers connected
 * again place them by it. */
static void test_place_leave_and_change(void)
{
   t_reset();
   t_entries(
         "input_plays_as_device_1 = \"0001:0001 A\"\n"
         "input_plays_as_player_1 = \"1\"\n"
         "input_plays_as_device_2 = \"0002:0002 B\"\n"
         "input_plays_as_player_2 = \"1\"\n");
   t_connect(0, "A", 1, 1);
   t_connect(1, "B", 2, 2);
   t_disconnect(0);
   CHECK(t_user_of(1) == 3, "the sharer moved when port 1 emptied");
   t_connect(0, "Auto", 9, 9);
   CHECK(t_user_of(0) == 0, "an Auto controller did not take the empty port 1");

   t_reset();
   t_connect(0, "A", 1, 1);
   t_connect(1, "B", 2, 2);
   CHECK(input_config_plays_as_set(1, 1), "Player 1 took no entry");
   input_config_plays_as_place(0);
   input_config_plays_as_place(1);
   CHECK(t_user_of(1) == 0 && t_user_of(0) == 1,
         "connected again, the controller set to Player 1 is not on port 1");
}

/* The controllers placed again as input_autoconfigure_reallocate_ports()
 * places them: the one in @slot, or with @slot MAX_USERS those a change
 * of Max Users moves, lowest slot first, as on connect. */
static void t_place_again(unsigned slot)
{
   unsigned s;
   uint8_t again[MAX_USERS];

   if (slot < MAX_USERS)
   {
      memset(again, 0, sizeof(again));
      again[slot] = 1;
   }
   else
      input_config_plays_as_max_users_changed(again);
   for (s = 0; s < MAX_USERS; s++)
      if (again[s] && input_config_get_device_name(s))
         input_config_plays_as_place(s);
   input_config_binds_changed();
}

/* Placed again, a steady arrangement stays; a Player change and a
 * lower Max Users move what they change. */
static void test_place_again(void)
{
   t_reset();
   t_entries(
         "input_plays_as_device_1 = \"0001:0001 A\"\n"
         "input_plays_as_player_1 = \"1\"\n"
         "input_plays_as_device_2 = \"0002:0002 B\"\n"
         "input_plays_as_player_2 = \"1\"\n");
   t_connect(0, "A", 1, 1);
   t_connect(1, "B", 2, 2);
   t_place_again(0);
   t_place_again(1);
   CHECK(t_user_of(0) == 0 && t_user_of(1) == 3,
         "placed again, a steady arrangement moved");

   CHECK(input_config_plays_as_set(1, 2), "Player 2 was refused");
   t_place_again(1);
   CHECK(t_user_of(0) == 0 && t_user_of(1) == 1,
         "placed again, Player 2 is not on port 2");

   CHECK(input_config_plays_as_set(1, 1), "Player 1 was refused");
   t_place_again(1);
   CHECK(t_user_of(1) == 3,
         "placed again, Player 1 does not share from port 4");
   settings.uints.input_max_users = 2;
   t_place_again(MAX_USERS);
   CHECK(t_user_of(0) == 0 && t_user_of(1) == 1,
         "with Max Users 2 the sharer was not placed below it");
}

/* A Player change places that controller again, and no other: an Auto
 * controller above a port emptied earlier stays where it is. */
static void test_place_again_player(void)
{
   t_reset();
   t_connect(0, "A", 1, 1);
   t_connect(1, "X", 3, 3);
   t_connect(2, "B", 2, 2);
   t_disconnect(1);
   CHECK(input_config_plays_as_set(0, 1), "Player 1 was refused");
   t_place_again(0);
   CHECK(t_user_of(0) == 0 && t_user_of(2) == 2,
         "A's Player changed, B left port 3");
}

/* A Max Users change places again only the controllers left with no
 * port, and those whose player it brings within Max Users. */
static void test_place_again_max_users(void)
{
   t_reset();
   t_entries(
         "input_plays_as_device_1 = \"0001:0001 A\"\n"
         "input_plays_as_player_1 = \"1\"\n");
   t_connect(0, "A", 1, 1);
   t_connect(1, "X", 9, 9);
   t_connect(2, "C", 3, 3);
   t_connect(3, "D", 4, 4);
   t_disconnect(1);
   settings.uints.input_max_users = 3;
   t_place_again(MAX_USERS);
   CHECK(t_user_of(0) == 0 && t_user_of(2) == 2 && t_user_of(3) == 1,
         "Max Users 3 moved a controller below it");

   t_reset();
   settings.uints.input_max_users = 2;
   t_entries(
         "input_plays_as_device_1 = \"0003:0003 C\"\n"
         "input_plays_as_player_1 = \"3\"\n");
   t_connect(0, "A", 1, 1);
   t_connect(1, "C", 3, 3);
   settings.uints.input_max_users = 4;
   t_place_again(MAX_USERS);
   CHECK(t_user_of(0) == 0 && t_user_of(1) == 2,
         "Max Users 4 did not put Player 3 on port 3");
}

/* A Player or Max Users change that leaves no player in effect places
 * the controller it changed as Auto does, on the first free port: set
 * back to Auto, or its player past a lower Max Users. A connect with
 * none in effect still leaves it be (test_place_no_player). */
static void test_place_again_last_player(void)
{
   t_reset();
   t_connect(0, "Pad", 1, 1);
   CHECK(input_config_plays_as_set(0, 3), "Player 3 was refused");
   t_place_again(0);
   CHECK(t_user_of(0) == 2, "Player 3 is not on port 3");
   CHECK(input_config_plays_as_set(0, 0), "Auto was refused");
   t_place_again(0);
   CHECK(t_user_of(0) == 0,
         "the last player set back to Auto stayed on port 3");

   t_reset();
   t_connect(0, "Pad", 1, 1);
   CHECK(input_config_plays_as_set(0, 3), "Player 3 was refused");
   t_place_again(0);
   settings.uints.input_max_users = 2;
   t_place_again(MAX_USERS);
   CHECK(t_user_of(0) == 0,
         "the last player past Max Users 2 got no port");
}

/* A player above Max Users was never in effect: the controller set
 * back to Auto from it, or left above a lower Max Users, stays where
 * it is with port 2 free. */
static void test_place_again_never_in_effect(void)
{
   t_reset();
   t_connect(0, "A", 1, 1);
   t_connect(2, "C", 3, 3);
   CHECK(input_config_plays_as_set(2, 6), "Player 6 was refused");
   t_place_again(2);
   CHECK(t_user_of(2) == 2, "fixture: C on 6 left port 3");
   CHECK(input_config_plays_as_set(2, 0), "Auto was refused");
   t_place_again(2);
   CHECK(t_user_of(2) == 2,
         "C back on Auto from 6, above Max Users, left port 3");

   t_reset();
   t_connect(0, "A", 1, 1);
   t_connect(2, "C", 3, 3);
   CHECK(input_config_plays_as_set(2, 6), "Player 6 was refused");
   t_place_again(2);
   settings.uints.input_max_users = 2;
   t_place_again(MAX_USERS);
   CHECK(t_user_of(2) == MAX_USERS,
         "C on 6, above Max Users 4 and then 2, took port 2");
}

/* The frame's end, as runloop.c's: what the poll left for it is done
 * with the core off the stack. */
static void t_frame_end(void)
{
   if (input_state_get_ptr()->first_press_pending)
      input_first_press_apply();
}

/* A and B both Player 1: A in slot 0 on port 1, B in slot 1 sharing
 * from port 4. */
static void t_two_player_1(void)
{
   t_entries(
         "input_plays_as_device_1 = \"0001:0001 A\"\n"
         "input_plays_as_player_1 = \"1\"\n"
         "input_plays_as_device_2 = \"0002:0002 B\"\n"
         "input_plays_as_player_2 = \"1\"\n");
   t_connect(0, "A", 1, 1);
   t_connect(1, "B", 2, 2);
}

/* Nothing joining and nothing changing: the frames' ends tell the core
 * nothing. */
static void test_announce_nothing(void)
{
   t_reset();
   input_driver_poll();
   t_frame_end();
   input_driver_poll();
   t_frame_end();
   CHECK(t_inits == 0, "the core was told its controllers with nothing changed");
}

/* A sharer feeds player 1's core port with player 1's controller and
 * alone is player 1; its remap port is untouched, so a remap save,
 * which writes the remap ports, cannot store the join. */
static void test_join_port_map(void)
{
   t_reset();
   t_two_player_1();
   input_driver_poll();
   t_buttons[0] = T_A;
   t_buttons[1] = T_B;
   CHECK(t_read(0) == (T_A | T_B),
         "core port 1 does not read both of player 1's controllers");
   CHECK(t_read(3) == 0, "core port 4 still reads the sharer");
   CHECK(settings.uints.input_remap_ports[3] == 3,
         "the join was written to the remap ports");
   CHECK(input_remapping_user_port(3) == 0
         && input_remapping_join_user(3) == 0
         && input_remapping_join_user(0) == MAX_USERS,
         "the sharer's core port is not player 1's");
   t_buttons[0] = 0;
   CHECK(t_read(0) == T_B, "the sharer alone is not player 1");
}

/* A sharer follows player 1's Mapped Port: user 1 moved to core port
 * 2 by a remap takes the sharer with it. */
static void test_join_follows_mapped_port(void)
{
   t_reset();
   t_two_player_1();
   t_map(0, 1);
   t_buttons[1] = T_B;
   CHECK(t_read(1) == T_B && t_read(0) == 0,
         "the sharer does not follow user 1 to core port 2");
   CHECK(input_remapping_user_port(3) == 1,
         "the sharer's core port is not user 1's Mapped Port");
}

/* The poll joins and lets go: an entry set back to Auto ends the join;
 * the occupant leaving does not. */
static void test_join_poll(void)
{
   t_reset();
   t_two_player_1();
   t_buttons[1] = T_B;
   input_driver_poll();
   CHECK(t_read(0) == T_B, "the poll did not join the sharer");
   t_disconnect(0);
   input_driver_poll();
   CHECK(t_read(0) == T_B, "the sharer stopped feeding port 1 when A left");
   input_config_plays_as_set(1, 0);
   input_driver_poll();
   CHECK(t_read(0) == 0 && t_read(3) == T_B,
         "the join outlived the sharer's entry");
}

/* Port 1's reservation names Res; B is Player 1. */
static void t_reserve_1(void)
{
   settings.uints.input_device_reservation_type[0] =
      INPUT_DEVICE_RESERVATION_PREFERRED;
   strlcpy(settings.arrays.input_reserved_devices[0], "0005:0005 Res",
         sizeof(settings.arrays.input_reserved_devices[0]));
   t_entries(
         "input_plays_as_device_1 = \"0002:0002 B\"\n"
         "input_plays_as_player_1 = \"1\"\n");
}

/* A controller port 1's reservation names shares with a Player 1
 * controller there, in either connect order: both feed core port 1. */
static void test_join_reserved(void)
{
   unsigned order;
   for (order = 0; order < 2; order++)
   {
      t_reset();
      t_reserve_1();
      if (order)
      {
         t_connect(0, "B", 2, 2);
         t_connect(1, "Res", 5, 5);
      }
      else
      {
         t_connect(0, "Res", 5, 5);
         t_connect(1, "B", 2, 2);
      }
      input_driver_poll();
      t_buttons[0] = T_A;
      t_buttons[1] = T_B;
      CHECK(t_read(0) == (T_A | T_B) && t_read(3) == 0,
            order ? "Player 1 first: the reserved controller does not "
                    "feed core port 1"
                  : "reserved first: Player 1 does not feed core port 1");
   }
}

/* The Player 1 controller leaving: the reserved controller sharing
 * with it stays and feeds core port 1, and the next Auto controller
 * takes port 1 beside it. */
static void test_join_reserved_stays(void)
{
   t_reset();
   t_reserve_1();
   t_connect(0, "B", 2, 2);
   t_connect(1, "Res", 5, 5);
   input_driver_poll();
   t_disconnect(0);
   input_driver_poll();
   t_buttons[1] = T_B;
   CHECK(t_read(0) == T_B && t_read(3) == 0
         && input_remapping_join_user(3) == 0,
         "the reserved controller stopped sharing when Player 1 left");

   t_connect(0, "Auto", 9, 9);
   input_driver_poll();
   t_buttons[0] = T_A;
   CHECK(t_user_of(0) == 0 && t_read(0) == (T_A | T_B),
         "the next Auto controller is not on port 1 beside the reserved "
         "controller");
}

/* Player 1's rumble reaches the sharer; core port 4's does not. */
static void test_rumble_join(void)
{
   t_reset();
   t_two_player_1();
   input_driver_poll();
   CHECK(t_rumble_port(0, 1100) == ((1u << 0) | (1u << 1)),
         "player 1's rumble did not reach both of its controllers");
   CHECK(t_rumble_port(3, 1101) == 0,
         "core port 4's rumble reached the sharer");
}

/* First press: a press on any of player 1's controllers gives user 1
 * a port, and the sharer feeds it; the sharer's own port stays None. */
static void test_first_press_group(void)
{
   unsigned u;
   input_driver_state_t *st = input_state_get_ptr();

   t_reset();
   t_two_player_1();
   for (u = 0; u < MAX_USERS; u++)
      settings.uints.input_remap_ports[u] = MAX_USERS;
   st->first_press_on = true;
   input_remapping_update_port_map();

   /* seen released, then pressed on the sharer */
   input_driver_poll();
   t_buttons[1] = T_A;
   input_driver_poll();
   input_first_press_apply();

   CHECK(settings.uints.input_remap_ports[0] == 0,
         "a press on the sharer did not give user 1 core port 1");
   CHECK(settings.uints.input_remap_ports[3] == MAX_USERS,
         "the sharer was given a port of its own");
   CHECK(t_read(0) == T_A, "the sharer does not feed the port it claimed");
   st->first_press_on = false;
}

/* A sharer's remaps go by its core port's device, whatever its own
 * Mapped Port says: A remapped to B, core port 4 with no device, under
 * First Press (user 1 pressed, the rest None) or the identity map.
 * Returns core port 1's buttons with A held on the sharer. */
static unsigned t_remap_join(bool first_press)
{
   unsigned u, i, got;
   input_driver_state_t *st = input_state_get_ptr();

   t_reset();
   if (first_press)
      for (u = 0; u < MAX_USERS; u++)
         settings.uints.input_remap_ports[u] = MAX_USERS;
   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
      input_config_binds[3][i].attr =
         RETRO_KEYBIND_ATTR(RETROK_UNKNOWN, NO_BTN, true);
   settings.uints.input_libretro_device[3] = RETRO_DEVICE_NONE;
   settings.uints.input_remap_ids[3][RETRO_DEVICE_ID_JOYPAD_A] =
      RETRO_DEVICE_ID_JOYPAD_B;
   settings.bools.input_remap_binds_enable = true;
   st->first_press_on = first_press;
   t_two_player_1();
   if (first_press)
      settings.uints.input_remap_ports[0] = 0;
   input_remapping_update_port_map();

   t_buttons[1] = T_A;
   input_driver_poll();
   got = t_read(0);

   settings.bools.input_remap_binds_enable = false;
   st->first_press_on = false;
   return got;
}

static void test_remap_join(void)
{
   CHECK(t_remap_join(true) == T_B,
         "First Press: the sharer's remap is not core port 1's");
   CHECK(t_remap_join(false) == T_B,
         "the identity map: the sharer's remap is not core port 1's");
}

/* First Press: a controller beginning to share tells the core its
 * controllers at the frame's end; going on, nothing more. */
static void test_announce_first_press(void)
{
   unsigned u;
   input_driver_state_t *st = input_state_get_ptr();

   t_reset();
   for (u = 0; u < MAX_USERS; u++)
      settings.uints.input_remap_ports[u] = MAX_USERS;
   /* A was pressed */
   settings.uints.input_remap_ports[0] = 0;
   st->first_press_on = true;
   t_entries(
         "input_plays_as_device_1 = \"0001:0001 A\"\n"
         "input_plays_as_player_1 = \"1\"\n"
         "input_plays_as_device_2 = \"0002:0002 B\"\n"
         "input_plays_as_player_2 = \"1\"\n");
   t_connect(0, "A", 1, 1);
   input_remapping_update_port_map();
   command_event(CMD_EVENT_CONTROLLER_INIT, NULL);
   t_inits = 0;

   t_connect(1, "B", 2, 2);
   input_driver_poll();
   t_frame_end();
   CHECK(t_inits == 1 && t_device[0] == RETRO_DEVICE_JOYPAD,
         "a controller beginning to share did not tell the core");

   input_driver_poll();
   t_frame_end();
   CHECK(t_inits == 1, "sharing going on told the core again");
   st->first_press_on = false;
}

/* The join ending gives the sharer's own core port its device back at
 * the frame's end. */
static void test_announce_join_ends(void)
{
   t_reset();
   t_two_player_1();
   input_driver_poll();
   t_frame_end();
   CHECK(t_device[3] == RETRO_DEVICE_NONE,
         "core port 4 was told a device with its user sharing");
   t_inits = 0;

   input_config_plays_as_set(1, 0);
   input_driver_poll();
   t_frame_end();
   CHECK(t_inits == 1 && t_device[3] == RETRO_DEVICE_JOYPAD,
         "the join ending did not give core port 4 its device back");
}

/* Netplay on: the join still feeds player 1's core port, but the core
 * is not told its controllers again, the ports being the session's;
 * nor when netplay starts between the poll and the frame's end. */
static void test_announce_join_netplay(void)
{
   t_reset();
   t_netplay = true;
   t_two_player_1();
   input_driver_poll();
   t_frame_end();
   t_buttons[1] = T_B;
   CHECK(t_read(0) == T_B, "netplay: the sharer does not feed port 1");
   CHECK(t_inits == 0, "netplay: the join told the core its controllers");

   t_netplay = false;
   input_config_plays_as_set(1, 0);
   input_driver_poll();
   t_frame_end();
   CHECK(t_read(0) == 0 && t_inits == 1,
         "the join ending after netplay did not tell the core");

   t_inits = 0;
   input_config_plays_as_set(1, 1);
   input_driver_poll();
   t_netplay = true;
   t_frame_end();
   CHECK(t_inits == 0,
         "netplay started at the frame's end: the join told the core");
   t_netplay = false;
}

/* The port a controller's row shows: the user reading its slot below
 * Max Users, a sharer's included; none past Max Users. */
static void test_slot_user(void)
{
   t_reset();
   t_two_player_1();
   CHECK(input_config_slot_user(0) == 0 && input_config_slot_user(1) == 3,
         "the rows' ports are not the users reading the slots");
   CHECK(input_config_slot_user(7) == MAX_USERS,
         "a slot past Max Users has a port");
}

/* What a Controllers row shows: where its controller plays, the player
 * it feeds (the one it shares, an Auto sharer's too) and the port its
 * binds are on; none past Max Users; a player above Max Users playing
 * Auto. A port's page is for the player its controller plays as, its
 * own when empty. */
static void test_plays_as_where(void)
{
   unsigned player, port;

   t_reset();
   t_two_player_1();
   t_connect(2, "C", 3, 3);
   CHECK(input_config_plays_as_where(0, &player, &port)
            == INPUT_PLAYS_AS_OWN_PORT && player == 1 && port == 1,
         "A on port 1 is not Player 1 there");
   CHECK(input_config_plays_as_where(1, &player, &port)
            == INPUT_PLAYS_AS_SHARING && player == 1 && port == 4,
         "B is not Player 1 shared, its binds on port 4");
   CHECK(input_config_plays_as_where(2, &player, &port)
            == INPUT_PLAYS_AS_OWN_PORT && player == 2 && port == 2,
         "C on Auto is not Player 2 on port 2");
   CHECK(input_config_plays_as_where(3, &player, &port)
            == INPUT_PLAYS_AS_GONE && !player && !port,
         "an empty slot is not gone");
   CHECK(input_config_user_player(0) == 0
         && input_config_user_player(3) == 0
         && input_config_user_player(1) == 1
         && input_config_user_player(2) == 2,
         "a port's page is not for the player its controller plays as");
   t_disconnect(0);
   CHECK(input_config_user_player(0) == 0
         && input_config_user_player(3) == 0,
         "port 1 emptied: its page or its sharer's is not Player 1's");

   /* an Auto controller sharing by a reservation */
   t_reset();
   t_reserve_1();
   t_connect(0, "B", 2, 2);
   t_connect(1, "Res", 5, 5);
   CHECK(input_config_plays_as_player(1) == 0
         && input_config_plays_as_where(1, &player, &port)
            == INPUT_PLAYS_AS_SHARING && player == 1 && port == 4,
         "an Auto sharer does not show Player 1");

   /* past Max Users: no port; a player above it plays Auto */
   t_reset();
   settings.uints.input_max_users = 2;
   t_entries(
         "input_plays_as_device_1 = \"0003:0003 C\"\n"
         "input_plays_as_player_1 = \"3\"\n");
   t_connect(0, "A", 1, 1);
   t_connect(1, "C", 3, 3);
   t_connect(2, "D", 4, 4);
   CHECK(input_config_plays_as_where(1, &player, &port)
            == INPUT_PLAYS_AS_ABOVE_MAX_USERS && player == 2 && port == 2,
         "Player 3 with Max Users 2 does not play Auto as Player 2");
   CHECK(input_config_plays_as_where(2, &player, &port)
            == INPUT_PLAYS_AS_NO_PORT,
         "a third controller with Max Users 2 has a port");
}

/* Entries that give no player within Max Users: Auto controllers are
 * placed as with no entries, a Device Index swap kept, and a driver
 * restart still puts each controller back on its port. */
static void test_place_no_player(void)
{
   t_reset();
   settings.uints.input_joypad_index[0] = 1;
   settings.uints.input_joypad_index[1] = 0;
   t_entries(
         "input_plays_as_device_1 = \"0009:0009 Away\"\n"
         "input_plays_as_player_1 = \"0\"\n"
         "input_plays_as_device_2 = \"0002:0002 B\"\n"
         "input_plays_as_player_2 = \"5\"\n");
   CHECK(input_config_plays_as_active(), "the entries are not in use");
   t_dev(0, "A", 1, 1);
   CHECK(!input_config_plays_as_place(0) && t_user_of(0) == 1,
         "an entry with no player undid a Device Index swap");
   t_dev(2, "C", 3, 3);
   CHECK(!input_config_plays_as_place(2) && t_user_of(2) == 2,
         "an entry with no player moved an Auto controller");
   t_connect(1, "B", 2, 2);
   CHECK(t_user_of(1) == 0 && t_user_of(0) == 1,
         "Player 5 with Max Users 4 was placed");

   t_reset();
   t_entries(
         "input_plays_as_device_1 = \"0001:0001 A\"\n"
         "input_plays_as_player_1 = \"0\"\n");
   input_driver_registry_connect(0, "x", "A", "", 1, 1);
   input_driver_registry_connect(1, "x", "B", "", 2, 2);
   input_driver_registry_restart();
   input_driver_registry_connect(0, "x", "B", "", 2, 2);
   input_driver_registry_connect(1, "x", "A", "", 1, 1);
   CHECK(settings.uints.input_joypad_index[0] == 1
         && settings.uints.input_joypad_index[1] == 0,
         "an entry with no player kept a restart from restoring the ports");
   input_driver_registry_disconnect(0);
   input_driver_registry_disconnect(1);
}

/* t_connect, with the registry told as the connect callback tells it. */
static void t_plug(unsigned slot, const char *name, uint16_t vid,
      uint16_t pid)
{
   t_connect(slot, name, vid, pid);
   input_driver_registry_connect(slot, "x", name, "", vid, pid);
}

/* A driver restart that reports the pads in other slots with no
 * disconnects between: the Player 1 pad keeps its entry and port 1. */
static void test_place_restart(void)
{
   t_reset();
   t_entries(
         "input_plays_as_device_1 = \"0002:0002 B\"\n"
         "input_plays_as_player_1 = \"1\"\n");
   t_plug(0, "A", 1, 1);
   t_plug(1, "B", 2, 2);
   input_driver_registry_restart();
   t_plug(0, "B", 2, 2);
   t_plug(1, "A", 1, 1);
   CHECK(t_user_of(0) == 0 && input_config_plays_as_player(0) == 1
         && t_user_of(1) == 1,
         "after a restart the Player 1 pad lost its entry or port 1");
   input_driver_registry_disconnect(0);
   input_driver_registry_disconnect(1);
}

/* One of two identical pads, plugged in at @phys, connects in @slot. */
static void t_plug_twin(unsigned slot, const char *phys)
{
   t_dev(slot, "Pad", 1, 1);
   input_config_set_device_phys(slot, phys);
   input_config_plays_as_place(slot);
   input_driver_registry_connect(slot, "x", "Pad", phys, 1, 1);
}

/* Two identical pads, the second set to Player 2: a config reload and
 * a driver restart leave it its entry and port 2, whichever slots the
 * pads come back in; the first never takes it by slot order. */
static void test_place_restart_twins(void)
{
   unsigned s;
   static const char *p2 =
         "input_plays_as_device_1 = \"0001:0001 Pad\"\n"
         "input_plays_as_player_1 = \"2\"\n";

   t_reset();
   t_plug_twin(0, "usb-1");
   t_plug_twin(1, "usb-2");
   CHECK(input_config_plays_as_set(1, 2), "Player 2 was refused");
   input_config_plays_as_place(1);
   t_entries(p2);
   CHECK(input_config_plays_as_entry(0) == -1
         && input_config_plays_as_entry(1) == 0,
         "a reload gave the Player 2 twin's entry to the first");

   input_driver_registry_restart();
   t_plug_twin(0, "usb-1");
   t_plug_twin(1, "usb-2");
   CHECK(input_config_plays_as_entry(0) == -1
         && input_config_plays_as_entry(1) == 0
         && t_user_of(1) == 1 && t_user_of(0) == 0,
         "after a restart the first twin took the Player 2 twin's entry");

   input_driver_registry_restart();
   t_plug_twin(0, "usb-2");
   t_plug_twin(1, "usb-1");
   CHECK(input_config_plays_as_entry(0) == 0
         && input_config_plays_as_entry(1) == -1
         && t_user_of(0) == 1 && t_user_of(1) == 0,
         "the Player 2 twin, back in the other slot, lost its entry");

   for (s = 0; s < 2; s++)
   {
      input_driver_registry_disconnect(s);
      input_config_set_device_phys(s, "");
   }

   /* a place longer than the registry keeps (a serial behind a hub) */
   t_reset();
   t_plug_twin(0, "usb-0000:00:14.0-1.4.1/input0"
         "0123456789abcdef0123456789abcdef0123456789");
   CHECK(input_config_plays_as_set(0, 2), "Player 2 was refused");
   input_config_plays_as_place(0);
   input_driver_registry_restart();
   t_plug_twin(0, "usb-0000:00:14.0-1.4.1/input0"
         "0123456789abcdef0123456789abcdef0123456789");
   CHECK(input_config_plays_as_entry(0) == 0 && t_user_of(0) == 1,
         "a pad with a long place lost its entry after a restart");
   input_driver_registry_disconnect(0);
   input_config_set_device_phys(0, "");
}

/* The reservations writing the ports after a Plays As move, as
 * reallocate_port_if_needed() hands them on: the config file still
 * gets the ports the user set. */
static void test_place_saved_ports(void)
{
   unsigned u;
   bool runtime;
   bool configured = true;
   t_reset();
   t_entries(
         "input_plays_as_device_1 = \"0002:0002 B\"\n"
         "input_plays_as_player_1 = \"2\"\n");
   t_connect(0, "B", 2, 2);
   runtime = input_driver_ports_runtime();
   settings.uints.input_joypad_index[2] = 3;
   settings.uints.input_joypad_index[3] = 2;
   if (runtime)
      input_driver_ports_runtime_moved();
   for (u = 0; u < MAX_USERS; u++)
      if (input_config_get_saved_joypad_index(u) != u)
         configured = false;
   CHECK(runtime && t_user_of(0) == 1 && configured,
         "a reservation's write saved the ports Plays As moved");
}

/* An input driver and a joypad driver that can be left running
 * across a video driver restart. */
static bool t_kv_survives(void *data) { (void)data; return true; }
static bool t_kv_pad_survives(void) { return true; }
static input_driver_t        t_kv;
static input_device_driver_t t_kv_joypad;

/* They start, under the settings and entries as they are. */
static void t_kv_start(void)
{
   input_driver_state_t *st = input_state_get_ptr();
   t_kv.ident                 = "kv";
   t_kv.survives_video        = t_kv_survives;
   t_kv_joypad                = t_joypad;
   t_kv_joypad.survives_video = t_kv_pad_survives;
   strlcpy(settings.arrays.input_driver, "kv",
         sizeof(settings.arrays.input_driver));
   strlcpy(settings.arrays.input_joypad_driver, "kvpad",
         sizeof(settings.arrays.input_joypad_driver));
   st->current_driver = &t_kv;
   st->current_data   = &t_kv;
   st->primary_joypad = NULL;
   input_driver_init_joypads();
   st->primary_joypad = &t_kv_joypad;
}

/* Whether the input driver is kept across a video driver restart, as
 * the drivers' teardown asks. */
static bool t_kv_kept(void)
{
   input_driver_state_t *st = input_state_get_ptr();
   bool kept;
   input_driver_keep_for_video_restart(true, NULL);
   kept            = st->kept_driver != NULL;
   st->kept_driver = NULL;
   st->kept_data   = NULL;
   return kept;
}

/* A config reload, as an override comes or goes, with the input
 * driver left running: the configured ports read again undo the
 * Plays As moves, and an override's entries change who goes where.
 * Either way the driver starts again, to place the controllers. */
static void test_place_reload_kept(void)
{
   unsigned u;
   static const char *p2 =
         "input_plays_as_device_1 = \"0001:0001 P\"\n"
         "input_plays_as_player_1 = \"2\"\n";
   input_driver_state_t *st = input_state_get_ptr();

   t_reset();
   t_entries(p2);
   t_kv_start();
   t_connect(0, "P", 1, 1);
   t_connect(1, "A", 9, 9);
   CHECK(t_user_of(0) == 1 && t_user_of(1) == 0 && t_kv_kept(),
         "placed by Plays As, the input driver was not kept");
   for (u = 0; u < MAX_USERS; u++)
      settings.uints.input_joypad_index[u] = u;
   t_entries(p2);
   CHECK(!t_kv_kept(),
         "a reload undid the Plays As moves and the driver was kept");
   t_kv_start();
   t_connect(0, "P", 1, 1);
   t_connect(1, "A", 9, 9);
   CHECK(t_user_of(0) == 1 && t_user_of(1) == 0 && t_kv_kept(),
         "started again, the controllers were not placed as before");

   /* moves undone by a reload that also took the entry away: once
    * started again with nothing to move, the driver is kept again */
   for (u = 0; u < MAX_USERS; u++)
      settings.uints.input_joypad_index[u] = u;
   input_plays_as_read(NULL);
   CHECK(!t_kv_kept(),
         "a reload took the entry away and the driver was kept");
   t_kv_start();
   t_connect(0, "P", 1, 1);
   t_connect(1, "A", 9, 9);
   CHECK(t_user_of(0) == 0 && t_kv_kept(),
         "started again with nothing moved, the driver was not kept");

   t_reset();
   t_kv_start();
   t_connect(0, "P", 1, 1);
   CHECK(t_kv_kept(), "with no entries the input driver was not kept");
   t_entries(p2);
   CHECK(!t_kv_kept(),
         "an override set a player and the driver was kept");
   st->current_driver = NULL;
   st->current_data   = NULL;
}

/* A load made only to compare against, as saving an override makes
 * one: putting back the copy taken before it restores the entries and
 * claims. */
static void test_plays_as_copy(void)
{
   input_plays_as_copy_t *copy;
   t_reset();
   t_dev(0, "Pad", 0x045e, 0x028e);
   t_dev(1, "Stick", 0x1234, 0x5678);
   CHECK(input_config_plays_as_set(1, 3), "Player 3 took no entry");
   copy = input_plays_as_copy_new();
   CHECK(copy, "no copy of the entries");
   if (!copy)
      return;
   t_entries(
         "input_plays_as_device_1 = \"045e:028e Pad\"\n"
         "input_plays_as_player_1 = \"2\"\n");
   CHECK(input_config_plays_as_entry(0) == 0
         && input_config_plays_as_entry(1) == -1,
         "the load did not read the file's entries");
   input_plays_as_copy_restore(copy);
   input_plays_as_copy_free(copy);
   CHECK(input_config_plays_as_entry(0) == -1
         && input_config_plays_as_entry(1) == 0
         && input_config_plays_as_player(1) == 3
         && input_config_plays_as_entry_name(0)
         && !strcmp(input_config_plays_as_entry_name(0), "Stick")
         && !input_config_plays_as_entry_name(1),
         "the entries read to compare against outlived the copy");
}

/* Two of a model are written lowest slot first, one on Auto before
 * another given a player as an Auto entry of its own: the next start
 * gives each its own back, and so does a reload once the session's are
 * laid out as the main config's. One gone keeps its place. */
static void test_plays_as_write_twins(void)
{
   unsigned s;
   config_file_t *conf;
   static const char main_b_pad[] =
         "input_plays_as_device_1 = \"0002:0002 B\"\n"
         "input_plays_as_player_1 = \"3\"\n"
         "input_plays_as_device_2 = \"0001:0001 Pad\"\n"
         "input_plays_as_player_2 = \"2\"\n";
   static const char main_two[] =
         "input_plays_as_device_1 = \"0001:0001 Pad\"\n"
         "input_plays_as_player_1 = \"2\"\n"
         "input_plays_as_device_2 = \"0001:0001 Pad\"\n"
         "input_plays_as_player_2 = \"3\"\n";
   static const char main_three[] =
         "input_plays_as_device_1 = \"0001:0001 Pad\"\n"
         "input_plays_as_player_1 = \"2\"\n"
         "input_plays_as_device_2 = \"0001:0001 Pad\"\n"
         "input_plays_as_player_2 = \"3\"\n"
         "input_plays_as_device_3 = \"0001:0001 Pad\"\n"
         "input_plays_as_player_3 = \"4\"\n";

   /* the second pad's entry made below the first's */
   t_reset();
   t_dev(0, "Pad", 1, 1);
   t_dev(1, "Pad", 1, 1);
   t_dev(2, "B", 2, 2);
   t_entries(main_b_pad);
   input_config_plays_as_set(2, 0);
   input_config_plays_as_set(1, 4);
   CHECK(input_config_plays_as_entry(1) == 0
         && input_config_plays_as_entry(0) == 1,
         "fixture: the second pad's entry did not take the one B let go");
   conf = config_file_new_alloc();
   input_plays_as_write(conf);
   CHECK(t_conf_is(conf, "input_plays_as_device_1", "0001:0001 Pad")
         && t_conf_is(conf, "input_plays_as_player_1", "2")
         && t_conf_is(conf, "input_plays_as_device_2", "0001:0001 Pad")
         && t_conf_is(conf, "input_plays_as_player_2", "4")
         && !t_conf_is(conf, "input_plays_as_device_3", NULL),
         "two of a model were not written lowest slot first");
   CHECK(input_config_plays_as_entry(1) == 0
         && input_config_plays_as_entry(0) == 1
         && input_config_plays_as_player(1) == 4,
         "writing the entries laid out the session's");
   input_plays_as_lay_out();
   CHECK(input_config_plays_as_player(0) == 2
         && input_config_plays_as_player(1) == 4
         && input_config_plays_as_player(2) == 0,
         "laying out the entries changed a controller's player");
   input_plays_as_read(conf);
   CHECK(input_config_plays_as_player(0) == 2
         && input_config_plays_as_player(1) == 4,
         "a reload of the entries written swapped two of a model");
   for (s = 0; s < 3; s++)
      input_config_plays_as_release(s);
   input_plays_as_read(conf);
   CHECK(input_config_plays_as_player(0) == 2
         && input_config_plays_as_player(1) == 4
         && input_config_plays_as_player(2) == 0,
         "the next start swapped two of a model");
   config_file_free(conf);

   /* saved elsewhere, or not saved: the main config read again gives
    * the session's back */
   t_reset();
   t_dev(0, "Pad", 1, 1);
   t_dev(1, "Pad", 1, 1);
   t_dev(2, "B", 2, 2);
   t_entries(main_b_pad);
   input_config_plays_as_set(2, 0);
   input_config_plays_as_set(1, 4);
   conf = config_file_new_alloc();
   input_plays_as_write(conf);
   config_file_free(conf);
   t_entries(main_b_pad);
   CHECK(input_config_plays_as_player(0) == 2
         && input_config_plays_as_player(1) == 0
         && input_config_plays_as_player(2) == 3,
         "the main config read again after a save elsewhere swapped two of a model");

   /* the middle one of three on Auto */
   t_reset();
   t_dev(0, "Pad", 1, 1);
   t_dev(1, "Pad", 1, 1);
   t_dev(2, "Pad", 1, 1);
   t_entries(main_three);
   input_config_plays_as_set(1, 0);
   conf = config_file_new_alloc();
   input_plays_as_write(conf);
   CHECK(t_conf_is(conf, "input_plays_as_player_1", "2")
         && t_conf_is(conf, "input_plays_as_device_2", "0001:0001 Pad")
         && t_conf_is(conf, "input_plays_as_player_2", "0")
         && t_conf_is(conf, "input_plays_as_player_3", "4"),
         "one on Auto before another of its model was not written as Auto");
   for (s = 0; s < 3; s++)
      input_config_plays_as_release(s);
   input_plays_as_read(conf);
   CHECK(input_config_plays_as_player(0) == 2
         && input_config_plays_as_player(1) == 0
         && input_config_plays_as_player(2) == 4,
         "the next start gave the third pad's player to the second");
   config_file_free(conf);

   /* the last of two on Auto, a third gone: Auto before the gone one's */
   t_reset();
   t_dev(0, "Pad", 1, 1);
   t_dev(1, "Pad", 1, 1);
   t_entries(main_three);
   input_config_plays_as_set(1, 0);
   conf = config_file_new_alloc();
   input_plays_as_write(conf);
   CHECK(t_conf_is(conf, "input_plays_as_player_1", "2")
         && t_conf_is(conf, "input_plays_as_player_2", "0")
         && t_conf_is(conf, "input_plays_as_player_3", "4"),
         "one on Auto was not written before a gone one's entry");
   for (s = 0; s < 2; s++)
      input_config_plays_as_release(s);
   input_plays_as_read(conf);
   CHECK(input_config_plays_as_player(0) == 2
         && input_config_plays_as_player(1) == 0,
         "the next start with the same pads gave the one on Auto the gone one's player");
   t_dev(2, "Pad", 1, 1);
   for (s = 0; s < 3; s++)
      input_config_plays_as_release(s);
   input_plays_as_read(conf);
   CHECK(input_config_plays_as_player(0) == 2
         && input_config_plays_as_player(1) == 0
         && input_config_plays_as_player(2) == 4,
         "the next start with the gone pad back last did not give it its own");
   config_file_free(conf);

   /* the last on Auto, none gone: nothing written for it */
   t_reset();
   t_dev(0, "Pad", 1, 1);
   t_dev(1, "Pad", 1, 1);
   t_entries(main_two);
   input_config_plays_as_set(1, 0);
   conf = config_file_new_alloc();
   input_plays_as_write(conf);
   CHECK(t_conf_is(conf, "input_plays_as_player_1", "2")
         && !t_conf_is(conf, "input_plays_as_device_2", NULL),
         "an Auto entry was written after the last of a model");
   config_file_free(conf);

   /* a gone pad ahead of one on Auto: if it stays gone, the pads after
    * it move up one, the one given a player starting on Auto */
   t_reset();
   t_dev(0, "Pad", 1, 1);
   t_dev(1, "Pad", 1, 1);
   t_dev(2, "Pad", 1, 1);
   t_entries(main_three);
   t_disconnect(0);
   input_config_plays_as_set(1, 0);
   conf = config_file_new_alloc();
   input_plays_as_write(conf);
   CHECK(t_conf_is(conf, "input_plays_as_player_1", "2")
         && t_conf_is(conf, "input_plays_as_player_2", "0")
         && t_conf_is(conf, "input_plays_as_player_3", "4"),
         "a gone pad's entry did not keep its place before one on Auto");
   for (s = 0; s < 3; s++)
      input_config_plays_as_release(s);
   input_plays_as_read(conf);
   CHECK(input_config_plays_as_player(1) == 2
         && input_config_plays_as_player(2) == 0,
         "the next start without the gone pad did not move the others up one");
   t_dev(0, "Pad", 1, 1);
   for (s = 0; s < 3; s++)
      input_config_plays_as_release(s);
   input_plays_as_read(conf);
   CHECK(input_config_plays_as_player(0) == 2
         && input_config_plays_as_player(1) == 0
         && input_config_plays_as_player(2) == 4,
         "the next start with the gone pad back did not give each its own");
   config_file_free(conf);

   /* connected in the other order, one of three gone */
   t_reset();
   t_entries(main_three);
   t_connect(2, "Pad", 1, 1);
   t_connect(0, "Pad", 1, 1);
   CHECK(input_config_plays_as_entry(2) == 0
         && input_config_plays_as_entry(0) == 1,
         "fixture: the pads did not take entries in the order they came");
   conf = config_file_new_alloc();
   input_plays_as_write(conf);
   CHECK(t_conf_is(conf, "input_plays_as_player_1", "3")
         && t_conf_is(conf, "input_plays_as_player_2", "2")
         && t_conf_is(conf, "input_plays_as_player_3", "4"),
         "two of a model were not written lowest slot first, or one gone moved");
   for (s = 0; s < 3; s++)
      input_config_plays_as_release(s);
   input_plays_as_read(conf);
   CHECK(input_config_plays_as_player(0) == 3
         && input_config_plays_as_player(2) == 2,
         "the next start swapped two of a model, one being gone");
   config_file_free(conf);

   /* laid out while the joypad driver restarts: each takes back its
    * own where it is plugged in */
   t_reset();
   t_entries(main_b_pad);
   t_plug_twin(0, "usb-1");
   t_plug_twin(1, "usb-2");
   t_connect(2, "B", 2, 2);
   input_config_plays_as_set(1, 3);
   input_config_plays_as_set(2, 0);
   input_driver_registry_restart();
   conf = config_file_new_alloc();
   input_plays_as_write(conf);
   config_file_free(conf);
   input_plays_as_lay_out();
   t_plug_twin(0, "usb-2");
   t_plug_twin(1, "usb-1");
   CHECK(input_config_plays_as_player(0) == 3
         && input_config_plays_as_player(1) == 2,
         "entries laid out during a restart lost the pads' own");
   for (s = 0; s < 2; s++)
   {
      input_driver_registry_disconnect(s);
      input_config_set_device_phys(s, "");
   }

   /* written while a restart has reported a later pad but not the
    * first: no Auto entry for the first */
   t_reset();
   t_entries(main_two);
   t_plug_twin(0, "usb-1");
   t_plug_twin(1, "usb-2");
   input_driver_registry_restart();
   t_plug_twin(1, "usb-2");
   conf = config_file_new_alloc();
   input_plays_as_write(conf);
   CHECK(t_conf_is(conf, "input_plays_as_player_1", "2")
         && t_conf_is(conf, "input_plays_as_player_2", "3")
         && !t_conf_is(conf, "input_plays_as_device_3", NULL),
         "a pad not reported yet after a restart was written as Auto");
   config_file_free(conf);
   t_plug_twin(0, "usb-1");
   CHECK(input_config_plays_as_player(0) == 2
         && input_config_plays_as_player(1) == 3,
         "the pads did not take back their own after the restart");
   for (s = 0; s < 2; s++)
   {
      input_driver_registry_disconnect(s);
      input_config_set_device_phys(s, "");
   }

   /* a pad two entries name, by name and by vid:pid: one Auto entry */
   t_reset();
   t_entries(
         "input_plays_as_device_1 = \"Pad\"\n"
         "input_plays_as_player_1 = \"2\"\n"
         "input_plays_as_device_2 = \"0001:0001 Pad\"\n"
         "input_plays_as_player_2 = \"3\"\n");
   t_connect(1, "Pad", 1, 1);
   t_connect(2, "Pad", 1, 1);
   t_connect(0, "Pad", 1, 1);
   CHECK(input_config_plays_as_entry(0) == -1
         && input_config_plays_as_entry(1) == 0
         && input_config_plays_as_entry(2) == 1,
         "fixture: the pads did not take the entries in the order they came");
   conf = config_file_new_alloc();
   input_plays_as_write(conf);
   CHECK(t_conf_is(conf, "input_plays_as_device_3", NULL)
         && !t_conf_is(conf, "input_plays_as_device_4", NULL),
         "a pad two models' entries name was given two Auto entries");
   config_file_free(conf);

   /* two models' entries naming the same pads, interleaved: unchanged,
    * each keeps its place */
   t_reset();
   t_entries(
         "input_plays_as_device_1 = \"0001:0001 Pad\"\n"
         "input_plays_as_player_1 = \"2\"\n"
         "input_plays_as_device_2 = \"Pad\"\n"
         "input_plays_as_player_2 = \"3\"\n"
         "input_plays_as_device_3 = \"0001:0001 Pad\"\n"
         "input_plays_as_player_3 = \"4\"\n");
   t_connect(0, "Pad", 1, 1);
   t_connect(1, "Pad", 1, 1);
   t_connect(2, "Pad", 1, 1);
   CHECK(input_config_plays_as_player(0) == 2
         && input_config_plays_as_player(1) == 3
         && input_config_plays_as_player(2) == 4,
         "fixture: the pads did not take the entries in slot order");
   conf = config_file_new_alloc();
   input_plays_as_write(conf);
   CHECK(t_conf_is(conf, "input_plays_as_device_2", "Pad")
         && t_conf_is(conf, "input_plays_as_player_2", "3")
         && t_conf_is(conf, "input_plays_as_player_3", "4"),
         "an entry was moved past another model's");
   for (s = 0; s < 3; s++)
      input_config_plays_as_release(s);
   input_plays_as_read(conf);
   CHECK(input_config_plays_as_player(0) == 2
         && input_config_plays_as_player(1) == 3
         && input_config_plays_as_player(2) == 4,
         "the next start swapped pads two models' entries name");
   config_file_free(conf);
}

/* With all sixteen entries in use, an Auto entry for two of a model
 * is not written in place of another controller's. */
static void test_plays_as_write_twins_full(void)
{
   unsigned e;
   char key[48];
   char name[32];
   config_file_t *conf;

   t_reset();
   t_dev(0, "Pad", 1, 1);
   t_dev(1, "Pad", 1, 1);
   input_config_plays_as_set(0, 2);
   input_config_plays_as_set(1, 3);
   for (e = 2; e < MAX_USERS; e++)
   {
      snprintf(name, sizeof(name), "Gone %u", e);
      t_dev(2, name, (uint16_t)(e + 2), 1);
      input_config_plays_as_set(2, 1);
      t_disconnect(2);
   }
   input_config_plays_as_set(0, 0);
   t_dev(2, "New", 99, 1);
   CHECK(input_config_plays_as_set(2, 5)
         && input_config_plays_as_entry(2) == 0,
         "fixture: the new controller did not take the free entry");
   conf = config_file_new_alloc();
   input_plays_as_write(conf);
   for (e = 0; e < MAX_USERS; e++)
   {
      snprintf(key, sizeof(key), "input_plays_as_device_%u", e + 1);
      CHECK(t_conf_is(conf, key, NULL), "an entry was not written");
   }
   CHECK(t_conf_is(conf, "input_plays_as_player_2", "3")
         && t_conf_is(conf, "input_plays_as_device_16", "0011:0001 Gone 15"),
         "an Auto entry took another controller's place");
   CHECK(input_config_plays_as_player(0) == 0
         && input_config_plays_as_player(1) == 3
         && input_config_plays_as_player(2) == 5,
         "writing a full table changed a controller's player");
   config_file_free(conf);
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
   test_vid_pid_parse();
   test_plays_as_match();
   test_plays_as_entry_name();
   test_plays_as_round_trip();
   test_plays_as_entry_life();
   test_place_auto();
   test_place_player();
   test_place_limits();
   test_place_reservation();
   test_place_reservation_order();
   test_place_leave_and_change();
   test_place_again();
   test_place_again_player();
   test_place_again_max_users();
   test_place_again_last_player();
   test_place_again_never_in_effect();
   test_announce_nothing();
   test_join_port_map();
   test_join_follows_mapped_port();
   test_join_poll();
   test_join_reserved();
   test_join_reserved_stays();
   test_rumble_join();
   test_first_press_group();
   test_remap_join();
   test_announce_first_press();
   test_announce_join_ends();
   test_announce_join_netplay();
   test_slot_user();
   test_plays_as_where();
   test_place_no_player();
   test_place_restart();
   test_place_restart_twins();
   test_place_saved_ports();
   test_place_reload_kept();
   test_plays_as_copy();
   test_plays_as_write_twins();
   test_plays_as_write_twins_full();
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
