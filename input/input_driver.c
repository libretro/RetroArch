/**
 *  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Andr s Su rez (input mapper code)
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation, either version 3 of the License, or (at your option)
 *  any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with RetroArch. If not, see <http://www.gnu.org/licenses/>.
 **/

#include <memory/mem_stats.h>
#include "libretro.h"
#include <queues/message_queue.h>
#include <streams/interface_stream.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <string/stdstring.h>
#include <encodings/utf.h>
#include <clamping.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif /* HAVE_CONFIG_H */

#if defined(_WIN32) && !defined(SOCKET)
#include <winsock2.h>
#endif

#include "input_driver.h"
#include "common/input_device_pins.h"
#ifdef HAVE_OVERLAY
#include "../led/led_defines.h"
#endif
#include "../gfx/gfx_instrument.h"
#include "../gfx/gfx_surface.h"
#ifdef HAVE_RPNG
#include <formats/rpng.h>
#endif
#include "input_keymaps.h"
#include "input_remapping.h"
#include "input_osk.h"
#include "input_output_store.h"
#include "input_registry.h"
#include "input_types.h"

#ifdef HAVE_MIST
#include "../steam/steam.h"
#endif

#ifdef HAVE_COCOATOUCH
#include "../ui/drivers/cocoa/apple_platform.h"
#endif

#ifdef HAVE_BSV_MOVIE
#include "bsv/bsvmovie.h"
#endif

#ifdef HAVE_CHEEVOS
#include "../cheevos/cheevos.h"
#endif

#ifdef HAVE_NETWORKING
#include <net/net_compat.h>
#include <net/net_socket.h>
#endif

#ifdef HAVE_NETWORKING
#include "../network/netplay/netplay.h"
#endif

#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#endif

#include "../accessibility.h"
#include "../command.h"
#include "../config.def.keybinds.h"
#include "../configuration.h"
#include "../config.def.h"
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#include "../gfx/common/win32_common.h"
#endif
#include "../core_info.h"
#include "../driver.h"
#include "../frontend/frontend_driver.h"
#include "../list_special.h"
#include "../paths.h"
#include "../performance_counters.h"
#include "../retroarch.h"
#include "../tasks/tasks_internal.h"
#include "../verbosity.h"
#include "../gfx/video_driver.h"

#ifdef ANDROID
#include "../frontend/drivers/platform_unix.h"
#endif

#include "../ai/game_ai.h"
#include <compat/strl.h>
#ifdef HAVE_MCP
#include "../network/mcp_server.h"
#endif
#ifdef HAVE_CRYPTO
#include <crypto/crypto.h>
#include <crypto/kdf.h>
#endif
#ifdef __MACH__
#include <TargetConditionals.h>
#endif

/* Force a helper out of line even though it has a single call site.
 * Follows the RXML_NOINLINE precedent in
 * libretro-common/formats/xml/rxml.c.  Under -Os the compiler already
 * optimises for size and the outlining only adds call overhead, so it
 * is disabled there. */
#if defined(__OPTIMIZE_SIZE__)
#define INPUT_NOINLINE
#elif defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))
#define INPUT_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define INPUT_NOINLINE __declspec(noinline)
#else
#define INPUT_NOINLINE
#endif

#define HOLD_BTN_DELAY_SEC 2

/* Precomputed reciprocals used in analog input scaling.
 * 0x8000 = 32768 is the full int16 range (unsigned half),
 * 0x7fff = 32767 is the max positive int16 value. */
#define INV_0x8000 (1.0f / 0x8000)
#define INV_0x7fff (1.0f / 0x7fff)

/* Depends on ASCII character values */
#define ISPRINT(c) (((int)(c) >= ' ' && (int)(c) <= '~') ? 1 : 0)
#define IS_UTF8_CONTINUATION(c) ((((uint8_t)(c)) & 0xc0) == 0x80)

#define INPUT_REMOTE_KEY_PRESSED(input_st, key, port) (input_st->remote_st_ptr.buttons[(port)] & (UINT64_C(1) << (key)))

#define IS_COMPOSITION(c)       ( (c & 0x0F000000) ? 1 : 0)
#define IS_COMPOSITION_KR(c)    ( (c & 0x01000000) ? 1 : 0)
#define IS_END_COMPOSITION(c)   ( (c & 0xF0000000) ? 1 : 0)

struct input_remote
{
#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
#ifdef _WIN32
   SOCKET net_fd[MAX_USERS];
#else
   int net_fd[MAX_USERS];
#endif
   /* "First Sender Only": the address each user listens to, once a
    * first message has come */
   uint32_t sender[MAX_USERS];
   bool     sender_known[MAX_USERS];
   /* the device last heard, and the port listened on, for the menu */
   uint32_t last[MAX_USERS];
   bool     heard[MAX_USERS];
   uint16_t port[MAX_USERS];
#endif
   bool state[RARCH_BIND_LIST_END];
};


/**
 * check_input_driver_block_hotkey:
 *
 * Checks if 'hotkey enable' key is pressed.
 *
 * If we haven't bound anything to this,
 * always allow hotkeys.

 * If we hold ENABLE_HOTKEY button, block all libretro input to allow
 * hotkeys to be bound to same keys as RetroPad.
 **/
#define CHECK_INPUT_DRIVER_BLOCK_HOTKEY(normal_bind, autoconf_bind) \
( \
         (((RETRO_KEYBIND_KEY(normal_bind))      != RETROK_UNKNOWN) \
      || ((normal_bind)->mbutton   != NO_BTN) \
      || ((normal_bind)->joykey    != NO_BTN) \
      || ((normal_bind)->joyaxis   != AXIS_NONE) \
      || ((RETRO_KEYBIND_KEY(autoconf_bind))     != RETROK_UNKNOWN) \
      || ((autoconf_bind)->joykey  != NO_BTN) \
      || ((autoconf_bind)->joyaxis != AXIS_NONE)) \
)

/* Human readable order of input binds */
const unsigned input_config_bind_order[24] = {
   RETRO_DEVICE_ID_JOYPAD_UP,
   RETRO_DEVICE_ID_JOYPAD_DOWN,
   RETRO_DEVICE_ID_JOYPAD_LEFT,
   RETRO_DEVICE_ID_JOYPAD_RIGHT,
   RETRO_DEVICE_ID_JOYPAD_B,
   RETRO_DEVICE_ID_JOYPAD_A,
   RETRO_DEVICE_ID_JOYPAD_Y,
   RETRO_DEVICE_ID_JOYPAD_X,
   RETRO_DEVICE_ID_JOYPAD_SELECT,
   RETRO_DEVICE_ID_JOYPAD_START,
   RETRO_DEVICE_ID_JOYPAD_L,
   RETRO_DEVICE_ID_JOYPAD_R,
   RETRO_DEVICE_ID_JOYPAD_L2,
   RETRO_DEVICE_ID_JOYPAD_R2,
   RETRO_DEVICE_ID_JOYPAD_L3,
   RETRO_DEVICE_ID_JOYPAD_R3,
   19, /* Left Analog Up */
   18, /* Left Analog Down */
   17, /* Left Analog Left */
   16, /* Left Analog Right */
   23, /* Right Analog Up */
   22, /* Right Analog Down */
   21, /* Right Analog Left */
   20, /* Right Analog Right */
};

/**************************************/
/* TODO/FIXME - turn these into static global variable */
retro_keybind_set input_config_binds[MAX_USERS];
retro_keybind_set input_autoconf_binds[MAX_USERS];
input_bind_label_set input_config_bind_labels[MAX_USERS];
input_bind_label_set input_autoconf_bind_labels[MAX_USERS];

static void *input_null_init(const char *joypad_driver) { return (void*)-1; }
static void input_null_poll(void *data) { }
static int16_t input_null_input_state(
      void *data,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      rarch_joypad_info_t *joypad_info,
      const retro_keybind_set *retro_keybinds,
      bool keyboard_mapping_blocked,
      unsigned port, unsigned device, unsigned index, unsigned id) { return 0; }
static void input_null_free(void *data) { }
static bool input_null_set_sensor_state(void *data, unsigned port,
         enum retro_sensor_action action, unsigned rate) { return false; }
static float input_null_get_sensor_input(void *data, unsigned port, unsigned id) { return 0.0; }
static uint64_t input_null_get_capabilities(void *data) { return 0; }
static void input_null_grab_mouse(void *data, bool state) { }
static bool input_null_grab_stdin(void *data) { return false; }
static void input_null_keypress_vibrate(void) { }

static input_driver_t input_null = {
   input_null_init,
   input_null_poll,
   input_null_input_state,
   input_null_free,
   input_null_set_sensor_state,
   input_null_get_sensor_input,
   input_null_get_capabilities,
   "null",
   input_null_grab_mouse,
   input_null_grab_stdin,
   input_null_keypress_vibrate
};

static input_device_driver_t null_joypad = {
   NULL, /* init */
   NULL, /* query_pad */
   NULL, /* destroy */
   NULL, /* button */
   NULL, /* state */
   NULL, /* get_buttons */
   NULL, /* axis */
   NULL, /* poll */
   NULL, /* rumble */
   NULL, /* rumble_gain */
   NULL, /* set_sensor_state */
   NULL, /* get_sensor_input */
   NULL, /* name */
   "null",
};


/* Stands in for the joypad drivers on the read paths while background
 * controller input is off and the window is unfocused (see
 * input_driver_joypad_for_read()). Every read is idle and nothing
 * reaches the real driver. Every read entry is filled, because readers
 * call button/axis/state without checking for NULL. */
static bool idle_joypad_query(unsigned pad) { return false; }
static int32_t idle_joypad_button(unsigned port, uint16_t joykey) { return 0; }
static int16_t idle_joypad_state(rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds, unsigned port) { return 0; }
static void idle_joypad_get_buttons(unsigned port, input_bits_t *state)
{
   BIT256_CLEAR_ALL_PTR(state);
}
static int16_t idle_joypad_axis(unsigned port, uint32_t joyaxis) { return 0; }
static void idle_joypad_poll(void) { }
static bool idle_joypad_rumble(unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength) { return false; }
static bool idle_joypad_rumble_gain(unsigned pad, unsigned gain) { return false; }
static bool idle_joypad_set_sensor_state(unsigned port,
      enum retro_sensor_action action, unsigned rate) { return false; }
static bool idle_joypad_get_sensor_input(unsigned port,
      unsigned id, float *value) { return false; }
static const char *idle_joypad_name(unsigned pad) { return NULL; }

static const input_device_driver_t idle_joypad = {
   NULL, /* init */
   idle_joypad_query,
   NULL, /* destroy */
   idle_joypad_button,
   idle_joypad_state,
   idle_joypad_get_buttons,
   idle_joypad_axis,
   idle_joypad_poll,
   idle_joypad_rumble,
   idle_joypad_rumble_gain,
   idle_joypad_set_sensor_state,
   idle_joypad_get_sensor_input,
   idle_joypad_name,
   "idle",
};

/* input_driver_state_t::frame_valid keeps a bit per port in 16-bit
 * words. */
typedef char input_frame_valid_fits_ports[(MAX_USERS <= 16) ? 1 : -1];

static const input_device_driver_t *input_joypad_for_read_(
      input_driver_state_t *st, const input_device_driver_t *drv);

#define INPUT_JOYPAD_FOR_READ(st, drv) input_joypad_for_read_((st), (drv))

#ifdef HAVE_HID
static bool null_hid_joypad_query(void *data, unsigned pad) {
   return pad < MAX_USERS; }
static const char *null_hid_joypad_name(
      void *data, unsigned pad) { return NULL; }
static void null_hid_joypad_get_buttons(void *data,
      unsigned port, input_bits_t *state) { BIT256_CLEAR_ALL_PTR(state); }
static int16_t null_hid_joypad_button(
      void *data, unsigned port, uint16_t joykey) { return 0; }
static bool null_hid_joypad_rumble(void *data, unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength) { return false; }
static int16_t null_hid_joypad_axis(
      void *data, unsigned port, uint32_t joyaxis) { return 0; }
static void *null_hid_init(void) { return (void*)-1; }
static void null_hid_free(const void *data) { }
static void null_hid_poll(void *data) { }
static int16_t null_hid_joypad_state(
      void *data,
      rarch_joypad_info_t *joypad_info,
      const void *binds_data,
      unsigned port) { return 0; }

static hid_driver_t null_hid = {
   null_hid_init,               /* init */
   null_hid_joypad_query,       /* joypad_query */
   null_hid_free,               /* free */
   null_hid_joypad_button,      /* button */
   null_hid_joypad_state,       /* state */
   null_hid_joypad_get_buttons, /* get_buttons */
   null_hid_joypad_axis,        /* axis */
   null_hid_poll,               /* poll */
   null_hid_joypad_rumble,      /* rumble */
   null_hid_joypad_name,        /* joypad_name */
   "null",
};
#endif

input_device_driver_t *joypad_drivers[] = {
#ifdef HAVE_XINPUT
   &xinput_joypad,
#endif
#ifdef GEKKO
   &gx_joypad,
#endif
#ifdef WIIU
   &wiiu_joypad,
#endif
#ifdef _XBOX1
   &xdk_joypad,
#endif
#if defined(ORBIS)
   &ps4_joypad,
#endif
#if defined(__PSL1GHT__) || defined(__PS3__)
   &ps3_joypad,
#endif
#if defined(PSP) || defined(VITA)
   &psp_joypad,
#endif
#if defined(PS2)
   &ps2_joypad,
#endif
#ifdef _3DS
   &ctr_joypad,
#endif
#ifdef SWITCH
   &switch_joypad,
#endif
#ifdef HAVE_DINPUT
   &dinput_joypad,
#endif
#ifdef HAVE_UDEV
   &udev_joypad,
#endif
#if defined(__linux) && !defined(ANDROID)
   &linuxraw_joypad,
#endif
#ifdef HAVE_PARPORT
   &parport_joypad,
#endif
#ifdef ANDROID
   &android_joypad,
#endif
#if defined(HAVE_SDL3)
   &sdl3_joypad,
#elif defined(HAVE_SDL2)
   &sdl2_joypad,
#elif defined(HAVE_SDL)
   &sdl1_joypad,
#endif
#if defined(DINGUX) && defined(HAVE_SDL_DINGUX)
   &sdl_dingux_joypad,
#endif
#ifdef __QNX__
   &qnx_joypad,
#endif
#ifdef HAVE_MFI
   &mfi_joypad,
#endif
#ifdef DJGPP
   &dos_joypad,
#endif
/* Selecting the HID gamepad driver disables the Wii U gamepad. So while
 * we want the HID code to be compiled & linked, we don't want the driver
 * to be selectable in the UI. */
#if defined(HAVE_HID) && !defined(WIIU)
   &hid_joypad,
#endif
#ifdef __EMSCRIPTEN__
   &rwebpad_joypad,
#endif
#if defined(_WIN32) && !defined(_XBOX) && _WIN32_WINNT >= 0x0501 && !defined(__WINRT__)
#ifdef HAVE_WINRAWINPUT
   &winraw_joypad,
#endif
#endif
#ifdef HAVE_TEST_DRIVERS
   &test_joypad,
#endif
   &null_joypad,
   NULL,
};

input_driver_t *input_drivers[] = {
#ifdef ORBIS
   &input_ps4,
#endif
#if defined(__PSL1GHT__) || defined(__PS3__)
   &input_ps3,
#endif
#if defined(SN_TARGET_PSP2) || defined(PSP) || defined(VITA)
   &input_psp,
#endif
#if defined(PS2)
   &input_ps2,
#endif
#if defined(_3DS)
   &input_ctr,
#endif
#if defined(SWITCH)
   &input_switch,
#endif
#ifdef HAVE_X11
   &input_x,
#endif
#ifdef HAVE_WAYLAND
   &input_wayland,
#endif
#ifdef __WINRT__
   &input_uwp,
#endif
#ifdef XENON
   &input_xenon360,
#endif
#if defined(_WIN32) && !defined(_XBOX) && _WIN32_WINNT >= 0x0501 && !defined(__WINRT__)
#ifdef HAVE_WINRAWINPUT
   /* winraw only available since XP */
   &input_winraw,
#endif
#endif
#if defined(HAVE_XINPUT2) || defined(HAVE_XINPUT_XBOX1) || defined(__WINRT__)
   &input_xinput,
#endif
#ifdef HAVE_DINPUT
   &input_dinput,
#endif
#if defined(HAVE_SDL2) && !defined(HAVE_COCOA)
   &input_sdl2,
#elif defined(HAVE_SDL) && !defined(HAVE_COCOA)
   &input_sdl1,
#endif
#if defined(HAVE_SDL3) && !defined(HAVE_COCOA)
   &input_sdl3,
#endif
#if defined(DINGUX) && defined(HAVE_SDL_DINGUX)
   &input_sdl_dingux,
#endif
#ifdef GEKKO
   &input_gx,
#endif
#ifdef WIIU
   &input_wiiu,
#endif
#ifdef ANDROID
   &input_android,
#endif
#ifdef HAVE_UDEV
   &input_udev,
#endif
#if defined(__linux__) && !defined(ANDROID)
   &input_linuxraw,
#endif
#if defined(HAVE_COCOA) || defined(HAVE_COCOATOUCH)
   &input_cocoa,
#endif
#ifdef __QNX__
   &input_qnx,
#endif
#ifdef __EMSCRIPTEN__
   &input_rwebinput,
#endif
#ifdef DJGPP
   &input_dos,
#endif
#ifdef HAVE_TEST_DRIVERS
   &input_test,
#endif
   &input_null,
   NULL,
};

#ifdef HAVE_HID
hid_driver_t *hid_drivers[] = {
#if defined(HAVE_BTSTACK)
   &btstack_hid,
#endif
#if defined(__APPLE__) && defined(HAVE_IOHIDMANAGER)
   &iohidmanager_hid,
#endif
#if defined(HAVE_LIBUSB) && defined(HAVE_THREADS)
   &libusb_hid,
#endif
#ifdef HAVE_WIIUSB_HID
   &wiiusb_hid,
#endif
#ifdef HAVE_GEKKO_HID
   &gekko_hid,
#endif
#if defined(WIIU)
   &wiiu_hid,
#endif
   &null_hid,
   NULL,
};
#endif

static input_driver_state_t input_driver_st; /* double alignment */

/* --- The snapshot bridge ------------------------------------------------
 *
 * Everything that reads a controller - a core's state query, the
 * hotkeys, the menu - calls into the joypad driver for it: a button
 * here, an axis there, at whatever moment in the frame the reader
 * gets to it. A driver whose state is filled in by a thread of its
 * own can change between two of those calls, so a reader can see half
 * of one report and half of the next, and no reader can be moved off
 * the frame's thread while it has to call the driver.
 *
 * The bridge puts a snapshot between the two. The first read of a pad
 * after a poll copies the pad out of the driver in one go: its buttons,
 * and the axes and hats readers have asked for before. Every read
 * after that, until the next poll, is served from the copy. A control
 * nobody has asked for yet is fetched from the driver the first time
 * and is part of the next copy; one that was not read out of a copy is
 * left out of the next. So what is copied follows what the binds use,
 * without anyone having to say when binds change.
 *
 * It is a joypad driver as far as readers can tell: the real driver's
 * table with button, axis, state and get_buttons replaced. state() is
 * the loop every driver has, run against the copy. Rumble, sensors,
 * names and the poll itself still go to the real driver.
 *
 * This is the legacy bridge of the input plan (WP-03, WP-05). A driver
 * is switched over when it has been checked - that its get_buttons
 * says what its button() says, and its state() is the common loop -
 * and the rest read their drivers as before. */
#define INPUT_SNAPSHOT_AXES 16
#define INPUT_SNAPSHOT_HATS 4

typedef struct
{
   input_bits_t buttons;
   int16_t      axes[INPUT_SNAPSHOT_AXES];
   uint16_t     axes_known; /* axes in the copy */
   uint16_t     axes_used;  /* axes read since the copy was taken */
   uint16_t     hats;       /* four bits a hat: up, down, left, right */
   uint8_t      hats_known;
   uint8_t      hats_used;
} input_pad_snapshot_t;

typedef struct
{
   const input_device_driver_t *real;
   input_device_driver_t        adapter;
   input_pad_snapshot_t         pads[MAX_USERS];
} input_snapshot_bridge_t;

static input_snapshot_bridge_t input_snapshot_bridge[2];
static bool                    input_snapshot_forced;

void input_driver_set_snapshot_bridge(bool on)
{
   input_snapshot_forced = on;
}

/* Drivers checked for the bridge: that get_buttons() says what
 * button() says for every plain button, that hats and axes answer
 * through button() and axis() alone, and that state() is the common
 * loop over the binds.
 *
 * - xinput (xinput_hybrid_joypad.c, the Windows driver with XInput and
 *   DirectInput pads). Its state() compares an axis against the
 *   threshold as integers where the common loop divides; the two agree
 *   for every threshold below 1.0, and at 1.0 the driver's own wraps
 *   and reads every bound axis as pressed, which the bridge does not.
 *   The XInput-only driver of the same name has no get_buttons() and
 *   stays as it was.
 *
 * - winraw_joypad (winraw_joypad.c, the Windows RawInput driver). Its
 *   state() is the common loop with the button and axis reads written
 *   out in place, and compares an axis against the threshold as 32-bit
 *   integers, which agrees with the common loop at every threshold.
 *
 * - udev (udev_joypad.c, Linux). get_buttons() hands over the same
 *   sixty-four bits button() reads, and its state() is the common
 *   loop as written. It has more axes than a copy holds; the ones
 *   past the sixteenth are read from the driver as before.
 *
 * - dinput (dinput_joypad.c, the DirectInput-only Windows driver). It
 *   had no get_buttons(); it has one now, over the same button array
 *   button() reads. Its state() compares an axis against the
 *   threshold as 32-bit integers, as winraw_joypad's does.
 *
 * - linuxraw (linuxraw_joypad.c, Linux). Its get_buttons() handed
 *   over sixteen of the thirty-two buttons button() reads; it hands
 *   over all of them now. No hats; state() is the common loop.
 *
 * - sdl2 (sdl2_joypad.c) and sdl3 (sdl3_joypad.c). Neither had a
 *   get_buttons(); each has one now, a walk of the pad's buttons
 *   through the call button() makes. state() is the common loop.
 *   With these drivers a read is a call into SDL, so here the copy
 *   also saves those.
 *
 * - xinput, the XInput-only driver (xinput_joypad.c: UWP, Xbox, and
 *   builds without DirectInput). It has a get_buttons() now, so it is
 *   served like the hybrid driver of the same name.
 *
 * samples/input/joypad_bridge_checked holds udev, dinput, linuxraw,
 * sdl2, sdl3 and the XInput-only driver to that: for random pads, binds
 * and thresholds, what the bridge would make of the driver's answers
 * is what the driver's own state() says.
 *
 * Asked several times a frame, so the answer is kept with the driver
 * it was for. */
static bool input_snapshot_driver_checked(const input_device_driver_t *drv)
{
   static const input_device_driver_t *asked;
   static bool                         answer;

   if (drv != asked)
   {
      asked  = drv;
      answer = drv->ident
         && (   string_is_equal(drv->ident, "xinput")
             || string_is_equal(drv->ident, "winraw_joypad")
             || string_is_equal(drv->ident, "udev")
             || string_is_equal(drv->ident, "dinput")
             || string_is_equal(drv->ident, "linuxraw")
             || string_is_equal(drv->ident, "sdl2")
             || string_is_equal(drv->ident, "sdl3"));
      /* While drivers are being switched over: RETROARCH_INPUT_SNAPSHOT=0
       * in the environment reads the driver directly, so a problem can
       * be tried with and without the bridge on one build. */
      if (answer)
      {
         const char *env = getenv("RETROARCH_INPUT_SNAPSHOT");
         if (env && env[0] == '0')
            answer = false;
      }
      if (answer && drv->get_buttons && drv->button && drv->axis)
         RARCH_LOG("[Input] Controllers of the \"%s\" driver are read"
               " through a snapshot taken once a poll.\n", drv->ident);
   }
   return answer;
}

static void input_snapshot_fetch_axis(const input_device_driver_t *real,
      input_pad_snapshot_t *snap, unsigned pad, unsigned i)
{
   /* a driver gives each direction on its own; one of the two is 0 */
   snap->axes[i] = (int16_t)(real->axis(pad, AXIS_POS(i))
                           + real->axis(pad, AXIS_NEG(i)));
}

static void input_snapshot_fetch_hat(const input_device_driver_t *real,
      input_pad_snapshot_t *snap, unsigned pad, unsigned h)
{
   uint16_t bits = 0;
   if (real->button(pad, (uint16_t)HAT_MAP(h, HAT_UP_MASK)))
      bits |= 1;
   if (real->button(pad, (uint16_t)HAT_MAP(h, HAT_DOWN_MASK)))
      bits |= 2;
   if (real->button(pad, (uint16_t)HAT_MAP(h, HAT_LEFT_MASK)))
      bits |= 4;
   if (real->button(pad, (uint16_t)HAT_MAP(h, HAT_RIGHT_MASK)))
      bits |= 8;
   snap->hats = (uint16_t)((snap->hats & ~(0xf << (h * 4))) | (bits << (h * 4)));
}

/* The pad's copy for this frame, taken now if this is the first read
 * since the poll. */
static input_pad_snapshot_t *input_snapshot_pad(unsigned b, unsigned pad)
{
   input_snapshot_bridge_t *bridge = &input_snapshot_bridge[b];
   input_pad_snapshot_t *snap      = &bridge->pads[pad];
   uint16_t *valid                 = &input_driver_st.frame_valid.snapshot[b];

   if (!(*valid & (1 << pad)))
   {
      unsigned i;

      /* Copy what was read out of the last copy, and no more: a
       * control that has stopped being read - the bind screen scans
       * every axis, a remap drops a stick - leaves the copy a frame
       * later, and one that is read again comes back the same way. */
      snap->axes_known = snap->axes_used;
      snap->hats_known = snap->hats_used;
      snap->axes_used  = 0;
      snap->hats_used  = 0;

      bridge->real->get_buttons(pad, &snap->buttons);
      for (i = 0; i < INPUT_SNAPSHOT_AXES; i++)
         if (snap->axes_known & (1 << i))
            input_snapshot_fetch_axis(bridge->real, snap, pad, i);
      for (i = 0; i < INPUT_SNAPSHOT_HATS; i++)
         if (snap->hats_known & (1 << i))
            input_snapshot_fetch_hat(bridge->real, snap, pad, i);
      *valid |= (1 << pad);
   }
   return snap;
}

static int32_t input_snapshot_button(unsigned b, unsigned pad, uint16_t joykey)
{
   const input_device_driver_t *real = input_snapshot_bridge[b].real;
   input_pad_snapshot_t *snap;
   uint16_t dir                      = GET_HAT_DIR(joykey);

   if (pad >= MAX_USERS)
      return real->button(pad, joykey);

   if (dir)
   {
      unsigned h   = GET_HAT(joykey);
      unsigned bit;

      switch (dir)
      {
         case HAT_UP_MASK:    bit = 0; break;
         case HAT_DOWN_MASK:  bit = 1; break;
         case HAT_LEFT_MASK:  bit = 2; break;
         case HAT_RIGHT_MASK: bit = 3; break;
         default:
            return real->button(pad, joykey);
      }
      if (h >= INPUT_SNAPSHOT_HATS)
         return real->button(pad, joykey);

      snap = input_snapshot_pad(b, pad);
      if (!(snap->hats_known & (1 << h)))
      {
         input_snapshot_fetch_hat(real, snap, pad, h);
         snap->hats_known |= (1 << h);
      }
      snap->hats_used |= (1 << h);
      return (snap->hats >> (h * 4 + bit)) & 1;
   }

   if (joykey >= 256)
      return real->button(pad, joykey);

   snap = input_snapshot_pad(b, pad);
   return BIT256_GET(snap->buttons, joykey) ? 1 : 0;
}

static int16_t input_snapshot_axis(unsigned b, unsigned pad, uint32_t joyaxis)
{
   const input_device_driver_t *real = input_snapshot_bridge[b].real;
   input_pad_snapshot_t *snap;
   bool     negative                 = AXIS_NEG_GET(joyaxis) < INPUT_SNAPSHOT_AXES;
   unsigned i                        = negative
      ? AXIS_NEG_GET(joyaxis) : AXIS_POS_GET(joyaxis);
   int16_t  value;

   if (pad >= MAX_USERS || i >= INPUT_SNAPSHOT_AXES)
      return real->axis(pad, joyaxis);

   snap = input_snapshot_pad(b, pad);
   if (!(snap->axes_known & (1 << i)))
   {
      input_snapshot_fetch_axis(real, snap, pad, i);
      snap->axes_known |= (1 << i);
   }
   snap->axes_used |= (1 << i);
   value = snap->axes[i];
   if (negative)
      return (value < 0) ? value : 0;
   return (value > 0) ? value : 0;
}

/* Whether @joyaxis, bound to a trigger of controller @pad, is known to
 * rest at the far end from its bind. */
static bool input_trigger_rests_far(unsigned pad, uint32_t joyaxis)
{
   unsigned a = (AXIS_NEG_GET(joyaxis) < 16)
      ? AXIS_NEG_GET(joyaxis) : AXIS_POS_GET(joyaxis);
   return pad < MAX_USERS && a < 16
      && (input_driver_st.trigger_rest[pad] & (1 << a)) != 0;
}

/* The pull of such a trigger, 0 to 0x7fff, counted from its rest. */
static int16_t input_trigger_pull(const input_device_driver_t *drv,
      unsigned pad, uint32_t joyaxis)
{
   bool negative = AXIS_NEG_GET(joyaxis) < 16;
   unsigned a    = negative ? AXIS_NEG_GET(joyaxis) : AXIS_POS_GET(joyaxis);
   int raw       = drv->axis(pad, AXIS_POS(a)) + drv->axis(pad, AXIS_NEG(a));
   if (negative)
      raw        = -raw;
   raw           = (raw + 0x7fff) / 2;
   return (int16_t)((raw < 0) ? 0 : (raw > 0x7fff) ? 0x7fff : raw);
}

/* Learns it: the axis is at the far end, and its other direction is
 * bound to nothing (a combined axis has a trigger on each). */
static void input_trigger_learn(const input_device_driver_t *drv,
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds, unsigned pad, uint32_t joyaxis)
{
   unsigned i;
   input_driver_state_t *input_st = &input_driver_st;
   bool negative = AXIS_NEG_GET(joyaxis) < 16;
   unsigned a    = negative ? AXIS_NEG_GET(joyaxis) : AXIS_POS_GET(joyaxis);
   uint32_t other  = negative ? AXIS_POS(a) : AXIS_NEG(a);
   int there;

   if (a >= 16 || ((input_st->trigger_rest[pad]
               | input_st->trigger_two_way[pad]) & (1 << a)))
      return;
   there = drv->axis(pad, other);
   if (there > -0x7000 && there < 0x7000)
      return;
   for (i = 0; i < RARCH_BIND_LIST_END; i++)
      if (     binds[i].joyaxis == other
            || (   binds[i].joyaxis == AXIS_NONE
                && joypad_info->auto_binds[i].joyaxis == other))
      {
         input_st->trigger_two_way[pad] |= (1 << a);
         return;
      }
   input_st->trigger_rest[pad] |= (1 << a);
}

/* The RetroPad mask from the binds: the loop every joypad driver's
 * state() is, against the copy. */
static int16_t input_snapshot_state(unsigned b,
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds)
{
   unsigned i;
   int16_t  ret = 0;
   uint16_t pad = joypad_info->joy_idx;
   bool full_range = config_get_ptr()->bools.input_trigger_full_range;

   if (pad >= MAX_USERS)
      return 0;

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[i].joykey != NO_BTN)
         ? binds[i].joykey  : joypad_info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis != AXIS_NONE)
         ? binds[i].joyaxis : joypad_info->auto_binds[i].joyaxis;
      if (     (uint16_t)joykey != NO_BTN
            && input_snapshot_button(b, pad, (uint16_t)joykey))
         ret |= (1 << i);
      else if (joyaxis != AXIS_NONE)
      {
         int value;
         /* "Full-Range Analog Triggers" */
         if (     full_range
               && (   i == RETRO_DEVICE_ID_JOYPAD_L2
                   || i == RETRO_DEVICE_ID_JOYPAD_R2))
         {
            const input_device_driver_t *drv = &input_snapshot_bridge[b].adapter;
            input_trigger_learn(drv, joypad_info, binds, pad, joyaxis);
            value = input_trigger_rests_far(pad, joyaxis)
               ? input_trigger_pull(drv, pad, joyaxis)
               : abs(input_snapshot_axis(b, pad, joyaxis));
         }
         else
            value = abs(input_snapshot_axis(b, pad, joyaxis));
         if (((float)value / 0x8000) > joypad_info->axis_threshold)
            ret |= (1 << i);
      }
   }
   return ret;
}

static void input_snapshot_get_buttons(unsigned b, unsigned pad,
      input_bits_t *state)
{
   if (pad >= MAX_USERS)
   {
      input_snapshot_bridge[b].real->get_buttons(pad, state);
      return;
   }
   *state = input_snapshot_pad(b, pad)->buttons;
}

/* A driver's functions take no context, so each bridge has its own. */
static int32_t input_snapshot0_button(unsigned pad, uint16_t joykey)
{ return input_snapshot_button(0, pad, joykey); }
static int16_t input_snapshot0_axis(unsigned pad, uint32_t joyaxis)
{ return input_snapshot_axis(0, pad, joyaxis); }
static int16_t input_snapshot0_state(rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds, unsigned port)
{ return input_snapshot_state(0, joypad_info, binds); }
static void input_snapshot0_get_buttons(unsigned pad, input_bits_t *state)
{ input_snapshot_get_buttons(0, pad, state); }

static int32_t input_snapshot1_button(unsigned pad, uint16_t joykey)
{ return input_snapshot_button(1, pad, joykey); }
static int16_t input_snapshot1_axis(unsigned pad, uint32_t joyaxis)
{ return input_snapshot_axis(1, pad, joyaxis); }
static int16_t input_snapshot1_state(rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds, unsigned port)
{ return input_snapshot_state(1, joypad_info, binds); }
static void input_snapshot1_get_buttons(unsigned pad, input_bits_t *state)
{ input_snapshot_get_buttons(1, pad, state); }

/* The bridge for @drv, set up if it is not the driver it last stood in
 * for; NULL if @drv is to be read directly. */
static const input_device_driver_t *input_snapshot_for(
      input_driver_state_t *st, const input_device_driver_t *drv)
{
   unsigned b = (drv == st->secondary_joypad && drv != st->primary_joypad)
      ? 1 : 0;
   input_snapshot_bridge_t *bridge = &input_snapshot_bridge[b];

   if (!input_snapshot_forced && !input_snapshot_driver_checked(drv))
      return NULL;
   /* the bridge reads through all three */
   if (!drv->get_buttons || !drv->button || !drv->axis)
      return NULL;

   if (bridge->real != drv)
   {
      memset(bridge->pads, 0, sizeof(bridge->pads));
      st->frame_valid.snapshot[b] = 0;
      bridge->real                = drv;
      bridge->adapter             = *drv;
      bridge->adapter.init        = NULL;
      bridge->adapter.destroy     = NULL;
      if (b == 0)
      {
         bridge->adapter.button      = input_snapshot0_button;
         bridge->adapter.axis        = input_snapshot0_axis;
         bridge->adapter.state       = input_snapshot0_state;
         bridge->adapter.get_buttons = input_snapshot0_get_buttons;
      }
      else
      {
         bridge->adapter.button      = input_snapshot1_button;
         bridge->adapter.axis        = input_snapshot1_axis;
         bridge->adapter.state       = input_snapshot1_state;
         bridge->adapter.get_buttons = input_snapshot1_get_buttons;
      }
   }
   return &bridge->adapter;
}

/* A new controller sits in @slot: what readers asked of the last one
 * says nothing about this one. */
static void input_snapshot_forget_pad(unsigned slot)
{
   unsigned b;
   if (slot >= MAX_USERS)
      return;
   for (b = 0; b < 2; b++)
   {
      memset(&input_snapshot_bridge[b].pads[slot], 0,
            sizeof(input_snapshot_bridge[b].pads[slot]));
      input_driver_st.frame_valid.snapshot[b] &= ~(1 << slot);
   }
}

static const input_device_driver_t *input_joypad_for_read_(
      input_driver_state_t *st, const input_device_driver_t *drv)
{
   const input_device_driver_t *bridge;

   if (!drv)
      return NULL;
   if (st->flags & INP_FLAG_JOYPAD_UNFOCUSED)
      return &idle_joypad;
   if ((bridge = input_snapshot_for(st, drv)))
      return bridge;
   return drv;
}

const input_device_driver_t *input_driver_joypad_for_read(
      const input_device_driver_t *drv)
{
   return INPUT_JOYPAD_FOR_READ(&input_driver_st, drv);
}

/* Publishes INP_FLAG_JOYPAD_UNFOCUSED for this poll. Only controllers
 * are gated here; the keyboard and mouse input drivers apply their own
 * focus checks. The focus query only runs with the setting off. */
static void input_driver_update_joypad_focus(
      input_driver_state_t *input_st, const settings_t *settings)
{
   if (     !settings->bools.input_joypad_background
         && !video_driver_has_focus())
      input_st->flags |=  INP_FLAG_JOYPAD_UNFOCUSED;
   else
      input_st->flags &= ~INP_FLAG_JOYPAD_UNFOCUSED;
}

/**************************************/

input_driver_state_t *input_state_get_ptr(void)
{
   return &input_driver_st;
}

/**
 * config_get_input_driver_options:
 *
 * Get an enumerated list of all input driver names, separated by '|'.
 *
 * Returns: string listing of all input driver names, separated by '|'.
 **/
const char* config_get_input_driver_options(void)
{
   return char_list_new_special(STRING_LIST_INPUT_DRIVERS, NULL);
}

/**
 * config_get_joypad_driver_options:
 *
 * Get an enumerated list of all joypad driver names, separated by '|'.
 *
 * Returns: string listing of all joypad driver names, separated by '|'.
 **/
const char* config_get_joypad_driver_options(void)
{
   return char_list_new_special(STRING_LIST_INPUT_JOYPAD_DRIVERS, NULL);
}

/**
 * Finds first suitable joypad driver and initializes. Used as a fallback by
 * input_joypad_init_driver when no matching driver is found.
 *
 * @param data        joypad state data pointer, which can be NULL and will be
 *                    initialized by the new joypad driver, if one is found.
 * @param skip_ident  optional ident of a driver that was already tried (and
 *                    failed) as the explicitly configured driver; skipped
 *                    here since re-running its init would just fail again
 *                    (and, for the hybrid XInput driver, repeat the
 *                    controller slot probe).
 *
 * @return joypad driver if found and initialized, otherwise NULL.
 **/
static const input_device_driver_t *input_joypad_init_first(void *data,
      const char *skip_ident)
{
   int i;
   for (i = 0; joypad_drivers[i]; i++)
   {
      if (     joypad_drivers[i]
            && joypad_drivers[i]->init)
      {
         void *ptr;
         if (     skip_ident
               && string_is_equal(skip_ident, joypad_drivers[i]->ident))
            continue;
         ptr = joypad_drivers[i]->init(data);
         if (ptr)
         {
            RARCH_LOG("[Input] Found joypad driver: \"%s\".\n",
                  joypad_drivers[i]->ident);
            return joypad_drivers[i];
         }
      }
   }

   return NULL;
}

bool input_driver_set_rumble(
         unsigned port, unsigned joy_idx,
         enum retro_rumble_effect effect, uint16_t strength)
{
   const input_device_driver_t  *primary_joypad;
   const input_device_driver_t      *sec_joypad;
   bool rumble_state   = false;

   if (joy_idx >= MAX_USERS)
      return false;

   primary_joypad = input_driver_st.primary_joypad;
   sec_joypad     = input_driver_st.secondary_joypad;

   if (primary_joypad && primary_joypad->set_rumble)
      rumble_state = primary_joypad->set_rumble(joy_idx, effect, strength);
   /* if sec_joypad exists, this set_rumble() return value will replace primary_joypad's return */
   if (sec_joypad     && sec_joypad->set_rumble)
      rumble_state = sec_joypad->set_rumble(joy_idx, effect, strength);

   return rumble_state;
}

bool input_driver_set_rumble_gain(
         unsigned gain,
         unsigned input_max_users)
{
   int i;

   if (  input_driver_st.primary_joypad
      && input_driver_st.primary_joypad->set_rumble_gain)
   {
      for (i = 0; i < (int)input_max_users; i++)
         input_driver_st.primary_joypad->set_rumble_gain(i, gain);
      return true;
   }
   return false;
}

bool input_driver_set_sensor(
         unsigned port, bool sensors_enable,
         enum retro_sensor_action action, unsigned rate)
{
   const input_driver_t *current_driver;
   bool enabled    = false;
   bool is_disable =
         (action == RETRO_SENSOR_ACCELEROMETER_DISABLE)
      || (action == RETRO_SENSOR_GYROSCOPE_DISABLE)
      || (action == RETRO_SENSOR_ILLUMINANCE_DISABLE);

   if (!input_driver_st.current_data)
      return false;
   /* If sensors are disabled, inhibit any enable
    * actions (but always allow disable actions) */
   if (!sensors_enable && !is_disable)
      return false;

   if (input_driver_st.primary_joypad && input_driver_st.primary_joypad->set_sensor_state)
      enabled = input_driver_st.primary_joypad->set_sensor_state(port, action, rate);

   /* An enable stops at the first driver that takes it, so a sensor
    * is only ever held by one of them. A disable has to reach both:
    * a joypad driver that reports the disable of a sensor it never
    * had as a success (as they are documented to) would otherwise
    * hide the disable from the input driver that is actually
    * holding the host sensor open, leaving it enabled and still
    * feeding the core after it asked for it to stop. */
   if (   (!enabled || is_disable)
       && (current_driver = input_driver_st.current_driver)
       &&  current_driver->set_sensor_state)
   {
      void *current_data = input_driver_st.current_data;
      enabled |= current_driver->set_sensor_state(current_data,
            port, action, rate);
   }
   return enabled;
}

/**************************************/

float input_driver_get_sensor(
         unsigned port, bool sensors_enable, unsigned id)
{
   if (!sensors_enable)
      return 0.0f;

   if (input_driver_st.primary_joypad && input_driver_st.primary_joypad->get_sensor_input)
   {
      float value;
      /* if joypad driver's get_sensor_input returns false, let input driver try */
      if (input_driver_st.primary_joypad->get_sensor_input(port, id, &value))
         return value;
   }
   if (input_driver_st.current_data)
   {
      const input_driver_t *input = input_driver_st.current_driver;
      if (input->get_sensor_input)
      {
         void *current_data = input_driver_st.current_data;
         return input->get_sensor_input(current_data, port, id);
      }
   }

   return 0.0f;
}

const input_device_driver_t *input_joypad_init_driver(
      const char *ident, void *data)
{
   if (ident && *ident)
   {
      int i;
      for (i = 0; joypad_drivers[i]; i++)
      {
         if (string_is_equal(ident, joypad_drivers[i]->ident)
               && joypad_drivers[i]->init)
         {
            void *ptr = joypad_drivers[i]->init(data);
            if (ptr)
            {
               RARCH_LOG("[Input] Found joypad driver: \"%s\".\n",
                     joypad_drivers[i]->ident);
               return joypad_drivers[i];
            }
         }
      }
   }
   /* Fall back to first available driver, skipping the configured
    * one that just failed above.
    *
    * Warn when this happens: from here on the active joypad driver is
    * not the configured one, which changes which pads are visible and
    * how they are named, and the only prior evidence was a "Found
    * joypad driver" line naming a driver the user never asked for.
    * On Windows in particular the first entry that initialises is
    * xinput, so a transient winraw/dinput init failure would silently
    * present as xinput with no indication why. */
   {
      const input_device_driver_t *fallback = input_joypad_init_first(data,
            (ident && *ident) ? ident : NULL);

      if (     ident
            && *ident
            && fallback
            && fallback->ident
            && !string_is_equal(ident, fallback->ident))
         RARCH_WARN("[Input] Configured joypad driver \"%s\" failed to "
               "initialise; falling back to \"%s\".\n",
               ident, fallback->ident);

      return fallback;
   }
}

static bool input_driver_button_combo_hold(
      unsigned mode,
      unsigned button,
      retro_time_t current_time,
      input_bits_t *p_input)
{
   rarch_timer_t *timer           = &input_driver_st.combo_timers[mode];
   runloop_state_t *runloop_st    = runloop_state_get_ptr();
   static bool enable_hotkey_dupe = false;

   /* Flag current press when button and 'enable_hotkey' are the same,
    * because 'input_hotkey_block_delay' clears the combo button bit. */
   if (     BIT256_GET_PTR(p_input, RARCH_ENABLE_HOTKEY)
         && BIT256_GET_PTR(p_input, button))
      enable_hotkey_dupe = true;

   /* Ignore press if 'enable_hotkey' is not the combo button. */
   if (      BIT256_GET_PTR(p_input, RARCH_ENABLE_HOTKEY)
         && !BIT256_GET_PTR(p_input, button)
         && !enable_hotkey_dupe)
      return false;

   /* Allow using the same button for 'enable_hotkey' if set,
    * and stop timer if holding fast-forward or slow-motion */
   if (     !BIT256_GET_PTR(p_input, button)
         && !( BIT256_GET_PTR(p_input, RARCH_ENABLE_HOTKEY)
            && !(input_driver_st.flags & INP_FLAG_BLOCK_HOTKEY)
            && !(runloop_st->flags & RUNLOOP_FLAG_SLOWMOTION)
            && !(runloop_st->flags & RUNLOOP_FLAG_FASTMOTION))
      )
   {
      /* Timer only runs while start is held down */
      enable_hotkey_dupe = false;
      timer->timer_begin = false;
      timer->timer_end   = true;
      timer->timeout_end = 0;
      return false;
   }

   /* User started holding down the start button, start the timer */
   if (!timer->timer_begin)
   {
      timer->timeout_us     = HOLD_BTN_DELAY_SEC * 1000000;
      timer->timeout_end    = current_time + timer->timeout_us;
      timer->timer_begin    = true;
      timer->timer_end      = false;
   }

   timer->current           = current_time;
   timer->timeout_us        = (timer->timeout_end - timer->current);

   if (!timer->timer_end && (timer->timeout_us <= 0))
   {
      /* Start has been held down long enough,
       * stop timer and enter menu */
      enable_hotkey_dupe = false;
      timer->timer_begin = false;
      timer->timer_end   = true;
      timer->timeout_end = 0;
      return true;
   }

   return false;
}

bool input_driver_pointer_is_offscreen(int16_t x, int16_t y)
{
   /* Use unsigned cast: values outside [-32700, 32700] will wrap
    * beyond 32700*2 = 65400 when biased by 32700 */
   return ((uint16_t)(x + 32700) > 65400u) || ((uint16_t)(y + 32700) > 65400u);
}

unsigned input_driver_lightgun_id_convert(unsigned id)
{
   switch (id)
   {
      case RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT:
         return RARCH_LIGHTGUN_DPAD_RIGHT;
      case RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT:
         return RARCH_LIGHTGUN_DPAD_LEFT;
      case RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP:
         return RARCH_LIGHTGUN_DPAD_UP;
      case RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN:
         return RARCH_LIGHTGUN_DPAD_DOWN;
      case RETRO_DEVICE_ID_LIGHTGUN_SELECT:
         return RARCH_LIGHTGUN_SELECT;
      case RETRO_DEVICE_ID_LIGHTGUN_PAUSE:
         return RARCH_LIGHTGUN_START;
      case RETRO_DEVICE_ID_LIGHTGUN_RELOAD:
         return RARCH_LIGHTGUN_RELOAD;
      case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
         return RARCH_LIGHTGUN_TRIGGER;
      case RETRO_DEVICE_ID_LIGHTGUN_AUX_A:
         return RARCH_LIGHTGUN_AUX_A;
      case RETRO_DEVICE_ID_LIGHTGUN_AUX_B:
         return RARCH_LIGHTGUN_AUX_B;
      case RETRO_DEVICE_ID_LIGHTGUN_AUX_C:
         return RARCH_LIGHTGUN_AUX_C;
      case RETRO_DEVICE_ID_LIGHTGUN_START:
         return RARCH_LIGHTGUN_START;
      default:
         break;
   }

   return 0;
}


bool input_driver_button_combo(
      unsigned mode,
      retro_time_t current_time,
      input_bits_t *p_input)
{
   switch (mode)
   {
      case INPUT_COMBO_DOWN_Y_L_R:
         if (   BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_DOWN)
             && BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_Y)
             && BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L)
             && BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R))
            return true;
         break;
      case INPUT_COMBO_L3_R3:
         if (   BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L3)
             && BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R3))
            return true;
         break;
      case INPUT_COMBO_L1_R1_START_SELECT:
         if (   BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L)
             && BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R)
             && BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_START)
             && BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
            return true;
         break;
      case INPUT_COMBO_START_SELECT:
         if (   BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_START)
             && BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
            return true;
         break;
      case INPUT_COMBO_L3_R:
         if (   BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L3)
             && BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R))
            return true;
         break;
      case INPUT_COMBO_L_R:
         if (   BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L)
             && BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R))
            return true;
         break;
      case INPUT_COMBO_DOWN_SELECT:
         if (   BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_DOWN)
             && BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
            return true;
         break;
      case INPUT_COMBO_L2_R2:
         if (   BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L2)
             && BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R2))
            return true;
         break;
      case INPUT_COMBO_HOLD_START:
         return input_driver_button_combo_hold(
               INPUT_COMBO_HOLD_START, RETRO_DEVICE_ID_JOYPAD_START, current_time, p_input);
      case INPUT_COMBO_HOLD_SELECT:
         return input_driver_button_combo_hold(
               INPUT_COMBO_HOLD_SELECT, RETRO_DEVICE_ID_JOYPAD_SELECT, current_time, p_input);
      default:
      case INPUT_COMBO_NONE:
         break;
   }

   return false;
}

static int32_t input_state_wrap(
      input_driver_t *input,
      void *data,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      rarch_joypad_info_t *joypad_info,
      const retro_keybind_set *binds,
      bool keyboard_mapping_blocked,
      unsigned _port,
      unsigned device,
      unsigned idx,
      unsigned id)
{
   int32_t ret = 0;

   if (!binds)
      return 0;

   /* Do a bitwise OR to combine input states together */

   if (device == RETRO_DEVICE_JOYPAD)
   {
      if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
      {
         if (joypad)
            ret |= joypad->state(joypad_info, binds[_port], _port);
         if (sec_joypad)
            ret |= sec_joypad->state(joypad_info, binds[_port], _port);
      }
      else if (id < RARCH_FIRST_CUSTOM_BIND)
      {
         /* Standard joypad buttons (0-15): serve from a per-port
          * bitmask cache when available.  The cache is populated
          * either by an earlier JOYPAD_MASK query for this port
          * (common in modern cores) or lazily on the first
          * individual button query (old-style cores).
          * This avoids up to 32 indirect joypad->button()/axis()
          * calls per port per frame. */
         input_driver_state_t *input_st = &input_driver_st;

         if (_port < MAX_USERS
               && !(input_st->frame_valid.joypad_cache & (1 << _port)))
         {
            int32_t cached = 0;
            if (joypad)
               cached |= joypad->state(joypad_info, binds[_port], _port);
            if (sec_joypad)
               cached |= sec_joypad->state(joypad_info, binds[_port], _port);
            if (input && input->input_state)
               cached |= input->input_state(
                     data, joypad, sec_joypad, joypad_info, binds,
                     keyboard_mapping_blocked,
                     _port, RETRO_DEVICE_JOYPAD, 0,
                     RETRO_DEVICE_ID_JOYPAD_MASK);
            input_st->joypad_state_cache[_port]       = cached;
            input_st->frame_valid.joypad_cache        |= (1 << _port);
         }

         if (    _port < MAX_USERS
              && (input_st->joypad_state_cache[_port] & (1 << id)))
            return 1;

         /* Cache says not pressed; skip input->input_state() below
          * since the cache already incorporates it. */
         return 0;
      }
      else
      {
         /* Extended bind IDs (turbo, hold, meta keys) are not
          * covered by joypad->state(), so use the original
          * per-button dispatch path. */
         if (RETRO_KEYBIND_VALID(&binds[_port][id]))
         {
            const uint64_t bind_joykey     = binds[_port][id].joykey;
            const uint64_t bind_joyaxis    = binds[_port][id].joyaxis;
            const uint64_t autobind_joykey = joypad_info->auto_binds[id].joykey;
            const uint64_t autobind_joyaxis= joypad_info->auto_binds[id].joyaxis;
            uint16_t port                  = joypad_info->joy_idx;
            float axis_threshold           = joypad_info->axis_threshold;
            float inv_0x8000               = INV_0x8000;
            const uint64_t joykey          = (bind_joykey != NO_BTN)
               ? bind_joykey  : autobind_joykey;
            const uint64_t joyaxis         = (bind_joyaxis != AXIS_NONE)
               ? bind_joyaxis : autobind_joyaxis;

            if (joypad)
            {
               if ((uint16_t)joykey != NO_BTN && joypad->button(
                        port, (uint16_t)joykey))
                  return 1;
               if (joyaxis != AXIS_NONE &&
                     ((float)abs(joypad->axis(port, (uint32_t)joyaxis))
                      * inv_0x8000) > axis_threshold)
                  return 1;
            }
            if (sec_joypad)
            {
               if ((uint16_t)joykey != NO_BTN && sec_joypad->button(
                        port, (uint16_t)joykey))
                  return 1;
               if (joyaxis != AXIS_NONE &&
                     ((float)abs(sec_joypad->axis(port, (uint32_t)joyaxis))
                      * inv_0x8000) > axis_threshold)
                  return 1;
            }
         }
      }
   }
   else if (device == RETRO_DEVICE_KEYBOARD)
   {
      /* Always ignore null key. */
      if (id == RETROK_UNKNOWN)
         return ret;
   }

   if (input && input->input_state)
      ret |= input->input_state(
            data,
            joypad,
            sec_joypad,
            joypad_info,
            binds,
            keyboard_mapping_blocked,
            _port,
            device,
            idx,
            id);

   /* Populate the per-port joypad cache from the MASK result so that
    * subsequent individual button queries (from hybrid or old-style
    * cores) can be served from cache without a second joypad->state()
    * call.  This is a no-op when the cache is already valid (i.e.
    * individual queries ran before the first MASK query). */
   if (    device == RETRO_DEVICE_JOYPAD
        && id     == RETRO_DEVICE_ID_JOYPAD_MASK
        && _port  <  MAX_USERS)
   {
      input_driver_state_t *input_st = &input_driver_st;
      if (!(input_st->frame_valid.joypad_cache & (1 << _port)))
      {
         input_st->joypad_state_cache[_port]       = ret;
         input_st->frame_valid.joypad_cache        |= (1 << _port);
      }
   }

   return ret;
}

static int16_t input_joypad_axis(
      float input_analog_deadzone,
      float input_analog_sensitivity,
      const input_device_driver_t *drv,
      unsigned port, uint32_t joyaxis, float normal_mag)
{
   int16_t val = ((joyaxis != AXIS_NONE) && drv && drv->axis)
      ? drv->axis(port, joyaxis) : 0;

   if (input_analog_deadzone)
   {
      /* If below deadzone, short-circuit immediately */
      if (normal_mag <= input_analog_deadzone)
         return 0;

      /* Radial/linear scaled deadzone rescale.
       * Precompute 1/normal_mag once; clamp implicitly via MIN/MAX. */
      {
         float inv_mag   = (normal_mag > 1.0f) ? (1.0f / normal_mag) : 1.0f;
         float dz_scale  = (normal_mag - input_analog_deadzone)
                         / (1.0f - input_analog_deadzone);
         if (dz_scale > 1.0f) dz_scale = 1.0f;
         val = (int16_t)((float)val * inv_mag * dz_scale);
      }
   }

   if (input_analog_sensitivity != 1.0f)
   {
      /* Fused: scale = sensitivity / 0x7fff; new_val = val * scale * 0x7fff
       * reduces to val * sensitivity, with clamp */
      int new_val = (int)((float)val * input_analog_sensitivity);
      if (new_val > 0x7fff)
         return 0x7fff;
      if (new_val < -0x7fff)
         return -0x7fff;
      return (int16_t)new_val;
   }

   return val;
}

/**
 * input_joypad_analog_button:
 * @drv                     : Input device driver handle.
 * @port                    : User number.
 * @idx                     : Analog key index.
 *                            E.g.:
 *                            - RETRO_DEVICE_INDEX_ANALOG_LEFT
 *                            - RETRO_DEVICE_INDEX_ANALOG_RIGHT
 * @ident                   : Analog key identifier.
 *                            E.g.:
 *                            - RETRO_DEVICE_ID_ANALOG_X
 *                            - RETRO_DEVICE_ID_ANALOG_Y
 * @binds                   : Binds of user.
 *
 * Gets analog value of analog key identifiers @idx and @ident
 * from user with number @port with provided keybinds (@binds).
 *
 * Returns: analog value on success, otherwise 0.
 **/
static int16_t input_joypad_analog_button(
      float input_analog_deadzone,
      float input_analog_sensitivity,
      const input_device_driver_t *drv,
      rarch_joypad_info_t *joypad_info,
      unsigned ident,
      const struct retro_keybind *bind)
{
   int16_t res      = 0;
   float normal_mag = 0.0f;
   uint16_t joy_idx = joypad_info->joy_idx;
   uint32_t axis    = (bind->joyaxis == AXIS_NONE)
      ? joypad_info->auto_binds[ident].joyaxis
      : bind->joyaxis;

   /* The joypad driver pointer can be NULL for the duration of a
    * driver teardown/reinit cycle - video_driver_free_internal()
    * clears primary_joypad before the joypad is recreated by
    * input_driver_init_joypads(). input_joypad_axis() already
    * guards against this, but the digital button fallback paths
    * below dereference drv directly. */
   if (!drv)
      return 0;

   /* Early exit for digital-only buttons: if neither the user bind
    * nor the autoconfig bind has an analog axis, this button has no
    * analog capability. Skip the input_joypad_axis() call and
    * deadzone magnitude computation entirely — go straight to the
    * digital button fallback. Saves one drv->axis() indirect call
    * plus float math per pressed digital button in the remap loop. */
   if (axis == AXIS_NONE)
   {
      uint16_t key = (bind->joykey == NO_BTN)
         ? joypad_info->auto_binds[ident].joykey
         : bind->joykey;

      if (drv->button(joy_idx, key))
         return 0x7fff;
      return 0;
   }

   /* a trigger learned to rest at the far end: its pull, from there */
   if (     (ident == RETRO_DEVICE_ID_JOYPAD_L2 || ident == RETRO_DEVICE_ID_JOYPAD_R2)
         && input_trigger_rests_far(joy_idx, axis)
         && config_get_ptr()->bools.input_trigger_full_range)
      return input_trigger_pull(drv, joy_idx, axis);

   /* Analog button - call drv->axis at most once */
   if (input_analog_deadzone)
   {
      int16_t mult = drv->axis(joy_idx, axis);
      if (mult != 0)
      {
         /* Manual abs avoids fabs() float-to-int rounding ambiguity */
         normal_mag = (float)(mult < 0 ? -mult : mult) * INV_0x7fff;
      }
   }

   /* If the result is zero, it's got a digital button attached to it instead */
   if ((res = abs(input_joypad_axis(input_analog_deadzone,
            input_analog_sensitivity, drv,
            joy_idx, axis, normal_mag))) == 0)
   {
      uint16_t key = (bind->joykey == NO_BTN)
         ? joypad_info->auto_binds[ident].joykey
         : bind->joykey;

      if (drv->button(joy_idx, key))
         return 0x7fff;
      return 0;
   }

   return res;
}

static int16_t input_joypad_analog_axis(
      unsigned input_analog_dpad_mode,
      float input_analog_deadzone,
      float input_analog_sensitivity,
      const input_device_driver_t *drv,
      rarch_joypad_info_t *joypad_info,
      unsigned idx,
      unsigned ident,
      const struct retro_keybind *binds)
{
   int16_t res                              = 0;
   /* Analog sticks. Either RETRO_DEVICE_INDEX_ANALOG_LEFT
    * or RETRO_DEVICE_INDEX_ANALOG_RIGHT */
   unsigned ident_minus                     = 0;
   unsigned ident_plus                      = 0;
   unsigned ident_x_minus                   = 0;
   unsigned ident_x_plus                    = 0;
   unsigned ident_y_minus                   = 0;
   unsigned ident_y_plus                    = 0;
   const struct retro_keybind *bind_minus   = NULL;
   const struct retro_keybind *bind_plus    = NULL;
   const struct retro_keybind *bind_x_minus = NULL;
   const struct retro_keybind *bind_x_plus  = NULL;
   const struct retro_keybind *bind_y_minus = NULL;
   const struct retro_keybind *bind_y_plus  = NULL;

   /* See input_joypad_analog_button() - drv is NULL while the
    * joypad driver is being torn down and reinitialised. */
   if (!drv)
      return 0;

   /* Skip analog input with analog_dpad_mode */
   switch (input_analog_dpad_mode)
   {
      case ANALOG_DPAD_LSTICK:
         if (idx == RETRO_DEVICE_INDEX_ANALOG_LEFT)
            return 0;
         break;
      case ANALOG_DPAD_RSTICK:
         if (idx == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
            return 0;
         break;
      case ANALOG_DPAD_LRSTICK:
      case ANALOG_DPAD_TWINSTICK:
         if (     idx == RETRO_DEVICE_INDEX_ANALOG_LEFT
               || idx == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
            return 0;
         break;
      default:
         break;
   }

   input_conv_analog_id_to_bind_id(idx, ident, ident_minus, ident_plus);

   bind_minus   = &binds[ident_minus];
   bind_plus    = &binds[ident_plus];

   if (!RETRO_KEYBIND_VALID(bind_minus) || !RETRO_KEYBIND_VALID(bind_plus))
      return 0;

   input_conv_analog_id_to_bind_id(idx,
         RETRO_DEVICE_ID_ANALOG_X, ident_x_minus, ident_x_plus);

   bind_x_minus = &binds[ident_x_minus];
   bind_x_plus  = &binds[ident_x_plus];

   if (!RETRO_KEYBIND_VALID(bind_x_minus) || !RETRO_KEYBIND_VALID(bind_x_plus))
      return 0;

   input_conv_analog_id_to_bind_id(idx,
         RETRO_DEVICE_ID_ANALOG_Y, ident_y_minus, ident_y_plus);

   bind_y_minus = &binds[ident_y_minus];
   bind_y_plus  = &binds[ident_y_plus];

   if (!RETRO_KEYBIND_VALID(bind_y_minus) || !RETRO_KEYBIND_VALID(bind_y_plus))
      return 0;

   /* Keyboard bind priority */
   if (     RETRO_KEYBIND_KEY(bind_plus)  != RETROK_UNKNOWN
         || RETRO_KEYBIND_KEY(bind_minus) != RETROK_UNKNOWN)
   {
      input_driver_state_t *input_st = &input_driver_st;

      if (RETRO_KEYBIND_KEY(bind_plus) && input_state_wrap(
            input_st->current_driver,
            input_st->current_data,
            input_st->primary_joypad,
            NULL,
            joypad_info,
            (*input_st->libretro_input_binds),
            !!(input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED),
            0, RETRO_DEVICE_KEYBOARD, 0,
            RETRO_KEYBIND_KEY(bind_plus)))
         res  = 0x7fff;
      if (RETRO_KEYBIND_KEY(bind_minus) && input_state_wrap(
            input_st->current_driver,
            input_st->current_data,
            input_st->primary_joypad,
            NULL,
            joypad_info,
            (*input_st->libretro_input_binds),
            !!(input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED),
            0, RETRO_DEVICE_KEYBOARD, 0,
            RETRO_KEYBIND_KEY(bind_minus)))
         res += -0x7fff;

      if (res)
         return res;
   }

   {
      uint32_t axis_minus         = (bind_minus->joyaxis   == AXIS_NONE)
         ? joypad_info->auto_binds[ident_minus].joyaxis
         : bind_minus->joyaxis;
      uint32_t axis_plus          = (bind_plus->joyaxis    == AXIS_NONE)
         ? joypad_info->auto_binds[ident_plus].joyaxis
         : bind_plus->joyaxis;
      float normal_mag            = 0.0f;

      /* normalized magnitude of stick actuation, needed for scaled
       * radial deadzone */
      if (input_analog_deadzone)
      {
         float x                  = 0.0f;
         float y                  = 0.0f;
         float mag_sq;
         uint32_t x_axis_minus    = (bind_x_minus->joyaxis == AXIS_NONE)
            ? joypad_info->auto_binds[ident_x_minus].joyaxis
            : bind_x_minus->joyaxis;
         uint32_t x_axis_plus     = (bind_x_plus->joyaxis  == AXIS_NONE)
            ? joypad_info->auto_binds[ident_x_plus].joyaxis
            : bind_x_plus->joyaxis;
         uint32_t y_axis_minus    = (bind_y_minus->joyaxis == AXIS_NONE)
            ? joypad_info->auto_binds[ident_y_minus].joyaxis
            : bind_y_minus->joyaxis;
         uint32_t y_axis_plus     = (bind_y_plus->joyaxis  == AXIS_NONE)
            ? joypad_info->auto_binds[ident_y_plus].joyaxis
            : bind_y_plus->joyaxis;
         /* normalized magnitude for radial scaled analog deadzone */
         if (x_axis_plus != AXIS_NONE && drv->axis)
            x                     = drv->axis(
                  joypad_info->joy_idx, x_axis_plus);
         if (x_axis_minus != AXIS_NONE && drv->axis)
            x                    += drv->axis(joypad_info->joy_idx,
                  x_axis_minus);
         if (y_axis_plus != AXIS_NONE && drv->axis)
            y                     = drv->axis(
                  joypad_info->joy_idx, y_axis_plus);
         if (y_axis_minus != AXIS_NONE && drv->axis)
            y                    += drv->axis(
                  joypad_info->joy_idx, y_axis_minus);

         /* Use squared magnitude to avoid sqrtf when possible.
          * dz_sq is the squared deadzone threshold in raw axis units.
          * If mag_sq <= dz_sq, the stick is inside the deadzone
          * and input_joypad_axis() will return 0 without needing
          * the actual magnitude. Only compute sqrtf when we're in
          * the rescaling region (between deadzone and full tilt). */
         mag_sq = x * x + y * y;
         {
            float dz_raw = input_analog_deadzone * 0x7fff;
            float dz_sq  = dz_raw * dz_raw;
            if (mag_sq <= dz_sq)
               normal_mag = 0.0f; /* Will trigger early-exit in input_joypad_axis */
            else
               normal_mag = INV_0x7fff * sqrtf(mag_sq);
         }
      }

      res           = abs(
            input_joypad_axis(
               input_analog_deadzone,
               input_analog_sensitivity,
               drv, joypad_info->joy_idx,
               axis_plus, normal_mag));
      res          -= abs(
            input_joypad_axis(
               input_analog_deadzone,
               input_analog_sensitivity,
               drv, joypad_info->joy_idx,
               axis_minus, normal_mag));
   }

   if (res == 0)
   {
      uint16_t key_minus    = (bind_minus->joykey == NO_BTN)
         ? joypad_info->auto_binds[ident_minus].joykey
         : bind_minus->joykey;
      uint16_t key_plus     = (bind_plus->joykey  == NO_BTN)
         ? joypad_info->auto_binds[ident_plus].joykey
         : bind_plus->joykey;
      if (drv->button && drv->button(joypad_info->joy_idx, key_plus))
         res  = 0x7fff;
      if (drv->button && drv->button(joypad_info->joy_idx, key_minus))
         res += -0x7fff;
   }

   return res;
}

/**
 * input_joypad_analog_stick:
 *
 * Processes both X and Y axes of a single analog stick together,
 * computing the radial deadzone magnitude only once. This avoids
 * the redundant drv->axis() calls and sqrtf() that occur when
 * input_joypad_analog_axis() is called separately for X and Y.
 *
 * @param input_analog_dpad_mode Analog-to-dpad mode for this port
 * @param input_analog_deadzone  Deadzone threshold (0.0 = disabled)
 * @param input_analog_sensitivity Sensitivity multiplier
 * @param drv                    Joypad driver
 * @param joypad_info            Joypad info (joy_idx, auto_binds, threshold)
 * @param idx                    Stick index (ANALOG_LEFT or ANALOG_RIGHT)
 * @param binds                  Keybinds for this port
 * @param out_x                  Output: X axis value
 * @param out_y                  Output: Y axis value
 *
 * @return true if the stick was processed, false if skipped (dpad mode)
 */
INPUT_NOINLINE static bool input_joypad_analog_stick(
      unsigned input_analog_dpad_mode,
      float input_analog_deadzone,
      float input_analog_sensitivity,
      const input_device_driver_t *drv,
      rarch_joypad_info_t *joypad_info,
      unsigned idx,
      const struct retro_keybind *binds,
      int16_t *out_x, int16_t *out_y)
{
   unsigned ident_x_minus          = 0;
   unsigned ident_x_plus           = 0;
   unsigned ident_y_minus          = 0;
   unsigned ident_y_plus           = 0;
   const struct retro_keybind *bind_x_minus = NULL;
   const struct retro_keybind *bind_x_plus  = NULL;
   const struct retro_keybind *bind_y_minus = NULL;
   const struct retro_keybind *bind_y_plus  = NULL;
   float normal_mag                = 0.0f;

   *out_x = 0;
   *out_y = 0;

   /* See input_joypad_analog_button() - drv is NULL while the
    * joypad driver is being torn down and reinitialised. */
   if (!drv)
      return false;

   /* Skip analog input with analog_dpad_mode */
   switch (input_analog_dpad_mode)
   {
      case ANALOG_DPAD_LSTICK:
         if (idx == RETRO_DEVICE_INDEX_ANALOG_LEFT)
            return false;
         break;
      case ANALOG_DPAD_RSTICK:
         if (idx == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
            return false;
         break;
      case ANALOG_DPAD_LRSTICK:
      case ANALOG_DPAD_TWINSTICK:
         if (     idx == RETRO_DEVICE_INDEX_ANALOG_LEFT
               || idx == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
            return false;
         break;
      default:
         break;
   }

   /* Resolve all 4 bind IDs for this stick at once */
   input_conv_analog_id_to_bind_id(idx,
         RETRO_DEVICE_ID_ANALOG_X, ident_x_minus, ident_x_plus);
   input_conv_analog_id_to_bind_id(idx,
         RETRO_DEVICE_ID_ANALOG_Y, ident_y_minus, ident_y_plus);

   bind_x_minus = &binds[ident_x_minus];
   bind_x_plus  = &binds[ident_x_plus];
   bind_y_minus = &binds[ident_y_minus];
   bind_y_plus  = &binds[ident_y_plus];

   if (   !RETRO_KEYBIND_VALID(bind_x_minus) || !RETRO_KEYBIND_VALID(bind_x_plus)
       || !RETRO_KEYBIND_VALID(bind_y_minus) || !RETRO_KEYBIND_VALID(bind_y_plus))
      return false;

   /* Keyboard bind priority — check X */
   if (     RETRO_KEYBIND_KEY(bind_x_plus)  != RETROK_UNKNOWN
         || RETRO_KEYBIND_KEY(bind_x_minus) != RETROK_UNKNOWN)
   {
      input_driver_state_t *input_st = &input_driver_st;
      bool kb_blocked = !!(input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED);

      if (RETRO_KEYBIND_KEY(bind_x_plus) && input_state_wrap(
            input_st->current_driver,
            input_st->current_data,
            input_st->primary_joypad,
            NULL, joypad_info,
            (*input_st->libretro_input_binds),
            kb_blocked,
            0, RETRO_DEVICE_KEYBOARD, 0,
            RETRO_KEYBIND_KEY(bind_x_plus)))
         *out_x  = 0x7fff;
      if (RETRO_KEYBIND_KEY(bind_x_minus) && input_state_wrap(
            input_st->current_driver,
            input_st->current_data,
            input_st->primary_joypad,
            NULL, joypad_info,
            (*input_st->libretro_input_binds),
            kb_blocked,
            0, RETRO_DEVICE_KEYBOARD, 0,
            RETRO_KEYBIND_KEY(bind_x_minus)))
         *out_x += -0x7fff;
   }

   /* Keyboard bind priority — check Y */
   if (     RETRO_KEYBIND_KEY(bind_y_plus)  != RETROK_UNKNOWN
         || RETRO_KEYBIND_KEY(bind_y_minus) != RETROK_UNKNOWN)
   {
      input_driver_state_t *input_st = &input_driver_st;
      bool kb_blocked = !!(input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED);

      if (RETRO_KEYBIND_KEY(bind_y_plus) && input_state_wrap(
            input_st->current_driver,
            input_st->current_data,
            input_st->primary_joypad,
            NULL, joypad_info,
            (*input_st->libretro_input_binds),
            kb_blocked,
            0, RETRO_DEVICE_KEYBOARD, 0,
            RETRO_KEYBIND_KEY(bind_y_plus)))
         *out_y  = 0x7fff;
      if (RETRO_KEYBIND_KEY(bind_y_minus) && input_state_wrap(
            input_st->current_driver,
            input_st->current_data,
            input_st->primary_joypad,
            NULL, joypad_info,
            (*input_st->libretro_input_binds),
            kb_blocked,
            0, RETRO_DEVICE_KEYBOARD, 0,
            RETRO_KEYBIND_KEY(bind_y_minus)))
         *out_y += -0x7fff;
   }

   /* If keyboard produced results for both axes, we're done */
   if (*out_x && *out_y)
      return true;

   {
      uint32_t x_axis_minus = (bind_x_minus->joyaxis == AXIS_NONE)
         ? joypad_info->auto_binds[ident_x_minus].joyaxis
         : bind_x_minus->joyaxis;
      uint32_t x_axis_plus  = (bind_x_plus->joyaxis  == AXIS_NONE)
         ? joypad_info->auto_binds[ident_x_plus].joyaxis
         : bind_x_plus->joyaxis;
      uint32_t y_axis_minus = (bind_y_minus->joyaxis == AXIS_NONE)
         ? joypad_info->auto_binds[ident_y_minus].joyaxis
         : bind_y_minus->joyaxis;
      uint32_t y_axis_plus  = (bind_y_plus->joyaxis  == AXIS_NONE)
         ? joypad_info->auto_binds[ident_y_plus].joyaxis
         : bind_y_plus->joyaxis;

      /* Compute radial magnitude ONCE for this stick */
      if (input_analog_deadzone)
      {
         float x    = 0.0f;
         float y    = 0.0f;
         float mag_sq;
         if (x_axis_plus != AXIS_NONE && drv->axis)
            x  = drv->axis(joypad_info->joy_idx, x_axis_plus);
         if (x_axis_minus != AXIS_NONE && drv->axis)
            x += drv->axis(joypad_info->joy_idx, x_axis_minus);
         if (y_axis_plus != AXIS_NONE && drv->axis)
            y  = drv->axis(joypad_info->joy_idx, y_axis_plus);
         if (y_axis_minus != AXIS_NONE && drv->axis)
            y += drv->axis(joypad_info->joy_idx, y_axis_minus);

         mag_sq = x * x + y * y;
         {
            float dz_raw = input_analog_deadzone * 0x7fff;
            float dz_sq  = dz_raw * dz_raw;
            if (mag_sq <= dz_sq)
               normal_mag = 0.0f;
            else
               normal_mag = INV_0x7fff * sqrtf(mag_sq);
         }
      }

      /* X axis — reuse cached normal_mag */
      if (!*out_x)
      {
         int16_t x_val;
         x_val  = abs(input_joypad_axis(
               input_analog_deadzone, input_analog_sensitivity,
               drv, joypad_info->joy_idx,
               x_axis_plus, normal_mag));
         x_val -= abs(input_joypad_axis(
               input_analog_deadzone, input_analog_sensitivity,
               drv, joypad_info->joy_idx,
               x_axis_minus, normal_mag));

         if (x_val == 0)
         {
            uint16_t key_minus = (bind_x_minus->joykey == NO_BTN)
               ? joypad_info->auto_binds[ident_x_minus].joykey
               : bind_x_minus->joykey;
            uint16_t key_plus  = (bind_x_plus->joykey  == NO_BTN)
               ? joypad_info->auto_binds[ident_x_plus].joykey
               : bind_x_plus->joykey;
            if (drv->button && drv->button(joypad_info->joy_idx, key_plus))
               x_val  = 0x7fff;
            if (drv->button && drv->button(joypad_info->joy_idx, key_minus))
               x_val += -0x7fff;
         }
         *out_x = x_val;
      }

      /* Y axis — reuse same cached normal_mag */
      if (!*out_y)
      {
         int16_t y_val;
         y_val  = abs(input_joypad_axis(
               input_analog_deadzone, input_analog_sensitivity,
               drv, joypad_info->joy_idx,
               y_axis_plus, normal_mag));
         y_val -= abs(input_joypad_axis(
               input_analog_deadzone, input_analog_sensitivity,
               drv, joypad_info->joy_idx,
               y_axis_minus, normal_mag));

         if (y_val == 0)
         {
            uint16_t key_minus = (bind_y_minus->joykey == NO_BTN)
               ? joypad_info->auto_binds[ident_y_minus].joykey
               : bind_y_minus->joykey;
            uint16_t key_plus  = (bind_y_plus->joykey  == NO_BTN)
               ? joypad_info->auto_binds[ident_y_plus].joykey
               : bind_y_plus->joykey;
            if (drv->button && drv->button(joypad_info->joy_idx, key_plus))
               y_val  = 0x7fff;
            if (drv->button && drv->button(joypad_info->joy_idx, key_minus))
               y_val += -0x7fff;
         }
         *out_y = y_val;
      }
   }

   return true;
}

void input_keyboard_line_append(struct input_keyboard_line *kb_line,
      const char *word, size_t len)
{
   /* Need room for current content + inserted text + NUL terminator.
    * new_size is the string length after insertion (not counting NUL). */
   size_t new_size     = kb_line->size + len;

   /* Overflow guard */
   if (new_size < kb_line->size || new_size < len)
      return;

   /* Grow buffer with exponential strategy if needed */
   if (new_size > kb_line->capacity)
   {
      /* At least double the current capacity, with a minimum of 64 */
      size_t new_cap = kb_line->capacity ? kb_line->capacity * 2 : 64;
      char *newbuf;

      while (new_cap < new_size)
         new_cap *= 2;

      /* +1 for NUL terminator */
      newbuf = (char*)realloc(kb_line->buffer, new_cap + 1);
      if (!newbuf)
         return;

      kb_line->buffer   = newbuf;
      kb_line->capacity = new_cap;
   }

   /* Shift existing content after insertion point to make room */
   memmove(
         kb_line->buffer + kb_line->ptr + len,
         kb_line->buffer + kb_line->ptr,
         kb_line->size - kb_line->ptr);

   /* Insert new text at cursor */
   memcpy(kb_line->buffer + kb_line->ptr, word, len);
   kb_line->ptr  += len;
   kb_line->size  = new_size;

   kb_line->buffer[kb_line->size] = '\0';
}

void input_keyboard_line_clear(input_driver_state_t *input_st)
{
   if (input_st->keyboard_line.buffer)
      free(input_st->keyboard_line.buffer);
   input_st->keyboard_line.buffer       = NULL;
   input_st->keyboard_line.ptr          = 0;
   input_st->keyboard_line.size         = 0;
   input_st->keyboard_line.capacity     = 0;
}

/* A line of text has been opened, or closed. This is the one place the
 * platform is told, whoever opened it and whatever types into it.
 *
 * Windows takes the IME away from its window except while a line is
 * open (win32_text_entry()). The other platforms still find out their
 * own ways - Android and iOS are called from the menu, SDL3 and the
 * Vita look every poll - and are to be told from here as well. */
static void input_text_entry_changed(bool active)
{
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
   win32_text_entry(active);
#else
   (void)active;
#endif
}

void input_keyboard_line_free(input_driver_state_t *input_st)
{
   input_keyboard_line_t *kb_line = (input_keyboard_line_t*)&input_st->keyboard_line;
   if (kb_line->buffer)
      free(kb_line->buffer);
   kb_line->buffer             = NULL;
   kb_line->ptr                =  0;
   kb_line->size               = 0;
   kb_line->capacity           = 0;
   kb_line->cb                 = NULL;
   kb_line->userdata           = NULL;
   kb_line->enabled            = false;
   input_st->osk_textbox_focus = false;
   input_text_entry_changed(false);
}

const char **input_keyboard_start_line(
      void *userdata,
      struct input_keyboard_line *kb_line,
      input_keyboard_line_complete_t cb)
{
   kb_line->buffer                   = NULL;
   kb_line->ptr                      = 0;
   kb_line->size                     = 0;
   kb_line->capacity                 = 0;
   kb_line->cb                       = cb;
   kb_line->userdata                 = userdata;
   kb_line->enabled                  = true;
   input_driver_st.osk_textbox_focus = false;
   input_text_entry_changed(true);

   return (const char**)&kb_line->buffer;
}

#ifdef HAVE_OVERLAY
static int16_t input_overlay_device_mouse_state(
      input_overlay_t *ol, unsigned id)
{
   int16_t res;
   input_overlay_pointer_state_t *ptr_st = &ol->pointer_state;

   switch(id)
   {
      case RETRO_DEVICE_ID_MOUSE_X:
         ptr_st->device_mask |= (1 << RETRO_DEVICE_MOUSE);
         res =   (ptr_st->mouse.scale_x)
               * (ptr_st->screen_x - ptr_st->mouse.prev_screen_x);
         return res;
      case RETRO_DEVICE_ID_MOUSE_Y:
         res =   (ptr_st->mouse.scale_y)
               * (ptr_st->screen_y - ptr_st->mouse.prev_screen_y);
         return res;
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         return    (ptr_st->mouse.click & 0x1)
                || (ptr_st->mouse.hold  & 0x1);
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         return    (ptr_st->mouse.click & 0x2)
                || (ptr_st->mouse.hold  & 0x2);
      case RETRO_DEVICE_ID_MOUSE_MIDDLE:
         return    (ptr_st->mouse.click & 0x4)
                || (ptr_st->mouse.hold  & 0x4);
      default:
         break;
   }

   return 0;
}

static int16_t input_overlay_lightgun_state(
      bool input_overlay_lightgun_allow_offscreen,
      input_overlay_t *ol, unsigned id)
{
   unsigned rarch_id;
   input_overlay_pointer_state_t *ptr_st = &ol->pointer_state;

   switch(id)
   {
      /* Pointer positions have been clamped earlier in input drivers,   *
       * so if we want to pass true offscreen value, it must be detected */
      case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
         ptr_st->device_mask |= (1 << RETRO_DEVICE_LIGHTGUN);
         if (   ( ptr_st->ptr[0].x > -0x7fff && ptr_st->ptr[0].x != 0x7fff)
               || !input_overlay_lightgun_allow_offscreen)
            return ptr_st->ptr[0].x;
         return -0x8000;
      case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
         if (   ( ptr_st->ptr[0].y > -0x7fff && ptr_st->ptr[0].y != 0x7fff)
               || !input_overlay_lightgun_allow_offscreen)
            return ptr_st->ptr[0].y;
         return -0x8000;
      case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
         ptr_st->device_mask |= (1 << RETRO_DEVICE_LIGHTGUN);
         return ( input_overlay_lightgun_allow_offscreen
               && input_driver_pointer_is_offscreen(ptr_st->ptr[0].x, ptr_st->ptr[0].y));
      case RETRO_DEVICE_ID_LIGHTGUN_AUX_A:
      case RETRO_DEVICE_ID_LIGHTGUN_AUX_B:
      case RETRO_DEVICE_ID_LIGHTGUN_AUX_C:
      case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
      case RETRO_DEVICE_ID_LIGHTGUN_START:
      case RETRO_DEVICE_ID_LIGHTGUN_PAUSE:
      case RETRO_DEVICE_ID_LIGHTGUN_SELECT:
      case RETRO_DEVICE_ID_LIGHTGUN_RELOAD:
      case RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP:
      case RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN:
      case RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT:
      case RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT:
         rarch_id = input_driver_lightgun_id_convert(id);
         break;
      default:
         rarch_id = RARCH_BIND_LIST_END;
         break;
   }

   return (   rarch_id < RARCH_BIND_LIST_END
           && (   ptr_st->lightgun.multitouch_id == rarch_id
               || BIT256_GET(ol->overlay_state.buttons, rarch_id)));
}

static int16_t input_overlay_pointer_state(input_overlay_t *ol,
      input_overlay_pointer_state_t *ptr_st,
      unsigned idx, unsigned id)
{
   ptr_st->device_mask |= (1 << RETRO_DEVICE_POINTER);

   switch (id)
   {
      case RETRO_DEVICE_ID_POINTER_X:
         return ptr_st->ptr[idx].x;
      case RETRO_DEVICE_ID_POINTER_Y:
         return ptr_st->ptr[idx].y;
      case RETRO_DEVICE_ID_POINTER_PRESSED:
         return (idx < ptr_st->count)
               && ptr_st->ptr[idx].x != -0x8000
               && ptr_st->ptr[idx].y != -0x8000;
      case RETRO_DEVICE_ID_POINTER_COUNT:
         return ptr_st->count;
      case RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN:
         return input_driver_pointer_is_offscreen(ptr_st->ptr[idx].x, ptr_st->ptr[idx].y);
   }

   return 0;
}

static int16_t input_overlay_pointing_device_state(
      int input_overlay_lightgun_port,
      bool input_overlay_lightgun_allow_offscreen,
      input_overlay_t *ol, unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   switch (device)
   {
      case RETRO_DEVICE_MOUSE:
         return input_overlay_device_mouse_state(ol, id);
      case RETRO_DEVICE_LIGHTGUN:
         if (     input_overlay_lightgun_port == -1
               || input_overlay_lightgun_port == (int)port)
            return input_overlay_lightgun_state(
                  input_overlay_lightgun_allow_offscreen,
                  ol, id);
         break;
      case RETRO_DEVICE_POINTER:
         return input_overlay_pointer_state(ol,
               (input_overlay_pointer_state_t*)&ol->pointer_state,
               idx, id);
      default:
         break;
   }

   return 0;
}
#endif

#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
static bool input_remote_init_network(input_remote_t *handle,
      uint16_t port, unsigned user)
{
   int fd;
   struct addrinfo *res  = NULL;
   port                  = port + user;

   if (!network_init())
      return false;

   RARCH_LOG("[Network] Bringing up remote interface on port %hu.\n",
         (unsigned short)port);

   if ((fd = socket_init((void**)&res, port, NULL, SOCKET_TYPE_DATAGRAM, AF_INET)) >= 0)
   {
      handle->net_fd[user] = fd;
      handle->port[user]   = port;

      if (socket_nonblock(handle->net_fd[user]))
      {
         if (socket_bind(handle->net_fd[user], res))
         {
            freeaddrinfo_retro(res);
            return true;
         }
         RARCH_ERR("%s\n", msg_hash_to_str(MSG_FAILED_TO_BIND_SOCKET));
      }
   }

   if (res)
      freeaddrinfo_retro(res);
   return false;
}

void input_remote_free(input_remote_t *handle, unsigned max_users)
{
   int user;
   for (user = 0; user < (int)max_users; user ++)
      socket_close(handle->net_fd[user]);
   free(handle);
}

static input_remote_t *input_remote_new(
      settings_t *settings,
      uint16_t port, unsigned max_users)
{
   int user;
   input_remote_t      *handle = (input_remote_t*)
      calloc(1, sizeof(*handle));

   if (!handle)
      return NULL;

   for (user = 0; user < (int)max_users; user++)
   {
      handle->net_fd[user] = -1;
      if (settings->bools.network_remote_enable_user[user])
         if (!input_remote_init_network(handle, port, user))
         {
            input_remote_free(handle, max_users);
            return NULL;
         }
   }

   return handle;
}

static void input_remote_parse_packet(
      input_remote_state_t *input_state,
      struct remote_message *msg, unsigned user)
{
   /* Parse message */
   switch (msg->device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (msg->id < 16)
         {
            input_state->buttons[user] &= ~(1 << msg->id);
            if (msg->state)
               input_state->buttons[user] |= 1 << msg->id;
         }
         break;
      case RETRO_DEVICE_ANALOG:
         if (msg->id<2 && msg->index<2)
            input_state->analog[msg->index * 2 + msg->id][user] = msg->state;
         break;
   }
}

input_remote_t *input_driver_init_remote(
      settings_t *settings,
      unsigned num_active_users)
{
   return input_remote_new(settings,
         settings->uints.network_remote_base_port,
         num_active_users);
}

bool input_remote_info(unsigned user, unsigned *port,
      uint32_t *address, bool *heard)
{
   input_remote_t *remote = input_driver_st.remote;

   if (!remote || user >= MAX_USERS || !remote->port[user])
      return false;
   *port    = remote->port[user];
   *address = ntohl(remote->last[user]);
   *heard   = remote->heard[user];
   return true;
}

/* A sender sends one message for each control that changed. All that
 * is queued is taken, so a frame's changes arrive together and none
 * waits a poll for each one before it; the count is bounded, so a
 * flood cannot hold the poll. */
#define INPUT_REMOTE_DRAIN_MAX 64

static void input_remote_poll(input_driver_state_t *input_st,
      settings_t *settings, unsigned max_users)
{
   unsigned user, n;
   bool first_sender = settings->bools.network_remote_first_sender;

   for (user = 0; user < max_users; user++)
   {
      int fd;

      if (!settings->bools.network_remote_enable_user[user])
         continue;
#if defined(_WIN32)
      if (input_st->remote->net_fd[user] == INVALID_SOCKET)
#else
      if (input_st->remote->net_fd[user] < 0)
#endif
         continue;
      fd = (int)input_st->remote->net_fd[user];

      for (n = 0; n < INPUT_REMOTE_DRAIN_MAX; n++)
      {
         struct remote_message msg;
         struct sockaddr_in from;
         socklen_t from_len = sizeof(from);
         ssize_t ret        = recvfrom(fd, (char*)&msg, sizeof(msg), 0,
               (struct sockaddr*)&from, &from_len);

         if (ret == (ssize_t)sizeof(msg))
         {
            if (first_sender)
            {
               input_remote_t *remote = input_st->remote;
               uint32_t addr          = from.sin_addr.s_addr;
               if (!remote->sender_known[user])
               {
                  uint32_t a                 = ntohl(addr);
                  remote->sender[user]       = addr;
                  remote->sender_known[user] = true;
                  RARCH_LOG("[Network] User %u's Network RetroPad listens"
                        " to %u.%u.%u.%u only.\n", user + 1,
                        (unsigned)(a >> 24), (unsigned)((a >> 16) & 0xff),
                        (unsigned)((a >> 8) & 0xff), (unsigned)(a & 0xff));
               }
               else if (remote->sender[user] != addr)
                  continue;
            }
            input_st->remote->last[user]  = from.sin_addr.s_addr;
            input_st->remote->heard[user] = true;
            input_remote_parse_packet(&input_st->remote_st_ptr, &msg, user);
         }
         else if (ret < 0 && isagain((int)ret))
            break; /* nothing more queued */
         else if (ret < 0)
         {
            /* the socket failed: nothing stays held */
            input_remote_state_t *st = &input_st->remote_st_ptr;
            st->buttons[user]        = 0;
            st->analog[0][user]      = 0;
            st->analog[1][user]      = 0;
            st->analog[2][user]      = 0;
            st->analog[3][user]      = 0;
            break;
         }
         /* a message of any other size is not one of ours */
      }
   }
}
#endif

static int16_t input_state_device(
      input_driver_state_t *input_st,
      settings_t *settings,
      input_mapper_t *handle,
      unsigned input_analog_dpad_mode,
      int32_t ret,
      unsigned port, unsigned device,
      unsigned idx, unsigned id,
      bool button_mask)
{
   int16_t res  = 0;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:

         if (id < RARCH_FIRST_META_KEY)
         {
#ifdef HAVE_NETWORKGAMEPAD
            /* Don't process binds if input is coming from Remote RetroPad */
            if (     input_st->remote
                  && INPUT_REMOTE_KEY_PRESSED(input_st, id, port))
               res |= 1;
            else
#endif
            {
               bool bind_valid       = input_st->libretro_input_binds[port]
                  && RETRO_KEYBIND_VALID(&(*input_st->libretro_input_binds[port])[id]);
               unsigned remap_button = settings->uints.input_remap_ids[port][id];

               /* TODO/FIXME: What on earth is this code doing...? */
               if (!(bind_valid && (id != remap_button)))
               {
                  if (button_mask)
                  {
                     if (ret & (1 << id))
                        res |= (1 << id);
                  }
                  else
                     res = ret;
               }

               if (BIT256_GET(handle->buttons[port], id))
                  res = 1;

#ifdef HAVE_OVERLAY
               /* Check if overlay is active and button
                * corresponding to 'id' has been pressed */
               if (  (port == 0)
                   && input_st->overlay_ptr
                   && (input_st->overlay_ptr->flags & INPUT_OVERLAY_ALIVE)
                   && BIT256_GET(input_st->overlay_ptr->overlay_state.buttons, id))
               {
#ifdef HAVE_MENU
                  bool menu_driver_alive        = (menu_state_get_ptr()->flags &
                        MENU_ST_FLAG_ALIVE) ? true : false;
#else
                  bool menu_driver_alive        = false;
#endif
                  bool input_remap_binds_enable = settings->bools.input_remap_binds_enable;

                  /* This button has already been processed
                   * inside input_driver_poll() if all the
                   * following are true:
                   * > Menu driver is not running
                   * > Input remaps are enabled
                   * > 'id' is not equal to remapped button index
                   * If these conditions are met, input here
                   * is ignored */
                  if (   (menu_driver_alive
                      || !input_remap_binds_enable)
                      || (id == remap_button))
                     res |= 1;
               }
#endif
            }

            if (id <= RETRO_DEVICE_ID_JOYPAD_R3)
            {
               uint8_t turbo_period     = settings->uints.input_turbo_period;
               uint8_t turbo_duty_cycle = settings->uints.input_turbo_duty_cycle;
               uint8_t turbo_mode       = settings->uints.input_turbo_mode;

               /* Apply hold button logic.
                * When hold modifier is pressed, tapping buttons toggles their held state.
                * Held buttons report as pressed even when not physically touched.
                * Physical presses pass through normally (no modification). */

               /* Handle hold modifier state and toggle logic */
               if (!input_st->hold_btns.frame_enable[port])
               {
                  /* Hold modifier not pressed - clear edge detection state */
                  input_st->hold_btns.hold_pressed[port] = 0;
               }
               else
               {
                  /* Hold modifier is pressed - handle toggle on rising edge */
                  if (!res)
                     input_st->hold_btns.hold_pressed[port] &= ~(1 << id);
                  else if (!(input_st->hold_btns.hold_pressed[port] & (1 << id)))
                  {
                     /* Rising edge - toggle hold for this button */
                     input_st->hold_btns.hold_pressed[port] |= (1 << id);
                     input_st->hold_btns.enable[port] ^= (1 << id);
                  }
               }

               /* Apply hold effect: if button is held and not physically pressed */
               if (!res && (input_st->hold_btns.enable[port] & (1 << id)))
                  res = 1;

               /* Apply turbo button if activated. */

               /* Don't allow classic mode turbo for D-pad unless explicitly allowed. */
               if (     turbo_mode <= INPUT_TURBO_MODE_CLASSIC_TOGGLE
                     && !settings->bools.input_turbo_allow_dpad
                     && id >= RETRO_DEVICE_ID_JOYPAD_UP
                     && id <= RETRO_DEVICE_ID_JOYPAD_RIGHT)
                  break;

               if (turbo_duty_cycle == 0)
                  turbo_duty_cycle = turbo_period / 2;

               /* Clear underlying button to prevent duplicates. */
               if (input_st->turbo_btns.frame_enable[port])
               {
                  /* The empty Turbo Bind uses the port's turbo hotkey,
                   * which has no RetroPad button to clear. */
                  int turbo_bind = settings->ints.input_turbo_bind;

                  if (     turbo_bind >= 0
                        && id == settings->uints.input_remap_ids[port][turbo_bind])
                     res = 0;
               }

               if (turbo_mode > INPUT_TURBO_MODE_CLASSIC_TOGGLE)
               {
                  unsigned turbo_button = settings->uints.input_turbo_button;
                  unsigned remap_button = settings->uints.input_remap_ids[port][turbo_button];

                  /* Single button modes only care about the defined button. */
                  if (id != remap_button)
                     break;

                  /* Pressing turbo bind toggles turbo button on or off.
                   * Holding the button will pass through, else
                   * the pressed state will be modulated by a
                   * periodic pulse defined by the configured duty cycle.
                   */

                  /* Avoid detecting the turbo button being held as multiple toggles */
                  if (!input_st->turbo_btns.frame_enable[port])
                     input_st->turbo_btns.turbo_pressed[port] &= ~(1 << 31);
                  else if (input_st->turbo_btns.turbo_pressed[port] >= 0)
                  {
                     input_st->turbo_btns.turbo_pressed[port] |= (1 << 31);
                     /* Toggle turbo for selected button. */
                     if (input_st->turbo_btns.enable[port] != (1 << id))
                        input_st->turbo_btns.enable[port] = (1 << id);
                     input_st->turbo_btns.mode1_enable[port] ^= 1;
                  }

                  if (input_st->turbo_btns.turbo_pressed[port] & (1 << 31))
                  {
                     /* Avoid detecting buttons being held as multiple toggles */
                     if (!res)
                        input_st->turbo_btns.turbo_pressed[port] &= ~(1 << id);
                     else if (!(input_st->turbo_btns.turbo_pressed[port] & (1 << id))
                           && turbo_mode == INPUT_TURBO_MODE_SINGLEBUTTON)
                     {
                        uint16_t enable_new;
                        input_st->turbo_btns.turbo_pressed[port] |= 1 << id;
                        enable_new = input_st->turbo_btns.enable[port] ^ (1 << id);
                        if (enable_new)
                           input_st->turbo_btns.enable[port] = enable_new;
                     }
                  }
                  /* Hold mode stops turbo on release */
                  else if ((turbo_mode == INPUT_TURBO_MODE_SINGLEBUTTON_HOLD)
                        && (input_st->turbo_btns.enable[port])
                        && (input_st->turbo_btns.mode1_enable[port]))
                     input_st->turbo_btns.mode1_enable[port] = 0;

                  if (     (!res)
                        && (input_st->turbo_btns.mode1_enable[port])
                        && (input_st->turbo_btns.enable[port] & (1 << id)))
                     res = ((input_st->turbo_btns.count % turbo_period) < turbo_duty_cycle);
               }
               else if (turbo_mode == INPUT_TURBO_MODE_CLASSIC)
               {
                  /* If turbo button is held, all buttons pressed
                   * will go into a turbo mode. Until the button is
                   * released again, the input state will be modulated by a
                   * periodic pulse defined by the configured duty cycle.
                   */
                  if (res)
                  {
                     if (input_st->turbo_btns.frame_enable[port])
                        input_st->turbo_btns.enable[port] |= (1 << id);

                     if (input_st->turbo_btns.enable[port] & (1 << id))
                        /* if turbo button is enabled for this key ID */
                        res = ((input_st->turbo_btns.count % turbo_period) < turbo_duty_cycle);
                  }
                  else
                     input_st->turbo_btns.enable[port] &= ~(1 << id);
               }
               else /* Classic toggle mode */
               {
                  /* Works pretty much the same as
                   * classic mode above but with a
                   * toggle mechanic */

                  /* Check if it's to enable the turbo func,
                   * if we're still holding the button from
                   * previous toggle then ignore */
                  if (   (res)
                      && (input_st->turbo_btns.frame_enable[port]))
                  {
                     if (!(input_st->turbo_btns.turbo_pressed[port] & (1 << id)))
                     {
                        input_st->turbo_btns.enable[port] ^= (1 << id);
                        /* Remember for the toggle check */
                        input_st->turbo_btns.turbo_pressed[port] |= (1 << id);
                     }
                  }
                  else
                     input_st->turbo_btns.turbo_pressed[port] &= ~(1 << id);

                  if (res)
                  {
                     /* If turbo button is enabled for this key ID */
                     if (input_st->turbo_btns.enable[port] & (1 << id))
                        res = ((input_st->turbo_btns.count % turbo_period) < turbo_duty_cycle);
                  }
               }
            }
         }

         break;


      case RETRO_DEVICE_KEYBOARD:

         res = ret;

         if (id < RETROK_LAST)
         {
#ifdef HAVE_OVERLAY
            if (port == 0)
            {
               if (input_st->overlay_ptr
                     && (input_st->overlay_ptr->flags & INPUT_OVERLAY_ALIVE))
               {
                  input_overlay_state_t
                     *ol_state          = &input_st->overlay_ptr->overlay_state;

                  if (OVERLAY_GET_KEY(ol_state, id))
                     res               |= 1;
               }
            }
#endif
            if (MAPPER_GET_KEY(handle, id))
               res |= 1;
         }

         break;


      case RETRO_DEVICE_ANALOG:
         {
#if defined(HAVE_NETWORKGAMEPAD) || defined(HAVE_OVERLAY)
#ifdef HAVE_NETWORKGAMEPAD
            input_remote_state_t
               *input_state         = &input_st->remote_st_ptr;

#endif
            unsigned base           = (idx == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
               ? 2 : 0;
            if (id == RETRO_DEVICE_ID_ANALOG_Y)
               base += 1;
#ifdef HAVE_NETWORKGAMEPAD
            if (     input_st->remote && idx < RETRO_DEVICE_INDEX_ANALOG_BUTTON
                  && input_state && input_state->analog[base][port])
               res          = input_state->analog[base][port];
            else
#endif
#endif
            {
               if (id < RARCH_FIRST_META_KEY)
               {
                  bool bind_valid         = input_st->libretro_input_binds[port]
                     && RETRO_KEYBIND_VALID(&(*input_st->libretro_input_binds[port])[id]);

                  if (bind_valid)
                  {
                     /* reset_state - used to reset input state of a button
                      * when the gamepad mapper is in action for that button*/
                     bool reset_state        = false;
                     if (idx < 2 && id < 2)
                     {
                        unsigned offset = RARCH_FIRST_CUSTOM_BIND +
                           (idx * 4) + (id * 2);

                        if (settings->uints.input_remap_ids[port][offset] != offset)
                           reset_state = true;
                        else if (settings->uints.input_remap_ids[port][offset + 1] != (offset+1))
                           reset_state = true;

                        /* The empty Turbo Bind has no RetroPad button
                         * to clear. */
                        if (     input_st->turbo_btns.frame_enable[port]
                              && settings->ints.input_turbo_bind >= 0)
                        {
                           unsigned remap_bind = settings->uints.input_remap_ids
                              [port][settings->ints.input_turbo_bind];

                           if (offset == remap_bind || offset + 1 == remap_bind)
                           {
                              res = 0;
                              break;
                           }
                        }
                     }

                     if (reset_state)
                        res = 0;
                     else
                     {
                        res = ret;

#ifdef HAVE_OVERLAY
                        if (     (input_st->overlay_ptr)
                              && (input_st->overlay_ptr->flags & INPUT_OVERLAY_ALIVE)
                              && (port == 0)
                              && (idx != RETRO_DEVICE_INDEX_ANALOG_BUTTON)
                              && !(    (  input_analog_dpad_mode == ANALOG_DPAD_LSTICK
                                       && idx == RETRO_DEVICE_INDEX_ANALOG_LEFT)
                                    || (  input_analog_dpad_mode == ANALOG_DPAD_RSTICK
                                       && idx == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
                                    || (  (  input_analog_dpad_mode == ANALOG_DPAD_LRSTICK
                                          || input_analog_dpad_mode == ANALOG_DPAD_TWINSTICK)
                                       && (  idx == RETRO_DEVICE_INDEX_ANALOG_LEFT
                                          || idx == RETRO_DEVICE_INDEX_ANALOG_RIGHT))
                                 )
                           )
                        {
                           input_overlay_state_t *ol_state =
                              &input_st->overlay_ptr->overlay_state;
                           int16_t ol_analog               =
                                 ol_state->analog[base];

                           /* Analog values are an integer corresponding
                            * to the extent of the analog motion; these
                            * cannot be OR'd together, we must instead
                            * keep the value with the largest magnitude */
                           if (ol_analog)
                           {
                              if (res == 0)
                                 res = ol_analog;
                              else
                              {
                                 int16_t ol_analog_abs = (ol_analog >= 0) ?
                                       ol_analog : -ol_analog;
                                 int16_t res_abs       = (res >= 0) ?
                                       res : -res;

                                 res = (ol_analog_abs > res_abs) ?
                                       ol_analog : res;
                              }
                           }
                        }
#endif
                     }
                  }
               }
            }

            if (idx < 2 && id < 2)
            {
               unsigned offset = 0 + (idx * 4) + (id * 2);
               int        val1 = handle->analog_value[port][offset];
               int        val2 = handle->analog_value[port][offset+1];

               /* OR'ing these analog values is 100% incorrect,
                * but I have no idea what this code is supposed
                * to be doing (val1 and val2 always seem to be
                * zero), so I will leave it alone... */
               if (val1)
                  res          |= val1;
               else if (val2)
                  res          |= val2;
            }
         }
         break;

      case RETRO_DEVICE_MOUSE:
      case RETRO_DEVICE_LIGHTGUN:
      case RETRO_DEVICE_POINTER:

#ifdef HAVE_OVERLAY
         if (     (input_st->overlay_ptr)
               && (input_st->overlay_ptr->flags & INPUT_OVERLAY_ENABLE)
               && !(input_st->overlay_ptr->flags & INPUT_OVERLAY_STYLUS_HIDDEN)
               && (settings->bools.input_overlay_pointer_enable))
            res = input_overlay_pointing_device_state(
                  settings->ints.input_overlay_lightgun_port,
                  settings->bools.input_overlay_lightgun_allow_offscreen,
                  input_st->overlay_ptr, port, device, idx, id);
#endif

         if (res || input_st->flags & INP_FLAG_BLOCK_POINTER_INPUT)
            break;

         if (id < RARCH_FIRST_META_KEY)
         {
            bool bind_valid = input_st->libretro_input_binds[port]
               && RETRO_KEYBIND_VALID(&(*input_st->libretro_input_binds[port])[id]);

            if (bind_valid)
            {
               if (button_mask)
               {
                  if (ret & (1 << id))
                     res |= (1 << id);
               }
               else
                  res = ret;
            }
         }

         break;
   }

   return res;
}


static int16_t input_state_internal(
      input_driver_state_t *input_st,
      settings_t *settings,
      unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   rarch_joypad_info_t joypad_info;
   float input_analog_deadzone             = settings->floats.input_analog_deadzone;
   float input_analog_sensitivity          = settings->floats.input_analog_sensitivity;
   unsigned *input_remap_port_map          = settings->uints.input_remap_port_map[port];
   /* Clamped: the arrays walked below are [MAX_USERS] and the setting
    * comes from the config file. */
   uint8_t max_users                       = (settings->uints.input_max_users
         > MAX_USERS) ? MAX_USERS
         : (uint8_t)settings->uints.input_max_users;
   const input_device_driver_t *joypad     = INPUT_JOYPAD_FOR_READ(
         input_st, input_st->primary_joypad);
#ifdef HAVE_MFI
   const input_device_driver_t *sec_joypad = INPUT_JOYPAD_FOR_READ(
         input_st, input_st->secondary_joypad);
#else
   const input_device_driver_t *sec_joypad = NULL;
#endif
   uint8_t mapped_port                     = 0;
   int16_t result                          = 0;
#ifdef HAVE_MENU
   struct menu_state *menu_st              = menu_state_get_ptr();
   bool input_blocked                      =    (menu_st->input_driver_flushing_input > 0)
                                             || (input_st->flags & INP_FLAG_BLOCK_LIBRETRO_INPUT);
#else
   bool input_blocked                      = (input_st->flags & INP_FLAG_BLOCK_LIBRETRO_INPUT) ? true : false;
#endif
   bool input_driver_analog_requested      = input_st->analog_requested[port];
   bool bitmask_enabled                    = false;

   device                                 &= RETRO_DEVICE_MASK;
   bitmask_enabled                         =    (device == RETRO_DEVICE_JOYPAD)
                                             && (id == RETRO_DEVICE_ID_JOYPAD_MASK);
   joypad_info.axis_threshold              = settings->floats.input_axis_threshold;

   /* Cache once; flag does not change during this function */
   {
      bool kb_mapping_blocked = !!(input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED);

   /* Loop over all 'physical' ports mapped to specified
    * 'virtual' port index */
   while ((mapped_port = *(input_remap_port_map++)) < MAX_USERS)
   {
      int16_t ret                    = 0;
      int16_t port_result            = 0;
      uint8_t input_analog_dpad_mode = settings->uints.input_analog_dpad_mode[mapped_port];

      joypad_info.joy_idx            = settings->uints.input_joypad_index[mapped_port];
      if (joypad_info.joy_idx >= MAX_USERS)
         joypad_info.joy_idx         = 0;
      joypad_info.auto_binds         = input_autoconf_binds[joypad_info.joy_idx];

      /* Skip disabled input devices */
      if (mapped_port >= max_users)
         continue;

      /* If core has requested analog input, disable
       * analog to dpad mapping (unless forced) */
      switch (input_analog_dpad_mode)
      {
         case ANALOG_DPAD_LSTICK:
         case ANALOG_DPAD_RSTICK:
         case ANALOG_DPAD_LRSTICK:
         case ANALOG_DPAD_TWINSTICK:
            if (input_driver_analog_requested)
               input_analog_dpad_mode = ANALOG_DPAD_NONE;
            break;
         case ANALOG_DPAD_LSTICK_FORCED:
            input_analog_dpad_mode = ANALOG_DPAD_LSTICK;
            break;
         case ANALOG_DPAD_RSTICK_FORCED:
            input_analog_dpad_mode = ANALOG_DPAD_RSTICK;
            break;
         case ANALOG_DPAD_LRSTICK_FORCED:
            input_analog_dpad_mode = ANALOG_DPAD_LRSTICK;
            break;
         case ANALOG_DPAD_TWINSTICK_FORCED:
            input_analog_dpad_mode = ANALOG_DPAD_TWINSTICK;
            break;
         default:
            break;
      }

      /* TODO/FIXME: This code is gibberish - a mess of nested
       * refactors that make no sense whatsoever. The entire
       * thing needs to be rewritten from scratch... */

      ret = input_state_wrap(
            input_st->current_driver,
            input_st->current_data,
            joypad,
            sec_joypad,
            &joypad_info,
            (*input_st->libretro_input_binds),
            kb_mapping_blocked,
            mapped_port, device, idx, id);

      /* "Aim From Analog Stick": where the port's stick is held is
       * where its lightgun or pointer points, the stick's centre
       * being the screen's. The stick is read as it comes: a deadzone
       * would make the centre of the screen a place it snaps to. */
      if (     settings->uints.input_aim_stick[mapped_port]
            && (   device == RETRO_DEVICE_LIGHTGUN
                || (device == RETRO_DEVICE_POINTER && idx == 0))
            && input_st->libretro_input_binds[mapped_port])
      {
         int axis = -1;
         if (device == RETRO_DEVICE_LIGHTGUN)
         {
            if (id == RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X)
               axis = RETRO_DEVICE_ID_ANALOG_X;
            else if (id == RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y)
               axis = RETRO_DEVICE_ID_ANALOG_Y;
            else if (id == RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN)
               ret  = 0; /* a stick is never off the screen */
         }
         else if (id == RETRO_DEVICE_ID_POINTER_X)
            axis = RETRO_DEVICE_ID_ANALOG_X;
         else if (id == RETRO_DEVICE_ID_POINTER_Y)
            axis = RETRO_DEVICE_ID_ANALOG_Y;
         else if (id == RETRO_DEVICE_ID_POINTER_PRESSED && !ret)
            /* the gun's trigger is the pointer's press */
            ret = input_state_wrap(
                  input_st->current_driver,
                  input_st->current_data,
                  joypad,
                  sec_joypad,
                  &joypad_info,
                  (*input_st->libretro_input_binds),
                  kb_mapping_blocked,
                  mapped_port, RETRO_DEVICE_LIGHTGUN, 0,
                  RETRO_DEVICE_ID_LIGHTGUN_TRIGGER);

         if (axis >= 0)
         {
            unsigned stick =
               (settings->uints.input_aim_stick[mapped_port] == INPUT_AIM_STICK_RIGHT)
               ? RETRO_DEVICE_INDEX_ANALOG_RIGHT : RETRO_DEVICE_INDEX_ANALOG_LEFT;
            ret = 0;
            if (sec_joypad)
               ret = input_joypad_analog_axis(ANALOG_DPAD_NONE, 0.0f, 1.0f,
                     sec_joypad, &joypad_info, stick, (unsigned)axis,
                     (*input_st->libretro_input_binds[mapped_port]));
            if (joypad && !ret)
               ret = input_joypad_analog_axis(ANALOG_DPAD_NONE, 0.0f, 1.0f,
                     joypad, &joypad_info, stick, (unsigned)axis,
                     (*input_st->libretro_input_binds[mapped_port]));
         }
      }

      /* Ignore analog sticks when using Analog to Digital */
      if (     (device == RETRO_DEVICE_ANALOG)
            && (input_analog_dpad_mode != ANALOG_DPAD_NONE))
         ret = 0;

      if (     (device == RETRO_DEVICE_ANALOG)
            && (ret == 0))
      {
         if (input_st->libretro_input_binds[mapped_port])
         {
            if (idx == RETRO_DEVICE_INDEX_ANALOG_BUTTON)
            {
               if (id < RARCH_FIRST_CUSTOM_BIND)
               {
                  bool valid_bind = RETRO_KEYBIND_VALID(&(*input_st->libretro_input_binds[mapped_port])[id]) &&
                        (id == settings->uints.input_remap_ids[mapped_port][id]);
                  /* Hardest pressure from buttons remapped onto this one. */
                  int16_t remapped = (int16_t)input_st->mapper.buttons[mapped_port].analog_buttons[id];

                  if (valid_bind)
                  {
                     if (sec_joypad)
                        ret = input_joypad_analog_button(
                              input_analog_deadzone,
                              input_analog_sensitivity,
                              sec_joypad, &joypad_info,
                              id,
                              &(*input_st->libretro_input_binds[mapped_port])[id]);

                     if (joypad && (ret == 0))
                        ret = input_joypad_analog_button(
                              input_analog_deadzone,
                              input_analog_sensitivity,
                              joypad, &joypad_info,
                              id,
                              &(*input_st->libretro_input_binds[mapped_port])[id]);
                  }

                  if (remapped > ret)
                     ret = remapped;
               }
            }
            else
            {
               if (sec_joypad)
                  ret = input_joypad_analog_axis(
                        input_analog_dpad_mode,
                        input_analog_deadzone,
                        input_analog_sensitivity,
                        sec_joypad,
                        &joypad_info,
                        idx,
                        id,
                        (*input_st->libretro_input_binds[mapped_port]));

               if (joypad && (ret == 0))
                  ret = input_joypad_analog_axis(
                        input_analog_dpad_mode,
                        input_analog_deadzone,
                        input_analog_sensitivity,
                        joypad,
                        &joypad_info,
                        idx,
                        id,
                        (*input_st->libretro_input_binds[mapped_port]));
            }
         }
      }

      if (!input_blocked)
      {
         input_mapper_t *handle = &input_st->mapper;

         if (bitmask_enabled)
         {
            uint8_t i;
            for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
               if (input_state_device(input_st,
                        settings, handle,
                        input_analog_dpad_mode, ret, mapped_port,
                        device, idx, i, true))
                  port_result |= (1 << i);
         }
         else
            port_result = input_state_device(input_st,
                  settings, handle,
                  input_analog_dpad_mode, ret, mapped_port,
                  device, idx, id, false);

         /* Handle Analog to Digital */
         if (     (device == RETRO_DEVICE_JOYPAD)
               && (input_analog_dpad_mode != ANALOG_DPAD_NONE)
               && joypad
            )
         {
            int16_t ret_axis;
            uint8_t s;
            uint8_t a;
            float axis_thr     = joypad_info.axis_threshold;
            float inv_0x7fff   = INV_0x7fff;

            for (s = RETRO_DEVICE_INDEX_ANALOG_LEFT; s <= RETRO_DEVICE_INDEX_ANALOG_RIGHT; s++)
            {
               if (     (s == RETRO_DEVICE_INDEX_ANALOG_LEFT  && input_analog_dpad_mode == ANALOG_DPAD_RSTICK)
                     || (s == RETRO_DEVICE_INDEX_ANALOG_RIGHT && input_analog_dpad_mode == ANALOG_DPAD_LSTICK))
                  continue;

               for (a = RETRO_DEVICE_ID_ANALOG_X; a <= RETRO_DEVICE_ID_ANALOG_Y; a++)
               {
                  ret_axis = input_joypad_analog_axis(
                        ANALOG_DPAD_NONE,
                        settings->floats.input_analog_deadzone,
                        settings->floats.input_analog_sensitivity,
                        joypad,
                        &joypad_info,
                        s,
                        a,
                        (*input_st->libretro_input_binds[mapped_port]));

                  if (ret_axis)
                  {
                     float norm  = (float)ret_axis * inv_0x7fff;
                     int bit     = -1;

                     if (a == RETRO_DEVICE_ID_ANALOG_Y && norm < -axis_thr)
                     {
                        if (input_analog_dpad_mode == ANALOG_DPAD_TWINSTICK && s == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
                           bit = RETRO_DEVICE_ID_JOYPAD_X;
                        else
                           bit = RETRO_DEVICE_ID_JOYPAD_UP;
                     }
                     else if (a == RETRO_DEVICE_ID_ANALOG_Y && norm > axis_thr)
                     {
                        if (input_analog_dpad_mode == ANALOG_DPAD_TWINSTICK && s == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
                           bit = RETRO_DEVICE_ID_JOYPAD_B;
                        else
                           bit = RETRO_DEVICE_ID_JOYPAD_DOWN;
                     }

                     if (a == RETRO_DEVICE_ID_ANALOG_X && norm < -axis_thr)
                     {
                        if (input_analog_dpad_mode == ANALOG_DPAD_TWINSTICK && s == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
                           bit = RETRO_DEVICE_ID_JOYPAD_Y;
                        else
                           bit = RETRO_DEVICE_ID_JOYPAD_LEFT;
                     }
                     else if (a == RETRO_DEVICE_ID_ANALOG_X && norm > axis_thr)
                     {
                        if (input_analog_dpad_mode == ANALOG_DPAD_TWINSTICK && s == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
                           bit = RETRO_DEVICE_ID_JOYPAD_A;
                        else
                           bit = RETRO_DEVICE_ID_JOYPAD_RIGHT;
                     }

                     if (bit > -1)
                     {
                        if (bitmask_enabled)
                           port_result |= (1 << bit);
                        else if (id == (unsigned)bit)
                        {
                           /* Digital results are OR'd together by the
                            * caller, so this must be a plain 1 - not
                            * the bind index, which would leave stray
                            * high bits in the returned value */
                           port_result = 1;
                           result      = 1;
                        }
                     }
                  }
               }
            }
         }
      }

      /* Digital values are represented by a bitmap;
       * we can just perform the logical OR of
       * successive samples.
       * Analog values are an integer corresponding
       * to the extent of the analog motion; these
       * cannot be OR'd together, we must instead
       * keep the value with the largest magnitude */
      if (device == RETRO_DEVICE_ANALOG)
      {
         if (result == 0)
            result = port_result;
         else
         {
            int16_t port_result_abs = (port_result >= 0)
               ? port_result : -port_result;
            int16_t result_abs      = (result >= 0)
               ? result      : -result;

            if (port_result_abs > result_abs)
               result = port_result;
         }
      }
      else
         result |= port_result;
   }
   } /* kb_mapping_blocked scope */

   return result;
}


#ifdef HAVE_OVERLAY
/**
 * input_overlay_add_inputs:
 * @desc : pointer to overlay description
 * @ol_state : pointer to overlay state. If valid, inputs
 *             that are actually 'touched' on the overlay
 *             itself will displayed. If NULL, inputs from
 *             the device connected to 'port' will be displayed.
 * @port : when ol_state is NULL, specifies the port of
 *         the input device from which input will be
 *         displayed.
 *
 * Adds inputs from current_input to the overlay, so it's displayed
 * @return true if an input that is pressed will change the overlay
 */
static bool input_overlay_add_inputs_inner(overlay_desc_t *desc,
      input_driver_state_t *input_st,
      settings_t *settings,
      input_overlay_state_t *ol_state, unsigned port)
{
   switch(desc->type)
   {
      case OVERLAY_TYPE_BUTTONS:
         {
            int i;

            /* Check custom binds in the mask */
            for (i = 0; i < CUSTOM_BINDS_U32_COUNT; ++i)
            {
               /* Get bank */
               uint32_t bank_mask = BITS_GET_ELEM(desc->button_mask,i);
               unsigned        id = i * 32;

               /* Worth pursuing? Have we got any bits left in here? */
               while (bank_mask)
               {
                  /* If this bit is set then we need to query the pad
                   * The button must be pressed.*/
                  if (bank_mask & 1)
                  {
                     if (id >= RARCH_CUSTOM_BIND_LIST_END)
                        break;

                     /* Light up the button if pressed */
                     if (     ol_state
                           ? !BIT256_GET(ol_state->buttons, id)
                           : !input_state_internal(input_st,
                              settings, port, RETRO_DEVICE_JOYPAD, 0, id))
                     {
                        /* We need ALL of the inputs to be active,
                         * abort. */
                        desc->touch_mask = 0;
                        return false;
                     }

                     desc->touch_mask   |= (1 << OVERLAY_MAX_TOUCH);
                  }

                  bank_mask >>= 1;
                  ++id;
               }
            }

            return (desc->touch_mask != 0);
         }

      case OVERLAY_TYPE_ANALOG_LEFT:
      case OVERLAY_TYPE_ANALOG_RIGHT:
         if (ol_state)
         {
            unsigned index_offset = (desc->type == OVERLAY_TYPE_ANALOG_RIGHT) ? 2 : 0;
            desc->touch_mask     |= (
                   ol_state->analog[index_offset]
                 | ol_state->analog[index_offset + 1]) << OVERLAY_MAX_TOUCH;
         }
         else
         {
            unsigned index        = (desc->type == OVERLAY_TYPE_ANALOG_RIGHT)
               ? RETRO_DEVICE_INDEX_ANALOG_RIGHT
               : RETRO_DEVICE_INDEX_ANALOG_LEFT;
            int16_t analog_x      = input_state_internal(input_st, settings, port, RETRO_DEVICE_ANALOG,
                  index, RETRO_DEVICE_ID_ANALOG_X);
            int16_t analog_y      = input_state_internal(input_st, settings, port, RETRO_DEVICE_ANALOG,
                  index, RETRO_DEVICE_ID_ANALOG_Y);

            /* Only modify overlay delta_x/delta_y values
             * if we are monitoring input from a physical
             * controller */
            desc->delta_x         = (analog_x / (float)0x8000) * (desc->range_x / 2.0f);
            desc->delta_y         = (analog_y / (float)0x8000) * (desc->range_y / 2.0f);
         }

         /* fall-through */

      case OVERLAY_TYPE_DPAD_AREA:
      case OVERLAY_TYPE_ABXY_AREA:
         return (desc->touch_mask != 0);

      case OVERLAY_TYPE_KEYBOARD:
         {
            bool tmp    = false;
            if (ol_state)
            {
               if (OVERLAY_GET_KEY(ol_state, desc->retro_key_idx))
                  tmp   = true;
            }
            else
               tmp      = input_state_internal(input_st, settings, port,
                     RETRO_DEVICE_KEYBOARD, 0, desc->retro_key_idx);

            if (tmp)
            {
               desc->touch_mask |= (1 << OVERLAY_MAX_TOUCH);
               return true;
            }
         }
         break;

      default:
         break;
   }

   return false;
}

static bool input_overlay_add_inputs(input_overlay_t *ol,
      input_overlay_state_t *ol_state,
      input_driver_state_t *input_st,
      settings_t *settings,
      bool show_touched, unsigned port)
{
   size_t i;
   bool button_pressed      = false;

   for (i = 0; i < ol->active->size; i++)
   {
      overlay_desc_t *desc  = &(ol->active->descs[i]);
      button_pressed       |= input_overlay_add_inputs_inner(
            desc, input_st,
            settings,
            show_touched
            ? ol_state
            : NULL,
            port);
   }

   return button_pressed;
}

static void input_overlay_get_eightway_slope_limits(
      const unsigned diagonal_sensitivity,
      float* low_slope, float* high_slope)
{
   /* Sensitivity setting is the relative size of diagonal zones to
    * cardinal zones. Convert to fraction of 45 deg span (max diagonal).
    */
   float f     =  2.0f * diagonal_sensitivity
             / (100.0f + diagonal_sensitivity);

   float high_angle  /* 67.5 deg max */
               = (f * (0.375 * M_PI) + (1.0f - f) * (0.25 * M_PI));
   float low_angle   /* 22.5 deg min */
               = (f * (0.125 * M_PI) + (1.0f - f) * (0.25 * M_PI));

   *high_slope = tan(high_angle);
   *low_slope  = tan(low_angle);
}

/**
 * input_overlay_set_eightway_diagonal_sensitivity:
 *
 * Gets the slope limits defining each eightway type's diagonal zones.
 */
void input_overlay_set_eightway_diagonal_sensitivity(void)
{
   settings_t           *settings = config_get_ptr();
   input_driver_state_t *input_st = &input_driver_st;

   input_overlay_get_eightway_slope_limits(
         settings->uints.input_overlay_dpad_diagonal_sensitivity,
         &input_st->overlay_eightway_dpad_slopes[0],
         &input_st->overlay_eightway_dpad_slopes[1]);

   input_overlay_get_eightway_slope_limits(
         settings->uints.input_overlay_abxy_diagonal_sensitivity,
         &input_st->overlay_eightway_abxy_slopes[0],
         &input_st->overlay_eightway_abxy_slopes[1]);
}

/**
 * input_overlay_get_eightway_state:
 * @desc : overlay descriptor handle for an eightway area
 * @out : current input state to be OR'd with eightway state
 * @x_dist : X offset from eightway area center
 * @y_dist : Y offset from eightway area center
 *
 * Gets the eightway area's current input state based on (@x_dist, @y_dist).
 **/
static INLINE void input_overlay_get_eightway_state(
      const struct overlay_desc *desc,
      overlay_eightway_config_t *eightway,
      input_bits_t *out,
      float x_dist, float y_dist)
{
   uint32_t *data;
   float abs_x, abs_y;

   x_dist /= desc->range_x;
   y_dist /= desc->range_y;

   abs_x = fabsf(x_dist);
   abs_y = fabsf(y_dist);

   /* Compare abs_y against slope * abs_x instead of computing
    * abs_slope = abs_y / abs_x.  Avoids a division and the
    * special-case for x_dist == 0 (vertical input). When
    * abs_x is zero, abs_y > slope_high * 0 is always true
    * for any nonzero touch, correctly selecting up/down. */

   if (x_dist > 0.0f)
   {
      if (y_dist < 0.0f)
      {
         /* Q1 */
         if (abs_y > *eightway->slope_high * abs_x)
            data = eightway->up.data;
         else if (abs_y < *eightway->slope_low * abs_x)
            data = eightway->right.data;
         else
            data = eightway->up_right.data;
      }
      else
      {
         /* Q4 */
         if (abs_y > *eightway->slope_high * abs_x)
            data = eightway->down.data;
         else if (abs_y < *eightway->slope_low * abs_x)
            data = eightway->right.data;
         else
            data = eightway->down_right.data;
      }
   }
   else
   {
      if (y_dist < 0.0f)
      {
         /* Q2 */
         if (abs_y > *eightway->slope_high * abs_x)
            data = eightway->up.data;
         else if (abs_y < *eightway->slope_low * abs_x)
            data = eightway->left.data;
         else
            data = eightway->up_left.data;
      }
      else
      {
         /* Q3 */
         if (abs_y > *eightway->slope_high * abs_x)
            data = eightway->down.data;
         else if (abs_y < *eightway->slope_low * abs_x)
            data = eightway->left.data;
         else
            data = eightway->down_left.data;
      }
   }

   bits_or_bits(out->data, data, CUSTOM_BINDS_U32_COUNT);
}

/**
 * input_overlay_get_analog_state:
 * @out : Overlay input state to be modified
 * @desc : Overlay descriptor handle
 * @base : 0 or 2 for analog_left or analog_right
 * @x : X coordinate
 * @y : Y coordinate
 * @x_dist : X offset from analog center
 * @y_dist : Y offset from analog center
 * @first_touch : Set true if analog was not controlled in previous poll
 *
 * Gets the analog input state based on @x and @y, and applies to @out.
 */
static void input_overlay_get_analog_state(
      input_overlay_state_t *out, struct overlay_desc *desc,
      unsigned base, float x, float y, float *x_dist, float *y_dist,
      bool first_touch)
{
   float x_val, y_val;
   float x_val_sat, y_val_sat;
   const int b = base / 2;

   static float x_center[2];
   static float y_center[2];

   if (first_touch)
   {
      unsigned recenter_zone =  /* [0,100] */
            config_get_ptr()->uints.input_overlay_analog_recenter_zone;

      if (recenter_zone != 0)
      {
         float touch_dist, w;

         x_val      = (x - desc->x_shift) / desc->range_x;
         y_val      = (y - desc->y_shift) / desc->range_y;
         touch_dist = sqrt((x_val * x_val + y_val * y_val) * 1e4);

         /* Inside zone, recenter to first touch.
          * Outside zone, recenter to zone perimeter. */
         if (touch_dist <= recenter_zone || recenter_zone >= 100)
            w = 0.0f;
         else
            w = (touch_dist - recenter_zone) / touch_dist;

         x_center[b] = x * (1.0f - w) + desc->x_shift * w;
         y_center[b] = y * (1.0f - w) + desc->y_shift * w;
      }
      else
      {
         x_center[b] = desc->x_shift;
         y_center[b] = desc->y_shift;
      }
   }

   *x_dist   = x - x_center[b];
   *y_dist   = y - y_center[b];
   x_val     = *x_dist / desc->range_x;
   y_val     = *y_dist / desc->range_y;
   x_val_sat = x_val   / desc->analog_saturate_pct;
   y_val_sat = y_val   / desc->analog_saturate_pct;

   out->analog[base + 0] = clamp_float(x_val_sat, -1.0f, 1.0f) * 32767.0f;
   out->analog[base + 1] = clamp_float(y_val_sat, -1.0f, 1.0f) * 32767.0f;
}

/**
 * input_overlay_coords_inside_hitbox:
 * @desc                  : Overlay descriptor handle.
 * @x                     : X coordinate value.
 * @y                     : Y coordinate value.
 * @use_range_mod         : Set true to use range_mod hitbox
 *
 * Check whether the given @x and @y coordinates of the overlay
 * descriptor @desc is inside the overlay descriptor's hitbox.
 *
 * Returns: true (1) if X, Y coordinates are inside a hitbox,
 * otherwise false (0).
 **/
static bool input_overlay_coords_inside_hitbox(const struct overlay_desc *desc,
      float x, float y, bool use_range_mod)
{
   float range_x, range_y;

   if (use_range_mod)
   {
      range_x = desc->range_x_mod;
      range_y = desc->range_y_mod;
   }
   else
   {
      range_x = desc->range_x_hitbox;
      range_y = desc->range_y_hitbox;
   }

   switch (desc->hitbox)
   {
      case OVERLAY_HITBOX_RADIAL:
      {
         /* Ellipse. */
         float x_dist  = (x - desc->x_hitbox) / range_x;
         float y_dist  = (y - desc->y_hitbox) / range_y;
         float sq_dist = x_dist * x_dist + y_dist * y_dist;
         return (sq_dist <= 1.0f);
      }
      case OVERLAY_HITBOX_RECT:
         return
               (fabs(x - desc->x_hitbox) <= range_x)
            && (fabs(y - desc->y_hitbox) <= range_y);
      case OVERLAY_HITBOX_NONE:
         break;
   }
   return false;
}

/**
 * input_overlay_poll:
 * @out                   : Polled output data.
 * @touch_idx             : Touch pointer index.
 * @old_touch_idx         : Touch pointer index from previous poll, or -1.
 * @norm_x                : Normalized X coordinate.
 * @norm_y                : Normalized Y coordinate.
 * @touch_scale           : Overlay scale.
 *
 * Polls input overlay for a single touch pointer.
 *
 * @norm_x and @norm_y are the result of
 * video_driver_translate_coord_viewport().
 *
 * @return true if touch pointer is inside any hitbox
 **/
static bool input_overlay_poll(
      input_overlay_t *ol,
      input_overlay_state_t *out,
      int touch_idx, int old_touch_idx,
      int16_t norm_x, int16_t norm_y, float touch_scale)
{
   size_t i, j;
   struct overlay_desc *descs = ol->active->descs;
   unsigned int highest_prio  = 0;
   bool any_hitbox_pressed    = false;
   bool use_range_mod;

   /* norm_x and norm_y is in [-0x7fff, 0x7fff] range,
    * like RETRO_DEVICE_POINTER. */
   float x = (float)(norm_x + 0x7fff) / 0xffff;
   float y = (float)(norm_y + 0x7fff) / 0xffff;

   x -= ol->active->mod_x;
   y -= ol->active->mod_y;
   x /= ol->active->mod_w;
   y /= ol->active->mod_h;

   x *= touch_scale;
   y *= touch_scale;

   for (i = 0; i < ol->active->size; i++)
   {
      float x_dist, y_dist;
      unsigned int base         = 0;
      unsigned int desc_prio    = 0;
      struct overlay_desc *desc = &descs[i];

      /* Use range_mod if this touch pointer contributed
       * to desc's touch_mask in the previous poll */
      use_range_mod = (old_touch_idx != -1)
            && BIT32_GET(desc->old_touch_mask, old_touch_idx);

      if (!input_overlay_coords_inside_hitbox(desc, x, y, use_range_mod))
         continue;

      /* Check for exclusive hitbox, which blocks other input.
       * range_mod_exclusive has priority over exclusive. */
      if (use_range_mod && (desc->flags & OVERLAY_DESC_RANGE_MOD_EXCLUSIVE))
         desc_prio = 2;
      else if (desc->flags & OVERLAY_DESC_EXCLUSIVE)
         desc_prio = 1;

      if (highest_prio > desc_prio)
         continue;

      if (desc_prio > highest_prio)
      {
         highest_prio = desc_prio;
         memset(out, 0, sizeof(*out));
         for (j = 0; j < i; j++)
            BIT32_CLEAR(descs[j].touch_mask, touch_idx);
      }

      BIT32_SET(desc->touch_mask, touch_idx);
      x_dist = x - desc->x_shift;
      y_dist = y - desc->y_shift;

      switch (desc->type)
      {
         case OVERLAY_TYPE_BUTTONS:
            bits_or_bits(out->buttons.data,
                  desc->button_mask.data,
                  ARRAY_SIZE(desc->button_mask.data));

            if (BIT256_GET(desc->button_mask, RARCH_OVERLAY_NEXT))
               ol->next_index = desc->next_index;
            break;
         case OVERLAY_TYPE_KEYBOARD:
            if (desc->retro_key_idx < RETROK_LAST)
               OVERLAY_SET_KEY(out, desc->retro_key_idx);
            break;
         case OVERLAY_TYPE_DPAD_AREA:
         case OVERLAY_TYPE_ABXY_AREA:
            /* Gate on eightway_config because task_overlay_desc_
             * populate_eightway_config can legitimately fail its
             * calloc under OOM and leave this field NULL for an
             * area-type desc that requires it.  Without the
             * gate input_overlay_get_eightway_state NULL-derefs
             * on eightway->slope_high below. */
            if (desc->eightway_config)
               input_overlay_get_eightway_state(
                     desc, desc->eightway_config,
                     &out->buttons, x_dist, y_dist);
            break;
         case OVERLAY_TYPE_ANALOG_RIGHT:
            base = 2;
            /* fall-through */
         default:
            input_overlay_get_analog_state(
                  out, desc, base, x, y,
                  &x_dist, &y_dist, !use_range_mod);
            break;
      }

      if (desc->flags & OVERLAY_DESC_MOVABLE)
      {
         desc->delta_x = clamp_float(x_dist, -desc->range_x, desc->range_x)
            * ol->active->mod_w;
         desc->delta_y = clamp_float(y_dist, -desc->range_y, desc->range_y)
            * ol->active->mod_h;
      }

      any_hitbox_pressed = true;
   }

   if (ol->flags & INPUT_OVERLAY_BLOCKED)
      memset(out, 0, sizeof(*out));

   return any_hitbox_pressed;
}

/**
 * input_overlay_update_desc_geom:
 * @ol                    : overlay handle.
 * @desc                  : overlay descriptors handle.
 *
 * Update input overlay descriptors' vertex geometry.
 **/
static void input_overlay_update_desc_geom(input_overlay_t *ol,
      struct overlay_desc *desc)
{
   if (!OVERLAY_HAS_IMAGE(&desc->image) || !(desc->flags & OVERLAY_DESC_MOVABLE))
      return;

   if (ol->iface->vertex_geom)
      ol->iface->vertex_geom(ol->iface_data, desc->image_index,
            desc->mod_x + desc->delta_x, desc->mod_y + desc->delta_y,
            desc->mod_w, desc->mod_h);

   desc->delta_x = 0.0f;
   desc->delta_y = 0.0f;
}


#ifdef HAVE_RPNG
/* Show one frame of a two-frame APNG, from the pair composed at load.
 * False when the surface is still the video thread's or the submit
 * failed: the caller keeps its current state, so the next poll tries
 * again instead of the press or release being lost. */
static bool input_overlay_update_apng_frame(input_overlay_t *ol,
      size_t i, int target_frame)
{
   gfx_surface_t *s    = (gfx_surface_t*)ol->surfaces[i];
   const uint32_t *pix = ol->anim_2frame_pix[i];
   size_t frame_len;

   if (     !pix || !s || !s->num_slots || s->inflight
         || !gfx_surface_slot_writable(s, 0))
      return false;

   frame_len = VIDEO_SCALE_AREA(s->dims);
   memcpy(s->slots[0], pix + (target_frame ? frame_len : 0),
         frame_len * sizeof(uint32_t));
   return gfx_surface_submit(s, 0, ol->images[i]->supports_rgba)
         != GFX_SURFACE_SUBMIT_FAILED;
}

/* Update every two-frame APNG to the frame matching its aggregated
 * press state. Called after desc press state has been collected. */
static void input_overlay_update_2frame(input_overlay_t *ol)
{
   size_t i;

   if (!ol->anim_2frame || !ol->surfaces)
      return;

   for (i = 0; i < ol->num_images; i++)
   {
      uint8_t target;
      if (!ol->anim_2frame[i])
         continue;

      target = ol->anim_2frame_pressed[i] ? 1 : 0;
      if (     ol->anim_2frame_cur[i] != target
            && input_overlay_update_apng_frame(ol, i, target))
         ol->anim_2frame_cur[i] = target;
      ol->anim_2frame_pressed[i] = 0;
   }
}

/* Advance every animated image of the pack whose frame is due, once
 * per input poll on the main thread. A frame is composed by the
 * stream into a free slot and submitted: the texture is updated in
 * place, so the page's handles stand and a page switch is still an
 * upload of nothing. A slot still with the video thread means the
 * frame is skipped rather than waited for - the overlay is a control
 * surface, and a late button image is worse than a dropped one. */
void input_overlay_animate(input_overlay_t *ol, retro_time_t now)
{
   size_t i;

   if (     !ol
         || !ol->anim_stream
         || !(ol->flags & INPUT_OVERLAY_ENABLE)
         || !ol->surfaces)
      return;

   for (i = 0; i < ol->num_images; i++)
   {
      rpng_apng_stream_t *st = (rpng_apng_stream_t*)ol->anim_stream[i];
      gfx_surface_t *s       = (gfx_surface_t*)ol->surfaces[i];
      const uint32_t *frame;
      int duration_ms        = 0;

      if (!st || !s || !s->num_slots)
         continue;
      /* Two-frame APNGs are driven by desc press state, not time. */
      if (ol->anim_2frame && ol->anim_2frame[i])
         continue;
      if (ol->anim_next_us[i] && now < ol->anim_next_us[i])
         continue;
      /* The last frame is still the video thread's, or the GPU's: this
       * one is dropped rather than waited for, and the next poll tries
       * again. */
      if (s->inflight || !gfx_surface_slot_writable(s, 0))
         continue;

      if (!(frame = rpng_apng_stream_next(st, &duration_ms)))
      {
         /* End of a pass: overlays loop, which is what an animated
          * control surface is for. */
         rpng_apng_stream_rewind(st);
         if (!(frame = rpng_apng_stream_next(st, &duration_ms)))
            continue;
      }
      memcpy(s->slots[0], frame,
            VIDEO_SCALE_AREA(s->dims) * sizeof(uint32_t));
      if (gfx_surface_submit(s, 0, ol->images[i]->supports_rgba)
            == GFX_SURFACE_SUBMIT_FAILED)
         continue;
      ol->anim_next_us[i] = now
         + (retro_time_t)(duration_ms > 0 ? duration_ms : 100) * 1000;
   }
}
#endif

/* ledN_map while the overlay LED driver is the LED driver, else NULL:
 * nothing is hidden. */
static const unsigned *input_overlay_led_map(void)
{
   if (!(input_driver_st.flags & INP_FLAG_OVERLAY_LEDS))
      return NULL;
   return config_get_ptr()->uints.led_map;
}

/**
 * input_overlay_post_poll:
 *
 * Called after all the input_overlay_poll() calls to
 * update alpha mods for pressed/unpressed controls
 **/
static void input_overlay_post_poll(
      input_overlay_t *ol,
      bool show_input, float opacity)
{
   size_t i;

   input_overlay_alpha_pass(ol, opacity, show_input, opacity,
         input_driver_st.overlay_leds_lit, input_overlay_led_map());

   for (i = 0; i < ol->active->size; i++)
   {
      struct overlay_desc *desc = &ol->active->descs[i];

#ifdef HAVE_RPNG
      /* A two-frame APNG shares its press state across every desc
       * that uses the same image. The anim arrays are per unique
       * image of the pack, not per page entry, so they are indexed
       * by pack_image_index rather than image_index. */
      if (     desc->touch_mask != 0
            && ol->anim_2frame
            && OVERLAY_HAS_IMAGE(&desc->image)
            && desc->pack_image_index < ol->num_images
            && ol->anim_2frame[desc->pack_image_index])
         ol->anim_2frame_pressed[desc->pack_image_index] = 1;
#endif

      input_overlay_update_desc_geom(ol, desc);

      desc->old_touch_mask = desc->touch_mask;
      desc->touch_mask     = 0;
   }

#ifdef HAVE_RPNG
   input_overlay_update_2frame(ol);
#endif
}

static void input_overlay_desc_init_hitbox(struct overlay_desc *desc)
{
   desc->x_hitbox       =
         ((desc->x_shift + desc->range_x * desc->reach_right) +
          (desc->x_shift - desc->range_x * desc->reach_left)) / 2.0f;

   desc->y_hitbox       =
         ((desc->y_shift + desc->range_y * desc->reach_down) +
          (desc->y_shift - desc->range_y * desc->reach_up)) / 2.0f;

   desc->range_x_hitbox =
         (desc->range_x * desc->reach_right +
          desc->range_x * desc->reach_left) / 2.0f;

   desc->range_y_hitbox =
         (desc->range_y * desc->reach_down +
          desc->range_y * desc->reach_up) / 2.0f;

   desc->range_x_mod    = desc->range_x_hitbox * desc->range_mod;
   desc->range_y_mod    = desc->range_y_hitbox * desc->range_mod;
}

/**
 * input_overlay_scale:
 * @ol                    : Overlay handle.
 * @layout                : Scale + offset factors.
 *
 * Scales the overlay and all its associated descriptors
 * and applies any aspect ratio/offset factors.
 **/
static void input_overlay_scale(struct overlay *ol,
      const overlay_layout_t *layout)
{
   size_t i;

   ol->mod_w = ol->w * layout->x_scale;
   ol->mod_h = ol->h * layout->y_scale;
   ol->mod_x = (ol->center_x + (ol->x - ol->center_x) *
         layout->x_scale) + layout->x_offset;
   ol->mod_y = (ol->center_y + (ol->y - ol->center_y) *
         layout->y_scale) + layout->y_offset;

   for (i = 0; i < ol->size; i++)
   {
      struct overlay_desc *desc = &ol->descs[i];
      float x_shift_offset      = 0.0f;
      float y_shift_offset      = 0.0f;
      float scale_w;
      float scale_h;
      float adj_center_x;
      float adj_center_y;

      /* Apply 'x separation' factor */
      if (desc->x < (0.5f - 0.0001f))
         x_shift_offset = layout->x_separation * -1.0f;
      else if (desc->x > (0.5f + 0.0001f))
         x_shift_offset = layout->x_separation;

      desc->x_shift     = desc->x + x_shift_offset;

      /* Apply 'y separation' factor */
      if (desc->y < (0.5f - 0.0001f))
         y_shift_offset = layout->y_separation * -1.0f;
      else if (desc->y > (0.5f + 0.0001f))
         y_shift_offset = layout->y_separation;

      desc->y_shift     = desc->y + y_shift_offset;

      scale_w           = ol->mod_w * desc->range_x;
      scale_h           = ol->mod_h * desc->range_y;
      adj_center_x      = ol->mod_x + desc->x_shift * ol->mod_w;
      adj_center_y      = ol->mod_y + desc->y_shift * ol->mod_h;

      desc->mod_w       = 2.0f * scale_w;
      desc->mod_h       = 2.0f * scale_h;
      desc->mod_x       = adj_center_x - scale_w;
      desc->mod_y       = adj_center_y - scale_h;

      input_overlay_desc_init_hitbox(desc);
   }
}

static void input_overlay_parse_layout(
      const struct overlay *ol,
      const overlay_layout_desc_t *layout_desc,
      float display_aspect_ratio,
      overlay_layout_t *overlay_layout)
{
   /* Set default values */
   overlay_layout->x_scale      = 1.0f;
   overlay_layout->y_scale      = 1.0f;
   overlay_layout->x_separation = 0.0f;
   overlay_layout->y_separation = 0.0f;
   overlay_layout->x_offset     = 0.0f;
   overlay_layout->y_offset     = 0.0f;

   /* Perform auto-scaling, if required */
   if (layout_desc->auto_scale)
   {
      /* Sanity check - if scaling is blocked,
       * or aspect ratios are invalid, then we
       * can do nothing */
      if (   (ol->flags & OVERLAY_BLOCK_SCALE)
          || (ol->aspect_ratio <= 0.0f)
          || (display_aspect_ratio <= 0.0f))
         return;

      /* If display is wider than overlay,
       * reduce width */
      if (display_aspect_ratio > ol->aspect_ratio)
      {
         overlay_layout->x_scale = ol->aspect_ratio /
               display_aspect_ratio;

         if (overlay_layout->x_scale <= 0.0f)
         {
            overlay_layout->x_scale = 1.0f;
            return;
         }

         /* If auto-scale X separation is enabled, move elements
          * horizontally towards the edges of the screen */
         if (ol->flags & OVERLAY_AUTO_X_SEPARATION)
            overlay_layout->x_separation = ((1.0f / overlay_layout->x_scale) - 1.0f) * 0.5f;
      }
      /* If display is taller than overlay,
       * reduce height */
      else
      {
         overlay_layout->y_scale = display_aspect_ratio /
               ol->aspect_ratio;

         if (overlay_layout->y_scale <= 0.0f)
         {
            overlay_layout->y_scale = 1.0f;
            return;
         }

         /* If auto-scale Y separation is enabled, move elements
          * vertically towards the edges of the screen */
         if (ol->flags & OVERLAY_AUTO_Y_SEPARATION)
            overlay_layout->y_separation = ((1.0f / overlay_layout->y_scale) - 1.0f) * 0.5f;
      }

      return;
   }

   /* Regular 'manual' scaling/position adjustment
    * > Landscape display orientations */
   if (display_aspect_ratio > 1.0f)
   {
      float scale              = layout_desc->scale_landscape;
      float aspect_adjust      = layout_desc->aspect_adjust_landscape;
      /* Note: Y offsets have their sign inverted,
       * since from a usability perspective positive
       * values should move the overlay upwards */
      overlay_layout->x_offset = layout_desc->x_offset_landscape;
      overlay_layout->y_offset = layout_desc->y_offset_landscape * -1.0f;

      if (!(ol->flags & OVERLAY_BLOCK_X_SEPARATION))
         overlay_layout->x_separation = layout_desc->x_separation_landscape;
      if (!(ol->flags & OVERLAY_BLOCK_Y_SEPARATION))
         overlay_layout->y_separation = layout_desc->y_separation_landscape;

      if (!(ol->flags & OVERLAY_BLOCK_SCALE))
      {
         /* In landscape orientations, aspect correction
          * adjusts the overlay width */
         overlay_layout->x_scale = (aspect_adjust >= 0.0f) ?
               (scale * (aspect_adjust + 1.0f)) :
               (scale / ((aspect_adjust * -1.0f) + 1.0f));
         overlay_layout->y_scale = scale;
      }
   }
   /* > Portrait display orientations */
   else
   {
      float scale              = layout_desc->scale_portrait;
      float aspect_adjust      = layout_desc->aspect_adjust_portrait;

      overlay_layout->x_offset = layout_desc->x_offset_portrait;
      overlay_layout->y_offset = layout_desc->y_offset_portrait * -1.0f;

      if (!(ol->flags & OVERLAY_BLOCK_X_SEPARATION))
         overlay_layout->x_separation = layout_desc->x_separation_portrait;
      if (!(ol->flags & OVERLAY_BLOCK_Y_SEPARATION))
         overlay_layout->y_separation = layout_desc->y_separation_portrait;

      if (!(ol->flags & OVERLAY_BLOCK_SCALE))
      {
         /* In portrait orientations, aspect correction
          * adjusts the overlay height */
         overlay_layout->x_scale = scale;
         overlay_layout->y_scale = (aspect_adjust >= 0.0f) ?
               (scale * (aspect_adjust + 1.0f)) :
               (scale / ((aspect_adjust * -1.0f) + 1.0f));
      }
   }
}

static void input_overlay_set_vertex_geom(input_overlay_t *ol)
{
   size_t i;

   if (!ol->iface->vertex_geom)
      return;

   if (OVERLAY_HAS_IMAGE(&ol->active->image))
      ol->iface->vertex_geom(ol->iface_data, 0,
            ol->active->mod_x, ol->active->mod_y,
            ol->active->mod_w, ol->active->mod_h);

   for (i = 0; i < ol->active->size; i++)
   {
      struct overlay_desc *desc = &ol->active->descs[i];
      if (OVERLAY_HAS_IMAGE(&desc->image))
         ol->iface->vertex_geom(ol->iface_data, desc->image_index,
               desc->mod_x, desc->mod_y, desc->mod_w, desc->mod_h);
   }
}

/**
 * input_overlay_set_scale_factor:
 * @ol                    : Overlay handle.
 * @layout_desc           : Scale + offset factors.
 *
 * Scales the overlay and applies any aspect ratio/
 * offset factors.
 **/
void input_overlay_set_scale_factor(
      input_overlay_t *ol, const overlay_layout_desc_t *layout_desc,
      unsigned output_dims)
{
   size_t i;
   float display_aspect_ratio = 0.0f;

   if (!ol || !layout_desc)
      return;

   if (VIDEO_SCALE_H(output_dims) > 0)
      display_aspect_ratio = (float)VIDEO_SCALE_W(output_dims) /
         (float)VIDEO_SCALE_H(output_dims);

   for (i = 0; i < ol->size; i++)
   {
      struct overlay *current_overlay = &ol->overlays[i];
      overlay_layout_t overlay_layout;

      input_overlay_parse_layout(current_overlay,
            layout_desc, display_aspect_ratio, &overlay_layout);
      input_overlay_scale(current_overlay, &overlay_layout);
   }

   input_overlay_set_vertex_geom(ol);
}

/* The video driver is about to go: every pack's textures, active or
 * cached, are unloaded while it can still do so. A pack whose pixels
 * went to the driver it is losing has nothing to upload again and is
 * reloaded from its path (input_overlay_has_source). */
void input_overlay_video_teardown(void)
{
   input_overlay_release_textures(input_driver_st.overlay_ptr);
   input_overlay_release_textures(input_driver_st.overlay_cache_ptr);
}

static void input_overlay_load_active_geom(
      input_overlay_t *ol, float opacity);

void input_overlay_load_active(input_overlay_t *ol, float opacity)
{
   /* No page in the driver, no per-image state to set on it: the
    * setters would index whatever the driver held before. */
   if (input_overlay_load_page(ol) != INPUT_OVERLAY_PAGE_NONE)
      input_overlay_load_active_geom(ol, opacity);
}

/* The per-page state that follows either load: alpha, geometry,
 * full-screen. */
static void input_overlay_load_active_geom(
      input_overlay_t *ol, float opacity)
{
   input_overlay_set_alpha_mod(ol, opacity);
   input_overlay_set_vertex_geom(ol);

   if (ol->iface->full_screen)
      ol->iface->full_screen(ol->iface_data,
            (ol->active->flags & OVERLAY_FULL_SCREEN));
}

/**
 * input_overlay_next_move_touch_masks
 * @ol : Overlay handle.
 *
 * Finds similar descs in the next overlay (i.e. same location and type)
 * and moves touch masks from active overlay to next.
 */
void input_overlay_next_move_touch_masks(input_overlay_t *ol)
{
   const struct overlay *active = ol->active;
   const struct overlay *next   = ol->overlays + ol->next_index;
   size_t i, j;

   for (i = 0; i < active->size; i++)
   {
      struct overlay_desc *desc = active->descs + i;

      if (desc->old_touch_mask)
      {
         for (j = 0; j < next->size; j++)
         {
            struct overlay_desc *desc2 = next->descs + j;

            if (     desc2->type == desc->type
                  && fabs(desc2->x - desc->x) < 0.01f
                  && fabs(desc2->y - desc->y) < 0.01f)
               desc2->old_touch_mask = desc->old_touch_mask;
         }

         desc->old_touch_mask = 0;
      }
   }
}

/**
 * input_overlay_poll_clear:
 * @ol                    : overlay handle
 *
 * Call when there is nothing to poll. Allows overlay to
 * clear certain state.
 **/
static void input_overlay_poll_clear(
      input_overlay_t *ol, float opacity)
{
   size_t i;

   ol->flags &= ~INPUT_OVERLAY_BLOCKED;

   input_overlay_set_alpha_mod(ol, opacity);

   for (i = 0; i < ol->active->size; i++)
   {
      struct overlay_desc *desc = &ol->active->descs[i];

      desc->old_touch_mask      = desc->touch_mask;
      desc->touch_mask          = 0;

      input_overlay_update_desc_geom(ol, desc);
   }

#ifdef HAVE_RPNG
   input_overlay_update_2frame(ol);
#endif
}

void input_overlay_set_alpha_mod(input_overlay_t *ol, float mod)
{
   input_overlay_alpha_pass(ol, mod, false, mod,
         input_driver_st.overlay_leds_lit, input_overlay_led_map());
}

static void input_overlay_free_images(input_overlay_t *ol)
{
   size_t i;

   if (!ol || !ol->images)
      return;

   for (i = 0; i < ol->num_images; i++)
      image_texture_free(ol->images[i]);

#ifdef HAVE_RPNG
   if (ol->anim_stream)
      for (i = 0; i < ol->num_images; i++)
         if (ol->anim_stream[i])
            rpng_apng_stream_close((rpng_apng_stream_t*)ol->anim_stream[i]);
#endif
   if (ol->anim_data)
      for (i = 0; i < ol->num_images; i++)
         free(ol->anim_data[i]);
   free(ol->anim_stream);
   free(ol->anim_data);
   free(ol->anim_len);
   free(ol->anim_next_us);
   if (ol->anim_2frame_pix)
      for (i = 0; i < ol->num_images; i++)
         free(ol->anim_2frame_pix[i]);
   free(ol->anim_2frame);
   free(ol->anim_2frame_pressed);
   free(ol->anim_2frame_cur);
   free(ol->anim_2frame_pix);
   ol->anim_stream         = NULL;
   ol->anim_data           = NULL;
   ol->anim_len            = NULL;
   ol->anim_next_us        = NULL;
   ol->anim_2frame         = NULL;
   ol->anim_2frame_pressed = NULL;
   ol->anim_2frame_cur     = NULL;
   ol->anim_2frame_pix     = NULL;

   free(ol->images);
   ol->images = NULL;
}

static void input_overlay_free_overlays(input_overlay_t *ol)
{
   size_t i;

   if (!ol || !ol->overlays)
      return;

   for (i = 0; i < ol->size; i++)
      input_overlay_free_overlay(&ol->overlays[i]);

   free(ol->overlays);
   ol->overlays = NULL;
}

void input_overlay_free_overlay(struct overlay *overlay)
{
   size_t i;

   if (!overlay)
      return;

   for (i = 0; i < overlay->size; i++)
   {
      if (overlay->descs[i].eightway_config)
         free(overlay->descs[i].eightway_config);
      overlay->descs[i].eightway_config = NULL;
   }

   if (overlay->load_images)
      free(overlay->load_images);
   overlay->load_images = NULL;
   if (overlay->descs)
      free(overlay->descs);
   overlay->descs       = NULL;
}

/**
 * input_overlay_free:
 * @ol                    : Overlay handle.
 *
 * Frees overlay handle.
 **/
static void input_overlay_free(input_overlay_t *ol)
{
   if (!ol)
      return;

   /* The driver's page refers to the pack's textures: it lets go of
    * them first, before they are unloaded - under threaded video a
    * frame can be drawn between the two calls. */
   if (ol->iface && ol->iface->enable)
      ol->iface->enable(ol->iface_data, false);

   input_overlay_release_textures(ol);
   input_overlay_free_images(ol);

   input_overlay_free_overlays(ol);

   if (ol->path)
   {
      free(ol->path);
      ol->path = NULL;
   }

   /* alpha_want is the second half of the same block. */
   free(ol->alpha_sent);

   free(ol);
}

void input_overlay_auto_rotate_(
      unsigned output_dims,
      bool input_overlay_enable,
      input_overlay_t *ol)
{
   size_t i;
   enum overlay_orientation screen_orientation         = OVERLAY_ORIENTATION_PORTRAIT;
   enum overlay_orientation active_overlay_orientation = OVERLAY_ORIENTATION_NONE;
   bool tmp                                            = false;

   /* Sanity check */
   if (!ol || !(ol->flags & INPUT_OVERLAY_ALIVE) || !input_overlay_enable)
      return;

   /* Get current screen orientation */
   if (VIDEO_SCALE_W(output_dims) > VIDEO_SCALE_H(output_dims))
      screen_orientation = OVERLAY_ORIENTATION_LANDSCAPE;

   /* Get orientation of active overlay */
   if (*ol->active->name)
   {
      if (strstr(ol->active->name, "landscape"))
         active_overlay_orientation = OVERLAY_ORIENTATION_LANDSCAPE;
      else if (strstr(ol->active->name, "portrait"))
         active_overlay_orientation = OVERLAY_ORIENTATION_PORTRAIT;
      else /* Sanity check */
         return;
   }
   else /* Sanity check */
      return;

   /* If screen and overlay have the same orientation,
    * no action is required */
   if (screen_orientation == active_overlay_orientation)
      return;

   /* Attempt to find index of overlay corresponding
    * to opposite orientation */
   for (i = 0; i < ol->active->size; i++)
   {
      overlay_desc_t *desc = &ol->active->descs[i];

      if (!desc)
         continue;

      if (*desc->next_index_name)
      {
         bool next_overlay_found = false;
         if (active_overlay_orientation == OVERLAY_ORIENTATION_LANDSCAPE)
            next_overlay_found = (strstr(desc->next_index_name, "portrait") != 0);
         else
            next_overlay_found = (strstr(desc->next_index_name, "landscape") != 0);

         if (next_overlay_found)
         {
            /* We have a valid target overlay
             * > Trigger 'overly next' command event
             * Note: tmp == false. This prevents CMD_EVENT_OVERLAY_NEXT
             * from calling input_overlay_auto_rotate_() again */
            ol->next_index     = desc->next_index;
            command_event(CMD_EVENT_OVERLAY_NEXT, &tmp);
            break;
         }
      }
   }
}

/**
 * input_overlay_poll_lightgun
 * @settings : pointer to settings
 * @ol : overlay handle
 * @old_ptr_count : previous poll's non-hitbox pointer count
 *
 * Updates multi-touch button state of the overlay lightgun.
 */
static void input_overlay_poll_lightgun(settings_t *settings,
      input_overlay_t *ol, const int old_ptr_count)
{
   input_overlay_pointer_state_t *ptr_st = &ol->pointer_state;
   const int ptr_count                   = ptr_st->count;
   unsigned action                       = OVERLAY_LIGHTGUN_ACTION_NONE;
   int8_t trig_delay                     =
         settings->uints.input_overlay_lightgun_trigger_delay;
   int8_t delay_idx;

   static uint16_t trig_buf;
   static uint8_t now_idx;
   static uint8_t peak_ptr_count;
   static const unsigned action_to_id[OVERLAY_LIGHTGUN_ACTION_END] = {
      RARCH_BIND_LIST_END,
      RARCH_LIGHTGUN_TRIGGER,
      RARCH_LIGHTGUN_RELOAD,
      RARCH_LIGHTGUN_AUX_A,
      RARCH_LIGHTGUN_AUX_B,
      RARCH_LIGHTGUN_AUX_C,
      RARCH_LIGHTGUN_START,
      RARCH_LIGHTGUN_SELECT,
      RARCH_LIGHTGUN_DPAD_UP,
      RARCH_LIGHTGUN_DPAD_DOWN,
      RARCH_LIGHTGUN_DPAD_LEFT,
      RARCH_LIGHTGUN_DPAD_RIGHT
   };

   /* Update peak pointer count */
   if (!old_ptr_count && ptr_count)
      peak_ptr_count = ptr_count;
   else
      peak_ptr_count = MAX(ptr_count, peak_ptr_count);

   /* Apply trigger delay */
   now_idx   = (now_idx + 1) % (OVERLAY_LIGHTGUN_TRIG_MAX_DELAY + 1);
   delay_idx = (now_idx + trig_delay) % (OVERLAY_LIGHTGUN_TRIG_MAX_DELAY + 1);

   if (ptr_count > 0)
      BIT16_SET(trig_buf, delay_idx);
   else
      BIT16_CLEAR(trig_buf, delay_idx);

   /* Create button input if we're past the trigger delay */
   if (BIT16_GET(trig_buf, now_idx))
   {
      switch (peak_ptr_count)
      {
         case 1:
            if (settings->bools.input_overlay_lightgun_trigger_on_touch)
               action = OVERLAY_LIGHTGUN_ACTION_TRIGGER;
            break;
         case 2:
            action = settings->uints.input_overlay_lightgun_two_touch_input;
            break;
         case 3:
            action = settings->uints.input_overlay_lightgun_three_touch_input;
            break;
         case 4:
            action = settings->uints.input_overlay_lightgun_four_touch_input;
            break;
         default:
            break;
      }
   }

   ptr_st->lightgun.multitouch_id = action_to_id[action];
}

static void input_overlay_get_mouse_scale(settings_t *settings,
      float *scale_x, float *scale_y,
      int *swipe_thres_x, int *swipe_thres_y)
{
   const struct retro_game_geometry *geom = video_driver_get_core_geometry();

   if (geom->base_height)
   {
      float adj_x, adj_y;
      unsigned output_size = video_driver_get_output_dims();
      float speed          = settings->floats.input_overlay_mouse_speed;
      float swipe_thres    =
            655.35f * settings->floats.input_overlay_mouse_swipe_threshold;
      float display_aspect = (float)VIDEO_SCALE_W(output_size)
                           / VIDEO_SCALE_H(output_size);
      float core_aspect    = (float)geom->base_width / geom->base_height;

      if (display_aspect > core_aspect)
      {
         adj_x = speed * (display_aspect / core_aspect);
         adj_y = speed;
      }
      else
      {
         adj_y = speed * (core_aspect / display_aspect);
         adj_x = speed;
      }

      *scale_x = (adj_x * geom->base_width) / (float)0x7fff;
      *scale_y = (adj_y * geom->base_height) / (float)0x7fff;

      if (display_aspect > 1.0f)
      {
         *swipe_thres_x = (int)(swipe_thres / display_aspect);
         *swipe_thres_y = (int)swipe_thres;
      }
      else
      {
         *swipe_thres_x = (int)swipe_thres;
         *swipe_thres_y = (int)(swipe_thres / display_aspect);
      }
   }
}

/**
 * input_overlay_poll_mouse
 * @settings : pointer to settings
 * @mouse_st : pointer to overlay mouse state
 * @ol : overlay handle
 * @ptr_count : this poll's non-hitbox pointer count
 * @old_ptr_count : previous poll's non-hitbox pointer count
 *
 * Updates button state of the overlay mouse.
 */
static void input_overlay_poll_mouse(settings_t *settings,
      struct input_overlay_mouse_state *mouse_st,
      input_overlay_t *ol,
      const int ptr_count,
      const int old_ptr_count)
{
   input_overlay_pointer_state_t *ptr_st = &ol->pointer_state;
   const retro_time_t now_usec           = cpu_features_get_time_usec();
   const retro_time_t hold_usec          = settings->uints.input_overlay_mouse_hold_msec * 1000;
   const retro_time_t dtap_usec          = settings->uints.input_overlay_mouse_dtap_msec * 1000;
   const uint8_t alt_2touch              = settings->uints.input_overlay_mouse_alt_two_touch_input;
   int swipe_thres_x                     = 0;
   int swipe_thres_y                     = 0;
   const bool hold_to_drag               = settings->bools.input_overlay_mouse_hold_to_drag;
   const bool dtap_to_drag               = settings->bools.input_overlay_mouse_dtap_to_drag;
   bool want_feedback                    = false;
   bool is_swipe, is_brief, is_long;

   static retro_time_t start_usec;
   static retro_time_t last_down_usec;
   static retro_time_t last_up_usec;
   static retro_time_t pending_click_usec;
   static retro_time_t click_dur_usec;
   static retro_time_t click_end_usec;
   static int x_start;
   static int y_start;
   static int peak_ptr_count;
   static int old_peak_ptr_count;
   static bool check_gestures;
   static bool pending_click;
   static const uint8_t btns[OVERLAY_MAX_TOUCH + 1] =
         {0x0, 0x1, 0x2, 0x4};  /* none, lmb, rmb, mmb */

   input_overlay_get_mouse_scale(settings,
         (float*)&mouse_st->scale_x, &mouse_st->scale_y,
         &swipe_thres_x, &swipe_thres_y);

   /* Check for pointer count changes */
   if (ptr_count != old_ptr_count)
   {
      mouse_st->click = 0;
      pending_click   = false;

      /* Assume main pointer changed. Reset deltas */
      mouse_st->prev_screen_x = x_start = ptr_st->screen_x;
      mouse_st->prev_screen_y = y_start = ptr_st->screen_y;

      if (ptr_count > old_ptr_count)
      {
         /* Pointer added */
         peak_ptr_count = ptr_count;
         start_usec     = now_usec;

         /* Alt 2-touch input. After gesture checks,
          * use 2nd touch as a button */
         if (!check_gestures && ptr_count == 2)
            mouse_st->hold = btns[alt_2touch];
      }
      else
      {
         /* Pointer removed */
         mouse_st->hold = 0;
         if (!ptr_count)
            old_peak_ptr_count = peak_ptr_count;
      }
   }

   /* Action type */
   is_swipe = abs(ptr_st->screen_x - x_start) > swipe_thres_x ||
              abs(ptr_st->screen_y - y_start) > swipe_thres_y;
   is_brief = (now_usec - start_usec) < 200000;
   is_long  = (now_usec - start_usec) > (hold_to_drag ? hold_usec : 250000);

   /* Check if new button input should be created */
   if (check_gestures)
   {
      if (!is_swipe)
      {
         if (     hold_to_drag
               && is_long && ptr_count && !mouse_st->hold)
         {
            /* Long press */
            mouse_st->hold = btns[ptr_count];
            want_feedback  = true;
         }
         else if (is_brief)
         {
            if (ptr_count && !old_ptr_count)
            {
               /* New input. Check for double tap */
               if (     dtap_to_drag
                     && now_usec - last_up_usec < dtap_usec)
                  mouse_st->hold = btns[old_peak_ptr_count];

               last_down_usec = now_usec;
            }
            else if (!ptr_count && old_ptr_count)
            {
               /* Finished a tap. Send click */
               click_dur_usec = (now_usec - last_down_usec) + 5000;

               if (dtap_to_drag)
               {
                  pending_click      = true;
                  pending_click_usec = now_usec + dtap_usec;
               }
               else
               {
                  mouse_st->click    = btns[peak_ptr_count];
                  click_end_usec     = now_usec + click_dur_usec;
               }

               last_up_usec = now_usec;
            }
         }
      }
      else
      {
         /* Swiping. Stop gesture checks and possibly hold a button */
         if (ptr_count > 1)
         {
            if (hold_to_drag && !alt_2touch)
            {
               mouse_st->hold = btns[ptr_count];
               want_feedback = true;
            }
            else if (alt_2touch && !hold_to_drag
                  && ptr_count == 2)
               mouse_st->hold = btns[alt_2touch];
         }
         check_gestures = false;
      }
   }

   /* Check for pending click */
   if (pending_click && now_usec >= pending_click_usec)
   {
      mouse_st->click = (1 << (old_peak_ptr_count - 1));
      click_end_usec  = now_usec + click_dur_usec;
      pending_click   = false;
   }

   if (!ptr_count)
      check_gestures = true;
   else if (is_long)
      check_gestures = false;

   /* Remove stale clicks */
   if (mouse_st->click && now_usec > click_end_usec)
      mouse_st->click = 0;

   if (want_feedback && settings->bools.vibrate_on_keypress)
   {
      input_driver_t *input = input_driver_st.current_driver;
      if (input && input->keypress_vibrate)
         input->keypress_vibrate();
   }
}

/**
 * input_overlay_track_touch_inputs
 * @state : Overlay input state for this poll
 * @old_state : Overlay input state for previous poll
 *
 * Matches current touch inputs to previous poll's, based on distance.
 * Updates old_touch_index_lut and assigns -1 to any new inputs.
 */
static void input_overlay_track_touch_inputs(
      input_overlay_state_t *state, input_overlay_state_t *old_state)
{
   int *const old_index_lut = input_driver_st.old_touch_index_lut;
   int i, j, t, new_idx;
   float x_dist, y_dist, sq_dist, outlier;
   float min_sq_dist[OVERLAY_MAX_TOUCH];

   memset(old_index_lut, -1, sizeof(int) * OVERLAY_MAX_TOUCH);

   /* Compute (squared) distances and match new indexes to old */
   for (i = 0; i < state->touch_count; i++)
   {
      min_sq_dist[i] = 3e8f;

      for (j = 0; j < old_state->touch_count; j++)
      {
         x_dist  = state->touch[i].x - old_state->touch[j].x;
         y_dist  = state->touch[i].y - old_state->touch[j].y;

         sq_dist = x_dist * x_dist + y_dist * y_dist;

         if (sq_dist < min_sq_dist[i])
         {
            min_sq_dist[i]   = sq_dist;
            old_index_lut[i] = j;
         }
      }
   }

   /* If touch_count increased, find the outliers and assign -1 */
   for (t = old_state->touch_count; t < state->touch_count; t++)
   {
      new_idx = OVERLAY_MAX_TOUCH - 1;
      outlier = 0;

      for (i = 0; i < state->touch_count; i++)
         if (min_sq_dist[i] > outlier)
         {
            outlier        = min_sq_dist[i];
            new_idx        = i;
            min_sq_dist[i] = 0;
         }

      old_index_lut[new_idx] = -1;
   }
}

static void input_overlay_update_pointer_coords(
      input_overlay_pointer_state_t *ptr_st, int touch_idx)
{
   void *input_data      = input_driver_st.current_data;
   input_driver_t *input = (input_driver_t*)input_driver_st.current_driver;

   /* Need multi-touch coordinates for pointer only */
   if (     ptr_st->count
         && !(ptr_st->device_mask & (1 << RETRO_DEVICE_POINTER)))
   {
      ptr_st->count++;
      return;
   }

   /* Need viewport pointers for pointer and lightgun */
   if (     ptr_st->device_mask
         & ((1 << RETRO_DEVICE_LIGHTGUN) | (1 << RETRO_DEVICE_POINTER)))
   {
      ptr_st->ptr[ptr_st->count].x  = input->input_state(
            input_data, NULL, NULL, NULL, NULL, true, 0,
            RETRO_DEVICE_POINTER,
            touch_idx,
            RETRO_DEVICE_ID_POINTER_X);
      ptr_st->ptr[ptr_st->count].y  = input->input_state(
            input_data, NULL, NULL, NULL, NULL, true, 0,
            RETRO_DEVICE_POINTER,
            touch_idx,
            RETRO_DEVICE_ID_POINTER_Y);
   }

   /* Need fullscreen pointer for mouse only */
   if (     !ptr_st->count
         && (ptr_st->device_mask & (1 << RETRO_DEVICE_MOUSE)))
   {
      ptr_st->mouse.prev_screen_x = ptr_st->screen_x;
      ptr_st->screen_x            = input->input_state(
            input_data, NULL, NULL, NULL, NULL, true, 0,
            RARCH_DEVICE_POINTER_SCREEN,
            touch_idx,
            RETRO_DEVICE_ID_POINTER_X);
      ptr_st->mouse.prev_screen_y = ptr_st->screen_y;
      ptr_st->screen_y            = input->input_state(
            input_data, NULL, NULL, NULL, NULL, true, 0,
            RARCH_DEVICE_POINTER_SCREEN,
            touch_idx,
            RETRO_DEVICE_ID_POINTER_Y);
   }

   ptr_st->count++;
}

/*
 * input_poll_overlay:
 *
 * Poll pressed buttons/keys on currently active overlay.
 **/
INPUT_NOINLINE static void input_poll_overlay(
      bool keyboard_mapping_blocked,
      settings_t *settings,
      void *ol_data,
      float opacity,
      unsigned analog_dpad_mode,
      float axis_threshold)
{
   input_overlay_state_t old_ol_state;
   int i, j;
   input_overlay_t *ol                      = (input_overlay_t*)ol_data;
   int blocked_touch_idx                    = -1;
   uint16_t key_mod                         = 0;
   uint16_t ptrdev_touch_mask               = 0;
   uint16_t hitbox_touch_mask               = 0;
   bool button_pressed                      = false;
   input_driver_state_t *input_st           = &input_driver_st;
   void *input_data                         = input_st->current_data;
   input_overlay_state_t *ol_state          = &ol->overlay_state;
   input_overlay_pointer_state_t *ptr_state = &ol->pointer_state;
   input_driver_t *input                    = (input_driver_t*)input_st->current_driver;
   enum overlay_show_input_type
         input_overlay_show_inputs          = (enum overlay_show_input_type)
               settings->uints.input_overlay_show_inputs;
   unsigned input_overlay_show_inputs_port  = settings->uints.input_overlay_show_inputs_port;
   float touch_scale                        = (float)settings->uints.input_touch_scale;
   bool ol_ptr_enable                       = settings->bools.input_overlay_pointer_enable;
   bool osk_state_changed                   = false;

   static int old_ptr_count;
   static int old_blocked_touch_idx;
   static int16_t old_ptrdev_touch_mask;
   static int16_t old_hitbox_touch_mask;

   if (!ol_state)
      return;

   memcpy(&old_ol_state, ol_state,
         sizeof(old_ol_state));
   memset(ol_state, 0, sizeof(*ol_state));

   if (ol_ptr_enable)
   {
      old_ptr_count    = ptr_state->count;
      ptr_state->count = 0;
   }

   /* input_data is dereferenced by the driver's pointer paths, so a
    * NULL current_data (mid driver teardown/reinit, e.g. Android
    * surface recreation) must skip driver input for this frame
    * instead of faulting inside the input driver.
    *
    * A stylus-hidden overlay reads no touches either: with none, the
    * rest of this poll releases whatever a finger was holding (button,
    * key, pointing device) the same way lifting it would. */
   if (     input->input_state
         && input_data
         && !(ol->flags & INPUT_OVERLAY_STYLUS_HIDDEN))
   {
      rarch_joypad_info_t joypad_info;
      unsigned device                 = (ol->active->flags & OVERLAY_FULL_SCREEN)
         ? RARCH_DEVICE_POINTER_SCREEN
         : RETRO_DEVICE_POINTER;
      const input_device_driver_t
         *joypad                      = input_st->primary_joypad;
#ifdef HAVE_MFI
      const input_device_driver_t
         *sec_joypad                  = input_st->secondary_joypad;
#else
      const input_device_driver_t
         *sec_joypad                  = NULL;
#endif

      joypad_info.joy_idx             = 0;
      joypad_info.auto_binds          = NULL;
      joypad_info.axis_threshold      = 0.0f;

      /* Get driver input.
       * Bound check first: the PRESSED query must never be issued
       * with idx == OVERLAY_MAX_TOUCH, since drivers index their
       * touch arrays by idx and are not required to range-check it. */
      for (i = 0;
               (i < OVERLAY_MAX_TOUCH)
            && input->input_state(
               input_data,
               joypad,
               sec_joypad,
               &joypad_info,
               NULL,
               keyboard_mapping_blocked,
               0,
               device,
               i,
               RETRO_DEVICE_ID_POINTER_PRESSED);
            i++)
      {
         ol_state->touch[i].x = input->input_state(
               input_data,
               joypad,
               sec_joypad,
               &joypad_info,
               NULL,
               keyboard_mapping_blocked,
               0,
               device,
               i,
               RETRO_DEVICE_ID_POINTER_X);
         ol_state->touch[i].y = input->input_state(
               input_data,
               joypad,
               sec_joypad,
               &joypad_info,
               NULL,
               keyboard_mapping_blocked,
               0,
               device,
               i,
               RETRO_DEVICE_ID_POINTER_Y);
      }
      ol_state->touch_count = i;

      /* Update lookup table of new to old touch indexes */
      input_overlay_track_touch_inputs(ol_state, &old_ol_state);

      /* Poll overlay */
      for (i = 0; i < ol_state->touch_count; i++)
      {
         input_overlay_state_t polled_data;
         int old_i           = input_st->old_touch_index_lut[i];
         bool hitbox_pressed = false;

         if (old_i != -1)
         {
            /* Keep each touch pointer dedicated to the same input type
             * (hitbox or pointing device) from the previous poll */
            if (BIT16_GET(old_hitbox_touch_mask, old_i))
               BIT16_SET(hitbox_touch_mask, i);
            else if (BIT16_GET(old_ptrdev_touch_mask, old_i))
               BIT16_SET(ptrdev_touch_mask, i);

            /* Skip blocked touch pointer and freeze any overlay_next
             * input until the blocked touch is removed */
            if (old_i == old_blocked_touch_idx)
            {
               blocked_touch_idx = i;
               if (BIT256_GET(old_ol_state.buttons, RARCH_OVERLAY_NEXT))
                  BIT256_SET(ol_state->buttons, RARCH_OVERLAY_NEXT);
               continue;
            }
         }

         memset(&polled_data, 0, sizeof(struct input_overlay_state));

         /* Check hitboxes only if this touch pointer
          * is not controlling a pointing device */
         if (   ol->flags & INPUT_OVERLAY_ENABLE
             && !(ol->flags & INPUT_OVERLAY_GAMEPAD_HIDDEN)
             && !BIT16_GET(ptrdev_touch_mask, i))
            hitbox_pressed = input_overlay_poll(
                  ol, &polled_data, i, old_i,
                  ol_state->touch[i].x, ol_state->touch[i].y, touch_scale);
         else
            ol->flags &= ~INPUT_OVERLAY_BLOCKED;

         if (hitbox_pressed)
         {
            /* Block touch pointer if overlay_next was pressed */
            if (BIT256_GET(polled_data.buttons, RARCH_OVERLAY_NEXT))
               blocked_touch_idx = i;

            bits_or_bits(ol_state->buttons.data,
                  polled_data.buttons.data,
                  ARRAY_SIZE(polled_data.buttons.data));

            for (j = 0; j < (int)ARRAY_SIZE(ol_state->keys); j++)
               ol_state->keys[j] |= polled_data.keys[j];

            /* Fingers pressed later take priority and matched up
             * with overlay poll priorities. */
            for (j = 0; j < 4; j++)
               if (polled_data.analog[j])
                  ol_state->analog[j] = polled_data.analog[j];

            hitbox_touch_mask |= (1 << i);
         }
         else if (   ol_ptr_enable
                  && ptr_state->device_mask
                  && !BIT16_GET(hitbox_touch_mask, i)
                  && !(ol->flags & INPUT_OVERLAY_BLOCKED))
         {
            input_overlay_update_pointer_coords(ptr_state, i);
            ptrdev_touch_mask |= (1 << i);
         }
      }
   }

   if (ol_ptr_enable)
   {
      if (ptr_state->device_mask & (1 << RETRO_DEVICE_LIGHTGUN))
         input_overlay_poll_lightgun(settings, ol, old_ptr_count);
      if (ptr_state->device_mask & (1 << RETRO_DEVICE_MOUSE))
         input_overlay_poll_mouse(settings, &ptr_state->mouse, ol,
               ptr_state->count, old_ptr_count);
   }

   if (     OVERLAY_GET_KEY(ol_state, RETROK_LSHIFT)
         || OVERLAY_GET_KEY(ol_state, RETROK_RSHIFT))
      key_mod |= RETROKMOD_SHIFT;

   if (     OVERLAY_GET_KEY(ol_state, RETROK_LCTRL)
         || OVERLAY_GET_KEY(ol_state, RETROK_RCTRL))
      key_mod |= RETROKMOD_CTRL;

   if (     OVERLAY_GET_KEY(ol_state, RETROK_LALT)
         || OVERLAY_GET_KEY(ol_state, RETROK_RALT))
      key_mod |= RETROKMOD_ALT;

   if (     OVERLAY_GET_KEY(ol_state, RETROK_LMETA)
         || OVERLAY_GET_KEY(ol_state, RETROK_RMETA))
      key_mod |= RETROKMOD_META;

   /* CAPSLOCK SCROLLOCK NUMLOCK */
   for (i = (int)ARRAY_SIZE(ol_state->keys); i-- > 0;)
   {
      if (ol_state->keys[i] != old_ol_state.keys[i])
      {
         uint32_t orig_bits = old_ol_state.keys[i];
         uint32_t new_bits  = ol_state->keys[i];
         osk_state_changed  = true;

         for (j = 0; j < 32; j++)
            if ((orig_bits & (1 << j)) != (new_bits & (1 << j)))
            {
               unsigned rk = i * 32 + j;
               uint32_t c  = input_keymaps_translate_rk_to_ascii((enum retro_key)rk, (enum retro_mod)key_mod);
               input_keyboard_event(new_bits & (1 << j),
                     rk, c, key_mod, RETRO_DEVICE_POINTER);
            }
      }
   }

   /* Map "analog" buttons to analog axes like regular input drivers do. */
   for (j = 0; j < 4; j++)
   {
      unsigned bind_plus  = RARCH_ANALOG_LEFT_X_PLUS + 2 * j;
      unsigned bind_minus = bind_plus + 1;

      if (ol_state->analog[j])
         continue;

      if ((BIT256_GET(ol->overlay_state.buttons, bind_plus)))
         ol_state->analog[j] += 0x7fff;
      if ((BIT256_GET(ol->overlay_state.buttons, bind_minus)))
         ol_state->analog[j] -= 0x7fff;
   }

   /* Check for analog_dpad_mode.
    * Map analogs to d-pad buttons when configured. */
   switch (analog_dpad_mode)
   {
      case ANALOG_DPAD_LSTICK:
      case ANALOG_DPAD_RSTICK:
      {
         float analog_x, analog_y;
         unsigned analog_base = 2;

         if (analog_dpad_mode == ANALOG_DPAD_LSTICK)
            analog_base = 0;

         analog_x = (float)ol_state->analog[analog_base + 0] / 0x7fff;
         analog_y = (float)ol_state->analog[analog_base + 1] / 0x7fff;

         if (analog_x <= -axis_threshold)
            BIT256_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_LEFT);
         if (analog_x >=  axis_threshold)
            BIT256_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_RIGHT);
         if (analog_y <= -axis_threshold)
            BIT256_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_UP);
         if (analog_y >=  axis_threshold)
            BIT256_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_DOWN);
         break;
      }

      case ANALOG_DPAD_LRSTICK:
      {
         float analog_x, analog_y;

         analog_x = (float)ol_state->analog[0] / 0x7fff;
         analog_y = (float)ol_state->analog[1] / 0x7fff;

         if (!analog_x)
            analog_x = (float)ol_state->analog[2] / 0x7fff;

         if (!analog_y)
            analog_y = (float)ol_state->analog[3] / 0x7fff;

         if (analog_x <= -axis_threshold)
            BIT256_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_LEFT);
         if (analog_x >=  axis_threshold)
            BIT256_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_RIGHT);
         if (analog_y <= -axis_threshold)
            BIT256_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_UP);
         if (analog_y >=  axis_threshold)
            BIT256_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_DOWN);
         break;
      }

      case ANALOG_DPAD_TWINSTICK:
      {
         float analog_x, analog_y;

         analog_x = (float)ol_state->analog[0] / 0x7fff;
         analog_y = (float)ol_state->analog[1] / 0x7fff;

         if (analog_x <= -axis_threshold)
            BIT256_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_LEFT);
         if (analog_x >=  axis_threshold)
            BIT256_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_RIGHT);
         if (analog_y <= -axis_threshold)
            BIT256_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_UP);
         if (analog_y >=  axis_threshold)
            BIT256_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_DOWN);

         analog_x = (float)ol_state->analog[2] / 0x7fff;
         analog_y = (float)ol_state->analog[3] / 0x7fff;

         if (analog_x <= -axis_threshold)
            BIT256_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_Y);
         if (analog_x >=  axis_threshold)
            BIT256_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_A);
         if (analog_y <= -axis_threshold)
            BIT256_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_X);
         if (analog_y >=  axis_threshold)
            BIT256_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_B);
         break;
      }

      default:
         break;
   }

   button_pressed = input_overlay_add_inputs(ol, ol_state, input_st,
         settings,
         (input_overlay_show_inputs == OVERLAY_SHOW_INPUT_TOUCHED),
         input_overlay_show_inputs_port);

   /* Block other touchscreen input as needed. A stylus-hidden overlay
    * blocks nothing: the pen's pointer state has to reach the core. */
   if (ol->flags & INPUT_OVERLAY_STYLUS_HIDDEN)
      input_st->flags &= ~INP_FLAG_BLOCK_POINTER_INPUT;
   else if (button_pressed
#if TARGET_OS_IPHONE
         || (ptr_state->device_mask & (1 << RETRO_DEVICE_LIGHTGUN))
         || (ol->flags & INPUT_OVERLAY_BLOCKED))
#else
         || ol_ptr_enable)
#endif
      input_st->flags |=  INP_FLAG_BLOCK_POINTER_INPUT;
   else
      input_st->flags &= ~INP_FLAG_BLOCK_POINTER_INPUT;

   if (input_overlay_show_inputs == OVERLAY_SHOW_INPUT_NONE)
      button_pressed = false;

   /* menu_toggle fires on release and cannot tell a lift from a
    * slide-off, so a slide-off must cancel it here. */
   if (ol_state->touch_count)
   {
      int d, t;
      for (d = 0; d < (int)ol->active->size; d++)
      {
         struct overlay_desc *desc = &ol->active->descs[d];

         if (    desc->touch_mask
             || !desc->old_touch_mask
             || !BIT256_GET(desc->button_mask, RARCH_MENU_TOGGLE))
            continue;

         for (t = 0; t < ol_state->touch_count; t++)
         {
            int old_t = input_st->old_touch_index_lut[t];
            if (old_t >= 0 && BIT32_GET(desc->old_touch_mask, old_t))
            {
               input_st->flags |= INP_FLAG_MENU_PRESS_CANCEL;
               break;
            }
         }
      }
   }

   if (button_pressed || ol_state->touch_count)
      input_overlay_post_poll(ol,
            button_pressed, opacity);
   else
      input_overlay_poll_clear(ol, opacity);

   /* Create haptic feedback for any change in button/key state,
    * unless touch_count decreased. */
   if (     input->keypress_vibrate
         && settings->bools.vibrate_on_keypress
         && ol_state->touch_count
         && ol_state->touch_count >= old_ol_state.touch_count)
   {
      if (     osk_state_changed
            || bits_any_different(
                     ol_state->buttons.data,
                     old_ol_state.buttons.data,
                     ARRAY_SIZE(old_ol_state.buttons.data))
         )
         input->keypress_vibrate();
   }

   old_hitbox_touch_mask  = hitbox_touch_mask;
   old_ptrdev_touch_mask  = ptrdev_touch_mask;
   old_blocked_touch_idx  = blocked_touch_idx;
   ptr_state->device_mask = 0;
}
#endif

/**
 * input_config_translate_str_to_rk:
 * @str                            : String to translate to key ID.
 *
 * Translates string representation to key identifier.
 *
 * Returns: key identifier.
 **/
enum retro_key input_config_translate_str_to_rk(const char *str, size_t len)
{
   size_t i;
   if (len == 1 && ISALPHA((int)*str))
      return (enum retro_key)(RETROK_a + (TOLOWER((int)*str) - (int)'a'));
   for (i = 0; input_config_key_map[i].str; i++)
   {
      if (string_is_equal_noncase(input_config_key_map[i].str, str))
         return input_config_key_map[i].key;
   }
   return RETROK_UNKNOWN;
}

/**
 * input_config_translate_str_to_bind_id:
 * @str                            : String to translate to bind ID.
 *
 * Translate string representation to bind ID.
 *
 * Returns: Bind ID value on success, otherwise
 * RARCH_BIND_LIST_END on not found.
 **/
unsigned input_config_translate_str_to_bind_id(const char *str)
{
   unsigned i;

   for (i = 0; input_config_bind_map[i].valid; i++)
      if (string_is_equal(str, input_config_bind_map[i].base))
         return i;

   return RARCH_BIND_LIST_END;
}

/* Every helper below returns the untruncated (would-be) length of what
 * it formatted, so one bind label longer than the destination pushes
 * _len past len, and each later "len - _len" size underflows to a huge
 * size_t: bionic's FORTIFY aborts on it, and the next copy's "s + _len"
 * writes out of bounds everywhere else. Saturate the offset at the last
 * writable byte after every segment. */
#define BIND_STR_CLAMP(_l, _cap) (((_l) < (_cap)) ? (_l) : ((_cap) - 1))

size_t input_config_get_bind_string(
      void *settings_data,
      char *s,
      const struct retro_keybind *bind,
      const struct retro_keybind *auto_bind,
      const struct input_bind_label *label,
      const struct input_bind_label *auto_label,
      size_t len)
{
   settings_t *settings                 = (settings_t*)settings_data;
   size_t _len                          = 0;
   int delim                            = 0;
   bool  input_descriptor_label_show    =
      settings->bools.input_descriptor_label_show;

   if (len == 0)
      return 0;
   *s                                 = '\0';

   if      (bind      && bind->joykey  != NO_BTN)
      _len = input_config_get_bind_string_joykey(
            input_descriptor_label_show,
            s, "", bind, label, len);
   else if (bind      && bind->joyaxis != AXIS_NONE)
      _len = input_config_get_bind_string_joyaxis(
            input_descriptor_label_show,
            s, "", bind, label, len);
   else if (auto_bind && auto_bind->joykey != NO_BTN)
      _len = input_config_get_bind_string_joykey(
            input_descriptor_label_show,
            s, "(Auto)", auto_bind, auto_label, len);
   else if (auto_bind && auto_bind->joyaxis != AXIS_NONE)
      _len = input_config_get_bind_string_joyaxis(
            input_descriptor_label_show,
            s, "(Auto)", auto_bind, auto_label, len);
   _len = BIND_STR_CLAMP(_len, len);

   if (*s)
      delim = 1;

#ifndef RARCH_CONSOLE
   {
      char key[64];
      key[0] = '\0';

      input_keymaps_translate_rk_to_str(RETRO_KEYBIND_KEY(bind), key, sizeof(key));
      if (     key[0] == 'n'
            && key[1] == 'u'
            && key[2] == 'l'
            && key[3] == '\0'
         )
         *key = '\0';
      /*empty?*/
      else if (*key != '\0')
      {
         if (delim)
            _len  = BIND_STR_CLAMP(
                  _len + strlcpy_lit(s + _len, ", ", len - _len), len);
         _len     = BIND_STR_CLAMP(
               _len + snprintf(s + _len, len - _len,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_KEY), key),
               len);
         delim = 1;
      }
   }
#endif

   if (bind->mbutton != NO_BTN)
   {
      int tag = 0;
      switch (bind->mbutton)
      {
         case RETRO_DEVICE_ID_MOUSE_LEFT:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_LEFT;
            break;
         case RETRO_DEVICE_ID_MOUSE_RIGHT:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_RIGHT;
            break;
         case RETRO_DEVICE_ID_MOUSE_MIDDLE:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_MIDDLE;
            break;
         case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON4;
            break;
         case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON5;
            break;
         case RETRO_DEVICE_ID_MOUSE_WHEELUP:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_UP;
            break;
         case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_DOWN;
            break;
         case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_UP;
            break;
         case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_DOWN;
            break;
      }

      if (tag != 0)
      {
         if (delim)
            _len  = BIND_STR_CLAMP(
                  _len + strlcpy_lit(s + _len, ", ", len - _len), len);
         _len     = BIND_STR_CLAMP(
               _len + strlcpy(s + _len,
                     msg_hash_to_str((enum msg_hash_enums)tag), len - _len),
               len);
      }
   }

   /*completely empty?*/
   if (*s == '\0')
      _len = BIND_STR_CLAMP(
            _len + strlcpy(s + _len, RARCH_NO_BIND, len - _len), len);
   return _len;
}

size_t input_config_get_bind_string_joykey(
      bool input_descriptor_label_show,
      char *s, const char *suffix,
      const struct retro_keybind *bind,
      const struct input_bind_label *label, size_t len)
{
   const char *joykey_label = label ? label->joykey : NULL;
   size_t _len = 0;
   if (len == 0)
      return 0;
   if (GET_HAT_DIR(bind->joykey))
   {
      if (      joykey_label
            && (joykey_label && *joykey_label)
            && input_descriptor_label_show)
         return BIND_STR_CLAMP(fill_pathname_join_delim(s,
               joykey_label, suffix, ' ', len), len);
      /* TODO/FIXME - localize */
      _len  = BIND_STR_CLAMP((size_t)snprintf(s, len,
            "Hat #%u ", (unsigned)GET_HAT(bind->joykey)), len);
      switch (GET_HAT_DIR(bind->joykey))
      {
         case HAT_UP_MASK:
            _len  = BIND_STR_CLAMP(
                  _len + strlcpy_lit(s + _len, "Up",    len - _len), len);
            break;
         case HAT_DOWN_MASK:
            _len  = BIND_STR_CLAMP(
                  _len + strlcpy_lit(s + _len, "Down",  len - _len), len);
            break;
         case HAT_LEFT_MASK:
            _len  = BIND_STR_CLAMP(
                  _len + strlcpy_lit(s + _len, "Left",  len - _len), len);
            break;
         case HAT_RIGHT_MASK:
            _len  = BIND_STR_CLAMP(
                  _len + strlcpy_lit(s + _len, "Right", len - _len), len);
            break;
         default:
            _len  = BIND_STR_CLAMP(
                  _len + strlcpy_lit(s + _len, "?",     len - _len), len);
            break;
      }
   }
   else
   {
      if (      joykey_label
            && (joykey_label && *joykey_label)
            && input_descriptor_label_show)
         return BIND_STR_CLAMP(fill_pathname_join_delim(s,
               joykey_label, suffix, ' ', len), len);

      /* TODO/FIXME - localize */
      _len  = BIND_STR_CLAMP(strlcpy_lit(s, "Button ", len), len);
      _len  = BIND_STR_CLAMP(
            _len + snprintf(s + _len, len - _len, "%u",
                  (unsigned)bind->joykey), len);
   }

   if (suffix && *suffix)
      _len  = BIND_STR_CLAMP(
            _len + snprintf(s + _len, len - _len, " %s", suffix), len);

   return _len;
}

size_t input_config_get_bind_string_joyaxis(
      bool input_descriptor_label_show,
      char *s, const char *suffix,
      const struct retro_keybind *bind,
      const struct input_bind_label *label, size_t len)
{
   const char *joyaxis_label = label ? label->joyaxis : NULL;
   size_t _len = 0;
   if (len == 0)
      return 0;
   if (      joyaxis_label
         && (joyaxis_label && *joyaxis_label)
         && input_descriptor_label_show)
      return BIND_STR_CLAMP(fill_pathname_join_delim(s,
            joyaxis_label, suffix, ' ', len), len);

   /* TODO/FIXME - localize */
   _len = BIND_STR_CLAMP(strlcpy_lit(s, "Axis ", len), len);

   if (AXIS_NEG_GET(bind->joyaxis) != AXIS_DIR_NONE)
      _len  = BIND_STR_CLAMP(
            _len + snprintf(s + _len, len - _len, "-%u",
                  (unsigned)AXIS_NEG_GET(bind->joyaxis)), len);
   else if (AXIS_POS_GET(bind->joyaxis) != AXIS_DIR_NONE)
      _len  = BIND_STR_CLAMP(
            _len + snprintf(s + _len, len - _len, "+%u",
                  (unsigned)AXIS_POS_GET(bind->joyaxis)), len);

   if (suffix && *suffix)
      _len  = BIND_STR_CLAMP(
            _len + snprintf(s + _len, len - _len, " %s", suffix), len);

   return _len;
}

void osk_update_last_codepoint(
      unsigned *last_codepoint,
      unsigned *last_codepoint_len,
      const char *word)
{
   const char *letter         = word;
   const char    *pos         = letter;

   if (word[0] == 0)
   {
      *last_codepoint         = 0;
      *last_codepoint_len     = 0;
      return;
   }

   for (;;)
   {
      unsigned codepoint      = utf8_walk(&letter);
      if (letter[0] == 0)
      {
         *last_codepoint      = codepoint;
         *last_codepoint_len  = (unsigned)(letter - pos);
         break;
      }
      pos                     = letter;
   }
}

#ifdef HAVE_LANGEXTRA
/* combine 3 korean elements. make utf8 character */
static unsigned get_kr_utf8(int c1, int c2, int c3)
{
   int  uv = c1 * (28 * 21) + c2 * 28 + c3 + 0xac00;
   int  tv = (uv >> 12) | ((uv & 0x0f00) << 2) | ((uv & 0xc0) << 2) | ((uv & 0x3f) << 16);
   return  (tv | 0x8080e0);
}

/* utf8 korean composition */
static unsigned get_kr_composition(char* pcur, char* padd)
{
   size_t _len;
   static char cc1[] = {"ㄱㄱㄲ ㄷㄷㄸ ㅂㅂㅃ ㅅㅅㅆ ㅈㅈㅉ"};
   static char cc2[] = {"ㅗㅏㅘ ㅗㅐㅙ ㅗㅣㅚ ㅜㅓㅝ ㅜㅔㅞ ㅜㅣㅟ ㅡㅣㅢ"};
   static char cc3[] = {"ㄱㄱㄲ ㄱㅅㄳ ㄴㅈㄵ ㄴㅎㄶ ㄹㄱㄺ ㄹㅁㄻ ㄹㅂㄼ ㄹㅅㄽ ㄹㅌㄾ ㄹㅍㄿ ㄹㅎㅀ ㅂㅅㅄ ㅅㅅㅆ"};
   static char s1[]  = {"ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣㆍㄱㄲㄳㄴㄵㄶㄷㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅄㅅㅆㅇㅈㅊㅋㅌㅍㅎ"};
   char *tmp1        = NULL;
   char *tmp2        = NULL;
   int c1            = -1;
   int c2            = -1;
   int c3            =  0;
   int nv            = -1;
   char utf8[8]      = {0, 0, 0, 0, 0, 0, 0, 0};
   unsigned ret      =  *((unsigned*)pcur);

   /* check korean */
   if (!pcur[0] || !pcur[1] || !pcur[2] || pcur[3])
      return ret;
   if (!padd[0] || !padd[1] || !padd[2] || padd[3])
      return ret;
   if ((tmp1 = strstr(s1, pcur)))
      c1 = (int)((tmp1 - s1) / 3);
   if ((tmp1 = strstr(s1, padd)))
      nv = (int)((tmp1 - s1) / 3);
   if (nv == -1 || nv >= 19 + 21)
      return ret;

   /* single element composition  */
   _len = strlcpy(utf8, pcur, sizeof(utf8));
   strlcpy(utf8 + _len, padd, sizeof(utf8) - _len);

   if ((tmp2 = strstr(cc1, utf8)))
   {
      *((unsigned*)padd) = *((unsigned*)(tmp2 + 6)) & 0xffffff;
      return 0;
   }
   else if ((tmp2 = strstr(cc2, utf8)))
   {
      *((unsigned*)padd) = *((unsigned*)(tmp2 + 6)) & 0xffffff;
      return 0;
   }
   if (tmp2 && tmp2 < cc2 + sizeof(cc2) - 10)
   {
      *((unsigned*)padd) = *((unsigned*)(tmp2 + 6)) & 0xffffff;
      return 0;
   }

   if (c1 >= 19)
      return ret;

   if (c1 == -1)
   {
      int tv = ((pcur[0] & 0x0f) << 12) | ((pcur[1] & 0x3f) << 6) | (pcur[2] & 0x03f);
      tv     = tv  - 0xac00;
      c1     = tv  / (28 * 21);
      c2     = (tv % (28 * 21)) / 28;
      c3     = (tv % (28));
      if (c1 < 0 || c1 >= 19 || c2 < 0 || c2 > 21 || c3 < 0 || c3 > 28)
         return ret;
   }

   if (c1 == -1 && c2 == -1 && c3 == 0)
      return ret;

   if (c2 == -1 && c3 == 0)
   {
      /* 2nd element attach */
      if (nv < 19)
         return ret;
      c2  = nv - 19;
   }
   else
      if (c2 >= 0 && c3 == 0)
      {
         if (nv < 19)
         {
            /* 3rd element attach */
            if (!(tmp1 = strstr(s1 + (19 + 21) * 3, padd)))
               return ret;
            c3 = (int)((tmp1 - s1) / 3 - 19 - 21);
         }
         else
         {
            /* 2nd element transform */
            strlcpy(utf8, s1 + (19 + c2) * 3, 4);
            utf8[3] = 0;
            strlcpy(utf8 + 3, padd, sizeof(utf8) - 3);
            if (    !(tmp2 = strstr(cc2, utf8))
                  || (tmp2 >= cc2 + sizeof(cc2) - 10))
               return ret;
            strlcpy(utf8, tmp2 + 6, 4);
            utf8[3] = 0;
            if (!(tmp1 = strstr(s1 + (19) * 3, utf8)))
               return ret;
            c2 = (int)((tmp1 - s1) / 3 - 19);
         }
      }
      else
         if (c3 > 0)
         {
            strlcpy(utf8, s1 + (19 + 21 + c3) * 3, 4);
            utf8[3] = 0;
            if (nv < 19)
            {
               /* 3rd element transform */
               strlcpy(utf8 + 3, padd, sizeof(utf8) - 3);
               if (    !(tmp2 = strstr(cc3, utf8))
                     || (tmp2 >= cc3 + sizeof(cc3) - 10))
                     return ret;
               strlcpy(utf8, tmp2 + 6, 4);
               utf8[3] = 0;
               if (!(tmp1 = strstr(s1 + (19 + 21) * 3, utf8)))
                  return ret;
               c3 = (int)((tmp1 - s1) / 3 - 19 - 21);
            }
            else
            {
               int tv = 0;
               if ((tmp2 = strstr(cc3, utf8)))
                  tv = (tmp2 - cc3) % 10;
               if (tv == 6)
               {
                  /*  complex 3rd element -> disassemble */
                  strlcpy(utf8, tmp2 - 3, 4);
                  if (!(tmp1 = strstr(s1, utf8)))
                     return ret;
                  tv = (int)((tmp1 - s1) / 3);
                  strlcpy(utf8, tmp2 - 6, 4);
                  if (!(tmp1 = strstr(s1 + (19 + 21) * 3, utf8)))
                     return ret;
                  c3 = (int)((tmp1 - s1) / 3 - 19 - 21);
               }
               else
               {
                  if (!(tmp1 = strstr(s1, utf8)) || (tmp1 - s1) >= 19 * 3)
                     return ret;
                  tv = (int)((tmp1 - s1) / 3);
                  c3 = 0;
               }
               *((unsigned*)padd) = get_kr_utf8(tv, nv - 19, 0);
               return get_kr_utf8(c1, c2, c3);
            }
         }
         else
            return ret;
   *((unsigned*)padd) = get_kr_utf8(c1, c2, c3);
   return 0;
}
#endif

/**
 * input_keyboard_line_event:
 * @state                    : Input keyboard line handle.
 * @character                : Inputted character.
 *
 * Called on every keyboard character event.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
static bool input_keyboard_line_event(
      input_driver_state_t *input_st,
      input_keyboard_line_t *state, uint32_t character)
{
   char array[2];
   bool            ret         = false;
   const char            *word = NULL;
   char            c           = (character >= 128) ? '?' : character;
#ifdef HAVE_LANGEXTRA
   static uint32_t composition = 0;
   /* reset composition, when edit box is opened. */
   if (state->size == 0)
      composition = 0;
   /* reset composition, when 1 byte(=english) input */
   if (character && character < 0xff)
      composition = 0;
   if (IS_COMPOSITION(character) || IS_END_COMPOSITION(character))
   {
      size_t _len = strlen((char*)&composition);
      if (composition && state->buffer && state->size >= _len && state->ptr >= _len)
      {
         memmove(state->buffer + state->ptr - _len, state->buffer + state->ptr, _len + 1);
         state->ptr  -= _len;
         state->size -= _len;
      }
      if (IS_COMPOSITION_KR(character) && composition)
      {
         unsigned new_comp;
         character   = character & 0xffffff;
         new_comp    = get_kr_composition((char*)&composition, (char*)&character);
         if (new_comp)
            input_keyboard_line_append(state, (char*)&new_comp, 3);
         composition = character;
      }
      else
      {
         if (IS_END_COMPOSITION(character))
            composition = 0;
         else
            composition = character & 0xffffff;
         character     &= 0xffffff;
      }
      if (character)
         input_keyboard_line_append(state, (char*)&character, strlen((char*)&character));
      word = state->buffer;
   }
   else
#endif

   /* Treat extended chars as ? as we cannot support
    * printable characters for unicode stuff. */
   if (c == '\r' || c == '\n')
   {
      state->cb(state->userdata, state->buffer);

      array[0] = c;
      array[1] = 0;

      ret      = true;
      word     = array;
   }
   else if (c == '\b' || c == '\x7f') /* 0x7f is ASCII for del */
   {
      if (state->ptr && state->buffer)
      {
         size_t ptr = state->ptr;

         while (state->ptr)
         {
            state->ptr--;
            if (!IS_UTF8_CONTINUATION(state->buffer[state->ptr]))
               break;
         }

         memmove(state->buffer + state->ptr,
               state->buffer + ptr,
               state->size - ptr + 1);
         state->size -= ptr - state->ptr;

         word     = state->buffer;
      }
   }
   else if (ISPRINT(c))
   {
      char *newbuf = (char*)
         realloc(state->buffer, state->size + 2);
      if (!newbuf)
         return false;

      memmove(newbuf + state->ptr + 1,
            newbuf + state->ptr,
            state->size - state->ptr + 1);
      newbuf[state->ptr] = c;
      state->ptr++;
      state->size++;
      newbuf[state->size] = '\0';

      state->buffer = newbuf;

      array[0] = c;
      array[1] = 0;

      word     = array;
   }

   /* OSK - update last character */
   if (word)
      osk_update_last_codepoint(
            &input_st->osk_last_codepoint,
            &input_st->osk_last_codepoint_len,
            word);

   return ret;
}

bool input_osk_native_active(void)
{
   /* Drivers that can report the panel state themselves raise the
    * flag from their poll, on the main thread. Everything below is a
    * backend whose panel state lives outside the input driver. */
   if (input_state_get_ptr()->flags & INP_FLAG_NATIVE_KB_SHOWN)
      return true;
#ifdef HAVE_MIST
   if (steam_has_osk_open())
      return true;
#endif
#ifdef HAVE_COCOATOUCH
   if (ios_keyboard_active())
      return true;
#endif
   return false;
}

bool input_osk_native_available(void)
{
   /* No Steam arm here: steam_has_osk_open() answers whether the
    * panel is up, which is not the same question, and the Steam OSK
    * has no cheap availability query. */
   if (input_state_get_ptr()->flags & INP_FLAG_NATIVE_KB_AVAIL)
      return true;
#ifdef HAVE_COCOATOUCH
   return true;
#else
   return false;
#endif
}

void input_event_osk_append(
      input_keyboard_line_t *keyboard_line,
      enum osk_type *osk_idx,
      unsigned *osk_last_codepoint,
      unsigned *osk_last_codepoint_len,
      int ptr,
      bool show_symbol_pages,
      const char *word,
      size_t len)
{
#ifdef HAVE_LANGEXTRA
   if (memcmp(word, "\xe2\x87\xa6", 4) == 0) /* backspace character */
      input_keyboard_event(true, '\x7f', '\x7f', 0, RETRO_DEVICE_KEYBOARD);
   else if (memcmp(word, "\xe2\x8f\x8e", 4) == 0) /* return character */
      input_keyboard_event(true, '\n', '\n', 0, RETRO_DEVICE_KEYBOARD);
   else if (memcmp(word, "\xe2\x87\xa7", 4) == 0) /* up arrow */
      *osk_idx = OSK_UPPERCASE_LATIN;
   else if (memcmp(word, "\xe2\x87\xa9", 4) == 0) /* down arrow */
      *osk_idx = OSK_LOWERCASE_LATIN;
   else if (memcmp(word, "\xe2\x8a\x95", 4) == 0) /* plus sign (next button) */
   {
      if (*msg_hash_get_uint(MSG_HASH_USER_LANGUAGE) == RETRO_LANGUAGE_KOREAN)
      {
         static int prv_osk = OSK_TYPE_UNKNOWN + 1;
         if (*osk_idx < OSK_KOREAN_PAGE1)
         {
            prv_osk = *osk_idx;
            *osk_idx = OSK_KOREAN_PAGE1;
         }
         else
            *osk_idx = (enum osk_type)prv_osk;
      }
      else if (*osk_idx < (show_symbol_pages ? OSK_TYPE_LAST - 1 : OSK_SYMBOLS_PAGE1))
         *osk_idx = (enum osk_type)(*osk_idx + 1);
      else
         *osk_idx = (enum osk_type)(OSK_TYPE_UNKNOWN + 1);
   }
   else if (*osk_idx == OSK_KOREAN_PAGE1 && word && len == 3)
   {
      unsigned character = *((unsigned*)word) | 0x01000000;
      input_keyboard_line_event(&input_driver_st, keyboard_line, character);
   }
#else
   if (memcmp(word, "Bksp", 5) == 0)
      input_keyboard_event(true, '\x7f', '\x7f', 0, RETRO_DEVICE_KEYBOARD);
   else if (memcmp(word, "Enter", 6) == 0)
      input_keyboard_event(true, '\n', '\n', 0, RETRO_DEVICE_KEYBOARD);
   else if (memcmp(word, "Upper", 6) == 0)
      *osk_idx = OSK_UPPERCASE_LATIN;
   else if (memcmp(word, "Lower", 6) == 0)
      *osk_idx = OSK_LOWERCASE_LATIN;
   else if (memcmp(word, "Next", 5) == 0)
   {
      if (*osk_idx < (show_symbol_pages ? OSK_TYPE_LAST - 1 : OSK_SYMBOLS_PAGE1))
         *osk_idx = (enum osk_type)(*osk_idx + 1);
      else
         *osk_idx = (enum osk_type)(OSK_TYPE_UNKNOWN + 1);
   }
#endif
   else
   {
      input_keyboard_line_append(keyboard_line, word, len);
      osk_update_last_codepoint(
            osk_last_codepoint,
            osk_last_codepoint_len,
            word);
   }
}

void *input_driver_init_wrap(input_driver_t *input, const char *name)
{
   void *ret = NULL;
   if (!input || !input->init)
      return NULL;
   /* the keyboards and mice listed are the last driver's; the one
    * starting lists its own, if it can tell them apart */
   input_config_clear_keyboard_display_names();
   input_config_clear_mouse_info();
   /* and so are the keyboards the ports' pins are looked up among */
   memset(input_driver_st.keyboard_identity, 0,
         sizeof(input_driver_st.keyboard_identity));
   memset(input_driver_st.keyboard_choice, 0,
         sizeof(input_driver_st.keyboard_choice));
   input_driver_st.keyboard_identities = 0;
   input_driver_st.keyboard_absent     = 0;
   memset(input_driver_st.mouse_identity, 0,
         sizeof(input_driver_st.mouse_identity));
   input_driver_st.mouse_identities    = 0;
   input_driver_st.mouse_pinned        = 0;
   input_driver_st.mouse_absent        = 0;
   if ((ret = input->init(name)))
   {
      input_driver_init_joypads();
      return ret;
   }
   return NULL;
}

bool input_driver_find_driver(settings_t *settings,
      const char *prefix, bool verbosity_enabled)
{
   int i = (int)driver_find_index("input_driver",
         settings->arrays.input_driver);

   if (i >= 0)
   {
      input_driver_st.current_driver = (input_driver_t*)input_drivers[i];
      RARCH_LOG("[Input] Found %s: \"%s\".\n", prefix,
            input_driver_st.current_driver->ident);
   }
   else
   {
      input_driver_t *tmp = NULL;
      if (verbosity_enabled)
      {
         unsigned d;
         RARCH_ERR("Couldn't find any %s named \"%s\"\n", prefix,
               settings->arrays.input_driver);
         RARCH_LOG_OUTPUT("Available %ss are:\n", prefix);
         for (d = 0; input_drivers[d]; d++)
            RARCH_LOG_OUTPUT("\t%s\n", input_drivers[d]->ident);
         RARCH_WARN("Going to default to first %s...\n", prefix);
      }

      tmp = (input_driver_t*)input_drivers[0];
      if (!tmp)
         return false;
      input_driver_st.current_driver = tmp;
   }

   return true;
}

void input_mapper_reset(void *data)
{
   unsigned i;
   input_mapper_t *handle = (input_mapper_t*)data;

   for (i = 0; i < MAX_USERS; i++)
   {
      unsigned j;
      for (j = 0; j < 8; j++)
      {
         handle->analog_value[i][j]           = 0;
         handle->buttons[i].data[j]           = 0;
         handle->buttons[i].analogs[j]        = 0;
      }
      for (j = 0; j < ARRAY_SIZE(handle->buttons[i].analog_buttons); j++)
         handle->buttons[i].analog_buttons[j] = 0;
   }
   for (i = 0; i < RETROK_LAST; i++)
      handle->key_button[i]         = 0;
   for (i = 0; i < (RETROK_LAST / 32 + 1); i++)
      handle->keys[i]               = 0;
}

/**
 * Sets the sensor state. Used by RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE.
 *
 * @param port
 * @param action
 * @param rate
 *
 * @return true if the sensor state has been successfully set
 **/
bool input_set_sensor_state(unsigned port,
      enum retro_sensor_action action, unsigned rate)
{
   settings_t *settings        = config_get_ptr();
   /* Default to enabled if settings not loaded yet (matches config default) */
   bool input_sensors_enable   = settings
      ? settings->bools.input_sensors_enable
      : true;
   unsigned joy_idx            = settings
      ? settings->uints.input_joypad_index[port]
      : port;
   return input_driver_set_sensor(
      joy_idx, input_sensors_enable, action, rate);
}

/* Core-facing wrappers that track which sensors the core has enabled.
 * Used as the retro_sensor_interface callbacks so the frontend knows
 * what the core requested independently of shader/platform enables. */
bool input_core_set_sensor_state(unsigned port,
      enum retro_sensor_action action, unsigned rate)
{
   input_driver_state_t *input_st = &input_driver_st;

   switch (action)
   {
      case RETRO_SENSOR_ACCELEROMETER_ENABLE:
         input_st->core_accel_rate = rate;
         break;
      case RETRO_SENSOR_ACCELEROMETER_DISABLE:
         input_st->core_accel_rate = 0;
         break;
      case RETRO_SENSOR_GYROSCOPE_ENABLE:
         input_st->core_gyro_rate = rate;
         break;
      case RETRO_SENSOR_GYROSCOPE_DISABLE:
         input_st->core_gyro_rate = 0;
         break;
      default:
         break;
   }

   /* pass through to the driver directly. */
   return input_set_sensor_state(port, action, rate);
}

float input_core_get_sensor_state(unsigned port, unsigned id)
{
   input_driver_state_t *input_st = &input_driver_st;

   /* Only return data for sensor types the core has enabled */
   if (id <= RETRO_SENSOR_ACCELEROMETER_Z && !input_st->core_accel_rate)
      return 0.0f;
   if (  id >= RETRO_SENSOR_GYROSCOPE_X
      && id <= RETRO_SENSOR_GYROSCOPE_Z
      && !input_st->core_gyro_rate)
      return 0.0f;

   return input_get_sensor_state(port, id);
}

const char *joypad_driver_name(unsigned i)
{
   if (!input_driver_st.primary_joypad || !input_driver_st.primary_joypad->name)
      return NULL;
   return input_driver_st.primary_joypad->name(i);
}

static void input_rumble_forget(void);
static uint32_t input_driver_detect_settings(const settings_t *settings);

/* The joypad driver, restarted because controllers came or went.
 *
 * On Windows that is asked for from the window procedure - a device
 * notification, then a timer - and with threaded video the window
 * procedure runs on the video thread. The restart destroys the joypad
 * driver and its pad tables, starts the registry over and has the
 * driver report every controller again: on that thread it did all of
 * that while the frontend's thread could be in the middle of a poll,
 * reading the tables being freed.
 *
 * Asked for on another thread, it is only noted, and the next
 * input_driver_poll() - the frontend's thread, before it polls
 * anything - does it. Asked for on the frontend's own thread it is
 * done at once, as before. */
#ifdef HAVE_THREADS
/* 0: none due. 1: due. 2: due, and the driver is given the input
 * driver's data, as the caller gave it. */
static retro_atomic_int_t input_joypad_reinit_due;
#endif

static void joypad_driver_reinit_now(void *data,
      const char *joypad_driver_name);

void joypad_driver_reinit(void *data, const char *joypad_driver_name)
{
#ifdef HAVE_THREADS
   if (!task_is_on_main_thread())
   {
      retro_atomic_store_release_int(&input_joypad_reinit_due,
            data ? 2 : 1);
      return;
   }
#endif
   joypad_driver_reinit_now(data, joypad_driver_name);
}

#ifdef HAVE_THREADS
/* The poll's: a restart another thread asked for. The driver is the
 * one the setting names - what the callers pass. True if one was
 * done, and the joypad drivers are not the ones they were. */
static bool joypad_driver_reinit_take(void)
{
   int due = retro_atomic_load_relaxed_int(&input_joypad_reinit_due);

   if (!due)
      return false;
   due = retro_atomic_exchange_int(&input_joypad_reinit_due, 0);
   if (!due)
      return false;
   RARCH_DBG("[Input] Controllers changed: the joypad driver is restarted"
         " by the poll.\n");
   joypad_driver_reinit_now(
         (due == 2) ? input_driver_st.current_data : NULL,
         config_get_ptr()->arrays.input_joypad_driver);
   return true;
}
#endif

static void joypad_driver_reinit_now(void *data,
      const char *joypad_driver_name)
{
   input_rumble_forget();

   if (input_driver_st.primary_joypad)
   {
      const input_device_driver_t *tmp  = input_driver_st.primary_joypad;
      input_driver_st.primary_joypad    = NULL;
      /* Run poll one last time in order to detect disconnections */
      tmp->poll();
      tmp->destroy();
   }
#ifdef HAVE_MFI
   if (input_driver_st.secondary_joypad)
   {
      const input_device_driver_t *tmp  = input_driver_st.secondary_joypad;
      input_driver_st.secondary_joypad  = NULL;
      tmp->poll();
      tmp->destroy();
   }
#endif
   if (!input_driver_st.primary_joypad)
   {
      input_driver_registry_restart();
      strlcpy(input_driver_st.joypad_setting_at_init,
            joypad_driver_name ? joypad_driver_name : "",
            sizeof(input_driver_st.joypad_setting_at_init));
      input_driver_st.detect_settings_at_init =
            input_driver_detect_settings(config_get_ptr());
      input_driver_st.primary_joypad    = input_joypad_init_driver(joypad_driver_name, data);
   }
}

/**
 * Retrieves the sensor state associated with the provided port and ID.
 *
 * @param port
 * @param id    Sensor ID
 *
 * @return The current state associated with the port and ID as a float
 **/
/**
 * Internal version that accepts a pre-resolved settings pointer,
 * avoiding repeated config_get_ptr() calls when invoked in a batch
 * (e.g. the per-frame sensor cache update in input_driver_poll).
 */
static float input_get_sensor_state_internal(
      settings_t *settings, unsigned port, unsigned id)
{
   float raw_value;
   unsigned joy_idx;
   bool input_sensors_enable = false;
   float sensitivity         = 1.0f;
   unsigned fetch_id         = id;
   float sign                = 1.0f;

   /* Return 0 if settings unavailable (e.g., during driver transitions) */
   if (!settings)
      return 0.0f;

   if (port >= MAX_USERS)
      return 0.0f;

   input_sensors_enable = settings->bools.input_sensors_enable;
   joy_idx              = settings->uints.input_joypad_index[port];

   /* Return 0 when sensors disabled */
   if (!input_sensors_enable)
      return 0.0f;

   /* Apply sensor axis remap from autoconfig profile */
   if (id <= RETRO_SENSOR_GYROSCOPE_Z)
   {
      const input_sensor_map_t *map = input_config_get_sensor_map(joy_idx);
      if (map && map->axes[id].source >= 0)
      {
         fetch_id = (unsigned)map->axes[id].source;
         sign     = (float)map->axes[id].sign;
      }
   }

   /* Get sensitivity for sensor type */
   if (id >= RETRO_SENSOR_ACCELEROMETER_X && id <= RETRO_SENSOR_ACCELEROMETER_Z)
      sensitivity = settings->floats.input_sensor_accelerometer_sensitivity;
   else if (id >= RETRO_SENSOR_GYROSCOPE_X && id <= RETRO_SENSOR_GYROSCOPE_Z)
      sensitivity = settings->floats.input_sensor_gyroscope_sensitivity;

   /* Apply sensor orientation rotation for X and Y axes.
    * Skip on iOS/tvOS — cocoa_input handles rotation at the driver level.
    * Setting values: 0=Auto, 1=0°, 2=90°, 3=180°, 4=270°
    * Rotation transforms (clockwise):
    * 0°:   (x, y)   -> (x, y)
    * 90°:  (x, y)   -> (y, -x)
    * 180°: (x, y)   -> (-x, -y)
    * 270°: (x, y)   -> (-y, x)
    * Both accelerometer and gyroscope use the same orientation setting
    * since they share the same physical sensor orientation */
#ifndef HAVE_COCOATOUCH
   if (id == RETRO_SENSOR_ACCELEROMETER_X || id == RETRO_SENSOR_ACCELEROMETER_Y ||
       id == RETRO_SENSOR_GYROSCOPE_X || id == RETRO_SENSOR_GYROSCOPE_Y)
   {
      unsigned orientation_setting = settings->uints.input_sensor_orientation;
      unsigned rotation = 0;
      bool is_gyro = (id == RETRO_SENSOR_GYROSCOPE_X || id == RETRO_SENSOR_GYROSCOPE_Y);
      bool is_x_axis = (id == RETRO_SENSOR_ACCELEROMETER_X || id == RETRO_SENSOR_GYROSCOPE_X);

      if (orientation_setting == 0)
      {
         /* Auto: detect from platform */
#ifdef ANDROID
         if (g_android)
            rotation = g_android->detected_screen_rotation;
#endif
      }
      else
      {
         /* Manual: setting 1=0°, 2=90°, 3=180°, 4=270° */
         rotation = orientation_setting - 1;
      }

      switch (rotation)
      {
         case 1: /* 90° CW: X->Y, Y->-X */
            if (is_x_axis)
               fetch_id = is_gyro ? RETRO_SENSOR_GYROSCOPE_Y : RETRO_SENSOR_ACCELEROMETER_Y;
            else
            {
               fetch_id = is_gyro ? RETRO_SENSOR_GYROSCOPE_X : RETRO_SENSOR_ACCELEROMETER_X;
               sign    *= -1.0f;
            }
            break;
         case 2: /* 180°: X->-X, Y->-Y */
            sign *= -1.0f;
            break;
         case 3: /* 270° CW: X->-Y, Y->X */
            if (is_x_axis)
            {
               fetch_id = is_gyro ? RETRO_SENSOR_GYROSCOPE_Y : RETRO_SENSOR_ACCELEROMETER_Y;
               sign    *= -1.0f;
            }
            else
               fetch_id = is_gyro ? RETRO_SENSOR_GYROSCOPE_X : RETRO_SENSOR_ACCELEROMETER_X;
            break;
         default: /* 0°: no change */
            break;
      }
   }
#endif /* HAVE_COCOATOUCH */

   /* Get raw sensor value (use fetch_id for rotated axes) */
   raw_value = input_driver_get_sensor(joy_idx, true, fetch_id);

   /* Apply sensitivity and rotation sign */
   return raw_value * sensitivity * sign;
}

float input_get_sensor_state(unsigned port, unsigned id)
{
   return input_get_sensor_state_internal(config_get_ptr(), port, id);
}

void input_sensor_start_rest_capture(void)
{
   input_driver_state_t *input_st = &input_driver_st;
   input_st->rest_accum[0]       = 0.0f;
   input_st->rest_accum[1]       = 0.0f;
   input_st->rest_accum[2]       = 0.0f;
   input_st->rest_sample_count   = 0;
   input_st->rest_capturing      = true;
}

static void input_sensor_update_rest_capture(settings_t *settings)
{
   input_driver_state_t *input_st = &input_driver_st;

   if (!input_st->rest_capturing)
      return;

   input_st->rest_accum[0] += input_get_sensor_state_internal(settings, 0, RETRO_SENSOR_ACCELEROMETER_X);
   input_st->rest_accum[1] += input_get_sensor_state_internal(settings, 0, RETRO_SENSOR_ACCELEROMETER_Y);
   input_st->rest_accum[2] += input_get_sensor_state_internal(settings, 0, RETRO_SENSOR_ACCELEROMETER_Z);
   input_st->rest_sample_count++;

   if (input_st->rest_sample_count >= 30)
   {
      input_st->sensor_accelerometer_rest[0] = input_st->rest_accum[0] / 30.0f;
      input_st->sensor_accelerometer_rest[1] = input_st->rest_accum[1] / 30.0f;
      input_st->sensor_accelerometer_rest[2] = input_st->rest_accum[2] / 30.0f;
      input_st->rest_capturing = false;
   }
}

/* The device registry, kept in step with what the joypad drivers
 * report. A mirror for now: it names each controller and recognises
 * one that comes back, and the log says which; ports are assigned as
 * before. */
static input_registry_t input_registry;

const struct input_registry *input_driver_get_registry(void)
{
   return &input_registry;
}

/* Ports put back after a driver restart are a runtime matter. The
 * setting they are written to, input_joypad_index, is also what the
 * config file saves, and a saved exchange would outlive the session
 * that needed it: the next start enumerates afresh and the players
 * would come up exchanged. So what the user configured is kept beside
 * the setting, and the config file is given that.
 *
 * input_ports_runtime is the setting as the last restore left it. If
 * the setting no longer matches, something else has written it since -
 * the menu, a reserved device - and what it holds is the user's again. */
static unsigned input_ports_configured[MAX_USERS];
static unsigned input_ports_runtime[MAX_USERS];
static bool     input_ports_runtime_active;

/* A driver reports its controllers again as it starts, or within a
 * moment of it. One that turns up long afterwards was plugged in or
 * woken by somebody, and moving players around then - perhaps an hour
 * into a game - is not putting things back, it is a surprise. */
#define INPUT_REGISTRY_RESTART_USEC 10000000
static retro_time_t input_registry_restart_time;

unsigned input_config_get_saved_joypad_index(unsigned port)
{
   settings_t *settings = config_get_ptr();

   if (port >= MAX_USERS)
      return 0;
   if (     input_ports_runtime_active
         && !memcmp(settings->uints.input_joypad_index,
               input_ports_runtime, sizeof(input_ports_runtime)))
      return input_ports_configured[port];
   return settings->uints.input_joypad_index[port];
}

void input_driver_registry_restart(void)
{
   unsigned i;
   uint8_t port_of_slot[MAX_USERS];
   settings_t *settings = config_get_ptr();

   memset(port_of_slot, INPUT_REGISTRY_NO_PORT, sizeof(port_of_slot));
   for (i = 0; i < MAX_USERS; i++)
      if (settings->uints.input_joypad_index[i] < MAX_USERS)
         port_of_slot[settings->uints.input_joypad_index[i]] = (uint8_t)i;

   input_registry_restart(&input_registry, port_of_slot, MAX_USERS);
   input_registry_restart_time = cpu_features_get_time_usec();
}

/* Every controller the driver had before it restarted is back. */
static void input_driver_registry_restore_ports(void)
{
   unsigned i;
   unsigned moved;
   settings_t *settings = config_get_ptr();

   /* A reserved device is the user's own word on who goes where, and
    * it is applied on every connect. Leave ports to it. */
   for (i = 0; i < MAX_USERS; i++)
   {
      if (settings->uints.input_device_reservation_type[i]
            != INPUT_DEVICE_RESERVATION_NONE)
      {
         input_registry_restart_cancel(&input_registry);
         return;
      }
   }

   /* The mapping has to be one-to-one for exchanges to keep it so. */
   input_config_sanitize_joypad_indices();

   if (     !input_ports_runtime_active
         || memcmp(settings->uints.input_joypad_index,
               input_ports_runtime, sizeof(input_ports_runtime)))
      memcpy(input_ports_configured, settings->uints.input_joypad_index,
            sizeof(input_ports_configured));

   moved = input_registry_restore_ports(&input_registry,
         settings->uints.input_joypad_index, MAX_USERS);

   memcpy(input_ports_runtime, settings->uints.input_joypad_index,
         sizeof(input_ports_runtime));
   input_ports_runtime_active = memcmp(input_ports_runtime,
         input_ports_configured, sizeof(input_ports_runtime)) != 0;

   if (moved)
      RARCH_LOG("[Input] The joypad driver restarted and reported its"
            " controllers in other slots: each is back on the port it"
            " had.\n");
}

void input_driver_registry_connect(unsigned slot, const char *provider,
      const char *name, const char *phys, uint16_t vid, uint16_t pid)
{
   bool returned                    = false;
   input_device_handle_t handle;
   const input_device_record_t *rec;

   if (     input_registry.restarting
         && cpu_features_get_time_usec() - input_registry_restart_time
               > INPUT_REGISTRY_RESTART_USEC)
      input_registry_restart_cancel(&input_registry);

   handle = input_registry_connect(
         &input_registry, provider, slot, name, phys, vid, pid, &returned);
   rec    = input_registry_get(&input_registry, handle);

   input_snapshot_forget_pad(slot);

   if (rec)
      RARCH_LOG("[Input] Device %u, \"%s\" (%04x:%04x, %s): %s slot %u.\n",
            (unsigned)rec->id, rec->name, rec->vid, rec->pid, rec->provider,
            returned ? "back, in" : "new, in", slot);

   if (input_registry_restart_complete(&input_registry))
      input_driver_registry_restore_ports();
}

void input_driver_registry_disconnect(unsigned slot)
{
   const input_device_record_t *rec = input_registry_at_slot(
         &input_registry, slot);

   if (rec)
      RARCH_LOG("[Input] Device %u, \"%s\": left slot %u.\n",
            (unsigned)rec->id, rec->name, slot);
   input_registry_disconnect(&input_registry, slot);
}

/* A core's rumble calls, held until the frame has run (see
 * input_output_store.h). One slot per port and motor. */
#define INPUT_RUMBLE_SLOT(port, effect) ((port) * 2 + (unsigned)(effect))

static output_store_t     input_rumble_store;
/* What the driver answered the last time a slot was written: 0 not
 * written yet, 1 false, 2 true. */
static retro_atomic_int_t input_rumble_answer[MAX_USERS * 2];

static void input_rumble_write(unsigned slot, int value, void *userdata)
{
   settings_t *settings = (settings_t*)userdata;
   unsigned port        = slot / 2;
   bool ok              = input_driver_set_rumble(port,
         settings->uints.input_joypad_index[port],
         (enum retro_rumble_effect)(slot & 1), (uint16_t)value);

   retro_atomic_store_release_int(&input_rumble_answer[slot], ok ? 2 : 1);
}

/**
 * Sets the rumble state. Used by RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE.
 *
 * The strength is stored, not written: a core calls this from inside
 * retro_run(), often every frame and sometimes several times in one,
 * and each call used to be a driver write on the core's own stack.
 * input_driver_flush_rumble() writes the frame's last value once the
 * core has run.
 *
 * @param port      User number.
 * @param effect    Rumble effect.
 * @param strength  Strength of rumble effect.
 *
 * @return what the driver answered the last time this motor was
 * written; before the first write, whether a driver could take it.
 **/
bool input_set_rumble_state(unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   const input_device_driver_t *primary_joypad;
   const input_device_driver_t *sec_joypad;
   settings_t *settings     = config_get_ptr();
   uint16_t scaled_strength = strength;
   unsigned slot;
   int answer;

   /* Both come from the core. */
   if (     port >= MAX_USERS
         || (     effect != RETRO_RUMBLE_STRONG
               && effect != RETRO_RUMBLE_WEAK))
      return false;

   primary_joypad = input_driver_st.primary_joypad;
   sec_joypad     = input_driver_st.secondary_joypad;

   /* If gain setting is not supported, do software gain control */
   if (primary_joypad)
   {
      if (!primary_joypad->set_rumble_gain)
      {
         unsigned rumble_gain = settings->uints.input_rumble_gain;
         scaled_strength      = (rumble_gain * strength) / 100.0;
      }
   }

   slot = INPUT_RUMBLE_SLOT(port, effect);
   output_store_post(&input_rumble_store, slot, scaled_strength);

   answer = retro_atomic_load_acquire_int(&input_rumble_answer[slot]);
   if (answer)
      return answer == 2;
   return    (primary_joypad && primary_joypad->set_rumble)
          || (sec_joypad     && sec_joypad->set_rumble);
}

/* Write what the core's rumble calls left this frame: one driver call
 * for each motor it set, with the last strength. Main thread, with the
 * core off the stack. */
void input_driver_flush_rumble(void)
{
   if (output_store_pending(&input_rumble_store))
      output_store_take(&input_rumble_store, input_rumble_write,
            config_get_ptr());
}

/* Stop every motor now, and forget what was waiting to be written.
 * Used by CMD_EVENT_RUMBLE_STOP. */
void input_driver_stop_rumble(void)
{
   unsigned i;
   settings_t *settings = config_get_ptr();

   output_store_drop(&input_rumble_store);

   for (i = 0; i < MAX_USERS; i++)
   {
      unsigned joy_idx = settings->uints.input_joypad_index[i];
      input_driver_set_rumble(i, joy_idx, RETRO_RUMBLE_STRONG, 0);
      input_driver_set_rumble(i, joy_idx, RETRO_RUMBLE_WEAK, 0);
   }
}

/* The joypad driver is going away: nothing stored is for the next
 * one, and its answers are not the next one's either. */
static void input_rumble_forget(void)
{
   unsigned i;

   output_store_drop(&input_rumble_store);
   for (i = 0; i < MAX_USERS * 2; i++)
      retro_atomic_store_release_int(&input_rumble_answer[i], 0);
}

/**
 * Sets the rumble gain. Used by MENU_ENUM_LABEL_INPUT_RUMBLE_GAIN.
 *
 * @param gain  Rumble gain, 0-100 [%]
 *
 * @return true if the rumble gain has been successfully set
 **/
bool input_set_rumble_gain(unsigned gain)
{
   return (input_driver_set_rumble_gain(
            gain, config_get_ptr()->uints.input_max_users));
}

/* What each port reads is worked out again: after the list of
 * keyboards was made, and after a port's setting was changed. */
static void input_keyboard_pins_resolve(void)
{
   unsigned port;
   input_driver_state_t *input_st = &input_driver_st;
   settings_t *settings           = config_get_ptr();
   unsigned listed                = input_st->keyboard_identities;

   if (!settings)
      return;

   /* A number from before pins - or one typed into the configuration
    * - is taken at its word, and the port pinned to what it names. */
   for (port = 0; port < MAX_USERS; port++)
   {
      unsigned idx = settings->uints.input_keyboard_index[port];
      if (     !settings->arrays.input_keyboard_device[port][0]
            && idx >= 1 && idx <= listed
            && input_st->keyboard_identity[idx - 1][0])
         strlcpy(settings->arrays.input_keyboard_device[port],
               input_st->keyboard_identity[idx - 1],
               sizeof(settings->arrays.input_keyboard_device[port]));
   }

   input_pins_resolve(input_st->keyboard_choice,
         (const char (*)[INPUT_PIN_LEN])settings->arrays.input_keyboard_device,
         settings->uints.input_keyboard_index, MAX_USERS,
         (const char (*)[INPUT_PIN_LEN])input_st->keyboard_identity, listed);

   input_st->keyboard_absent = 0;
   for (port = 0; port < MAX_USERS; port++)
   {
      int8_t choice = input_st->keyboard_choice[port];
      if (choice >= 1)
      {
         /* the number shown follows the keyboard */
         if (settings->uints.input_keyboard_index[port] != (unsigned)choice)
         {
            RARCH_LOG("[Input] Port %u's keyboard \"%s\" is keyboard %d now.\n",
                  port + 1, settings->arrays.input_keyboard_device[port], choice);
            settings->uints.input_keyboard_index[port] = (unsigned)choice;
         }
      }
      else if (settings->arrays.input_keyboard_device[port][0])
      {
         if (!(input_st->keyboard_absent & (1 << port)))
            RARCH_LOG("[Input] Port %u's keyboard \"%s\" is not there: the port"
                  " reads %s.\n", port + 1,
                  settings->arrays.input_keyboard_device[port],
                  choice == INPUT_PIN_NONE
                  ? "no keyboard, as another port has its own"
                  : "every keyboard");
         input_st->keyboard_absent |= (uint16_t)(1 << port);
      }
   }
}

void input_keyboard_pins_set_devices(const char (*base)[64], unsigned n)
{
   input_driver_state_t *input_st = &input_driver_st;

   if (n > MAX_INPUT_DEVICES)
      n = MAX_INPUT_DEVICES;
   memset(input_st->keyboard_identity, 0, sizeof(input_st->keyboard_identity));
   if (n)
      input_pins_identities(input_st->keyboard_identity, base, n);
   input_st->keyboard_identities = n;
   input_st->keyboard_absent     = 0;
   input_keyboard_pins_resolve();
}

int input_keyboard_port_choice(unsigned port)
{
   return (port < MAX_USERS) ? input_driver_st.keyboard_choice[port] : 0;
}

bool input_keyboard_pin_absent(unsigned port)
{
   return port < MAX_USERS
      && (input_driver_st.keyboard_absent & (1 << port));
}

void input_keyboard_pin_from_index(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   settings_t *settings           = config_get_ptr();
   unsigned idx;

   if (!settings || port >= MAX_USERS)
      return;
   idx = settings->uints.input_keyboard_index[port];
   /* every keyboard: no pin. A listed one: that one. A number past the
    * list, with nothing to name: no pin either, and the number stays
    * as the setting it always was. */
   settings->arrays.input_keyboard_device[port][0] = '\0';
   if (idx >= 1 && idx <= input_st->keyboard_identities)
      strlcpy(settings->arrays.input_keyboard_device[port],
            input_st->keyboard_identity[idx - 1],
            sizeof(settings->arrays.input_keyboard_device[port]));
   input_st->keyboard_absent &= (uint16_t)~(1 << port);
   input_keyboard_pins_resolve();
}

/* Which mouse each pinned port reads is worked out again: after the
 * list of mice was made, and after a port's setting was changed. */
static void input_mouse_pins_resolve(void)
{
   unsigned port;
   int16_t choice[MAX_USERS];
   input_driver_state_t *input_st = &input_driver_st;
   settings_t *settings           = config_get_ptr();
   uint16_t was_absent            = input_st->mouse_absent;

   if (!settings)
      return;

   input_pins_resolve_mice(choice,
         (const char (*)[INPUT_PIN_LEN])settings->arrays.input_mouse_device,
         settings->uints.input_mouse_index, MAX_USERS,
         (const char (*)[INPUT_PIN_LEN])input_st->mouse_identity,
         input_st->mouse_identities);

   input_st->mouse_pinned = 0;
   input_st->mouse_absent = 0;
   for (port = 0; port < MAX_USERS; port++)
   {
      const char *pin = settings->arrays.input_mouse_device[port];
      unsigned k;

      input_st->mouse_choice[port] = choice[port];
      if (!pin[0])
         continue;
      input_st->mouse_pinned |= (uint16_t)(1 << port);

      for (k = 0; k < input_st->mouse_identities; k++)
         if (     input_st->mouse_identity[k][0]
               && string_is_equal(input_st->mouse_identity[k], pin))
            break;
      if (k < input_st->mouse_identities)
      {
         /* the number shown follows the mouse */
         if (settings->uints.input_mouse_index[port] != k)
         {
            RARCH_LOG("[Input] Port %u's mouse \"%s\" is mouse %u now.\n",
                  port + 1, pin, k + 1);
            settings->uints.input_mouse_index[port] = k;
         }
      }
      else
      {
         if (!(was_absent & (1 << port)))
            RARCH_LOG("[Input] Port %u's mouse \"%s\" is not there: the port"
                  " reads %s.\n", port + 1, pin,
                  choice[port] == INPUT_PIN_NO_MOUSE
                  ? "no mouse, as another port has its own"
                  : "the mouse its Mouse Index names");
         input_st->mouse_absent |= (uint16_t)(1 << port);
      }
   }
}

void input_mouse_pins_set_devices(const char (*base)[64], unsigned n)
{
   input_driver_state_t *input_st = &input_driver_st;

   if (n > MAX_INPUT_DEVICES)
      n = MAX_INPUT_DEVICES;
   memset(input_st->mouse_identity, 0, sizeof(input_st->mouse_identity));
   if (n)
      input_pins_identities(input_st->mouse_identity, base, n);
   input_st->mouse_identities = n;
   input_st->mouse_absent     = 0;
   input_mouse_pins_resolve();
}

unsigned input_mouse_port_index(unsigned port)
{
   const input_driver_state_t *input_st = &input_driver_st;
   if (port >= MAX_USERS)
      return MAX_INPUT_DEVICES;
   /* a port with no pin reads by its setting, as it always did - and
    * so does every port under a driver that does not say what its
    * mice are */
   if (!(input_st->mouse_pinned & (1 << port)))
      return config_get_ptr()->uints.input_mouse_index[port];
   if (input_st->mouse_choice[port] < 0)
      return MAX_INPUT_DEVICES;
   return (unsigned)input_st->mouse_choice[port];
}

bool input_mouse_pin_absent(unsigned port)
{
   return port < MAX_USERS
      && (input_driver_st.mouse_absent & (1 << port));
}

void input_mouse_pin_from_index(unsigned port, bool pin)
{
   input_driver_state_t *input_st = &input_driver_st;
   settings_t *settings           = config_get_ptr();
   unsigned idx;

   if (!settings || port >= MAX_USERS)
      return;
   idx = settings->uints.input_mouse_index[port];
   settings->arrays.input_mouse_device[port][0] = '\0';
   if (     pin
         && idx < input_st->mouse_identities
         && input_st->mouse_identity[idx][0])
      strlcpy(settings->arrays.input_mouse_device[port],
            input_st->mouse_identity[idx],
            sizeof(settings->arrays.input_mouse_device[port]));
   input_st->mouse_absent &= (uint16_t)~(1 << port);
   input_mouse_pins_resolve();
}

const char *input_driver_get_ident(void)
{
   const input_driver_t *input = input_driver_st.current_driver;
   return (input && input->ident) ? input->ident : "";
}

uint64_t input_driver_get_capabilities(void)
{
   const input_driver_t *input = input_driver_st.current_driver;
   if (!input || !input->get_capabilities)
      return 0;
   return input->get_capabilities(input_driver_st.current_data);
}

/* The settings read when a controller is reported and given a port:
 * whether controllers are configured at all, where their profiles
 * are, how many ports there are, and which controller each port is
 * reserved for. A joypad driver reports its controllers when it
 * starts, so a change to any of these is applied by starting it
 * again. One number for the lot, to tell a change by. */
static uint32_t input_driver_detect_settings(const settings_t *settings)
{
   unsigned i;
   const char *p;
   uint32_t h = 2166136261u;
#define DETECT_MIX(v) do { h ^= (uint32_t)(v); h *= 16777619u; } while (0)
   DETECT_MIX(settings->bools.input_autodetect_enable ? 1 : 0);
   DETECT_MIX(settings->uints.input_max_users);
   for (p = settings->paths.directory_autoconfig; *p; p++)
      DETECT_MIX((unsigned char)*p);
   for (i = 0; i < MAX_USERS; i++)
   {
      DETECT_MIX(settings->uints.input_device_reservation_type[i]);
      for (p = settings->arrays.input_reserved_devices[i]; *p; p++)
         DETECT_MIX((unsigned char)*p);
      DETECT_MIX(0xff);
   }
#undef DETECT_MIX
   return h;
}

void input_driver_init_joypads(void)
{
   settings_t                   *settings    = config_get_ptr();
   if (!input_driver_st.primary_joypad)
   {
      /* the driver reports its controllers afresh from here */
      input_driver_registry_restart();
      input_driver_st.kept_not_next = false;
      strlcpy(input_driver_st.joypad_setting_at_init,
            settings->arrays.input_joypad_driver,
            sizeof(input_driver_st.joypad_setting_at_init));
      input_driver_st.detect_settings_at_init =
            input_driver_detect_settings(settings);
      input_driver_st.primary_joypad        = input_joypad_init_driver(
         settings->arrays.input_joypad_driver,
         input_driver_st.current_data);
   }
}

bool input_key_pressed(int key, bool keyboard_pressed)
{
   /* If a keyboard key is pressed then immediately return
    * true, otherwise call button_is_pressed to determine
    * if the input comes from another input device */
   if (!((key < RARCH_BIND_LIST_END) && keyboard_pressed))
   {
      const input_device_driver_t
         *joypad                     = (const input_device_driver_t*)
         INPUT_JOYPAD_FOR_READ(&input_driver_st,
               input_driver_st.primary_joypad);
      const uint64_t bind_joykey     = input_config_binds[0][key].joykey;
      const uint64_t bind_joyaxis    = input_config_binds[0][key].joyaxis;
      const uint64_t autobind_joykey = input_autoconf_binds[0][key].joykey;
      const uint64_t autobind_joyaxis= input_autoconf_binds[0][key].joyaxis;
      uint16_t port                  = 0;
      float axis_threshold           = config_get_ptr()->floats.input_axis_threshold;
      const uint64_t joykey          = (bind_joykey != NO_BTN)
         ? bind_joykey  : autobind_joykey;
      const uint64_t joyaxis         = (bind_joyaxis != AXIS_NONE)
         ? bind_joyaxis : autobind_joyaxis;

      if ((uint16_t)joykey != NO_BTN && joypad->button(
               port, (uint16_t)joykey))
         return true;
      if (joyaxis != AXIS_NONE &&
            ((float)abs(joypad->axis(port, (uint32_t)joyaxis))
             * INV_0x8000) > axis_threshold)
         return true;
      return false;
   }
   return true;
}

/* What the video driver needs of the input driver, as calls into here.
 * gfx/ used to take input_state_get_ptr() and work on the fields
 * itself: free the driver and the joypads from its own teardown, hand
 * out the addresses of current_driver and current_data, read and set
 * flags. It still starts the input driver - a video driver's init is
 * handed the two slots below and may fill them - but through these. */

retro_time_t input_driver_get_poll_time(void)
{
   return input_driver_st.poll_time_us;
}

uint32_t input_driver_get_flags(void)
{
   return input_driver_st.flags;
}

input_driver_t *input_driver_get_current(void)
{
   return input_driver_st.current_driver;
}

/* The kind of window the video driver being started says it put up
 * (input_driver_left_to_frontend()), for video_driver_init_input(). */
static enum input_window_kind input_window_for_video = INPUT_WINDOW_OTHER;
/* and the input state it holds for that window, if it holds any
 * (input_driver_left_to_frontend_with()) */
static void *input_window_data_for_video             = NULL;

/* The slots a video driver's init fills in when it brings its own
 * input driver. */
input_driver_t **input_driver_video_slots(void ***data_slot)
{
   /* the driver about to start has said nothing yet */
   input_window_for_video      = INPUT_WINDOW_OTHER;
   input_window_data_for_video = NULL;
   *data_slot = (void**)&input_driver_st.current_data;
   return &input_driver_st.current_driver;
}

void input_driver_left_to_frontend(enum input_window_kind window,
      input_driver_t **input, void **input_data)
{
   input_window_for_video      = window;
   input_window_data_for_video = NULL;
   if (input)
      *input                   = NULL;
   if (input_data)
      *input_data              = NULL;
}

#ifdef HAVE_SDL3
bool input_driver_is_sdl3(void)
{
   return input_driver_st.current_driver == &input_sdl3;
}
#endif

void input_driver_left_to_frontend_with(enum input_window_kind window,
      void *window_data,
      input_driver_t **input, void **input_data)
{
   input_driver_left_to_frontend(window, input, input_data);
   input_window_data_for_video = window_data;
}

/* Leaving the input driver running across a video driver restart
 * ---------------------------------------------------------------
 * The input driver has always been freed by the video driver's
 * teardown and started again by its start-up, joypad drivers and all:
 * a content load, a fullscreen toggle that restarts, a video setting
 * changed, and every controller is found, named and configured again.
 * It was that way because the video driver made the input driver and
 * some input drivers held the video driver's window or lived on its
 * thread.
 *
 * A driver that holds nothing of the video driver's can say so
 * (input_driver_t::survives_video), and is then left alone when the
 * video driver is only being restarted:
 *
 *   driver_uninit()            asks here, before the video driver is
 *                              freed, whether to keep it
 *   input_driver_free_with_video()
 *                              frees nothing if it was kept
 *   the video driver's init    gets the kept driver back wherever it
 *                              would have started one
 *                              (input_driver_take_kept(), from the
 *                              functions in input_driver_choice.c and
 *                              from video_driver_init_input())
 *   drivers_init()             frees it after all if nothing took it
 *                              back (input_driver_drop_kept())
 *
 * A restart here is any teardown the video driver comes straight back
 * from: a reinit, and content loaded or closed
 * (DRIVER_LIFETIME_SESSION_SWITCH).
 *
 * It is not kept when the drivers are being shut down rather than
 * restarted; when the input or the joypad driver setting, or a
 * setting that decides which port a controller gets, has changed
 * since it started, since a restart is how such a change is applied;
 * when its data is the video driver's own; once, after the
 * controllers' configuration has been reset while running
 * (input_driver_restart_with_next_video_restart()); or with
 * RETROARCH_INPUT_KEEP=0 in the environment.
 *
 * What the restart did for the driver besides: the mouse grab is let
 * go here, because it is the old window's, and taken again by the
 * video driver's start-up as it always is. */
void input_driver_keep_for_video_restart(bool restart,
      const void *video_data)
{
   input_driver_state_t *input_st = &input_driver_st;
   settings_t *settings           = config_get_ptr();
   input_driver_t *drv            = input_st->current_driver;
   void *data                     = input_st->current_data;

   input_st->kept_driver          = NULL;
   input_st->kept_data            = NULL;

   if (     !restart
         || !drv
         || !data
         || data == video_data
         || !drv->survives_video
         || !input_st->primary_joypad)
      return;
   if (input_st->kept_not_next)
      return;
   /* RETROARCH_INPUT_KEEP=0 in the environment: every restart
    * restarts the input driver, as it used to, should something need
    * telling apart from this. */
   {
      const char *env = getenv("RETROARCH_INPUT_KEEP");
      if (env && env[0] == '0')
         return;
   }
   if (     !string_is_equal(settings->arrays.input_driver, drv->ident)
         || !string_is_equal(settings->arrays.input_joypad_driver,
               input_st->joypad_setting_at_init))
      return;
   /* The settings that decide a controller's port are not what they
    * were when the controllers were given theirs - a config override
    * came or went with the content, or they were changed in the menu:
    * the joypad driver starts again and reports them afresh. */
   if (     input_driver_detect_settings(settings)
         != input_st->detect_settings_at_init)
   {
      RARCH_DBG("[Input] The settings that give controllers their ports"
            " have changed: the input driver restarts with the video"
            " driver.\n");
      return;
   }
   if (!drv->survives_video(data))
      return;

   if (     (input_st->flags & INP_FLAG_GRAB_MOUSE_STATE)
         && drv->grab_mouse)
      drv->grab_mouse(data, false);

   input_st->kept_driver          = drv;
   input_st->kept_data            = data;
}

/* The controllers are configured when the joypad driver reports them,
 * which it does when it starts. Whatever throws that configuration
 * away while the drivers run - the menu's reset of the configuration
 * to its defaults - used to have it back at the next restart of the
 * drivers. So that it still does, the next restart is not one the
 * input driver is kept across. */
void input_driver_restart_with_next_video_restart(void)
{
   input_driver_st.kept_not_next = true;
}

/* The kept driver, for whoever would otherwise start one. False if
 * none was kept. */
bool input_driver_take_kept(input_driver_t **input, void **input_data)
{
   input_driver_state_t *input_st = &input_driver_st;
   if (!input_st->kept_driver)
      return false;
   *input                         = input_st->kept_driver;
   *input_data                    = input_st->kept_data;
   input_st->kept_driver          = NULL;
   input_st->kept_data            = NULL;
   return true;
}

/* Nothing took the kept driver back: it and the joypad drivers go, as
 * they would have with the video driver. */
void input_driver_drop_kept(void)
{
   input_driver_state_t *input_st = &input_driver_st;
   input_driver_t *drv            = input_st->kept_driver;
   void *data                     = input_st->kept_data;

   if (!drv)
      return;
   input_st->kept_driver          = NULL;
   input_st->kept_data            = NULL;

   if (drv->free)
      drv->free(data);
   if (input_st->primary_joypad)
   {
      const input_device_driver_t *tmp   = input_st->primary_joypad;
      input_st->primary_joypad           = NULL;
      tmp->destroy();
   }
#ifdef HAVE_MFI
   if (input_st->secondary_joypad)
   {
      const input_device_driver_t *tmp   = input_st->secondary_joypad;
      input_st->secondary_joypad         = NULL;
      tmp->destroy();
   }
#endif
   if (input_st->current_data == data)
      input_st->current_data = NULL;
}

/* The video driver is going away. Unless the input driver's data is
 * the video driver's own (@video_data), or the driver is being kept
 * across a restart, the input driver and the joypad drivers go with
 * it. */
void input_driver_free_with_video(const void *video_data)
{
   input_driver_state_t *input_st = &input_driver_st;

   if (input_st->current_data == video_data)
      return;

   if (input_st->kept_driver)
   {
      input_st->flags &= ~INP_FLAG_KB_MAPPING_BLOCKED;
      return;
   }

   if (input_st->current_driver)
      if (input_st->current_driver->free)
         input_st->current_driver->free(input_st->current_data);
   if (input_st->primary_joypad)
   {
      const input_device_driver_t *tmp   = input_st->primary_joypad;
      input_st->primary_joypad           = NULL;
      tmp->destroy();
   }
#ifdef HAVE_MFI
   if (input_st->secondary_joypad)
   {
      const input_device_driver_t *tmp   = input_st->secondary_joypad;
      input_st->secondary_joypad         = NULL;
      tmp->destroy();
   }
#endif
   input_st->flags       &= ~INP_FLAG_KB_MAPPING_BLOCKED;
   input_st->current_data = NULL;
}

void input_pointer_capture_apply(bool force)
{
   input_driver_state_t *input_st = &input_driver_st;
   bool want_grab = (input_st->capture_reasons
         & ~INPUT_CAPTURE_FULLSCREEN_CURSOR) != 0;
   bool want_hide = input_st->capture_reasons != 0;
   bool grabbed   = (input_st->flags & INP_FLAG_GRAB_MOUSE_STATE) != 0;

   /* a driver that has just started is told what is wanted of it, and
    * is not told to let go of what it never had */
   if (want_grab != grabbed || (force && want_grab))
   {
      if (want_grab && input_driver_grab_mouse())
         input_st->flags |=  INP_FLAG_GRAB_MOUSE_STATE;
      else
      {
         if (!want_grab)
            input_driver_ungrab_mouse();
         input_st->flags &= ~INP_FLAG_GRAB_MOUSE_STATE;
      }
      RARCH_DBG("[Input] %s => %s (reasons 0x%02x)\n",
            msg_hash_to_str(MSG_GRAB_MOUSE_STATE),
            (input_st->flags & INP_FLAG_GRAB_MOUSE_STATE) ? "ON" : "OFF",
            (unsigned)input_st->capture_reasons);
   }
   if (     want_hide != input_st->capture_cursor_hidden
         || (force && want_hide))
   {
      video_driver_show_mouse(!want_hide);
      input_st->capture_cursor_hidden = want_hide;
   }
}

void input_pointer_capture_hold(unsigned reasons)
{
   input_driver_st.capture_reasons |= (uint8_t)reasons;
   input_pointer_capture_apply(false);
}

void input_pointer_capture_release(unsigned reasons)
{
   input_driver_st.capture_reasons &= (uint8_t)~reasons;
   input_pointer_capture_apply(false);
}

void input_pointer_capture_set_fullscreen(bool fullscreen, bool exclusive)
{
   input_driver_state_t *input_st = &input_driver_st;
   input_st->capture_reasons &= (uint8_t)~(INPUT_CAPTURE_FULLSCREEN
         | INPUT_CAPTURE_FULLSCREEN_CURSOR);
   if (fullscreen)
      input_st->capture_reasons |= INPUT_CAPTURE_FULLSCREEN_CURSOR
         | (exclusive ? INPUT_CAPTURE_FULLSCREEN : 0);
}

bool input_pointer_capture_toggle(void)
{
   input_driver_state_t *input_st = &input_driver_st;
   const input_driver_t *input    = (const input_driver_t*)
      input_st->current_driver;

   if (!input || !input->grab_mouse)
      return false;
   if (input_st->flags & INP_FLAG_GRAB_MOUSE_STATE)
      input_st->capture_reasons  = 0;
   else
      input_st->capture_reasons |= INPUT_CAPTURE_USER;
   input_pointer_capture_apply(false);
   return true;
}

unsigned input_pointer_capture_reasons(void)
{
   return input_driver_st.capture_reasons;
}

bool video_driver_init_input(
      input_driver_t *tmp,
      settings_t *settings,
      bool verbosity_enabled)
{
   enum input_window_kind window = input_window_for_video;
   void              *new_data = NULL;
   input_driver_t    **input   = &input_driver_st.current_driver;

   /* A driver kept across this restart that the video driver's init
    * did not take back itself. If the video driver started one of its
    * own all the same, that is the input driver and the kept one goes;
    * otherwise the kept one is it. */
   if (input_driver_st.kept_driver)
   {
      if (     *input
            && input_driver_st.current_data
            && input_driver_st.current_data != input_driver_st.kept_data)
      {
         input_driver_t *kept = input_driver_st.kept_driver;
         void *kept_data      = input_driver_st.kept_data;
         input_driver_st.kept_driver = NULL;
         input_driver_st.kept_data   = NULL;
         if (kept->free)
            kept->free(kept_data);
      }
      else
      {
         input_driver_take_kept(input,
               (void**)&input_driver_st.current_data);
         return true;
      }
   }

   if (*input)
#if HAVE_TEST_DRIVERS
      /* Test driver not in use, keep selected driver */
      if (strcmp(settings->arrays.input_driver, "test") != 0)
         return true;
      else if (!*settings->paths.test_input_file_general)
      {
         RARCH_LOG("[Input] Test input driver selected, but no input file provided - falling back.\n");
         return true;
      }
      else
         RARCH_LOG("[Video] Graphics driver initialized an input driver, but ignoring it as test input driver is in use.\n");
#else
      return true;
#endif
   else
   {
      /* The video drivers and contexts of the window systems used to
       * start the input driver for their window themselves, each from
       * its own start-up - on the video thread, with threaded video.
       * They say what kind of window it is
       * (input_driver_left_to_frontend()) and it is started here, by
       * the frontend, on its own thread. The test driver, when it is
       * the setting and has a file to play, is left to the setting
       * below as it was. */
      if (window != INPUT_WINDOW_OTHER
#if HAVE_TEST_DRIVERS
            && !(   strcmp(settings->arrays.input_driver, "test") == 0
                 && *settings->paths.test_input_file_general)
#endif
         )
      {
         input_driver_t *drv = NULL;
         void *drv_data      = NULL;
         const char *joypad  = settings->arrays.input_joypad_driver;

         switch (window)
         {
#if defined(_WIN32) || defined(_XBOX) || defined(__WINRT__)
            case INPUT_WINDOW_WINDOWS:
               input_driver_init_windows(joypad, &drv, &drv_data);
               break;
#endif
#ifdef HAVE_X11
            case INPUT_WINDOW_X11:
               input_driver_init_x11(joypad, &drv, &drv_data);
               break;
#endif
            case INPUT_WINDOW_KMS:
               /* starts one only when the setting is "x" or "udev";
                * any other is the setting's to name, below */
               input_driver_init_kms(joypad, &drv, &drv_data);
               break;
#ifdef HAVE_WAYLAND
            case INPUT_WINDOW_WAYLAND:
               input_driver_init_wayland(joypad,
                     input_window_data_for_video, &drv, &drv_data);
               break;
#endif
#ifdef HAVE_SDL3
            case INPUT_WINDOW_SDL3:
               input_driver_init_sdl3(joypad, &drv, &drv_data);
               break;
#endif
#if defined(HAVE_SDL) && !defined(HAVE_SDL2) && !defined(HAVE_SDL3)
            case INPUT_WINDOW_SDL1:
               input_driver_init_sdl1(joypad, &drv, &drv_data);
               break;
#endif
            case INPUT_WINDOW_PLATFORM:
               input_driver_init_platform(joypad, &drv, &drv_data);
               break;
            default:
               break;
         }
         if (drv && drv_data)
         {
            *input                       = drv;
            input_driver_st.current_data = drv_data;
            return true;
         }
      }
      /* Nothing for the window, or no such window: the configured one. */
      RARCH_LOG("[Video] Graphics driver did not initialize an input driver."
         " Attempting to pick a suitable driver.\n");
   }

   if (tmp)
      *input = tmp;
   else
   {
      if (!(input_driver_find_driver(
            settings, "input driver",
            verbosity_enabled)))
      {
         RARCH_ERR("[Video] Cannot find input driver. Exiting...\n");
         return false;
      }
   }

   /* This should never really happen as tmp (driver.input) is always
    * found before this in find_driver_input(), or we have aborted
    * in a similar fashion anyways. */
   if (     !input_driver_st.current_driver
         || !(new_data = input_driver_init_wrap(
               input_driver_st.current_driver,
               settings->arrays.input_joypad_driver)))
   {
      RARCH_ERR("[Video] Cannot initialize input driver. Exiting...\n");
      return false;
   }

   input_driver_st.current_data = new_data;

   return true;
}

bool input_driver_grab_mouse(void)
{
   const input_driver_t *input = (const input_driver_t*)input_driver_st.current_driver;
   if (!input || !input->grab_mouse)
      return false;
   input->grab_mouse(input_driver_st.current_data, true);
   return true;
}

bool input_driver_ungrab_mouse(void)
{
   const input_driver_t *input = (const input_driver_t*)
   input_driver_st.current_driver;
   if (!input || !input->grab_mouse)
      return false;
   input->grab_mouse(input_driver_st.current_data, false);
   return true;
}

void input_config_reset(void)
{
   unsigned i;
   input_driver_state_t *input_st = &input_driver_st;

   memcpy(input_config_binds[0], retro_keybinds_1, sizeof(retro_keybinds_1));

   for (i = 1; i < MAX_USERS; i++)
      memcpy(input_config_binds[i], retro_keybinds_rest,
            sizeof(retro_keybinds_rest));

   for (i = 0; i < MAX_USERS; i++)
   {
      /* Note: Don't use input_config_clear_device_name()
       * here, since this will re-index devices each time
       * (not required - we are setting all 'name indices'
       * to zero manually) */
      input_st->input_device_info[i].name[0]          = '\0';
      input_st->input_device_info[i].display_name[0]  = '\0';
      input_st->input_device_info[i].config_name[0]   = '\0';
      input_st->input_device_info[i].joypad_driver[0] = '\0';
      input_st->input_device_info[i].vid              = 0;
      input_st->input_device_info[i].pid              = 0;
      input_st->input_device_info[i].autoconfigured   = false;
      input_st->input_device_info[i].name_index       = 0;

      input_config_reset_autoconfig_binds(i);

      input_st->libretro_input_binds[i] = (const retro_keybind_set *)&input_config_binds[i];
   }
}

void input_config_set_device(unsigned port, unsigned id)
{
   settings_t        *settings = config_get_ptr();
   if (settings && (port < MAX_USERS))
      configuration_set_uint(settings,
            settings->uints.input_libretro_device[port], id);
}

unsigned input_config_get_device(unsigned port)
{
   settings_t *settings = config_get_ptr();
   if (settings && (port < MAX_USERS))
      return settings->uints.input_libretro_device[port];
   return RETRO_DEVICE_NONE;
}

const struct retro_keybind *input_config_get_bind_auto(
      unsigned port, unsigned id)
{
   settings_t *settings = config_get_ptr();
   unsigned  joy_idx    = settings->uints.input_joypad_index[port];
   if (joy_idx < MAX_USERS)
      return &input_autoconf_binds[joy_idx][id];
   return NULL;
}

unsigned *input_config_get_device_ptr(unsigned port)
{
   settings_t *settings = config_get_ptr();
   if (settings && (port < MAX_USERS))
      return &settings->uints.input_libretro_device[port];
   return NULL;
}

/* The player -> pad mapping is required to be a permutation of
 * [0, MAX_USERS):
 *
 * - Two players sharing a pad index makes a single physical device
 *   drive two libretro ports at once, so one controller presses
 *   Start for both players.
 * - reallocate_port_if_needed() in tasks/task_autodetect.c reassigns
 *   ports by transposing entries of this array. A transposition
 *   preserves the permutation property but cannot restore it, so a
 *   mapping that is already non-injective stays non-injective for
 *   every subsequent hotplug.
 * - That same function inverts the mapping through an array indexed
 *   by pad index, so an out of range entry is an out of bounds write.
 *
 * Nothing validates the mapping when it is read back from a
 * configuration file, and the menu's device index selector happily
 * lets two players point at the same pad, so the invariant has to be
 * asserted rather than assumed. */
bool input_config_sanitize_joypad_indices(void)
{
   unsigned i;
   bool taken[MAX_USERS];
   unsigned next_free   = 0;
   bool corrected       = false;
   settings_t *settings = config_get_ptr();

   if (!settings)
      return false;

   for (i = 0; i < MAX_USERS; i++)
      taken[i] = false;

   for (i = 0; i < MAX_USERS; i++)
   {
      unsigned joy_idx = settings->uints.input_joypad_index[i];

      if (joy_idx < MAX_USERS && !taken[joy_idx])
      {
         taken[joy_idx] = true;
         continue;
      }

      while (next_free < MAX_USERS && taken[next_free])
         next_free++;

      /* MAX_USERS players cannot claim more than MAX_USERS distinct
       * indices, so this is unreachable; bail out rather than write
       * past the end of the array if it ever is reached. */
      if (next_free >= MAX_USERS)
         break;

      RARCH_WARN("[Input] Player %u had %s pad index %u, "
            "reassigning to %u.\n",
            i + 1,
            (joy_idx < MAX_USERS) ? "duplicate" : "out of range",
            joy_idx, next_free);

      settings->uints.input_joypad_index[i] = next_free;
      taken[next_free]                      = true;
      corrected                             = true;
   }

   return corrected;
}

/* Adds an index to devices with the same name,
 * so they can be uniquely identified in the
 * frontend */
static void input_config_reindex_device_names(input_driver_state_t *input_st)
{
   unsigned i, j;
   unsigned name_index;

   /* Reset device name indices */
   for (i = 0; i < MAX_INPUT_DEVICES; i++)
      input_st->input_device_info[i].name_index       = 0;

   /* Scan device names */
   for (i = 0; i < MAX_INPUT_DEVICES; i++)
   {
      const char *device_name = input_config_get_device_name(i);

      /* If current device name is empty, or a non-zero
       * name index has already been assigned, continue
       * to the next device */
      if (
               (!device_name || !*device_name)
            || input_st->input_device_info[i].name_index != 0)
         continue;

      /* > Uniquely named devices have a name index
       *   of 0
       * > Devices with the same name have a name
       *   index starting from 1 */
      name_index = 1;

      /* Loop over all devices following the current
       * selection */
      for (j = i + 1; j < MAX_INPUT_DEVICES; j++)
      {
         const char *next_device_name = input_config_get_device_name(j);

         if (!next_device_name || !*next_device_name)
            continue;

         /* Check if names match */
         if (string_is_equal(device_name, next_device_name))
         {
            /* If this is the first match, set a starting
             * index for the current device selection */
            if (input_st->input_device_info[i].name_index == 0)
               input_st->input_device_info[i].name_index = name_index++;

            /* Set name index for the next device
             * (will keep incrementing as more matches
             *  are found) */
            input_st->input_device_info[j].name_index    = name_index++;
         }
      }
   }
}

const char *input_config_get_device_name(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   if (!*input_st->input_device_info[port].name)
      return NULL;
   return input_st->input_device_info[port].name;
}

const char *input_config_get_device_display_name(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   if (!*input_st->input_device_info[port].display_name)
      return NULL;
   return input_st->input_device_info[port].display_name;
}

const char *input_config_get_device_config_name(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   if (!*input_st->input_device_info[port].config_name)
      return NULL;
   return input_st->input_device_info[port].config_name;
}

const char *input_config_get_device_joypad_driver(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   if (!*input_st->input_device_info[port].joypad_driver)
      return NULL;
   return input_st->input_device_info[port].joypad_driver;
}

const char *input_config_get_device_phys(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   if (!*input_st->input_device_info[port].phys)
      return NULL;
   return input_st->input_device_info[port].phys;
}

uint16_t input_config_get_device_vid(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   return input_st->input_device_info[port].vid;
}

uint16_t input_config_get_device_pid(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   return input_st->input_device_info[port].pid;
}

bool input_config_get_device_autoconfigured(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   return input_st->input_device_info[port].autoconfigured;
}

unsigned input_config_get_device_name_index(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   return input_st->input_device_info[port].name_index;
}

/* TODO/FIXME: This is required by linuxraw_joypad.c
 * and parport_joypad.c. These input drivers should
 * be refactored such that this dubious low-level
 * access is not required */
char *input_config_get_device_name_ptr(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   return input_st->input_device_info[port].name;
}

size_t input_config_get_device_name_size(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   return sizeof(input_st->input_device_info[port].name);
}

void input_config_set_device_name(unsigned port, const char *name)
{
   input_driver_state_t *input_st = &input_driver_st;
   if (!name || !*name)
      return;

   strlcpy(input_st->input_device_info[port].name, name,
         sizeof(input_st->input_device_info[port].name));
   /* another controller: what its triggers rest at is learned anew */
   if (port < MAX_USERS)
   {
      input_st->trigger_rest[port]    = 0;
      input_st->trigger_two_way[port] = 0;
   }

   input_config_reindex_device_names(input_st);
}

void input_config_set_device_display_name(unsigned port, const char *name)
{
   input_driver_state_t *input_st = &input_driver_st;
   if (name && *name)
      strlcpy(input_st->input_device_info[port].display_name, name,
            sizeof(input_st->input_device_info[port].display_name));
}

void input_config_set_device_config_name(unsigned port, const char *name)
{
   input_driver_state_t *input_st = &input_driver_st;
   if (name && *name)
      strlcpy(input_st->input_device_info[port].config_name, name,
            sizeof(input_st->input_device_info[port].config_name));
}

void input_config_set_device_joypad_driver(unsigned port, const char *driver)
{
   input_driver_state_t *input_st = &input_driver_st;
   if (driver && *driver)
      strlcpy(input_st->input_device_info[port].joypad_driver, driver,
            sizeof(input_st->input_device_info[port].joypad_driver));
}

void input_config_set_device_phys(unsigned port, const char *phys)
{
   input_driver_state_t *input_st = &input_driver_st;
   if (phys && *phys)
      strlcpy(input_st->input_device_info[port].phys, phys,
            sizeof(input_st->input_device_info[port].phys));
   else
      input_st->input_device_info[port].phys[0] = '\0';
}

void input_config_set_device_vid(unsigned port, uint16_t vid)
{
   input_driver_state_t *input_st        = &input_driver_st;
   input_st->input_device_info[port].vid = vid;
}

void input_config_set_device_pid(unsigned port, uint16_t pid)
{
   input_driver_state_t *input_st        = &input_driver_st;
   input_st->input_device_info[port].pid = pid;
}

void input_config_set_device_autoconfigured(unsigned port, bool autoconfigured)
{
   input_driver_state_t *input_st = &input_driver_st;
   input_st->input_device_info[port].autoconfigured = autoconfigured;
}

void input_config_set_device_name_index(unsigned port, unsigned name_index)
{
   input_driver_state_t *input_st = &input_driver_st;
   input_st->input_device_info[port].name_index = name_index;
}

void input_config_clear_device_name(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   input_st->input_device_info[port].name[0] = '\0';
   input_config_reindex_device_names(input_st);
}

void input_config_clear_device_display_name(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   input_st->input_device_info[port].display_name[0] = '\0';
}

void input_config_clear_device_config_name(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   input_st->input_device_info[port].config_name[0] = '\0';
}

void input_config_clear_device_joypad_driver(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   input_st->input_device_info[port].joypad_driver[0] = '\0';
}

const char *input_config_get_mouse_display_name(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   if (!*input_st->input_mouse_info[port].display_name)
      return NULL;
   return input_st->input_mouse_info[port].display_name;
}

void input_config_set_mouse_display_name(unsigned port, const char *name)
{
   char name_ascii[NAME_MAX_LENGTH];
   input_driver_state_t *input_st = &input_driver_st;

   name_ascii[0] = '\0';

   /* Strip non-ASCII characters */
   if (name && *name)
   {
      string_copy_only_ascii(name_ascii, name);
      string_trim_whitespace(name_ascii);
   }

   if (*name_ascii)
      strlcpy(input_st->input_mouse_info[port].display_name, name_ascii,
            sizeof(input_st->input_mouse_info[port].display_name));
}

void input_config_clear_mouse_info(void)
{
   input_driver_state_t *input_st = &input_driver_st;
   memset(input_st->input_mouse_info, 0, sizeof(input_st->input_mouse_info));
}

void input_config_set_mouse_device(unsigned idx, const char *device,
      uint16_t vid, uint16_t pid, bool hidden)
{
   input_mouse_info_t *info;
   if (idx >= MAX_INPUT_DEVICES)
      return;
   info         = &input_driver_st.input_mouse_info[idx];
   strlcpy(info->device, device ? device : "", sizeof(info->device));
   info->vid    = vid;
   info->pid    = pid;
   info->hidden = hidden;
}

const char *input_config_get_mouse_device(unsigned idx)
{
   return (idx < MAX_INPUT_DEVICES)
      ? input_driver_st.input_mouse_info[idx].device : "";
}

bool input_config_mouse_offered(unsigned idx)
{
   unsigned i;
   const input_mouse_info_t *info = input_driver_st.input_mouse_info;

   if (idx >= MAX_INPUT_DEVICES)
      return false;
   if (*info[idx].display_name && !info[idx].hidden)
      return true;
   for (i = 0; i < MAX_INPUT_DEVICES; i++)
      if (*info[i].display_name && !info[i].hidden)
         return false; /* there are mice listed, and this is not one */
   return true;
}

uint16_t input_config_get_mouse_vid(unsigned idx)
{
   return (idx < MAX_INPUT_DEVICES)
      ? input_driver_st.input_mouse_info[idx].vid : 0;
}

uint16_t input_config_get_mouse_pid(unsigned idx)
{
   return (idx < MAX_INPUT_DEVICES)
      ? input_driver_st.input_mouse_info[idx].pid : 0;
}

bool input_config_get_mouse_hidden(unsigned idx)
{
   return (idx < MAX_INPUT_DEVICES)
      && input_driver_st.input_mouse_info[idx].hidden;
}

void input_config_clear_keyboard_display_names(void)
{
   unsigned i;
   input_driver_state_t *input_st = &input_driver_st;
   for (i = 0; i < MAX_INPUT_DEVICES; i++)
   {
      input_st->input_keyboard_info[i].display_name[0] = '\0';
      input_st->input_keyboard_info[i].vid             = 0;
      input_st->input_keyboard_info[i].pid             = 0;
   }
}

void input_config_set_keyboard_ids(unsigned idx, uint16_t vid, uint16_t pid)
{
   if (idx >= MAX_INPUT_DEVICES)
      return;
   input_driver_st.input_keyboard_info[idx].vid = vid;
   input_driver_st.input_keyboard_info[idx].pid = pid;
}

uint16_t input_config_get_keyboard_vid(unsigned idx)
{
   return (idx < MAX_INPUT_DEVICES)
      ? input_driver_st.input_keyboard_info[idx].vid : 0;
}

uint16_t input_config_get_keyboard_pid(unsigned idx)
{
   return (idx < MAX_INPUT_DEVICES)
      ? input_driver_st.input_keyboard_info[idx].pid : 0;
}

void input_config_set_keyboard_display_name(unsigned idx, const char *name)
{
   char name_ascii[NAME_MAX_LENGTH];
   input_driver_state_t *input_st = &input_driver_st;

   if (idx >= MAX_INPUT_DEVICES)
      return;

   name_ascii[0] = '\0';

   /* Strip non-ASCII characters, as for mice */
   if (name && *name)
   {
      string_copy_only_ascii(name_ascii, name);
      string_trim_whitespace(name_ascii);
   }

   /* a keyboard with no name is still a keyboard */
   strlcpy(input_st->input_keyboard_info[idx].display_name,
         *name_ascii ? name_ascii : "N/A",
         sizeof(input_st->input_keyboard_info[idx].display_name));
}

const char *input_config_get_keyboard_display_name(unsigned idx)
{
   input_driver_state_t *input_st = &input_driver_st;
   if (     idx >= MAX_INPUT_DEVICES
         || !*input_st->input_keyboard_info[idx].display_name)
      return NULL;
   return input_st->input_keyboard_info[idx].display_name;
}

void input_keyboard_mapping_bits(unsigned mode, unsigned key)
{
   input_driver_state_t *input_st = &input_driver_st;
   switch (mode)
   {
      case 0:
         BIT512_CLEAR_PTR(&input_st->keyboard_mapping_bits, key);
         break;
      case 1:
         BIT512_SET_PTR(&input_st->keyboard_mapping_bits, key);
         break;
      default:
         break;
   }
}

void config_read_keybinds_conf(void *data)
{
   unsigned i;
   config_file_t            *conf = (config_file_t*)data;
   bool key_store[RETROK_LAST]    = {0};

   if (!conf)
      return;

   for (i = 0; i < MAX_USERS; i++)
   {
      unsigned j;

      for (j = 0; input_config_bind_map_get_valid(j); j++)
      {
         char str[NAME_MAX_LENGTH];
         char prefix[16];
         const struct input_bind_map *keybind =
            (const struct input_bind_map*)INPUT_CONFIG_BIND_MAP_GET(j);
         struct retro_keybind *bind      = &input_config_binds[i][j];
         bool meta                       = false;
         const char *btn                 = NULL;
         struct config_entry_list *entry = NULL;

         if (!bind || !RETRO_KEYBIND_VALID(bind) || !keybind || !keybind->valid)
            continue;

         meta                       = keybind->meta;
         btn                        = keybind->base;

         prefix[0]                  = '\0';
         input_config_get_prefix(prefix, sizeof(prefix), i, meta);

         if (!*prefix || !btn)
            continue;

         fill_pathname_join_delim(str, prefix, btn,  '_', sizeof(str));

         /* Clear old mapping bit unless just recently set */
         if (!key_store[RETRO_KEYBIND_KEY(bind)])
            input_keyboard_mapping_bits(0, RETRO_KEYBIND_KEY(bind));

         entry                      = config_get_entry(conf, str);
         if (entry && entry->value && *entry->value)
            RETRO_KEYBIND_SET_KEY(bind,
                  input_config_translate_str_to_rk(
                     entry->value, strlen(entry->value)));

         /* Store new mapping bit and remember it for a while
          * so that next clear leaves the new key alone */
         input_keyboard_mapping_bits(1, RETRO_KEYBIND_KEY(bind));
         key_store[RETRO_KEYBIND_KEY(bind)]       = true;

         input_config_parse_joy_button  (str, conf, prefix, btn, bind,
               &input_config_bind_labels[i][j]);
         input_config_parse_joy_axis    (str, conf, prefix, btn, bind,
               &input_config_bind_labels[i][j]);
         input_config_parse_mouse_button(str, conf, prefix, btn, bind);
      }
   }
}

#ifdef HAVE_COMMAND
void input_driver_init_command(input_driver_state_t *input_st,
      settings_t *settings)
{
#ifdef HAVE_STDIN_CMD
   bool input_stdin_cmd_enable       = settings->bools.stdin_cmd_enable;

   if (input_stdin_cmd_enable)
   {
      input_driver_state_t *input_st = &input_driver_st;
      const input_driver_t *input    = (const input_driver_t*)
	      input_st->current_driver;
      bool grab_stdin                = input->grab_stdin
         && input->grab_stdin(input_st->current_data);
      if (grab_stdin)
      {
         RARCH_WARN("stdin command interface is desired, "
               "but input driver has already claimed stdin.\n"
               "Cannot use this command interface.\n");
      }
      else
      {
         input_st->command[0] = command_stdin_new();
         if (!input_st->command[0])
            RARCH_ERR("Failed to initialize the stdin command interface.\n");
      }
   }
#endif

   /* Initialize the network command interface */
#ifdef HAVE_NETWORK_CMD
   {
      bool input_network_cmd_enable = settings->bools.network_cmd_enable;
      if (input_network_cmd_enable)
      {
         unsigned network_cmd_port  = settings->uints.network_cmd_port;
         if (!(input_st->command[1] = command_network_new(network_cmd_port,
                     settings->arrays.network_cmd_bind_address)))
            RARCH_ERR("Failed to initialize the network command interface.\n");
      }
   }
#endif

#ifdef HAVE_MCP
   /* The MCP server. A token is required; the first start makes one,
    * kept in the configuration (sealed by the keychain) for the user
    * to give the client. Without a random source it must be set by
    * hand. */
   if (settings->bools.mcp_server_enable)
   {
      char *token = settings->arrays.mcp_server_token;
#ifdef HAVE_CRYPTO
      if (!*token)
      {
         uint8_t  raw[24];
         unsigned i;
         if (crypto_random_bytes(raw, sizeof(raw)) == 0)
         {
            for (i = 0; i < sizeof(raw); i++)
               snprintf(token + 2 * i, 3, "%02x", raw[i]);
            settings->flags |= SETTINGS_FLG_MODIFIED;
         }
         crypto_memzero(raw, sizeof(raw));
      }
#endif
      if (!*token)
         RARCH_ERR("[MCP] Set mcp_server_token to start the MCP server.\n");
      else if (!(input_st->command[3] = command_mcp_new(
                  (uint16_t)settings->uints.mcp_server_port,
                  settings->arrays.mcp_server_bind_address, token)))
         RARCH_ERR("[MCP] Failed to start the MCP server.\n");
   }
#endif

#if defined(HAVE_LAKKA)
   if (!(input_st->command[2] = command_uds_new()))
      RARCH_ERR("Failed to initialize the UDS command interface.\n");
#elif defined(__EMSCRIPTEN__)
   if (!(input_st->command[2] = command_emscripten_new()))
      RARCH_ERR("Failed to initialize the emscripten command interface.\n");
#endif

}

unsigned input_driver_command_generation(void)
{
   return input_driver_st.command_generation;
}

void input_driver_deinit_command(input_driver_state_t *input_st)
{
   int i;
   for (i = 0; i < (int)ARRAY_SIZE(input_st->command); i++)
   {
      if (input_st->command[i])
         input_st->command[i]->destroy(
            input_st->command[i]);
      input_st->command_generation++;

      input_st->command[i] = NULL;
    }
}
#endif

void input_game_focus_free(void)
{
   input_game_focus_state_t *game_focus_st = &input_driver_st.game_focus_state;

   /* Ensure that game focus mode is disabled */
   if (game_focus_st->enabled)
   {
      enum input_game_focus_cmd_type game_focus_cmd = GAME_FOCUS_CMD_OFF;
      command_event(CMD_EVENT_GAME_FOCUS_TOGGLE, &game_focus_cmd);
   }

   game_focus_st->enabled        = false;
   game_focus_st->core_requested = false;
}

#ifdef HAVE_OVERLAY
static void input_overlay_enable_(bool enable)
{
   settings_t *settings           = config_get_ptr();
   input_driver_state_t *input_st = &input_driver_st;
   input_overlay_t *ol            = input_st->overlay_ptr;
   float opacity                  = (ol && (ol->flags & INPUT_OVERLAY_IS_OSK))
      ? settings->floats.input_osk_overlay_opacity
      : settings->floats.input_overlay_opacity;
   bool auto_rotate               = settings->bools.input_overlay_auto_rotate;
   bool hide_mouse_cursor         = !settings->bools.input_overlay_show_mouse_cursor
         && settings->bools.video_fullscreen;

   if (!ol)
      return;

   if (enable)
   {
      /* Set video interface */
      if (     !video_driver_get_overlay_interface(&ol->iface, &ol->iface_data)
            || !ol->iface)
      {
         RARCH_ERR("[Input] Overlay interface is not present in video driver.\n");
         ol->flags &= ~INPUT_OVERLAY_ALIVE;
         return;
      }

      /* Load last-active overlay */
      ol->flags &= ~INPUT_OVERLAY_TEXTURES_DECLINED;
      input_overlay_load_active(ol, opacity);

      /* Adjust to current settings */
      command_event(CMD_EVENT_OVERLAY_SET_SCALE_FACTOR, NULL);

      if (auto_rotate)
      {
         input_overlay_auto_rotate_(
               video_driver_get_output_dims(), true, ol);
      }

      /* Enable */
      if (ol->iface->enable)
         ol->iface->enable(ol->iface_data, true);

      ol->flags |= (INPUT_OVERLAY_ENABLE | INPUT_OVERLAY_BLOCKED);

      if (hide_mouse_cursor)
         video_driver_show_mouse(false);
   }
   else
   {
      /* Disable and clear input state */
      ol->flags       &= ~INPUT_OVERLAY_ENABLE;
      input_st->flags &= ~INP_FLAG_BLOCK_POINTER_INPUT;

      if (ol->iface && ol->iface->enable)
         ol->iface->enable(ol->iface_data, false);
      /* The pack's textures stay: the driver they were made on is
       * still here, and a pack that comes back from the cache shows
       * its pages without an upload. Only the video teardown unloads
       * them (input_overlay_video_teardown), the driver still there. */
      ol->iface = NULL;

      memset(&ol->overlay_state, 0, sizeof(input_overlay_state_t));
      memset(&ol->pointer_state, 0, sizeof(input_overlay_pointer_state_t));
   }
}

/* The video side reads the active overlay's viewport override from a
 * copy (video_driver_update_viewport() may run on the threaded video
 * worker, and the overlay is freed and replaced here); refreshed
 * whenever overlay_ptr or the overlay it shows changes. */
static void input_overlay_viewport_publish(void)
{
   input_overlay_t *ol = input_driver_st.overlay_ptr;
   video_driver_set_overlay_viewport(ol ? ol->active : NULL);
}

static void input_overlay_deinit(void)
{
   input_overlay_free(input_driver_st.overlay_ptr);
   input_driver_st.overlay_ptr = NULL;
   input_overlay_viewport_publish();

   input_overlay_free(input_driver_st.overlay_cache_ptr);
   input_driver_st.overlay_cache_ptr = NULL;

   input_driver_st.flags &= ~INP_FLAG_BLOCK_POINTER_INPUT;
}

static void input_overlay_move_to_cache(void)
{
   input_driver_state_t *input_st = &input_driver_st;
   input_overlay_t      *ol       = input_st->overlay_ptr;

   if (!ol)
      return;

   /* Free existing cache */
   input_overlay_free(input_driver_st.overlay_cache_ptr);

   /* Disable current overlay */
   input_overlay_enable_(false);

   /* Move to cache */
   input_st->overlay_cache_ptr = ol;
   input_st->overlay_ptr       = NULL;
   input_overlay_viewport_publish();
}

static void input_overlay_swap_with_cached(void)
{
   input_driver_state_t *input_st = &input_driver_st;
   input_overlay_t      *ol;

   /* Disable current overlay */
   input_overlay_enable_(false);

   /* Swap with cached */
   ol                          = input_st->overlay_cache_ptr;
   input_st->overlay_cache_ptr = input_st->overlay_ptr;
   input_st->overlay_ptr       = ol;
   input_overlay_viewport_publish();

   /* Enable and update to current settings */
   input_overlay_enable_(true);
}

void input_overlay_unload(void)
{
   bool input_overlay_enable   = config_get_ptr()->bools.input_overlay_enable;
   runloop_state_t *runloop_st = runloop_state_get_ptr();

   /* Free if overlays disabled or initing/deiniting core */
   if (     !input_overlay_enable
         || !runloop_is_inited()
         ||  (runloop_st->flags & RUNLOOP_FLAG_SHUTDOWN_INITIATED))
      input_overlay_deinit();
   else
      input_overlay_move_to_cache();
}

void input_overlay_leds_enable(bool enable)
{
   input_driver_state_t *input_st = &input_driver_st;

   input_st->overlay_leds_lit = 0;
   if (enable)
      input_st->flags |=  INP_FLAG_OVERLAY_LEDS;
   else
      input_st->flags &= ~INP_FLAG_OVERLAY_LEDS;

   if (input_st->overlay_ptr && input_st->overlay_ptr->active)
   {
      settings_t *settings = config_get_ptr();
      input_overlay_set_alpha_mod(input_st->overlay_ptr,
            (input_st->overlay_ptr->flags & INPUT_OVERLAY_IS_OSK)
            ? settings->floats.input_osk_overlay_opacity
            : settings->floats.input_overlay_opacity);
   }
}

void input_overlay_set_led(int led, bool lit)
{
   input_driver_state_t *input_st = &input_driver_st;
   input_overlay_t      *ol       = input_st->overlay_ptr;
   uint32_t              was      = input_st->overlay_leds_lit;

   /* The LED number is the core's (retro_led_interface), with no range
    * of its own. */
   if (led < 0 || led >= MAX_LEDS)
      return;

   if (lit)
      input_st->overlay_leds_lit |=  (1u << led);
   else
      input_st->overlay_leds_lit &= ~(1u << led);

   /* A light going out is shown at once; one coming on shows at the
    * next poll's alpha pass, as every other image does. */
   if (     ol && ol->active
         && input_st->overlay_leds_lit != was)
      input_overlay_hide_leds(ol, input_st->overlay_leds_lit,
            input_overlay_led_map());
}

static bool input_overlay_want_hidden(void)
{
   settings_t *settings = config_get_ptr();
   bool hide            = false;

#ifdef HAVE_MENU
   if (settings->bools.input_overlay_hide_in_menu)
      hide = (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE) != 0;
#endif
   if (settings->bools.input_overlay_hide_when_gamepad_connected
         && !settings->bools.input_overlay_pointer_enable)
      hide = hide || (input_config_get_device_name(0) != NULL);

   return hide;
}

void input_driver_menu_combo_source_gate(input_bits_t *bits)
{
   unsigned i;
   size_t d;
   input_driver_state_t *input_st = &input_driver_st;
   input_overlay_t *ol            = input_st->overlay_ptr;
   uint16_t only                  = 0;

   if (!ol || !ol->active)
      return;
   /* the buttons the overlay alone holds */
   for (i = 0; i < 16; i++)
      if (     BIT256_GET_PTR(bits, i)
            && BIT256_GET(ol->overlay_state.buttons, i)
            && !(input_st->system_buttons_not_overlay & (1u << i)))
         only |= (uint16_t)(1u << i);
   if (!only)
      return;
   /* (looked for only now, with such a button held) */
   for (d = 0; d < ol->active->size; d++)
      if (BIT256_GET(ol->active->descs[d].button_mask, RARCH_MENU_TOGGLE))
         break;
   if (d == ol->active->size)
      return;
   for (i = 0; i < 16; i++)
      if (only & (1u << i))
         BIT256_CLEAR_PTR(bits, i);
}

void input_overlay_check_mouse_cursor(void)
{
   input_driver_state_t *input_st = &input_driver_st;
   input_overlay_t *ol            = input_st->overlay_ptr;

   if (ol && (ol->flags & INPUT_OVERLAY_ENABLE))
   {
      if (config_get_ptr()->bools.input_overlay_show_mouse_cursor)
         video_driver_show_mouse(true);
      else if (input_st->flags & INP_FLAG_GRAB_MOUSE_STATE)
         video_driver_show_mouse(false);
   }
}

static void input_overlay_loaded_move_images(input_overlay_t *ol,
      struct string_list *image_list, struct string_list *anim_list)
{
   size_t i;

   ol->images = (struct texture_image**)malloc(ol->num_images * sizeof(struct texture_image *));

   if (!ol->images)
   {
      for (i = 0; i < ol->num_images; i++)
         image_texture_free((struct texture_image*)image_list->elems[i].attr.p);
      ol->num_images = 0;
      RARCH_ERR("[Overlay] Couldn't allocate images array.\n");
   }

   for (i = 0; i < ol->num_images; i++)
      ol->images[i] = (struct texture_image*)image_list->elems[i].attr.p;

#ifdef HAVE_RPNG
   /* The animated images' file bytes, and a stream over each: the
    * pack owns both from here, and frees them with the images. A
    * pack with no animation allocates nothing. */
   if (anim_list && anim_list->size >= ol->num_images)
   {
      size_t animated = 0;
      for (i = 0; i < ol->num_images; i++)
         if (anim_list->elems[i].attr.p)
            animated++;
      if (     animated
            && (ol->anim_data    = (void**)calloc(ol->num_images, sizeof(void*)))
            && (ol->anim_len     = (size_t*)calloc(ol->num_images, sizeof(size_t)))
            && (ol->anim_stream  = (void**)calloc(ol->num_images, sizeof(void*)))
            && (ol->anim_next_us = (int64_t*)calloc(ol->num_images, sizeof(int64_t)))
            && (ol->anim_2frame  = (uint8_t*)calloc(ol->num_images, sizeof(uint8_t)))
            && (ol->anim_2frame_pressed = (uint8_t*)calloc(ol->num_images, sizeof(uint8_t)))
            && (ol->anim_2frame_cur     = (uint8_t*)calloc(ol->num_images, sizeof(uint8_t)))
            && (ol->anim_2frame_pix     = (uint32_t**)calloc(ol->num_images, sizeof(uint32_t*))))
      {
         for (i = 0; i < ol->num_images; i++)
         {
            overlay_anim_src_t *src =
               (overlay_anim_src_t*)anim_list->elems[i].attr.p;
            if (!src)
               continue;
            ol->anim_data[i]   = src->data;
            ol->anim_len[i]    = src->len;
            ol->anim_stream[i] = rpng_apng_stream_open(
                  (const uint8_t*)src->data, src->len);
            if (ol->anim_stream[i])
            {
               rpng_apng_stream_t *st = (rpng_apng_stream_t*)ol->anim_stream[i];
               unsigned w             = 0;
               unsigned h             = 0;
               int num_frames         = 0;
               rpng_apng_stream_set_argb(st,
                     ol->images[i]->supports_rgba ? 0 : 1);
               rpng_apng_stream_get_info(st, &w, &h, &num_frames, NULL);
               /* A two-frame APNG is an unpressed/pressed pair. Both
                * frames are composed here, once: the second depends
                * on the first, so composing on demand would decode
                * both on every press. Anything that goes wrong leaves
                * it an ordinary looping animation. */
               if (     num_frames == 2
                     && w == ol->images[i]->width
                     && h == ol->images[i]->height)
               {
                  size_t frame_len = (size_t)w * h;
                  uint32_t *pix    = (uint32_t*)malloc(
                        frame_len * 2 * sizeof(uint32_t));
                  const uint32_t *frame;
                  int duration_ms  = 0;

                  if (     pix
                        && (frame = rpng_apng_stream_next(st, &duration_ms)))
                  {
                     memcpy(pix, frame, frame_len * sizeof(uint32_t));
                     if ((frame = rpng_apng_stream_next(st, &duration_ms)))
                     {
                        memcpy(pix + frame_len, frame,
                              frame_len * sizeof(uint32_t));
                        ol->anim_2frame_pix[i] = pix;
                        ol->anim_2frame[i]     = 1;
                        pix                    = NULL;
                     }
                  }
                  free(pix);
                  rpng_apng_stream_rewind(st);
               }
            }
         }
      }
   }
   if (anim_list)
      for (i = 0; i < anim_list->size; i++)
         free(anim_list->elems[i].attr.p);
#endif
}

/* task_data = overlay_task_data_t* */
static void input_overlay_loaded(retro_task_t *task,
      void *task_data, void *user_data, const char *err)
{
#ifdef HAVE_MENU
   uint16_t overlay_types;
#endif
   overlay_task_data_t  *data     = (overlay_task_data_t*)task_data;
   input_overlay_t      *ol       = NULL;
   input_driver_state_t *input_st = &input_driver_st;
   bool enable_overlay            = !input_overlay_want_hidden()
         && config_get_ptr()->bools.input_overlay_enable;

   if (err)
      return;

   /* NULL-check task_data: the task producer (task_overlay_handler
    * in tasks/task_overlay.c) can legitimately fail its data
    * alloc under OOM and pass NULL here; data->overlays below
    * would NULL-deref.  Nothing useful to do with a NULL payload
    * so bail cleanly. */
   if (!data)
      return;

   ol              = (input_overlay_t*)calloc(1, sizeof(*ol));
   /* NULL-check the ol calloc: the field writes below NULL-deref
    * on OOM.  Task_data ownership transferred to us by the
    * retro_task framework via task_set_data - we must tear it
    * down cleanly.  image_list entries each have a heap
    * texture_image* in elems[i].attr.p that move_images would
    * normally transfer into ol->images; since ol doesn't exist,
    * free each texture explicitly before string_list_free
    * (string_list_free only frees the list struct and its
    * entries' .data strings, not their .attr.p attachments). */
   if (!ol)
   {
      if (data->image_list)
      {
         size_t i;
         for (i = 0; i < data->image_list->size; i++)
            image_texture_free((struct texture_image*)data->image_list->elems[i].attr.p);
         string_list_free(data->image_list);
      }
      if (data->anim_list)
      {
         size_t i;
         for (i = 0; i < data->anim_list->size; i++)
         {
            overlay_anim_src_t *src =
               (overlay_anim_src_t*)data->anim_list->elems[i].attr.p;
            if (src)
               free(src->data);
            free(src);
         }
         string_list_free(data->anim_list);
      }
      free(data);
      return;
   }
   ol->overlays    = data->overlays;
   ol->size        = data->size;
   ol->active      = data->active;
   ol->path        = data->overlay_path;
   ol->num_images  = data->image_list->size;
   ol->next_index  = (unsigned)((ol->index + 1) % ol->size);
   ol->flags      |= INPUT_OVERLAY_ALIVE;
   if (data->flags & OVERLAY_LOADER_IS_OSK)
      ol->flags   |= INPUT_OVERLAY_IS_OSK;
   if (data->flags & OVERLAY_LOADER_HAS_LEDS)
      ol->flags   |= INPUT_OVERLAY_HAS_LEDS;

   /* One block for the sent alphas and the pass's scratch, sized for
    * the page with the most images. Without it every alpha is set
    * every pass, as before. */
   {
      size_t i, cap = 0;
      for (i = 0; i < ol->size; i++)
         if (ol->overlays[i].load_images_size > cap)
            cap = ol->overlays[i].load_images_size;
      if (cap && (ol->alpha_sent = (float*)malloc(2 * cap * sizeof(float))))
      {
         ol->alpha_want = ol->alpha_sent + cap;
         ol->alpha_cap  = cap;
         input_overlay_alpha_forget(ol);
      }
   }
#ifdef HAVE_MENU
   overlay_types   = data->overlay_types;
#endif

   if (ol->num_images > 0)
      input_overlay_loaded_move_images(ol, data->image_list, data->anim_list);
   string_list_free(data->image_list);
   if (data->anim_list)
      string_list_free(data->anim_list);

   free(data);

   /* Due to the asynchronous nature of overlay loading
    * it is possible for overlay_ptr to be non-NULL here
    * > Ensure it is free()'d before assigning new pointer */
   if (input_st->overlay_ptr)
      input_overlay_free(input_st->overlay_ptr);
   input_st->overlay_ptr = ol;
   input_overlay_viewport_publish();

   /* Enable or disable the overlay */
   input_overlay_enable_(enable_overlay);

   /* Abort if enable failed */
   if (!(ol->flags & INPUT_OVERLAY_ALIVE))
   {
      input_st->overlay_ptr = NULL;
      input_overlay_viewport_publish();
      input_overlay_free(ol);
      return;
   }

   /* Cache or free if hidden */
   if (!enable_overlay)
      input_overlay_unload();

   /* Soft-hide when gamepad connected but pointer input enabled */
   {
      settings_t *settings = config_get_ptr();
      if (enable_overlay
            && settings->bools.input_overlay_hide_when_gamepad_connected
            && settings->bools.input_overlay_pointer_enable
            && input_config_get_device_name(0) != NULL)
         ol->flags |= INPUT_OVERLAY_GAMEPAD_HIDDEN;
   }

   input_overlay_set_eightway_diagonal_sensitivity();

   /* Trigger viewport recalculation - overlay may have viewport override */
   command_event(CMD_EVENT_VIDEO_SET_ASPECT_RATIO, NULL);

#ifdef HAVE_MENU
   /* Update menu entries if this is the main overlay */
   if (!(ol->flags & INPUT_OVERLAY_IS_OSK))
   {
      struct menu_state *menu_st = menu_state_get_ptr();

      if (menu_st->overlay_types != overlay_types)
      {
         menu_st->overlay_types = overlay_types;
         menu_st->flags        |=  MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
      }
   }
#endif
}

#define SYSTEM_OVERLAY_DIR "gamepads/Named_Overlays"

static const char *input_overlay_path(bool want_osk)
{
   static char   system_overlay_path[PATH_MAX_LENGTH] = {0};
   char          overlay_directory[PATH_MAX_LENGTH];
   const char *a = NULL;
   settings_t   *settings                             = config_get_ptr();
   playlist_t   *playlist                             = playlist_get_cached();
   core_info_t  *core_info                            = NULL;
   const char   *content_path                         = path_get(RARCH_PATH_CONTENT);

   if (want_osk)
      return settings->paths.path_osk_overlay;
   /* If the option is set to turn this off, just return default */
   if (!settings->bools.input_overlay_enable_autopreferred)
       return settings->paths.path_overlay;
   /* If there's an override, use it */
   if (retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_OVERLAY_PRESET, NULL))
       return settings->paths.path_overlay;
   /* If there's no core, just return the default */
   a = path_get(RARCH_PATH_CORE);
   if (!a || !*a)
      return settings->paths.path_overlay;
   /* Let's go hunting */
   fill_pathname_expand_special(overlay_directory,
         settings->paths.directory_overlay,
         sizeof(overlay_directory));

   /* Try based on the playlist entry first */
   if (playlist)
   {
      if (content_path && *content_path)
      {
         const struct playlist_entry *entry = NULL;
         playlist_get_index_by_path(playlist, content_path, &entry);
         if (entry && entry->db_name)
         {
            size_t _len = fill_pathname_join_special_ext(system_overlay_path,
                  overlay_directory, SYSTEM_OVERLAY_DIR, entry->db_name, "",
                  sizeof(system_overlay_path));
            char *ext = path_get_extension_mutable(system_overlay_path);
            if (!ext)
               ext = system_overlay_path + _len;
            strlcpy_lit(ext, ".cfg", 5);
            if (path_is_valid(system_overlay_path))
               return system_overlay_path;
         }
      }
   }

   /* Maybe the core info will have some clues... */
   core_info_get_current_core(&core_info);
   if (core_info)
   {
      if (core_info->databases_list && core_info->databases_list->size == 1)
      {
         fill_pathname_join_special_ext(system_overlay_path,
               overlay_directory, SYSTEM_OVERLAY_DIR, core_info->databases_list->elems[0].data, ".cfg",
               sizeof(system_overlay_path));
         if (path_is_valid(system_overlay_path))
            return system_overlay_path;
      }

      if (core_info->display_name)
      {
         fill_pathname_join_special_ext(system_overlay_path,
               overlay_directory, SYSTEM_OVERLAY_DIR, core_info->display_name, ".cfg",
               sizeof(system_overlay_path));
         if (path_is_valid(system_overlay_path))
            return system_overlay_path;
      }
   }

   /* Maybe based on the content's directory name... */
   if (content_path && *content_path)
   {
      char dirname[DIR_MAX_LENGTH];
      fill_pathname_parent_dir_name(dirname, content_path, sizeof(dirname));
      fill_pathname_join_special_ext(system_overlay_path,
            overlay_directory, SYSTEM_OVERLAY_DIR, dirname, ".cfg",
            sizeof(system_overlay_path));
      if (path_is_valid(system_overlay_path))
         return system_overlay_path;
   }

   /* I give up */
   return settings->paths.path_overlay;
}

void input_overlay_init(void)
{
   settings_t *settings           = config_get_ptr();
   input_driver_state_t *input_st = &input_driver_st;
   input_overlay_t *ol            = input_st->overlay_ptr;
   input_overlay_t *ol_cache      = input_st->overlay_cache_ptr;
   bool want_osk                  =
            (input_st->flags & INP_FLAG_KB_LINEFEED_ENABLE)
         && *settings->paths.path_osk_overlay;
   const char *path_overlay       = input_overlay_path(want_osk);
   bool want_hidden               = input_overlay_want_hidden();
   bool overlay_shown             = ol
         && (ol->flags & INPUT_OVERLAY_ENABLE)
         && string_is_equal(path_overlay, ol->path);
   /* A cached pack is reusable only while it still has something to
    * upload from: the pixels are released once they are on the GPU,
    * so a pack whose textures went with a video reinit is reloaded
    * from its path instead. */
   bool overlay_cached            = ol_cache
         && (ol_cache->flags & INPUT_OVERLAY_ALIVE)
         && string_is_equal(path_overlay, ol_cache->path)
         && input_overlay_has_source(ol_cache);
   bool overlay_hidden            = !ol && overlay_cached;

#if defined(GEKKO)
   /* Avoid a crash at startup or even when toggling overlay in rgui */
   if (mem_stats_free() < (3 * 1024 * 1024))
      return;
#endif

   /* Cancel if overlays disabled or task already done */
   if (     !settings->bools.input_overlay_enable
         || ( want_hidden && overlay_hidden)
         || (!want_hidden && overlay_shown))
      return;

   /* Restore if cached */
   if (!want_hidden && overlay_cached)
   {
      input_overlay_swap_with_cached();
      return;
   }

   /* Cache current overlay when loading a different type */
   if (want_osk != (ol && (ol->flags & INPUT_OVERLAY_IS_OSK)))
      input_overlay_unload();
   else
      input_overlay_deinit();

   /* Start task */
   task_push_overlay_load_default(
         input_overlay_loaded, path_overlay, want_osk, NULL);
}
#endif

void input_pad_connect(unsigned port, input_device_driver_t *driver)
{
   if (port >= MAX_USERS || !driver)
   {
      RARCH_ERR("[Input] input_pad_connect: Bad parameters.\n");
      return;
   }

   input_autoconfigure_connect(driver->name(port), NULL, NULL, driver->ident,
          port, 0, 0);
}

static bool input_keys_pressed_other_sources(
      input_driver_state_t *input_st,
      unsigned i,
      input_bits_t *p_new_state)
{
#ifdef HAVE_COMMAND
   int j;
   for (j = 0; j < (int)ARRAY_SIZE(input_st->command); j++)
      if ((i < RARCH_BIND_LIST_END) && input_st->command[j]
         && input_st->command[j]->state[i])
      {
         if (i < 16)
            input_st->system_buttons_not_overlay |= (uint16_t)(1u << i);
         return true;
      }
#endif

#ifdef HAVE_NETWORKGAMEPAD
   /* Only process key presses related to game input if using Remote RetroPad */
   if (i < RARCH_CUSTOM_BIND_LIST_END
         && input_st->remote
         && INPUT_REMOTE_KEY_PRESSED(input_st, i, 0))
   {
      if (i < 16)
         input_st->system_buttons_not_overlay |= (uint16_t)(1u << i);
      return true;
   }
#endif

#ifdef HAVE_OVERLAY
   if (               input_st->overlay_ptr &&
         ((BIT256_GET(input_st->overlay_ptr->overlay_state.buttons, i))))
      return true;
#endif

   return false;
}

#define CHECK_GAME_FOCUS_ENABLE_HOTKEY_COMBO(i) \
   if (     (input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED) \
         && (  (i == RARCH_ENABLE_HOTKEY) \
            || (i != RARCH_ENABLE_HOTKEY && !block_hotkey[RARCH_ENABLE_HOTKEY])) \
      ) \
   { \
      if (input_state_wrap( \
            input_st->current_driver, \
            input_st->current_data, \
            joypad, \
            sec_joypad, \
            joypad_info, \
            binds, \
            !!(input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED), \
            port, RETRO_DEVICE_JOYPAD, 0, \
            i)) \
         block_hotkey[i] = false; \
   } \


/**
 * input_keys_pressed:
 *
 * Grab an input sample for this frame.
 */
static void input_keys_pressed(
      unsigned port,
      unsigned hotkey_port,
      bool is_menu,
      unsigned input_hotkey_block_delay,
      input_bits_t *p_new_state,
      const retro_keybind_set *binds,
      const struct retro_keybind *binds_norm,
      const struct retro_keybind *binds_auto,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      rarch_joypad_info_t *joypad_info,
      bool input_hotkey_device_merge)
{
   unsigned i;
   /* Autoconf binds are indexed by joy_idx, not frontend port */
   unsigned joy_idx               = joypad_info->joy_idx;
   int32_t ret                    = 0;
   input_driver_state_t *input_st = &input_driver_st;
   /* RetroPad buttons held this frame, for wait_release_mask pruning */
   uint16_t held_now              = 0;
   bool block_hotkey[RARCH_BIND_LIST_END];
   bool enable_hotkey_pressed     = false;
   bool any_pressed               = false;
   bool libretro_hotkey_set       =
            binds_norm->joykey  != NO_BTN
         || binds_norm->joyaxis != AXIS_NONE
         || binds_auto->joykey  != NO_BTN
         || binds_auto->joyaxis != AXIS_NONE;
   bool keyboard_hotkey_set       =
         RETRO_KEYBIND_KEY(binds_norm) != RETROK_UNKNOWN;

   if (!binds)
      return;

   if (     input_hotkey_device_merge
         && (libretro_hotkey_set || keyboard_hotkey_set))
      libretro_hotkey_set = keyboard_hotkey_set = true;

   /* Cache once - flag does not change during a single poll */
   {
      bool kb_blocked = !!(input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED);

   if (     (port == hotkey_port)
         && (RETRO_KEYBIND_VALID(binds_norm) || RETRO_KEYBIND_VALID(binds_auto))
         && CHECK_INPUT_DRIVER_BLOCK_HOTKEY(binds_norm, binds_auto))
   {
      if (input_state_wrap(
            input_st->current_driver,
            input_st->current_data,
            joypad,
            sec_joypad,
            joypad_info,
            binds,
            kb_blocked,
            port, RETRO_DEVICE_JOYPAD, 0,
            RARCH_ENABLE_HOTKEY))
      {
         enable_hotkey_pressed = true;
         if (input_st->input_hotkey_block_counter < input_hotkey_block_delay)
            input_st->input_hotkey_block_counter++;
         else
            input_st->flags |= INP_FLAG_BLOCK_LIBRETRO_INPUT;
      }
      else
         input_st->flags |= INP_FLAG_BLOCK_HOTKEY;
   }

   /* While the wait is clear nothing has been captured; the first
    * frame it is armed (here, or by the menu bind / dialog paths)
    * captures whatever is held below. */
   if (!(input_st->flags & INP_FLAG_WAIT_INPUT_RELEASE))
      input_st->wait_release_mask[port] = 0xFFFF;

#ifdef HAVE_MENU
   /* Prevent triggering menu actions after binding */
   if (     !(input_st->flags & INP_FLAG_MENU_PRESS_PENDING)
         && menu_state_get_ptr()->input_driver_flushing_input)
      input_st->flags |= INP_FLAG_WAIT_INPUT_RELEASE;
#endif

   /* Check libretro input if emulated device type is active,
    * except device type must be always active in menu. */
   if (     !(input_st->flags & INP_FLAG_BLOCK_LIBRETRO_INPUT)
         && !(!is_menu && !input_config_get_device(port)))
      ret = input_state_wrap(
            input_st->current_driver,
            input_st->current_data,
            joypad,
            sec_joypad,
            joypad_info,
            binds,
            kb_blocked,
            port, RETRO_DEVICE_JOYPAD, 0,
            RETRO_DEVICE_ID_JOYPAD_MASK);

   /* what a controller or a key holds is not the overlay's alone */
   input_st->system_buttons_not_overlay |= (uint16_t)ret;

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      if (     (ret & (UINT64_C(1) << i))
            || input_keys_pressed_other_sources(input_st, i, p_new_state))
      {
         any_pressed = true;
         held_now   |= (uint16_t)(1u << i);
         /* Only the buttons the wait was armed against are held
          * back; a button pressed after arming is delivered, so a
          * button that stays down (rear-touch triggers, a stuck or
          * remapped key) cannot lock out the rest of the pad. */
         if (     (input_st->flags & INP_FLAG_WAIT_INPUT_RELEASE)
               && (input_st->wait_release_mask[port] & (1u << i)))
            continue;

         BIT256_SET_PTR(p_new_state, i);
      }
   }

   /* Drop released buttons from the captured set; on the arming
    * frame this narrows 0xFFFF down to what is actually held. */
   input_st->wait_release_mask[port] &= held_now;

   /* Allow menu toggle to bypass 'enable_hotkey' when it is
    * not part of the usual buttons, unless 'enable_hotkey'
    * is set in autoconf. */
   if (     !any_pressed
         && !(input_st->flags & INP_FLAG_WAIT_INPUT_RELEASE)
         && (binds[port][RARCH_MENU_TOGGLE].joykey != NO_BTN)
         && (  input_autoconf_binds[joy_idx][RARCH_ENABLE_HOTKEY].joykey == binds[port][RARCH_MENU_TOGGLE].joykey
            || input_autoconf_binds[joy_idx][RARCH_ENABLE_HOTKEY].joykey == NO_BTN))
   {
      /* Ignore keyboard menu toggle button and check
       * joypad menu toggle button for pressing
       * it without 'enable_hotkey', because Guide button
       * is not part of the usual buttons. */
      i = RARCH_MENU_TOGGLE;

      if (!(RETRO_KEYBIND_VALID(&binds[port][i])
            && input_state_wrap(
                  input_st->current_driver,
                  input_st->current_data,
                  joypad,
                  sec_joypad,
                  joypad_info,
                  binds,
                  !!(input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED),
                  port, RETRO_DEVICE_KEYBOARD, 0,
                  RETRO_KEYBIND_KEY(&input_config_binds[port][i]))))
      {
         bool bit_pressed = RETRO_KEYBIND_VALID(&binds[port][i])
               && input_state_wrap(
                     input_st->current_driver,
                     input_st->current_data,
                     joypad,
                     sec_joypad,
                     joypad_info,
                     binds,
                     !!(input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED),
                     port, RETRO_DEVICE_JOYPAD, 0, i);

         if (     bit_pressed
               || (i == RARCH_MENU_TOGGLE && input_st->platform_menu_button)
               || input_keys_pressed_other_sources(input_st, i, p_new_state))
         {
            if (!(input_st->flags & INP_FLAG_MENU_PRESS_PENDING))
               input_st->flags &= ~INP_FLAG_MENU_PRESS_CANCEL;

            input_st->flags |= INP_FLAG_MENU_PRESS_PENDING;
         }
         else if (input_st->flags & INP_FLAG_MENU_PRESS_PENDING)
            /* Also set 'enable_hotkey' to prevent hotkey delay untrigger */
            BIT256_SET_PTR(p_new_state, RARCH_ENABLE_HOTKEY);
      }
   }

   /* Ignore hotkey block delay when menu toggle and hotkey enabler share the same key */
   if (     !any_pressed
         && !(input_st->flags & INP_FLAG_WAIT_INPUT_RELEASE)
         && !(input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED)
         && RETRO_KEYBIND_KEY(&binds[port][RARCH_MENU_TOGGLE]) == RETRO_KEYBIND_KEY(&binds[port][RARCH_ENABLE_HOTKEY]))
   {
      i = RARCH_MENU_TOGGLE;

      if (     RETRO_KEYBIND_VALID(&binds[port][i])
            && input_state_wrap(
                  input_st->current_driver,
                  input_st->current_data,
                  joypad,
                  sec_joypad,
                  joypad_info,
                  binds,
                  !!(input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED),
                  port, RETRO_DEVICE_KEYBOARD, 0,
                  RETRO_KEYBIND_KEY(&input_config_binds[port][i])))
         input_st->flags |= INP_FLAG_MENU_PRESS_PENDING;
      else if (input_st->flags & INP_FLAG_MENU_PRESS_PENDING)
         /* Also set 'enable_hotkey' to prevent hotkey delay untrigger */
         BIT256_SET_PTR(p_new_state, RARCH_ENABLE_HOTKEY);
   }

   /* Hotkeys are only relevant for the first user or core port */
   if (port != hotkey_port)
      return;

   /* Check hotkeys to block keyboard and joypad hotkeys separately.
    * This looks complicated because hotkeys must be unblocked based
    * on the device type depending if 'enable_hotkey' is set or not.. */
   if (     input_st->flags & INP_FLAG_BLOCK_HOTKEY
         && (libretro_hotkey_set && keyboard_hotkey_set))
   {
      /* Block everything when hotkey bind exists for both device types */
      for (i = RARCH_FIRST_META_KEY; i < RARCH_BIND_LIST_END; i++)
      {
         block_hotkey[i] = true;

         /* Don't block controller hotkey enabler with Game Focus */
         CHECK_GAME_FOCUS_ENABLE_HOTKEY_COMBO(i);
      }
   }
   else if (input_st->flags & INP_FLAG_BLOCK_HOTKEY
         && (!libretro_hotkey_set || !keyboard_hotkey_set))
   {
      /* Block selectively when hotkey bind exists for either device type */
      for (i = RARCH_FIRST_META_KEY; i < RARCH_BIND_LIST_END; i++)
      {
         bool keyboard_hotkey_pressed = false;
         bool libretro_hotkey_pressed = false;

         /* Default */
         block_hotkey[i]              = true;

         /* No 'enable_hotkey' in joypad */
         if (!libretro_hotkey_set)
         {
            if (     binds[port][i].joykey  != NO_BTN
                  || binds[port][i].joyaxis != AXIS_NONE)
            {
               /* Allow blocking if keyboard hotkey is pressed */
               if (input_state_wrap(
                     input_st->current_driver,
                     input_st->current_data,
                     joypad,
                     sec_joypad,
                     joypad_info,
                     binds,
                     !!(input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED),
                     port, RETRO_DEVICE_KEYBOARD, 0,
                     RETRO_KEYBIND_KEY(&input_config_binds[port][i])))
               {
                  keyboard_hotkey_pressed = true;

                  /* Always block */
                  block_hotkey[i] = true;
               }

               /* Deny blocking if joypad hotkey is pressed */
               if (input_state_wrap(
                     input_st->current_driver,
                     input_st->current_data,
                     joypad,
                     sec_joypad,
                     joypad_info,
                     binds,
                     !!(input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED),
                     port, RETRO_DEVICE_JOYPAD, 0,
                     i))
               {
                  libretro_hotkey_pressed = true;

                  /* Only deny block if keyboard is not pressed */
                  if (!keyboard_hotkey_pressed)
                     block_hotkey[i] = false;
               }
            }
         }

         /* No 'enable_hotkey' in keyboard */
         if (!keyboard_hotkey_set)
         {
            if (RETRO_KEYBIND_KEY(&binds[port][i]) != RETROK_UNKNOWN)
            {
               /* Deny blocking if keyboard hotkey is pressed */
               if (input_state_wrap(
                     input_st->current_driver,
                     input_st->current_data,
                     joypad,
                     sec_joypad,
                     joypad_info,
                     binds,
                     !!(input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED),
                     port, RETRO_DEVICE_KEYBOARD, 0,
                     RETRO_KEYBIND_KEY(&input_config_binds[port][i])))
               {
                  keyboard_hotkey_pressed = true;

                  /* Only deny block if joypad is not pressed */
                  if (!libretro_hotkey_pressed)
                     block_hotkey[i] = false;
               }

               /* Allow blocking if joypad hotkey is pressed */
               if (input_state_wrap(
                     input_st->current_driver,
                     input_st->current_data,
                     joypad,
                     sec_joypad,
                     joypad_info,
                     binds,
                     !!(input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED),
                     port, RETRO_DEVICE_JOYPAD, 0,
                     i))
               {
                  /* Only block if keyboard is not pressed */
                  if (!keyboard_hotkey_pressed)
                     block_hotkey[i] = true;
               }
            }
         }

         /* Don't block controller hotkey enabler with Game Focus */
         CHECK_GAME_FOCUS_ENABLE_HOTKEY_COMBO(i);
      }
   }
   else
   {
      /* Clear everything */
      for (i = RARCH_FIRST_META_KEY; i < RARCH_BIND_LIST_END; i++)
         block_hotkey[i] = false;
   }

   if (!is_menu && RETRO_KEYBIND_VALID(&binds[port][RARCH_GAME_FOCUS_TOGGLE]))
   {
      /* Never block Game Focus toggle hotkey */
      block_hotkey[RARCH_GAME_FOCUS_TOGGLE] = false;
   }

   for (i = RARCH_FIRST_META_KEY; i < RARCH_BIND_LIST_END; i++)
   {
      bool other_pressed = input_keys_pressed_other_sources(input_st, i, p_new_state);
      bool bit_pressed   = RETRO_KEYBIND_VALID(&binds[port][i])
            && input_state_wrap(
                  input_st->current_driver,
                  input_st->current_data,
                  joypad,
                  sec_joypad,
                  joypad_info,
                  binds,
                  kb_blocked,
                  port, RETRO_DEVICE_JOYPAD, 0,
                  i);

      if (     bit_pressed
            || other_pressed
            || (i == RARCH_MENU_TOGGLE && input_st->platform_menu_button))
      {
         any_pressed = true;
         if (input_st->flags & INP_FLAG_WAIT_INPUT_RELEASE)
            continue;

         if (libretro_hotkey_set || keyboard_hotkey_set)
         {
            /* Do not block "other source" (input overlay) presses */
            if (block_hotkey[i] && !other_pressed)
               continue;
         }

         /* Set menu toggle on release */
         if (i == RARCH_MENU_TOGGLE)
         {
            if (!(input_st->flags & INP_FLAG_MENU_PRESS_PENDING))
            {
               input_st->flags |=  INP_FLAG_MENU_PRESS_PENDING;
               input_st->flags &= ~INP_FLAG_MENU_PRESS_CANCEL;
            }
            continue;
         }
         else if (i != RARCH_ENABLE_HOTKEY)
         {
            input_st->flags |= INP_FLAG_MENU_PRESS_CANCEL;

            /* Game Focus toggle is always allowed, so it must clear menu cancel */
            if (i == RARCH_GAME_FOCUS_TOGGLE)
               input_st->flags &= ~INP_FLAG_MENU_PRESS_CANCEL;
         }

         BIT256_SET_PTR(p_new_state, i);
      }
      else
      {
         if (i == RARCH_MENU_TOGGLE)
         {
            /* Untrigger menu if press was shorter than hotkey block delay */
            if (      (input_st->flags & INP_FLAG_MENU_PRESS_PENDING)
                  && !(input_st->flags & INP_FLAG_MENU_PRESS_CANCEL)
                  && !BIT256_GET_PTR(p_new_state, RARCH_ENABLE_HOTKEY)
                  && input_st->input_hotkey_block_counter
                  && input_st->input_hotkey_block_counter < input_hotkey_block_delay)
               input_st->flags &= ~INP_FLAG_MENU_PRESS_PENDING;

            if (input_st->flags & INP_FLAG_MENU_PRESS_PENDING)
            {
               /* Forget menu press if any other hotkey was pressed */
               if (!(input_st->flags & INP_FLAG_MENU_PRESS_CANCEL))
                  BIT256_SET_PTR(p_new_state, i);

               input_st->flags &= ~(INP_FLAG_MENU_PRESS_PENDING | INP_FLAG_MENU_PRESS_CANCEL);
            }
         }
      }
   }

   if (     (input_st->flags & INP_FLAG_WAIT_INPUT_RELEASE)
         && !input_st->wait_release_mask[port])
      input_st->flags &= ~INP_FLAG_WAIT_INPUT_RELEASE;

   if (input_st->flags & INP_FLAG_BLOCK_HOTKEY && !enable_hotkey_pressed)
      input_st->input_hotkey_block_counter = 0;

   } /* kb_blocked scope */
}

void input_driver_set_shader_uses_sensors(bool uses)
{
   retro_atomic_store_release_int(
         &input_driver_st.shader_uses_sensors, uses ? 1 : 0);
}

void input_driver_read_sensor_snapshot(float *gyro3,
      float *accel3, float *rest3)
{
   input_driver_state_t *input_st = &input_driver_st;
   for (;;)
   {
      int v[9];
      int i;
      float *dst[3];
      int s1 = retro_atomic_load_acquire_int(&input_st->sensor_snap_seq);
      if (s1 & 1)
         continue;
      for (i = 0; i < 9; i++)
         v[i] = retro_atomic_load_relaxed_int(
               &input_st->sensor_snap_bits[i]);
      retro_atomic_thread_fence_acquire();
      if (retro_atomic_load_relaxed_int(&input_st->sensor_snap_seq) != s1)
         continue;
      dst[0] = gyro3;
      dst[1] = accel3;
      dst[2] = rest3;
      for (i = 0; i < 9; i++)
         memcpy(&dst[i / 3][i % 3], &v[i], sizeof(float));
      return;
   }
}

#ifdef HAVE_THREADS
static void input_key_lane_take(void);
#endif

/* RetroPad buttons a first press is looked for on: all but the d-pad. */
#define INPUT_FIRST_PRESS_BUTTONS ( \
        ((1U << RARCH_FIRST_CUSTOM_BIND) - 1) \
      & ~(  (1U << RETRO_DEVICE_ID_JOYPAD_UP) \
          | (1U << RETRO_DEVICE_ID_JOYPAD_DOWN) \
          | (1U << RETRO_DEVICE_ID_JOYPAD_LEFT) \
          | (1U << RETRO_DEVICE_ID_JOYPAD_RIGHT)))
/* in first_press_pending: no press, every unmapped user gets its own port */
#define INPUT_FIRST_PRESS_GIVE_UP (1U << 31)
static bool input_first_press_blocked(void);

#ifdef HAVE_BSV_MOVIE
/* The replay is over: said by the code that reads it, which is handed
 * the replay and not the input state. */
void bsv_movie_set_end(void)
{
   input_driver_st.bsv_movie_state.flags |= BSV_FLAG_MOVIE_END;
}
#endif

/* The settings a driver reads, by name. A driver does not take the
 * settings (config_get_ptr()) and pick a field out: what drivers
 * depend on is this list, and the layout of the settings is the
 * frontend's business. Each answers with the default before the
 * settings are there. */
unsigned input_config_get_mouse_index(unsigned port)
{
   settings_t *settings = config_get_ptr();
   return (settings && port < MAX_USERS)
      ? settings->uints.input_mouse_index[port] : 0;
}

unsigned input_config_get_joypad_index(unsigned port)
{
   settings_t *settings = config_get_ptr();
   return (settings && port < MAX_USERS)
      ? settings->uints.input_joypad_index[port] : port;
}

unsigned input_config_get_rumble_gain(void)
{
   settings_t *settings = config_get_ptr();
   return settings ? settings->uints.input_rumble_gain : DEFAULT_RUMBLE_GAIN;
}

#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
bool input_config_get_nowinkey_enable(void)
{
   settings_t *settings = config_get_ptr();
   return settings && settings->bools.input_nowinkey_enable;
}
#endif

#if defined(_WIN32) && defined(HAVE_WINRAWINPUT)
bool input_config_get_keyboard_background(void)
{
   settings_t *settings = config_get_ptr();
   return settings && settings->bools.input_keyboard_background;
}

bool input_config_get_winraw_xinput_enable(void)
{
   settings_t *settings = config_get_ptr();
   return settings && settings->bools.input_winraw_xinput_enable;
}

bool input_config_get_winraw_player_lights(void)
{
   settings_t *settings = config_get_ptr();
   return settings && settings->bools.input_winraw_player_lights;
}
#endif

bool input_config_get_sdl3_system_keyboard(void)
{
   settings_t *settings = config_get_ptr();
   return settings && settings->bools.input_sdl3_system_keyboard;
}

/* Sensors are allowed; and they are, until the settings say not. */
bool input_config_get_sensors_enable(void)
{
   settings_t *settings = config_get_ptr();
   return !settings || settings->bools.input_sensors_enable;
}

unsigned input_config_get_block_timeout(void)
{
   settings_t *settings = config_get_ptr();
   return settings ? settings->uints.input_block_timeout : 0;
}

/* The device itself may vibrate, where it can. */
bool input_config_get_device_vibration(void)
{
   settings_t *settings = config_get_ptr();
   return settings && settings->bools.enable_device_vibration;
}

#ifdef ANDROID
bool input_config_get_stylus_enable(void)
{
   settings_t *settings = config_get_ptr();
   return settings && settings->bools.input_stylus_enable;
}

bool input_config_get_stylus_require_contact_for_click(void)
{
   settings_t *settings = config_get_ptr();
   return settings && settings->bools.input_stylus_require_contact_for_click;
}

bool input_config_get_stylus_hover_moves_pointer(void)
{
   settings_t *settings = config_get_ptr();
   return settings && settings->bools.input_stylus_hover_moves_pointer;
}

unsigned input_config_get_stylus_pressure_sensitivity(void)
{
   settings_t *settings = config_get_ptr();
   return settings ? settings->uints.input_stylus_pressure_sensitivity : 0;
}

bool input_config_get_android_disconnect_workaround(void)
{
   settings_t *settings = config_get_ptr();
   return settings && settings->bools.android_input_disconnect_workaround;
}

/* The physical keyboard the user named, as "vid:pid", or "". */
const char *input_config_get_android_physical_keyboard(void)
{
   settings_t *settings = config_get_ptr();
   return settings ? settings->arrays.input_android_physical_keyboard : "";
}
#endif

unsigned input_config_get_split_joycon(unsigned port)
{
   settings_t *settings = config_get_ptr();
   return (settings && port < MAX_USERS)
      ? settings->uints.input_split_joycon[port] : 0;
}

bool input_config_get_backtouch_enable(void)
{
   settings_t *settings = config_get_ptr();
   return settings && settings->bools.input_backtouch_enable;
}

bool input_config_get_backtouch_toggle(void)
{
   settings_t *settings = config_get_ptr();
   return settings && settings->bools.input_backtouch_toggle;
}

bool input_config_get_keyboard_gamepad_enable(void)
{
   settings_t *settings = config_get_ptr();
   return settings && settings->bools.input_keyboard_gamepad_enable;
}

bool input_config_get_small_keyboard_enable(void)
{
   settings_t *settings = config_get_ptr();
   return settings && settings->bools.input_small_keyboard_enable;
}

unsigned input_config_get_keyboard_gamepad_mapping_type(void)
{
   settings_t *settings = config_get_ptr();
   return settings ? settings->uints.input_keyboard_gamepad_mapping_type : 0;
}

/* The screensaver is to be kept away. */
bool input_config_get_suspend_screensaver_enable(void)
{
   settings_t *settings = config_get_ptr();
   return settings && settings->bools.ui_suspend_screensaver_enable;
}

#ifdef GEKKO
unsigned input_config_get_mouse_scale(void)
{
   settings_t *settings = config_get_ptr();
   return settings ? settings->uints.input_mouse_scale : 1;
}
#endif

#ifdef UDEV_TOUCH_SUPPORT
bool input_config_get_touch_vmouse_pointer(void)
{ settings_t *s = config_get_ptr(); return s && s->bools.input_touch_vmouse_pointer; }
bool input_config_get_touch_vmouse_mouse(void)
{ settings_t *s = config_get_ptr(); return s && s->bools.input_touch_vmouse_mouse; }
bool input_config_get_touch_vmouse_touchpad(void)
{ settings_t *s = config_get_ptr(); return s && s->bools.input_touch_vmouse_touchpad; }
bool input_config_get_touch_vmouse_trackball(void)
{ settings_t *s = config_get_ptr(); return s && s->bools.input_touch_vmouse_trackball; }
bool input_config_get_touch_vmouse_gesture(void)
{ settings_t *s = config_get_ptr(); return s && s->bools.input_touch_vmouse_gesture; }
#endif

/* A joypad driver puts a port back on a controller: on the Wii and
 * GameCube, port 1 on the first pad when only one is left. */
void input_config_set_joypad_index(unsigned port, unsigned idx)
{
   settings_t *settings = config_get_ptr();
   if (settings && port < MAX_USERS)
      settings->uints.input_joypad_index[port] = idx;
}

/* An input driver that found no device at all, on the very first
 * start, before there is a configuration file: the configured input
 * driver becomes @ident, so that the next start does not come up with
 * the same driver and nothing to control it with. Returns whether this
 * is such a start and it was done. */
bool input_driver_first_start_fallback(const char *ident)
{
   settings_t *settings = config_get_ptr();
   if (!settings || !settings->bools.menu_show_start_screen || !ident)
      return false;
   strlcpy(settings->arrays.input_driver, ident,
         sizeof(settings->arrays.input_driver));
   return true;
}

/* An overlay is switched on and one is chosen. */
bool input_config_overlay_configured(void)
{
   settings_t *settings = config_get_ptr();
   return     settings
           && settings->bools.input_overlay_enable
           && *settings->paths.path_overlay;
}

/* The joypad driver the user chose, or NULL. */
const char *input_config_get_joypad_driver(void)
{
   settings_t *settings = config_get_ptr();
   return settings ? settings->arrays.input_joypad_driver : NULL;
}

const char *input_config_get_keyboard_layout(void)
{
   settings_t *settings = config_get_ptr();
   return settings ? settings->arrays.input_keyboard_layout : "";
}

/* The directory of controller profiles, or NULL when profiles are
 * switched off or there is none. */
const char *input_config_get_autoconfig_dir(void)
{
   settings_t *settings = config_get_ptr();
   if (     !settings
         || !settings->bools.input_autodetect_enable
         || settings->paths.directory_autoconfig[0] == '\0')
      return NULL;
   return settings->paths.directory_autoconfig;
}

#ifdef HAVE_TEST_DRIVERS
/* The script of the test input driver, or of the test joypad driver. */
const char *input_config_get_test_input_file(bool joypad)
{
   settings_t *settings = config_get_ptr();
   if (!settings)
      return "";
   return joypad ? settings->paths.test_input_file_joypad
                 : settings->paths.test_input_file_general;
}
#endif

/* What a driver may ask the frontend, and tell it, without taking its
 * state: each is one thing, by name. A driver that needs something
 * that is not here gets a call added here, not the state pointer. */

/* The platform's own on-screen keyboard: whether there is one to show,
 * and whether it is up. */
void input_driver_set_native_keyboard_available(bool available)
{
   if (available)
      input_driver_st.flags |=  INP_FLAG_NATIVE_KB_AVAIL;
   else
      input_driver_st.flags &= ~INP_FLAG_NATIVE_KB_AVAIL;
}

void input_driver_set_native_keyboard_shown(bool shown)
{
   if (shown)
      input_driver_st.flags |=  INP_FLAG_NATIVE_KB_SHOWN;
   else
      input_driver_st.flags &= ~INP_FLAG_NATIVE_KB_SHOWN;
}

bool input_driver_native_keyboard_shown(void)
{
   return (input_driver_st.flags & INP_FLAG_NATIVE_KB_SHOWN) != 0;
}

/* A line of text is being typed: the keys go to it and not to the
 * binds. */
bool input_driver_keyboard_mapping_blocked(void)
{
   return (input_driver_st.flags & INP_FLAG_KB_MAPPING_BLOCKED) != 0;
}

bool input_driver_keyboard_line_enabled(void)
{
   return input_driver_st.keyboard_line.enabled;
}

/* The platform's keyboard hands over the whole line as it stands. */
void input_driver_keyboard_line_set(const char *utf8, size_t len)
{
   input_keyboard_line_clear(&input_driver_st);
   if (len)
      input_keyboard_line_append(&input_driver_st.keyboard_line, utf8, len);
}

/* The platform's keyboard is done with the line: it is let go and the
 * binds get the keys again. */
void input_driver_keyboard_line_end(void)
{
   input_keyboard_line_free(&input_driver_st);
   input_driver_st.flags &= ~INP_FLAG_KB_MAPPING_BLOCKED;
}

bool input_driver_pointer_input_blocked(void)
{
   return (input_driver_st.flags & INP_FLAG_BLOCK_POINTER_INPUT) != 0;
}

bool input_driver_game_focus_enabled(void)
{
   return input_driver_st.game_focus_state.enabled;
}

bool input_driver_mouse_grabbed(void)
{
   return (input_driver_st.flags & INP_FLAG_GRAB_MOUSE_STATE) != 0;
}

/* The input driver's own data, for a joypad driver that is part of it. */
void *input_driver_current_data(void)
{
   return input_driver_st.current_data;
}

/* The platform's own menu button is held, or is not: the Home button
 * of a Wii Remote, the console's reset button on a GameCube, a tap on
 * a 3DS's bottom screen, the menu key of a handheld. It is not one of
 * a controller's buttons and has no bind; held, it counts as the Menu
 * Toggle hotkey held. Called by the driver that reads the button, on
 * the frontend's thread. */
void input_driver_set_platform_menu_button(bool held)
{
   input_driver_st.platform_menu_button = held;
}

/* Reads of the input driver for a consumer that is not the core: the
 * menu, which used to call the driver itself with the driver, its data
 * and the joypads taken out of the input state. It asks here instead,
 * and holds none of them.
 *
 * Whether there is a driver to read at all. */
bool input_driver_has_device_state(void)
{
   return     input_driver_st.current_driver
           && input_driver_st.current_driver->input_state;
}

/* A mouse, a pointer or a key: @device, @idx and @id as libretro has
 * them. No binds go to the driver, so no pad's button is read through
 * its bind. Only while input_driver_has_device_state(). */
int16_t input_driver_device_state(unsigned port,
      unsigned device, unsigned idx, unsigned id)
{
   rarch_joypad_info_t joypad_info;
   input_driver_state_t *input_st          = &input_driver_st;
   const input_device_driver_t *joypad     = INPUT_JOYPAD_FOR_READ(
         input_st, input_st->primary_joypad);
#ifdef HAVE_MFI
   const input_device_driver_t *sec_joypad = INPUT_JOYPAD_FOR_READ(
         input_st, input_st->secondary_joypad);
#else
   const input_device_driver_t *sec_joypad = NULL;
#endif

   joypad_info.joy_idx        = 0;
   joypad_info.auto_binds     = NULL;
   joypad_info.axis_threshold = 0.0f;

   return input_st->current_driver->input_state(
         input_st->current_data,
         joypad, sec_joypad, &joypad_info,
         NULL,
         (input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED) != 0,
         port, device, idx, id);
}

/* The same for the capture of a bind, which reads the mouse buttons
 * and the keys with the frontend's binds and the pad @joy_idx in
 * hand, as it always has. Only while input_driver_has_device_state(). */
int16_t input_driver_bind_capture_state(unsigned joy_idx, unsigned port,
      unsigned device, unsigned idx, unsigned id)
{
   rarch_joypad_info_t joypad_info;
   input_driver_state_t *input_st          = &input_driver_st;
   settings_t *settings                    = config_get_ptr();
   const input_device_driver_t *joypad     = input_st->primary_joypad;
#ifdef HAVE_MFI
   const input_device_driver_t *sec_joypad = input_st->secondary_joypad;
#else
   const input_device_driver_t *sec_joypad = NULL;
#endif

   if (joy_idx >= MAX_USERS)
      joy_idx                 = 0;
   joypad_info.axis_threshold = settings->floats.input_axis_threshold;
   joypad_info.joy_idx        = joy_idx;
   joypad_info.auto_binds     = input_autoconf_binds[joy_idx];

   return input_st->current_driver->input_state(
         input_st->current_data,
         joypad, sec_joypad, &joypad_info,
         (*input_st->libretro_input_binds),
         (input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED) != 0,
         port, device, idx, id);
}

/* A controller as the controller has it - its own buttons, axes and
 * hats, not the RetroPad's - for the capture of a bind. The menu read
 * the joypad drivers for this itself, and polled them; it asks here.
 *
 * Each array is filled when it is given: @buttons with whether each
 * is down, @axes with each axis's position, and each of @hats has the
 * mask of its directions held OR-ed in. With @poll the joypad drivers
 * are polled first, as the capture does once a frame. Where there are
 * two joypad drivers the second is read after the first, over it. */
void input_driver_capture_pad(unsigned pad, bool poll,
      bool *buttons, unsigned num_buttons,
      int16_t *axes, unsigned num_axes,
      uint16_t *hats, unsigned num_hats)
{
   unsigned i, d;
   const input_device_driver_t *drivers[2];

   drivers[0] = input_driver_st.primary_joypad;
#ifdef HAVE_MFI
   drivers[1] = input_driver_st.secondary_joypad;
#else
   drivers[1] = NULL;
#endif

   for (d = 0; d < 2; d++)
   {
      const input_device_driver_t *joypad = drivers[d];

      if (!joypad)
         continue;
      if (poll && joypad->poll)
         joypad->poll();

      if (buttons)
         for (i = 0; i < num_buttons; i++)
            buttons[i] = joypad->button(pad, (uint16_t)i);

      if (axes)
         for (i = 0; i < num_axes; i++)
         {
            axes[i]  = joypad->axis(pad, AXIS_POS(i));
            axes[i] += joypad->axis(pad, AXIS_NEG(i));
         }

      if (hats)
         for (i = 0; i < num_hats; i++)
         {
            if (joypad->button(pad, HAT_MAP(i, HAT_UP_MASK)))
               hats[i] |= HAT_UP_MASK;
            if (joypad->button(pad, HAT_MAP(i, HAT_DOWN_MASK)))
               hats[i] |= HAT_DOWN_MASK;
            if (joypad->button(pad, HAT_MAP(i, HAT_LEFT_MASK)))
               hats[i] |= HAT_LEFT_MASK;
            if (joypad->button(pad, HAT_MAP(i, HAT_RIGHT_MASK)))
               hats[i] |= HAT_RIGHT_MASK;
         }
   }
}

/* The device drivers are polled, and nothing else of a poll is done:
 * for a driver restart, which must not act on what was read before
 * it. */
void input_driver_poll_devices(void)
{
   input_driver_state_t *input_st          = &input_driver_st;
   const input_device_driver_t *joypad     = input_st->primary_joypad;
#ifdef HAVE_MFI
   const input_device_driver_t *sec_joypad = input_st->secondary_joypad;
#else
   const input_device_driver_t *sec_joypad = NULL;
#endif

   if (joypad && joypad->poll)
      joypad->poll();
   if (sec_joypad && sec_joypad->poll)
      sec_joypad->poll();
   if (input_st->current_driver && input_st->current_driver->poll)
      input_st->current_driver->poll(input_st->current_data);
}

/* A controller's profile is looked up again, as if it had just been
 * connected: after the menu has saved one for it. */
void input_driver_autoconfigure_pad(unsigned pad)
{
   const input_device_driver_t *joypad = input_driver_st.primary_joypad;

   if (joypad)
      input_autoconfigure_connect(joypad->name(pad),
            NULL, NULL, joypad->ident,
            pad, 0, 0);
}

/* The RetroPad controls a user's controller and keys hold right now, as
 * they are bound: before remaps, turbo and the rest. One bit each, by
 * the control's number: the sixteen buttons, then the sticks'
 * directions (RARCH_ANALOG_LEFT_X_PLUS on), a direction counting once
 * the stick is pushed half way. For the menu, which asks while it is
 * waiting for a press and at no other time; the poll does none of
 * this. */
uint32_t input_driver_user_controls_bound(unsigned user)
{
   unsigned idx;
   uint32_t held;
   rarch_joypad_info_t joypad_info;
   input_driver_state_t *input_st          = &input_driver_st;
   settings_t *settings                    = config_get_ptr();
   const input_device_driver_t *joypad     = INPUT_JOYPAD_FOR_READ(
         input_st, input_st->primary_joypad);
#ifdef HAVE_MFI
   const input_device_driver_t *sec_joypad = INPUT_JOYPAD_FOR_READ(
         input_st, input_st->secondary_joypad);
#else
   const input_device_driver_t *sec_joypad = NULL;
#endif

   if (user >= MAX_USERS || !input_st->current_driver)
      return 0;

   joypad_info.axis_threshold = settings->floats.input_axis_threshold;
   joypad_info.joy_idx        = settings->uints.input_joypad_index[user];
   if (joypad_info.joy_idx >= MAX_USERS)
      joypad_info.joy_idx     = 0;
   joypad_info.auto_binds     = input_autoconf_binds[joypad_info.joy_idx];

   held = (uint32_t)input_state_wrap(input_st->current_driver,
         input_st->current_data,
         joypad, sec_joypad, &joypad_info,
         (*input_st->libretro_input_binds),
         false, user, RETRO_DEVICE_JOYPAD, 0,
         RETRO_DEVICE_ID_JOYPAD_MASK) & 0xFFFF;

   /* the sticks: the controller's, or keys bound to their directions */
   for (idx = 0; idx < 2; idx++)
   {
      unsigned first = RARCH_ANALOG_LEFT_X_PLUS + (idx * 4);
      int16_t x      = 0;
      int16_t y      = 0;

      if (joypad)
         input_joypad_analog_stick(ANALOG_DPAD_NONE,
               settings->floats.input_analog_deadzone,
               settings->floats.input_analog_sensitivity,
               joypad, &joypad_info, idx,
               (*input_st->libretro_input_binds[user]), &x, &y);
      if (!x)
         x = (int16_t)input_state_wrap(input_st->current_driver,
               input_st->current_data,
               joypad, sec_joypad, &joypad_info,
               (*input_st->libretro_input_binds),
               false, user, RETRO_DEVICE_ANALOG, idx,
               RETRO_DEVICE_ID_ANALOG_X);
      if (!y)
         y = (int16_t)input_state_wrap(input_st->current_driver,
               input_st->current_data,
               joypad, sec_joypad, &joypad_info,
               (*input_st->libretro_input_binds),
               false, user, RETRO_DEVICE_ANALOG, idx,
               RETRO_DEVICE_ID_ANALOG_Y);

      if (x >  0x4000)
         held |= (1U << first);
      else if (x < -0x4000)
         held |= (1U << (first + 1));
      if (y >  0x4000)
         held |= (1U << (first + 2));
      else if (y < -0x4000)
         held |= (1U << (first + 3));
   }

   return held;
}

/* Whether a user's mapping gives the remap work in the poll anything
 * to do.
 *
 * That work reads the whole pad - every button, the analog buttons,
 * both sticks - and then looks for controls that are mapped to another
 * one. With every control left where it is there are none, the result
 * is always "nothing mapped", and the reads were nine tenths of what a
 * poll cost: for every user, every poll, whether or not anyone had
 * remapped anything. What a core reads of an unmapped control does not
 * come from here; it is read on demand.
 *
 * So: a pad or analog device has work when a button or an axis is
 * mapped away from itself, and a keyboard device when a control has a
 * key. A row that differs only in controls that are switched off is
 * counted as having work, which costs what it always did. */
static bool input_remap_user_has_work(const settings_t *settings,
      const input_driver_state_t *input_st, unsigned user, unsigned device)
{
   /* every button and axis mapped to itself; one compare of the row
    * against this is cheaper than a walk that stops at the first
    * difference, since on most polls there is none to stop at */
   static const unsigned unmapped_row[RARCH_FIRST_CUSTOM_BIND + 8] = {
       0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11,
      12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23 };
   unsigned j;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
      case RETRO_DEVICE_ANALOG:
#ifdef HAVE_ACCESSIBILITY
         /* buttons the frontend presses for the user go out
          * through the mapper */
         if (user == 0 && input_st->gamepad_input_override)
            return true;
#endif
         return memcmp(settings->uints.input_remap_ids[user],
               unmapped_row, sizeof(unmapped_row)) != 0;
      case RETRO_DEVICE_KEYBOARD:
         {
            const unsigned *keys = settings->uints.input_keymapper_ids[user];
            for (j = 0; j < RARCH_CUSTOM_BIND_LIST_END; j++)
               if (keys[j] != RETROK_UNKNOWN)
                  return true;
         }
         return false;
      default:
         break;
   }
   /* no other device is remapped here */
   return false;
}

void input_driver_poll(void)
{
   size_t i, j;
   rarch_joypad_info_t joypad_info[MAX_USERS];
   input_driver_state_t *input_st = &input_driver_st;
   const input_driver_t *input    = (const input_driver_t*)
	      input_st->current_driver;
   settings_t *settings           = config_get_ptr();
   const input_device_driver_t
      *joypad                     = input_st->primary_joypad;
#ifdef HAVE_MFI
   const input_device_driver_t
      *sec_joypad                 = input_st->secondary_joypad;
#else
   const input_device_driver_t
      *sec_joypad                 = NULL;
#endif
   bool input_remap_binds_enable  = settings->bools.input_remap_binds_enable;
   float input_axis_threshold     = settings->floats.input_axis_threshold;
   /* Clamped: the arrays walked below are [MAX_USERS] and the setting
    * comes from the config file. */
   uint8_t max_users              = (settings->uints.input_max_users
         > MAX_USERS) ? MAX_USERS
         : (uint8_t)settings->uints.input_max_users;

#ifdef HAVE_THREADS
   /* a restart of the joypad driver that another thread asked for:
    * before anything of it is polled or read */
   if (joypad_driver_reinit_take())
   {
      joypad                      = input_st->primary_joypad;
#ifdef HAVE_MFI
      sec_joypad                  = input_st->secondary_joypad;
#endif
   }
#endif

   if (joypad && joypad->poll)
      joypad->poll();
   if (sec_joypad && sec_joypad->poll)
      sec_joypad->poll();
   if (input && input->poll)
      input->poll(input_st->current_data);

#ifdef HAVE_THREADS
   /* the keys another thread has reported since the last poll */
   input_key_lane_take();
#endif

   /* When the devices were read. The statistics show how old the input
    * a frame was made from is by the time that frame is on screen, and
    * measure from here - wherever the poll mode puts this call: before
    * retro_run, in the core's input_poll, or in its first input_state.
    * Only while the statistics are shown: nothing else reads it, and
    * it is a clock read a frame. */
   input_st->poll_time_us         = settings->bools.video_statistics_show
      ? cpu_features_get_time_usec() : 0;

   /* The real drivers are polled regardless, so their state stays
    * current and nothing is replayed on refocus; everything read
    * below goes through the stand-in while controllers are gated. */
   input_driver_update_joypad_focus(input_st, settings);
   joypad                         = INPUT_JOYPAD_FOR_READ(input_st, joypad);
   sec_joypad                     = INPUT_JOYPAD_FOR_READ(input_st, sec_joypad);

   /* Invalidate joypad state bitmask cache for the new frame */
   memset(&input_st->frame_valid, 0, sizeof(input_st->frame_valid));

   /* Sensors. Three things can need doing here, and on most polls none
    * does: nothing has asked for sensors, and the poll then pays for
    * one setting, one flag and two rates.
    *
    * - Sensors are turned on or off at the driver when what is wanted
    *   of them changes: a shader that reads them, or a core that asked
    *   for them. The setting gates both.
    * - The accelerometer's rest position is averaged over the frames
    *   after they come on.
    * - A shader that reads sensors is handed a snapshot each poll. It
    *   is the only reader of one, so with no such shader there is none
    *   to make: the six reads and the publish used to run every poll
    *   with the setting on, which is its default. When the shader goes,
    *   noughts are published once and that is the end of it. */
   {
      bool sensors_on  = settings->bools.input_sensors_enable;
      bool shader_uses = sensors_on && retro_atomic_load_acquire_int(
               &input_st->shader_uses_sensors);
      bool want        = shader_uses
         || (sensors_on && (   input_st->core_accel_rate
                            || input_st->core_gyro_rate));

      if (want != input_st->frontend_sensors_enabled)
      {
         if (want)
         {
            input_set_sensor_state(0, RETRO_SENSOR_ACCELEROMETER_ENABLE,
                  input_st->core_accel_rate ? input_st->core_accel_rate : 60);
            input_set_sensor_state(0, RETRO_SENSOR_GYROSCOPE_ENABLE,
                  input_st->core_gyro_rate ? input_st->core_gyro_rate : 60);
            input_st->frontend_sensors_enabled = true;
            input_sensor_start_rest_capture();
         }
         else
         {
            input_set_sensor_state(0, RETRO_SENSOR_ACCELEROMETER_DISABLE, 0);
            input_set_sensor_state(0, RETRO_SENSOR_GYROSCOPE_DISABLE, 0);
            input_st->frontend_sensors_enabled = false;
         }
      }

      /* Update accelerometer rest position capture (runs for ~30 frames
       * after the sensors come on, then stops) */
      if (input_st->rest_capturing)
         input_sensor_update_rest_capture(settings);

      if (shader_uses || !input_st->sensor_snap_quiet)
      {
         if (shader_uses)
         {
            /* Cache sensor values so shader backends on the video thread
             * read a consistent per-frame snapshot instead of calling
             * into the input subsystem directly. */
            input_st->sensor_gyroscope_cache[0]     = input_get_sensor_state_internal(settings, 0, RETRO_SENSOR_GYROSCOPE_X);
            input_st->sensor_gyroscope_cache[1]     = input_get_sensor_state_internal(settings, 0, RETRO_SENSOR_GYROSCOPE_Y);
            input_st->sensor_gyroscope_cache[2]     = input_get_sensor_state_internal(settings, 0, RETRO_SENSOR_GYROSCOPE_Z);
            input_st->sensor_accelerometer_cache[0] = input_get_sensor_state_internal(settings, 0, RETRO_SENSOR_ACCELEROMETER_X);
            input_st->sensor_accelerometer_cache[1] = input_get_sensor_state_internal(settings, 0, RETRO_SENSOR_ACCELEROMETER_Y);
            input_st->sensor_accelerometer_cache[2] = input_get_sensor_state_internal(settings, 0, RETRO_SENSOR_ACCELEROMETER_Z);
         }
         else
         {
            memset(input_st->sensor_gyroscope_cache,     0,
                  sizeof(input_st->sensor_gyroscope_cache));
            memset(input_st->sensor_accelerometer_cache, 0,
                  sizeof(input_st->sensor_accelerometer_cache));
         }
         input_st->sensor_snap_quiet = !shader_uses;

         /* Publish the three vec3s as one coherent snapshot: the shader
          * backends read them from the video thread per frame, and plain
          * float stores could hand them a vector mixing two polls (and
          * carried no happens-before at all). Same seq discipline as the
          * video viewport snapshots. */
         {
            int seq = retro_atomic_load_relaxed_int(&input_st->sensor_snap_seq);
            float *src[3];
            int i, j;
            src[0] = input_st->sensor_gyroscope_cache;
            src[1] = input_st->sensor_accelerometer_cache;
            src[2] = input_st->sensor_accelerometer_rest;
            retro_atomic_store_relaxed_int(&input_st->sensor_snap_seq, seq + 1);
            retro_atomic_thread_fence_release();
            for (i = 0; i < 3; i++)
               for (j = 0; j < 3; j++)
               {
                  int b;
                  memcpy(&b, &src[i][j], sizeof(b));
                  retro_atomic_store_relaxed_int(
                        &input_st->sensor_snap_bits[i * 3 + j], b);
               }
            retro_atomic_thread_fence_release();
            retro_atomic_store_release_int(&input_st->sensor_snap_seq, seq + 2);
         }
      }
   }

#ifdef HAVE_OVERLAY
   if (      input_st->overlay_ptr
         && (input_st->overlay_ptr->flags & INPUT_OVERLAY_ALIVE))
   {
      unsigned input_analog_dpad_mode = settings->uints.input_analog_dpad_mode[0];
      float input_overlay_opacity     = (input_st->overlay_ptr->flags & INPUT_OVERLAY_IS_OSK)
         ? settings->floats.input_osk_overlay_opacity
         : settings->floats.input_overlay_opacity;

      switch (input_analog_dpad_mode)
      {
         case ANALOG_DPAD_LSTICK:
         case ANALOG_DPAD_RSTICK:
         case ANALOG_DPAD_LRSTICK:
         case ANALOG_DPAD_TWINSTICK:
            {
               unsigned mapped_port      = settings->uints.input_remap_ports[0];
               if (     mapped_port < MAX_USERS
                     && input_st->analog_requested[mapped_port])
                  input_analog_dpad_mode = ANALOG_DPAD_NONE;
            }
            break;
         case ANALOG_DPAD_LSTICK_FORCED:
            input_analog_dpad_mode       = ANALOG_DPAD_LSTICK;
            break;
         case ANALOG_DPAD_RSTICK_FORCED:
            input_analog_dpad_mode       = ANALOG_DPAD_RSTICK;
            break;
         case ANALOG_DPAD_LRSTICK_FORCED:
            input_analog_dpad_mode       = ANALOG_DPAD_LRSTICK;
            break;
         case ANALOG_DPAD_TWINSTICK_FORCED:
            input_analog_dpad_mode       = ANALOG_DPAD_TWINSTICK;
            break;
         default:
            break;
      }

      /* Under threaded video the pack's textures arrive after the
       * page was first shown; the page moves over to them here. */
      if (input_overlay_promote_textures(input_st->overlay_ptr))
         input_overlay_load_active_geom(
               input_st->overlay_ptr, input_overlay_opacity);
#ifdef HAVE_RPNG
      input_overlay_animate(input_st->overlay_ptr, cpu_features_get_time_usec());
#endif
      input_poll_overlay(
            !!(input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED),
            settings,
            input_st->overlay_ptr,
            input_overlay_opacity,
            input_analog_dpad_mode,
            settings->floats.input_axis_threshold);
   }
#endif

   input_st->turbo_btns.count++;

   if (input_st->flags & INP_FLAG_BLOCK_LIBRETRO_INPUT)
   {
      for (i = 0; i < max_users; i++)
      {
         input_st->turbo_btns.frame_enable[i] = 0;
         input_st->hold_btns.frame_enable[i]  = 0;
      }
      return;
   }

   /* Fused turbo/hold + remap loop.
    *
    * Previously these were two separate max_users iterations:
    *   1) turbo/hold: init joypad_info[i], call input_state_wrap ×2
    *   2) remap:      reuse joypad_info[i], do remap work
    * Now fused into a single pass to improve cache locality —
    * joypad_info[i], bind arrays, and joypad driver state stay hot
    * in L1 across all per-user work, and the loop overhead is halved. */
   {
      bool turbo_enable              = settings->bools.input_turbo_enable;
      uint16_t turbo_btn_id          = RARCH_TURBO_ENABLE;
      bool kb_blocked                = !!(input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED);
#ifdef HAVE_MENU
      bool menu_alive                =
         (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE) != 0;
#else
      bool menu_alive                = false;
#endif
      bool do_remap                  = input_remap_binds_enable && !menu_alive;
      bool first_press_blocked       = input_st->first_press_live
         && input_first_press_blocked();
#ifdef HAVE_OVERLAY
      input_overlay_t *overlay_pointer = (input_overlay_t*)input_st->overlay_ptr;
      bool poll_overlay              = (overlay_pointer &&
            (overlay_pointer->flags & INPUT_OVERLAY_ALIVE));
#endif
      input_mapper_t *handle         = &input_st->mapper;
      float input_analog_deadzone    = settings->floats.input_analog_deadzone;
      float input_analog_sensitivity = settings->floats.input_analog_sensitivity;

      if (settings->ints.input_turbo_bind != -1)
         turbo_btn_id = settings->ints.input_turbo_bind;

      for (i = 0; i < max_users; i++)
      {
         /* --- joypad_info init (shared by turbo/hold and remap) --- */
         joypad_info[i].axis_threshold        = input_axis_threshold;
         joypad_info[i].joy_idx               = settings->uints.input_joypad_index[i];
         if (joypad_info[i].joy_idx >= MAX_USERS)
            joypad_info[i].joy_idx            = 0;
         joypad_info[i].auto_binds            = input_autoconf_binds[joypad_info[i].joy_idx];

         /* --- Turbo button state --- */
         input_st->turbo_btns.frame_enable[i] =
                  RETRO_KEYBIND_VALID(&(*input_st->libretro_input_binds[i])[turbo_btn_id])
               && turbo_enable ?
            input_state_wrap(input_st->current_driver,
                  input_st->current_data,
                  joypad, sec_joypad, &joypad_info[i],
                  (*input_st->libretro_input_binds),
                  kb_blocked,
                  (unsigned)i,
                  RETRO_DEVICE_JOYPAD, 0, turbo_btn_id) : 0;

#ifdef HAVE_OVERLAY
         if (     (i == 0)
               && turbo_enable
               && overlay_pointer
               && (overlay_pointer->flags & INPUT_OVERLAY_ALIVE)
               && BIT256_GET(overlay_pointer->overlay_state.buttons, turbo_btn_id))
            input_st->turbo_btns.frame_enable[i] = true;
#endif

         /* --- Hold button modifier state --- */
         input_st->hold_btns.frame_enable[i] =
                  RETRO_KEYBIND_VALID(&(*input_st->libretro_input_binds[i])[RARCH_HOLD_ENABLE]) ?
            input_state_wrap(input_st->current_driver,
                  input_st->current_data,
                  joypad, sec_joypad, &joypad_info[i],
                  (*input_st->libretro_input_binds),
                  kb_blocked,
                  (unsigned)i,
                  RETRO_DEVICE_JOYPAD, 0, RARCH_HOLD_ENABLE) : 0;

#ifdef HAVE_OVERLAY
         if (     (i == 0)
               && overlay_pointer
               && (overlay_pointer->flags & INPUT_OVERLAY_ALIVE)
               && BIT256_GET(overlay_pointer->overlay_state.buttons, RARCH_HOLD_ENABLE))
            input_st->hold_btns.frame_enable[i] = true;
#endif

         /* --- First press on a user with no core port --- */
         if (first_press_blocked)
            input_st->first_press_pending = INPUT_FIRST_PRESS_GIVE_UP;
         else if (  input_st->first_press_live
               && !menu_alive
               && settings->uints.input_remap_ports[i] >= MAX_USERS
               && joypad)
         {
            /* The d-pad does not count: idle axes of some receivers
             * read as a direction. A button counts once it has been
             * seen released, so one that is stuck never does. */
            uint32_t held = (uint32_t)input_state_wrap(
                  input_st->current_driver,
                  input_st->current_data,
                  joypad, sec_joypad, &joypad_info[i],
                  (*input_st->libretro_input_binds),
                  kb_blocked,
                  (unsigned)i, RETRO_DEVICE_JOYPAD,
                  0, RETRO_DEVICE_ID_JOYPAD_MASK)
               & INPUT_FIRST_PRESS_BUTTONS;
            uint32_t edge;
#ifdef HAVE_NETWORKGAMEPAD
            /* a Network RetroPad is the user's controller too */
            uint32_t net  = input_st->remote
               ? ((uint32_t)input_st->remote_st_ptr.buttons[i]
                     & INPUT_FIRST_PRESS_BUTTONS) : 0;
            held         |= net;
#endif
            edge          = held & input_st->first_press_released[i];
            if (edge)
            {
               /* the user's controller, or its keys: both are this
                * user's, and the notification says which it was */
               uint32_t pad = (uint32_t)joypad->state(&joypad_info[i],
                     (*input_st->libretro_input_binds)[i], (unsigned)i);
               if (sec_joypad)
                  pad |= (uint32_t)sec_joypad->state(&joypad_info[i],
                        (*input_st->libretro_input_binds)[i], (unsigned)i);
               /* a key: not while the port's controller is there to
                * be pressed, if the keyboard is to wait for it */
#ifdef HAVE_NETWORKGAMEPAD
               pad |= net;
#endif
               if (     (edge & pad)
                     || !settings->uints.input_assign_ports_keyboard
                     || !joypad->query_pad
                     || !joypad->query_pad(joypad_info[i].joy_idx))
                  input_st->first_press_pending |= (1U << i);
            }
            input_st->first_press_released[i] |=
               ~held & INPUT_FIRST_PRESS_BUTTONS;
         }

         /* --- Remap work (conditional) --- */
         if (do_remap)
         {
         input_bits_t current_inputs;
         unsigned mapped_port            = settings->uints.input_remap_ports[i];
         /* mapped to no core port: nothing to remap */
         unsigned device                 = (mapped_port < MAX_USERS)
            ? (settings->uints.input_libretro_device[mapped_port] & RETRO_DEVICE_MASK)
            : RETRO_DEVICE_NONE;
         input_bits_t *p_new_state       = (input_bits_t*)&current_inputs;
         unsigned input_analog_dpad_mode = settings->uints.input_analog_dpad_mode[i];

         /* Nothing of this user's is mapped to anything else: there
          * is nothing to work out, and the pad is not read for it.
          * What the last poll that did have work left behind is
          * cleared, once. */
         if (!input_remap_user_has_work(settings, input_st,
                  (unsigned)i, device))
         {
            if (input_st->remap_worked & (1U << i))
            {
               BIT256_CLEAR_ALL(handle->buttons[i]);
               for (j = 0; j < 8; j++)
                  handle->analog_value[i][j] = 0;
               input_st->remap_worked &= ~(1U << i);
            }
            continue;
         }
         input_st->remap_worked |= (1U << i);

         /* Clear the whole state up front. The 'mapper' switch below
          * only reads p_new_state for the same device cases that the
          * 'device' switch fills in (KEYBOARD/JOYPAD/ANALOG), so no
          * uninitialised read can occur - but the two switches are
          * separate and the state is reached through p_new_state, so
          * GCC cannot correlate them and warns about a 'maybe
          * uninitialized' current_inputs at the BIT256_GET_PTR() in
          * the mapper switch. Clearing here rather than inside the
          * device switch case leaves the value defined on every path
          * and costs nothing extra for the devices that were already
          * clearing it. */
         BIT256_CLEAR_ALL_PTR(&current_inputs);

         switch (input_analog_dpad_mode)
         {
            case ANALOG_DPAD_LSTICK:
            case ANALOG_DPAD_RSTICK:
            case ANALOG_DPAD_LRSTICK:
            case ANALOG_DPAD_TWINSTICK:
               if (     mapped_port < MAX_USERS
                     && input_st->analog_requested[mapped_port])
                  input_analog_dpad_mode = ANALOG_DPAD_NONE;
               break;
            case ANALOG_DPAD_LSTICK_FORCED:
               input_analog_dpad_mode    = ANALOG_DPAD_LSTICK;
               break;
            case ANALOG_DPAD_RSTICK_FORCED:
               input_analog_dpad_mode    = ANALOG_DPAD_RSTICK;
               break;
            case ANALOG_DPAD_LRSTICK_FORCED:
               input_analog_dpad_mode    = ANALOG_DPAD_LRSTICK;
               break;
            case ANALOG_DPAD_TWINSTICK_FORCED:
               input_analog_dpad_mode    = ANALOG_DPAD_TWINSTICK;
               break;
            default:
               break;
         }

         switch (device)
         {
            case RETRO_DEVICE_KEYBOARD:
            case RETRO_DEVICE_JOYPAD:
            case RETRO_DEVICE_ANALOG:
               if (joypad)
               {
                  unsigned k;
                  int32_t ret = input_state_wrap(
                        input_st->current_driver,
                        input_st->current_data,
                        joypad,
                        sec_joypad,
                        &joypad_info[i],
                        (*input_st->libretro_input_binds),
                        kb_blocked,
                        (unsigned)i, RETRO_DEVICE_JOYPAD,
                        0, RETRO_DEVICE_ID_JOYPAD_MASK);

                  for (k = 0; k < RARCH_FIRST_CUSTOM_BIND; k++)
                  {
                     if (ret & (1 << k))
                     {
                        bool valid_bind  =
                           RETRO_KEYBIND_VALID(&(*input_st->libretro_input_binds[i])[k]);

                        if (valid_bind)
                        {
                           int16_t   val =
                              input_joypad_analog_button(
                                    input_analog_deadzone,
                                    input_analog_sensitivity,
                                    joypad,
                                    &joypad_info[i],
                                    k,
                                    &(*input_st->libretro_input_binds[i])[k]
                                    );
                           if (val)
                              p_new_state->analog_buttons[k] = val;
                        }

                        BIT256_SET_PTR(p_new_state, k);
                     }
                  }

                  /* Process both axes of each analog stick together,
                   * computing radial deadzone magnitude only once
                   * per stick instead of once per axis. */
                  for (k = 0; k < 2; k++)
                  {
                     int16_t stick_x = 0;
                     int16_t stick_y = 0;
                     if (input_joypad_analog_stick(
                              input_analog_dpad_mode,
                              input_analog_deadzone,
                              input_analog_sensitivity,
                              joypad, &joypad_info[i],
                              k, (*input_st->libretro_input_binds[i]),
                              &stick_x, &stick_y))
                     {
                        unsigned off_x = 0 + (k * 4);
                        unsigned off_y = 0 + (k * 4) + 2;
                        if (stick_x >= 0)
                           p_new_state->analogs[off_x]     = stick_x;
                        else
                           p_new_state->analogs[off_x + 1] = stick_x;
                        if (stick_y >= 0)
                           p_new_state->analogs[off_y]     = stick_y;
                        else
                           p_new_state->analogs[off_y + 1] = stick_y;
                     }
                  }
               }
               break;
            default:
               break;
         }

         /* mapper */
         switch (device)
         {
            /* keyboard to gamepad remapping */
            case RETRO_DEVICE_KEYBOARD:
               for (j = 0; j < RARCH_CUSTOM_BIND_LIST_END; j++)
               {
                  unsigned current_button_value;
                  unsigned remap_key =
                        settings->uints.input_keymapper_ids[i][j];

                  if (remap_key == RETROK_UNKNOWN)
                     continue;

                  if (j >= RARCH_FIRST_CUSTOM_BIND && j < RARCH_ANALOG_BIND_LIST_END)
                  {
                     int16_t current_axis_value = p_new_state->analogs[j - RARCH_FIRST_CUSTOM_BIND];
                     current_button_value = abs(current_axis_value) >
                           settings->floats.input_axis_threshold
                            * 32767;
                  }
                  else
                     current_button_value = BIT256_GET_PTR(p_new_state, j);

#ifdef HAVE_OVERLAY
                  if (poll_overlay && i == 0)
                  {
                     input_overlay_state_t *ol_state  = overlay_pointer
                        ? &overlay_pointer->overlay_state : NULL;
                     if (ol_state)
                        current_button_value |= BIT256_GET(ol_state->buttons, j);
                  }
#endif
                  /* Press */
                  if ((current_button_value == 1)
                        && !MAPPER_GET_KEY(handle, remap_key))
                  {
                     handle->key_button[remap_key] = (unsigned)j;

                     MAPPER_SET_KEY(handle, remap_key);
                     input_keyboard_event(true,
                           remap_key,
                           0, 0, RETRO_DEVICE_KEYBOARD);
                  }
                  /* Release */
                  else if ((current_button_value == 0)
                        && MAPPER_GET_KEY(handle, remap_key))
                  {
                     if (handle->key_button[remap_key] != j)
                        continue;

                     input_keyboard_event(false,
                           remap_key,
                           0, 0, RETRO_DEVICE_KEYBOARD);
                     MAPPER_UNSET_KEY(handle, remap_key);
                  }
               }
               break;

               /* gamepad remapping */
            case RETRO_DEVICE_JOYPAD:
            case RETRO_DEVICE_ANALOG:
               /* this loop iterates on all users and all buttons,
                * and checks if a pressed button is assigned to any
                * other button than the default one, then it sets
                * the bit on the mapper input bitmap, later on the
                * original input is cleared in input_state */
               BIT256_CLEAR_ALL(handle->buttons[i]);

               for (j = 0; j < 8; j++)
                  handle->analog_value[i][j] = 0;

               for (j = 0; j < RARCH_FIRST_CUSTOM_BIND; j++)
               {
                  bool remap_valid;
                  unsigned remap_button         =
                        settings->uints.input_remap_ids[i][j];
                  unsigned current_button_value =
                        BIT256_GET_PTR(p_new_state, j);

#ifdef HAVE_OVERLAY
                  if (poll_overlay && i == 0)
                  {
                     input_overlay_state_t *ol_state  =
                          overlay_pointer
                        ? &overlay_pointer->overlay_state
                        : NULL;
                     if (ol_state)
                        current_button_value |= BIT256_GET(ol_state->buttons, j);
                  }
#endif
                  remap_valid                   =
                        (current_button_value == 1)
                     && (j != remap_button)
                     && (remap_button != RARCH_UNMAPPED);

#ifdef HAVE_ACCESSIBILITY
                  /* gamepad override */
                  if (     (i == 0)
                        && input_st->gamepad_input_override & (1 << j))
                  {
                     BIT256_SET(handle->buttons[i], j);
                  }
#endif

                  if (remap_valid)
                  {
                     if (remap_button < RARCH_FIRST_CUSTOM_BIND)
                     {
                        /* A press with no pressure reading, e.g. from an
                         * overlay or the keyboard, counts as fully pressed. */
                        uint16_t pressure = p_new_state->analog_buttons[j]
                           ? p_new_state->analog_buttons[j]
                           : 0x7fff;

                        BIT256_SET(handle->buttons[i], remap_button);

                        /* If there are multiple physical buttons remapped to
                         * the same virtual button, e.g. A and B on the
                         * controller are both mapped to X, then whichever is
                         * being pressed hardest is what sets the pressure
                         * value for the mapped button. */
                        if (pressure > handle->buttons[i].analog_buttons[remap_button])
                           handle->buttons[i].analog_buttons[remap_button] = pressure;
                     }
                     else
                     {
                        unsigned remap_axis_bind =
                              remap_button - RARCH_FIRST_CUSTOM_BIND;
                        int      invert          = 1;

                        /* Bound: analog_value is int16_t[8].  remap_button
                         * comes from settings->uints.input_remap_ids[][]
                         * which is loaded from .rmp config files;
                         * input_remapping_load_file's config_get_int
                         * accepts any integer, so a malformed .rmp can
                         * supply remap_button anywhere in (RARCH_FIRST_
                         * CUSTOM_BIND, INT_MAX], producing a write at
                         * arbitrary offset past analog_value[i].  Reject
                         * out-of-range targets here. */
                        if (remap_axis_bind >= ARRAY_SIZE(handle->analog_value[i]))
                           continue;

                        if (remap_button % 2 != 0)
                           invert = -1;

                        handle->analog_value[i][remap_axis_bind] =
                              (p_new_state->analog_buttons[j]
                               ? p_new_state->analog_buttons[j]
                               : 32767) * invert;
                     }
                  }
               }

               for (j = 0; j < 8; j++)
               {
                  unsigned k                 = (unsigned)j + RARCH_FIRST_CUSTOM_BIND;
                  int16_t current_axis_value = p_new_state->analogs[j];
                  unsigned remap_axis        = settings->uints.input_remap_ids[i][k];

                  if (
                        (
                            abs(current_axis_value) > 0
                        && (k != remap_axis)
                        && (remap_axis != RARCH_UNMAPPED)
                        )
                     )
                  {
                     if (remap_axis < RARCH_FIRST_CUSTOM_BIND &&
                           abs(current_axis_value) >
                           settings->floats.input_axis_threshold
                            * 32767)
                     {
                        BIT256_SET(handle->buttons[i], remap_axis);
                     }
                     else
                     {
                        unsigned remap_axis_bind =
                           remap_axis - RARCH_FIRST_CUSTOM_BIND;

                        /* Pre-patch this read 'sizeof(handle->analog_value[i])'
                         * which is 16 (bytes), but the array has 8 elements;
                         * any remap_axis_bind in [8, 15] caused an OOB write.
                         * Use ARRAY_SIZE so the check is in elements. */
                        if (remap_axis_bind < ARRAY_SIZE(handle->analog_value[i]))
                        {
                           int invert = 1;
                           if (     (k % 2 == 0 && remap_axis % 2 != 0)
                                 || (k % 2 != 0 && remap_axis % 2 == 0)
                              )
                              invert = -1;

                           handle->analog_value[i][
                              remap_axis_bind] =
                                 current_axis_value * invert;
                        }
                     }
                  }

               }
               break;
            default:
               break;
         }
         } /* if (do_remap) */
      } /* for (i = 0; i < max_users; i++) */
   }

#ifdef HAVE_COMMAND
   for (i = 0; i < ARRAY_SIZE(input_st->command); i++)
   {
      if (input_st->command[i])
      {
         memset(input_st->command[i]->state,
                0, sizeof(input_st->command[i]->state));

         input_st->command[i]->poll(
            input_st->command[i]);
      }
   }
   command_owed_reply_poll();
#endif

#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
   if (input_st->remote)
      input_remote_poll(input_st, settings, max_users);
#endif
#ifdef HAVE_BSV_MOVIE
   if (BSV_MOVIE_IS_PLAYBACK_ON())
      bsv_movie_poll(input_st);
#endif
}

/* The quarter turns to turn a core's directions by: the setting, or
 * with 'Auto' what Video Rotation is set to. */
static unsigned input_rotation_turns(settings_t *settings)
{
   unsigned turns = settings->uints.input_rotation;
   if (turns == INPUT_ROTATION_AUTO)
      turns = settings->uints.video_rotation;
   return turns & 3;
}

/* The D-Pad of @view turned: with the picture a quarter turn round,
 * left on the controller is up in the game, up is right, and so on. */
static int16_t input_rotation_dpad(unsigned turns, int16_t view)
{
   /* clockwise from up, as the controller has them */
   static const uint8_t id[4] = {
      RETRO_DEVICE_ID_JOYPAD_UP,   RETRO_DEVICE_ID_JOYPAD_RIGHT,
      RETRO_DEVICE_ID_JOYPAD_DOWN, RETRO_DEVICE_ID_JOYPAD_LEFT };
   unsigned i;
   unsigned in  = (uint16_t)view;
   unsigned out = in & ~(  (1u << RETRO_DEVICE_ID_JOYPAD_UP)
                         | (1u << RETRO_DEVICE_ID_JOYPAD_DOWN)
                         | (1u << RETRO_DEVICE_ID_JOYPAD_LEFT)
                         | (1u << RETRO_DEVICE_ID_JOYPAD_RIGHT));
   for (i = 0; i < 4; i++)
      if (in & (1u << id[i]))
         out |= 1u << id[(i + turns) & 3];
   return (int16_t)out;
}

/* One axis of a stick turned the same way: @value is the axis asked
 * for, @other the stick's other axis. */
static int16_t input_rotation_axis(unsigned turns, unsigned id,
      int16_t value, int16_t other)
{
   int v;
   switch (turns)
   {
      case 1:
         v = (id == RETRO_DEVICE_ID_ANALOG_X) ? -other : other;
         break;
      case 2:
         v = -value;
         break;
      case 3:
         v = (id == RETRO_DEVICE_ID_ANALOG_X) ? other : -other;
         break;
      default:
         return value;
   }
   return (int16_t)((v > 32767) ? 32767 : v);
}

/* One axis of the D-Pad: @a and @b are its two buttons' bits. Returns
 * the bits to take out of @held. */
static unsigned input_socd_axis(unsigned mode, unsigned held,
      unsigned before, unsigned a, unsigned b, uint8_t *winner)
{
   if ((held & (a | b)) != (a | b))
   {
      *winner = 0;
      return 0;
   }
   switch (mode)
   {
      case INPUT_SOCD_NEUTRAL:
         return a | b;
      case INPUT_SOCD_UP:
         return b;
      case INPUT_SOCD_LAST:
      case INPUT_SOCD_FIRST:
         /* decided when the second one comes down; two that come down
          * together are neutral for as long as both stay held */
         if ((before & (a | b)) == a)
            *winner = (uint8_t)((mode == INPUT_SOCD_LAST) ? b : a);
         else if ((before & (a | b)) == b)
            *winner = (uint8_t)((mode == INPUT_SOCD_LAST) ? a : b);
         return (a | b) & ~(unsigned)*winner;
   }
   return 0;
}

/* The port's buttons as compiled for this frame, with opposite D-Pad
 * directions resolved as the SOCD settings say. */
static int16_t input_socd_clean(input_driver_state_t *input_st,
      unsigned horizontal, unsigned vertical, unsigned port, int16_t view)
{
   const unsigned up     = 1 << RETRO_DEVICE_ID_JOYPAD_UP;
   const unsigned down   = 1 << RETRO_DEVICE_ID_JOYPAD_DOWN;
   const unsigned left   = 1 << RETRO_DEVICE_ID_JOYPAD_LEFT;
   const unsigned right  = 1 << RETRO_DEVICE_ID_JOYPAD_RIGHT;
   unsigned held         = (uint16_t)view & (up | down | left | right);
   unsigned before       = input_st->socd_held[port];
   unsigned drop         = 0;

   if (horizontal)
      drop |= input_socd_axis(horizontal, held, before, left, right,
            &input_st->socd_winner[port][0]);
   if (vertical)
      drop |= input_socd_axis(vertical, held, before, up, down,
            &input_st->socd_winner[port][1]);
   input_st->socd_held[port] = (uint8_t)held;
   return (int16_t)((uint16_t)view & ~drop);
}

int16_t input_driver_state_wrapper(unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   input_driver_state_t
      *input_st                = &input_driver_st;
   settings_t *settings        = config_get_ptr();
   int16_t result              = 0;

   /* The port comes from the core, and indexes per-port arrays here
    * and below. */
   if (port >= MAX_USERS)
      return 0;

#ifdef HAVE_BSV_MOVIE
   if (BSV_MOVIE_IS_PLAYBACK_ON())
     return bsv_movie_read_state(input_st, port, device, idx, id);
#endif

   /* Read input state.
    *
    * The RetroPad's buttons come from the frame's view of the port: a
    * word holding all sixteen, compiled once through the mask path of
    * input_state_internal().  After that a button or a mask query is
    * a read of that word, however many a core makes and whichever way
    * it asks.
    *
    * A core that reads button by button used to walk the port
    * mapping, remaps, turbo, hold and overlay once per button.  The
    * view is compiled on its second button of the frame, not its
    * first: a core that reads a single button a frame keeps the cost
    * it had, and one that reads more pays for the walk once.  A core
    * that asks for the mask already got its buttons compiled this
    * way.  samples/runloop/frontend_overhead holds the two readings
    * to the same result, frame for frame. */
   if (     (device & RETRO_DEVICE_MASK) == RETRO_DEVICE_JOYPAD
         && idx == 0
         && (   id <  RARCH_FIRST_CUSTOM_BIND
             || id == RETRO_DEVICE_ID_JOYPAD_MASK))
   {
      const uint16_t port_bit = (uint16_t)(1 << port);

      /* Once the view is compiled a query is the read at the bottom:
       * the settings that decide when to compile it are not looked at
       * again for the rest of the frame. */
      if (!(input_st->frame_valid.view & port_bit))
      {
         /* SOCD cleaning needs both directions of an axis, so with it
          * on the view is compiled on the frame's first button too */
         unsigned socd_h      = settings->uints.input_socd_horizontal;
         unsigned socd_v      = settings->uints.input_socd_vertical;
         /* and turning the D-Pad needs all four */
         unsigned turns       = settings->uints.input_rotation
            ? input_rotation_turns(settings) : 0;

         if (     id == RETRO_DEVICE_ID_JOYPAD_MASK
               || (input_st->frame_valid.asked & port_bit)
               || socd_h || socd_v || turns)
         {
            input_st->frame_view_joypad[port] = input_state_internal(
                  input_st, settings, port, RETRO_DEVICE_JOYPAD, 0,
                  RETRO_DEVICE_ID_JOYPAD_MASK);
            /* turned first, cleaned after */
            if (turns)
               input_st->frame_view_joypad[port] = input_rotation_dpad(
                     turns, input_st->frame_view_joypad[port]);
            if (socd_h || socd_v)
               input_st->frame_view_joypad[port] = input_socd_clean(
                     input_st, socd_h, socd_v, port,
                     input_st->frame_view_joypad[port]);
            input_st->frame_valid.view       |= port_bit;
         }
      }

      if (!(input_st->frame_valid.view & port_bit))
      {
         /* the frame's first button for this port */
         input_st->frame_valid.asked |= port_bit;
         result = input_state_internal(input_st, settings,
               port, device, idx, id);
      }
      else if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         result = input_st->frame_view_joypad[port];
      else
         result = ((uint16_t)input_st->frame_view_joypad[port] >> id) & 1;
   }
   else
   {
      result = input_state_internal(input_st, settings, port, device, idx, id);
      /* a stick, turned as the D-Pad is: it takes the other axis too */
      if (     settings->uints.input_rotation
            && (device & RETRO_DEVICE_MASK) == RETRO_DEVICE_ANALOG
            && (   idx == RETRO_DEVICE_INDEX_ANALOG_LEFT
                || idx == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
            && (   id  == RETRO_DEVICE_ID_ANALOG_X
                || id  == RETRO_DEVICE_ID_ANALOG_Y))
      {
         unsigned turns = input_rotation_turns(settings);
         if (turns & 1)
            result = input_rotation_axis(turns, id, result,
                  input_state_internal(input_st, settings, port, device, idx,
                     (id == RETRO_DEVICE_ID_ANALOG_X)
                     ? RETRO_DEVICE_ID_ANALOG_Y : RETRO_DEVICE_ID_ANALOG_X));
         else if (turns)
            result = input_rotation_axis(turns, id, result, 0);
      }
   }

   /* Register any analog stick input requests for
    * this 'virtual' (core) port.  The first one switches
    * analog-to-d-pad off for the port, which changes its buttons:
    * the view is compiled again for the queries that follow. */
   if (     (device == RETRO_DEVICE_ANALOG)
       && ( (idx    == RETRO_DEVICE_INDEX_ANALOG_LEFT)
       ||   (idx    == RETRO_DEVICE_INDEX_ANALOG_RIGHT))
       && !input_st->analog_requested[port])
   {
      input_st->analog_requested[port] = true;
      input_st->frame_valid.view      &= ~(1 << port);
      input_st->frame_valid.asked     &= ~(1 << port);
   }

#ifdef HAVE_BSV_MOVIE
   if (BSV_MOVIE_IS_RECORDING())
#ifdef HAVE_REWIND
      if (!state_manager_frame_is_reversed())
#endif
      bsv_movie_push_input_event(
            input_st->bsv_movie_state_handle,
            port,
            device,
            idx,
            id,
            result);
#endif

#ifdef HAVE_GAME_AI
   if (settings->bools.game_ai_override_p1 && port == 0)
      result |= game_ai_input(port, device, idx, id, result);
   if (settings->bools.game_ai_override_p2 && port == 1)
      result |= game_ai_input(port, device, idx, id, result);
#endif

   return result;
}

#ifdef HAVE_HID
void *hid_driver_get_data(void)
{
   return (void*)input_driver_st.hid_data;
}

/* This is only to be called after we've invoked free() on the
 * HID driver; the memory will have already been freed, so we need to
 * reset the pointer.
 */
void hid_driver_reset_data(void) { input_driver_st.hid_data = NULL; }

/**
 * config_get_hid_driver_options:
 *
 * Get an enumerated list of all HID driver names, separated by '|'.
 *
 * Returns: string listing of all HID driver names, separated by '|'.
 **/
const char* config_get_hid_driver_options(void)
{
   return char_list_new_special(STRING_LIST_INPUT_HID_DRIVERS, NULL);
}

/**
 * input_hid_init_first:
 *
 * Finds first suitable HID driver and initializes.
 *
 * Returns: HID driver if found, otherwise NULL.
 **/
const hid_driver_t *input_hid_init_first(void)
{
   unsigned i;
   input_driver_state_t *input_st = &input_driver_st;

   for (i = 0; hid_drivers[i]; i++)
   {
      input_st->hid_data = hid_drivers[i]->init();

      if (input_st->hid_data)
      {
         RARCH_LOG("[Input] Found HID driver: \"%s\".\n",
               hid_drivers[i]->ident);
         return hid_drivers[i];
      }
   }

   return NULL;
}
#endif

void input_remapping_cache_global_config(void)
{
   unsigned i;
   settings_t *settings           = config_get_ptr();
   input_driver_state_t *input_st = &input_driver_st;

   for (i = 0; i < MAX_USERS; i++)
   {
      /* Libretro device type is always set to
       * RETRO_DEVICE_JOYPAD globally *unless*
       * an override has been set via the command
       * line interface */
      unsigned device = RETRO_DEVICE_JOYPAD;

      if (retroarch_override_setting_is_set(
            RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE, &i))
         device = settings->uints.input_libretro_device[i];

      input_st->remapping_cache.analog_dpad_mode[i] = settings->uints.input_analog_dpad_mode[i];
      input_st->remapping_cache.libretro_device[i]  = device;
   }

   input_st->remapping_cache.turbo_enable     = settings->bools.input_turbo_enable;
   input_st->remapping_cache.turbo_allow_dpad = settings->bools.input_turbo_allow_dpad;
   input_st->remapping_cache.turbo_bind       = settings->ints.input_turbo_bind;
   input_st->remapping_cache.turbo_mode       = settings->uints.input_turbo_mode;
   input_st->remapping_cache.turbo_button     = settings->uints.input_turbo_button;
   input_st->remapping_cache.turbo_period     = settings->uints.input_turbo_period;
   input_st->remapping_cache.turbo_duty_cycle = settings->uints.input_turbo_duty_cycle;
}

void input_remapping_restore_global_config(bool clear_cache, bool restore_analog_dpad_mode)
{
   unsigned i;
   settings_t *settings           = config_get_ptr();
   input_driver_state_t *input_st = &input_driver_st;

   if (!(input_st->flags & INP_FLAG_REMAPPING_CACHE_ACTIVE))
      goto end;

   for (i = 0; i < MAX_USERS; i++)
   {
      if (restore_analog_dpad_mode)
         configuration_set_uint(settings,
               settings->uints.input_analog_dpad_mode[i],
               input_st->remapping_cache.analog_dpad_mode[i]);

      configuration_set_uint(settings,
            settings->uints.input_libretro_device[i],
            input_st->remapping_cache.libretro_device[i]);
   }

   configuration_set_bool(settings,
         settings->bools.input_turbo_enable,
         input_st->remapping_cache.turbo_enable);

   configuration_set_bool(settings,
         settings->bools.input_turbo_allow_dpad,
         input_st->remapping_cache.turbo_allow_dpad);

   configuration_set_int(settings,
         settings->ints.input_turbo_bind,
         input_st->remapping_cache.turbo_bind);

   configuration_set_uint(settings,
         settings->uints.input_turbo_mode,
         input_st->remapping_cache.turbo_mode);

   configuration_set_uint(settings,
         settings->uints.input_turbo_button,
         input_st->remapping_cache.turbo_button);

   configuration_set_uint(settings,
         settings->uints.input_turbo_period,
         input_st->remapping_cache.turbo_period);

   configuration_set_uint(settings,
         settings->uints.input_turbo_duty_cycle,
         input_st->remapping_cache.turbo_duty_cycle);

end:
   if (clear_cache)
      input_st->flags &= ~INP_FLAG_REMAPPING_CACHE_ACTIVE;
}

/* Netplay's ports are the session's, and a replay has to see the
 * ports it was recorded with. */
static bool input_first_press_blocked(void)
{
#ifdef HAVE_BSV_MOVIE
   if (input_driver_st.bsv_movie_state.flags & (
              BSV_FLAG_MOVIE_START_RECORDING
            | BSV_FLAG_MOVIE_START_PLAYBACK
            | BSV_FLAG_MOVIE_RECORDING
            | BSV_FLAG_MOVIE_PLAYBACK))
      return true;
#endif
#ifdef HAVE_NETWORKING
   if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
      return true;
#endif
   return false;
}

bool input_first_press_enabled(void)
{
   return config_get_ptr()->bools.input_assign_ports_on_button_press
      && !input_first_press_blocked();
}

bool input_first_press_assigned(unsigned user)
{
   return user < MAX_USERS
      && (input_driver_st.first_press_assigned & (1U << user)) != 0;
}

void input_first_press_set_by_hand(unsigned user)
{
   if (user < MAX_USERS)
      input_driver_st.first_press_assigned &= ~(1U << user);
}

/* Whichever was pressed, the user's controller and its keys go to
 * the core port together, so the notification names both. */
size_t input_first_press_describe(unsigned user, unsigned port,
      char *s, size_t len)
{
   unsigned k;
   settings_t *settings = config_get_ptr();
   const input_device_driver_t *joypad = input_driver_st.primary_joypad;
   unsigned joy_idx     = settings->uints.input_joypad_index[user];
   const char *name     = NULL;
   bool keys            = false;

   if (joypad && joypad->query_pad && joypad->query_pad(joy_idx))
   {
      name = input_config_get_device_display_name(joy_idx);
      if (!name || !*name)
         name = input_config_get_device_name(joy_idx);
   }
   for (k = 0; k < RARCH_FIRST_CUSTOM_BIND && !keys; k++)
      keys = RETRO_KEYBIND_KEY(&input_config_binds[user][k]) != RETROK_UNKNOWN;

   if (name && *name)
      return snprintf(s, len, msg_hash_to_str(keys
               ? MSG_DEVICE_AND_KEYBOARD_ASSIGNED_TO_CORE_PORT_NR
               : MSG_DEVICE_ASSIGNED_TO_CORE_PORT_NR),
            name, port + 1);
   if (keys)
      return snprintf(s, len,
            msg_hash_to_str(MSG_KEYBOARD_ASSIGNED_TO_CORE_PORT_NR),
            port + 1);
   {
      char who[32];
      snprintf(who, sizeof(who), "%s %u",
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PORT), user + 1);
      return snprintf(s, len,
            msg_hash_to_str(MSG_DEVICE_ASSIGNED_TO_CORE_PORT_NR),
            who, port + 1);
   }
}

void input_first_press_apply(void)
{
   unsigned user, port;
   input_driver_state_t *input_st = &input_driver_st;
   settings_t *settings           = config_get_ptr();
   uint32_t pending               = input_st->first_press_pending;
   unsigned max_users             = settings->uints.input_max_users;
   bool mapped                    = false;

   input_st->first_press_pending  = 0;
   if (max_users > MAX_USERS)
      max_users                   = MAX_USERS;

   /* netplay or a replay began with users still unmapped: each gets
    * its own port, and the policy is off until content starts again */
   if (pending & INPUT_FIRST_PRESS_GIVE_UP)
   {
      for (user = 0; user < MAX_USERS; user++)
         if (settings->uints.input_remap_ports[user] >= MAX_USERS)
            settings->uints.input_remap_ports[user] = user;
      input_st->first_press_on = false;
      input_remapping_update_port_map();
      command_event(CMD_EVENT_CONTROLLER_INIT, NULL);
      return;
   }

   for (user = 0; user < max_users; user++)
   {
      char msg[256];
      size_t _len;

      if (     !(pending & (1U << user))
            || settings->uints.input_remap_ports[user] < MAX_USERS)
         continue;
      /* the lowest core port no user feeds */
      for (port = 0; port < max_users; port++)
         if (settings->uints.input_remap_port_map[port][0] >= MAX_USERS)
            break;
      if (port >= max_users)
         break;

      settings->uints.input_remap_ports[user] = port;
      input_st->first_press_assigned         |= (1U << user);
      input_remapping_update_port_map();
      mapped = true;

      _len = input_first_press_describe(user, port, msg, sizeof(msg));
      RARCH_LOG("[Input] %s.\n", msg);
      if (settings->bools.notification_show_autoconfig)
         runloop_msg_queue_push(msg, _len, 1, 100, false, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }

   /* the core is told the port's device, which the setting keeps */
   if (mapped)
      command_event(CMD_EVENT_CONTROLLER_INIT, NULL);
}

void input_remapping_update_port_map(void)
{
   unsigned i, j;
   settings_t *settings               = config_get_ptr();
   unsigned port_map_index[MAX_USERS] = {0};
   input_driver_state_t *input_st     = &input_driver_st;
   bool unmapped                      = false;

   /* First pass: 'reset' port map */
   for (i = 0; i < MAX_USERS; i++)
      for (j = 0; j < (MAX_USERS + 1); j++)
         settings->uints.input_remap_port_map[i][j] = MAX_USERS;

   /* Second pass: assign port indices from
    * 'input_remap_ports' */
   for (i = 0; i < MAX_USERS; i++)
   {
      unsigned remap_port = settings->uints.input_remap_ports[i];

      if (remap_port < MAX_USERS)
      {
         /* 'input_remap_port_map' provides a list of
          * 'physical' ports for each 'virtual' port
          * sampled in input_state().
          * (Note: in the following explanation, port
          * index starts from 0, rather than the frontend
          * display convention of 1)
          * For example - the following remap configuration
          * will map input devices 0+1 to port 0, and input
          * device 2 to port 1
          * > input_remap_ports[0] = 0;
          *   input_remap_ports[1] = 0;
          *   input_remap_ports[2] = 1;
          * This gives a port map of:
          * > input_remap_port_map[0] = { 0, 1, MAX_USERS, ... };
          *   input_remap_port_map[1] = { 2, MAX_USERS, ... }
          *   input_remap_port_map[2] = { MAX_USERS, ... }
          *   ...
          * A port map value of MAX_USERS indicates the end
          * of the 'physical' port list */
         settings->uints.input_remap_port_map[remap_port]
               [port_map_index[remap_port]] = i;
         port_map_index[remap_port]++;
         input_st->first_press_released[i] = 0;
      }
      else
         unmapped = true;
   }

   /* the poll looks for a first press only while there is a user
    * with no core port */
   input_st->first_press_live = unmapped && input_st->first_press_on;
}

void input_remapping_deinit(bool save_remap)
{
   runloop_state_t *runloop_st  = runloop_state_get_ptr();
   if (runloop_st->name.remapfile)
   {
      if (save_remap)
         input_remapping_save_file(runloop_st->name.remapfile);

      free(runloop_st->name.remapfile);
   }
   runloop_st->name.remapfile   = NULL;
   runloop_st->flags           &= ~(RUNLOOP_FLAG_REMAPS_CORE_ACTIVE
                               |    RUNLOOP_FLAG_REMAPS_CONTENT_DIR_ACTIVE
                               |    RUNLOOP_FLAG_REMAPS_GAME_ACTIVE);
}

void input_remapping_set_defaults(bool clear_cache)
{
   unsigned i, j;
   settings_t *settings           = config_get_ptr();
   input_driver_state_t *input_st = &input_driver_st;
   bool first_press               = input_first_press_enabled();

   for (i = 0; i < MAX_USERS; i++)
   {
      /* Button/keyboard remaps */
      for (j = 0; j < RARCH_FIRST_CUSTOM_BIND; j++)
      {
         const struct retro_keybind *keybind = &input_config_binds[i][j];

         configuration_set_uint(settings,
               settings->uints.input_remap_ids[i][j],
                     keybind ? keybind->id : RARCH_UNMAPPED);

         configuration_set_uint(settings,
               settings->uints.input_keymapper_ids[i][j], RETROK_UNKNOWN);
      }

      /* Analog stick remaps */
      for (j = RARCH_FIRST_CUSTOM_BIND; j < RARCH_ANALOG_BIND_LIST_END; j++)
         configuration_set_uint(settings,
               settings->uints.input_remap_ids[i][j], j);

      /* Controller port remaps: none under first-press assignment */
      configuration_set_uint(settings,
            settings->uints.input_remap_ports[i],
            first_press ? MAX_USERS : i);
   }

   /* first-press assignment starts over; the setting is looked at
    * here, so changing it applies from the next content start */
   memset(input_st->first_press_released, 0,
         sizeof(input_st->first_press_released));
   input_st->first_press_pending  = 0;
   input_st->first_press_assigned = 0;
   input_st->first_press_on       = first_press;

   /* Need to call 'input_remapping_update_port_map()'
    * whenever 'settings->uints.input_remap_ports'
    * is modified */
   input_remapping_update_port_map();

   /* Restore 'global' settings that were cached on
    * the last core init
    * > Prevents remap changes from 'bleeding through'
    *   into the main config file */
   input_remapping_restore_global_config(clear_cache, true);
}

void input_driver_collect_system_input(input_driver_state_t *input_st,
      settings_t *settings, input_bits_t *current_bits)
{
   rarch_joypad_info_t joypad_info;
   input_driver_t *input               = input_st->current_driver;
   const input_device_driver_t *joypad = NULL;
   const input_device_driver_t
      *sec_joypad                      = NULL;
   unsigned block_delay                = settings->uints.input_hotkey_block_delay;
   /* Both of the arrays indexed below are [MAX_USERS]; the setting is
    * read from the config file and is not guaranteed to respect that. */
   uint8_t max_users                   = (settings->uints.input_max_users
         > MAX_USERS) ? MAX_USERS
         : (uint8_t)settings->uints.input_max_users;
   uint8_t port                        = 0;
   uint8_t hotkey_port                 = 0;
#ifdef HAVE_MENU
   bool all_users_control_menu         = settings->bools.input_all_users_control_menu;
   bool display_kb                     = menu_input_dialog_get_display_kb();
   bool menu_is_alive                  = (menu_state_get_ptr()->flags &
         MENU_ST_FLAG_ALIVE) ? true : false;
   bool menu_input_active              = menu_is_alive &&
         !(settings->bools.menu_unified_controls && !display_kb);
#endif

   input_st->system_buttons_not_overlay = 0;

   /* Hotkeys and menu navigation read the controllers through the
    * stand-in while they are gated, the same as the core does. */
   input_driver_update_joypad_focus(input_st, settings);
   joypad                              = INPUT_JOYPAD_FOR_READ(
         input_st, input_st->primary_joypad);
#ifdef HAVE_MFI
   sec_joypad                          = INPUT_JOYPAD_FOR_READ(
         input_st, input_st->secondary_joypad);
#endif
   joypad_info.axis_threshold          = settings->floats.input_axis_threshold;

   /* Gather input from each (enabled) joypad */
   for (port = 0; port < (int)max_users; port++)
   {
      const struct retro_keybind *binds_norm = &input_config_binds[port][RARCH_ENABLE_HOTKEY];
      const struct retro_keybind *binds_auto = NULL;

      joypad_info.joy_idx                    = settings->uints.input_joypad_index[port];
      /* input_autoconf_binds is [MAX_USERS] and joy_idx comes from the
       * config file, so it can point past the end - the same bound the
       * rumble and autoconfig paths already apply. Fall back to the
       * first slot rather than skipping the port, so a bad index
       * degrades to the wrong binds instead of no input at all. */
      if (joypad_info.joy_idx >= MAX_USERS)
         joypad_info.joy_idx                 = 0;
      joypad_info.auto_binds                 = input_autoconf_binds[joypad_info.joy_idx];
      binds_auto                             = &input_autoconf_binds[joypad_info.joy_idx][RARCH_ENABLE_HOTKEY];

#ifdef HAVE_MENU
      if (menu_is_alive && joypad)
      {
         uint8_t s;
         uint8_t a;

         /* Read input from analog sticks according to settings. */
         for (s = RETRO_DEVICE_INDEX_ANALOG_LEFT; s <= RETRO_DEVICE_INDEX_ANALOG_RIGHT; s++)
         {
            if (     (settings->bools.menu_disable_left_analog  && s == RETRO_DEVICE_INDEX_ANALOG_LEFT)
                  || (settings->bools.menu_disable_right_analog && s == RETRO_DEVICE_INDEX_ANALOG_RIGHT))
               continue;

            for (a = RETRO_DEVICE_ID_ANALOG_X; a <= RETRO_DEVICE_ID_ANALOG_Y; a++)
            {
               int16_t ret = input_joypad_analog_axis(
                     ANALOG_DPAD_NONE,
                     settings->floats.input_analog_deadzone,
                     settings->floats.input_analog_sensitivity,
                     joypad,
                     &joypad_info,
                     s,
                     a,
                     (input_st->libretro_input_binds[port]
                        ? *input_st->libretro_input_binds[port] : NULL));

               if (ret)
               {
                  bool playlist = false;

                  /* Replace right analog stick navigation in playlists to thumbnail cycling. */
                  if (s == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
                  {
                     menu_entry_t entry;
                     MENU_ENTRY_INITIALIZE(entry);
                     menu_entry_get(&entry, 0, 0, NULL, true);

                     switch (entry.type)
                     {
                        case FILE_TYPE_RPL_ENTRY:
                        case FILE_TYPE_PLAYLIST_ENTRY:
                        case FILE_TYPE_PLAIN:
                        case FILE_TYPE_RDB:
                           playlist = true;
                           break;
                        default:
                           break;
                     }
                  }

                  if (a == RETRO_DEVICE_ID_ANALOG_Y && (float)ret / 0x7fff < -joypad_info.axis_threshold)
                  {
                     if (playlist)
                        BIT256_SET_PTR(current_bits, RARCH_ANALOG_RIGHT_Y_MINUS);
                     else
                        BIT256_SET_PTR(current_bits, RETRO_DEVICE_ID_JOYPAD_UP);
                  }
                  else if (a == RETRO_DEVICE_ID_ANALOG_Y && (float)ret / 0x7fff > joypad_info.axis_threshold)
                  {
                     if (playlist)
                        BIT256_SET_PTR(current_bits, RARCH_ANALOG_RIGHT_Y_PLUS);
                     else
                        BIT256_SET_PTR(current_bits, RETRO_DEVICE_ID_JOYPAD_DOWN);
                  }
                  if (a == RETRO_DEVICE_ID_ANALOG_X && (float)ret / 0x7fff < -joypad_info.axis_threshold)
                  {
                     if (playlist)
                        BIT256_SET_PTR(current_bits, RARCH_ANALOG_RIGHT_X_MINUS);
                     else
                        BIT256_SET_PTR(current_bits, RETRO_DEVICE_ID_JOYPAD_LEFT);
                  }
                  else if (a == RETRO_DEVICE_ID_ANALOG_X && (float)ret / 0x7fff > joypad_info.axis_threshold)
                  {
                     if (playlist)
                        BIT256_SET_PTR(current_bits, RARCH_ANALOG_RIGHT_X_PLUS);
                     else
                        BIT256_SET_PTR(current_bits, RETRO_DEVICE_ID_JOYPAD_RIGHT);
                  }
               }
            }
         }
      }
#endif /* HAVE_MENU */

      if (settings->bools.input_hotkey_follows_player1)
      {
         /* Hotkeys are bound to player 1 (the first user mapped to core port 0),
          * even if player 1 is remapped to a different user. */
         hotkey_port = settings->uints.input_remap_port_map[0][0];

         if (hotkey_port >= MAX_USERS)
            hotkey_port = 0;
      }

      input_keys_pressed(
            port,
            hotkey_port,
#ifdef HAVE_MENU
            menu_is_alive,
#else
            false,
#endif
            block_delay,
            current_bits,
            (const retro_keybind_set *)input_config_binds,
            binds_norm,
            binds_auto,
            joypad,
            sec_joypad,
            &joypad_info,
            settings->bools.input_hotkey_device_merge);

#ifdef HAVE_MENU
      if (menu_is_alive)
      {
         if (!all_users_control_menu)
            break;
      }
#endif /* HAVE_MENU */
   }

#ifdef HAVE_MENU
   if (menu_input_active)
   {
      /* Gather keyboard input, if enabled
       * Note: Keyboard input always read from
       * port 0 */
      if (!display_kb && input && input->input_state)
      {
         struct menu_state *menu_st  = menu_state_get_ptr();
         bool swap_ok_cancel_buttons = settings->bools.input_menu_swap_ok_cancel_buttons;
         unsigned i;
         unsigned ids[][2] =
         {
            {RETROK_RETURN,    RETRO_DEVICE_ID_JOYPAD_A      },
            {RETROK_BACKSPACE, RETRO_DEVICE_ID_JOYPAD_B      },
            {RETROK_DELETE,    RETRO_DEVICE_ID_JOYPAD_Y      },
            {RETROK_SLASH,     RETRO_DEVICE_ID_JOYPAD_X      },
            {RETROK_SPACE,     RETRO_DEVICE_ID_JOYPAD_START  },
            {RETROK_RSHIFT,    RETRO_DEVICE_ID_JOYPAD_SELECT },
            {RETROK_UP,        RETRO_DEVICE_ID_JOYPAD_UP     },
            {RETROK_DOWN,      RETRO_DEVICE_ID_JOYPAD_DOWN   },
            {RETROK_LEFT,      RETRO_DEVICE_ID_JOYPAD_LEFT   },
            {RETROK_RIGHT,     RETRO_DEVICE_ID_JOYPAD_RIGHT  },
            {RETROK_PAGEUP,    RETRO_DEVICE_ID_JOYPAD_L      },
            {RETROK_PAGEDOWN,  RETRO_DEVICE_ID_JOYPAD_R      },
            {RETROK_HOME,      RETRO_DEVICE_ID_JOYPAD_L3     },
            {RETROK_END,       RETRO_DEVICE_ID_JOYPAD_R3     },
            /* Extra keys read regardless of 'enable_hotkey' */
            {0,                RARCH_QUIT_KEY                }, /* 14 */
            {0,                RARCH_FULLSCREEN_TOGGLE_KEY   },
            {0,                RARCH_UI_COMPANION_TOGGLE     },
            {0,                RARCH_FPS_TOGGLE              },
            {0,                RARCH_NETPLAY_HOST_TOGGLE     },
            {0,                RARCH_BIND_LIST_END_NULL      },
         };

         ids[14][0] = RETRO_KEYBIND_KEY(&input_config_binds[0][RARCH_QUIT_KEY]);
         ids[15][0] = RETRO_KEYBIND_KEY(&input_config_binds[0][RARCH_FULLSCREEN_TOGGLE_KEY]);
         ids[16][0] = RETRO_KEYBIND_KEY(&input_config_binds[0][RARCH_UI_COMPANION_TOGGLE]);
         ids[17][0] = RETRO_KEYBIND_KEY(&input_config_binds[0][RARCH_FPS_TOGGLE]);
         ids[18][0] = RETRO_KEYBIND_KEY(&input_config_binds[0][RARCH_NETPLAY_HOST_TOGGLE]);
         ids[19][0] = RETROK_ESCAPE;

         /* Escape cancels dialogs */
         if (menu_st && menu_st->driver_data && *menu_st->driver_data->menu_state_msg)
            ids[19][1] = (swap_ok_cancel_buttons)
                  ? RETRO_DEVICE_ID_JOYPAD_A : RETRO_DEVICE_ID_JOYPAD_B;

         if (swap_ok_cancel_buttons)
         {
            ids[0][1] = RETRO_DEVICE_ID_JOYPAD_B;
            ids[1][1] = RETRO_DEVICE_ID_JOYPAD_A;
         }

         for (i = 0; i < ARRAY_SIZE(ids); i++)
         {
            if (ids[i][0] && input->input_state(
                     input_st->current_data,
                     joypad,
                     sec_joypad,
                     &joypad_info,
                     (const retro_keybind_set *)input_config_binds,
                     !!(input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED),
                     0,
                     RETRO_DEVICE_KEYBOARD, 0, ids[i][0]))
            {
               /* Wait for release when closing dialogs with Escape,
                * otherwise quit hotkey will also get triggered */
               if (ids[i][0] == RETROK_ESCAPE && ids[19][1] != RARCH_BIND_LIST_END_NULL)
                  input_st->flags |= INP_FLAG_WAIT_INPUT_RELEASE;

               BIT256_SET_PTR(current_bits, ids[i][1]);
            }
         }
      }
      else if (display_kb && input && input->input_state)
      {
         /* OSK grid navigation from keyboard / TV remote D-pad, plus
          * page switches and Escape to clear/close. Return and LCTRL/RCTRL
          * confirm the highlighted OSK character (same as menu OK) */
         unsigned i;
         bool swap_ok_cancel_buttons = settings->bools.input_menu_swap_ok_cancel_buttons;
         unsigned ids[][2] =
         {
            {RETROK_RETURN,    RETRO_DEVICE_ID_JOYPAD_A      },
            {RETROK_LCTRL,     RETRO_DEVICE_ID_JOYPAD_A      },
            {RETROK_RCTRL,     RETRO_DEVICE_ID_JOYPAD_A      },
            {RETROK_UP,        RETRO_DEVICE_ID_JOYPAD_UP     },
            {RETROK_DOWN,      RETRO_DEVICE_ID_JOYPAD_DOWN   },
            {RETROK_LEFT,      RETRO_DEVICE_ID_JOYPAD_LEFT   },
            {RETROK_RIGHT,     RETRO_DEVICE_ID_JOYPAD_RIGHT  },
            {RETROK_PAGEUP,    RETRO_DEVICE_ID_JOYPAD_L      },
            {RETROK_PAGEDOWN,  RETRO_DEVICE_ID_JOYPAD_R      },
            {RETROK_ESCAPE,    RETRO_DEVICE_ID_JOYPAD_SELECT },
         };

         if (swap_ok_cancel_buttons)
            ids[0][1] = RETRO_DEVICE_ID_JOYPAD_B;

         for (i = 0; i < ARRAY_SIZE(ids); i++)
         {
            if (     swap_ok_cancel_buttons
                  && (ids[i][0] == RETROK_LCTRL || ids[i][0] == RETROK_RCTRL))
               ids[i][1] = RETRO_DEVICE_ID_JOYPAD_B;

            if (     input_st->osk_textbox_focus
                  && ids[i][1] != RETRO_DEVICE_ID_JOYPAD_L
                  && ids[i][1] != RETRO_DEVICE_ID_JOYPAD_R
                  && ids[i][1] != RETRO_DEVICE_ID_JOYPAD_SELECT)
               continue;

            if (ids[i][0] && input->input_state(
                     input_st->current_data,
                     joypad,
                     sec_joypad,
                     &joypad_info,
                     (const retro_keybind_set *)input_config_binds,
                     !!(input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED),
                     0,
                     RETRO_DEVICE_KEYBOARD, 0, ids[i][0]))
               BIT256_SET_PTR(current_bits, ids[i][1]);
         }
      }
   }
   else
#endif /* HAVE_MENU */
   {
#if defined(HAVE_ACCESSIBILITY) && defined(HAVE_TRANSLATE)
      if (settings->bools.ai_service_enable)
      {
         int i;
         input_st->gamepad_input_override = 0;
         for (i = 0; i < MAX_USERS; i++)
         {
            /* Set gamepad input override */
            if (input_st->ai_gamepad_state[i] == 2)
               input_st->gamepad_input_override |= (1 << i);
            input_st->ai_gamepad_state[i] = 0;
         }
      }
#endif /* defined(HAVE_ACCESSIBILITY) && defined(HAVE_TRANSLATE) */
   }
}

#if defined(HAVE_MENU) && defined(HAVE_ACCESSIBILITY)
static const char *accessibility_lut_name(char key)
{
   switch (key)
   {
      case '`':
         return "tilde";
      case '!':
         return "exclamation point";
      case '@':
         return "at sign";
      case '#':
         return "hash sign";
      case '$':
         return "dollar sign";
      case '%':
         return "percent sign";
      case '^':
         return "carrot";
      case '&':
         return "ampersand";
      case '*':
         return "asterisk";
      case '(':
         return "left bracket";
      case ')':
         return "right bracket";
      case '-':
         return "minus";
      case '_':
         return "underscore";
      case '=':
         return "equals";
      case '+':
         return "plus";
      case '[':
         return "left square bracket";
      case '{':
         return "left curl bracket";
      case ']':
         return "right square bracket";
      case '}':
         return "right curl bracket";
      case '\\':
         return "back slash";
      case '|':
         return "pipe";
      case ';':
         return "semicolon";
      case ':':
         return "colon";
      case '\'':
         return "single quote";
      case '\"':
         return "double quote";
      case ',':
         return "comma";
      case '<':
         return "left angle bracket";
      case '.':
         return "period";
      case '>':
         return "right angle bracket";
      case '/':
         return "front slash";
      case '?':
         return "question mark";
      case ' ':
         return "space";
      default:
         break;
   }
   return NULL;
}
#endif

/* Keyboard events from a thread that is not the frontend's.
 *
 * input_keyboard_event() reaches into the menu, the on-screen
 * keyboard, the frontend's own key handling, and calls the core's
 * keyboard callback. All of that belongs to the thread the frontend
 * and the core run on. Most drivers report their keys from the poll,
 * on that thread. Some report them from where the window's messages
 * are handled, and with threaded video that is the video thread: the
 * Windows window procedure (every Windows input driver but raw input,
 * which reads at the poll) and X11's event loop. A key pressed there
 * used to run all of the above on the video thread, the core's
 * callback included, while the core ran its frame on the other.
 *
 * An event that comes on another thread is put in a queue instead, and
 * the next input_driver_poll() - on the frontend's thread - takes the
 * queue in order and does with each what would have been done at once.
 * The window's thread hands its keys over once a frame as it is, so
 * the poll that takes them is the first that could have seen them.
 *
 * One reader, the poll, which takes no lock. Writers take turns
 * through a flag; in practice there is one, the video thread. Sixty-
 * four events between two polls is more than a keyboard sends; past
 * that the newest are dropped, counted and said. */
static void input_keyboard_event_now(bool down, unsigned code,
      uint32_t character, uint16_t mod, unsigned device);

#ifdef HAVE_THREADS
#define INPUT_KEY_LANE_SIZE 64 /* a power of two */

static struct
{
   struct
   {
      uint32_t character;
      unsigned code;
      unsigned device;
      uint16_t mod;
      bool     down;
   } slot[INPUT_KEY_LANE_SIZE];
   retro_atomic_int_t head;      /* next to take: the poll's */
   retro_atomic_int_t tail;      /* next to fill: the writers' */
   retro_atomic_int_t writing;   /* a writer is in */
   retro_atomic_int_t dropped;
   bool warned;
} input_key_lane;

static void input_key_lane_push(bool down, unsigned code,
      uint32_t character, uint16_t mod, unsigned device)
{
   int head, tail;

   while (retro_atomic_exchange_int(&input_key_lane.writing, 1))
      ; /* another writer: there is not one in practice */

   head = retro_atomic_load_acquire_int(&input_key_lane.head);
   tail = retro_atomic_load_relaxed_int(&input_key_lane.tail);
   if ((unsigned)(tail - head) >= INPUT_KEY_LANE_SIZE)
      retro_atomic_fetch_add_int(&input_key_lane.dropped, 1);
   else
   {
      unsigned i                       = (unsigned)tail
         & (INPUT_KEY_LANE_SIZE - 1);
      input_key_lane.slot[i].down      = down;
      input_key_lane.slot[i].code      = code;
      input_key_lane.slot[i].character = character;
      input_key_lane.slot[i].mod       = mod;
      input_key_lane.slot[i].device    = device;
      retro_atomic_store_release_int(&input_key_lane.tail, tail + 1);
   }

   retro_atomic_store_release_int(&input_key_lane.writing, 0);
}

/* The poll's: every event that has come since the last one. */
static void input_key_lane_take(void)
{
   int head = retro_atomic_load_relaxed_int(&input_key_lane.head);
   int tail = retro_atomic_load_acquire_int(&input_key_lane.tail);

   while (head != tail)
   {
      unsigned i         = (unsigned)head & (INPUT_KEY_LANE_SIZE - 1);
      bool down          = input_key_lane.slot[i].down;
      unsigned code      = input_key_lane.slot[i].code;
      uint32_t character = input_key_lane.slot[i].character;
      uint16_t mod       = input_key_lane.slot[i].mod;
      unsigned device    = input_key_lane.slot[i].device;

      /* the slot is free before the event is acted on */
      retro_atomic_store_release_int(&input_key_lane.head, ++head);
      input_keyboard_event_now(down, code, character, mod, device);
   }

   if (     !input_key_lane.warned
         && retro_atomic_load_relaxed_int(&input_key_lane.dropped))
   {
      input_key_lane.warned = true;
      RARCH_WARN("[Input] More than %d keyboard events came between two"
            " polls: the newest were dropped.\n", INPUT_KEY_LANE_SIZE);
   }
}

unsigned input_driver_key_events_dropped(void)
{
   return (unsigned)retro_atomic_load_relaxed_int(&input_key_lane.dropped);
}
#endif

void input_keyboard_event(bool down, unsigned code,
      uint32_t character, uint16_t mod, unsigned device)
{
#ifdef HAVE_THREADS
   if (!task_is_on_main_thread())
   {
      input_key_lane_push(down, code, character, mod, device);
      return;
   }
#endif
   input_keyboard_event_now(down, code, character, mod, device);
}

static void input_keyboard_event_now(bool down, unsigned code,
      uint32_t character, uint16_t mod, unsigned device)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   retro_keyboard_event_t
      *key_event               = &runloop_st->key_event;
   input_driver_state_t
      *input_st                = &input_driver_st;
#ifdef HAVE_ACCESSIBILITY
   access_state_t *access_st   = access_state_get_ptr();
   settings_t *settings        = config_get_ptr();
   bool accessibility_enable   = settings->bools.accessibility_enable;
   unsigned accessibility_narrator_speech_speed
                               = settings->uints.accessibility_narrator_speech_speed;
#endif
#ifdef HAVE_MENU
   struct menu_state *menu_st  = menu_state_get_ptr();

   /* If screensaver is active, then it should be
    * disabled if:
    * - Key is down AND
    * - OSK is active, OR:
    * - Key is *not* mapped to RetroPad input (these
    *   inputs are handled in menu_event() - if we
    *   allow mapped RetroPad keys to toggle off
    *   the screensaver, then we end up with a 'duplicate'
    *   input that will trigger unwanted menu action)
    * - For extra amusement, a number of keyboard keys
    *   are hard-coded to RetroPad inputs (while the menu
    *   is running) in such a way that they cannot be
    *   detected via the regular 'keyboard_mapping_bits'
    *   record. We therefore have to check each of these
    *   explicitly...
    * Otherwise, input is ignored whenever screensaver
    * is active */
   if (menu_st->flags & MENU_ST_FLAG_SCREENSAVER_ACTIVE)
   {
      if (   (down)
          && (code != RETROK_UNKNOWN)
          && (menu_input_dialog_get_display_kb()
          || !((code == RETROK_SPACE)       /* RETRO_DEVICE_ID_JOYPAD_START */
          ||   (code == RETROK_SLASH)       /* RETRO_DEVICE_ID_JOYPAD_X */
          ||   (code == RETROK_RSHIFT)      /* RETRO_DEVICE_ID_JOYPAD_SELECT */
          ||   (code == RETROK_RIGHT)       /* RETRO_DEVICE_ID_JOYPAD_RIGHT */
          ||   (code == RETROK_LEFT)        /* RETRO_DEVICE_ID_JOYPAD_LEFT */
          ||   (code == RETROK_DOWN)        /* RETRO_DEVICE_ID_JOYPAD_DOWN */
          ||   (code == RETROK_UP)          /* RETRO_DEVICE_ID_JOYPAD_UP */
          ||   (code == RETROK_PAGEUP)      /* RETRO_DEVICE_ID_JOYPAD_L */
          ||   (code == RETROK_PAGEDOWN)    /* RETRO_DEVICE_ID_JOYPAD_R */
          ||   (code == RETROK_BACKSPACE)   /* RETRO_DEVICE_ID_JOYPAD_B */
          ||   (code == RETROK_RETURN)      /* RETRO_DEVICE_ID_JOYPAD_A */
          ||   (code == RETROK_DELETE)      /* RETRO_DEVICE_ID_JOYPAD_Y */
          ||   (BIT512_GET(input_st->keyboard_mapping_bits, code)))))
      {
         menu_st->flags             &= ~MENU_ST_FLAG_SCREENSAVER_ACTIVE;
         menu_st->input_last_time_us = menu_st->current_time_us;
         if (menu_st->driver_ctx->environ_cb)
            menu_st->driver_ctx->environ_cb(MENU_ENVIRON_DISABLE_SCREENSAVER,
                  NULL, menu_st->userdata);
      }
      return;
   }

   if (down)
      menu_st->input_last_time_us = menu_st->current_time_us;

#ifdef HAVE_ACCESSIBILITY
   if (     menu_input_dialog_get_display_kb()
         && down
         && is_accessibility_enabled(accessibility_enable,
            access_st->enabled))
   {
      if (code != 303 && code != 0)
      {
         char say_char[2];
         char c      = (char)character;
         say_char[0] = c;
         say_char[1] = '\0';

         if (character == 127 || character == 8)
            accessibility_speak_priority(accessibility_enable,
                  accessibility_narrator_speech_speed, "backspace", 10);
         else
         {
            const char *lut_name = accessibility_lut_name(c);
            if (lut_name)
               accessibility_speak_priority(
                     accessibility_enable,
                     accessibility_narrator_speech_speed,
                     lut_name, 10);
            else if (character != 0)
               accessibility_speak_priority(
                     accessibility_enable,
                     accessibility_narrator_speech_speed,
                     say_char, 10);
         }
      }
   }
#endif
#endif

   if (input_st->flags & INP_FLAG_DEFERRED_WAIT_KEYS)
   {
      if (down)
         return;
      input_st->keyboard_press_cb    = NULL;
      input_st->keyboard_press_data  = NULL;
      input_st->flags               &= ~(INP_FLAG_KB_MAPPING_BLOCKED
                                     |   INP_FLAG_DEFERRED_WAIT_KEYS
                                       );
   }
   else if (input_st->keyboard_press_cb)
   {
      if (!down || code == RETROK_UNKNOWN)
         return;
      if (input_st->keyboard_press_cb(input_st->keyboard_press_data, code))
         return;
      input_st->flags               |= INP_FLAG_DEFERRED_WAIT_KEYS;
   }
   else if (input_st->keyboard_line.enabled)
   {
      input_keyboard_line_t *line = &input_st->keyboard_line;

      if (!down)
         return;

      if (code == RETROK_TAB)
      {
         input_st->osk_textbox_focus = !input_st->osk_textbox_focus;
         return;
      }

      if (input_st->osk_textbox_focus)
      {
         switch (code)
         {
            case RETROK_LEFT:
               if (line->ptr && line->buffer)
               {
                  line->ptr--;
                  while (line->ptr && IS_UTF8_CONTINUATION(line->buffer[line->ptr]))
                     line->ptr--;

                  if (mod & RETROKMOD_CTRL)
                     while (line->ptr &&
                           (ISSPACE(line->buffer[line->ptr]) || !ISSPACE(line->buffer[line->ptr - 1])))
                        line->ptr--;
               }
               return;
            case RETROK_RIGHT:
               if (line->buffer && line->ptr < line->size)
               {
                  line->ptr++;
                  while (line->ptr < line->size && IS_UTF8_CONTINUATION(line->buffer[line->ptr]))
                     line->ptr++;

                  if (mod & RETROKMOD_CTRL)
                     while (line->ptr < line->size &&
                           (ISSPACE(line->buffer[line->ptr]) || !ISSPACE(line->buffer[line->ptr - 1])))
                        line->ptr++;
               }
               return;
            case RETROK_UP:
               line->ptr = 0;
               return;
            case RETROK_DOWN:
               line->ptr = line->size;
               return;
            default:
               break;
         }
      }

      if (!input_keyboard_line_event(input_st, line, character))
         return;

      /* Line is complete, can free it now. */
      input_keyboard_line_free(input_st);

      /* Unblock all hotkeys. */
      input_st->flags &= ~INP_FLAG_KB_MAPPING_BLOCKED;
   }
   else
   {
      if (code == RETROK_UNKNOWN)
         return;

      /* Check if keyboard events should be blocked when
       * pressing hotkeys and RetroPad binds, but
       * - not with Game Focus
       * - not from keyboard device type mappings
       * - not from overlay keyboard input
       * - with 'enable_hotkey' modifier set and unpressed.
       *
       * Also do not block key up events, because keys will
       * get stuck if Game Focus key is also pressing a key. */
      if (     down
            && !input_st->game_focus_state.enabled
            && BIT512_GET(input_st->keyboard_mapping_bits, code)
            && device != RETRO_DEVICE_POINTER)
      {
         unsigned j;
         settings_t *settings        = config_get_ptr();
         unsigned max_users          = settings->uints.input_max_users;
         bool hotkey_pressed         = (input_st->input_hotkey_block_counter > 0);
         bool block_key_event        = false;

         /* Loop enabled ports for keycode dupes. */
         for (j = 0; j < max_users; j++)
         {
            unsigned k;
            unsigned hotkey_code = RETRO_KEYBIND_KEY(&input_config_binds[0][RARCH_ENABLE_HOTKEY]);

            /* Block hotkey key events based on 'enable_hotkey' modifier,
             * and only when modifier is a keyboard key. */
            if (     j == 0
                  && !block_key_event
                  && !( !hotkey_pressed
                  &&     hotkey_code != RETROK_UNKNOWN
                  &&     hotkey_code != code))
            {
               for (k = RARCH_FIRST_META_KEY; k < RARCH_BIND_LIST_END; k++)
               {
                  if (RETRO_KEYBIND_KEY(&input_config_binds[j][k]) == code)
                  {
                     block_key_event = true;
                     break;
                  }
               }
            }

            /* RetroPad blocking needed only when emulated
             * device type is active. */
            if (     input_config_get_device(j)
                  && !block_key_event)
            {
               for (k = 0; k < RARCH_FIRST_META_KEY; k++)
               {
                  if (RETRO_KEYBIND_KEY(&input_config_binds[j][k]) == code)
                  {
                     block_key_event = true;
                     break;
                  }
               }
            }
         }

         /* No blocking when event comes from emulated keyboard device type */
         if (MAPPER_GET_KEY(&input_st->mapper, code))
            block_key_event = false;

         if (block_key_event)
            return;
      }

      if (*key_event)
      {
         if (*key_event == runloop_st->frontend_key_event)
         {
#ifdef HAVE_BSV_MOVIE
            if (BSV_MOVIE_IS_RECORDING())
#ifdef HAVE_REWIND
               if (!state_manager_frame_is_reversed())
#endif
               bsv_movie_push_key_event(
                     input_st->bsv_movie_state_handle, down, mod,
                     code, character);
#endif
         }
         (*key_event)(down, code, character, mod);
         /* Run-ahead's second instance polls on frames the running
          * core does not; it sees the event when it arrives there */
         if (     *key_event == runloop_st->frontend_key_event
               && runloop_st->secondary_key_event)
            runloop_st->secondary_key_event(down, code, character, mod);
      }
   }
}
