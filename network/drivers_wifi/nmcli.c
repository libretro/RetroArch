/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Jean-André Santoni
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

#include <time.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <array/rbuf.h>
#include <string/stdstring.h>
#include <retro_miscellaneous.h>
#include <string.h>

#include <libretro.h>

#include "../wifi_driver.h"
#include "../../retroarch.h"
#include "../../configuration.h"
#include "../../verbosity.h"

typedef struct
{
   wifi_network_scan_t scan;
} nmcli_t;

static void *nmcli_init(void)
{
   nmcli_t *nmcli = (nmcli_t*)calloc(1, sizeof(nmcli_t));
   return nmcli;
}

static void nmcli_free(void *data)
{
   nmcli_t *nmcli = (nmcli_t*)data;

   if (nmcli)
   {
      if (nmcli->scan.net_list)
         RBUF_FREE(nmcli->scan.net_list);
      free(nmcli);
   }
}

static bool nmcli_start(void *data)
{
   return true;
}

static void nmcli_stop(void *data) { }

static bool nmcli_enable(void* data, bool enabled)
{
   int ret = 0;

   if (enabled)
      ret = system("nmcli radio wifi on");
   else
      ret = system("nmcli radio wifi off");

   return WIFEXITED(ret) && WEXITSTATUS(ret) == 0;
}

static bool nmcli_connection_info(void *data, wifi_network_info_t *netinfo)
{
   FILE *cmd_file = NULL;
   char line[512];
   bool connected = false;

   cmd_file = popen("nmcli --terse --fields NAME,TYPE connection show --active | awk -F: '$2 ~ /^(wifi|802-11-wireless)$/ { print $1 }'", "r");

   connected = fgets(line, sizeof(line), cmd_file) != NULL;
   pclose(cmd_file);
   if (netinfo)
   {
      string_trim_whitespace(line);
      strlcpy(netinfo->ssid, line, sizeof(netinfo->ssid));
      netinfo->connected = connected;
   }

   return connected;
}

static void nmcli_scan(void *data)
{
   char line[512];
   nmcli_t *nmcli = (nmcli_t*)data;
   FILE *cmd_file = NULL;
   char cmd[256];
   int ret = 0;
   bool has_profile = false;

   nmcli->scan.scan_time = time(NULL);

   if (nmcli->scan.net_list)
      RBUF_FREE(nmcli->scan.net_list);

   cmd_file = popen("nmcli --terse --fields IN-USE,SSID dev wifi", "r");
   while (fgets(line, sizeof(line), cmd_file))
   {
      wifi_network_info_t entry;
      memset(&entry, 0, sizeof(entry));

      entry.connected = line[0] == '*';

      line[0] = ' '; /* skip the '*' */
      line[1] = ' '; /* skip the ':' */
      string_trim_whitespace_right(line);
      string_trim_whitespace_left(line);

      if (line[0] == '\0')
         continue;

      strlcpy(entry.ssid, line, sizeof(entry.ssid));

      /* Check if there is a profile for this ssid */
      snprintf(cmd, sizeof(cmd),
            "nmcli --terse --fields NAME,TYPE connection show | grep '^%s:'",
            entry.ssid);
      ret = system(cmd);
      has_profile = WIFEXITED(ret) && WEXITSTATUS(ret) == 0;
      /* If there is a profile attached to that ssid, we assume it contains a
       * password. If the password is wrong save_password will be set to false
       * after a failing attempt to connect. */
      entry.saved_password = has_profile;

      RBUF_PUSH(nmcli->scan.net_list, entry);
   }
   pclose(cmd_file);
}

static wifi_network_scan_t* nmcli_get_ssids(void *data)
{
   nmcli_t *nmcli = (nmcli_t*)data;
   return &nmcli->scan;
}

static bool nmcli_ssid_is_online(void *data, unsigned idx)
{
   nmcli_t *nmcli = (nmcli_t*)data;

   return nmcli->scan.net_list &&
      idx < RBUF_LEN(nmcli->scan.net_list) &&
      nmcli->scan.net_list[idx].connected;
}

static bool nmcli_connect_ssid(void *data,
      const wifi_network_info_t *netinfo)
{
   nmcli_t *nmcli = (nmcli_t*)data;
   char cmd[256];
   int ret = 0;
   unsigned int i = 0;
   bool saved_password = false;
   bool connected = false;

   if (!nmcli || !netinfo)
      return false;

   if (netinfo->saved_password)
      snprintf(cmd, sizeof(cmd), "nmcli connection up '%s'", netinfo->ssid);
   else
      /* This assumes the password and ssid don't contain single quotes */
      snprintf(cmd, sizeof(cmd),
            "nmcli dev wifi connect '%s' password '%s'",
            netinfo->ssid, netinfo->passphrase);

   ret = system(cmd);
   connected = WIFEXITED(ret) && WEXITSTATUS(ret) == 0;

   for (i = 0; i < RBUF_LEN(nmcli->scan.net_list); i++)
   {
      wifi_network_info_t *entry = &nmcli->scan.net_list[i];
      entry->connected = connected && strcmp(entry->ssid, netinfo->ssid) == 0;
      if (strcmp(entry->ssid, netinfo->ssid) == 0)
         /* If the connect attempt fails, it usually means the password is
          * wrong. The user can now try another one. */
         entry->saved_password = connected;
   }

   return connected;
}

static bool nmcli_disconnect_ssid(void *data,
      const wifi_network_info_t *netinfo)
{
   nmcli_t *nmcli = (nmcli_t*)data;
   char cmd[256];
   int ret = 0;
   unsigned int i = 0;
   bool disconnected = false;

   snprintf(cmd, sizeof(cmd), "nmcli connection down '%s'", netinfo->ssid);
   ret = system(cmd);

   disconnected = WIFEXITED(ret) && WEXITSTATUS(ret) == 0;

   for (i = 0; i < RBUF_LEN(nmcli->scan.net_list); i++)
   {
      wifi_network_info_t *entry = &nmcli->scan.net_list[i];
      if (strcmp(entry->ssid, netinfo->ssid) == 0)
         entry->connected = !disconnected;
   }

   return disconnected;
}

static void nmcli_tether_start_stop(void *a, bool b, char *c) { }

wifi_driver_t wifi_nmcli = {
   nmcli_init,
   nmcli_free,
   nmcli_start,
   nmcli_stop,
   nmcli_enable,
   nmcli_connection_info,
   nmcli_scan,
   nmcli_get_ssids,
   nmcli_ssid_is_online,
   nmcli_connect_ssid,
   nmcli_disconnect_ssid,
   nmcli_tether_start_stop,
   "nmcli",
};
