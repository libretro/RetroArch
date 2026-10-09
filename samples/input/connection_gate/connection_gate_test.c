/* Shipping connection admission, retirement and reuse with a reader
 * paused immediately after its atomic claim. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rthreads/rthreads.h>
#include "input/input_driver.h"
#include "input/connect/joypad_connection.h"

static retro_atomic_int_t *blocked_gate;
static retro_atomic_int_t claim_entered, claim_resume;

static int claim_add(retro_atomic_int_t *p, int value)
{
   int state = retro_atomic_fetch_add_int(p, value);
   if (p == blocked_gate)
   {
      retro_atomic_store_release_int(&claim_entered, 1);
      while (!retro_atomic_load_acquire_int(&claim_resume))
         sthread_yield();
   }
   return state;
}
#undef retro_atomic_fetch_add_int
#define retro_atomic_fetch_add_int(p, value) claim_add(p, value)
#include "../../../input/connect/joypad_connection.c"

void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
bool input_autoconfigure_disconnect(unsigned port, const char *name)
{ (void)port; (void)name; return true; }
void input_pad_connect(unsigned port, input_device_driver_t *driver)
{ (void)port; (void)driver; }

pad_connection_interface_t pad_connection_wii, pad_connection_wiiupro,
   pad_connection_ps3, pad_connection_ps4, pad_connection_snesusb,
   pad_connection_nesusb, pad_connection_wiiugca, pad_connection_ps2adapter,
   pad_connection_psxadapter, pad_connection_retrode,
   pad_connection_ps4_hori_mini, pad_connection_kade,
   pad_connection_dragonrise;

static int payload;
static retro_atomic_int_t destroyed, stop, bad_reads;
static int admitted;

#define CHECK(c, msg) do { if (!(c)) { \
   fprintf(stderr, "FAIL: %s\n", msg); exit(1); } } while (0)

static void *fake_init(void *data, uint32_t slot, hid_driver_t *driver)
{
   (void)data; (void)slot; (void)driver;
   payload = 42;
   return &payload;
}
static void fake_deinit(void *data)
{
   *(int*)data = 0;
   retro_atomic_fetch_add_int(&destroyed, 1);
}
static void fake_rumble(void *data, enum retro_rumble_effect effect,
      uint16_t strength)
{ (void)data; (void)effect; (void)strength; }
static int16_t fake_axis(void *data, unsigned axis)
{ (void)axis; return (int16_t)*(int*)data; }

static pad_connection_interface_t fake_iface;
static joypad_connection_entry_t entry;

static void publish(joypad_connection_t *slot)
{
   CHECK(pad_connection_pad_init_entry(slot, &entry, NULL, NULL) == 0,
         "slot was not reusable");
}
static void reader(void *data)
{
   joypad_connection_t *slot = (joypad_connection_t*)data;
   admitted = pad_connection_acquire(slot);
   if (admitted)
   {
      CHECK(*(int*)slot->connection == 42, "admitted reader lost its payload");
      pad_connection_release(slot);
   }
}
static void retire(void *data)
{
   pad_connection_pad_deinit((joypad_connection_t*)data, 0);
}
static void read_many(void *data)
{
   joypad_connection_t *slot = (joypad_connection_t*)data;
   while (!retro_atomic_load_acquire_int(&stop))
   {
      int value = pad_connection_get_axis(slot, 0, 0);
      if (value != 0 && value != 42)
         retro_atomic_fetch_add_int(&bad_reads, 1);
   }
}

int main(void)
{
   joypad_connection_t *slot;
   sthread_t *r, *owner, *readers[4];
   unsigned i;
   fake_iface.init       = fake_init;
   fake_iface.deinit     = fake_deinit;
   fake_iface.set_rumble = fake_rumble;
   fake_iface.get_axis   = fake_axis;
   entry.iface           = &fake_iface;
   slot = pad_connection_init(1);
   CHECK(slot, "allocation failed");
   CHECK(!pad_connection_acquire(slot), "closed slot admitted a reader");
   publish(slot);

   blocked_gate = &slot->gate;
   r = sthread_create(reader, slot);
   CHECK(r, "reader creation failed");
   while (!retro_atomic_load_acquire_int(&claim_entered)) sthread_yield();
   owner = sthread_create(retire, slot);
   CHECK(owner, "owner creation failed");
   while (retro_atomic_load_acquire_int(&slot->gate) & CONNECTION_LIVE)
      sthread_yield();
   CHECK(!retro_atomic_load_acquire_int(&destroyed),
         "retirement destroyed an admitted reader's payload");
   retro_atomic_store_release_int(&claim_resume, 1);
   sthread_join(r);
   sthread_join(owner);
   CHECK(admitted && retro_atomic_load_acquire_int(&destroyed) == 1,
         "admitted reader did not finish before destruction");

   retro_atomic_store_release_int(&claim_entered, 0);
   retro_atomic_store_release_int(&claim_resume, 0);
   r = sthread_create(reader, slot);
   CHECK(r, "rejected reader creation failed");
   while (!retro_atomic_load_acquire_int(&claim_entered)) sthread_yield();
   publish(slot);
   CHECK(retro_atomic_load_acquire_int(&slot->gate)
         == CONNECTION_LIVE + CONNECTION_USER,
         "publication overwrote a rejected reader's outstanding claim");
   retro_atomic_store_release_int(&claim_resume, 1);
   sthread_join(r);
   blocked_gate = NULL;
   CHECK(!admitted && retro_atomic_load_acquire_int(&slot->gate) == CONNECTION_LIVE,
         "old rejected reader corrupted the reused gate");
   CHECK(pad_connection_acquire(slot), "reused slot rejected a new reader");
   pad_connection_release(slot);
   pad_connection_pad_deinit(slot, 0);

   for (i = 0; i < 4; i++)
   {
      readers[i] = sthread_create(read_many, slot);
      CHECK(readers[i], "stress reader creation failed");
   }
   for (i = 0; i < 1000; i++)
   {
      publish(slot);
      sthread_yield();
      pad_connection_pad_deinit(slot, 0);
   }
   retro_atomic_store_release_int(&stop, 1);
   for (i = 0; i < 4; i++) sthread_join(readers[i]);
   CHECK(!retro_atomic_load_acquire_int(&bad_reads), "reader saw invalid payload");
   CHECK(!retro_atomic_load_acquire_int(&slot->gate), "closed gate leaked readers");
   pad_connection_destroy(slot);
   puts("PASS connection gate: admitted retirement, rejected reuse, concurrent churn");
   return 0;
}
