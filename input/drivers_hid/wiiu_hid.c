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

#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <wiiu/os.h>
#include <wiiu/syshid.h>

#include "../input_defines.h"
#include "../input_driver.h"
#include "../../verbosity.h"

#define DEVICE_UNUSED 0
#define DEVICE_USED   1

typedef struct wiiu_hid
{
  HIDClient *client;
  OSThread *polling_thread;
  void *polling_thread_stack;
  volatile bool polling_thread_quit;
} wiiu_hid_t;

void *alloc_zeroed(size_t alignment, size_t size);
static OSThread *new_thread(void);
static wiiu_hid_t *new_hid(void);
static void delete_hid(wiiu_hid_t *hid);
static void delete_hidclient(HIDClient *client);
static HIDClient *new_hidclient(void);

static void start_polling_thread(wiiu_hid_t *hid);
static void stop_polling_thread(wiiu_hid_t *hid);
static int wiiu_hid_polling_thread(int argc, const char **argv);
static int32_t wiiu_attach_callback(HIDClient *client, HIDDevice *device, uint32_t attach);

static bool wiiu_hid_joypad_query(void *data, unsigned pad)
{
   return pad < MAX_USERS;
}

static const char *wiiu_hid_joypad_name(void *data, unsigned pad)
{
   if (pad >= MAX_USERS)
      return NULL;

   return NULL;
}

static void wiiu_hid_joypad_get_buttons(void *data, unsigned port, retro_bits_t *state)
{
   (void)data;
   (void)port;

   BIT256_CLEAR_ALL_PTR(state);
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
   RARCH_LOG("[hid]: wiiu_hid: init\n");
//   HIDSetup();
   wiiu_hid_t *hid = new_hid();
   HIDClient *client = new_hidclient();
   if(!hid || !client) {
     goto error;
   }

   RARCH_LOG("[hid]: hid: 0x%x\n", hid);
   RARCH_LOG("[hid]: client: 0x%x\n", client);

   start_polling_thread(hid);
   if(!hid->polling_thread)
     goto error;

   RARCH_LOG("[hid]: Registering HIDClient\n");
   HIDAddClient(client, wiiu_attach_callback);
   hid->client = client;

   RARCH_LOG("[hid]: init success");
   return hid;

   error:
     RARCH_LOG("[hid]: initialization failed. cleaning up.\n");
     stop_polling_thread(hid);
     delete_hid(hid);
     delete_hidclient(client);
     return NULL;
}

static void wiiu_hid_free(void *data)
{
   wiiu_hid_t *hid = (wiiu_hid_t*)data;

   if (hid) {
      stop_polling_thread(hid);
      delete_hidclient(hid->client);
      delete_hid(hid);
   }
   //HIDTeardown();
}

static void wiiu_hid_poll(void *data)
{
   (void)data;
}

static void start_polling_thread(wiiu_hid_t *hid) {
  RARCH_LOG("[hid]: starting polling thread.\n");
  OSThreadAttributes attributes = OS_THREAD_ATTRIB_AFFINITY_CPU2;
  BOOL result;

  int32_t stack_size = 0x8000;
  int32_t priority = 10;
  OSThread *thread = new_thread();
  void *stack = alloc_zeroed(16, stack_size);

  if(!thread || !stack) {
    RARCH_LOG("[hid]: allocation failed, aborting thread start.\n");
    goto error;
  }

  RARCH_LOG("[hid]: thread: 0x%x; stack: 0x%x\n", thread, stack);

  if(!OSCreateThread(thread,
                     wiiu_hid_polling_thread,
                     1, (char *)hid,
                     stack+stack_size, stack_size,
                     priority,
                     attributes)) {
    RARCH_LOG("[hid]: OSCreateThread failed.\n");
    goto error;
  }

  hid->polling_thread = thread;
  hid->polling_thread_stack = stack;
  OSResumeThread(thread);
  return;

  error:
    if(thread)
      free(thread);
    if(stack)
      free(stack);

    return;
}


static void stop_polling_thread(wiiu_hid_t *hid) {
  int thread_result = 0;
  RARCH_LOG("[hid]: stopping polling thread.\n");

  if(!hid || !hid->polling_thread)
    return;

  hid->polling_thread_quit = true;
  OSJoinThread(hid->polling_thread, &thread_result);
  free(hid->polling_thread);
  free(hid->polling_thread_stack);
  hid->polling_thread = NULL;
  hid->polling_thread_stack = NULL;
}

void log_device(HIDDevice *device) {
  if(!device) {
    RARCH_LOG("NULL device.\n");
  }

  RARCH_LOG("                handle: %d\n", device->handle);
  RARCH_LOG("  physical_device_inst: %d\n", device->physical_device_inst);
  RARCH_LOG("                   vid: 0x%x\n", device->vid);
  RARCH_LOG("                   pid: 0x%x\n", device->pid);
  RARCH_LOG("       interface_index: %d\n", device->interface_index);
  RARCH_LOG("             sub_class: %d\n", device->sub_class);
  RARCH_LOG("              protocol: %d\n", device->protocol);
  RARCH_LOG("    max_packet_size_rx: %d\n", device->max_packet_size_rx);
  RARCH_LOG("    max_packet_size_tx: %d\n", device->max_packet_size_tx);
}

static int32_t wiiu_attach_callback(HIDClient *client, HIDDevice *device, uint32_t attach) {
  switch(attach) {
    case HID_DEVICE_ATTACH:
      RARCH_LOG("USB device attach event\n");
      break;
    case HID_DEVICE_DETACH:
      RARCH_LOG("USB device detach event\n");
      break;
    default:
      break;
  }
  log_device(device);

  return DEVICE_UNUSED;
}

static int wiiu_hid_polling_thread(int argc, const char **argv) {
  wiiu_hid_t *hid = (wiiu_hid_t *)argv;
  int i = 0;
  RARCH_LOG("[hid]: polling thread is starting\n");
  while(!hid->polling_thread_quit) {
    usleep(10000);
    i += 10000;
    if(i >= (1000 * 1000 * 3)) {
      RARCH_LOG("[hid]: thread: TICK!\n");
      i = 0;
    }
  }

  RARCH_LOG("[hid]: polling thread is stopping\n");
  return 0;
}

static OSThread *new_thread(void) {
  OSThread *t = alloc_zeroed(8, sizeof(OSThread));
  t->tag = OS_THREAD_TAG;

  return t;
}

static wiiu_hid_t *new_hid(void) {
  RARCH_LOG("[hid]: new_hid()\n");
  return alloc_zeroed(4, sizeof(wiiu_hid_t));
}

static void delete_hid(wiiu_hid_t *hid) {
  RARCH_LOG("[hid]: delete_hid()\n");
  if(hid)
    free(hid);
}

static HIDClient *new_hidclient(void) {
  RARCH_LOG("[hid]: new_hidclient()\n");
  return alloc_zeroed(32, sizeof(HIDClient));
}

static void delete_hidclient(HIDClient *client) {
  RARCH_LOG("[hid]: delete_hidclient()\n");
  if(client)
    free(client);
}

void *alloc_zeroed(size_t alignment, size_t size) {
  void *result = memalign(alignment, size);
  if(result) {
    memset(result, 0, size);
  }

  return result;
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
