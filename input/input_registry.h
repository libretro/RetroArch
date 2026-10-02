/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _INPUT_REGISTRY_H
#define _INPUT_REGISTRY_H

#include <stdint.h>
#include <string.h>

#include <boolean.h>
#include <retro_inline.h>

/* The device registry: which controllers there are, and which of them
 * has been here before.
 *
 * A joypad driver knows a controller by the slot it sits in, and a
 * slot is only a position: pull a pad out and plug another in and the
 * slot is the same, plug the first back in elsewhere and its slot is
 * different. Everything that should follow a controller around - its
 * port, its mapping, its calibration - needs to know the controller.
 *
 * The registry gives every controller two things:
 *
 * - A device id, for as long as the program runs. A controller that
 *   goes away and comes back gets the id it had. No two controllers
 *   present at once share one, and an id is never given to a
 *   different kind of controller.
 *
 * - A handle, for one stay. It stops resolving the moment the
 *   controller leaves, is re-reported by its driver, or is displaced
 *   from its slot, and the next stay gets a new one. Something that
 *   kept a handle across any of those finds out, where a kept slot
 *   number would quietly name whatever sits there now.
 *
 * Which controller has come back is decided from what the driver
 * reports: its own name, the controller's name and USB ids, and where
 * it is plugged in if the driver knows. Among controllers that left
 * and match on name and ids, the one last seen at the same place wins,
 * then the one last seen in the same slot, then the one that left most
 * recently; one last seen at a different place, when both places are
 * known, comes after those. Two identical pads with no location cannot
 * be told apart by this; they keep two ids between them, and which
 * gets which after both have left is the slot's and the order's doing.
 *
 * A driver that starts over is the one case where the registry also
 * says which port a controller belongs on. Nobody has plugged or
 * unplugged anything: the driver was torn down and brought back, on a
 * content load for one, and it reports its controllers again in
 * whatever order it finds them. input_registry_restart() notes who was
 * there and on which port; when every one of them has been reported
 * again, and nothing else has happened in between, each can be put
 * back on its port (input_registry_restore_ports()). If a controller
 * is missing, a new one turns up, or one leaves, that is no longer a
 * restart with the same controllers and ports are left to be assigned
 * the way they always were.
 *
 * This is the first piece of the device registry in the input plan
 * (WP-02). Beyond the restart above it mirrors what the drivers report
 * and decides nothing: a controller plugged in by hand goes where it
 * always went.
 *
 * One thread. The frontend calls it from the callbacks that apply a
 * connect or a disconnect, which run on the main thread. No allocation
 * and no driver, settings or libretro types, so that
 * samples/input/device_registry can build it on its own. */

/* Controllers present at once are bounded by the drivers' slots; the
 * rest of the table remembers ones that have left. */
#define INPUT_REGISTRY_RECORDS 64

typedef uint32_t input_device_handle_t; /* 0: none */

typedef struct
{
   input_device_handle_t handle; /* this stay's; 0 while away */
   uint32_t id;         /* the device id; 0: record unused */
   uint32_t key;        /* provider, name, vid and pid, hashed */
   uint32_t generation; /* stays so far */
   uint32_t seen;       /* registry clock when it last arrived or left */
   uint16_t vid;
   uint16_t pid;
   uint8_t  slot;       /* the driver slot it sits in, or last sat in */
   uint8_t  port;       /* the port it was on when its driver restarted */
   bool     present;
   bool     expected;   /* was here at the restart, not reported since */
   bool     restore;    /* reported again since: owed its port back */
   char     provider[32];
   char     name[128];
   char     phys[64];   /* where it is plugged in; "" if not known */
} input_device_record_t;

typedef struct input_registry
{
   input_device_record_t records[INPUT_REGISTRY_RECORDS];
   uint32_t next_id; /* the last device id given out */
   uint32_t clock;   /* bumped by every arrival and departure */
   unsigned expected_left; /* controllers a restart is still waiting for */
   bool     restarting;    /* between a restart and its last controller */
} input_registry_t;

#define INPUT_REGISTRY_NO_PORT 0xff

static INLINE void input_registry_init(input_registry_t *reg)
{
   memset(reg, 0, sizeof(*reg));
}

static INLINE uint32_t input_registry_hash(uint32_t h, const char *s)
{
   for (; s && *s; s++)
      h = (h ^ (uint8_t)*s) * 16777619u;
   return (h ^ 0xff) * 16777619u;
}

/* What makes two reports the same kind of controller. */
static INLINE uint32_t input_registry_key(const char *provider,
      const char *name, uint16_t vid, uint16_t pid)
{
   uint32_t h = 2166136261u;
   h = input_registry_hash(h, provider);
   h = input_registry_hash(h, name);
   h = (h ^ vid) * 16777619u;
   h = (h ^ pid) * 16777619u;
   return h ? h : 1;
}

static INLINE void input_registry_copy(char *dst, size_t len, const char *src)
{
   size_t i = 0;
   if (src)
      for (; i + 1 < len && src[i]; i++)
         dst[i] = src[i];
   dst[i] = '\0';
}

/* The controller present in @slot, or NULL. */
static INLINE input_device_record_t *input_registry_at_slot(
      input_registry_t *reg, unsigned slot)
{
   unsigned i;
   for (i = 0; i < INPUT_REGISTRY_RECORDS; i++)
      if (     reg->records[i].present
            && reg->records[i].slot == slot)
         return &reg->records[i];
   return NULL;
}

/* The controller @handle was given to, or NULL if that stay is over. */
static INLINE const input_device_record_t *input_registry_get(
      const input_registry_t *reg, input_device_handle_t handle)
{
   unsigned i = (handle & 0xff);
   if (!handle || i == 0 || i > INPUT_REGISTRY_RECORDS)
      return NULL;
   if (     !reg->records[i - 1].present
         ||  reg->records[i - 1].handle != handle)
      return NULL;
   return &reg->records[i - 1];
}

static INLINE unsigned input_registry_count(const input_registry_t *reg)
{
   unsigned i, n = 0;
   for (i = 0; i < INPUT_REGISTRY_RECORDS; i++)
      if (reg->records[i].present)
         n++;
   return n;
}

/* A restart is over, or is no longer one: nobody is owed a port. */
static INLINE void input_registry_restart_cancel(input_registry_t *reg)
{
   unsigned i;
   for (i = 0; i < INPUT_REGISTRY_RECORDS; i++)
   {
      reg->records[i].expected = false;
      reg->records[i].restore  = false;
   }
   reg->expected_left = 0;
   reg->restarting    = false;
}

static INLINE bool input_registry_leave_(input_registry_t *reg,
      unsigned slot)
{
   input_device_record_t *rec = input_registry_at_slot(reg, slot);
   if (!rec)
      return false;
   rec->present = false;
   rec->restore = false;
   rec->handle  = 0;
   rec->seen    = ++reg->clock;
   return true;
}

/* The controller in @slot has left. Returns false if none was there.
 * A controller leaving in the middle of a restart ends it. */
static INLINE bool input_registry_disconnect(input_registry_t *reg,
      unsigned slot)
{
   if (!input_registry_leave_(reg, slot))
      return false;
   if (reg->restarting)
      input_registry_restart_cancel(reg);
   return true;
}

/* The driver is about to start over and report its controllers again.
 * Every controller present is taken as gone, to be recognised when it
 * is reported, and is expected back. @port_of_slot says which port
 * each driver slot is mapped to now (INPUT_REGISTRY_NO_PORT for none);
 * a controller already owed its port by an earlier restart that has
 * not finished keeps the port it had then. */
static INLINE void input_registry_restart(input_registry_t *reg,
      const uint8_t *port_of_slot, unsigned slots)
{
   unsigned i;

   reg->expected_left = 0;
   for (i = 0; i < INPUT_REGISTRY_RECORDS; i++)
   {
      input_device_record_t *rec = &reg->records[i];

      if (rec->present)
      {
         if (!rec->restore)
            rec->port  = (rec->slot < slots)
               ? port_of_slot[rec->slot] : INPUT_REGISTRY_NO_PORT;
         rec->present  = false;
         rec->restore  = false;
         rec->handle   = 0;
         rec->seen     = ++reg->clock;
         rec->expected = true;
      }
      if (rec->expected)
         reg->expected_left++;
   }
   reg->restarting = (reg->expected_left > 0);
}

/* Every controller that was there at the restart has been reported
 * again, and nothing else has come or gone. */
static INLINE bool input_registry_restart_complete(
      const input_registry_t *reg)
{
   return reg->restarting && reg->expected_left == 0;
}

/* Put every controller of a completed restart back on the port it had.
 * @index maps a port to the driver slot it reads, one entry per port,
 * and stays a one-to-one mapping: two entries are exchanged for each
 * controller that is not where it was. Returns how many were moved.
 * The restart is over afterwards. */
static INLINE unsigned input_registry_restore_ports(input_registry_t *reg,
      unsigned *index, unsigned ports)
{
   unsigned i, q;
   unsigned moved = 0;

   if (!input_registry_restart_complete(reg))
      return 0;

   for (i = 0; i < INPUT_REGISTRY_RECORDS; i++)
   {
      const input_device_record_t *rec = &reg->records[i];

      if (!rec->present || !rec->restore || rec->port >= ports)
         continue;

      for (q = 0; q < ports; q++)
         if (index[q] == rec->slot)
            break;
      if (q == ports || q == rec->port)
         continue;

      index[q]         = index[rec->port];
      index[rec->port] = rec->slot;
      moved++;
   }

   input_registry_restart_cancel(reg);
   return moved;
}

/* A driver reports a controller in @slot. Returns the handle for this
 * stay, 0 if the table is full of controllers that are present. If
 * @returned is not NULL it is set when the controller was here before.
 *
 * A controller already present in the slot ends its stay first: either
 * it is being reported again, by a driver that started over, and gets
 * its id back below, or something else has taken its place. */
static INLINE input_device_handle_t input_registry_connect(
      input_registry_t *reg, const char *provider, unsigned slot,
      const char *name, const char *phys, uint16_t vid, uint16_t pid,
      bool *returned)
{
   unsigned i;
   int      best       = -1;
   int      best_rank  = -1;
   int      free_slot  = -1;
   int      oldest     = -1;
   uint32_t key        = input_registry_key(provider, name, vid, pid);
   bool     have_phys  = phys && *phys;
   input_device_record_t *rec;

   if (returned)
      *returned = false;
   if (slot > 0xff)
      return 0;

   input_registry_leave_(reg, slot);

   for (i = 0; i < INPUT_REGISTRY_RECORDS; i++)
   {
      int rank;
      rec = &reg->records[i];

      if (rec->present)
         continue;
      if (!rec->id)
      {
         if (free_slot < 0)
            free_slot = (int)i;
         continue;
      }
      /* the record to give up if the table is full: left longest ago */
      if (oldest < 0 || rec->seen < reg->records[oldest].seen)
         oldest = (int)i;

      if (     rec->key != key
            || rec->vid != vid
            || rec->pid != pid
            || strncmp(rec->name, name ? name : "", sizeof(rec->name) - 1)
            || strncmp(rec->provider, provider ? provider : "",
                  sizeof(rec->provider) - 1))
         continue;

      /* same place, then same slot, then whoever left last; one that
       * was somewhere else, when both places are known, comes after
       * all of those - a pad moved to another socket is still the pad */
      if (have_phys && !strncmp(rec->phys, phys, sizeof(rec->phys) - 1))
         rank = 3;
      else if (have_phys && *rec->phys)
         rank = 0;
      else if (rec->slot == slot)
         rank = 2;
      else
         rank = 1;

      if (     rank > best_rank
            || (rank == best_rank && rec->seen > reg->records[best].seen))
      {
         best      = (int)i;
         best_rank = rank;
      }
   }

   if (best >= 0)
   {
      rec = &reg->records[best];
      if (returned)
         *returned = true;
   }
   else
   {
      if (free_slot < 0)
         free_slot = oldest;
      if (free_slot < 0)
         return 0;
      {
         /* a record given up keeps counting stays, so a handle from
          * its previous owner can never resolve to this one */
         uint32_t generation = reg->records[free_slot].generation;
         rec                 = &reg->records[free_slot];
         memset(rec, 0, sizeof(*rec));
         rec->generation     = generation;
      }
      rec->id   = ++reg->next_id;
      rec->key  = key;
      rec->vid  = vid;
      rec->pid  = pid;
      rec->port = INPUT_REGISTRY_NO_PORT;
      input_registry_copy(rec->provider, sizeof(rec->provider), provider);
      input_registry_copy(rec->name,     sizeof(rec->name),     name);
   }

   /* During a restart, a controller that was there before it is one
    * fewer to wait for. Anything else means this is not the same set
    * of controllers coming back. */
   if (reg->restarting)
   {
      if (rec->expected)
      {
         rec->expected = false;
         rec->restore  = true;
         reg->expected_left--;
      }
      else
         input_registry_restart_cancel(reg);
   }

   input_registry_copy(rec->phys, sizeof(rec->phys), phys);
   rec->slot       = (uint8_t)slot;
   rec->present    = true;
   rec->seen       = ++reg->clock;
   rec->generation++;
   rec->handle     = (rec->generation << 8)
                   | (uint32_t)((rec - reg->records) + 1);
   return rec->handle;
}

#endif
