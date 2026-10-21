/* A core's RETRO_DEVICE_POINTER read where the input driver has a
 * second list of pointers (Android's second display), on
 * RARCH_DEVICE_POINTER_SECOND.
 *
 * input_state_device() and the overlay's pointer answer are extracted
 * from input_driver.c (extract.py). The stand-in driver answers a main
 * list and a second list; each read is one core query, the driver's
 * own answer passed in as input_state_internal() passes it. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "input/input_driver.h"

#define input_turbo_hold_apply(turbo, hold, settings, port, id, res) (res)

#define CHECK(c) do { if (!(c)) { \
   fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); exit(1); } } while (0)

#define LIST_MAX 8

struct finger
{
   int16_t x, y;
   bool down;
};

static retro_keybind_set binds;
struct retro_keybind *input_config_binds[MAX_USERS] = { binds };

static struct finger main_list[LIST_MAX];
static struct finger second_list[LIST_MAX];
static unsigned main_count, second_count;
static bool has_second;   /* the driver answers RARCH_DEVICE_POINTER_SECOND */

static input_driver_state_t state;
static settings_t settings;
static input_overlay_t overlay;

static int16_t driver_state(unsigned device, unsigned idx, unsigned id)
{
   const struct finger *list;
   unsigned count;

   if (device == RETRO_DEVICE_POINTER)
   {
      list  = main_list;
      count = main_count;
   }
   else if (device == RARCH_DEVICE_POINTER_SECOND && has_second)
   {
      list  = second_list;
      count = second_count;
   }
   else
      return 0;
   if (id == RETRO_DEVICE_ID_POINTER_COUNT)
      return (int16_t)count;
   if (idx >= LIST_MAX)
      return 0;
   /* X and Y past the count are what was last there, as Android's */
   switch (id)
   {
      case RETRO_DEVICE_ID_POINTER_X:
         return list[idx].x;
      case RETRO_DEVICE_ID_POINTER_Y:
         return list[idx].y;
      case RETRO_DEVICE_ID_POINTER_PRESSED:
         return idx < count && list[idx].down;
      default:
         break;
   }
   return 0;
}

bool input_driver_has_device_state(void)
{
   return true;
}

int16_t input_driver_device_state(unsigned port,
      unsigned device, unsigned idx, unsigned id)
{
   (void)port;
   return driver_state(device, idx, id);
}

bool input_driver_pointer_is_offscreen(int16_t x, int16_t y)
{
   return x == -0x8000 || y == -0x8000;
}

static int16_t input_overlay_device_mouse_state(
      input_overlay_t *ol, unsigned id)
{
   (void)ol;
   (void)id;
   return 0;
}

static int16_t input_overlay_lightgun_state(
      bool input_overlay_lightgun_allow_offscreen,
      input_overlay_t *ol, unsigned id)
{
   (void)input_overlay_lightgun_allow_offscreen;
   (void)ol;
   (void)id;
   return 0;
}

#include "driver_code.inc"

static int16_t core_read(unsigned idx, unsigned id)
{
   int32_t ret = driver_state(RETRO_DEVICE_POINTER, idx, id);
   return input_state_device(&state, &settings, &state.mapper,
         ANALOG_DPAD_NONE, ret, 0, RETRO_DEVICE_POINTER, idx, id, false);
}

static void finger_set(struct finger *f, int x, int y, bool down)
{
   f->x    = (int16_t)x;
   f->y    = (int16_t)y;
   f->down = down;
}

static void check_finger(unsigned idx, int x, int y, bool down)
{
   CHECK(core_read(idx, RETRO_DEVICE_ID_POINTER_X) == x);
   CHECK(core_read(idx, RETRO_DEVICE_ID_POINTER_Y) == y);
   CHECK(core_read(idx, RETRO_DEVICE_ID_POINTER_PRESSED) == down);
}

/* Two main fingers and a stale third; three on the second display. */
static void reset(bool second)
{
   unsigned i;
   struct retro_keybind valid;

   memset(&valid, 0, sizeof(valid));
   valid.joykey  = NO_BTN;
   valid.joyaxis = AXIS_NONE;
   RETRO_KEYBIND_SET_VALID(&valid, true);
   for (i = 0; i < RARCH_BIND_LIST_END; i++)
      binds[i] = valid;

   memset(&state, 0, sizeof(state));
   memset(&settings, 0, sizeof(settings));
   memset(&overlay, 0, sizeof(overlay));
   memset(main_list, 0, sizeof(main_list));
   memset(second_list, 0, sizeof(second_list));
   settings.ints.input_overlay_lightgun_port = -1;

   finger_set(&main_list[0], 100, 200, true);
   finger_set(&main_list[1], 110, 210, true);
   finger_set(&main_list[2], 777, 778, false);
   main_count = 2;
   finger_set(&second_list[0], -300, 400, true);
   finger_set(&second_list[1], -310, 410, true);
   finger_set(&second_list[2], -320, 420, true);
   second_count = 3;
   has_second   = second;
}

/* The overlay answering for the pointer, as its poll leaves it: one
 * finger off its buttons, and the driver's pointer blocked. */
static void overlay_owns(unsigned count)
{
   state.overlay_ptr                    = &overlay;
   overlay.flags                        = INPUT_OVERLAY_ENABLE;
   overlay.pointer_state.ptr[0]         = VIDEO_POS_PACK(5000, 6000);
   overlay.pointer_state.count          = count;
   settings.bools.input_overlay_pointer_enable = true;
   state.flags                         |= INP_FLAG_BLOCK_POINTER_INPUT;
}

/* Overlay pointer off and one of its buttons held. */
static void overlay_blocks(void)
{
   state.overlay_ptr  = &overlay;
   overlay.flags      = INPUT_OVERLAY_ENABLE;
   state.flags       |= INP_FLAG_BLOCK_POINTER_INPUT;
}

/* A driver with no second list: only the main list answers. */
static void t_no_second(void)
{
   reset(false);
   CHECK(core_read(0, RETRO_DEVICE_ID_POINTER_COUNT) == 2);
   check_finger(0, 100, 200, true);
   check_finger(1, 110, 210, true);
   check_finger(2, 777, 778, false);

   reset(true);
   second_count = 0;
   CHECK(core_read(0, RETRO_DEVICE_ID_POINTER_COUNT) == 2);
   check_finger(2, 777, 778, false);

   reset(false);
   overlay_owns(1);
   CHECK(core_read(0, RETRO_DEVICE_ID_POINTER_COUNT) == 1);
   check_finger(0, 5000, 6000, true);
   check_finger(1, 0, 0, false);

   reset(false);
   overlay_blocks();
   CHECK(core_read(0, RETRO_DEVICE_ID_POINTER_COUNT) == 0);
   check_finger(0, 0, 0, false);
   puts("[pass] no second list: the main list's answers as they were");
}

static void t_second_alone(void)
{
   reset(true);
   CHECK(core_read(0, RETRO_DEVICE_ID_POINTER_COUNT) == 5);
   check_finger(0, 100, 200, true);
   check_finger(1, 110, 210, true);
   check_finger(2, -300, 400, true);
   check_finger(3, -310, 410, true);
   check_finger(4, -320, 420, true);
   check_finger(5, 0, 0, false);

   main_count = 0;
   CHECK(core_read(0, RETRO_DEVICE_ID_POINTER_COUNT) == 3);
   check_finger(0, -300, 400, true);
   check_finger(2, -320, 420, true);
   puts("[pass] no overlay: the second list after the driver's main list");
}

static void t_second_overlay(void)
{
   reset(true);
   overlay_owns(1);
   CHECK(core_read(0, RETRO_DEVICE_ID_POINTER_COUNT) == 4);
   check_finger(0, 5000, 6000, true);
   check_finger(1, -300, 400, true);
   check_finger(3, -320, 420, true);
   check_finger(4, 0, 0, false);
   CHECK(overlay.pointer_state.device_mask & (1 << RETRO_DEVICE_POINTER));

   /* every main finger on a button: the overlay has none */
   reset(true);
   overlay_owns(0);
   CHECK(core_read(0, RETRO_DEVICE_ID_POINTER_COUNT) == 3);
   check_finger(0, -300, 400, true);
   check_finger(2, -320, 420, true);
   check_finger(3, 0, 0, false);
   puts("[pass] overlay pointer: the second list after the overlay's");
}

static void t_second_blocked(void)
{
   reset(true);
   overlay_blocks();
   CHECK(core_read(0, RETRO_DEVICE_ID_POINTER_COUNT) == 3);
   check_finger(0, -300, 400, true);
   check_finger(1, -310, 410, true);
   check_finger(2, -320, 420, true);
   check_finger(3, 0, 0, false);
   puts("[pass] overlay button held: the second list alone");
}

int main(void)
{
   t_no_second();
   t_second_alone();
   t_second_overlay();
   t_second_blocked();
   puts("pointer second: PASS");
   return 0;
}
