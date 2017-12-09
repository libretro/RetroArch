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

<<<<<<< HEAD
#define POLL_THREAD_SLEEP 10000
<<<<<<< HEAD
=======

#define DEVICE_UNUSED 0
#define DEVICE_USED   1

typedef struct wiiu_hid_user wiiu_hid_user_t;

struct wiiu_hid_user
{
  wiiu_hid_user_t *next;
  uint8_t *send_control_buffer;
  uint8_t *send_control_type;

  uint32_t handle;
  uint32_t physical_device_inst;
  uint16_t vid;
  uint16_t pid;
  uint8_t interface_index;
  uint8_t sub_class;
  uint8_t protocol;
  uint16_t max_packet_size_rx;
  uint16_t max_packet_size_tx;
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

<<<<<<< HEAD
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
=======
#define DEVICE_UNUSED 0
#define DEVICE_USED   1
#define ALIGN_POINTER sizeof(void *)
>>>>>>> Simplify, add logging, revert some of the changes

typedef struct wiiu_hid
{
  HIDClient *client;
  OSThread *polling_thread;
  void *polling_thread_stack;
  volatile bool polling_thread_quit;
} wiiu_hid_t;

<<<<<<< HEAD
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
=======
void *alloc_zeroed(size_t alignment, size_t size);
static wiiu_hid_t *new_hid(void);
static void delete_hid(wiiu_hid_t *hid);
static void delete_hidclient(HIDClient *client);
>>>>>>> Simplify, add logging, revert some of the changes
static HIDClient *new_hidclient(void);

<<<<<<< HEAD
static int32_t wiiu_attach_callback(HIDClient *client, HIDDevice *device, uint32_t attach);
=======
//static int32_t wiiu_attach_callback(HIDClient *client, HIDDevice *device, uint32_t attach);
>>>>>>> Only call HIDSetup/HidTeardown once
static void start_polling_thread(wiiu_hid_t *hid);
static void stop_polling_thread(wiiu_hid_t *hid);
static int wiiu_hid_polling_thread(int argc, const char **argv);
static void wiiu_hid_do_poll(wiiu_hid_t *hid);
<<<<<<< HEAD

static void enqueue_device(void);
=======
>>>>>>> Start implementing HID polling thread

//HIDClient *new_hidclient(void);
//void delete_hidclient(HIDClient *client);
wiiu_hid_t *new_hid(void);
void delete_hid(wiiu_hid_t *hid);

/**
 * HID driver entrypoints registered with hid_driver_t
 */
=======
static void start_polling_thread(wiiu_hid_t *hid);
static void stop_polling_thread(wiiu_hid_t *hid);
static int wiiu_hid_polling_thread(int argc, const char **argv);
static int32_t wiiu_attach_callback(HIDClient *client, HIDDevice *device, uint32_t attach);
>>>>>>> Simplify, add logging, revert some of the changes

static bool wiiu_hid_joypad_query(void *data, unsigned pad)
{
   return pad < MAX_USERS;
}

static const char *wiiu_hid_joypad_name(void *data, unsigned pad)
{
   /* TODO/FIXME - implement properly */
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
<<<<<<< HEAD
  wiiu_hid_t *hid = new_hid();
//  HIDClient *client = new_hidclient();

//  if(!hid || !client)
  if(!hid)
    goto error;

  start_polling_thread(hid);
  if(hid->polling_thread == NULL)
    goto error;

//  HIDAddClient(client, wiiu_attach_callback);
//  hid->client = client;

  return hid;

  error:
    if(hid) {
      stop_polling_thread(hid);
      delete_hid(hid);
    }
//    if(client) {
//      delete_hidclient(client);
//    }

    return NULL;
=======
   HIDSetup();
   wiiu_hid_t *hid = new_hid();
   HIDClient *client = new_hidclient();
   if(!hid || !client) {
     goto error;
   }

   start_polling_thread(hid);
   if(!hid->polling_thread)
     goto error;

   HIDAddClient(client, wiiu_attach_callback);
   hid->client = client;

   return hid;

   error:
     RARCH_LOG("[hid]: initialization failed. cleaning up.\n");
     stop_polling_thread(hid);
     delete_hid(hid);
     delete_hidclient(client);
     return NULL;
>>>>>>> Simplify, add logging, revert some of the changes
}

static void wiiu_hid_free(void *data)
{
<<<<<<< HEAD
  wiiu_hid_t *hid = (wiiu_hid_t*)data;

  if (hid) {
    stop_polling_thread(hid);
<<<<<<< HEAD
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
=======
    delete_hid(hid);
>>>>>>> Only call HIDSetup/HidTeardown once
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
=======
   wiiu_hid_t *hid = (wiiu_hid_t*)data;

   if (hid) {
      stop_polling_thread(hid);
      delete_hidclient(hid->client);
      delete_hid(hid);
   }
   HIDTeardown();
}

>>>>>>> Simplify, add logging, revert some of the changes
static void wiiu_hid_poll(void *data)
{
   (void)data;
}

static void start_polling_thread(wiiu_hid_t *hid) {
  RARCH_LOG("[hid]: starting polling thread.\n");
  OSThreadAttributes attributes = OS_THREAD_ATTRIB_AFFINITY_CPU2 |
                                  OS_THREAD_ATTRIB_STACK_USAGE;
  BOOL result;

  int32_t stack_size = 0x8000;
  int32_t priority = 10;
  OSThread *thread = memalign(ALIGN_POINTER, sizeof(OSThread));
  void *stack = memalign(ALIGN_POINTER, stack_size);

<<<<<<< HEAD
<<<<<<< HEAD
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
=======
  if(!thread || !stack)
>>>>>>> Only call HIDSetup/HidTeardown once
    goto error;
=======
  if(!thread || !stack) {
    RARCH_LOG("[hid]: allocation failed, aborting thread start.\n");
    goto error;
  }

  RARCH_LOG("[hid]: thread = %x; stack = %x\n", thread, stack);
>>>>>>> Simplify, add logging, revert some of the changes

  if(!OSCreateThread(thread, wiiu_hid_polling_thread, 1, (char *)hid, stack, stack_size, priority, attributes)) {
    RARCH_LOG("[hid]: OSCreateThread failed.\n");
    goto error;
  }

  hid->polling_thread = thread;
  hid->polling_thread_stack = stack;
  return;

  error:
<<<<<<< HEAD
<<<<<<< HEAD
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
=======
>>>>>>> Only call HIDSetup/HidTeardown once
    if(stack)
      free(stack);
=======
>>>>>>> Simplify, add logging, revert some of the changes
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
<<<<<<< HEAD
<<<<<<< HEAD
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
=======
>>>>>>> Only call HIDSetup/HidTeardown once
=======
  hid->polling_thread = NULL;
  hid->polling_thread_stack = NULL;
>>>>>>> Simplify, add logging, revert some of the changes
}

void log_device(HIDDevice *device) {
  if(!device) {
    RARCH_LOG("NULL device.\n");
  }

<<<<<<< HEAD
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
=======
>>>>>>> Only call HIDSetup/HidTeardown once
/**
 * Callbacks
 */
/*
int32_t wiiu_attach_callback(HIDClient *client, HIDDevice *device, uint32_t attach) {
  int32_t result = DEVICE_UNUSED;

  switch(attach) {
    case HID_DEVICE_ATTACH:
      RARCH_LOG("Device attached\n");
      break;
    case HID_DEVICE_DETACH:
      RARCH_LOG("Device detached\n");
=======
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
>>>>>>> Simplify, add logging, revert some of the changes
      break;
    default:
      break;
  }
  log_device(device);

  return DEVICE_UNUSED;
}
<<<<<<< HEAD
*/
/**
 * Allocation
 */

wiiu_hid_t *new_hid(void) {
  wiiu_hid_t *hid = calloc(1, sizeof(wiiu_hid_t));
  if(hid)
    memset(hid, 0, sizeof(wiiu_hid_t));

  return hid;
=======

static int wiiu_hid_polling_thread(int argc, const char **argv) {
  wiiu_hid_t *hid = (wiiu_hid_t *)argv;
  while(!hid->polling_thread_quit) {
    usleep(10000);
  }

  return 0;
}

static wiiu_hid_t *new_hid(void) {
  return alloc_zeroed(2, sizeof(wiiu_hid_t));
>>>>>>> Simplify, add logging, revert some of the changes
}

static void delete_hid(wiiu_hid_t *hid) {
  if(hid)
    free(hid);
}
<<<<<<< HEAD
/*
HIDClient *new_hidclient(void) {
  HIDClient *client = memalign(32, sizeof(HIDClient));
  if(client)
    memset(client, 0, sizeof(HIDClient));

  return client;
}

void delete_hidclient(HIDClient *client) {
  if(client)
    free(client);
}
*/
=======

static HIDClient *new_hidclient(void) {
  return alloc_zeroed(32, sizeof(HIDClient));
}

static void delete_hidclient(HIDClient *client) {
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


>>>>>>> Simplify, add logging, revert some of the changes
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
   "wiiu_usb",
};
