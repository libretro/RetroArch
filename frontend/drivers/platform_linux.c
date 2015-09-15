/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2015 - Daniel De Matteis
 * Copyright (C) 2012-2015 - Jason Fetters
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 * * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/param.h> /* PATH_MAX */

#include <boolean.h>
#include <retro_dirent.h>
#include <compat/strl.h>
#include <rhash.h>
#include <file/file_path.h>

#include "../frontend_driver.h"
#include "../../general.h"

static const char *proc_apm_path             = "/proc/apm";
static const char *proc_acpi_battery_path    = "/proc/acpi/battery";
static const char *proc_acpi_sys_ac_adapter_path= "/sys/class/power_supply/ACAD";
static const char *proc_acpi_sys_battery_path= "/sys/class/power_supply";
static const char *proc_acpi_ac_adapter_path = "/proc/acpi/ac_adapter";

static bool load_power_file(const char *path, char *buf, size_t buflen)
{
   ssize_t br = 0;
   const int fd = open(path, O_RDONLY);
   if (fd == -1)
      return false;
   br = read(fd, buf, buflen-1);
   close(fd);
   if (br < 0)
      return false;
   buf[br] = '\0';             /* null-terminate the string. */

   return true;
}

static bool make_proc_acpi_key_val(char **_ptr, char **_key, char **_val)
{
    char *ptr = *_ptr;

    while (*ptr == ' ')
        ptr++;  /* skip whitespace. */

    if (*ptr == '\0')
        return false;  /* EOF. */

    *_key = ptr;

    while ((*ptr != ':') && (*ptr != '\0'))
        ptr++;

    if (*ptr == '\0')
        return false;  /* (unexpected) EOF. */

    *(ptr++) = '\0';  /* terminate the key. */

    while ((*ptr == ' ') && (*ptr != '\0'))
        ptr++;  /* skip whitespace. */

    if (*ptr == '\0')
        return false;  /* (unexpected) EOF. */

    *_val = ptr;

    while ((*ptr != '\n') && (*ptr != '\0'))
        ptr++;

    if (*ptr != '\0')
        *(ptr++) = '\0';  /* terminate the value. */

    *_ptr = ptr;  /* store for next time. */
    return true;
}

#define ACPI_KEY_STATE                 0x10614a06U
#define ACPI_KEY_PRESENT               0xc28ac046U
#define ACPI_KEY_CHARGING_STATE        0x5ba13e29U
#define ACPI_KEY_REMAINING_CAPACITY    0xf36952edU
#define ACPI_KEY_DESIGN_CAPACITY       0x05e6488dU

#define ACPI_VAL_CHARGING_DISCHARGING  0xf268327aU
#define ACPI_VAL_CHARGING              0x095ee228U
#define ACPI_VAL_YES                   0x0b88c316U
#define ACPI_VAL_ONLINE                0x6842bf17U

static void
check_proc_acpi_battery(const char * node, bool * have_battery,
      bool * charging, int *seconds, int *percent)
{
   const char *base  = proc_acpi_battery_path;
   char path[1024];
   char info[1024]   = {0};
   char state[1024]  = {0};
   char         *ptr = NULL;
   char         *key = NULL;
   char         *val = NULL;
   bool       charge = false;
   bool       choose = false;
   int       maximum = -1;
   int     remaining = -1;
   int          secs = -1;
   int           pct = -1;

   snprintf(path, sizeof(path), "%s/%s/%s", base, node, "state");

   if (!load_power_file(path, state, sizeof (state)))
      return;

   snprintf(path, sizeof(path), "%s/%s/%s", base, node, "info");
   if (!load_power_file(path, info, sizeof (info)))
      return;

   ptr = &state[0];

   while (make_proc_acpi_key_val(&ptr, &key, &val))
   {
      uint32_t key_hash = djb2_calculate(key);
      uint32_t val_hash = djb2_calculate(val);

      switch (key_hash)
      {
         case ACPI_KEY_PRESENT:
            if (val_hash == ACPI_VAL_YES)
               *have_battery = true;
            break;
         case ACPI_KEY_CHARGING_STATE:
            switch (val_hash)
            {
               case ACPI_VAL_CHARGING_DISCHARGING:
               case ACPI_VAL_CHARGING:
                  charge = true;
                  break;
            }
            break;
         case ACPI_KEY_REMAINING_CAPACITY:
            {
               char  *endptr = NULL;
               const int cvt = (int)strtol(val, &endptr, 10);

               if (*endptr == ' ')
                  remaining = cvt;
            }
            break;
      }
   }

   ptr = &info[0];

   while (make_proc_acpi_key_val(&ptr, &key, &val))
   {
      uint32_t key_hash = djb2_calculate(key);

      switch (key_hash)
      {
         case ACPI_KEY_DESIGN_CAPACITY:
            {
               char  *endptr = NULL;
               const int cvt = (int)strtol(val, &endptr, 10);

               if (*endptr == ' ')
                  maximum = cvt;
            }
            break;
      }
   }

   if ((maximum >= 0) && (remaining >= 0))
   {
      pct = (int) ((((float) remaining) / ((float) maximum)) * 100.0f);
      if (pct < 0)
         pct = 0;
      if (pct > 100)
         pct = 100;
   }

   /* !!! FIXME: calculate (secs). */

   /*
    * We pick the battery that claims to have the most minutes left.
    *  (failing a report of minutes, we'll take the highest percent.)
    */
   if ((secs < 0) && (*seconds < 0))
   {
      if ((pct < 0) && (*percent < 0))
         choose = true;  /* at least we know there's a battery. */
      if (pct > *percent)
         choose = true;
   }
   else if (secs > *seconds)
      choose = true;

   if (choose)
   {
      *seconds = secs;
      *percent = pct;
      *charging = charge;
   }
}

static void
check_proc_acpi_sys_battery(const char * node, bool * have_battery,
      bool * charging, int *seconds, int *percent)
{
   unsigned capacity;
   char path[1024], info[1024], state[1024];
   const char *base  = proc_acpi_sys_battery_path;
   char         *ptr = NULL;
   char         *key = NULL;
   char         *val = NULL;
   bool       charge = false;
   bool       choose = false;
   int       maximum = -1;
   int     remaining = -1;
   int          secs = -1;
   int           pct = -1;

   if (!strstr(node, "BAT"))
      return;

   snprintf(path, sizeof(path), "%s/%s/%s", base, node, "status");
   if (!load_power_file(path, state, sizeof (state)))
      return;

   if (strstr(state, "Discharging"))
      *have_battery = true;
   else if (strstr(state, "Full"))
      *have_battery = true;

   snprintf(path, sizeof(path), "%s/%s/%s", base, node, "capacity");
   if (!load_power_file(path, state, sizeof (state)))
      return;

   capacity = atoi(state);

   *percent = capacity;
}


static void
check_proc_acpi_ac_adapter(const char * node, bool *have_ac)
{
   char path[1024];
   const char *base = proc_acpi_ac_adapter_path;
   char  state[256] = {0};
   char        *ptr = NULL;
   char        *key = NULL;
   char        *val = NULL;

   snprintf(path, sizeof(path), "%s/%s/%s", base, node, "state");
   if (!load_power_file(path, state, sizeof (state)))
      return;

   ptr = &state[0];
   while (make_proc_acpi_key_val(&ptr, &key, &val))
   {
      uint32_t key_hash = djb2_calculate(key);
      uint32_t val_hash = djb2_calculate(val);

      if (key_hash == ACPI_KEY_STATE &&
            val_hash == ACPI_VAL_ONLINE)
         *have_ac = true;
   }
}

static void
check_proc_acpi_sys_ac_adapter(const char * node, bool *have_ac)
{
   char  state[256], path[1024];
   const char *base = proc_acpi_sys_ac_adapter_path;

   snprintf(path, sizeof(path), "%s/%s/%s", base, node, "online");
   if (!load_power_file(path, state, sizeof (state)))
      return;

   if (strstr(state, "1"))
      *have_ac = true;
}

static bool next_string(char **_ptr, char **_str)
{
   char *ptr = *_ptr;
   char *str = *_str;

   while (*ptr == ' ')       /* skip any spaces... */
      ptr++;

   if (*ptr == '\0')
      return false;

   str = ptr;
   while ((*ptr != ' ') && (*ptr != '\n') && (*ptr != '\0'))
      ptr++;

   if (*ptr != '\0')
      *(ptr++) = '\0';

   *_str = str;
   *_ptr = ptr;
   return true;
}

static bool int_string(char *str, int *val)
{
   char *endptr = NULL;
   *val = (int) strtol(str, &endptr, 0);
   return ((*str != '\0') && (*endptr == '\0'));
}

static bool frontend_linux_powerstate_check_apm(
      enum frontend_powerstate *state,
      int *seconds, int *percent)
{
   char buf[128], *ptr;
   int ac_status       = 0;
   int battery_status  = 0;
   int battery_flag    = 0;
   int battery_percent = 0;
   int battery_time    = 0;
   char *str           = NULL;
   
   if (!load_power_file(proc_apm_path, buf, sizeof(buf)))
      return false;

   ptr                 = &buf[0];

   if (!next_string(&ptr, &str))     /* driver version */
      return false;
   if (!next_string(&ptr, &str))     /* BIOS version */
      return false;
   if (!next_string(&ptr, &str))     /* APM flags */
      return false;

   if (!next_string(&ptr, &str))     /* AC line status */
      return false;
   else if (!int_string(str, &ac_status))
      return false;

   if (!next_string(&ptr, &str))     /* battery status */
      return false;
   else if (!int_string(str, &battery_status))
      return false;

   if (!next_string(&ptr, &str))     /* battery flag */
      return false;
   else if (!int_string(str, &battery_flag))
      return false;
   if (!next_string(&ptr, &str))    /* remaining battery life percent */
      return false;
   if (str[strlen(str) - 1] == '%')
      str[strlen(str) - 1] = '\0';
   if (!int_string(str, &battery_percent))
      return false;

   if (!next_string(&ptr, &str))     /* remaining battery life time */
      return false;
   else if (!int_string(str, &battery_time))
      return false;

   if (!next_string(&ptr, &str))     /* remaining battery life time units */
      return false;
   else if (!strcmp(str, "min"))
      battery_time *= 60;

   if (battery_flag == 0xFF) /* unknown state */
      *state = FRONTEND_POWERSTATE_NONE;
   else if (battery_flag & (1 << 7))       /* no battery */
      *state = FRONTEND_POWERSTATE_NO_SOURCE;
   else if (battery_flag & (1 << 3))   /* charging */
      *state = FRONTEND_POWERSTATE_CHARGING;
   else if (ac_status == 1)
      *state = FRONTEND_POWERSTATE_CHARGED;        /* on AC, not charging. */
   else
      *state = FRONTEND_POWERSTATE_ON_POWER_SOURCE;

   if (battery_percent >= 0)         /* -1 == unknown */
      *percent = (battery_percent > 100) ? 100 : battery_percent; /* clamp between 0%, 100% */
   if (battery_time >= 0)            /* -1 == unknown */
      *seconds = battery_time;

   return true;
}

static bool frontend_linux_powerstate_check_acpi(
      enum frontend_powerstate *state,
      int *seconds, int *percent)
{
   bool ret            = false;
   struct RDIR *entry  = NULL;
   bool have_battery   = false;
   bool have_ac        = false;
   bool charging       = false;

   *state = FRONTEND_POWERSTATE_NONE;

   entry = retro_opendir(proc_acpi_battery_path);
   if (!entry)
      goto end;

   if (retro_dirent_error(entry))
      goto end;

   while (retro_readdir(entry))
      check_proc_acpi_battery(retro_dirent_get_name(entry),
            &have_battery, &charging, seconds, percent);

   retro_closedir(entry);

   entry = retro_opendir(proc_acpi_ac_adapter_path);
   if (!entry)
      goto end;

   while (retro_readdir(entry))
      check_proc_acpi_ac_adapter(retro_dirent_get_name(entry), &have_ac);

   if (!have_battery)
      *state = FRONTEND_POWERSTATE_NO_SOURCE;
   else if (charging)
      *state = FRONTEND_POWERSTATE_CHARGING;
   else if (have_ac)
      *state = FRONTEND_POWERSTATE_CHARGED;
   else
      *state = FRONTEND_POWERSTATE_ON_POWER_SOURCE;

   ret = true;

end:
   if (entry)
      retro_closedir(entry);

   return ret;
}

static bool frontend_linux_powerstate_check_acpi_sys(
      enum frontend_powerstate *state,
      int *seconds, int *percent)
{
   bool ret            = false;
   struct RDIR *entry  = NULL;
   bool have_battery   = false;
   bool have_ac        = false;
   bool charging       = false;

   *state = FRONTEND_POWERSTATE_NONE;

   entry = retro_opendir(proc_acpi_sys_battery_path);
   if (!entry)
      goto error;

   if (retro_dirent_error(entry))
      goto error;

   while (retro_readdir(entry))
      check_proc_acpi_sys_battery(retro_dirent_get_name(entry),
            &have_battery, &charging, seconds, percent);

   retro_closedir(entry);

   entry = retro_opendir(proc_acpi_sys_ac_adapter_path);
   if (!entry)
      goto error;

   while (retro_readdir(entry))
      check_proc_acpi_sys_ac_adapter(retro_dirent_get_name(entry), &have_ac);

   if (!have_battery)
   {
      *state = FRONTEND_POWERSTATE_NO_SOURCE;
   }
   else if (charging)
      *state = FRONTEND_POWERSTATE_CHARGING;
   else if (have_ac)
      *state = FRONTEND_POWERSTATE_CHARGED;
   else
      *state = FRONTEND_POWERSTATE_ON_POWER_SOURCE;

   return true;

error:
   if (entry)
      retro_closedir(entry);

   return false;
}

static enum frontend_powerstate 
frontend_linux_get_powerstate(int *seconds, int *percent)
{
   enum frontend_powerstate ret = FRONTEND_POWERSTATE_NONE;

   if (frontend_linux_powerstate_check_apm(&ret, seconds, percent))
      return ret;

   if (frontend_linux_powerstate_check_acpi(&ret, seconds, percent))
      return ret;

   if (frontend_linux_powerstate_check_acpi_sys(&ret, seconds, percent))
      return ret;

   return FRONTEND_POWERSTATE_NONE;
}

#define LINUX_ARCH_X86_64     0x23dea434U
#define LINUX_ARCH_X86        0x0b88b8cbU
#define LINUX_ARCH_ARM        0x0b885ea5U
#define LINUX_ARCH_PPC64      0x1028cf52U
#define LINUX_ARCH_MIPS       0x7c9aa25eU
#define LINUX_ARCH_TILE       0x7c9e7873U

static enum frontend_architecture frontend_linux_get_architecture(void)
{
   uint32_t buffer_hash;
   struct utsname buffer;

   if (uname(&buffer) != 0)
      return FRONTEND_ARCH_NONE;

   buffer_hash = djb2_calculate(buffer.machine);

   switch (buffer_hash)
   {
      case LINUX_ARCH_X86_64:
         return FRONTEND_ARCH_X86_64;
      case LINUX_ARCH_X86:
         return FRONTEND_ARCH_X86;
      case LINUX_ARCH_ARM:
         return FRONTEND_ARCH_ARM;
      case LINUX_ARCH_PPC64:
         return FRONTEND_ARCH_PPC;
      case LINUX_ARCH_MIPS:
         return FRONTEND_ARCH_MIPS;
      case LINUX_ARCH_TILE:
         return FRONTEND_ARCH_TILE;
   }

   return FRONTEND_ARCH_NONE;
}

static void frontend_linux_get_os(char *s, size_t len, int *major, int *minor)
{
   unsigned krel;
   struct utsname buffer;

   if (uname(&buffer) != 0)
      return;

   sscanf(buffer.release, "%d.%d.%u", major, minor, &krel);
   strlcpy(s, "Linux", len);
}

static void frontend_linux_get_env(int *argc,
      char *argv[], void *data, void *params_data)
{
   const char *xdg                 = NULL;
   const char *home                = NULL;
   char base_path[PATH_MAX];

   xdg  = getenv("XDG_CONFIG_HOME");
   home = getenv("HOME");

   if (xdg)
     snprintf(base_path, sizeof(base_path), "%s/retroarch", xdg);
   else if (home)
     snprintf(base_path, sizeof(base_path), "%s/.config/retroarch", home);
   else
     snprintf(base_path, sizeof(base_path), "retroarch");

   fill_pathname_join(g_defaults.dir.core, base_path,
      "cores", sizeof(g_defaults.dir.core));
   fill_pathname_join(g_defaults.dir.core_info, base_path,
      "cores", sizeof(g_defaults.dir.core_info));

   return;
}

frontend_ctx_driver_t frontend_ctx_linux = {
   frontend_linux_get_env,       /* environment_get */
   NULL,                         /* init */
   NULL,                         /* deinit */
   NULL,                         /* exitspawn */
   NULL,                         /* process_args */
   NULL,                         /* exec */
   NULL,                         /* set_fork */
   NULL,                         /* shutdown */
   NULL,                         /* get_name */
   frontend_linux_get_os,
   NULL,                         /* get_rating */
   NULL,                         /* load_content */
   frontend_linux_get_architecture,
   frontend_linux_get_powerstate,
   NULL,                         /* parse_drive_list */
   "linux",
};
