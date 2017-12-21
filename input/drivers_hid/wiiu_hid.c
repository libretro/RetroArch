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
#include <retro_endianness.h>

#include "wiiu_hid.h"

static wiiu_event_list events;
static wiiu_adapter_list adapters;

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
   wiiu_hid_t *hid = new_hid();
   HIDClient *client = new_hidclient();
   joypad_connection_t *connections = pad_connection_init(MAX_HID_PADS);

   if(!hid || !client || !connections) {
     goto error;
   }

   hid->connections = connections;

   wiiu_hid_init_lists();
   start_polling_thread(hid);
   if(!hid->polling_thread)
     goto error;

   RARCH_LOG("[hid]: Registering HIDClient\n");
   HIDAddClient(client, wiiu_attach_callback);
   hid->client = client;

   RARCH_LOG("[hid]: init success\n");
   return hid;

   error:
     RARCH_LOG("[hid]: initialization failed. cleaning up.\n");
     if(connections)
       free(connections);
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
      if(events.list) {
        wiiu_attach_event *event;
        while( (event = events.list) != NULL) {
          events.list = event->next;
          delete_attach_event(event);
        }
        memset(&events, 0, sizeof(events));
      }
   }
}

static void wiiu_hid_poll(void *data)
{
   (void)data;
}

static void wiiu_hid_send_control(void *data, uint8_t *buf, size_t size)
{
  wiiu_adapter_t *adapter = (wiiu_adapter_t *)data;
  if(!adapter)
    return;

  HIDWrite(adapter->handle, buf, size, NULL, NULL);
}

static int32_t wiiu_hid_set_report(void *data, uint8_t report_type,
               uint8_t report_id, void *report_data, uint32_t report_length)
{
  wiiu_adapter_t *adapter = (wiiu_adapter_t *)data;
  if(!adapter)
    return -1;

  return HIDSetReport(adapter->handle,
                      report_type,
                      report_id,
                      report_data,
                      report_length,
                      NULL, NULL);
}

static int32_t wiiu_hid_set_idle(void *data, uint8_t duration)
{
  wiiu_adapter_t *adapter = (wiiu_adapter_t *)data;
  if(!adapter)
    return -1;

  return HIDSetIdle(adapter->handle,
                    adapter->interface_index,
                    duration,
                    NULL, NULL);
}

static int32_t wiiu_hid_set_protocol(void *data, uint8_t protocol)
{
  wiiu_adapter_t *adapter = (wiiu_adapter_t *)data;
  if(!adapter)
    return -1;

  return HIDSetProtocol(adapter->handle,
                    adapter->interface_index,
                    protocol,
                    NULL, NULL);
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

  OSSetThreadCleanupCallback(thread, wiiu_hid_polling_thread_cleanup);

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

static void log_device(HIDDevice *device) {
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


static void synchronized_add_event(wiiu_attach_event *event) {
  OSFastMutex_Lock(&(events.lock));
  event->next = events.list;
  events.list = event;
  OSFastMutex_Unlock(&(events.lock));
}

static wiiu_attach_event *synchronized_get_events_list(void) {
  wiiu_attach_event *list;
  OSFastMutex_Lock(&(events.lock));
  list = events.list;
  events.list = NULL;
  OSFastMutex_Unlock(&(events.lock));

  return list;
}

static void synchronized_add_to_adapters_list(wiiu_adapter_t *adapter) {
  OSFastMutex_Lock(&(adapters.lock));
  adapter->next = adapters.list;
  adapters.list = adapter;
  OSFastMutex_Unlock(&(adapters.lock));
}

static int32_t wiiu_attach_callback(HIDClient *client, HIDDevice *device, uint32_t attach) {
  wiiu_attach_event *event;
  log_device(device);
  switch(attach) {
    case HID_DEVICE_ATTACH:
    case HID_DEVICE_DETACH:
      event = new_attach_event(device);
      if(!event)
        goto error;

      event->type = attach;
      synchronized_add_event(event);
      return DEVICE_USED;
    default:
      break;
  }

  error:
    delete_attach_event(event);
    return DEVICE_UNUSED;
}



static void wiiu_hid_detach(wiiu_hid_t *hid, wiiu_attach_event *event) {
}


static void wiiu_hid_attach(wiiu_hid_t *hid, wiiu_attach_event *event) {
  wiiu_adapter_t *adapter = new_adapter(event);

  if(!adapter) {
    RARCH_ERR("[hid]: Failed to allocate adapter.\n");
    goto error;
  }

  adapter->hid    = hid;

  RARCH_LOG("[hid]: pad_connection_pad_init\n");
  adapter->slot   = pad_connection_pad_init(hid->connections,
//      "hid", event->vendor_id, event->product_id, adapter,
      "hid", SWAP16(event->vendor_id), SWAP16(event->product_id), adapter,
      &wiiu_hid);

  if(adapter->slot < 0) {
    RARCH_ERR("[hid]: No available slots.\n");
    goto error;
  }

  RARCH_LOG("[hid]: got slot %d\n", adapter->slot);

  if(!pad_connection_has_interface(hid->connections, adapter->slot)) {
    RARCH_ERR("[hid]: Interface not found for HID device with vid=0x%04x pid=0x%04x\n",
              event->vendor_id, event->product_id);
    goto error;
  }

  RARCH_LOG("[hid]: adding to adapter list\n");
  synchronized_add_to_adapters_list(adapter);
  RARCH_LOG("[hid]: starting read loop\n");
  wiiu_start_read_loop(adapter);
  return;

  error:
    delete_adapter(adapter);
}

void wiiu_start_read_loop(wiiu_adapter_t *adapter)
{
  adapter->state = ADAPTER_STATE_READING;
  HIDRead(adapter->handle, adapter->rx_buffer, adapter->rx_size, wiiu_hid_read_loop_callback, adapter);
}

static void wiiu_hid_read_loop_callback(uint32_t handle, int32_t error,
              uint8_t *buffer, uint32_t buffer_size, void *userdata)
{
  uint32_t coreId = OSGetCoreId();
  wiiu_adapter_t *adapter = (wiiu_adapter_t *)userdata;
  if(!adapter)
  {
    RARCH_ERR("read_loop_callback: bad userdata\n");
    return;
  }

  RARCH_LOG("read_loop_callback running on core %d\n", coreId);
  usleep(5000);
  if(!adapter->hid->polling_thread_quit) {
    adapter->state = ADAPTER_STATE_READING;
    HIDRead(adapter->handle, adapter->rx_buffer, adapter->rx_size,
      wiiu_hid_read_loop_callback, adapter);
    return;
  }

  adapter->state = ADAPTER_STATE_DONE;
}

/**
 * Block until all the HIDRead() calls have returned.
 */
static void wiiu_hid_polling_thread_cleanup(OSThread *thread, void *stack) {
  int not_done = 0;
  wiiu_adapter_t *adapter;
  do {
    OSFastMutex_Lock(&(adapters.lock));
    not_done = 0;
    for(adapter = adapters.list; adapter != NULL; adapter = adapter->next) {
      if(adapter->state != ADAPTER_STATE_DONE) {
        not_done++;
      }
    }
    OSFastMutex_Unlock(&(adapters.lock));
    usleep(1000);
  } while(not_done);
}

static void wiiu_handle_attach_events(wiiu_hid_t *hid, wiiu_attach_event *list) {
  wiiu_attach_event *event;
  if(!hid || !list)
    return;

  for(event = list; event != NULL; event = event->next) {
    if(event->type == HID_DEVICE_ATTACH) {
      wiiu_hid_attach(hid, event);
    } else {
      wiiu_hid_detach(hid, event);
    }
  }
}

static int wiiu_hid_polling_thread(int argc, const char **argv) {
  wiiu_hid_t *hid = (wiiu_hid_t *)argv;
  int i = 0;
  RARCH_LOG("[hid]: polling thread is starting\n");
  while(!hid->polling_thread_quit) {
    wiiu_handle_attach_events(hid, synchronized_get_events_list());
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

static void wiiu_hid_init_lists(void) {
  RARCH_LOG("[hid]: Initializing events list\n");
  memset(&events, 0, sizeof(events));
  OSFastMutex_Init(&(events.lock), "attach_events");
  RARCH_LOG("[hid]: Initializing adapters list\n");
  memset(&adapters, 0, sizeof(adapters));
  OSFastMutex_Init(&(adapters.lock), "adapters");
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

static wiiu_adapter_t *new_adapter(wiiu_attach_event *event) {
  RARCH_LOG("[hid]: new_adapter()\n");

  wiiu_adapter_t *adapter  = alloc_zeroed(64, sizeof(wiiu_adapter_t));
  adapter->handle          = event->handle;
  adapter->interface_index = event->interface_index;
  adapter->rx_size         = event->max_packet_size_rx;
  adapter->rx_buffer       = alloc_zeroed(64, adapter->rx_size);

  return adapter;
}

static void delete_adapter(wiiu_adapter_t *adapter) {
  RARCH_LOG("[hid]: delete_adapter()\n");
  if(adapter) {
    if(adapter->rx_buffer) {
      free(adapter->rx_buffer);
      adapter->rx_buffer = NULL;
    }
    free(adapter);
  }
}

static wiiu_attach_event *new_attach_event(HIDDevice *device) {
  if(!device)
    return NULL;

  wiiu_attach_event *event = alloc_zeroed(4, sizeof(wiiu_attach_event));
  if(!event)
    return NULL;
  event->handle = device->handle;
  event->vendor_id = device->vid;
  event->product_id = device->pid;
  event->interface_index = device->interface_index;
  event->is_keyboard = (device->sub_class == 1 && device->protocol == 1);
  event->is_mouse = (device->sub_class == 1 && device->protocol == 2);
  event->max_packet_size_rx = device->max_packet_size_rx;
  event->max_packet_size_tx = device->max_packet_size_tx;

  return event;
}

static void delete_attach_event(wiiu_attach_event *event) {
  if(event)
    free(event);
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
   wiiu_hid_send_control,
   wiiu_hid_set_report,
   wiiu_hid_set_idle,
   wiiu_hid_set_protocol
};

