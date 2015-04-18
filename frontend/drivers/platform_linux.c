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

#include "../frontend_driver.h"

#include <stdio.h>
#include <stdint.h>
#include <boolean.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <compat/strl.h>

static const char *proc_apm_path = "/proc/apm";
static const char *proc_acpi_battery_path = "/proc/acpi/battery";
static const char *proc_acpi_ac_adapter_path = "/proc/acpi/ac_adapter";

static int open_acpi_file(const char *base, const char *node, const char *key)
{
   const size_t pathlen = strlen(base) + strlen(node) + strlen(key) + 3;
   char *path = (char *)alloca(pathlen);
   if (!path)
      return -1;

   snprintf(path, pathlen, "%s/%s/%s", base, node, key);
   return open(path, O_RDONLY);
}

static bool load_acpi_file(const char *base, const char *node, const char *key,
      char *buf, size_t buflen)
{
   ssize_t br = 0;
   const int fd = open_acpi_file(base, node, key);
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

static void
check_proc_acpi_battery(const char * node, bool * have_battery,
      bool * charging, int *seconds, int *percent)
{
   const char *base = proc_acpi_battery_path;
   char info[1024];
   char state[1024];
   char *ptr = NULL;
   char *key = NULL;
   char *val = NULL;
   bool charge = false;
   bool choose = false;
   int maximum = -1;
   int remaining = -1;
   int secs = -1;
   int pct = -1;

   if (!load_acpi_file(base, node, "state", state, sizeof (state)))
      return;
   else if (!load_acpi_file(base, node, "info", info, sizeof (info)))
      return;

   ptr = &state[0];
   while (make_proc_acpi_key_val(&ptr, &key, &val))
   {
      if (!strcmp(key, "present"))
      {
         if (!strcmp(val, "yes"))
            *have_battery = true;
      }
      else if (!strcmp(key, "charging state"))
      {
         /* !!! FIXME: what exactly _does_ charging/discharging mean? */
         if (!strcmp(val, "charging/discharging"))
            charge = true;
         else if (!strcmp(val, "charging"))
            charge = true;
      }
      else if (!strcmp(key, "remaining capacity"))
      {
         char *endptr = NULL;
         const int cvt = (int) strtol(val, &endptr, 10);
         if (*endptr == ' ')
            remaining = cvt;
      }
   }

   ptr = &info[0];
   while (make_proc_acpi_key_val(&ptr, &key, &val))
   {
      if (!strcmp(key, "design capacity"))
      {
         char *endptr = NULL;
         const int cvt = (int) strtol(val, &endptr, 10);
         if (*endptr == ' ')
            maximum = cvt;
      }
   }

   if ((maximum >= 0) && (remaining >= 0))
   {
      pct = (int) ((((float) remaining) / ((float) maximum)) * 100.0f);
      if (pct < 0)
         pct = 0;
      else if (pct > 100)
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
check_proc_acpi_ac_adapter(const char * node, bool *have_ac)
{
    const char *base = proc_acpi_ac_adapter_path;
    char state[256];
    char *ptr = NULL;
    char *key = NULL;
    char *val = NULL;

    if (!load_acpi_file(base, node, "state", state, sizeof (state)))
        return;

    ptr = &state[0];
    while (make_proc_acpi_key_val(&ptr, &key, &val))
    {
        if (!strcmp(key, "state"))
        {
            if (!strcmp(val, "on-line"))
                *have_ac = true;
        }
    }
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

bool frontend_linux_powerstate_check_apm(enum frontend_powerstate *state,
      int *seconds, int *percent)
{
   int ac_status = 0;
   int battery_status = 0;
   int battery_flag = 0;
   int battery_percent = 0;
   int battery_time = 0;
   const int fd = open(proc_apm_path, O_RDONLY);
   char buf[128];
   char *ptr = &buf[0];
   char *str = NULL;
   ssize_t br;

   if (fd == -1)
      return false;       /* can't use this interface. */

   br = read(fd, buf, sizeof (buf) - 1);
   close(fd);

   if (br < 0)
      return false;

   buf[br] = '\0';             /* null-terminate the string. */
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

   const int pct = battery_percent;
   const int secs = battery_time;

   if (pct >= 0)         /* -1 == unknown */
      *percent = (pct > 100) ? 100 : pct; /* clamp between 0%, 100% */
   if (secs >= 0)        /* -1 == unknown */
      *seconds = secs;

   return true;
}

bool frontend_linux_powerstate_check_acpi(enum frontend_powerstate *state,
      int *seconds, int *percent)
{
   struct dirent *dent = NULL;
   DIR *dirp = NULL;
   bool have_battery = false;
   bool have_ac = false;
   bool charging = false;

   *state = FRONTEND_POWERSTATE_NONE;

   dirp = opendir(proc_acpi_battery_path);
   if (dirp == NULL)
      return false;  /* can't use this interface. */

   while ((dent = readdir(dirp)) != NULL)
   {
      const char *node = dent->d_name;
      check_proc_acpi_battery(node, &have_battery, &charging,
            seconds, percent);
   }
   closedir(dirp);

   dirp = opendir(proc_acpi_ac_adapter_path);
   if (dirp == NULL)
      return false;  /* can't use this interface. */

   while ((dent = readdir(dirp)) != NULL)
   {
      const char *node = dent->d_name;
      check_proc_acpi_ac_adapter(node, &have_ac);
   }
   closedir(dirp);

   if (!have_battery)
      *state = FRONTEND_POWERSTATE_NO_SOURCE;
   else if (charging)
      *state = FRONTEND_POWERSTATE_CHARGING;
   else if (have_ac)
      *state = FRONTEND_POWERSTATE_CHARGED;
   else
      *state = FRONTEND_POWERSTATE_ON_POWER_SOURCE;

   return true;   /* definitive answer. */
}

static enum frontend_powerstate 
frontend_linux_get_powerstate(int *seconds, int *percent)
{
   enum frontend_powerstate ret = FRONTEND_POWERSTATE_NONE;

   if (frontend_linux_powerstate_check_apm(&ret, seconds, percent))
      return ret;

   if (frontend_linux_powerstate_check_acpi(&ret, seconds, percent))
      return ret;

   return FRONTEND_POWERSTATE_NONE;
}

enum frontend_architecture frontend_linux_get_architecture(void)
{
   struct utsname buffer;

   if (uname(&buffer) != 0)
      return FRONTEND_ARCH_NONE;

   if (!strcmp(buffer.machine, "x86_64"))
      return FRONTEND_ARCH_X86_64;
   if (!strcmp(buffer.machine, "x86"))
      return FRONTEND_ARCH_X86;

   return FRONTEND_ARCH_NONE;
}

static void frontend_linux_get_os(char *name, size_t sizeof_name, int *major, int *minor)
{
   struct utsname buffer;

   if (uname(&buffer) != 0)
      return;

   strlcpy(name, buffer.release, sizeof_name);
}

const frontend_ctx_driver_t frontend_ctx_linux = {
   NULL,                         /* environment_get */
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
   "linux",
};
