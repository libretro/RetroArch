/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - gavin_darkglider
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <boolean.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <lists/dir_list.h>
#include <lists/string_list.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "reboot2payload.h"
#include "../../configuration.h"
#include "../../verbosity.h"

#define R2P_SYSFS_DIR      "/sys/devices/r2p"
#define R2P_SYSFS_ACTION   R2P_SYSFS_DIR "/action"
#define R2P_SYSFS_PARAM1   R2P_SYSFS_DIR "/param1"
#define R2P_SYSFS_PARAM2   R2P_SYSFS_DIR "/param2"

/* Lakka's initramfs always mounts the FAT boot partition at /flash. */
#define R2P_BOOT_ROOT      "/flash"
#define R2P_HEKATE_IPL     R2P_BOOT_ROOT "/bootloader/hekate_ipl.ini"
#define R2P_HEKATE_INIDIR  R2P_BOOT_ROOT "/bootloader/ini"

#define R2P_CMDLINE_PATH   "/proc/cmdline"
#define R2P_DEFAULT_ACTION "self"
#define R2P_SELF_SUFFIX    " (self)"
#define R2P_HEKATE_MENU    "Hekate Menu"

enum r2p_payload_kind
{
   R2P_KIND_BOOTLOADER = 0, /* action=bootloader              */
   R2P_KIND_LAUNCH,         /* action=self, param2=0 (ipl.ini) */
   R2P_KIND_INI_CONFIG      /* action=self, param2=1 (ini/)    */
};

typedef struct r2p_payload
{
   char name[R2P_NAME_MAXLEN];
   enum r2p_payload_kind kind;
   unsigned index;
} r2p_payload_t;

static r2p_payload_t *r2p_list             = NULL;
static size_t         r2p_list_count       = 0;
static size_t         r2p_list_capacity    = 0;
static size_t         r2p_selected         = 0;
static bool           r2p_list_initialized = false;

/* pmc_r2p.* values from /proc/cmdline: the entry Lakka was booted
 * from. Seeds the default selection and feeds r2p_arm_self(). */
static char     r2p_self_action[16] = R2P_DEFAULT_ACTION;
static unsigned r2p_self_param1     = 0;
static unsigned r2p_self_param2     = 0;

static bool r2p_append_payload(const char *name,
      enum r2p_payload_kind kind, unsigned index)
{
   r2p_payload_t *slot;

   if (string_is_empty(name))
      return false;

   if (r2p_list_count >= r2p_list_capacity)
   {
      size_t new_cap       = (r2p_list_capacity == 0)
            ? 8 : r2p_list_capacity * 2;
      r2p_payload_t *grown = (r2p_payload_t*)realloc(r2p_list,
            new_cap * sizeof(*grown));
      if (!grown)
         return false;
      r2p_list             = grown;
      r2p_list_capacity    = new_cap;
   }

   slot        = &r2p_list[r2p_list_count++];
   strlcpy(slot->name, name, sizeof(slot->name));
   slot->kind  = kind;
   slot->index = index;
   return true;
}

static char *r2p_trim(char *s)
{
   char *end;
   while (*s && isspace((unsigned char)*s))
      s++;
   end = s + strlen(s);
   while (end > s && isspace((unsigned char)end[-1]))
      end--;
   *end = '\0';
   return s;
}

static void r2p_parse_cmdline(void)
{
   RFILE *fp;
   char   line[1024];
   char  *tok;
   char  *saveptr = NULL;

   strlcpy(r2p_self_action, R2P_DEFAULT_ACTION, sizeof(r2p_self_action));
   r2p_self_param1 = 0;
   r2p_self_param2 = 0;

   if (!(fp = filestream_open(R2P_CMDLINE_PATH,
               RETRO_VFS_FILE_ACCESS_READ,
               RETRO_VFS_FILE_ACCESS_HINT_NONE)))
      return;

   if (!filestream_gets(fp, line, sizeof(line)))
   {
      filestream_close(fp);
      return;
   }
   filestream_close(fp);

   for (tok  = strtok_r(line, " \t\r\n", &saveptr);
        tok;
        tok  = strtok_r(NULL, " \t\r\n", &saveptr))
   {
      if (string_starts_with(tok, "pmc_r2p.action="))
         strlcpy(r2p_self_action, tok + STRLEN_CONST("pmc_r2p.action="),
               sizeof(r2p_self_action));
      else if (string_starts_with(tok, "pmc_r2p.param1="))
         r2p_self_param1 = (unsigned)strtoul(
               tok + STRLEN_CONST("pmc_r2p.param1="), NULL, 0);
      else if (string_starts_with(tok, "pmc_r2p.param2="))
         r2p_self_param2 = (unsigned)strtoul(
               tok + STRLEN_CONST("pmc_r2p.param2="), NULL, 0);
   }
}

/* Appends one entry per [section] of a Hekate INI, numbering them
 * from *index_inout the same way Hekate does. */
static void r2p_parse_ini_file(const char *ini_path,
      enum r2p_payload_kind kind, unsigned *index_inout)
{
   char   line[256];
   RFILE *fp = filestream_open(ini_path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!fp)
      return;

   while (filestream_gets(fp, line, sizeof(line)))
   {
      char  *section = r2p_trim(line);
      size_t _len    = strlen(section);

      if (_len < 2 || section[0] != '[' || section[_len - 1] != ']')
         continue;

      section[_len - 1] = '\0';
      section           = r2p_trim(section + 1);

      /* [config] holds Hekate's global options, not a boot entry. */
      if (     string_is_empty(section)
            || string_is_equal_case_insensitive(section, "config"))
         continue;

      r2p_append_payload(section, kind, *index_inout);
      (*index_inout)++;
   }

   filestream_close(fp);
}

static void r2p_clear_payloads(void)
{
   free(r2p_list);
   r2p_list          = NULL;
   r2p_list_count    = 0;
   r2p_list_capacity = 0;
}

/* List index of the entry Lakka was booted from, or 0 (Hekate menu)
 * when no parsed entry matches the kernel command line. */
static size_t r2p_index_for_self(void)
{
   size_t i;
   enum r2p_payload_kind want_kind;

   if (!string_is_equal_case_insensitive(r2p_self_action, "self"))
      return 0;

   want_kind = (r2p_self_param2 == 1)
      ? R2P_KIND_INI_CONFIG
      : R2P_KIND_LAUNCH;

   for (i = 0; i < r2p_list_count; i++)
      if (     r2p_list[i].kind  == want_kind
            && r2p_list[i].index == r2p_self_param1)
         return i;

   return 0;
}

static void r2p_refresh_payloads(void)
{
   unsigned launch_index = 1;
   unsigned cfg_index    = 1;

   r2p_clear_payloads();
   r2p_parse_cmdline();

   r2p_append_payload(R2P_HEKATE_MENU, R2P_KIND_BOOTLOADER, 0);

   if (path_is_valid(R2P_HEKATE_IPL))
      r2p_parse_ini_file(R2P_HEKATE_IPL, R2P_KIND_LAUNCH, &launch_index);

   if (path_is_directory(R2P_HEKATE_INIDIR))
   {
      struct string_list *ini_files = dir_list_new(R2P_HEKATE_INIDIR,
            "ini", false, false, false, false);
      if (ini_files)
      {
         size_t i;
         dir_list_sort(ini_files, true);
         for (i = 0; i < ini_files->size; i++)
            r2p_parse_ini_file(ini_files->elems[i].data,
                  R2P_KIND_INI_CONFIG, &cfg_index);
         string_list_free(ini_files);
      }
   }

   r2p_selected         = r2p_index_for_self();
   r2p_list_initialized = true;

   RARCH_LOG("[R2P] %u entries loaded; cmdline action=%s "
         "param1=%u param2=%u, default idx=%u.\n",
         (unsigned)r2p_list_count, r2p_self_action,
         r2p_self_param1, r2p_self_param2,
         (unsigned)r2p_selected);
}

static void r2p_ensure_initialized(void)
{
   if (!r2p_list_initialized)
      r2p_refresh_payloads();
}

bool r2p_is_supported(void)
{
   return path_is_directory(R2P_SYSFS_DIR);
}

void r2p_load_selection(const struct settings *settings)
{
   size_t i;
   unsigned    saved_kind;
   const char *saved_name;

   r2p_refresh_payloads();

   if (!settings)
      return;

   saved_kind = settings->uints.reboot_payload_kind;
   saved_name = settings->arrays.reboot_payload_name;

   if (string_is_empty(saved_name))
      return;

   for (i = 0; i < r2p_list_count; i++)
   {
      if (     (unsigned)r2p_list[i].kind == saved_kind
            && string_is_equal(r2p_list[i].name, saved_name))
      {
         r2p_selected = i;
         return;
      }
   }

   RARCH_WARN("[R2P] Saved payload \"%s\" not found, "
         "using the running entry.\n", saved_name);
}

void r2p_cycle_selection(struct settings *settings, bool forward)
{
   const r2p_payload_t *p;

   r2p_ensure_initialized();

   if (r2p_list_count == 0)
      return;

   if (forward)
      r2p_selected = (r2p_selected + 1) % r2p_list_count;
   else
      r2p_selected = (r2p_selected == 0)
         ? r2p_list_count - 1
         : r2p_selected - 1;

   if (!settings)
      return;

   p = &r2p_list[r2p_selected];
   settings->uints.reboot_payload_kind = (unsigned)p->kind;
   strlcpy(settings->arrays.reboot_payload_name, p->name,
         sizeof(settings->arrays.reboot_payload_name));
}

const char *r2p_get_destination_name(void)
{
   static char buf[R2P_NAME_MAXLEN + sizeof(R2P_SELF_SUFFIX)];
   const r2p_payload_t *p;

   r2p_ensure_initialized();

   if (r2p_selected >= r2p_list_count)
      return R2P_HEKATE_MENU;

   p = &r2p_list[r2p_selected];
   if (r2p_selected != r2p_index_for_self())
      return p->name;

   snprintf(buf, sizeof(buf), "%s%s", p->name, R2P_SELF_SUFFIX);
   return buf;
}

static bool r2p_write_sysfs(const char *action,
      const char *param1, const char *param2)
{
   if (!filestream_write_file(R2P_SYSFS_ACTION, action, strlen(action)))
   {
      RARCH_ERR("[R2P] Failed to write \"%s\".\n", R2P_SYSFS_ACTION);
      return false;
   }
   if (!filestream_write_file(R2P_SYSFS_PARAM1, param1, strlen(param1)))
   {
      RARCH_ERR("[R2P] Failed to write \"%s\".\n", R2P_SYSFS_PARAM1);
      return false;
   }
   if (!filestream_write_file(R2P_SYSFS_PARAM2, param2, strlen(param2)))
   {
      RARCH_ERR("[R2P] Failed to write \"%s\".\n", R2P_SYSFS_PARAM2);
      return false;
   }
   return true;
}

bool r2p_arm_selected(void)
{
   const r2p_payload_t *p;
   const char          *action = "self";
   char                 param1[16];
   const char          *param2 = "0";

   r2p_ensure_initialized();

   if (r2p_selected >= r2p_list_count)
      return false;

   p = &r2p_list[r2p_selected];
   snprintf(param1, sizeof(param1), "%u", p->index);

   switch (p->kind)
   {
      case R2P_KIND_BOOTLOADER:
         action = "bootloader";
         break;
      case R2P_KIND_LAUNCH:
         break;
      case R2P_KIND_INI_CONFIG:
         param2 = "1";
         break;
      default:
         return false;
   }

   if (!r2p_write_sysfs(action, param1, param2))
      return false;

   RARCH_LOG("[R2P] Armed action=%s param1=%s param2=%s (\"%s\").\n",
         action, param1, param2, p->name);
   return true;
}

bool r2p_arm_self(void)
{
   char param1[16];
   char param2[16];

   r2p_ensure_initialized();

   snprintf(param1, sizeof(param1), "%u", r2p_self_param1);
   snprintf(param2, sizeof(param2), "%u", r2p_self_param2);

   if (!r2p_write_sysfs(r2p_self_action, param1, param2))
      return false;

   RARCH_LOG("[R2P] Armed self action=%s param1=%s param2=%s.\n",
         r2p_self_action, param1, param2);
   return true;
}

void r2p_deinit(void)
{
   r2p_clear_payloads();
   r2p_selected         = 0;
   r2p_list_initialized = false;
}
