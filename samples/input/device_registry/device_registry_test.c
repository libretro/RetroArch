/* device_registry_test.c -- which controllers there are, and which of
 * them has been here before.
 *
 * A joypad driver knows a controller by its slot. The registry gives
 * each one a device id that it keeps when it goes away and comes back,
 * and a handle that is good for one stay.
 *
 * The claims:
 *
 *   1. A controller that leaves and returns has the id it had, and a
 *      new handle. The old handle no longer resolves.
 *   2. A handle stops resolving when the controller leaves, when its
 *      driver reports it again, and when something else takes its
 *      slot.
 *   3. Two identical pads have different ids while both are present.
 *   4. A different kind of controller never gets another's id, even
 *      in the same slot.
 *   5. Among identical pads that left, the one that was plugged in at
 *      the same place comes back as itself; failing that the one from
 *      the same slot; failing that the one that left last. A pad moved
 *      to another socket is still that pad.
 *   6. A driver that starts over and reports its controllers again
 *      gives every one of them its id back.
 *   7. A full table gives up the controller that left longest ago,
 *      never one that is present, and a handle from the record's
 *      previous owner does not resolve to the new one.
 *   8. Through 200000 arrivals and departures: no two controllers
 *      present share a slot or an id, every handle given for a
 *      current stay resolves to its controller, and no handle from an
 *      ended stay resolves at all.
 *
 *   9. A driver that starts over and reports the controllers it had,
 *      in any order and in any slots, has each one put back on the
 *      port it was on, and the port mapping stays one-to-one.
 *  10. That happens only for the same controllers: if one is missing,
 *      a new one turns up, or one leaves again, nothing is moved.
 *  11. A driver that starts over twice before reporting still puts
 *      everyone back where they were before the first.
 *
 * The registry is the frontend's own, from input_registry.h. A
 * sabotage mode lets a handle outlive its stay, the way a kept slot
 * number does, and is asserted to be caught. */

#include <stdio.h>
#include <string.h>

#include <boolean.h>

#include "input_registry.h"

static unsigned failures = 0;
static bool     quiet    = false;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         if (!quiet) \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
         failures++; \
      } \
   } while (0)

/* A handle that is checked the way a slot number would be: is there
 * anything in that record at all. */
static bool sabotage_stale_handles = false;

static const input_device_record_t *resolve(const input_registry_t *reg,
      input_device_handle_t handle)
{
   if (sabotage_stale_handles)
   {
      unsigned i = handle & 0xff;
      if (i && i <= INPUT_REGISTRY_RECORDS && reg->records[i - 1].present)
         return &reg->records[i - 1];
      return NULL;
   }
   return input_registry_get(reg, handle);
}

static uint32_t id_of(const input_registry_t *reg, input_device_handle_t h)
{
   const input_device_record_t *rec = input_registry_get(reg, h);
   return rec ? rec->id : 0;
}

/* 1, 2, 4 */
static void lane_one_pad(void)
{
   static input_registry_t reg;
   input_device_handle_t a, b, c;
   uint32_t id;
   bool returned = true;

   input_registry_init(&reg);

   a  = input_registry_connect(&reg, "udev", 0, "Pad", "", 0x45e, 0x28e, &returned);
   id = id_of(&reg, a);
   CHECK(a && id, "a controller was not registered");
   CHECK(!returned, "a controller seen for the first time 'returned'");
   CHECK(input_registry_count(&reg) == 1, "one controller is not one");

   CHECK(input_registry_disconnect(&reg, 0), "a present controller could not leave");
   CHECK(!resolve(&reg, a), "a handle resolves after its controller left");
   CHECK(input_registry_count(&reg) == 0, "a controller that left is still counted");
   CHECK(!input_registry_disconnect(&reg, 0), "an empty slot was disconnected");

   b = input_registry_connect(&reg, "udev", 3, "Pad", "", 0x45e, 0x28e, &returned);
   CHECK(returned, "a controller that came back was not recognised");
   CHECK(id_of(&reg, b) == id, "a controller came back with another id");
   CHECK(b != a, "a new stay got the old handle");
   CHECK(!resolve(&reg, a), "the old handle resolves again after a return");

   /* the driver reports it again */
   c = input_registry_connect(&reg, "udev", 3, "Pad", "", 0x45e, 0x28e, &returned);
   CHECK(id_of(&reg, c) == id, "a controller reported again changed id");
   CHECK(!resolve(&reg, b), "a handle survived its controller being reported again");
   CHECK(input_registry_count(&reg) == 1, "reporting a controller again made two");

   /* something else takes the slot */
   a = input_registry_connect(&reg, "udev", 3, "Other pad", "", 0x54c, 0x9cc, &returned);
   CHECK(!returned, "a different controller 'returned'");
   CHECK(id_of(&reg, a) != id, "a different controller got another's id");
   CHECK(!resolve(&reg, c), "a handle survived its controller losing its slot");
   CHECK(input_registry_count(&reg) == 1, "a displaced controller is still present");

   /* same name, another driver: not the same controller */
   input_registry_disconnect(&reg, 3);
   b = input_registry_connect(&reg, "sdl2", 0, "Pad", "", 0x45e, 0x28e, &returned);
   CHECK(!returned && id_of(&reg, b) != id,
         "a controller from another driver took an id that was not its own");
}

/* 3, 5 */
static void lane_identical_pads(void)
{
   static input_registry_t reg;
   input_device_handle_t h;
   uint32_t id_a, id_b, id_c;

   /* no location: slot, then recency */
   input_registry_init(&reg);
   id_a = id_of(&reg, input_registry_connect(&reg, "x", 0, "Twin", "", 1, 2, NULL));
   id_b = id_of(&reg, input_registry_connect(&reg, "x", 1, "Twin", "", 1, 2, NULL));
   CHECK(id_a && id_b && id_a != id_b, "two identical pads share an id");

   input_registry_disconnect(&reg, 0);
   input_registry_disconnect(&reg, 1);
   h = input_registry_connect(&reg, "x", 0, "Twin", "", 1, 2, NULL);
   CHECK(id_of(&reg, h) == id_a, "the pad back in slot 0 is not the one that was there");
   h = input_registry_connect(&reg, "x", 1, "Twin", "", 1, 2, NULL);
   CHECK(id_of(&reg, h) == id_b, "the pad back in slot 1 is not the one that was there");

   input_registry_disconnect(&reg, 0);   /* a leaves first */
   input_registry_disconnect(&reg, 1);   /* b leaves last */
   h = input_registry_connect(&reg, "x", 5, "Twin", "", 1, 2, NULL);
   CHECK(id_of(&reg, h) == id_b, "in a new slot, the pad that left last did not come back first");
   h = input_registry_connect(&reg, "x", 6, "Twin", "", 1, 2, NULL);
   CHECK(id_of(&reg, h) == id_a, "the second identical pad did not get the remaining id");

   /* with a location: the place decides, whatever the slot */
   input_registry_init(&reg);
   id_a = id_of(&reg, input_registry_connect(&reg, "x", 0, "Twin", "usb-1.1", 1, 2, NULL));
   id_b = id_of(&reg, input_registry_connect(&reg, "x", 1, "Twin", "usb-1.2", 1, 2, NULL));
   input_registry_disconnect(&reg, 0);
   input_registry_disconnect(&reg, 1);
   h = input_registry_connect(&reg, "x", 0, "Twin", "usb-1.2", 1, 2, NULL);
   CHECK(id_of(&reg, h) == id_b, "a pad back at its own socket took the id of the slot's last pad");
   h = input_registry_connect(&reg, "x", 1, "Twin", "usb-1.1", 1, 2, NULL);
   CHECK(id_of(&reg, h) == id_a, "the other pad did not get its id back at its own socket");

   /* moved to another socket: still that pad, and not a third */
   input_registry_disconnect(&reg, 1);
   h = input_registry_connect(&reg, "x", 1, "Twin", "usb-2.4", 1, 2, NULL);
   CHECK(id_of(&reg, h) == id_a, "a pad moved to another socket lost its id");

   /* a third identical pad while both are present is a third */
   h    = input_registry_connect(&reg, "x", 2, "Twin", "usb-3.1", 1, 2, NULL);
   id_c = id_of(&reg, h);
   CHECK(id_c && id_c != id_a && id_c != id_b, "a third identical pad reused an id in use");
}

/* 6 */
static void lane_driver_restart(void)
{
   static input_registry_t reg;
   uint32_t id[4];
   input_device_handle_t h[4];
   unsigned i;

   input_registry_init(&reg);
   for (i = 0; i < 4; i++)
   {
      h[i]  = input_registry_connect(&reg, "x", i, (i & 1) ? "Odd" : "Even",
            "", 7, (uint16_t)(i / 2), NULL);
      id[i] = id_of(&reg, h[i]);
   }
   /* the driver starts over: no disconnects, every pad reported again */
   for (i = 0; i < 4; i++)
   {
      bool returned = false;
      input_device_handle_t n = input_registry_connect(&reg, "x", i,
            (i & 1) ? "Odd" : "Even", "", 7, (uint16_t)(i / 2), &returned);
      CHECK(returned && id_of(&reg, n) == id[i],
            "a controller lost its id when its driver started over");
      CHECK(!resolve(&reg, h[i]), "a handle survived its driver starting over");
   }
   CHECK(input_registry_count(&reg) == 4, "a driver starting over changed how many are present");
}

/* 7 */
static void lane_full_table(void)
{
   static input_registry_t reg;
   input_device_handle_t first, h, keep;
   char name[32];
   unsigned i;

   input_registry_init(&reg);

   keep = input_registry_connect(&reg, "x", 0, "Stays", "", 9, 9, NULL);

   /* fill the rest of the table with pads that have left */
   first = 0;
   for (i = 0; i < INPUT_REGISTRY_RECORDS - 1; i++)
   {
      sprintf(name, "Visitor %u", i);
      h = input_registry_connect(&reg, "x", 1, name, "", 9, (uint16_t)i, NULL);
      if (i == 0)
         first = h;
      input_registry_disconnect(&reg, 1);
   }
   CHECK(!resolve(&reg, first), "a visitor's handle resolves after it left");

   /* one more: the first visitor's record is the one given up */
   h = input_registry_connect(&reg, "x", 1, "Newcomer", "", 9, 1000, NULL);
   CHECK(h != 0, "a full table of absent controllers refused a new one");
   CHECK(resolve(&reg, keep) != NULL, "a present controller was given up for a new one");
   CHECK(!resolve(&reg, first),
         "a handle from a record's previous owner resolves to the new owner");
   {
      const input_device_record_t *rec = input_registry_get(&reg, h);
      CHECK(rec && !strcmp(rec->name, "Newcomer"), "the newcomer is not in the table");
   }

   /* the visitor that was given up is a stranger now; the others are not */
   input_registry_disconnect(&reg, 1);
   {
      bool returned = true;
      input_registry_connect(&reg, "x", 1, "Visitor 0", "", 9, 0, &returned);
      CHECK(!returned, "a controller whose record was given up 'returned'");
      input_registry_disconnect(&reg, 1);
      input_registry_connect(&reg, "x", 1, "Visitor 40", "", 9, 40, &returned);
      CHECK(returned, "a remembered controller was forgotten when the table filled");
   }
}

/* 8 */
#define STORM_SLOTS 16
#define STORM_KINDS 6

static void lane_storm(void)
{
   static input_registry_t reg;
   static input_device_handle_t ended[4096];
   input_device_handle_t current[STORM_SLOTS];
   unsigned n_ended = 0;
   uint32_t rng     = 0x2468aceu;
   unsigned step, i, j;
   unsigned bad_slot = 0, bad_id = 0, bad_current = 0, bad_stale = 0;

   input_registry_init(&reg);
   memset(current, 0, sizeof(current));

   for (step = 0; step < 200000; step++)
   {
      unsigned slot, kind;
      rng  = rng * 1664525u + 1013904223u;
      slot = (rng >> 8) % STORM_SLOTS;
      kind = (rng >> 16) % STORM_KINDS;

      if ((rng >> 28) < 6)
      {
         if (current[slot] && n_ended < 4096)
            ended[n_ended++] = current[slot];
         input_registry_disconnect(&reg, slot);
         current[slot] = 0;
      }
      else
      {
         char name[16];
         sprintf(name, "Kind %u", kind);
         if (current[slot] && n_ended < 4096)
            ended[n_ended++] = current[slot];
         current[slot] = input_registry_connect(&reg, "x", slot, name,
               "", 5, (uint16_t)kind, NULL);
      }

      if (step % 97)
         continue;

      for (i = 0; i < INPUT_REGISTRY_RECORDS; i++)
      {
         if (!reg.records[i].present)
            continue;
         for (j = i + 1; j < INPUT_REGISTRY_RECORDS; j++)
         {
            if (!reg.records[j].present)
               continue;
            if (reg.records[i].slot == reg.records[j].slot)
               bad_slot++;
            if (reg.records[i].id == reg.records[j].id)
               bad_id++;
         }
      }
      for (i = 0; i < STORM_SLOTS; i++)
      {
         const input_device_record_t *rec;
         if (!current[i])
            continue;
         rec = resolve(&reg, current[i]);
         if (!rec || rec->slot != i)
            bad_current++;
      }
      for (i = 0; i < n_ended; i++)
         if (resolve(&reg, ended[i]))
            bad_stale++;
   }

   CHECK(bad_slot == 0, "two present controllers share a slot");
   CHECK(bad_id == 0, "two present controllers share an id");
   CHECK(bad_current == 0, "a current handle does not resolve to its controller");
   CHECK(bad_stale == 0, "a handle from an ended stay resolves");
   CHECK(n_ended > 1000, "the storm ended too few stays to mean anything");
}

/* 9, 10, 11 */
#define PORTS 8

static void ports_identity(unsigned *index, uint8_t *port_of_slot)
{
   unsigned i;
   for (i = 0; i < PORTS; i++)
   {
      index[i]        = i;
      port_of_slot[i] = (uint8_t)i;
   }
}

static bool ports_one_to_one(const unsigned *index)
{
   unsigned i, j;
   for (i = 0; i < PORTS; i++)
   {
      if (index[i] >= PORTS)
         return false;
      for (j = i + 1; j < PORTS; j++)
         if (index[i] == index[j])
            return false;
   }
   return true;
}

/* three different pads on ports 1 to 3, in slots 0 to 2 */
static void three_pads(input_registry_t *reg, uint32_t *id)
{
   static const char *const name[3] = { "Red", "Green", "Blue" };
   unsigned i;
   input_registry_init(reg);
   for (i = 0; i < 3; i++)
      id[i] = id_of(reg, input_registry_connect(reg, "x", i, name[i],
               "", 3, (uint16_t)i, NULL));
}

static unsigned port_of(const input_registry_t *reg, const unsigned *index,
      uint32_t id)
{
   unsigned i, q;
   for (i = 0; i < INPUT_REGISTRY_RECORDS; i++)
      if (reg->records[i].present && reg->records[i].id == id)
         for (q = 0; q < PORTS; q++)
            if (index[q] == reg->records[i].slot)
               return q;
   return PORTS;
}

static void lane_restart_ports(void)
{
   static input_registry_t reg;
   unsigned index[PORTS];
   uint8_t  port_of_slot[PORTS];
   uint32_t id[3];

   /* 9: the same three, reported in other slots and another order */
   three_pads(&reg, id);
   ports_identity(index, port_of_slot);
   input_registry_restart(&reg, port_of_slot, PORTS);
   CHECK(input_registry_count(&reg) == 0, "a restart left a controller present");
   CHECK(!input_registry_restart_complete(&reg), "a restart is complete before anyone is back");

   input_registry_connect(&reg, "x", 1, "Blue",  "", 3, 2, NULL);
   CHECK(!input_registry_restart_complete(&reg), "a restart is complete with two still out");
   CHECK(input_registry_restore_ports(&reg, index, PORTS) == 0 && index[0] == 0,
         "ports were moved before the restart was complete");
   input_registry_connect(&reg, "x", 2, "Red",   "", 3, 0, NULL);
   input_registry_connect(&reg, "x", 0, "Green", "", 3, 1, NULL);
   CHECK(input_registry_restart_complete(&reg), "a restart is not complete with everyone back");

   CHECK(input_registry_restore_ports(&reg, index, PORTS) > 0, "nobody was put back");
   CHECK(ports_one_to_one(index), "the port mapping is no longer one-to-one");
   CHECK(   port_of(&reg, index, id[0]) == 0
         && port_of(&reg, index, id[1]) == 1
         && port_of(&reg, index, id[2]) == 2,
         "a controller is not on the port it had before the restart");
   CHECK(!input_registry_restart_complete(&reg), "a restart is still pending after ports were restored");
   CHECK(input_registry_restore_ports(&reg, index, PORTS) == 0, "ports were restored twice");

   /* same slots, same order: nothing to move */
   three_pads(&reg, id);
   ports_identity(index, port_of_slot);
   input_registry_restart(&reg, port_of_slot, PORTS);
   input_registry_connect(&reg, "x", 0, "Red",   "", 3, 0, NULL);
   input_registry_connect(&reg, "x", 1, "Green", "", 3, 1, NULL);
   input_registry_connect(&reg, "x", 2, "Blue",  "", 3, 2, NULL);
   CHECK(input_registry_restore_ports(&reg, index, PORTS) == 0 && index[0] == 0
         && index[1] == 1 && index[2] == 2, "ports moved when nothing had");

   /* identical pads told apart by where they are plugged in */
   input_registry_init(&reg);
   id[0] = id_of(&reg, input_registry_connect(&reg, "x", 0, "Twin", "usb-1", 1, 2, NULL));
   id[1] = id_of(&reg, input_registry_connect(&reg, "x", 1, "Twin", "usb-2", 1, 2, NULL));
   ports_identity(index, port_of_slot);
   input_registry_restart(&reg, port_of_slot, PORTS);
   input_registry_connect(&reg, "x", 0, "Twin", "usb-2", 1, 2, NULL);
   input_registry_connect(&reg, "x", 1, "Twin", "usb-1", 1, 2, NULL);
   input_registry_restore_ports(&reg, index, PORTS);
   CHECK(   port_of(&reg, index, id[0]) == 0
         && port_of(&reg, index, id[1]) == 1,
         "identical pads at known sockets swapped ports over a restart");

   /* 10: one is missing */
   three_pads(&reg, id);
   ports_identity(index, port_of_slot);
   input_registry_restart(&reg, port_of_slot, PORTS);
   input_registry_connect(&reg, "x", 0, "Blue", "", 3, 2, NULL);
   input_registry_connect(&reg, "x", 1, "Red",  "", 3, 0, NULL);
   CHECK(!input_registry_restart_complete(&reg), "a restart is complete with a controller missing");
   CHECK(input_registry_restore_ports(&reg, index, PORTS) == 0
         && index[0] == 0 && index[1] == 1 && index[2] == 2,
         "ports were moved with a controller missing");

   /* 10: a new one turns up */
   three_pads(&reg, id);
   ports_identity(index, port_of_slot);
   input_registry_restart(&reg, port_of_slot, PORTS);
   input_registry_connect(&reg, "x", 0, "Blue",     "", 3, 2, NULL);
   input_registry_connect(&reg, "x", 3, "Stranger", "", 3, 9, NULL);
   input_registry_connect(&reg, "x", 1, "Red",      "", 3, 0, NULL);
   input_registry_connect(&reg, "x", 2, "Green",    "", 3, 1, NULL);
   CHECK(!input_registry_restart_complete(&reg)
         && input_registry_restore_ports(&reg, index, PORTS) == 0,
         "ports were moved though a new controller turned up");

   /* 10: one that came back leaves again */
   three_pads(&reg, id);
   ports_identity(index, port_of_slot);
   input_registry_restart(&reg, port_of_slot, PORTS);
   input_registry_connect(&reg, "x", 0, "Blue", "", 3, 2, NULL);
   input_registry_disconnect(&reg, 0);
   input_registry_connect(&reg, "x", 0, "Blue",  "", 3, 2, NULL);
   input_registry_connect(&reg, "x", 1, "Red",   "", 3, 0, NULL);
   input_registry_connect(&reg, "x", 2, "Green", "", 3, 1, NULL);
   CHECK(input_registry_restore_ports(&reg, index, PORTS) == 0,
         "ports were moved though a controller left during the restart");

   /* a disconnect for a slot nobody is in does not end the restart:
    * drivers report their old slots empty as they shut down */
   three_pads(&reg, id);
   ports_identity(index, port_of_slot);
   input_registry_restart(&reg, port_of_slot, PORTS);
   input_registry_disconnect(&reg, 2);
   input_registry_connect(&reg, "x", 1, "Blue",  "", 3, 2, NULL);
   input_registry_connect(&reg, "x", 0, "Green", "", 3, 1, NULL);
   input_registry_connect(&reg, "x", 3, "Red",   "", 3, 0, NULL);
   CHECK(input_registry_restore_ports(&reg, index, PORTS) > 0
         && port_of(&reg, index, id[0]) == 0
         && port_of(&reg, index, id[1]) == 1
         && port_of(&reg, index, id[2]) == 2
         && ports_one_to_one(index),
         "an empty slot's disconnect stopped a restart from restoring ports");

   /* 11: two restarts before anyone is reported */
   three_pads(&reg, id);
   ports_identity(index, port_of_slot);
   input_registry_restart(&reg, port_of_slot, PORTS);
   input_registry_connect(&reg, "x", 2, "Red", "", 3, 0, NULL);
   {
      /* the second restart sees Red in slot 2, which the mapping calls
       * port 3; Red is owed port 1 from the first */
      input_registry_restart(&reg, port_of_slot, PORTS);
   }
   input_registry_connect(&reg, "x", 0, "Blue",  "", 3, 2, NULL);
   input_registry_connect(&reg, "x", 1, "Red",   "", 3, 0, NULL);
   input_registry_connect(&reg, "x", 2, "Green", "", 3, 1, NULL);
   CHECK(input_registry_restore_ports(&reg, index, PORTS) > 0
         && port_of(&reg, index, id[0]) == 0
         && port_of(&reg, index, id[1]) == 1
         && port_of(&reg, index, id[2]) == 2,
         "a second restart lost the ports from before the first");
}

static void all_lanes(void)
{
   lane_one_pad();
   lane_identical_pads();
   lane_driver_restart();
   lane_full_table();
   lane_storm();
   lane_restart_ports();
}

int main(void)
{
   all_lanes();
   if (failures)
   {
      fprintf(stderr, "FAIL device_registry_test: %u failures\n", failures);
      return 1;
   }

   /* A handle that outlives its stay has to be caught. */
   quiet                  = true;
   sabotage_stale_handles = true;
   all_lanes();
   quiet                  = false;
   sabotage_stale_handles = false;

   if (!failures)
   {
      fprintf(stderr, "FAIL device_registry_test: handles outliving their"
            " stay went unnoticed\n");
      return 1;
   }

   fprintf(stderr, "PASS device_registry_test (sabotage caught by %u"
         " checks)\n", failures);
   return 0;
}
