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
<<<<<<< HEAD
=======

#define DEVICE_UNUSED 0
#define DEVICE_USED   1

typedef struct wiiu_hid_user wiiu_hid_user_t;

struct wiiu_hid_user
{
  wiiu_hid_user_t *next;
  uint8_t *buffer;
  uint32_t transfersize;
  uint32_t handle;
};

typedef struct wiiu_hid
{
   HIDClient *client;
   OSThread *polling_thread;
   // memory accounting; keep a pointer to the stack buffer so we can clean up later.
   void *polling_thread_stack;
   // setting this to true tells the polling thread to quit
   volatile bool polling_thread_quit;
} wiiu_hid_t;
>>>>>>> Start implementing HID polling thread

#define DEVICE_UNUSED 0
#define DEVICE_USED   1

typedef struct wiiu_hid_user wiiu_hid_user_t;

<<<<<<< HEAD
struct wiiu_hid_user
{
  wiiu_hid_user_t *next;
  uint8_t *buffer;
  uint32_t transfersize;
  uint32_t handle;
};

typedef struct wiiu_hid
{
   HIDClient *client;
   OSThread *polling_thread;
   // memory accounting; keep a pointer to the stack buffer so we can clean up later.
   void *polling_thread_stack;
   // setting this to true tells the polling thread to quit
   volatile bool polling_thread_quit;
} wiiu_hid_t;

=======
>>>>>>> More progress on the HID driver
/*
 * The attach/detach callback has no access to the wiiu_hid_t object. Therefore, we need a
 * global place to handle device data.
 */
static wiiu_hid_user_t *pad_list = NULL;
static OSFastMutex *pad_list_mutex;

static wiiu_hid_t *new_wiiu_hid_t(void);
static void delete_wiiu_hid_t(wiiu_hid_t *hid);
static wiiu_hid_user_t *new_wiiu_hid_user_t(void);
static void delete_wiiu_hid_user_t(wiiu_hid_user_t *user);
static HIDClient *new_hidclient(void);
static void delete_hidclient(HIDClient *hid);
static OSFastMutex *new_fastmutex(const char *name);
static void delete_fastmutex(OSFastMutex *mutex);

static int32_t wiiu_attach_callback(HIDClient *client, HIDDevice *device, uint32_t attach);
static void start_polling_thread(wiiu_hid_t *hid);
static void stop_polling_thread(wiiu_hid_t *hid);
static int wiiu_hid_polling_thread(int argc, const char **argv);
static void wiiu_hid_do_poll(wiiu_hid_t *hid);
<<<<<<< HEAD

static void enqueue_device(void);
=======
>>>>>>> Start implementing HID polling thread

static void enqueue_device(void);

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
      delete_hidclient(client);
    if(pad_list_mutex) {
      delete_fastmutex(pad_list_mutex);
      pad_list_mutex = NULL;
    }

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

<<<<<<< HEAD
<<<<<<< HEAD
=======
>>>>>>> More progress on the HID driver
static void free_pad_list(void) {
  wiiu_hid_user_t *top;

  while(pad_list != NULL) {
    top = pad_list;
    pad_list = top->next;
    delete_wiiu_hid_user_t(top);
  }
}

<<<<<<< HEAD
=======
>>>>>>> Start implementing HID polling thread
=======
>>>>>>> More progress on the HID driver
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

<<<<<<< HEAD
<<<<<<< HEAD
=======
>>>>>>> More progress on the HID driver
  if(pad_list_mutex == NULL) {
    pad_list_mutex = new_fastmutex("pad_list");
  }

  if(!thread || !stack || !pad_list_mutex)
<<<<<<< HEAD
=======
  if(!thread || !stack)
>>>>>>> Start implementing HID polling thread
=======
>>>>>>> More progress on the HID driver
    goto error;

  if(!OSCreateThread(thread, wiiu_hid_polling_thread, 1, (char *)hid, stack, stack_size, priority, attributes))
    goto error;

  hid->polling_thread = thread;
  hid->polling_thread_stack = stack;
  return;

  error:
<<<<<<< HEAD
<<<<<<< HEAD
    if(pad_list_mutex)
      delete_fastmutex(pad_list_mutex);
=======
>>>>>>> Start implementing HID polling thread
=======
    if(pad_list_mutex)
      delete_fastmutex(pad_list_mutex);
>>>>>>> More progress on the HID driver
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
<<<<<<< HEAD
<<<<<<< HEAD
=======
>>>>>>> More progress on the HID driver

  // with the thread stopped, we don't need the mutex.
  delete_fastmutex(pad_list_mutex);
  pad_list_mutex = NULL;
  free_pad_list();
<<<<<<< HEAD
=======
>>>>>>> Start implementing HID polling thread
=======
>>>>>>> More progress on the HID driver
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

<<<<<<< HEAD
<<<<<<< HEAD
=======
>>>>>>> More progress on the HID driver
int32_t wiiu_attach_device(HIDClient *client, HIDDevice *device) {
  wiiu_hid_user_t *adapter = new_wiiu_hid_user_t();

  if(!adapter)
    goto error;

  error:
    if(adapter) {
      delete_wiiu_hid_user_t(adapter);
    }
    return DEVICE_UNUSED;
}

int32_t wiiu_detach_device(HIDClient *client, HIDDevice *device) {
  return DEVICE_UNUSED;
}

<<<<<<< HEAD
=======
>>>>>>> Start implementing HID polling thread
=======
>>>>>>> More progress on the HID driver
/**
 * Callbacks
 */

int32_t wiiu_attach_callback(HIDClient *client, HIDDevice *device, uint32_t attach) {
  int32_t result = DEVICE_UNUSED;

  switch(attach) {
    case HID_DEVICE_ATTACH:
      result = wiiu_attach_device(client, device);
      break;
    case HID_DEVICE_DETACH:
      result = wiiu_detach_device(client, device);
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

static HIDClient *new_hidclient(void) {
  HIDClient *client = calloc(1, sizeof(HIDClient));
  if(client != NULL) {
    memset(client, 0, sizeof(HIDClient));
  }

  return client;
}

static OSFastMutex *new_fastmutex(const char *name) {
  OSFastMutex *mutex = calloc(1, sizeof(OSFastMutex));
  if(mutex != NULL) {
    memset(mutex, 0, sizeof(OSFastMutex));
  }

  OSFastMutex_Init(mutex, name);

  return mutex;
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

static void delete_fastmutex(OSFastMutex *mutex) {
  if(mutex)
    free(mutex);
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
