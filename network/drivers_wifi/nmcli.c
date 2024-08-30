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
   /* semantics here are broken: nmcli_enable(..., false) is called
    * on startup which is probably not what we want. */

#if 0
   if (enabled)
      pclose(popen("nmcli radio wifi on", "r"));
   else
      pclose(popen("nmcli radio wifi off", "r"));
#endif

   return true;
}

static bool nmcli_connection_info(void *data, wifi_network_info_t *netinfo)
{
   FILE *cmd_file = NULL;
   char line[512];

   if (!netinfo)
      return false;

   cmd_file = popen("nmcli -f NAME c show --active | tail -n+2", "r");

   if (fgets(line, sizeof(line), cmd_file))
   {
      strlcpy(netinfo->ssid, line, sizeof(netinfo->ssid));
      netinfo->connected = true;
      return true;
   }

   return false;
}

static void nmcli_scan(void *data)
{
   char line[512];
   nmcli_t *nmcli = (nmcli_t*)data;
   FILE *cmd_file = NULL;

   nmcli->scan.scan_time = time(NULL);

   if (nmcli->scan.net_list)
      RBUF_FREE(nmcli->scan.net_list);

   cmd_file = popen("nmcli -f IN-USE,SSID dev wifi | tail -n+2", "r");
   while (fgets(line, 512, cmd_file))
   {
      wifi_network_info_t entry;
      memset(&entry, 0, sizeof(entry));

      string_trim_whitespace(line);
      if (!line || line[0] == '\0')
         continue;

      if (line[0] == '*')
      {
         entry.connected = true;
         line[0] = ' ';
         string_trim_whitespace(line);
      }

      strlcpy(entry.ssid, line, sizeof(entry.ssid));

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

   if (!nmcli->scan.net_list || idx >= RBUF_LEN(nmcli->scan.net_list))
      return false;
   return nmcli->scan.net_list[idx].connected;
}

static bool nmcli_connect_ssid(void *data,
      const wifi_network_info_t *netinfo)
{
   nmcli_t *nmcli = (nmcli_t*)data;
   char cmd[256];
   int ret, i;

   if (!nmcli || !netinfo)
      return false;

   snprintf(cmd, sizeof(cmd),
         "nmcli dev wifi connect \"%s\" password \"%s\" 2>&1",
         netinfo->ssid, netinfo->passphrase);
   if ((ret = pclose(popen(cmd, "r"))) == 0)
   {
      for (i = 0; i < RBUF_LEN(nmcli->scan.net_list); i++)
      {
         wifi_network_info_t* entry = &nmcli->scan.net_list[i];
         entry->connected = strcmp(entry->ssid, netinfo->ssid) == 0;
      }
   }

   return true;
}

static bool nmcli_disconnect_ssid(void *data,
      const wifi_network_info_t *netinfo)
{
   char cmd[256];
   snprintf(cmd, sizeof(cmd), "nmcli c down \"%s\"", netinfo->ssid);
   pclose(popen(cmd, "r"));

   return true;
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
