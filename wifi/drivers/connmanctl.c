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
#include <configuration.h>
#include <verbosity.h>

#include "../wifi_driver.h"
#include "../../retroarch.h"
#include "../../lakka.h"
#ifdef HAVE_GFX_WIDGETS
#include "../../gfx/gfx_widgets.h"
#endif

typedef struct
{
   bool connman_cache[256];
   unsigned connman_counter;
   struct string_list* lines;
   char command[256];
   bool connmanctl_widgets_supported;
} connman_t;

static void *connmanctl_init(void)
{
   connman_t *connman = (connman_t*)calloc(1, sizeof(connman_t));
#ifdef HAVE_GFX_WIDGETS
   connman->connmanctl_widgets_supported = gfx_widgets_ready();
#endif
   return connman;
}

static void connmanctl_free(void *data)
{
   if (data)
      free(data);
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

static bool connmanctl_tether_status(connman_t *connman)
{
   /* Returns true if the tethering is active
    * false when tethering is not active
    */
   size_t ln_size;
   FILE *command_file = NULL;
   char ln[3]         = {0};

   /* Following command lists 'technologies' of connman,
    * greps the wifi + 10 following lines, then first
    * occurance of 'Tethering', then 'True' and counts
    * the matching lines.
    * Expected result is either 1 (active) or 0 (not active)
    */
   snprintf(connman->command, sizeof(connman->command), "\
         connmanctl technologies | \
         grep \"/net/connman/technology/wifi\" -A 10 | \
         grep \"^  Tethering =\" -m 1 | \
         grep \"True\" | \
         wc -l");

   command_file = popen(connman->command, "r");

   fgets(ln, sizeof(ln), command_file);

   ln_size = strlen(ln)-1;
   if (ln[ln_size] == '\n')
      ln[ln_size] = '\0';

   RARCH_LOG("[CONNMANCTL] Tether Status: command: \"%s\", output: \"%s\"\n",
         connman->command, ln);

   pclose(command_file);

   if (!ln)
      return false;
   if (ln[0] == '0')
      return false;
   if (ln[0] == '1')
      return true;
   return false;
}

static void connmanctl_tether_toggle(
      connman_t *connman, bool switch_on, char* apname, char* passkey)
{
   /* Starts / stops the tethering service on wi-fi device */
   char output[256]     = {0};
   FILE *command_file   = NULL;
   settings_t *settings = config_get_ptr();
#ifdef HAVE_GFX_WIDGETS
   bool widgets_active  = connman->connmanctl_widgets_supported;
#endif

   snprintf(connman->command, sizeof(connman->command), "\
         connmanctl tether wifi %s %s %s",
         switch_on ? "on" : "off", apname, passkey);

   command_file = popen(connman->command, "r");

   RARCH_LOG("[CONNMANCTL] Tether toggle: command: \"%s\"\n",
         connman->command);

   while (fgets(output, sizeof(output), command_file))
   {
      size_t output_size = strlen(output) - 1;
      if (output[output_size] == '\n')
         output[output_size] = '\0';

      RARCH_LOG("[CONNMANCTL] Tether toggle: output: \"%s\"\n",
            output);

#ifdef HAVE_GFX_WIDGETS
      if (!widgets_active)
#endif
         runloop_msg_queue_push(output, 1, 180, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT,
               MESSAGE_QUEUE_CATEGORY_INFO);
   }

   pclose(command_file);

   RARCH_LOG("[CONNMANCTL] Tether toggle: command finished\n");

   if (switch_on)
   {
      if (!connmanctl_tether_status(connman))
         configuration_set_bool(settings,
               settings->bools.localap_enable, false);
   }
   else
   {
      if (connmanctl_tether_status(connman))
         configuration_set_bool(settings,
               settings->bools.localap_enable, true);
   }
}

static void connmanctl_scan(void *data)
{
   char line[512];
   union string_list_elem_attr attr;
   FILE *serv_file                  = NULL;
   settings_t *settings             = config_get_ptr();
   connman_t *connman               = (connman_t*)data;

   attr.i = RARCH_FILETYPE_UNSET;
   if (connman->lines)
      free(connman->lines);
   connman->lines = string_list_new();

   if (connmanctl_tether_status(connman))
   {
      runloop_msg_queue_push(msg_hash_to_str(MSG_LOCALAP_SWITCHING_OFF),
            1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT,
            MESSAGE_QUEUE_CATEGORY_INFO);
      configuration_set_bool(settings,
            settings->bools.localap_enable, false);
      connmanctl_tether_toggle(connman, false, "", "");
   }

   pclose(popen("connmanctl enable wifi", "r"));

   pclose(popen("connmanctl scan wifi", "r"));

   runloop_msg_queue_push(msg_hash_to_str(MSG_WIFI_SCAN_COMPLETE),
         1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT,
         MESSAGE_QUEUE_CATEGORY_INFO);

   serv_file = popen("connmanctl services", "r");
   while (fgets(line, 512, serv_file))
   {
      size_t len = strlen(line);
      if (len > 0 && line[len-1] == '\n')
         line[--len] = '\0';

      string_list_append(connman->lines, line, attr);
   }
   pclose(serv_file);
}

static void connmanctl_get_ssids(void *data, struct string_list* ssids)
{
   unsigned i;
   union string_list_elem_attr attr;
   attr.i = RARCH_FILETYPE_UNSET;
   connman_t *connman = (connman_t*)data;

   if (!connman->lines)
      return;

   for (i = 0; i < connman->lines->size; i++)
   {
      char ssid[32];
      const char *line = connman->lines->elems[i].data;

      strlcpy(ssid, line+4, sizeof(ssid));
      string_list_append(ssids, ssid, attr);
   }
}

static bool connmanctl_ssid_is_online(void *data, unsigned i)
{
   char ln[512]       = {0};
   char service[128]  = {0};
   connman_t *connman = (connman_t*)data;
   const char *line   = connman->lines->elems[i].data;
   FILE *command_file = NULL;

   if (connman->connman_counter == 60)
   {
      static struct string_list* list = NULL;
      connman->connman_counter = 0;
      list            = string_split(line, " ");
      if (!list)
         return false;

      if (list->size == 0)
      {
         string_list_free(list);
         return false;
      }

      strlcpy(service, list->elems[list->size-1].data, sizeof(service));
      string_list_free(list);

      snprintf(connman->command, sizeof(connman->command), "\
            connmanctl services %s | grep 'State = \\(online\\|ready\\)'",
            service);

      command_file = popen(connman->command, "r");

      while (fgets(ln, 512, command_file))
      {
         connman->connman_cache[i] = true;
         return true;
      }
      pclose(command_file);
      connman->connman_cache[i] = false;
   }
   else
   {
      connman->connman_counter++;
      return connman->connman_cache[i];
   }

   return false;
}

static bool connmanctl_connect_ssid(
      void *data, unsigned idx, const char* passphrase)
{
   unsigned i;
   char ln[512]                        = {0};
   char name[64]                       = {0};
   char service[128]                   = {0};
   char settings_dir[PATH_MAX_LENGTH]  = {0};
   char settings_path[PATH_MAX_LENGTH] = {0};
   FILE *command_file                  = NULL;
   FILE *settings_file                 = NULL;
   connman_t *connman                  = (connman_t*)data;
   const char *line                    = connman->lines->elems[idx].data;
   settings_t *settings                = config_get_ptr();
   static struct string_list* list     = NULL;
#ifdef HAVE_GFX_WIDGETS
   bool widgets_active                 = connman->connmanctl_widgets_supported;
#endif
   /* connmanctl services outputs a 4 character prefixed lines,
    * either whitespace or an identifier. i.e.:
    * $ connmanctl services
    *     '*A0 SSID some_unique_id'
    *     '    SSID some_another_unique_id'
    */
   list = string_split(line+4, " ");
   if (!list)
      return false;

   if (list->size == 0)
   {
      string_list_free(list);
      return false;
   }

   for (i = 0; i < list->size-1; i++)
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

   for (i = 0; i < strlen(name); i++)
      fprintf(settings_file, "%02x", (unsigned int) name[i]);
   fprintf(settings_file, "\n");

   fprintf(settings_file, "Favorite=%s\n", "true");
   fprintf(settings_file, "AutoConnect=%s\n", "true");
   fprintf(settings_file, "Passphrase=%s\n", passphrase);
   fprintf(settings_file, "IPv4.method=%s\n", "dhcp");
   fclose(settings_file);

   if (connmanctl_tether_status(connman))
   {
      runloop_msg_queue_push(msg_hash_to_str(MSG_LOCALAP_SWITCHING_OFF),
            1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT,
            MESSAGE_QUEUE_CATEGORY_INFO);
      configuration_set_bool(settings,
            settings->bools.localap_enable, false);
      connmanctl_tether_toggle(connman, false, "", "");
   }

   pclose(popen("systemctl restart connman", "r"));

   snprintf(connman->command, sizeof(connman->command), "\
         connmanctl connect %s 2>&1",
         service);

   command_file = popen(connman->command, "r");

   while (fgets(ln, sizeof(ln), command_file))
   {
#ifdef HAVE_GFX_WIDGETS
      if (!widgets_active)
#endif
         runloop_msg_queue_push(ln, 1, 180, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT,
               MESSAGE_QUEUE_CATEGORY_INFO);
   }
   pclose(command_file);

   return true;
}

static void connmanctl_get_connected_ssid(
      connman_t *connman, char* ssid, size_t buffersize)
{
   size_t ssid_size;
   /* Stores the SSID of the currently connected Wi-Fi
    * network in ssid
    */
   FILE *command_file = NULL;

   if (buffersize < 1)
      return;

   /* Following command lists all connman services, greps
    * only 'wifi_' services, then greps the one with
    * 'R' (Ready) or 'O' (Online) flag and cuts out the ssid
    */
   snprintf(connman->command, sizeof(connman->command), "\
         connmanctl services | \
         grep wifi_ | \
         grep \"^..\\(R\\|O\\)\" | \
         cut -d' ' -f 2");

   command_file = popen(connman->command, "r");

   fgets(ssid, buffersize, command_file);

   pclose(command_file);

   ssid_size = strlen(ssid) - 1;

   if ((ssid_size + 1) > 0)
      if (ssid[ssid_size] == '\n')
         ssid[ssid_size] = '\0';

   RARCH_LOG("[CONNMANCTL] Get Connected SSID: command: \"%s\", output: \"%s\"\n",
         connman->command, (ssid_size + 1) ? ssid : "<nothing_found>");
}

static void connmanctl_get_connected_servicename(
      connman_t *connman, char* servicename, size_t buffersize)
{
   /* Stores the service name of currently connected Wi-Fi
    * network in servicename
    */
   FILE *command_file = NULL;
   FILE *service_file = NULL;
   char ln[3]         = {0};
   char *temp;

   if (buffersize < 1)
      return;

   temp = (char*)malloc(sizeof(char) * buffersize);

   /* Following command lists all stored services in
    * connman settings folder, which are then used in
    * the next while loop for parsing if the service
    * is currently online/ready
    */
   snprintf(connman->command, sizeof(connman->command), "\
         for serv in %s/wifi_*/ ; do \
            if [ -d $serv ] ; then \
               basename $serv ; \
            fi ; \
         done",
         LAKKA_CONNMAN_DIR);

   command_file = popen(connman->command, "r");

   RARCH_LOG("[CONNMANCTL] Testing configured services for activity: command: \"%s\"\n",
         connman->command);

   while (fgets(temp, buffersize, command_file))
   {
      size_t ln_size;
      size_t temp_size = strlen(temp) - 1;

      if ((temp_size + 1) > 0)
         if (temp[temp_size] == '\n')
            temp[temp_size] = '\0';

      if ((temp_size + 1) == 0)
      {
         RARCH_WARN("[CONNMANCTL] Service name empty.\n");
         continue;
      }

      /* Here we test the found service for online | ready
       * status and count the lines. Expected results are
       * 0 = is not active, 1 = is active
       */
      snprintf(connman->command, sizeof(connman->command), "\
            connmanctl services %s | \
            grep \"^  State = \\(online\\|ready\\)\" | \
            wc -l",
            temp);

      service_file = popen(connman->command, "r");

      fgets(ln, sizeof(ln), service_file);
      ln_size = strlen(ln) - 1;

      if (ln[ln_size] == '\n')
         ln[ln_size] = '\0';

      pclose(service_file);

      RARCH_LOG("[CONNMANCTL] Service: \"%s\", status: \"%s\"\n",
            temp, ln);

      if (ln[0] == '1')
      {
         pclose(command_file);

         strlcpy(servicename, temp, buffersize);

         free(temp);

         RARCH_LOG("[CONNMANCTL] Service \"%s\" considered as currently online\n",
               servicename);

         return;
      }
   }

   pclose(command_file);
}

static void connmanctl_tether_start_stop(void *data, bool start, char* configfile)
{
   /* Start / stop wrapper for the tethering service
    * It also checks, if we are currently connected
    * to a wi-fi network, which needs to be disconnected
    * before starting the tethering service, or if the
    * tethering service is already running / not running
    * before performing the desired action
    */
   FILE *command_file  = NULL;
   char apname[64]     = {0};
   char passkey[256]   = {0};
   char ln[512]        = {0};
   char ssid[64]       = {0};
   char service[256]   = {0};
   connman_t *connman  = (connman_t*)data;
#ifdef HAVE_GFX_WIDGETS
   bool widgets_active = connman->connmanctl_widgets_supported;
#endif

   RARCH_LOG("[CONNMANCTL] Tether start stop: begin\n");

   if (start) /* we want to start tethering */
   {
      RARCH_LOG("[CONNMANCTL] Tether start stop: request to start access point\n");

      if (connmanctl_tether_status(connman)) /* check if already tethering and bail out if so */
      {
         RARCH_LOG("[CONNMANCTL] Tether start stop: AP already running\n");
         runloop_msg_queue_push(msg_hash_to_str(MSG_LOCALAP_ALREADY_RUNNING),
               1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT,
               MESSAGE_QUEUE_CATEGORY_INFO);
         return;
      }

      /* check if there is a config file, if not, create one, if yes, parse it */
      if (!(command_file = fopen(configfile, "r")))
      {
         RARCH_WARN("[CONNMANCTL] Tether start stop: config \"%s\" does not exist\n",
               configfile);

         if (!(command_file = fopen(configfile, "w")))
         {
            RARCH_ERR("[CONNMANCTL] Tether start stop: cannot create config file \"%s\"\n",
                  configfile);

            runloop_msg_queue_push(msg_hash_to_str(MSG_LOCALAP_ERROR_CONFIG_CREATE),
                  1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT,
                  MESSAGE_QUEUE_CATEGORY_ERROR);

            return;
         }

         RARCH_LOG("[CONNMANCTL] Tether start stop: creating new config \"%s\"\n",
               configfile);

         snprintf(apname, sizeof(apname), "LakkaAccessPoint");
         snprintf(passkey, sizeof(passkey), "RetroArch");

         fprintf(command_file, "APNAME=%s\nPASSWORD=%s", apname, passkey);

         fclose(command_file);

         RARCH_LOG("[CONNMANCTL] Tether start stop: new config \"%s\" created\n",
               configfile);
      }
      else
      {
         fclose(command_file);

         RARCH_LOG("[CONNMANCTL] Tether start stop: config \"%s\" exists, reading it\n",
               configfile);

         snprintf(connman->command, sizeof(connman->command), "\
               grep -m 1 \"^APNAME=\" %s | cut -d '=' -f 2- && \
               grep -m 1 \"^PASSWORD=\" %s | cut -d '=' -f 2-",
               configfile, configfile);

         command_file = popen(connman->command, "r");

         int i = 0;

         RARCH_LOG("[CONNMANCTL] Tether start stop: parsing command: \"%s\"\n",
               connman->command);

         while (fgets(ln, sizeof(ln), command_file))
         {
            size_t ln_size = strlen(ln) - 1;

            i++;
            if ((ln_size + 1) > 1)
            {
               if (ln[ln_size] == '\n')
                  ln[ln_size] = '\0';

               if (i == 1)
               {
                  strlcpy(apname, ln, sizeof(apname));

                  RARCH_LOG("[CONNMANCTL] Tether start stop: found APNAME: \"%s\"\n",
                        apname);

                  continue;
               }

               if (i == 2)
               {
                  strlcpy(passkey, ln, sizeof(passkey));

                  RARCH_LOG("[CONNMANCTL] Tether start stop: found PASSWORD: \"%s\"\n",
                        passkey);

                  continue;
               }

               if (i > 2)
               {
                  RARCH_WARN("[CONNMANCTL] Tether start stop: you should not get here...\n");
                  break;
               }
            }
         }

         pclose(command_file);
      }

      if (!apname || !passkey)
      {
         RARCH_ERR("[CONNMANCTL] Tether start stop: APNAME or PASSWORD missing\n");

         snprintf(ln, sizeof(ln),
               msg_hash_to_str(MSG_LOCALAP_ERROR_CONFIG_PARSE),
               configfile);

         runloop_msg_queue_push(ln,
               1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT,
               MESSAGE_QUEUE_CATEGORY_ERROR);

         return;
      }

      /* check if connected to a wi-fi network */
      RARCH_LOG("[CONNMANCTL] Tether start stop: checking if not connected to a wi-fi network...\n");
      connmanctl_get_connected_ssid(connman, ssid, sizeof(ssid));

      if (strlen(ssid) != 0)
      {
         connmanctl_get_connected_servicename(connman, service, sizeof(service));

         if (strlen(service) != 0)
         {
            /* disconnect from wi-fi network */
            RARCH_LOG("[CONNMANCTL] Tether start stop: connected to SSID \"%s\", service \"%s\"\n",
                  ssid, service);

            snprintf(ln, sizeof(ln),
                  msg_hash_to_str(MSG_WIFI_DISCONNECT_FROM),
                  ssid);

            runloop_msg_queue_push(ln, 1, 180, true,
                  NULL, MESSAGE_QUEUE_ICON_DEFAULT,
                  MESSAGE_QUEUE_CATEGORY_INFO);

            snprintf(connman->command, sizeof(connman->command), "\
                  connmanctl disconnect %s",
                  service);

            command_file = popen(connman->command, "r");

            RARCH_LOG("[CONNMANCTL] Tether start stop: disconnecting from service \"%s\", command: \"%s\"\n",
                  service, connman->command);

            while (fgets(ln, sizeof(ln), command_file))
            {
               size_t ln_size = strlen(ln) - 1;
               if (ln[ln_size] == '\n')
                  ln[ln_size] = '\0';

               RARCH_LOG("[CONNMANCTL] Tether start stop: output: \"%s\"\n",
                     ln);

#ifdef HAVE_GFX_WIDGETS
               if (!widgets_active)
#endif
                  runloop_msg_queue_push(ln, 1, 180, true,
                        NULL, MESSAGE_QUEUE_ICON_DEFAULT,
                        MESSAGE_QUEUE_CATEGORY_INFO);
            }

            pclose(command_file);

            RARCH_LOG("[CONNMANCTL] Tether start stop: disconnect end\n");
         }
      }

      snprintf(connman->command, sizeof(connman->command),
            msg_hash_to_str(MSG_LOCALAP_STARTING),
            apname, passkey);

      runloop_msg_queue_push(connman->command,
            1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT,
            MESSAGE_QUEUE_CATEGORY_INFO);
   }
   else /* we want to stop tethering */
   {
      RARCH_LOG("[CONNMANCTL] Tether start stop: request to stop access point\n");

      if (!connmanctl_tether_status(connman)) /* check if not tethering and when not, bail out */
      {
         RARCH_LOG("[CONNMANCTL] Tether start stop: access point is not running\n");

         runloop_msg_queue_push(msg_hash_to_str(MSG_LOCALAP_NOT_RUNNING),
               1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT,
               MESSAGE_QUEUE_CATEGORY_INFO);

         return;
      }

      runloop_msg_queue_push(msg_hash_to_str(MSG_LOCALAP_SWITCHING_OFF),
            1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT,
            MESSAGE_QUEUE_CATEGORY_INFO);
   }

   RARCH_LOG("[CONNMANCTL] Tether start stop: calling tether_toggle()\n");

   /* call the tether toggle function */
   connmanctl_tether_toggle(connman, start, apname, passkey);

   RARCH_LOG("[CONNMANCTL] Tether start stop: end\n");
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
   connmanctl_tether_start_stop,
   "connmanctl",
};
