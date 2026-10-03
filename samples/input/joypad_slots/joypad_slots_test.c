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
   pad_connection_ps3, pad_connection_ps4, pad_connection_snesusb,
   pad_connection_nesusb, pad_connection_wiiugca,
   pad_connection_ps2adapter, pad_connection_psxadapter,
   pad_connection_retrode, pad_connection_ps4_hori_mini,
   pad_connection_kade, pad_connection_dragonrise;

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

static pad_connection_interface_t fake_iface = {
   fake_init, fake_deinit, fake_packet, fake_rumble,
   fake_buttons, fake_axis, fake_name, NULL,
   false, 0, NULL, NULL, NULL, NULL
};
static joypad_connection_entry_t fake_entry = { "fake", 1, 1, &fake_iface };

/* ---- the frontend, reading every slot as fast as it can ---- */

static joypad_connection_t *slots;
static retro_atomic_int_t   reading;

static void reader(void *unused)
{
   (void)unused;
   while (retro_atomic_load_acquire_int(&reading))
   {
      input_bits_t state;
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
   if (failures)
      return 1;
   printf("[pass] joypad_slots_test: %u plug cycles each way, %d calls into live pads\n",
         CYCLES, retro_atomic_load_acquire_int(&calls));
   return 0;
}
