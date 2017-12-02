/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <stdlib.h>
#include <string.h>

#include <wiiu/os.h>
#include <wiiu/syshid.h>

#include "../input_defines.h"
#include "../input_driver.h"

typedef struct wiiu_hid
{
   void *empty;
   uint32_t nsyshid;
   HIDClient *client;
   HIDAttachCallback attach_callback;
} wiiu_hid_t;

typedef struct wiiu_hid_user
{
  unsigned char *buffer;
  uint32_t nsishid;
  void *hid_read;
  void *hid_write;
  uint32_t transfersize;
} wiiu_hid_user_t;

static wiiu_hid_t *new_wiiu_hid_t(void);
static void delete_wiiu_hid_t(wiiu_hid_t *hid);
static HIDClient *new_hidclient(void);
static void delete_hidclient(HIDClient *hid);

int32_t wiiu_attach_callback(HIDClient *client, HIDDevice *device, uint32_t attach);

/**
 * HID driver entrypoints registered with hid_driver_t
 */

static bool wiiu_hid_joypad_query(void *data, unsigned pad)
{
   return pad < MAX_USERS;
}

static const char *wiiu_hid_joypad_name(void *data, unsigned pad)
{
   return NULL;
}

static uint64_t wiiu_hid_joypad_get_buttons(void *data, unsigned port)
{
   (void)data;
   (void)port;

   return 0;
}

static bool wiiu_hid_joypad_button(void *data, unsigned port, uint16_t joykey)
{
   (void)data;
   (void)port;
   (void)joykey;

   return false;
}

static bool wiiu_hid_joypad_rumble(void *data, unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   (void)data;
   (void)pad;
   (void)effect;
   (void)strength;

   return false;
}

static int16_t wiiu_hid_joypad_axis(void *data, unsigned port, uint32_t joyaxis)
{
   (void)data;
   (void)port;
   (void)joyaxis;

   return 0;
}

static void *wiiu_hid_init(void)
{
  wiiu_hid_t *hid = new_wiiu_hid_t();
  HIDClient *client = new_hidclient();

  if(!hid || !client)
    goto error;

  HIDAddClient(client, hid->attach_callback);
  hid->client = client;

  return hid;

  error:
    if(hid)
      delete_wiiu_hid_t(hid);
    if(client)
      free(client);

    return NULL;
}

static void wiiu_hid_free(void *data)
{
  wiiu_hid_t *hid = (wiiu_hid_t*)data;

  if (hid) {
    delete_wiiu_hid_t(hid);
  }
}

static void wiiu_hid_poll(void *data)
{
   (void)data;
}

/**
 * Callbacks
 */

int32_t wiiu_attach_callback(HIDClient *client, HIDDevice *device, uint32_t attach) {
  return 0;
}

static void wiiu_read_callback(uint32_t handle, int32_t errno, unsigned char *buffer, uint32_t transferred, void *usr) {
}

static void wiiu_write_callback(uint32_t handle, int32_t errno, unsigned char *buffer, uint32_t transferred, void *usr) {
}

/**
 * Allocation/deallocation
 */

static wiiu_hid_t *new_wiiu_hid_t(void) {
  wiiu_hid_t *hid = (wiiu_hid_t*)calloc(1, sizeof(wiiu_hid_t));

  if(!hid)
    goto error;

  memset(hid, 0, sizeof(wiiu_hid_t));
  hid->attach_callback = wiiu_attach_callback;

  return hid;

  error:
    if(hid)
      delete_wiiu_hid_t(hid);
    return NULL;
}

static void delete_wiiu_hid_t(wiiu_hid_t *hid) {
  if(!hid)
    return;

  if(hid->client) {
    HIDDelClient(hid->client);
    delete_hidclient(hid->client);
  }

  free(hid);
}

static HIDClient *new_hidclient() {
  HIDClient *client = calloc(1, sizeof(HIDClient));
  if(client != NULL) {
    memset(client, 0, sizeof(HIDClient));
  }

  return client;
}

static void delete_hidclient(HIDClient *client) {
  if(client)
    free(client);
}

hid_driver_t wiiu_hid = {
   wiiu_hid_init,
   wiiu_hid_joypad_query,
   wiiu_hid_free,
   wiiu_hid_joypad_button,
   wiiu_hid_joypad_get_buttons,
   wiiu_hid_joypad_axis,
   wiiu_hid_poll,
   wiiu_hid_joypad_rumble,
   wiiu_hid_joypad_name,
   "wiiu",
};
