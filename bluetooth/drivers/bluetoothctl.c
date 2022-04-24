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

#include <compat/strl.h>
#include <configuration.h>

#include "../bluetooth_driver.h"
#include "../../retroarch.h"

typedef struct
{
   bool bluetoothctl_cache[256];
   unsigned bluetoothctl_counter[256];
   struct string_list* lines;
   char command[256];
} bluetoothctl_t;

static void *bluetoothctl_init(void)
{
   return calloc(1, sizeof(bluetoothctl_t));
}

static void bluetoothctl_free(void *data)
{
   if (data)
      free(data);
}

static void bluetoothctl_scan(void *data)
{
   char line[512];
   union string_list_elem_attr attr;
   FILE *dev_file                   = NULL;
   bluetoothctl_t *btctl            = (bluetoothctl_t*) data;

   attr.i = 0;
   if (btctl->lines)
      free(btctl->lines);
   btctl->lines = string_list_new();

   pclose(popen("bluetoothctl -- power on", "r"));

   pclose(popen("bluetoothctl --timeout 10 scan on", "r"));

   runloop_msg_queue_push(msg_hash_to_str(MSG_BLUETOOTH_SCAN_COMPLETE),
         1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT,
         MESSAGE_QUEUE_CATEGORY_INFO);

   dev_file = popen("bluetoothctl -- devices", "r");

   while (fgets(line, 512, dev_file))
   {
      size_t len = strlen(line);
      if (len > 0 && line[len-1] == '\n')
         line[--len] = '\0';

      string_list_append(btctl->lines, line, attr);
   }

   pclose(dev_file);
}

static void bluetoothctl_get_devices(void *data, struct string_list* devices)
{
   unsigned i;
   union string_list_elem_attr attr;
   bluetoothctl_t *btctl = (bluetoothctl_t*) data;

   attr.i = 0;

   if (!btctl->lines)
      return;

   for (i = 0; i < btctl->lines->size; i++)
   {
      char device[64];
      const char *line = btctl->lines->elems[i].data;

      /* bluetoothctl devices outputs lines of the format:
       * $ bluetoothctl devices
       *     'Device (mac address) (device name)'
       */
      strlcpy(device, line+24, sizeof(device));
      string_list_append(devices, device, attr);
   }
}

static bool bluetoothctl_device_is_connected(void *data, unsigned i)
{
   bluetoothctl_t *btctl = (bluetoothctl_t*) data;
   char ln[512]          = {0};
   char device[18]       = {0};
   const char *line      = btctl->lines->elems[i].data;
   FILE *command_file    = NULL;

   if (btctl->bluetoothctl_counter[i] == 60)
   {
      static struct string_list* list = NULL;
      btctl->bluetoothctl_counter[i]  = 0;
      list                            = string_split(line, " ");
      if (!list)
         return false;

      if (list->size == 0)
      {
         string_list_free(list);
         return false;
      }

      strlcpy(device, list->elems[1].data, sizeof(device));
      string_list_free(list);

      snprintf(btctl->command, sizeof(btctl->command), "\
            bluetoothctl -- info %s | grep 'Connected: yes'",
            device);

      command_file = popen(btctl->command, "r");

      while (fgets(ln, 512, command_file))
      {
         btctl->bluetoothctl_cache[i] = true;
         return true;
      }
      pclose(command_file);
      btctl->bluetoothctl_cache[i] = false;
   }
   else
   {
      btctl->bluetoothctl_counter[i]++;
      return btctl->bluetoothctl_cache[i];
   }

   return false;
}

static bool bluetoothctl_connect_device(void *data, unsigned idx)
{
   unsigned i;
   bluetoothctl_t *btctl               = (bluetoothctl_t*) data;
   char device[18]                     = {0};
   const char *line                    = btctl->lines->elems[idx].data;
   static struct string_list* list     = NULL;

   /* bluetoothctl devices outputs lines of the format:
    * $ bluetoothctl devices
    *     'Device (mac address) (device name)'
    */
   list                                = string_split(line, " ");
   if (!list)
      return false;

   if (list->size == 0)
   {
      string_list_free(list);
      return false;
   }

   strlcpy(device, list->elems[1].data, sizeof(device));
   string_list_free(list);

   snprintf(btctl->command, sizeof(btctl->command), "\
         bluetoothctl -- pairable on");

   pclose(popen(btctl->command, "r"));

   snprintf(btctl->command, sizeof(btctl->command), "\
         bluetoothctl -- pair %s",
         device);

   pclose(popen(btctl->command, "r"));

   snprintf(btctl->command, sizeof(btctl->command), "\
         bluetoothctl -- trust %s",
         device);

   pclose(popen(btctl->command, "r"));

   snprintf(btctl->command, sizeof(btctl->command), "\
         bluetoothctl -- connect %s",
         device);

   pclose(popen(btctl->command, "r"));

   btctl->bluetoothctl_counter[idx] = 0;
   return true;
}

static bool bluetoothctl_remove_device(void *data, unsigned idx)
{
   unsigned i;
   bluetoothctl_t *btctl               = (bluetoothctl_t*) data;
   char device[18]                     = {0};
   const char *line                    = btctl->lines->elems[idx].data;
   static struct string_list* list     = NULL;

   /* bluetoothctl devices outputs lines of the format:
    * $ bluetoothctl devices
    *     'Device (mac address) (device name)'
    */
   list                                = string_split(line, " ");
   if (!list)
      return false;

   if (list->size == 0)
   {
      string_list_free(list);
      return false;
   }

   strlcpy(device, list->elems[1].data, sizeof(device));
   string_list_free(list);

   snprintf(btctl->command, sizeof(btctl->command), "\
         echo -e \"disconnect %s\\nremove %s\\n\" | bluetoothctl",
         device, device);

   pclose(popen(btctl->command, "r"));

   runloop_msg_queue_push(msg_hash_to_str(MSG_BLUETOOTH_PAIRING_REMOVED),
         1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT,
         MESSAGE_QUEUE_CATEGORY_INFO);

   btctl->bluetoothctl_counter[idx] = 0;
   return true;
}

static void bluetoothctl_device_get_sublabel(
      void *data, char *s, unsigned i, size_t len)
{
   bluetoothctl_t *btctl = (bluetoothctl_t*) data;
   /* bluetoothctl devices outputs lines of the format:
    * $ bluetoothctl devices
    *     'Device (mac address) (device name)'
    */
   const char      *line = btctl->lines->elems[i].data;
   strlcpy(s, line+7, 18);
}

bluetooth_driver_t bluetooth_bluetoothctl = {
   bluetoothctl_init,
   bluetoothctl_free,
   bluetoothctl_scan,
   bluetoothctl_get_devices,
   bluetoothctl_device_is_connected,
   bluetoothctl_device_get_sublabel,
   bluetoothctl_connect_device,
   bluetoothctl_remove_device,
   "bluetoothctl",
};
