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

/* TODO/FIXME - static globals - should go into userdata
 * struct for driver */
static bool bluetoothctl_cache[256]       = {0};
static unsigned bluetoothctl_counter[256] = {0};
static struct string_list* lines          = NULL;
static char command[256]                  = {0};

static void *bluetoothctl_init(void)
{
   return (void*)-1;
}

static void bluetoothctl_free(void *data)
{
   (void)data;
}

static bool bluetoothctl_start(void *data)
{
   (void)data;
   return true;
}

static void bluetoothctl_stop(void *data)
{
   (void)data;
}

static void bluetoothctl_scan(void)
{
   char line[512];
   union string_list_elem_attr attr;
   FILE *dev_file                   = NULL;

   attr.i = 0;
   if (lines)
      free(lines);
   lines = string_list_new();

   pclose(popen("bluetoothctl -- power on", "r"));

   pclose(popen("bluetoothctl --timeout 15 scan on", "r"));

   runloop_msg_queue_push(msg_hash_to_str(MSG_BLUETOOTH_SCAN_COMPLETE),
         1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT,
         MESSAGE_QUEUE_CATEGORY_INFO);

   dev_file = popen("bluetoothctl -- devices", "r");

   while (fgets(line, 512, dev_file))
   {
      size_t len = strlen(line);
      if (len > 0 && line[len-1] == '\n')
         line[--len] = '\0';

      string_list_append(lines, line, attr);
   }

   pclose(dev_file);
}

static void bluetoothctl_get_devices(struct string_list* devices)
{
   unsigned i;
   union string_list_elem_attr attr;

   attr.i = 0;

   if (!lines)
      return;

   for (i = 0; i < lines->size; i++)
   {
      char device[64];
      const char *line = lines->elems[i].data;

      /* bluetoothctl devices outputs lines of the format:
       * $ bluetoothctl devices
       *     'Device (mac address) (device name)'
       */
      strlcpy(device, line+24, sizeof(device));
      string_list_append(devices, device, attr);
   }
}

static bool bluetoothctl_device_is_connected(unsigned i)
{
   char ln[512]       = {0};
   char device[18]    = {0};
   const char *line   = lines->elems[i].data;
   FILE *command_file = NULL;

   if (bluetoothctl_counter[i] == 60)
   {
      static struct string_list* list = NULL;
      bluetoothctl_counter[i] = 0;
      list            = string_split(line, " ");
      if (!list)
         return false;

      if (list->size == 0)
      {
         string_list_free(list);
         return false;
      }

      strlcpy(device, list->elems[1].data, sizeof(device));
      string_list_free(list);

      snprintf(command, sizeof(command), "\
            bluetoothctl -- info %s | grep 'Connected: yes'",
            device);

      command_file = popen(command, "r");

      while (fgets(ln, 512, command_file))
      {
         bluetoothctl_cache[i] = true;
         return true;
      }
      pclose(command_file);
      bluetoothctl_cache[i] = false;
   }
   else
   {
      bluetoothctl_counter[i]++;
      return bluetoothctl_cache[i];
   }

   return false;
}

static bool bluetoothctl_connect_device(unsigned idx)
{
   unsigned i;
   char device[18]                     = {0};
   const char *line                    = lines->elems[idx].data;
   static struct string_list* list     = NULL;

   /* bluetoothctl devices outputs lines of the format:
    * $ bluetoothctl devices
    *     'Device (mac address) (device name)'
    */
   list = string_split(line, " ");
   if (!list)
      return false;

   if (list->size == 0)
   {
      string_list_free(list);
      return false;
   }

   strlcpy(device, list->elems[1].data, sizeof(device));
   string_list_free(list);

   snprintf(command, sizeof(command), "\
         bluetoothctl -- trust %s",
         device);

   pclose(popen(command, "r"));

   snprintf(command, sizeof(command), "\
         bluetoothctl -- pair %s",
         device);

   pclose(popen(command, "r"));

   snprintf(command, sizeof(command), "\
         bluetoothctl -- connect %s",
         device);

   pclose(popen(command, "r"));

   bluetoothctl_counter[idx] = 0;
   return true;
}

void bluetoothctl_device_get_sublabel (char *s, unsigned i, size_t len)
{
   /* bluetoothctl devices outputs lines of the format:
    * $ bluetoothctl devices
    *     'Device (mac address) (device name)'
    */
   const char *line = lines->elems[i].data;
   strlcpy(s, line+7, 18);
}

bluetooth_driver_t bluetooth_bluetoothctl = {
   bluetoothctl_init,
   bluetoothctl_free,
   bluetoothctl_start,
   bluetoothctl_stop,
   bluetoothctl_scan,
   bluetoothctl_get_devices,
   bluetoothctl_device_is_connected,
   bluetoothctl_device_get_sublabel,
   bluetoothctl_connect_device,
   "bluetoothctl",
};
