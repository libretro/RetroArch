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

#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
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

extern char **environ;

static bool nmcli_exec(char *const argv[], bool quiet)
{
   posix_spawn_file_actions_t actions;
   posix_spawn_file_actions_t *file_actions = NULL;
   pid_t pid;
   int ret;
   int status;

   if (quiet)
   {
      ret = posix_spawn_file_actions_init(&actions);
      if (ret != 0)
         return false;

      ret = posix_spawn_file_actions_addopen(&actions,
            STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
      if (ret == 0)
         ret = posix_spawn_file_actions_addopen(&actions,
               STDERR_FILENO, "/dev/null", O_WRONLY, 0);
      if (ret != 0)
      {
         posix_spawn_file_actions_destroy(&actions);
         return false;
      }
      file_actions = &actions;
   }

   ret = posix_spawnp(&pid, "nmcli", file_actions, NULL, argv, environ);
   if (file_actions)
      posix_spawn_file_actions_destroy(&actions);
   if (ret != 0)
      return false;

   do
   {
      ret = waitpid(pid, &status, 0);
   } while (ret < 0 && errno == EINTR);

   return ret == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

/* The argument vectors are filled in by assignment: C89 takes only
 * constant expressions in an aggregate initializer. */
static bool nmcli_profile_exists(const char *ssid)
{
   char *argv[6];
   argv[0] = (char*)"nmcli";
   argv[1] = (char*)"connection";
   argv[2] = (char*)"show";
   argv[3] = (char*)"id";
   argv[4] = (char*)ssid;
   argv[5] = NULL;
   return nmcli_exec(argv, true);
}

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

   nmcli->scan.scan_time = time(NULL);

   if (nmcli->scan.net_list)
      RBUF_FREE(nmcli->scan.net_list);

   cmd_file = popen("nmcli --terse --fields IN-USE,SSID dev wifi", "r");
   if (!cmd_file)
      return;

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

      /* If there is a profile attached to this SSID, assume it contains a
       * password. If the password is wrong save_password will be set to false
       * after a failing attempt to connect. */
      entry.saved_password = nmcli_profile_exists(entry.ssid);

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
   unsigned int i = 0;
   bool connected = false;

   if (!nmcli || !netinfo)
      return false;

   if (netinfo->saved_password)
   {
      char *argv[6];
      argv[0] = (char*)"nmcli";
      argv[1] = (char*)"connection";
      argv[2] = (char*)"up";
      argv[3] = (char*)"id";
      argv[4] = (char*)netinfo->ssid;
      argv[5] = NULL;
      connected = nmcli_exec(argv, false);
   }
   else
   {
      char *argv[8];
      argv[0] = (char*)"nmcli";
      argv[1] = (char*)"dev";
      argv[2] = (char*)"wifi";
      argv[3] = (char*)"connect";
      argv[4] = (char*)netinfo->ssid;
      argv[5] = (char*)"password";
      argv[6] = (char*)netinfo->passphrase;
      argv[7] = NULL;
      connected = nmcli_exec(argv, false);
   }

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
   char *argv[6];
   unsigned int i = 0;
   bool disconnected;

   argv[0] = (char*)"nmcli";
   argv[1] = (char*)"connection";
   argv[2] = (char*)"down";
   argv[3] = (char*)"id";
   argv[4] = (char*)netinfo->ssid;
   argv[5] = NULL;
   disconnected = nmcli_exec(argv, false);

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
