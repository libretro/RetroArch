/* Netplay input bookkeeping, driven directly.
 *
 * netplay_real_clients() and netplay_input_decode() are extracted from
 * netplay_frontend.c between their BEGIN/END markers. Input states are
 * allocated at their exact word count, so under ASan a read past one
 * fails, and under UBSan an out-of-range shift does. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <boolean.h>
#include <libretro.h>

#include "../../../network/netplay/netplay_private.h"

static unsigned failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("      FAIL: %s\n", msg); failures++; } } while (0)

#include "real_clients_fragment.c"
#include "input_decode_fragment.c"

static netplay_input_state_t state_new(uint32_t client, uint32_t size,
      bool used, netplay_input_state_t next)
{
   netplay_input_state_t s = (netplay_input_state_t)calloc(1,
         sizeof(*s) + (size - 1) * sizeof(uint32_t));
   s->client_num = client;
   s->size       = size;
   s->used       = used;
   s->next       = next;
   return s;
}

static void state_list_free(netplay_input_state_t s)
{
   while (s)
   {
      netplay_input_state_t next = s->next;
      free(s);
      s = next;
   }
}

/* A slave files its own input under MAX_CLIENTS; it has no bit, and
 * client 31 keeps the top one. */
static void t_real_clients(void)
{
   netplay_input_state_t list;

   printf("   real_clients: a slave's own input, client 31, mismatched states\n");
   list = state_new(MAX_CLIENTS, 1, true, NULL);
   list = state_new(31, 1, true, list);
   list = state_new(5,  2, true, list);
   list = state_new(7,  1, false, list);
   list = state_new(0,  1, true, list);

   CHECK(netplay_real_clients(list, 1) == ((UINT32_C(1) << 31) | 1),
         "clients of size 1");
   CHECK(netplay_real_clients(list, 2) == (UINT32_C(1) << 5),
         "clients of size 2");
   CHECK(netplay_real_clients(NULL, 1) == 0, "empty list");
   state_list_free(list);
}

static uint32_t *words(uint32_t n)
{
   return (uint32_t*)calloc(n, sizeof(uint32_t));
}

static void t_decode(void)
{
   uint32_t *pad   = words(1);
   uint32_t *ana   = words(3);
   uint32_t *mouse = words(2);

   printf("   input_decode: queries outside what a state carries read 0\n");

   pad[0] = (UINT32_C(1) << 3) | (UINT32_C(1) << 31);
   CHECK(netplay_input_decode(pad, 1, RETRO_DEVICE_JOYPAD, 0, 3)  == 1, "joypad bit 3");
   CHECK(netplay_input_decode(pad, 1, RETRO_DEVICE_JOYPAD, 0, 31) == 1, "joypad bit 31");
   CHECK(netplay_input_decode(pad, 1, RETRO_DEVICE_JOYPAD, 0, 4)  == 0, "joypad bit 4");
   CHECK(netplay_input_decode(pad, 1, RETRO_DEVICE_JOYPAD, 0, 40) == 0, "joypad id 40");
   CHECK(netplay_input_decode(pad, 1, RETRO_DEVICE_JOYPAD, 0,
            RETRO_DEVICE_ID_JOYPAD_MASK) == (int16_t)pad[0], "joypad mask");

   ana[0] = UINT32_C(1) << RETRO_DEVICE_ID_JOYPAD_R2;
   ana[1] = (uint32_t)(uint16_t)-100 | ((uint32_t)(uint16_t)200 << 16);
   ana[2] = (uint32_t)(uint16_t)300  | ((uint32_t)(uint16_t)-400 << 16);
   CHECK(netplay_input_decode(ana, 3, RETRO_DEVICE_ANALOG,
            RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X)  == -100, "left x");
   CHECK(netplay_input_decode(ana, 3, RETRO_DEVICE_ANALOG,
            RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y)  ==  200, "left y");
   CHECK(netplay_input_decode(ana, 3, RETRO_DEVICE_ANALOG,
            RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X) ==  300, "right x");
   CHECK(netplay_input_decode(ana, 3, RETRO_DEVICE_ANALOG,
            RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y) == -400, "right y");
   CHECK(netplay_input_decode(ana, 3, RETRO_DEVICE_ANALOG,
            RETRO_DEVICE_INDEX_ANALOG_BUTTON, RETRO_DEVICE_ID_JOYPAD_R2) == 0x7fff,
         "analog button held");
   CHECK(netplay_input_decode(ana, 3, RETRO_DEVICE_ANALOG,
            RETRO_DEVICE_INDEX_ANALOG_BUTTON, RETRO_DEVICE_ID_JOYPAD_L2) == 0,
         "analog button released");
   CHECK(netplay_input_decode(ana, 3, RETRO_DEVICE_ANALOG,
            RETRO_DEVICE_INDEX_ANALOG_BUTTON, 16) == 0, "analog button id 16");
   CHECK(netplay_input_decode(ana, 3, RETRO_DEVICE_ANALOG,
            RETRO_DEVICE_INDEX_ANALOG_LEFT, 5) == 0, "analog stick id 5");
   CHECK(netplay_input_decode(ana, 3, RETRO_DEVICE_ANALOG, 3, 0) == 0, "analog index 3");
   CHECK(netplay_input_decode(ana, 2, RETRO_DEVICE_ANALOG,
            RETRO_DEVICE_INDEX_ANALOG_RIGHT, 0) == 0, "analog of the wrong size");

   mouse[0] = UINT32_C(1) << RETRO_DEVICE_ID_MOUSE_LEFT;
   mouse[1] = (uint32_t)(uint16_t)7 | ((uint32_t)(uint16_t)-8 << 16);
   CHECK(netplay_input_decode(mouse, 2, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X) == 7,
         "mouse x");
   CHECK(netplay_input_decode(mouse, 2, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y) == -8,
         "mouse y");
   CHECK(netplay_input_decode(mouse, 2, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT) == 1,
         "mouse left");
   CHECK(netplay_input_decode(mouse, 2, RETRO_DEVICE_MOUSE, 0, 40) == 0, "mouse id 40");

   free(pad);
   free(ana);
   free(mouse);
}

/* A joypad port sent as analog: the core gets back the device it set,
 * subclass and all, and the state carries buttons and both sticks */
static void t_analog_joypad(void)
{
   static const uint32_t devs[] = {
      RETRO_DEVICE_JOYPAD,
      RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0),
      RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 5) };
   static const uint32_t others[] = {
      RETRO_DEVICE_NONE, RETRO_DEVICE_ANALOG,
      RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG, 1),
      RETRO_DEVICE_MOUSE, RETRO_DEVICE_KEYBOARD, RETRO_DEVICE_LIGHTGUN };
   uint32_t *st = words(3);
   unsigned i;

   printf("   analog joypads: the core sees its own device, both sticks sync\n");
   for (i = 0; i < sizeof(devs) / sizeof(devs[0]); i++)
   {
      uint32_t wire = NETPLAY_DEVICE_ANALOG_JOYPAD(devs[i]);
      CHECK((wire & RETRO_DEVICE_MASK) == RETRO_DEVICE_ANALOG, "sent as analog");
      CHECK(NETPLAY_DEVICE_FOR_CORE(wire) == devs[i], "joypad given back");
   }
   for (i = 0; i < sizeof(others) / sizeof(others[0]); i++)
      CHECK(NETPLAY_DEVICE_FOR_CORE(others[i]) == others[i], "others untouched");

   st[0] = UINT32_C(1) << RETRO_DEVICE_ID_JOYPAD_A;
   st[1] = (uint32_t)(uint16_t)-32768 | ((uint32_t)(uint16_t)32767 << 16);
   st[2] = (uint32_t)(uint16_t)1234;
   CHECK(netplay_input_decode(st, 3, RETRO_DEVICE_JOYPAD, 0,
            RETRO_DEVICE_ID_JOYPAD_A) == 1, "joypad query, button");
   CHECK(netplay_input_decode(st, 3, RETRO_DEVICE_JOYPAD, 0,
            RETRO_DEVICE_ID_JOYPAD_B) == 0, "joypad query, other button");
   CHECK(netplay_input_decode(st, 3, RETRO_DEVICE_ANALOG,
            RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X) == -32768,
         "left x, full");
   CHECK(netplay_input_decode(st, 3, RETRO_DEVICE_ANALOG,
            RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y) == 32767,
         "left y, full");
   CHECK(netplay_input_decode(st, 3, RETRO_DEVICE_ANALOG,
            RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X) == 1234,
         "right x");
   free(st);
}

int main(void)
{
   printf("netplay input:\n");
   t_real_clients();
   t_decode();
   t_analog_joypad();
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("all passed\n");
   return 0;
}
