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

/* USB HID pads on the Wii, through os/gekko's /dev/usb/hid.
 *
 * A worker thread follows the device list: a pad the joypad connection
 * layer knows gets a slot and a reader thread of its own, which blocks
 * on the interrupt IN endpoint and hands each report to the pad's
 * driver.  Control messages queued from other threads are sent by the
 * worker, one at a time per pad; reports the pad drivers set or get
 * from their own packet handlers go out directly. */

#include <stdlib.h>
#include <string.h>

#include <gekko/thread.h>
#include <gekko/usb.h>

#include <retro_atomic.h>

#include "../include/hid_driver.h"
#include "../input_defines.h"
#include "../input_driver.h"

#include "../connect/joypad_connection.h"

#include "../../tasks/tasks_internal.h"
#include "../../verbosity.h"

/* send_control's first byte. */
#define SC_NONE     0
#define SC_INTMSG   1
#define SC_CTRLMSG  2
#define SC_CTRLMSG2 3

#define CTL_MAX     128
#define REPORT_MAX  64
#define SCAN_US     250000

typedef struct gekko_hid gekko_hid_t;

typedef struct gekko_hid_pad
{
   gekko_hid_t       *hid;
   gk_thread_t       *reader;
   uint32_t           id;
   int32_t            slot;
   uint16_t           in_size;
   uint16_t           vid;
   uint16_t           pid;
   uint8_t            ep_in;
   uint8_t            ep_out;
   uint8_t            iface;
   volatile uint32_t  done;      /* the reader has stopped */
   volatile uint32_t  ctl_type;  /* a queued control message, or SC_NONE */
   uint32_t           ctl_len;
   uint8_t            ctl[CTL_MAX];
   uint8_t            report[REPORT_MAX];
} gekko_hid_pad_t;

struct gekko_hid
{
   joypad_connection_t *connections;
   gekko_hid_pad_t     *pads[MAX_USERS];
   gk_thread_t         *worker;
   retro_atomic_int_t   quit;
   volatile uint32_t    wake;
   uint32_t             ignored[GK_USB_MAX_DEVICES];
   int                  num_ignored;
};

hid_driver_t gekko_hid;

static void hid_wake(gekko_hid_t *hid)
{
   hid->wake++;
   gk_futex_wake(&hid->wake, 1);
}

/* ---- the reader ---- */

static void *reader(void *arg)
{
   gekko_hid_pad_t *pad = (gekko_hid_pad_t*)arg;
   gekko_hid_t     *hid = pad->hid;
   while (!retro_atomic_load_acquire_int(&hid->quit))
   {
      int n = gk_usb_intr(pad->id, pad->ep_in, pad->report, pad->in_size);
      if (n < 0)
         break;
      if (n > 0)
         pad_connection_packet(&hid->connections[pad->slot], pad->slot,
               pad->report, (uint32_t)n);
   }
   pad->done = 1;
   hid_wake(hid);
   return NULL;
}

/* ---- reports, for the pad drivers ---- */

/* A report ID the caller left at the front goes in wValue instead. */
static int32_t gekko_hid_set_report(void *handle, uint8_t type, uint8_t id,
      uint8_t *data, size_t len)
{
   gekko_hid_pad_t *pad = (gekko_hid_pad_t*)handle;
   if (!pad || len > 0xffff)
      return -1;
   if (len && data[0] == id)
   {
      data++;
      len--;
   }
   return gk_usb_ctrl(pad->id, 0x21, 0x09, (uint16_t)((type << 8) | id),
         pad->iface, data, (uint16_t)len);
}

static int32_t gekko_hid_get_report(void *handle, uint8_t type, uint8_t id,
      uint8_t *data, size_t len)
{
   gekko_hid_pad_t *pad = (gekko_hid_pad_t*)handle;
   if (!pad || len > 0xffff)
      return -1;
   return gk_usb_ctrl(pad->id, 0xa1, 0x01, (uint16_t)((type << 8) | id),
         pad->iface, data, (uint16_t)len);
}

static int32_t gekko_hid_set_idle(void *handle, uint8_t amount)
{
   gekko_hid_pad_t *pad = (gekko_hid_pad_t*)handle;
   return pad ? gk_usb_ctrl(pad->id, 0x21, 0x0a, (uint16_t)(amount << 8),
         pad->iface, NULL, 0) : -1;
}

static int32_t gekko_hid_set_protocol(void *handle, uint8_t protocol)
{
   gekko_hid_pad_t *pad = (gekko_hid_pad_t*)handle;
   return pad ? gk_usb_ctrl(pad->id, 0x21, 0x0b, protocol, pad->iface,
         NULL, 0) : -1;
}

static int32_t gekko_hid_read(void *handle, void *s, size_t len)
{
   gekko_hid_pad_t *pad = (gekko_hid_pad_t*)handle;
   return pad ? gk_usb_intr(pad->id, pad->ep_in, s, (uint32_t)len) : -1;
}

/* From any thread: the worker sends it.  One at a time; a message
 * arriving while one waits is dropped, and for LEDs and rumble the
 * next one wins. */
static void gekko_hid_send_control(void *handle, uint8_t *s, size_t len)
{
   gekko_hid_pad_t *pad = (gekko_hid_pad_t*)handle;
   if (!pad || !s || len < 2 || len - 1 > CTL_MAX || pad->ctl_type != SC_NONE)
      return;
   memcpy(pad->ctl, s + 1, len - 1);
   pad->ctl_len  = (uint32_t)(len - 1);
   pad->ctl_type = s[0];
   hid_wake(pad->hid);
}

static void send_queued(gekko_hid_pad_t *pad)
{
   int ret = 0;
   switch (pad->ctl_type)
   {
      case SC_INTMSG:
         ret = pad->ep_out ? gk_usb_intr(pad->id, pad->ep_out, pad->ctl,
               pad->ctl_len) : -1;
         break;
      case SC_CTRLMSG:
         ret = gk_usb_ctrl(pad->id, 0x21, 0x09, 0x03f4, pad->iface, pad->ctl,
               (uint16_t)pad->ctl_len);
         break;
      case SC_CTRLMSG2:
         ret = gk_usb_ctrl(pad->id, 0x21, 0x09, 0x0201, pad->iface, pad->ctl,
               (uint16_t)pad->ctl_len);
         break;
      default:
         break;
   }
   if (ret < 0)
      RARCH_WARN("[GekkoHID] Control message to slot %d dropped.\n",
            (int)pad->slot);
   pad->ctl_type = SC_NONE;
}

/* ---- pads coming and going ---- */

static void pad_remove(gekko_hid_t *hid, gekko_hid_pad_t *pad)
{
   unsigned tries;
   /* The reader may be waiting on the pad: cancel until it stops. */
   for (tries = 0; !pad->done && tries < 50; tries++)
   {
      gk_usb_cancel(pad->id, pad->ep_in);
      gk_sleep_us(10000);
   }
   if (pad->done)
      gk_thread_join(pad->reader);
   else
      gk_thread_detach(pad->reader);
   input_autoconfigure_disconnect(pad->slot,
         pad_connection_get_name(&hid->connections[pad->slot], pad->slot));
   pad_connection_pad_deinit(&hid->connections[pad->slot], pad->slot);
   gk_usb_close(pad->id);
   hid->pads[pad->slot] = NULL;
   if (pad->done)
      free(pad);
}

static int known(gekko_hid_t *hid, uint32_t id)
{
   int i;
   for (i = 0; i < MAX_USERS; i++)
      if (hid->pads[i] && hid->pads[i]->id == id)
         return 1;
   for (i = 0; i < hid->num_ignored; i++)
      if (hid->ignored[i] == id)
         return 1;
   return 0;
}

static void ignore(gekko_hid_t *hid, uint32_t id)
{
   if (hid->num_ignored < GK_USB_MAX_DEVICES)
      hid->ignored[hid->num_ignored++] = id;
}

static void pad_add(gekko_hid_t *hid, const gk_usb_dev_t *dev)
{
   gk_usb_info_t    info;
   gekko_hid_pad_t *pad;
   int              i;
   int32_t          slot;

   if (gk_usb_open(dev->id, &info))
   {
      ignore(hid, dev->id);
      return;
   }
   if (!(pad = (gekko_hid_pad_t*)calloc(1, sizeof(*pad))))
   {
      gk_usb_close(dev->id);
      return;
   }
   pad->hid   = hid;
   pad->id    = dev->id;
   pad->vid   = info.vid;
   pad->pid   = info.pid;
   pad->iface = info.if_number;
   for (i = 0; i < info.num_ep; i++)
   {
      const gk_usb_ep_t *ep = &info.ep[i];
      if ((ep->attributes & 3) != 3)
         continue;
      if ((ep->address & 0x80) && !pad->ep_in)
      {
         pad->ep_in   = ep->address;
         pad->in_size = ep->max_packet > REPORT_MAX ? REPORT_MAX
            : ep->max_packet;
      }
      else if (!(ep->address & 0x80) && !pad->ep_out)
         pad->ep_out = ep->address;
   }

   slot = pad->ep_in ? pad_connection_pad_init(hid->connections, "hid",
         info.vid, info.pid, pad, &gekko_hid) : -1;
   if (slot < 0 || !pad_connection_has_interface(hid->connections, slot))
   {
      if (slot >= 0)
         pad_connection_pad_deinit(&hid->connections[slot], slot);
      RARCH_LOG("[GekkoHID] %04x:%04x is not a pad RetroArch knows.\n",
            info.vid, info.pid);
      /* A keyboard or mouse stays awake for its reader */
      if (!(info.if_class == 3 && info.if_subclass == 1
               && (info.if_protocol == 1 || info.if_protocol == 2)))
         gk_usb_close(dev->id);
      free(pad);
      ignore(hid, dev->id);
      return;
   }
   pad->slot = slot;
   if (!(pad->reader = gk_thread_create(reader, pad, NULL, 16384,
               GK_PRIO_DEFAULT + 4)))
   {
      pad_connection_pad_deinit(&hid->connections[slot], slot);
      gk_usb_close(dev->id);
      free(pad);
      return;
   }
   hid->pads[slot] = pad;
   RARCH_LOG("[GekkoHID] \"%s\" (%04x:%04x) on slot %d.\n",
         pad_connection_get_name(&hid->connections[slot], slot),
         info.vid, info.pid, (int)slot);
   input_autoconfigure_connect(
         pad_connection_get_name(&hid->connections[slot], slot), NULL, NULL,
         "hid", slot, info.vid, info.pid);
}

static void scan(gekko_hid_t *hid)
{
   gk_usb_dev_t devs[GK_USB_MAX_DEVICES];
   int n = gk_usb_hid_list(devs, GK_USB_MAX_DEVICES), i, k;

   for (i = 0; i < MAX_USERS; i++)
   {
      gekko_hid_pad_t *pad = hid->pads[i];
      int present = 0;
      if (!pad)
         continue;
      for (k = 0; k < n; k++)
         if (devs[k].id == pad->id)
            present = 1;
      if (!present || pad->done)
         pad_remove(hid, pad);
   }
   /* Forget ignored devices that left, so they are looked at again. */
   for (i = 0; i < hid->num_ignored; )
   {
      int present = 0;
      for (k = 0; k < n; k++)
         if (devs[k].id == hid->ignored[i])
            present = 1;
      if (present)
         i++;
      else
         hid->ignored[i] = hid->ignored[--hid->num_ignored];
   }
   for (k = 0; k < n; k++)
      if (!known(hid, devs[k].id))
         pad_add(hid, &devs[k]);
}

static void *worker(void *arg)
{
   gekko_hid_t *hid = (gekko_hid_t*)arg;
   while (!retro_atomic_load_acquire_int(&hid->quit))
   {
      uint32_t seq = hid->wake;
      int i;
      scan(hid);
      for (i = 0; i < MAX_USERS; i++)
         if (hid->pads[i] && hid->pads[i]->ctl_type != SC_NONE)
            send_queued(hid->pads[i]);
      if (hid->wake == seq)
         gk_futex_wait(&hid->wake, seq, GK_US_TO_TICKS(SCAN_US));
   }
   return NULL;
}

/* ---- the driver ---- */

static void gekko_hid_free(const void *data)
{
   gekko_hid_t *hid = (gekko_hid_t*)data;
   int i;
   if (!hid)
      return;
   retro_atomic_store_release_int(&hid->quit, 1);
   if (hid->worker)
   {
      hid_wake(hid);
      gk_thread_join(hid->worker);
   }
   for (i = 0; i < MAX_USERS; i++)
      if (hid->pads[i])
         pad_remove(hid, hid->pads[i]);
   pad_connection_destroy(hid->connections);
   free(hid);
}

static void *gekko_hid_init(void)
{
   gekko_hid_t *hid = (gekko_hid_t*)calloc(1, sizeof(*hid));
   if (!hid)
      return NULL;
   retro_atomic_int_init(&hid->quit, 0);
   if (     !(hid->connections = pad_connection_init(MAX_USERS))
         || !(hid->worker = gk_thread_create(worker, hid, NULL, 16384,
               GK_PRIO_DEFAULT + 2)))
   {
      gekko_hid_free(hid);
      return NULL;
   }
   return hid;
}

static bool gekko_hid_joypad_query(void *data, unsigned pad)
{
   return pad < MAX_USERS;
}

static const char *gekko_hid_joypad_name(void *data, unsigned pad)
{
   gekko_hid_t *hid = (gekko_hid_t*)data;
   if (!hid || pad >= MAX_USERS)
      return NULL;
   return pad_connection_get_name(&hid->connections[pad], pad);
}

static void gekko_hid_joypad_get_buttons(void *data, unsigned port,
      input_bits_t *state)
{
   gekko_hid_t *hid = (gekko_hid_t*)data;
   if (hid && port < MAX_USERS)
      pad_connection_get_buttons(&hid->connections[port], port, state);
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t gekko_hid_joypad_button(void *data, unsigned port,
      uint16_t joykey)
{
   input_bits_t buttons;
   if (port >= DEFAULT_MAX_PADS || GET_HAT_DIR(joykey) || joykey >= 32)
      return 0;
   gekko_hid_joypad_get_buttons(data, port, &buttons);
   return BIT256_GET(buttons, joykey) != 0;
}

static int16_t gekko_hid_joypad_axis(void *data, unsigned port,
      uint32_t joyaxis)
{
   gekko_hid_t *hid = (gekko_hid_t*)data;
   if (!hid || port >= MAX_USERS)
      return 0;
   if (AXIS_NEG_GET(joyaxis) < 4)
   {
      int16_t val = pad_connection_get_axis(&hid->connections[port], port,
            AXIS_NEG_GET(joyaxis));
      if (val < 0)
         return val;
   }
   else if (AXIS_POS_GET(joyaxis) < 4)
   {
      int16_t val = pad_connection_get_axis(&hid->connections[port], port,
            AXIS_POS_GET(joyaxis));
      if (val > 0)
         return val;
   }
   return 0;
}

static int16_t gekko_hid_joypad_state(void *data,
      rarch_joypad_info_t *joypad_info, const void *binds_data,
      unsigned port)
{
   unsigned i;
   int16_t ret                       = 0;
   const struct retro_keybind *binds = (const struct retro_keybind*)binds_data;
   uint16_t port_idx                 = joypad_info->joy_idx;

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[i].joykey != NO_BTN)
         ? binds[i].joykey  : joypad_info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis != AXIS_NONE)
         ? binds[i].joyaxis : joypad_info->auto_binds[i].joyaxis;
      if (     (uint16_t)joykey != NO_BTN
            && gekko_hid_joypad_button(data, port_idx, (uint16_t)joykey))
         ret |= (1 << i);
      else if (joyaxis != AXIS_NONE &&
            ((float)abs(gekko_hid_joypad_axis(data, port_idx, joyaxis))
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }
   return ret;
}

static bool gekko_hid_joypad_rumble(void *data, unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   gekko_hid_t *hid = (gekko_hid_t*)data;
   if (!hid || pad >= MAX_USERS)
      return false;
   return pad_connection_rumble(&hid->connections[pad], pad, effect, strength);
}

static void gekko_hid_poll(void *data)
{
   (void)data;
}

hid_driver_t gekko_hid = {
   gekko_hid_init,
   gekko_hid_joypad_query,
   gekko_hid_free,
   gekko_hid_joypad_button,
   gekko_hid_joypad_state,
   gekko_hid_joypad_get_buttons,
   gekko_hid_joypad_axis,
   gekko_hid_poll,
   gekko_hid_joypad_rumble,
   gekko_hid_joypad_name,
   "wiiusb",
   gekko_hid_send_control,
   gekko_hid_set_report,
   gekko_hid_get_report,
   gekko_hid_set_idle,
   gekko_hid_set_protocol,
   gekko_hid_read
};
