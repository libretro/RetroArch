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

#include <dbus/dbus.h>
#include <compat/strl.h>
#include <configuration.h>
#include <retro_timers.h>
#include <string/stdstring.h>

#include "../bluetooth_driver.h"
#include "../../retroarch.h"

typedef struct
{
   /* object path. usually looks like /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF
    * technically unlimited, but should be enough */
   char path[128];

   /* for display purposes 64 bytes should be enough */
   char name[64];

   /* MAC address, 17 bytes */
   char address[18];

   /* freedesktop.org icon name
    * See bluez/src/dbus-common.c
    * Can be NULL */
   char icon[64];

   int connected;
   int paired;
   int trusted;
} device_info_t;

#define VECTOR_LIST_TYPE device_info_t
#define VECTOR_LIST_NAME device_info
#include "../../libretro-common/lists/vector_list.c"
#undef VECTOR_LIST_TYPE
#undef VECTOR_LIST_NAME

typedef struct
{
    struct device_info_vector_list *devices;
    char adapter[256];
    DBusConnection* dbus_connection;
    bool bluez_cache[256];
    int bluez_cache_counter[256];
} bluez_t;

static void *bluez_init (void)
{
   return calloc(1, sizeof(bluez_t));
}

static void bluez_free (void *data)
{
   if (data)
      free(data);
}

static int
set_bool_property (
   bluez_t *bluez,
   const char *path,
   const char *arg_adapter,
   const char *arg_property,
   int value)
{
   DBusError err;
   DBusMessage *message, *reply;
   DBusMessageIter req_iter, req_subiter;

   dbus_error_init(&err);

   message = dbus_message_new_method_call(
      "org.bluez",
      path,
      "org.freedesktop.DBus.Properties",
      "Set"
   );
   if (!message)
      return 1;

   dbus_message_iter_init_append(message, &req_iter);
   if (!dbus_message_iter_append_basic(
            &req_iter, DBUS_TYPE_STRING, &arg_adapter))
      goto fault;
   if (!dbus_message_iter_append_basic(
            &req_iter, DBUS_TYPE_STRING, &arg_property))
      goto fault;
   if (!dbus_message_iter_open_container(
            &req_iter, DBUS_TYPE_VARIANT,
            DBUS_TYPE_BOOLEAN_AS_STRING, &req_subiter))
      goto fault;
   if (!dbus_message_iter_append_basic(
            &req_subiter, DBUS_TYPE_BOOLEAN, &value))
      goto fault;
   if (!dbus_message_iter_close_container(
            &req_iter, &req_subiter))
      goto fault;

   reply = dbus_connection_send_with_reply_and_block(bluez->dbus_connection,
      message, 1000, &err);
   if (!reply)
      goto fault;
   dbus_message_unref(reply);
   dbus_message_unref(message);
   return 0;

fault:
   dbus_message_iter_abandon_container_if_open(&req_iter, &req_subiter);
   dbus_message_unref(message);
   return 1;
}

static int get_bool_property(
   bluez_t *bluez,
   const char *path,
   const char *arg_adapter,
   const char *arg_property,
   int *value)
{
   DBusMessage *message, *reply;
   DBusError err;
   DBusMessageIter root_iter, variant_iter;

   dbus_error_init(&err);

   message = dbus_message_new_method_call( "org.bluez", path,
      "org.freedesktop.DBus.Properties", "Get");
   if (!message)
      return 1;

   if (!dbus_message_append_args(message,
      DBUS_TYPE_STRING, &arg_adapter,
      DBUS_TYPE_STRING, &arg_property,
      DBUS_TYPE_INVALID))
      return 1;

   reply = dbus_connection_send_with_reply_and_block(bluez->dbus_connection,
      message, 1000, &err);

   dbus_message_unref(message);

   if (!reply)
      return 1;

   if (!dbus_message_iter_init(reply, &root_iter))
      return 1;

   if (DBUS_TYPE_VARIANT != dbus_message_iter_get_arg_type(&root_iter))
      return 1;

   dbus_message_iter_recurse(&root_iter, &variant_iter);
   dbus_message_iter_get_basic(&variant_iter, value);

   dbus_message_unref(reply);
   return 0;
}

static int adapter_discovery (bluez_t *bluez, const char *method)
{
   DBusMessage *message = dbus_message_new_method_call(
         "org.bluez", bluez->adapter,
         "org.bluez.Adapter1", method);
   if (!message)
      return 1;

   if (!dbus_connection_send(bluez->dbus_connection, message, NULL))
      return 1;

   dbus_connection_flush(bluez->dbus_connection);
   dbus_message_unref(message);

   return 0;
}

static int get_managed_objects (bluez_t *bluez, DBusMessage **reply)
{
   DBusMessage *message;
   DBusError err;

   dbus_error_init(&err);

   message = dbus_message_new_method_call("org.bluez", "/",
         "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
   if (!message)
      return 1;

   *reply = dbus_connection_send_with_reply_and_block(bluez->dbus_connection,
         message, -1, &err);
   /* if (!reply) is done by the caller in this one */

   dbus_message_unref(message);
   return 0;
}

static int device_method (bluez_t *bluez, const char *path, const char *method)
{
   DBusMessage *message, *reply;
   DBusError err;

   dbus_error_init(&err);

   message = dbus_message_new_method_call( "org.bluez", path,
         "org.bluez.Device1", method);
   if (!message)
      return 1;

   reply = dbus_connection_send_with_reply_and_block(bluez->dbus_connection,
         message, 10000, &err);
   if (!reply)
      return 1;

   dbus_connection_flush(bluez->dbus_connection);
   dbus_message_unref(message);

   return 0;
}

static int get_default_adapter(bluez_t *bluez, DBusMessage *reply)
{
   /* "...an application would discover the available adapters by
    * performing a ObjectManager.GetManagedObjects call and look for any
    * returned objects with an “org.bluez.Adapter1″ interface.
    * The concept of a default adapter was always a bit fuzzy and the
    * value could’t be changed, so if applications need something like it
    * they could e.g. just pick the first adapter they encounter in the
    * GetManagedObjects reply."
    * -- http://www.bluez.org/bluez-5-api-introduction-and-porting-guide/
    */

   DBusMessageIter root_iter;
   DBusMessageIter dict_1_iter, dict_2_iter;
   DBusMessageIter array_1_iter, array_2_iter;

   char *obj_path, *interface_name;

   /* a{oa{sa{sv}}} */
   if (!dbus_message_iter_init(reply, &root_iter))
      return 1;

   /* a */
   if (DBUS_TYPE_ARRAY != dbus_message_iter_get_arg_type(&root_iter))
      return 1;
   dbus_message_iter_recurse(&root_iter, &array_1_iter);
   do
   {
      /* a{...} */
      if (DBUS_TYPE_DICT_ENTRY != dbus_message_iter_get_arg_type(&array_1_iter))
         return 1;
      dbus_message_iter_recurse(&array_1_iter, &dict_1_iter);

      /* a{o...} */
      if (DBUS_TYPE_OBJECT_PATH != dbus_message_iter_get_arg_type(&dict_1_iter))
         return 1;
      dbus_message_iter_get_basic(&dict_1_iter, &obj_path);

      if (!dbus_message_iter_next(&dict_1_iter))
         return 1;
      /* a{oa} */
      if (DBUS_TYPE_ARRAY != dbus_message_iter_get_arg_type(&dict_1_iter))
         return 1;
      dbus_message_iter_recurse(&dict_1_iter, &array_2_iter);
      do
      {
         /* empty array? */
         if (DBUS_TYPE_INVALID == 
               dbus_message_iter_get_arg_type(&array_2_iter))
            continue;

         /* a{oa{...}} */
         if (DBUS_TYPE_DICT_ENTRY != 
               dbus_message_iter_get_arg_type(&array_2_iter))
            return 1;
         dbus_message_iter_recurse(&array_2_iter, &dict_2_iter);

         /* a{oa{s...}} */
         if (DBUS_TYPE_STRING != 
               dbus_message_iter_get_arg_type(&dict_2_iter))
            return 1;
         dbus_message_iter_get_basic(&dict_2_iter, &interface_name);

         if (string_is_equal(interface_name, "org.bluez.Adapter1"))
         {
            strlcpy(bluez->adapter, obj_path, 256);
            return 0;
         }
      } while (dbus_message_iter_next(&array_2_iter));
   } while (dbus_message_iter_next(&array_1_iter));

   /* Couldn't find an adapter */
   return 1;
}

static int read_scanned_devices (bluez_t *bluez, DBusMessage *reply)
{
   device_info_t device;
   DBusMessageIter root_iter;
   DBusMessageIter dict_1_iter, dict_2_iter, dict_3_iter;
   DBusMessageIter array_1_iter, array_2_iter, array_3_iter;
   DBusMessageIter variant_iter;
   char *obj_path, *interface_name, *interface_property_name;
   char *found_device_address, *found_device_name, *found_device_icon;

   /* a{oa{sa{sv}}} */
   if (!dbus_message_iter_init(reply, &root_iter))
      return 1;

   /* a */
   if (DBUS_TYPE_ARRAY != dbus_message_iter_get_arg_type(&root_iter))
      return 1;

   dbus_message_iter_recurse(&root_iter, &array_1_iter);

   do
   {
      /* a{...} */
      if (DBUS_TYPE_DICT_ENTRY != 
            dbus_message_iter_get_arg_type(&array_1_iter))
         return 1;

      dbus_message_iter_recurse(&array_1_iter, &dict_1_iter);

      /* a{o...} */
      if (DBUS_TYPE_OBJECT_PATH != 
            dbus_message_iter_get_arg_type(&dict_1_iter))
         return 1;

      dbus_message_iter_get_basic(&dict_1_iter, &obj_path);

      if (!dbus_message_iter_next(&dict_1_iter))
         return 1;

      /* a{oa} */
      if (DBUS_TYPE_ARRAY != 
            dbus_message_iter_get_arg_type(&dict_1_iter))
         return 1;

      dbus_message_iter_recurse(&dict_1_iter, &array_2_iter);
      do
      {
         /* empty array? */
         if (DBUS_TYPE_INVALID == 
               dbus_message_iter_get_arg_type(&array_2_iter))
            continue;

         /* a{oa{...}} */
         if (DBUS_TYPE_DICT_ENTRY != 
               dbus_message_iter_get_arg_type(&array_2_iter))
            return 1;
         dbus_message_iter_recurse(&array_2_iter, &dict_2_iter);

         /* a{oa{s...}} */
         if (DBUS_TYPE_STRING != 
               dbus_message_iter_get_arg_type(&dict_2_iter))
            return 1;
         dbus_message_iter_get_basic(&dict_2_iter, &interface_name);

         if (!string_is_equal(interface_name, "org.bluez.Device1"))
            continue;

         memset(&device, 0, sizeof(device));
         strlcpy(device.path, obj_path, 128);

         if (!dbus_message_iter_next(&dict_2_iter))
            return 1;

         /* a{oa{sa}} */
         if (DBUS_TYPE_ARRAY != dbus_message_iter_get_arg_type(&dict_2_iter))
            return 1;

         dbus_message_iter_recurse(&dict_2_iter, &array_3_iter);

         do {
            /* empty array? */
            if (DBUS_TYPE_INVALID ==
                  dbus_message_iter_get_arg_type(&array_3_iter))
               continue;

            /* a{oa{sa{...}}} */
            if (DBUS_TYPE_DICT_ENTRY != 
                  dbus_message_iter_get_arg_type(&array_3_iter))
               return 1;
            dbus_message_iter_recurse(&array_3_iter, &dict_3_iter);

            /* a{oa{sa{s...}}} */
            if (DBUS_TYPE_STRING != 
                  dbus_message_iter_get_arg_type(&dict_3_iter))
               return 1;

            dbus_message_iter_get_basic(&dict_3_iter,
                  &interface_property_name);

            if (!dbus_message_iter_next(&dict_3_iter))
               return 1;
            /* a{oa{sa{sv}}} */
            if (DBUS_TYPE_VARIANT != 
                  dbus_message_iter_get_arg_type(&dict_3_iter))
               return 1;

            /* Below, "Alias" property is used instead of "Name".
             * "This value ("Name") is only present for
             * completeness.  It is better to always use
             * the Alias property when displaying the
             * devices name."
             * -- bluez/doc/device-api.txt
             */

            /* DBUS_TYPE_VARIANT is a container type */
            dbus_message_iter_recurse(&dict_3_iter, &variant_iter);
            if (string_is_equal(interface_property_name, "Address"))
            {
               dbus_message_iter_get_basic(&variant_iter,
                     &found_device_address);
               strlcpy(device.address, found_device_address, 18);
            }
            else if (string_is_equal(interface_property_name, "Alias"))
            {
               dbus_message_iter_get_basic(&variant_iter,
                     &found_device_name);
               strlcpy(device.name, found_device_name, 64);
            }
            else if (string_is_equal(interface_property_name, "Icon"))
            {
               dbus_message_iter_get_basic(&variant_iter,
                     &found_device_icon);
               strlcpy(device.icon, found_device_icon, 64);
            }
            else if (string_is_equal(interface_property_name, "Connected"))
            {
               dbus_message_iter_get_basic(&variant_iter,
                     &device.connected);
            }
            else if (string_is_equal(interface_property_name, "Paired"))
            {
               dbus_message_iter_get_basic(&variant_iter,
                     &device.paired);
            }
            else if (string_is_equal(interface_property_name, "Trusted"))
            {
               dbus_message_iter_get_basic(&variant_iter,
                     &device.trusted);
            }
         } while (dbus_message_iter_next(&array_3_iter));

         if (!device_info_vector_list_append(bluez->devices, device))
            return 1;

      } while (dbus_message_iter_next(&array_2_iter));
   } while (dbus_message_iter_next(&array_1_iter));

   return 0;
}

static void bluez_dbus_connect(bluez_t *bluez)
{
   DBusError err;
   dbus_error_init(&err);
   bluez->dbus_connection = dbus_bus_get_private(DBUS_BUS_SYSTEM, &err);
}

static void bluez_dbus_disconnect(bluez_t *bluez)
{
   if (!bluez->dbus_connection)
      return;

   dbus_connection_close(bluez->dbus_connection);
   dbus_connection_unref(bluez->dbus_connection);
   bluez->dbus_connection = NULL;
}

static void bluez_scan(void *data)
{
   DBusError err;
   DBusMessage *reply;
   bluez_t *bluez = (bluez_t*)data;

   bluez_dbus_connect(bluez);

   if (get_managed_objects(bluez, &reply))
      return;
   if (!reply)
      return;

   /* Get default adapter */
   if (get_default_adapter(bluez, reply))
      return;
   dbus_message_unref(reply);

   /* Power device on */
   if (set_bool_property(bluez, bluez->adapter,
            "org.bluez.Adapter1", "Powered", 1))
      return;

   /* Start discovery */
   if (adapter_discovery(bluez, "StartDiscovery"))
      return;

   retro_sleep(10000);

   /* Stop discovery */
   if (adapter_discovery(bluez, "StopDiscovery"))
      return;

   /* Get scanned devices */
   if (get_managed_objects(bluez, &reply))
      return;
   if (!reply)
      return;

   if (bluez->devices)
      device_info_vector_list_free(bluez->devices);
   bluez->devices = device_info_vector_list_new();

   read_scanned_devices(bluez, reply);
   dbus_message_unref(reply);
   bluez_dbus_disconnect(bluez);
}

static void bluez_get_devices(void *data,
      struct string_list* devices_string_list)
{
   unsigned i;
   union string_list_elem_attr attr;
   bluez_t *bluez = (bluez_t*)data;

   attr.i = 0;

   if (!bluez->devices)
      return;

   for (i = 0; i < bluez->devices->count; i++)
   {
      char device[64];
      strlcpy(device, bluez->devices->data[i].name, sizeof(device));
      string_list_append(devices_string_list, device, attr);
   }
}

static bool bluez_device_is_connected(void *data, unsigned i)
{
   int value;
   bluez_t *bluez = (bluez_t*)data;

   if (bluez->bluez_cache_counter[i] == 60)
   {
      bluez->bluez_cache_counter[i] = 0;
      bluez_dbus_connect(bluez);

      /* Device disappeared */
      if (get_bool_property(bluez, bluez->devices->data[i].path,
            "org.bluez.Device1", "Connected", &value))
          value = false;

      bluez_dbus_disconnect(bluez);

      bluez->bluez_cache[i] = value;
      return value;
   }

   bluez->bluez_cache_counter[i]++;
   return bluez->bluez_cache[i];
}

static void bluez_device_get_sublabel(
      void *data, char *s, unsigned i, size_t len)
{
   bluez_t *bluez = (bluez_t*)data;
   strlcpy(s, bluez->devices->data[i].address, len);
}

static bool bluez_connect_device(void *data, unsigned i)
{
   bluez_t *bluez = (bluez_t*)data;
   bluez_dbus_connect(bluez);

   /* Trust the device */
   if (set_bool_property(bluez, bluez->devices->data[i].path,
            "org.bluez.Device1", "Trusted", 1))
      return false;

   /* Pair the device */
   device_method(bluez, bluez->devices->data[i].path, "Pair");

   /* Can be "Already Exists" */
   /* Connect the device */
   if (device_method(bluez, bluez->devices->data[i].path, "Connect"))
      return false;

   bluez_dbus_disconnect(bluez);
   bluez->bluez_cache_counter[i] = 0;
   return true;
}

static bool bluez_remove_device(void *data, unsigned i)
{
   bluez_t *bluez = (bluez_t*)data;
   bluez_dbus_connect(bluez);

   /* Disconnect the device */
   device_method(bluez, bluez->devices->data[i].path, "Disconnect");

   /* Remove the device */
   if (device_method(bluez, bluez->devices->data[i].path, "RemoveDevice"))
      return false;

   runloop_msg_queue_push(msg_hash_to_str(MSG_BLUETOOTH_PAIRING_REMOVED),
         1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT,
         MESSAGE_QUEUE_CATEGORY_INFO);

   bluez_dbus_disconnect(bluez);
   bluez->bluez_cache_counter[i] = 0;
   return true;
}

bluetooth_driver_t bluetooth_bluez = {
   bluez_init,
   bluez_free,
   bluez_scan,
   bluez_get_devices,
   bluez_device_is_connected,
   bluez_device_get_sublabel,
   bluez_connect_device,
   bluez_remove_device,
   "bluez",
};
