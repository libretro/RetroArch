/* The pad slots of input/connect/joypad_connection.c, read every frame
 * on one thread while a HID driver's own thread connects and
 * disconnects pads on another.
 *
 * A reader - get_buttons, get_axis, rumble, the name, has_interface,
 * a packet - calls into the pad's own data. Unplugging frees that data
 * in the pad's deinit. The slot must not be deinitialised while a
 * reader is inside it, and a reader that comes after must not go in.
 *
 * The same for the register/deregister pair, where the driver frees
 * the pad once it is deregistered, and for a driver that calls the
 * pad's interface itself under pad_connection_acquire().
 *
 * The player a slot's lights are to show is kept for the slot: a pad
 * started there is told it as it starts, and told again only when it
 * changes.
 *
 * And two threads looking for a vacant slot at once - a HID driver's
 * thread and the input poll, on the Wii U - must never be given the
 * same one.
 *
 * The fake pad poisons its data in deinit before freeing it, and every
 * call checks it: a call into a pad already torn down counts, and
 * under ASan is a use-after-free. Its calls linger a little, to give
 * an unplug the chance to land inside one. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#include "input/connect/joypad_connection.h"
#include "input/common/input_ignore_devices.h"

/* the frontend's; the user's list is the settings', here none */
bool input_device_ignored(uint16_t vid, uint16_t pid)
{ return input_ignore_devices_check(NULL, vid, pid); }

/* ---- the rest of RetroArch, stubbed ---- */

void RARCH_LOG(const char *f, ...)  { (void)f; }
void RARCH_WARN(const char *f, ...) { (void)f; }
void RARCH_ERR(const char *f, ...)  { (void)f; }
void RARCH_DBG(const char *f, ...)  { (void)f; }
bool input_autoconfigure_disconnect(unsigned port, const char *name)
{ (void)port; (void)name; return true; }
void input_pad_connect(unsigned port, input_device_driver_t *driver)
{ (void)port; (void)driver; }

pad_connection_interface_t pad_connection_wii, pad_connection_wiiupro,
   pad_connection_ps3, pad_connection_ps4, pad_connection_ps5,
   pad_connection_ps5_edge, pad_connection_switch_pro,
   pad_connection_switch2_pro,
   pad_connection_snesusb,
   pad_connection_nesusb, pad_connection_wiiugca,
   pad_connection_ps2adapter, pad_connection_psxadapter,
   pad_connection_retrode, pad_connection_ps4_hori_mini,
   pad_connection_kade, pad_connection_dragonrise,
   pad_connection_8bitdo_sn30_pro, pad_connection_8bitdo_sf30_pro,
   pad_connection_ps3_shanwan,
   pad_connection_8bitdo_pro_2, pad_connection_8bitdo_pro_3,
   pad_connection_8bitdo_ultimate_2, pad_connection_8bitdo_ultimate_2c,
   pad_connection_8bitdo_ultimate_3, pad_connection_ps5_third_party,
   pad_connection_switch_pro_n64, pad_connection_ps4_third_party,
   pad_connection_ps4_razer, pad_connection_ps4_no_output,
   pad_connection_gamecube_pc;

/* ---- the fake pad ---- */

#define PAD_LIVE 0x11FE11FE
#define PAD_DEAD 0x0DEAD0DE

typedef struct { retro_atomic_int_t magic; } fake_pad_t;

static retro_atomic_int_t calls_into_dead;
static retro_atomic_int_t inits, deinits, calls;

static void check_pad(void *data)
{
   unsigned i;
   fake_pad_t *pad = (fake_pad_t*)data;
   if (retro_atomic_load_acquire_int(&pad->magic) != PAD_LIVE)
      retro_atomic_fetch_add_int(&calls_into_dead, 1);
   /* linger */
   for (i = 0; i < 200; i++)
      retro_atomic_load_acquire_int(&pad->magic);
   if (retro_atomic_load_acquire_int(&pad->magic) != PAD_LIVE)
      retro_atomic_fetch_add_int(&calls_into_dead, 1);
   retro_atomic_fetch_add_int(&calls, 1);
}

static void *fake_init(void *data, uint32_t slot, hid_driver_t *driver)
{
   fake_pad_t *pad = (fake_pad_t*)malloc(sizeof(*pad));
   (void)data; (void)slot; (void)driver;
   retro_atomic_int_init(&pad->magic, PAD_LIVE);
   retro_atomic_fetch_add_int(&inits, 1);
   return pad;
}
static void fake_deinit(void *data)
{
   fake_pad_t *pad = (fake_pad_t*)data;
   retro_atomic_store_release_int(&pad->magic, PAD_DEAD);
   retro_atomic_fetch_add_int(&deinits, 1);
   free(pad);
}
static void fake_packet(void *data, uint8_t *p, uint16_t n)
{ (void)p; (void)n; check_pad(data); }
static void fake_rumble(void *data, enum retro_rumble_effect e, uint16_t s)
{ (void)e; (void)s; check_pad(data); }
static void fake_buttons(void *data, input_bits_t *state)
{ check_pad(data); BIT256_CLEAR_ALL_PTR(state); }
static int16_t fake_axis(void *data, unsigned axis)
{ (void)axis; check_pad(data); return 0; }
static const char *fake_name(void *data)
{ check_pad(data); return "fake pad"; }
static retro_atomic_int_t players_told, last_player;
static void fake_set_player(void *data, unsigned player)
{
   check_pad(data);
   retro_atomic_fetch_add_int(&players_told, 1);
   retro_atomic_store_release_int(&last_player, (int)player);
}

static pad_connection_interface_t fake_iface = {
   fake_init, fake_deinit, fake_packet, fake_rumble,
   fake_buttons, fake_axis, fake_name, NULL,
   false, 0, NULL, NULL, NULL, NULL, fake_set_player
};
static joypad_connection_entry_t fake_entry = { "fake", 1, 1, &fake_iface };

/* ---- the frontend, reading every slot as fast as it can ---- */

static joypad_connection_t *slots;
static retro_atomic_int_t   reading;

static void reader(void *unused)
{
   unsigned n = 0;
   (void)unused;
   while (retro_atomic_load_acquire_int(&reading))
   {
      input_bits_t state;
      /* the frontend telling the pad's port */
      pad_connection_set_player(&slots[0], 1 + (n++ & 1));
      uint8_t      pkt[4] = { 0 };
      pad_connection_get_buttons(&slots[0], 0, &state);
      pad_connection_get_axis(&slots[0], 0, 0);
      pad_connection_rumble(&slots[0], 0, RETRO_RUMBLE_STRONG, 1);
      pad_connection_get_name(&slots[0], 0);
      pad_connection_has_interface(slots, 0);
      pad_connection_packet(&slots[0], 0, pkt, 4);
      /* a driver that calls the pad itself, as the Wii U one does */
      if (pad_connection_acquire(&slots[0]))
      {
         if (slots[0].iface && slots[0].connection)
            slots[0].iface->get_buttons(slots[0].connection, &state);
         pad_connection_release(&slots[0]);
      }
   }
}

static void watchdog(int sig)
{
   (void)sig;
   printf("FAIL: hung\n");
   fflush(stdout);
   _exit(1);
}

#define CYCLES 3000

/* ---- two threads connecting pads at once ---- */

#define CLAIMS 20000

static retro_atomic_int_t holders[4];
static retro_atomic_int_t double_claims;

static void claimer(void *unused)
{
   unsigned i;
   (void)unused;
   for (i = 0; i < CLAIMS; i++)
   {
      int slot = pad_connection_find_vacant_pad(slots);
      if (slot < 0)
         continue;
      /* as the Wii U input drivers do: claim, mark, use, give back */
      if (retro_atomic_fetch_add_int(&holders[slot], 1) != 0)
         retro_atomic_fetch_add_int(&double_claims, 1);
      slots[slot].connected = true;
      {
         /* hold it a moment, as a pad being set up is */
         unsigned k;
         for (k = 0; k < 64; k++)
            retro_atomic_load_acquire_int(&holders[slot]);
      }
      retro_atomic_fetch_sub_int(&holders[slot], 1);
      slots[slot].connected = false;
      pad_connection_release_slot(&slots[slot]);
   }
}

int main(void)
{
   sthread_t *r[2];
   unsigned   i, failures = 0;

   signal(SIGALRM, watchdog);
   alarm(120);

   slots = pad_connection_init(4);
   retro_atomic_store_release_int(&reading, 1);
   r[0]  = sthread_create(reader, NULL);
   r[1]  = sthread_create(reader, NULL);

   /* The HID driver's thread: plug, a moment, unplug. */
   for (i = 0; i < CYCLES; i++)
   {
      int slot = pad_connection_pad_init_entry(slots, &fake_entry, NULL, NULL);
      if (slot != 0)
      {
         printf("FAIL: pad landed in slot %d\n", slot);
         failures++;
         break;
      }
      if (i & 1)
         usleep(20);
      pad_connection_pad_deinit(&slots[0], 0);
   }

   /* The register/deregister pair: the pad is the driver's own data,
    * freed by the driver once deregistered. */
   for (i = 0; i < CYCLES && !failures; i++)
   {
      fake_pad_t *pad = (fake_pad_t*)fake_init(NULL, 0, NULL);
      pad_connection_pad_register(slots, &fake_iface, pad, NULL, NULL,
            SLOT_AUTO);
      if (i & 1)
         usleep(20);
      pad_connection_pad_deregister(slots, &fake_iface, pad);
      fake_deinit(pad);
   }

   retro_atomic_store_release_int(&reading, 0);
   sthread_join(r[0]);
   sthread_join(r[1]);

   /* Two threads find vacant slots at once: never the same one. */
   r[0] = sthread_create(claimer, NULL);
   r[1] = sthread_create(claimer, NULL);
   sthread_join(r[0]);
   sthread_join(r[1]);
   if (retro_atomic_load_acquire_int(&double_claims))
   {
      printf("FAIL: a slot was handed to two threads at once %d time(s)\n",
            retro_atomic_load_acquire_int(&double_claims));
      failures++;
   }

   pad_connection_destroy(slots);

   /* the player told to an empty slot reaches the pad started there;
    * told again unchanged, it is not passed on */
   slots = pad_connection_init(4);
   pad_connection_pad_init_entry(slots, &fake_entry, NULL, NULL);
   pad_connection_set_player(&slots[1], 3);
   retro_atomic_store_release_int(&players_told, 0);
   if (     pad_connection_pad_init_entry(slots, &fake_entry, NULL, NULL) != 1
         || retro_atomic_load_acquire_int(&players_told) != 1
         || retro_atomic_load_acquire_int(&last_player) != 3)
   {
      printf("FAIL: a pad started in a slot was not told its player\n");
      failures++;
   }
   pad_connection_set_player(&slots[1], 3);
   pad_connection_set_player(&slots[1], 0);
   if (     retro_atomic_load_acquire_int(&players_told) != 2
         || retro_atomic_load_acquire_int(&last_player) != 0)
   {
      printf("FAIL: a player was told unchanged, or a change not told\n");
      failures++;
   }
   pad_connection_pad_deinit(&slots[0], 0);
   pad_connection_pad_deinit(&slots[1], 1);
   pad_connection_destroy(slots);
   alarm(0);

   if (retro_atomic_load_acquire_int(&calls_into_dead))
   {
      printf("FAIL: %d call(s) into a pad already torn down\n",
            retro_atomic_load_acquire_int(&calls_into_dead));
      failures++;
   }
   if (retro_atomic_load_acquire_int(&inits) != retro_atomic_load_acquire_int(&deinits))
   {
      printf("FAIL: %d pads set up, %d torn down\n",
            retro_atomic_load_acquire_int(&inits),
            retro_atomic_load_acquire_int(&deinits));
      failures++;
   }
   if (!retro_atomic_load_acquire_int(&calls))
   {
      printf("FAIL: the readers never reached a pad\n");
      failures++;
   }
   /* the table: Wii remotes on 057e:0330 by the name they give */
   {
      static const struct { const char *name; pad_connection_interface_t *iface; } t[3] = {
         { "Nintendo RVL-CNT-01",    &pad_connection_wii },
         { "Nintendo RVL-CNT-01-TR", &pad_connection_wii },
         { "Nintendo RVL-CNT-01-UC", &pad_connection_wiiupro } };
      for (i = 0; i < 3; i++)
      {
         joypad_connection_entry_t *e = find_connection_entry(
               SWAP_IF_BIG(0x057e), SWAP_IF_BIG(0x0330), t[i].name);
         if (!e || e->iface != t[i].iface)
         {
            printf("FAIL: \"%s\" on 057e:0330 not given its handler\n", t[i].name);
            failures++;
         }
      }
   }
   /* a DualShock 3 on Sony's id: ShanWan's handler only for a pad
    * named ShanWan's; on ShanWan's id, always */
   {
      static const struct { uint16_t vid, pid; const char *name;
         pad_connection_interface_t *iface; } t[4] = {
         { 0x054c, 0x0268, "PLAYSTATION(R)3 Controller", &pad_connection_ps3 },
         { 0x054c, 0x0268, "ShanWan PS3/PC Gamepad",     &pad_connection_ps3_shanwan },
         { 0x054c, 0x0268, "",                           &pad_connection_ps3 },
         { 0x2563, 0x0523, "PS3 GamePad",                &pad_connection_ps3_shanwan } };
      for (i = 0; i < 4; i++)
      {
         joypad_connection_entry_t *e = find_connection_entry(
               SWAP_IF_BIG(t[i].vid), SWAP_IF_BIG(t[i].pid), t[i].name);
         if (!e || e->iface != t[i].iface)
         {
            printf("FAIL: \"%s\" on %04x:%04x not given its handler\n",
                  t[i].name, t[i].vid, t[i].pid);
            failures++;
         }
      }
   }
   /* the right Joy-Con's id: only the NES and Famicom pads, by name */
   {
      joypad_connection_entry_t *e;
      e = find_connection_entry(SWAP_IF_BIG(0x057e), SWAP_IF_BIG(0x2007),
            "NES Controller (R)");
      if (!e || e->iface != &pad_connection_switch_pro)
      {
         printf("FAIL: a Switch Online NES pad not given the Pro Controller's handler\n");
         failures++;
      }
      e = find_connection_entry(SWAP_IF_BIG(0x057e), SWAP_IF_BIG(0x2007),
            "Joy-Con (R)");
      if (e)
      {
         printf("FAIL: a right Joy-Con given a handler\n");
         failures++;
      }
      e = find_connection_entry(SWAP_IF_BIG(0x1532), SWAP_IF_BIG(0x100b), "");
      if (!e || e->iface != &pad_connection_ps5_third_party)
      {
         printf("FAIL: a Razer Wolverine V2 Pro not given the DualSense handler\n");
         failures++;
      }
      e = find_connection_entry(SWAP_IF_BIG(0x1532), SWAP_IF_BIG(0x1000), "");
      if (!e || e->iface != &pad_connection_ps4_razer)
      {
         printf("FAIL: a Razer Raiju not given the DualShock 4 handler\n");
         failures++;
      }
      e = find_connection_entry(SWAP_IF_BIG(0x0e6f), SWAP_IF_BIG(0x0203), "");
      if (!e || e->iface != &pad_connection_ps4_no_output)
      {
         printf("FAIL: a Victrix Pro FS not kept from output\n");
         failures++;
      }
      e = find_connection_entry(SWAP_IF_BIG(0x0f0d), SWAP_IF_BIG(0x0055), "");
      if (!e || e->iface != &pad_connection_ps4_third_party)
      {
         printf("FAIL: a HORIPAD 4 not given the DualShock 4 handler\n");
         failures++;
      }
   }
   if (failures)
      return 1;
   printf("[pass] joypad_slots_test: %u plug cycles each way, %d calls into live pads; Wii remotes on 057e:0330, DualShock 3s and Switch Online pads by name, third-party DualShock 4s by id\n",
         CYCLES, retro_atomic_load_acquire_int(&calls));
   return 0;
}
