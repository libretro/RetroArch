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
#include <malloc.h>
#include <unistd.h>

#include <wiiu/os.h>
#include <wiiu/syshid.h>

#include "../input_defines.h"
#include "../input_driver.h"

#define POLL_THREAD_SLEEP 10000

#define DEVICE_UNUSED 0
#define DEVICE_USED   1

typedef struct wiiu_hid
{
   HIDClient *client;
   OSThread *polling_thread;
   // memory accounting; keep a pointer to the stack buffer so we can clean up later.
   void *polling_thread_stack;
   volatile bool polling_thread_quit;
} wiiu_hid_t;

typedef struct wiiu_hid_user
{
  uint8_t *buffer;
  uint32_t transfersize;
  uint32_t handle;
} wiiu_hid_user_t;

static wiiu_hid_t *new_wiiu_hid_t(void);
static void delete_wiiu_hid_t(wiiu_hid_t *hid);
static HIDClient *new_hidclient(void);
static void delete_hidclient(HIDClient *hid);

static int32_t wiiu_attach_callback(HIDClient *client, HIDDevice *device, uint32_t attach);
static void start_polling_thread(wiiu_hid_t *hid);
static void stop_polling_thread(wiiu_hid_t *hid);
static int wiiu_hid_polling_thread(int argc, const char **argv);
static void wiiu_hid_do_poll(wiiu_hid_t *hid);

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

  start_polling_thread(hid);
  if(hid->polling_thread == NULL)
    goto error;

  HIDAddClient(client, wiiu_attach_callback);
  hid->client = client;

  return hid;

  error:
    if(hid) {
      stop_polling_thread(hid);
      delete_wiiu_hid_t(hid);
    }
    if(client)
      free(client);

    return NULL;
}

static void wiiu_hid_free(void *data)
{
  wiiu_hid_t *hid = (wiiu_hid_t*)data;

  if (hid) {
    stop_polling_thread(hid);
    delete_wiiu_hid_t(hid);
  }
}

/**
 * This is a no-op because polling is done with a worker thread.
 */
static void wiiu_hid_poll(void *data)
{
   (void)data;
}

/**
 * Implementation functions
 */

static void start_polling_thread(wiiu_hid_t *hid) {
  OSThreadAttributes attributes = OS_THREAD_ATTRIB_AFFINITY_CPU2 |
                                  OS_THREAD_ATTRIB_DETACHED |
                                  OS_THREAD_ATTRIB_STACK_USAGE;
  int32_t stack_size = 0x8000;
  // wild-ass guess. the patcher thread used 28 for the network threads (10 for BOTW).
  int32_t priority = 19;
  OSThread *thread = memalign(8, sizeof(OSThread));
  void *stack = memalign(32, stack_size);

  if(!thread || !stack)
    goto error;

  if(!OSCreateThread(thread, wiiu_hid_polling_thread, 1, (char *)hid, stack, stack_size, priority, attributes))
    goto error;

  hid->polling_thread = thread;
  hid->polling_thread_stack = stack;
  return;

  error:
    if(stack)
      free(stack);
    if(thread)
      free(thread);

    return;
}

static void stop_polling_thread(wiiu_hid_t *hid) {
  int thread_result = 0;

  if(hid == NULL || hid->polling_thread == NULL)
    return;

  hid->polling_thread_quit = true;
  OSJoinThread(hid->polling_thread, &thread_result);

  free(hid->polling_thread);
  free(hid->polling_thread_stack);
}

/**
 * Entrypoint for the polling thread.
 */
static int wiiu_hid_polling_thread(int argc, const char **argv) {
  wiiu_hid_t *hid = (wiiu_hid_t *)argv;
  while(!hid->polling_thread_quit) {
    wiiu_hid_do_poll(hid);
  }

  return 0;
}

/**
 * Only call this from the polling thread.
 */
static void wiiu_hid_do_poll(wiiu_hid_t *hid) {
  usleep(POLL_THREAD_SLEEP);
}

/**
 * Callbacks
 */

int32_t wiiu_attach_callback(HIDClient *client, HIDDevice *device, uint32_t attach) {
  int32_t result = DEVICE_UNUSED;

  switch(attach) {
    case HID_DEVICE_ATTACH:
      // TODO: new device attached! Register it.
      break;
    case HID_DEVICE_DETACH:
      // TODO: device detached! Unregister it.
      break;
    default:
      // Undefined behavior, bail out
      break;
  }

  return result;
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
    hid->client = NULL;
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

static wiiu_hid_user_t *new_wiiu_hid_user_t(void) {
  wiiu_hid_user_t *user = calloc(1, sizeof(wiiu_hid_user_t));
  if(user != NULL) {
    memset(user, 0, sizeof(wiiu_hid_user_t));
  }

  return user;
}

static void delete_wiiu_hid_user_t(wiiu_hid_user_t *user) {
  if(user) {
    free(user);
  }
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
