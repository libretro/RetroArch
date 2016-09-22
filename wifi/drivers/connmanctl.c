/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2015 - Michael Lelli
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

#include "../wifi_driver.h"
#include "../../runloop.h"
#include <file/file_path.h>
#include <compat/strl.h>

static struct string_list* lines;

static void *connmanctl_init(const char *device, uint64_t caps,
      unsigned width, unsigned height)
{
   (void)device;
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

static void connmanctl_scan()
{
   union string_list_elem_attr attr;
   attr.i = RARCH_FILETYPE_UNSET;
   char line[512];
   if (lines)
      free(lines);
   lines = string_list_new();

   pclose(popen("connmanctl scan wifi", "r"));

   FILE* serv_file = popen("connmanctl services", "r");
   while (fgets (line, 512, serv_file) != NULL)
   {
      size_t len = strlen(line);
         if (len > 0 && line[len-1] == '\n') {
         line[--len] = '\0';
      }

      string_list_append(lines, line, attr);
   }
   pclose(serv_file);
}

static void connmanctl_get_ssids(struct string_list* ssids)
{
   unsigned i;
   union string_list_elem_attr attr;
   attr.i = RARCH_FILETYPE_UNSET;

   for (i = 0; i < lines->size; i++)
   {
      const char *line = lines->elems[i].data;
      char ssid[20];
      strlcpy(ssid, line+4, sizeof(ssid));
      string_list_append(ssids, ssid, attr);
   }
}

static bool connmanctl_ssid_is_online(unsigned i)
{
   const char *line = lines->elems[i].data;
   return line[2] == 'O';
}

static bool connmanctl_connect_ssid(unsigned i)
{
   const char *line = lines->elems[i].data;
   char service[128];
   char command[256];
   char ln[512];
   strlcpy(service, line+25, sizeof(service));

   strlcat(command, "connmanctl connect ", sizeof(command));
   strlcat(command, service, sizeof(command));
   strlcat(command, " 2>&1", sizeof(command));

   printf("%s\n", command);

   FILE* file = popen(command, "r");
   while (fgets (ln, 512, file) != NULL)
   {
      printf("%s\n", ln);
      runloop_msg_queue_push(ln, 1, 180, true);
   }
   pclose(file);
   
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
