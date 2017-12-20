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

#include <compat/strl.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <retro_miscellaneous.h>

#include "../wifi_driver.h"
#include "../../retroarch.h"
#include "../../lakka.h"

static bool connman_cache[256] = {0};
static unsigned connman_counter = 0;
static struct string_list* lines;

static void *connmanctl_init(void)
{
   return (void*)-1;
}

static void connmanctl_free(void *data)
{
   (void)data;
}

static bool connmanctl_start(void *data)
{
   (void)data;
   return true;
}

static void connmanctl_stop(void *data)
{
   (void)data;
}

static void connmanctl_scan(void)
{
   char line[512];
   union string_list_elem_attr attr;
   FILE *serv_file                  = NULL;

   attr.i = RARCH_FILETYPE_UNSET;
   if (lines)
      free(lines);
   lines = string_list_new();

   pclose(popen("connmanctl enable wifi", "r"));

   pclose(popen("connmanctl scan wifi", "r"));

   runloop_msg_queue_push("Wi-Fi scan complete.", 1, 180, true);

   serv_file = popen("connmanctl services", "r");
   while (fgets (line, 512, serv_file) != NULL)
   {
      size_t len = strlen(line);
      if (len > 0 && line[len-1] == '\n')
         line[--len] = '\0';

      string_list_append(lines, line, attr);
   }
   pclose(serv_file);
}

static void connmanctl_get_ssids(struct string_list* ssids)
{
   unsigned i;
   union string_list_elem_attr attr;
   attr.i = RARCH_FILETYPE_UNSET;

   if (!lines)
      return;

   for (i = 0; i < lines->size; i++)
   {
      char ssid[20];
      const char *line = lines->elems[i].data;

      strlcpy(ssid, line+4, sizeof(ssid));
      string_list_append(ssids, ssid, attr);
   }
}

static bool connmanctl_ssid_is_online(unsigned i)
{
   char ln[512] = {0};
   char service[128] = {0};
   char command[256] = {0};
   const char *line = lines->elems[i].data;
   FILE *command_file = NULL;

   if (connman_counter == 60)
   {
      connman_counter = 0;

      static struct string_list* list = NULL;
      list = string_split(line, " ");
      if (!list)
         return false;

      if (list->size == 0)
      {
         string_list_free(list);
         return false;
      }

      strlcpy(service, list->elems[list->size-1].data, sizeof(service));
      string_list_free(list);

      strlcat(command, "connmanctl services ", sizeof(command));
      strlcat(command, service, sizeof(command));
      strlcat(command, " | grep 'State = \\(online\\|ready\\)'", sizeof(command));

      command_file = popen(command, "r");

      while (fgets (ln, 512, command_file) != NULL)
      {
         connman_cache[i] = true;
         return true;
      }
      pclose(command_file);
      connman_cache[i] = false;
   }
   else
   {
      connman_counter++;
      return connman_cache[i];
   }

   return false;
}

static bool connmanctl_connect_ssid(unsigned i, const char* passphrase)
{
   char ln[512] = {0};
   char name[64] = {0};
   char service[128] = {0};
   char command[256] = {0};
   char settings_dir[PATH_MAX_LENGTH] = {0};
   char settings_path[PATH_MAX_LENGTH] = {0};
   FILE *command_file = NULL;
   FILE *settings_file = NULL;
   const char *line = lines->elems[i].data;

   static struct string_list* list = NULL;
   // connmanctl services outputs a 4 character prefixed lines, either whispace
   // or an identifier. i.e.:
   // $ connmanctl services
   //     '*A0 SSID some_unique_id'
   //     '    SSID some_another_unique_id'
   list = string_split(line+4, " ");
   if (!list)
      return false;

   if (list->size == 0)
   {
      string_list_free(list);
      return false;
   }

   for (int i = 0; i < list->size-1; i++)
   {
      strlcat(name, list->elems[i].data, sizeof(name));
      strlcat(name, " ", sizeof(name));
   }
   strlcpy(service, list->elems[list->size-1].data, sizeof(service));

   string_list_free(list);

   strlcat(settings_dir, LAKKA_CONNMAN_DIR, sizeof(settings_dir));
   strlcat(settings_dir, service, sizeof(settings_dir));

   path_mkdir(settings_dir);

   strlcat(settings_path, settings_dir, sizeof(settings_path));
   strlcat(settings_path, "/settings", sizeof(settings_path));

   settings_file = fopen(settings_path, "w");
   fprintf(settings_file, "[%s]\n", service);
   fprintf(settings_file, "Name=%s\n", name);
   fprintf(settings_file, "SSID=");

   for (int i=0; i < strlen(name); i++)
      fprintf(settings_file, "%02x", (unsigned int) name[i]);
   fprintf(settings_file, "\n");

   fprintf(settings_file, "Favorite=%s\n", "true");
   fprintf(settings_file, "AutoConnect=%s\n", "true");
   fprintf(settings_file, "Passphrase=%s\n", passphrase);
   fprintf(settings_file, "IPv4.method=%s\n", "dhcp");
   fclose(settings_file);

   pclose(popen("systemctl restart connman", "r"));

   strlcat(command, "connmanctl connect ", sizeof(command));
   strlcat(command, service, sizeof(command));
   strlcat(command, " 2>&1", sizeof(command));

   command_file = popen(command, "r");

   while (fgets (ln, 512, command_file) != NULL)
   {
      runloop_msg_queue_push(ln, 1, 180, true);
   }
   pclose(command_file);

   return true;
}

wifi_driver_t wifi_connmanctl = {
   connmanctl_init,
   connmanctl_free,
   connmanctl_start,
   connmanctl_stop,
   connmanctl_scan,
   connmanctl_get_ssids,
   connmanctl_ssid_is_online,
   connmanctl_connect_ssid,
   "connmanctl",
};
