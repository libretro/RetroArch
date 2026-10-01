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
#include <string.h>

#include "../bluetooth_driver.h"
#include "../../retroarch.h"

#define BLUETOOTHCTL_MAX_DEVICES 256

typedef struct
{
   bool bluetoothctl_cache[BLUETOOTHCTL_MAX_DEVICES];
   unsigned bluetoothctl_counter[BLUETOOTHCTL_MAX_DEVICES];
   struct string_list* lines;
   char command[256];
   /* The running "scan on", between scan_begin and scan_end. */
   FILE *scan_pipe;
} bluetoothctl_t;

/* bluetoothctl prints "Device XX:XX:XX:XX:XX:XX <name>". An advertised
 * name can contain newlines and produce forged output lines, so check
 * each record before using its address in a shell command. */
static bool bluetoothctl_device_address(const char *line, char address[18])
{
   size_t i;

   if (!line || strncmp(line, "Device ", 7) != 0 || strlen(line) < 25 ||
         line[24] != ' ')
      return false;

   for (i = 0; i < 17; i++)
   {
      unsigned char c = (unsigned char)line[7 + i];
      if (i % 3 == 2)
      {
         if (c != ':')
            return false;
      }
      else if (!((c >= '0' && c <= '9') ||
                 (c >= 'A' && c <= 'F') ||
                 (c >= 'a' && c <= 'f')))
         return false;
   }

   memcpy(address, line + 7, 17);
   address[17] = '\0';
   return true;
}

static void *bluetoothctl_init(void)
{
   return calloc(1, sizeof(bluetoothctl_t));
}

static void bluetoothctl_free(void *data)
{
   bluetoothctl_t *btctl = (bluetoothctl_t*)data;
   if (btctl && btctl->scan_pipe)
      pclose(btctl->scan_pipe);
   if (data)
      free(data);
}

/* Starts "scan on" for the scan window and returns: bluetoothctl
 * stops by itself when its timeout runs out, and scan_end collects it.
 * The pclose() of it used to be here, blocking for the whole window. */
static void bluetoothctl_scan_begin(void *data)
{
   bluetoothctl_t *btctl = (bluetoothctl_t*) data;

   pclose(popen("bluetoothctl -- power on", "r"));

   if (btctl->scan_pipe)
      pclose(btctl->scan_pipe);
   btctl->scan_pipe = popen("bluetoothctl --timeout 10 scan on", "r");
}

static void bluetoothctl_scan_end(void *data)
{
   char line[512];
   char address[18];
   const char *msg;
   union string_list_elem_attr attr;
   FILE *dev_file                   = NULL;
   bluetoothctl_t *btctl            = (bluetoothctl_t*) data;

   /* At the end of the window its timeout has run out, so this returns
    * at once; on a cancel it waits out what is left of it. */
   if (btctl->scan_pipe)
   {
      pclose(btctl->scan_pipe);
      btctl->scan_pipe = NULL;
   }

   attr.i = 0;
   if (btctl->lines)
      free(btctl->lines);
   btctl->lines = string_list_new();

   if (!btctl->lines)
      return;

   msg = msg_hash_to_str(MSG_BLUETOOTH_SCAN_COMPLETE);

   runloop_msg_queue_push(msg, strlen(msg),
         1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT,
         MESSAGE_QUEUE_CATEGORY_INFO);

   dev_file = popen("bluetoothctl -- devices", "r");

   if (!dev_file)
      return;

   while (fgets(line, 512, dev_file))
   {
      size_t _len = strlen(line);
      if (_len > 0 && line[_len - 1] == '\n')
         line[--_len] = '\0';

      if (btctl->lines->size < BLUETOOTHCTL_MAX_DEVICES &&
            bluetoothctl_device_address(line, address))
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

   if (!btctl->lines || !devices)
      return;

   for (i = 0; i < btctl->lines->size; i++)
   {
      char device[64];
      const char *line = btctl->lines->elems[i].data;

      /* bluetoothctl devices outputs lines of the format:
       * $ bluetoothctl devices
       *     'Device (mac address) (device name)'
       */
      strlcpy(device, line + 25, sizeof(device));
      string_list_append(devices, device, attr);
   }
}

static bool bluetoothctl_device_is_connected(void *data, unsigned i)
{
   bluetoothctl_t *btctl = (bluetoothctl_t*) data;
   char ln[512]          = {0};
   char device[18]       = {0};
   FILE *command_file    = NULL;

   if (!btctl->lines || i >= btctl->lines->size ||
         i >= BLUETOOTHCTL_MAX_DEVICES ||
         !bluetoothctl_device_address(btctl->lines->elems[i].data, device))
      return false;

   if (btctl->bluetoothctl_counter[i] == 60)
   {
      btctl->bluetoothctl_counter[i]  = 0;

      snprintf(btctl->command, sizeof(btctl->command), "\
            bluetoothctl -- info %s | grep 'Connected: yes'",
            device);

      command_file = popen(btctl->command, "r");

      if (!command_file)
         return false;

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
   bluetoothctl_t *btctl               = (bluetoothctl_t*) data;
   char device[18]                     = {0};

   if (!btctl->lines || idx >= btctl->lines->size ||
         idx >= BLUETOOTHCTL_MAX_DEVICES ||
         !bluetoothctl_device_address(btctl->lines->elems[idx].data, device))
      return false;

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
   const char *msg                     = NULL;
   bluetoothctl_t *btctl               = (bluetoothctl_t*) data;
   char device[18]                     = {0};

   if (!btctl->lines || idx >= btctl->lines->size ||
         idx >= BLUETOOTHCTL_MAX_DEVICES ||
         !bluetoothctl_device_address(btctl->lines->elems[idx].data, device))
      return false;

   snprintf(btctl->command, sizeof(btctl->command), "\
         bluetoothctl -- disconnect %s",
         device);

   pclose(popen(btctl->command, "r"));

   snprintf(btctl->command, sizeof(btctl->command), "\
         bluetoothctl -- remove %s",
         device);

   pclose(popen(btctl->command, "r"));

   msg = msg_hash_to_str(MSG_BLUETOOTH_PAIRING_REMOVED);

   runloop_msg_queue_push(msg, strlen(msg),
         1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT,
         MESSAGE_QUEUE_CATEGORY_INFO);

   btctl->bluetoothctl_counter[idx] = 0;
   return true;
}

static void bluetoothctl_device_get_sublabel(
      void *data, char *s, unsigned i, size_t len)
{
   bluetoothctl_t *btctl = (bluetoothctl_t*) data;
   char address[18];

   if (!s || !len)
      return;
   *s = '\0';
   if (!btctl->lines || i >= btctl->lines->size ||
         i >= BLUETOOTHCTL_MAX_DEVICES ||
         !bluetoothctl_device_address(btctl->lines->elems[i].data, address))
      return;
   strlcpy(s, address, len);
}

bluetooth_driver_t bluetooth_bluetoothctl = {
   bluetoothctl_init,
   bluetoothctl_free,
   bluetoothctl_scan_begin,
   bluetoothctl_scan_end,
   bluetoothctl_get_devices,
   bluetoothctl_device_is_connected,
   bluetoothctl_device_get_sublabel,
   bluetoothctl_connect_device,
   bluetoothctl_remove_device,
   "bluetoothctl",
};
