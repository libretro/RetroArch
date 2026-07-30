/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
 *  Copyright (C) 2016-2019 - Andrés Suárez
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

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <retro_dirent.h>
#include <string/stdstring.h>
#include <file/config_file.h>
#include <streams/file_stream.h>

#include "../configuration.h"
#include "../file_path_special.h"
#include "../list_special.h"
#include "../verbosity.h"
#include "../input/input_driver.h"
#include "../input/input_remapping.h"

#include "tasks_internal.h"
#ifdef HAVE_BLISSBOX
#include "../input/include/blissbox.h"
#endif

#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#endif

#include "../retroarch.h"
#include "../runloop.h"

typedef struct
{
   char *dir_autoconfig;
   char *dir_driver_autoconfig;
   config_file_t *autoconfig_file;
   /* External scan, carried between ticks.  The directory listing is
    * walked a slice at a time rather than in one go: on a cold cache
    * the eight hundred odd profiles that ship take tens of
    * milliseconds here and far longer on the slow storage a handheld
    * has, and this task runs on the main loop. */
   struct RDIR   *scan_rdir;
   config_file_t *scan_best;
   /* Verified-hint index (see input_autoconfigure_index_try):
    * index_build accumulates identity tuples for the directory
    * currently being scanned so the index can be (re)written when
    * the walk completes; index_build_count numbers its entries. */
   config_file_t *index_build;
   unsigned       index_build_count;
   /* Affinity high-water mark when the current directory's walk
    * began, to tell whether THIS directory produced a match. */
   unsigned       scan_dir_start_affinity;
   unsigned       scan_max_affinity;
   unsigned       scan_dir_idx;
   unsigned       port;
   input_device_info_t device_info; /* unsigned alignment */
   uint8_t flags;
   uint8_t scan_done;
   /* A tuple did not fit its index value buffer, so this
    * directory's index would be incomplete: abandon the build (see
    * input_autoconfigure_index_collect). */
   uint8_t index_build_abandoned;
   /* The current directory's index was fresh (valid header, file
    * count matches) but ranked no candidate at or above the match
    * bar.  If the full walk then agrees that nothing matches, the
    * index was simply right, and rewriting an identical one on
    * every connect of an unrecognised device would be waste. */
   uint8_t index_fresh_no_candidate;
} autoconfig_handle_t;

/*********************/
/* Utility functions */
/*********************/

static void free_autoconfig_handle(autoconfig_handle_t *autoconfig_handle)
{
   if (!autoconfig_handle)
      return;

   if (autoconfig_handle->index_build)
   {
      config_file_free(autoconfig_handle->index_build);
      autoconfig_handle->index_build = NULL;
   }

   /* A scan may be part way through a directory. */
   if (autoconfig_handle->scan_rdir)
   {
      retro_closedir(autoconfig_handle->scan_rdir);
      autoconfig_handle->scan_rdir = NULL;
   }

   if (autoconfig_handle->scan_best)
   {
      config_file_free(autoconfig_handle->scan_best);
      autoconfig_handle->scan_best = NULL;
   }

   if (autoconfig_handle->dir_autoconfig)
   {
      free(autoconfig_handle->dir_autoconfig);
      autoconfig_handle->dir_autoconfig = NULL;
   }

   if (autoconfig_handle->dir_driver_autoconfig)
   {
      free(autoconfig_handle->dir_driver_autoconfig);
      autoconfig_handle->dir_driver_autoconfig = NULL;
   }

   if (autoconfig_handle->autoconfig_file)
   {
      config_file_free(autoconfig_handle->autoconfig_file);
      autoconfig_handle->autoconfig_file = NULL;
   }

   free(autoconfig_handle);
   autoconfig_handle = NULL;
}

static void input_autoconfigure_free(retro_task_t *task)
{
   autoconfig_handle_t *autoconfig_handle = NULL;
   if (task && (autoconfig_handle = (autoconfig_handle_t*)task->state))
      free_autoconfig_handle(autoconfig_handle);
}

/******************************/
/* Autoconfig 'File' Handling */
/******************************/

/* Returns a value corresponding to the
 * 'affinity' between the connected input
 * device and the specified config file
 * > 0: No match
 * > 20-29: Device name matches
 * > 30-39: VID+PID match
 * > 50-59: Both device name and VID+PID match
 * > A physical port match adds 10, a physical port mismatch subtracts 10. */
/* Affinity of one identity tuple (one of a profile's main entry or
 * its up-to-9 alternatives) against the connected device.  Single
 * source of truth: both the per-file computation below and the
 * in-memory index matcher rank with this exact function, so the
 * index can never order candidates differently than a real scan
 * would. */
static unsigned input_autoconfigure_tuple_affinity(
      autoconfig_handle_t *autoconfig_handle,
      uint16_t config_vid, uint16_t config_pid,
      const char *config_device, const char *config_phys,
      int alt)
{
   unsigned affinity = 0;

   /* Check for matching VID+PID */
#ifdef HAVE_BLISSBOX
   /* > Bliss-Box shenanigans: its adapters report the wrapped
    * pad's ids at a fixed pid, so profiles are matched on vid
    * alone with the pid treated as the Bliss-Box constant. */
   if (autoconfig_handle->device_info.vid == BLISSBOX_VID)
      config_pid = BLISSBOX_PID;

   if (     (autoconfig_handle->device_info.vid == config_vid)
         && (autoconfig_handle->device_info.pid == config_pid)
         && (config_vid != 0)
         && (config_pid != 0)
         && (autoconfig_handle->device_info.vid != BLISSBOX_VID)
         && (autoconfig_handle->device_info.pid != BLISSBOX_PID))
      affinity += 30;
#else
   if (     (autoconfig_handle->device_info.vid == config_vid)
         && (autoconfig_handle->device_info.pid == config_pid)
         && (config_vid != 0)
         && (config_pid != 0))
      affinity += 30;
#endif

   /* Check for matching device name */
   if (     config_device
         && *config_device
         && string_is_equal(config_device,
               autoconfig_handle->device_info.name))
      affinity += 20;

   /* Check for matching physical location */
   if (     affinity >= 20
         && config_phys
         && *config_phys)
   {
      if (strstr(autoconfig_handle->device_info.phys, config_phys))
         affinity += 10;
      else
         affinity -= 10;
   }

   /* Store the selected alternative as last digit of affinity. */
   if (affinity > 0)
      affinity += alt;

   return affinity;
}

static unsigned input_autoconfigure_get_config_file_affinity(
      autoconfig_handle_t *autoconfig_handle,
      config_file_t *config)
{
   int i;
   char config_key[30];
   unsigned max_affinity           = 0;

   /* One main entry and up to 9 alternatives */
   for (i = 0; i < 10; i++)
   {
      size_t _len;
      char config_key_postfix[7];
      struct config_entry_list *entry = NULL;
      uint16_t config_vid = 0;
      uint16_t config_pid = 0;
      int tmp_int         = 0;
      unsigned affinity   = 0;
      const char *config_device = NULL;
      const char *config_phys   = NULL;

      if (i == 0)
         config_key_postfix[0] = '\0';
      else
         snprintf(config_key_postfix, sizeof(config_key_postfix),
                  "_alt%d",i);

      /* Parse config file */
      _len  = strlcpy(config_key, "input_vendor_id",
               sizeof(config_key));
      strlcpy(config_key  + _len, config_key_postfix,
            sizeof(config_key) - _len);
      if (config_get_int(config, config_key, &tmp_int))
         config_vid = (uint16_t)tmp_int;

      _len  = strlcpy(config_key, "input_product_id",
               sizeof(config_key));
      strlcpy(config_key  + _len, config_key_postfix,
               sizeof(config_key) - _len);
      if (config_get_int(config, config_key, &tmp_int))
         config_pid = (uint16_t)tmp_int;

      _len  = strlcpy(config_key, "input_device",
               sizeof(config_key));
      strlcpy(config_key  + _len, config_key_postfix,
            sizeof(config_key) - _len);
      if (     (entry  = config_get_entry(config, config_key))
            && (entry->value))
         config_device = entry->value;

      _len  = strlcpy(config_key, "input_phys",
               sizeof(config_key));
      _len += strlcpy(config_key + _len, config_key_postfix,
               sizeof(config_key) - _len);
      if (     (entry = config_get_entry(config, config_key))
            && (entry->value))
         config_phys = entry->value;

      affinity = input_autoconfigure_tuple_affinity(
            autoconfig_handle, config_vid, config_pid,
            config_device, config_phys, i);

      if (max_affinity < affinity)
         max_affinity = affinity;
   }

   return max_affinity;
}

/* 'Attaches' specified autoconfig file to autoconfig
 * handle, parsing required device info metadata */
static void input_autoconfigure_set_config_file(
      autoconfig_handle_t *autoconfig_handle,
      config_file_t *config, unsigned alternative)
{
   size_t _len;
   char config_key[32];
   struct config_entry_list *entry    = NULL;

   /* Attach config file */
   autoconfig_handle->autoconfig_file = config;

   /* > Extract config file path + name */
   if ((config->path && *config->path))
   {
      const char *config_file_name = path_basename_nocompression(config->path);
      if (config_file_name && *config_file_name)
         strlcpy(autoconfig_handle->device_info.config_name,
               config_file_name,
               sizeof(autoconfig_handle->device_info.config_name));
   }

   /* Parse config file */
   _len  = strlcpy(config_key, "input_device_display_name",
            sizeof(config_key));
   /* Read device display name */
   if (alternative > 0)
      snprintf(config_key + _len, sizeof(config_key) - _len,
               "_alt%d",alternative);

   if (  (entry = config_get_entry(config, config_key))
         && (entry->value && *entry->value))
      strlcpy(autoconfig_handle->device_info.display_name,
            entry->value,
            sizeof(autoconfig_handle->device_info.display_name));

   /* Set auto-configured status to 'true' */
   autoconfig_handle->device_info.autoconfigured = true;
}

/* Attempts to find an 'external' autoconfig file
 * (in the autoconfig directory) matching the connected
 * input device
 * > Returns 'true' if successful */

/*******************************/
/* Verified-hint profile index */
/*******************************/

/* A per-directory index of profile identity tuples, so that a device
 * connect can rank every profile with two file operations (read the
 * index, open the winner) instead of one open per profile - the
 * historical slowness of this task is exactly that per-file open
 * latency on cold or slow storage.
 *
 * The index is a hint, never an authority:
 *  - the ranking uses input_autoconfigure_tuple_affinity, the same
 *    function the real scan uses, over tuples recorded verbatim;
 *  - the winning candidate is opened and re-scored against its real
 *    file contents, and accepted only if the real affinity is at
 *    least what the index claimed;
 *  - anything else - no index, unparseable index, stale file count,
 *    winner unreadable, size drift, verify shortfall, or no
 *    candidate reaching the same >= 20 bar a scan match needs -
 *    falls back to the full directory scan, which also rebuilds the
 *    index from data it necessarily reads anyway.
 * A stale index can therefore cost one wasted open, but can never
 * select a profile the full scan would not have selected.
 *
 * The file lives beside the profiles it describes (no .cfg extension,
 * so the scan's extension filter never sees it).  If the directory is
 * not writable the write fails silently and every connect simply
 * performs the full scan, exactly as before this mechanism existed.
 * Freshness is keyed on the directory's *.cfg count (the VFS layer
 * exposes no mtime): additions and removals are caught by the count,
 * a renamed or deleted winner by the failed open, an edited winner by
 * the re-score, and a profile hand-edited to match a previously
 * unmatched device by the no-candidate fallback.  The one blind spot
 * is an in-place edit that would promote a profile that was neither
 * the winner nor previously acceptable; deleting the index file (or
 * any add/remove in the directory) clears it. */

#define AUTOCONFIG_INDEX_NAME    ".autoconfig_index"
#define AUTOCONFIG_INDEX_VERSION 1

/* Count the *.cfg entries in a directory: two getdents syscalls,
 * no per-file opens.  Must apply the same filter as the scan walk
 * so the count is comparable. */
static int input_autoconfigure_index_dir_count(const char *dir)
{
   struct RDIR *rdir;
   int count = 0;

   if (!(rdir = retro_opendir(dir)))
      return -1;

   while (retro_readdir(rdir))
   {
      const char *entry_name = retro_dirent_get_name(rdir);
      if (     entry_name
            && *entry_name
            && string_is_equal_noncase(
                  path_get_extension(entry_name), "cfg"))
         count++;
   }

   retro_closedir(rdir);
   return count;
}

/* Record one profile's identity tuples into the index being built.
 * Reads exactly the keys input_autoconfigure_get_config_file_affinity
 * reads, and stores them verbatim (tab-separated inside the quoted
 * value; a device name containing a tab or quote would corrupt only
 * its own entry, which the verify step then rejects). */
static void input_autoconfigure_index_collect(
      autoconfig_handle_t *autoconfig_handle,
      const char *entry_name, int64_t f_size, config_file_t *config)
{
   int i;
   char index_key[32];
   char index_val[NAME_MAX_LENGTH + 128];
   unsigned file_idx = autoconfig_handle->index_build_count;

   if (autoconfig_handle->index_build_abandoned)
      return;

   if (!autoconfig_handle->index_build)
   {
      if (!(autoconfig_handle->index_build = config_file_new_alloc()))
         return;
      autoconfig_handle->index_build->flags |=
            CONF_FILE_FLG_GUARANTEED_NO_DUPLICATES;
   }

   snprintf(index_key, sizeof(index_key), "f%u", file_idx);
   config_set_string(autoconfig_handle->index_build, index_key,
         entry_name);
   snprintf(index_key, sizeof(index_key), "s%u", file_idx);
   /* Written as int to stay symmetric with the config_get_int the
    * verify step reads it back with; a profile anywhere near that
    * bound is not a profile.  Oversized files record 0, which the
    * size-drift check treats as 'unknown' and skips. */
   snprintf(index_val, sizeof(index_val), "%d",
         (f_size > 0 && f_size <= 0x7fffffff) ? (int)f_size : 0);
   config_set_string(autoconfig_handle->index_build, index_key,
         index_val);

   for (i = 0; i < 10; i++)
   {
      size_t _len;
      char config_key[30];
      char config_key_postfix[7];
      struct config_entry_list *entry = NULL;
      uint16_t config_vid       = 0;
      uint16_t config_pid       = 0;
      int tmp_int               = 0;
      const char *config_device = "";
      const char *config_phys   = "";

      if (i == 0)
         config_key_postfix[0] = '\0';
      else
         snprintf(config_key_postfix, sizeof(config_key_postfix),
                  "_alt%d", i);

      _len  = strlcpy(config_key, "input_vendor_id",
               sizeof(config_key));
      strlcpy(config_key + _len, config_key_postfix,
            sizeof(config_key) - _len);
      if (config_get_int(config, config_key, &tmp_int))
         config_vid = (uint16_t)tmp_int;

      _len  = strlcpy(config_key, "input_product_id",
               sizeof(config_key));
      strlcpy(config_key + _len, config_key_postfix,
            sizeof(config_key) - _len);
      if (config_get_int(config, config_key, &tmp_int))
         config_pid = (uint16_t)tmp_int;

      _len  = strlcpy(config_key, "input_device",
               sizeof(config_key));
      strlcpy(config_key + _len, config_key_postfix,
            sizeof(config_key) - _len);
      if (     (entry = config_get_entry(config, config_key))
            && (entry->value))
         config_device = entry->value;

      _len  = strlcpy(config_key, "input_phys",
               sizeof(config_key));
      strlcpy(config_key + _len, config_key_postfix,
            sizeof(config_key) - _len);
      if (     (entry = config_get_entry(config, config_key))
            && (entry->value))
         config_phys = entry->value;

      /* Absent alternatives are not stored */
      if (     (i > 0)
            && (config_vid == 0)
            && (config_pid == 0)
            && (*config_device == '\0'))
         continue;

      snprintf(index_key, sizeof(index_key), "i%u_%d", file_idx, i);
      /* Device names and physical locations come from the file and
       * are not length-bounded, so the composed tuple can overflow
       * this buffer.  A truncated tuple must never be stored: the
       * verify step only catches an index that *overstates* a
       * candidate (real < claimed), and truncation can just as
       * easily understate one - a clipped physical location can
       * drop the +10 phys bonus, and a clipped separator makes the
       * parser skip the tuple entirely, hiding the profile from
       * ranking.  Either way the index could rank the true winner
       * below a rival whose own claim is honest, so the rival would
       * verify successfully and be selected where a full scan would
       * not have chosen it.  Abandon the whole index for this
       * directory instead; connects then scan in full, exactly as
       * they did before this mechanism existed. */
      if ((size_t)snprintf(index_val, sizeof(index_val),
               "%u\t%u\t%s\t%s",
               (unsigned)config_vid, (unsigned)config_pid,
               config_device, config_phys) >= sizeof(index_val))
      {
         autoconfig_handle->index_build_abandoned = 1;
         config_file_free(autoconfig_handle->index_build);
         autoconfig_handle->index_build           = NULL;
         autoconfig_handle->index_build_count     = 0;
         return;
      }
      config_set_string(autoconfig_handle->index_build, index_key,
            index_val);
   }

   autoconfig_handle->index_build_count++;
}

/* Write the accumulated index beside the profiles.  Failure (read
 * only directory, out of space) is ignored: the index is an
 * optimisation, and without one every connect scans as before. */
static void input_autoconfigure_index_write(
      autoconfig_handle_t *autoconfig_handle, const char *dir)
{
   char index_path[PATH_MAX_LENGTH];
   char index_val[32];
   config_file_t *index_build = autoconfig_handle->index_build;

   if (!index_build)
      return;

   snprintf(index_val, sizeof(index_val), "%d",
         AUTOCONFIG_INDEX_VERSION);
   config_set_string(index_build, "__version", index_val);
   snprintf(index_val, sizeof(index_val), "%u",
         autoconfig_handle->index_build_count);
   config_set_string(index_build, "__file_count", index_val);

   fill_pathname_join_special(index_path, dir,
         AUTOCONFIG_INDEX_NAME, sizeof(index_path));
   index_build->flags |= CONF_FILE_FLG_MODIFIED;
   config_file_write(index_build, index_path, false);

   config_file_free(index_build);
   autoconfig_handle->index_build       = NULL;
   autoconfig_handle->index_build_count = 0;
}

/* Try to resolve the connect from the directory's index.  On a
 * verified hit, returns the winning profile's parsed config with
 * *affinity set to its (re-verified) affinity.  Returns NULL on any
 * miss or doubt, and the caller performs the full scan. */
static config_file_t *input_autoconfigure_index_try(
      autoconfig_handle_t *autoconfig_handle, const char *dir,
      unsigned *affinity)
{
   char index_path[PATH_MAX_LENGTH];
   config_file_t *index_conf = NULL;
   config_file_t *winner     = NULL;
   int file_count            = 0;
   int version               = 0;
   int best_file             = -1;
   unsigned best_affinity    = 0;
   unsigned fi;

   fill_pathname_join_special(index_path, dir,
         AUTOCONFIG_INDEX_NAME, sizeof(index_path));
   if (!(index_conf = config_file_new(index_path)))
      return NULL;

   /* Header and freshness */
   if (     !config_get_int(index_conf, "__version", &version)
         || (version != AUTOCONFIG_INDEX_VERSION)
         || !config_get_int(index_conf, "__file_count", &file_count)
         || (file_count <= 0)
         || (file_count !=
               input_autoconfigure_index_dir_count(dir)))
   {
      config_file_free(index_conf);
      return NULL;
   }

   /* Rank every recorded tuple with the real affinity function */
   for (fi = 0; fi < (unsigned)file_count; fi++)
   {
      int i;
      char index_key[32];
      for (i = 0; i < 10; i++)
      {
         struct config_entry_list *entry;
         const char *p;
         char *endp;
         char config_device[NAME_MAX_LENGTH];
         unsigned config_vid, config_pid, a;
         const char *config_phys;
         size_t device_len;

         snprintf(index_key, sizeof(index_key), "i%u_%d", fi, i);
         if (     !(entry = config_get_entry(index_conf, index_key))
               || !entry->value)
            continue;

         /* vid \t pid \t device \t phys */
         p          = entry->value;
         config_vid = (unsigned)strtoul(p, &endp, 10);
         if (*endp != '\t')
            continue;
         p          = endp + 1;
         config_pid = (unsigned)strtoul(p, &endp, 10);
         if (*endp != '\t')
            continue;
         p          = endp + 1;
         if (!(endp = strchr(p, '\t')))
            continue;
         device_len = (size_t)(endp - p);
         if (device_len >= sizeof(config_device))
            device_len = sizeof(config_device) - 1;
         memcpy(config_device, p, device_len);
         config_device[device_len] = '\0';
         config_phys = endp + 1;

         a = input_autoconfigure_tuple_affinity(autoconfig_handle,
               (uint16_t)config_vid, (uint16_t)config_pid,
               config_device, config_phys, i);
         if (a > best_affinity)
         {
            best_affinity = a;
            best_file     = (int)fi;
         }
      }
   }

   /* The same bar a scan match needs; below it, scan in full (this
    * is also what finds a profile hand-edited to newly match).
    * The header was valid and current, so flag it: if the scan
    * agrees nothing matches, the index needs no rewrite. */
   if ((best_affinity < 20) || (best_file < 0))
   {
      autoconfig_handle->index_fresh_no_candidate = 1;
      config_file_free(index_conf);
      return NULL;
   }

   /* Open only the winner and verify the claim against the file */
   {
      char index_key[32];
      char profile_path[PATH_MAX_LENGTH];
      struct config_entry_list *entry;
      int64_t f_size    = 0;
      int64_t f_len     = 0;
      char *f_buf       = NULL;
      unsigned real_affinity;

      snprintf(index_key, sizeof(index_key), "f%d", best_file);
      if (     !(entry = config_get_entry(index_conf, index_key))
            || !entry->value
            || !*entry->value)
      {
         config_file_free(index_conf);
         return NULL;
      }
      fill_pathname_join_special(profile_path, dir, entry->value,
            sizeof(profile_path));

      snprintf(index_key, sizeof(index_key), "s%d", best_file);
      {
         int tmp_int = 0;
         if (config_get_int(index_conf, index_key, &tmp_int))
            f_size = (int64_t)tmp_int;
      }
      config_file_free(index_conf);

      if (     !filestream_read_file(profile_path,
                  (void**)&f_buf, &f_len)
            || !f_buf)
         return NULL;

      /* Size drift: the file changed since indexing */
      if ((f_size > 0) && (f_len != f_size))
      {
         free(f_buf);
         return NULL;
      }

      if (!(winner = config_file_new_take_string(f_buf, 0,
            profile_path)))
         return NULL;

      real_affinity = input_autoconfigure_get_config_file_affinity(
            autoconfig_handle, winner);
      if (real_affinity < best_affinity)
      {
         /* The index lied (in-place edit): scan in full instead */
         config_file_free(winner);
         return NULL;
      }

      *affinity = real_affinity;
      return winner;
   }
}

/* How many directory entries one tick will look at.
 *
 * Each is an open, a read and a close - cheap warm, and the dominant
 * cost cold or on slow storage, where it is per-file rather than per
 * byte.  This task runs on the main loop, so the whole listing is not
 * walked in one go. */
#define AUTOCONFIG_SCAN_ENTRIES_PER_TICK 48

/* Advance the external scan by one slice.
 *
 * Returns false while there is more to do, in which case the caller
 * returns and is called again on the next tick.  Returns true when the
 * scan is finished, with autoconfig_handle->scan_best holding the best
 * profile found, if any. */
static bool input_autoconfigure_scan_config_files_external(
      autoconfig_handle_t *autoconfig_handle)
{
   const char *dir_autoconfig        = autoconfig_handle->dir_autoconfig;
   const char *dir_driver_autoconfig = autoconfig_handle->dir_driver_autoconfig;
   const char *dirs[2];
   unsigned    num_dirs = 0;
   unsigned    budget   = AUTOCONFIG_SCAN_ENTRIES_PER_TICK;

   if (     (dir_autoconfig && *dir_autoconfig)
         && path_is_directory(dir_autoconfig))
      dirs[num_dirs++] = dir_autoconfig;

   if (     (dir_driver_autoconfig && *dir_driver_autoconfig)
         && path_is_directory(dir_driver_autoconfig))
      dirs[num_dirs++] = dir_driver_autoconfig;

   while (budget > 0)
   {
      const char *entry_name;
      char        config_file_path[PATH_MAX_LENGTH];
      config_file_t *config = NULL;
      unsigned    affinity  = 0;

      if (autoconfig_handle->scan_dir_idx >= num_dirs)
         break;                            /* every directory walked */

      if (!autoconfig_handle->scan_rdir)
      {
         const char *dir = dirs[autoconfig_handle->scan_dir_idx];

         /* Entering a new directory: try its index first.  A
          * verified hit answers the connect with two file
          * operations instead of one open per profile.  Any miss
          * or doubt falls through to the full walk below, which
          * rebuilds the index as it goes. */
         budget--;
         autoconfig_handle->index_fresh_no_candidate = 0;
         autoconfig_handle->index_build_abandoned    = 0;
         autoconfig_handle->scan_dir_start_affinity  =
               autoconfig_handle->scan_max_affinity;
         if ((config = input_autoconfigure_index_try(
               autoconfig_handle, dir, &affinity)))
         {
            if (autoconfig_handle->scan_best)
               config_file_free(autoconfig_handle->scan_best);
            autoconfig_handle->scan_best         = config;
            autoconfig_handle->scan_max_affinity = affinity;
            /* Same policy as a scan match: a directory that
             * produced one ends the search. */
            autoconfig_handle->scan_dir_idx      = num_dirs;
            break;
         }

         if (!(autoconfig_handle->scan_rdir = retro_opendir(dir)))
         {
            autoconfig_handle->scan_dir_idx++;
            continue;
         }
      }

      if (!retro_readdir(autoconfig_handle->scan_rdir))
      {
         retro_closedir(autoconfig_handle->scan_rdir);
         autoconfig_handle->scan_rdir = NULL;
         /* The walk read and parsed every profile: write the
          * rebuilt index beside them (silently a no-op on a read
          * only directory) - unless a fresh index already said,
          * correctly, that nothing here matches this device, in
          * which case the rebuilt one is identical and the write
          * is skipped.  A scan that *contradicts* a fresh index
          * (a profile was hand-edited in place to match) does
          * write, healing the staleness the header cannot see. */
         if (     !autoconfig_handle->index_fresh_no_candidate
               || (     (autoconfig_handle->scan_max_affinity >=
                          20)
                     && (autoconfig_handle->scan_max_affinity >
                          autoconfig_handle->scan_dir_start_affinity)))
            input_autoconfigure_index_write(autoconfig_handle,
                  dirs[autoconfig_handle->scan_dir_idx]);
         else if (autoconfig_handle->index_build)
         {
            config_file_free(autoconfig_handle->index_build);
            autoconfig_handle->index_build       = NULL;
            autoconfig_handle->index_build_count = 0;
         }
         autoconfig_handle->scan_dir_idx++;
         /* A directory that produced a match ends the search; the
          * later directory is only a fallback. */
         if (autoconfig_handle->scan_best)
            autoconfig_handle->scan_dir_idx = num_dirs;
         continue;
      }

      budget--;

      entry_name = retro_dirent_get_name(autoconfig_handle->scan_rdir);
      if (     (!entry_name || !*entry_name)
            || !string_is_equal_noncase(
                  path_get_extension(entry_name), "cfg"))
         continue;

      fill_pathname_join_special(config_file_path,
            dirs[autoconfig_handle->scan_dir_idx], entry_name,
            sizeof(config_file_path));

      /* Read and parse every profile.  An earlier revision skipped
       * parsing files a raw-buffer prefilter rejected; the index
       * rebuild needs every file's identity tuples regardless, the
       * bytes are already in memory either way, and a parse costs
       * microseconds against the milliseconds the read itself costs
       * cold.  Ranking parsed values also retires the prefilter's
       * substring heuristics, which were a standing source of
       * false-negative encodings. */
      {
         int64_t  buf_len = 0;
         char    *buf     = NULL;

         if (!filestream_read_file(config_file_path,
                  (void**)&buf, &buf_len) || !buf)
            continue;

         if (!(config = config_file_new_take_string(buf, 0,
               config_file_path)))
            continue;

         input_autoconfigure_index_collect(autoconfig_handle,
               entry_name, buf_len, config);
      }

      affinity = input_autoconfigure_get_config_file_affinity(
            autoconfig_handle, config);

      if (affinity > autoconfig_handle->scan_max_affinity)
      {
         if (autoconfig_handle->scan_best)
            config_file_free(autoconfig_handle->scan_best);

         autoconfig_handle->scan_best         = config;
         config                               = NULL;
         autoconfig_handle->scan_max_affinity = affinity;

         /* A vendor, product and physical location match is as good
          * as it gets; nothing later can beat it.  The partial
          * tuple collection is discarded rather than written - a
          * device this profile fits will hit the >= 60 early exit
          * with or without an index, so rebuilding is left to a
          * connect that walks the whole directory. */
         if (affinity >= 60)
         {
            retro_closedir(autoconfig_handle->scan_rdir);
            autoconfig_handle->scan_rdir    = NULL;
            if (autoconfig_handle->index_build)
            {
               config_file_free(autoconfig_handle->index_build);
               autoconfig_handle->index_build       = NULL;
               autoconfig_handle->index_build_count = 0;
            }
            autoconfig_handle->scan_dir_idx = num_dirs;
            break;
         }
      }
      else
      {
         config_file_free(config);
         config = NULL;
      }
   }

   if (autoconfig_handle->scan_dir_idx < num_dirs)
      return false;                        /* resume on the next tick */

   if (autoconfig_handle->scan_best)
   {
      /* Not a bare assignment: this attaches the file, records its
       * name, and reads the display name for whichever alternative
       * matched - which is the digit the affinity carries in its
       * units place. */
      input_autoconfigure_set_config_file(autoconfig_handle,
            autoconfig_handle->scan_best,
            autoconfig_handle->scan_max_affinity % 10);
      autoconfig_handle->scan_best = NULL;
   }

   RARCH_DBG("[Autoconf] Config files scanned: driver \"%s\", name \"%s\" (%04x/%04x), phys \"%s\", affinity %d.\n",
         autoconfig_handle->device_info.joypad_driver,
         autoconfig_handle->device_info.name,
         autoconfig_handle->device_info.vid,
         autoconfig_handle->device_info.pid,
         autoconfig_handle->device_info.phys,
         autoconfig_handle->scan_max_affinity);

   return true;
}

static bool input_autoconfigure_scan_config_files_internal(
      autoconfig_handle_t *autoconfig_handle)
{
   size_t i;

   /* Loop through internal autoconfig files
    * > input_builtin_autoconfs is a static const,
    *   and may be read safely in any thread  */
   for (i = 0; input_builtin_autoconfs[i]; i++)
   {
      char *autoconfig_str  = NULL;
      config_file_t *config = NULL;
      unsigned affinity     = 0;

      if (!input_builtin_autoconfs[i] || !*input_builtin_autoconfs[i])
         continue;

      /* Load autoconfig string.  The strdup stays (the builtin is
       * const and the parse mutates), but the copy is handed over:
       * the conf adopts it and entries borrow from it. */
      autoconfig_str = strdup(input_builtin_autoconfs[i]);
      config         = config_file_new_take_string(
            autoconfig_str, 0, NULL);
      autoconfig_str = NULL;

      /* Check for a match */
      if (autoconfig_handle && config)
         affinity = input_autoconfigure_get_config_file_affinity(
               autoconfig_handle, config);

      /* > In the case of internal autoconfigs, any kind
       *   of match is considered to be a success */
      if (affinity > 0)
      {
         if (autoconfig_handle && config)
            input_autoconfigure_set_config_file(
                  autoconfig_handle, config,
                  affinity % 10);
         return true;
      }

      /* No match - clean up */
      if (config)
      {
         config_file_free(config);
         config = NULL;
      }
   }

   return false;
}

/* Reallocate the automatically assigned player <-> port mapping if needed.
 * Objectives:
 * - if there is reservation for the device, assign it to the reserved player
 * - when assigning a new device to a reserved port, move the previous entry
 *      to first free slot if it was occupied
 * - use first free player port by default for new entries (overriding saved
 *      input_joypad_index, as it can
 *      get quite messy if reservations are done, due to the swaps above)
 * - do not consider "reserved" ports free
 * - if there is no reservation, do not change anything
 *      (not even the assignment to first free player port)
 */
static void reallocate_port_if_needed(
      unsigned detected_port,
      unsigned int vendor_id,
      unsigned int product_id,
      const char *device_name,
      const char *device_display_name)
{
   int player;
   char settings_value[NAME_MAX_LENGTH];
   char settings_value_device_name[NAME_MAX_LENGTH];
   unsigned prev_assigned_player_slots[MAX_USERS];
   unsigned int settings_value_vendor_id  = 0;
   unsigned int settings_value_product_id = 0;
   unsigned first_free_player_slot        = MAX_USERS + 1;
   bool device_has_reserved_slot          = false;
   bool no_reservation_at_all             = true;
   settings_t *settings                   = config_get_ptr();

   if (detected_port >= MAX_USERS)
      return;

   /* The swaps below are transpositions of input_joypad_index[]: they
    * preserve a one player <-> one pad mapping but cannot restore one,
    * so a mapping that arrives with two players on the same pad keeps
    * both of them there on every subsequent hotplug, and a single
    * controller drives two libretro ports. Assert the invariant before
    * relying on it. */
   input_config_sanitize_joypad_indices();

   /* MAX_USERS marks a pad index that no player is mapped to. Zero
    * initialising this would instead claim such pads for player 1,
    * making the reassignment below write to an unrelated slot. */
   for (player = 0; player < MAX_USERS; player++)
      prev_assigned_player_slots[player] = MAX_USERS;

   for (player = 0; player < MAX_USERS; player++)
   {
      if (     first_free_player_slot > MAX_USERS
            && (detected_port == settings->uints.input_joypad_index[player]
            || !input_config_get_device_name(settings->uints.input_joypad_index[player]))
            && settings->uints.input_device_reservation_type[player]
            != INPUT_DEVICE_RESERVATION_RESERVED)
      {
         first_free_player_slot = player;
         RARCH_DBG("[Autoconf] First unconfigured / unreserved player is %d.\n",
                   player+1);
      }
      prev_assigned_player_slots[settings->uints.input_joypad_index[player]] = player;
      if (settings->uints.input_device_reservation_type[player] != INPUT_DEVICE_RESERVATION_NONE)
         no_reservation_at_all = false;
   }
   /* 'input_max_users' is a count, so a slot index equal to it is
    * already out of range; assigning a pad there leaves the device
    * mapped to a disabled player and silently unusable. */
   if (first_free_player_slot >= settings->uints.input_max_users)
   {
      RARCH_ERR("[Autoconf] No free and unreserved player slots found for adding new device"
            " \"%s\"! Detected port %d, max_users: %d, first free slot %d.\n",
            device_name, detected_port,
            settings->uints.input_max_users,
            first_free_player_slot+1);
      if (prev_assigned_player_slots[detected_port] < MAX_USERS)
         RARCH_WARN("[Autoconf] Leaving detected player slot in place: %d.\n",
               prev_assigned_player_slots[detected_port]);
      return;
   }

   /* Both reassignment branches below write through
    * prev_assigned_player_slots[detected_port]. The sanitisation above
    * makes the mapping total, so this cannot trigger; it keeps the
    * writes in bounds if that ever stops holding. */
   if (prev_assigned_player_slots[detected_port] >= MAX_USERS)
      return;

   for (player = 0; player < MAX_USERS; player++)
   {
      if (settings->uints.input_device_reservation_type[player] != INPUT_DEVICE_RESERVATION_NONE)
         strlcpy(settings_value, settings->arrays.input_reserved_devices[player],
                 sizeof(settings_value));
      else
         settings_value[0] = '\0';

      if (*settings_value)
      {
         char *endptr;
         char *colon;
         unsigned long parsed_vid;
         unsigned long parsed_pid;

         RARCH_DBG("[Autoconf] Examining reserved device for player %d "
                   "type %d: \"%s\" against \"%04x:%04x\".\n",
                   player+1,
                   settings->uints.input_device_reservation_type[player],
                   settings_value, vendor_id, product_id);

         colon      = strchr(settings_value, ':');
         parsed_vid = strtoul(settings_value, &endptr, 16);

         if (colon && endptr == colon)
         {
            parsed_pid = strtoul(colon + 1, &endptr, 16);
            if (endptr != colon + 1 && (*endptr == ' ' || *endptr == '\0'))
            {
               settings_value_vendor_id  = (unsigned int)parsed_vid;
               settings_value_product_id = (unsigned int)parsed_pid;
               device_has_reserved_slot  = (  vendor_id  == settings_value_vendor_id
                                           && product_id == settings_value_product_id);
            }
            else
            {
               strlcpy(settings_value_device_name, settings_value,
                       sizeof(settings_value_device_name));
               device_has_reserved_slot =
                     string_is_equal(device_name, settings_value_device_name)
                  || string_is_equal(device_display_name, settings_value_device_name);
            }
         }
         else
         {
            strlcpy(settings_value_device_name, settings_value,
                    sizeof(settings_value_device_name));
            device_has_reserved_slot =
                  string_is_equal(device_name, settings_value_device_name)
               || string_is_equal(device_display_name, settings_value_device_name);
         }

         if (device_has_reserved_slot)
         {
            unsigned prev_assigned_port = settings->uints.input_joypad_index[player];
            const char *a = input_config_get_device_name(prev_assigned_port);
            if (     detected_port != prev_assigned_port
                 && (a && *a)
                 && (( settings_value_vendor_id  == input_config_get_device_vid(prev_assigned_port)
                 && settings_value_product_id == input_config_get_device_pid(prev_assigned_port))
                 || strcmp(a, settings_value_device_name) == 0))
            {
               RARCH_DBG("[Autoconf] Same type of device already took this slot, continuing search...\n");
               device_has_reserved_slot = false;
            }
            else
            {
               RARCH_DBG("[Autoconf] Reserved device matched.\n");
               break;
            }
         }
      }
   }

   if (device_has_reserved_slot)
   {
      unsigned prev_assigned_port = settings->uints.input_joypad_index[player];
      if (detected_port != prev_assigned_port)
      {
         RARCH_LOG("[Autoconf] Device \"%s\" (%x:%x) is reserved "
                   "for player %d, updating.\n",
                   device_name, vendor_id, product_id, player+1);

         /* todo: fix the pushed info message */
         settings->uints.input_joypad_index[player] = detected_port;

         RARCH_LOG("[Autoconf] Preferred slot was taken earlier by "
                   "\"%s\", reassigning that to %d.\n",
                    input_config_get_device_name(prev_assigned_port),
                    prev_assigned_player_slots[detected_port]+1);
         settings->uints.input_joypad_index[prev_assigned_player_slots[detected_port]] = prev_assigned_port;
         if (input_config_get_device_name(prev_assigned_port))
         {
            unsigned prev_assigned_port_l2 = settings->uints.input_joypad_index[first_free_player_slot];

            RARCH_LOG("[Autoconf] 2nd level reassignment, moving "
                      "previously assigned port %d to first free player %d.\n",
                      prev_assigned_port_l2, first_free_player_slot+1);
            settings->uints.input_joypad_index[prev_assigned_player_slots[detected_port]] = prev_assigned_port_l2;
            settings->uints.input_joypad_index[first_free_player_slot]                    = prev_assigned_port;
         }
      }
      else
      {
         RARCH_DBG("[Autoconf] Device \"%s\" (%x:%x) is reserved for "
                   "player %d, same as default assignment.\n",
                   device_name, vendor_id, product_id, player+1);
      }
   }
   else
   {
      unsigned prev_assigned_port;

      RARCH_DBG("[Autoconf] Device \"%s\" (%x:%x) is not reserved for "
            "any player slot.\n",
            device_name, vendor_id, product_id);
      if (   no_reservation_at_all
            || prev_assigned_player_slots[detected_port] == first_free_player_slot)
         return;

      prev_assigned_port = settings->uints.input_joypad_index[first_free_player_slot];
      settings->uints.input_joypad_index[first_free_player_slot] = detected_port;
      settings->uints.input_joypad_index[prev_assigned_player_slots[detected_port]] =
         prev_assigned_port;
      RARCH_DBG("[Autoconf] Earlier free player slot found, "
            "reassigning to player %d.\n",
            first_free_player_slot+1);
   }
}

/*************************/
/* Autoconfigure Connect */
/*************************/

static void cb_input_autoconfigure_connect(
      retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   unsigned port;
   autoconfig_handle_t *autoconfig_handle = NULL;

   if (!task)
      return;

   if (!(autoconfig_handle = (autoconfig_handle_t*)task->state))
      return;

   /* Use local copy of port index for brevity... */
   port = autoconfig_handle->port;

   /* We perform the actual 'connect' in this
    * callback, to ensure it occurs on the main
    * thread */

   /* Copy task handle parameters into global
    * state objects:
    * > Name */
   if (*autoconfig_handle->device_info.name)
      input_config_set_device_name(port,
            autoconfig_handle->device_info.name);
   else
      input_config_clear_device_name(port);

   /* > Display name */
   if (*autoconfig_handle->device_info.display_name)
      input_config_set_device_display_name(port,
            autoconfig_handle->device_info.display_name);
   else if (*autoconfig_handle->device_info.name)
      input_config_set_device_display_name(port,
            autoconfig_handle->device_info.name);
   else
      input_config_clear_device_display_name(port);

   /* > Driver */
   if (*autoconfig_handle->device_info.joypad_driver)
      input_config_set_device_joypad_driver(port,
            autoconfig_handle->device_info.joypad_driver);
   else
      input_config_clear_device_joypad_driver(port);

   /* > VID/PID */
   input_config_set_device_vid(port, autoconfig_handle->device_info.vid);
   input_config_set_device_pid(port, autoconfig_handle->device_info.pid);

   if (*autoconfig_handle->device_info.config_name)
      input_config_set_device_config_name(port,
            autoconfig_handle->device_info.config_name);
   else
      input_config_set_device_config_name(port,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE));

   /* > Auto-configured state */
   input_config_set_device_autoconfigured(port,
         autoconfig_handle->device_info.autoconfigured);

   /* Reset any existing binds */
   input_config_reset_autoconfig_binds(port);

   /* If an autoconfig file is available, load its
    * bind mappings */
   if (autoconfig_handle->device_info.autoconfigured)
      input_config_set_autoconfig_binds(port,
            autoconfig_handle->autoconfig_file);

#ifdef HAVE_CONFIGFILE
   /* 'Sort Remaps by Gamepad' must reload remaps after
    * controller detection, because controller name
    * does not exist yet at core init when launched from CLI */
   if (!retroarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
   {
      settings_t *settings = config_get_ptr();

      if (     settings->bools.auto_remaps_enable
            && settings->bools.input_remap_sort_by_controller_enable)
      {
         runloop_state_t *runloop_st = runloop_state_get_ptr();

         config_load_remap(settings->paths.directory_input_remapping, &runloop_st->system);
      }
   }
#endif

   reallocate_port_if_needed(port,autoconfig_handle->device_info.vid,
         autoconfig_handle->device_info.pid,
         autoconfig_handle->device_info.name,
         autoconfig_handle->device_info.display_name);
}

static void input_autoconfigure_connect_handler(retro_task_t *task)
{
   char task_title[NAME_MAX_LENGTH + 16];
   autoconfig_handle_t *autoconfig_handle = NULL;
   bool match_found                       = false;
   const char *device_display_name        = NULL;

   task_title[0] = '\0';

   if (!task)
      return;

   autoconfig_handle = (autoconfig_handle_t*)task->state;

   if (   !autoconfig_handle
       || !*autoconfig_handle->device_info.name
       || !(autoconfig_handle->flags & AUTOCONF_FLAG_AUTOCONFIG_ENABLED))
   {
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
      return;
   }

   /* Annoyingly, we have to scan all the autoconfig
    * files (and in-built configs) in a single shot
    * > Would prefer to scan one config per iteration
    *   of the task, but this would render the gamepad
    *   unusable for multiple frames after loading
    *   content... */

   /* Scan in order of preference:
    * - External autoconfig files
    * - Internal autoconfig definitions */
   /* The external scan walks the profile directory a slice at a time;
    * until it reports itself finished there is nothing else to do this
    * tick.  Everything below runs once, on the tick that completes it. */
   if (!autoconfig_handle->scan_done)
   {
      if (!input_autoconfigure_scan_config_files_external(autoconfig_handle))
         return;
      autoconfig_handle->scan_done = 1;
   }

   if (!(match_found = (autoconfig_handle->autoconfig_file != NULL)))
      match_found = input_autoconfigure_scan_config_files_internal(
         autoconfig_handle);

   /* If no match was found, attempt to use
    * fallback mapping
    * > Only enabled for certain drivers */
   if (!match_found)
   {
      const char *fallback_device_name = NULL;

      /* Preset fallback device names - must match
       * those set in 'input_autodetect_builtin.c' */
      if (string_is_equal(autoconfig_handle->device_info.joypad_driver,
            "android"))
         fallback_device_name = "Android Gamepad";
      else if (string_is_equal(autoconfig_handle->device_info.joypad_driver,
            "xinput"))
         fallback_device_name = "XInput Controller";
      else if (string_is_equal(autoconfig_handle->device_info.joypad_driver,
            "sdl2"))
         fallback_device_name = "Standard Gamepad";
      else if (string_is_equal(autoconfig_handle->device_info.joypad_driver,
            "sdl3"))
         fallback_device_name = "Gamepad";
#ifdef HAVE_TEST_DRIVERS
      else if (string_is_equal(autoconfig_handle->device_info.joypad_driver,
            "test"))
         fallback_device_name = "Test Gamepad";
#endif
      if (   (fallback_device_name && *fallback_device_name)
          && !string_is_equal(autoconfig_handle->device_info.name,
               fallback_device_name))
      {
         /* Save/restore device_info.name around the fallback-name
          * autoconfig scan.  device_info.name is a fixed-size
          * char[128] ivar, not a heap pointer, so the previous
          * strdup + free was unnecessary heap traffic, and carried
          * a NULL-deref-on-OOM failure mode (strdup returning NULL
          * was not checked before strlcpy(name, name_backup, ...)
          * below).  A stack buffer of matching size sidesteps both
          * issues. */
         char name_backup[sizeof(autoconfig_handle->device_info.name)];
         bool fallback_matched;

         strlcpy(name_backup, autoconfig_handle->device_info.name,
               sizeof(name_backup));

         strlcpy(autoconfig_handle->device_info.name,
               fallback_device_name,
               sizeof(autoconfig_handle->device_info.name));

         /* Apply the fallback built-in profile. */
         fallback_matched = input_autoconfigure_scan_config_files_internal(autoconfig_handle);

         strlcpy(autoconfig_handle->device_info.name,
               name_backup,
               sizeof(autoconfig_handle->device_info.name));

         if (fallback_matched && (autoconfig_handle->flags & AUTOCONF_FLAG_HAS_STANDARD_MAPPING))
            match_found = true;
      }
   }

   /* Get display name for task status message */
   device_display_name = autoconfig_handle->device_info.display_name;
   if (!device_display_name || !*device_display_name)
      device_display_name = autoconfig_handle->device_info.name;
   if (!device_display_name || !*device_display_name)
      device_display_name = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE);

   /* Generate task status message
    * > Note that 'connection successful' messages
    *   may be suppressed, but error messages are
    *   always shown */
   task->style = TASK_STYLE_NEGATIVE;
   if (autoconfig_handle->device_info.autoconfigured)
   {
      /* Successful addition style */
      task->style = TASK_STYLE_POSITIVE;

      if (match_found)
      {
         /* A valid autoconfig was applied */
         if (!(autoconfig_handle->flags & AUTOCONF_FLAG_SUPPRESS_NOTIFICATIONS))
            snprintf(task_title, sizeof(task_title),
                  msg_hash_to_str(MSG_DEVICE_CONFIGURED_IN_PORT_NR),
                  device_display_name,
                  autoconfig_handle->port + 1);
      }
      /* Device is autoconfigured, but a (most likely
       * incorrect) fallback definition was used... */
      else if (!(autoconfig_handle->flags & AUTOCONF_FLAG_SUPPRESS_FAILURE_NOTIF))
         snprintf(task_title, sizeof(task_title),
                  msg_hash_to_str(MSG_DEVICE_NOT_CONFIGURED_FALLBACK_NR),
                  device_display_name,
                  autoconfig_handle->device_info.vid,
                  autoconfig_handle->device_info.pid);
   }
   /* Autoconfig failed */
   else if (!(autoconfig_handle->flags & AUTOCONF_FLAG_SUPPRESS_FAILURE_NOTIF))
         snprintf(task_title, sizeof(task_title),
                  msg_hash_to_str(MSG_DEVICE_NOT_CONFIGURED_NR),
                  device_display_name,
                  autoconfig_handle->device_info.vid,
                  autoconfig_handle->device_info.pid);

   /* Update task title */
   task_free_title(task);
   if (*task_title)
   {
      task_set_title(task, strdup(task_title));
      RARCH_LOG("[Autoconf] %s.\n", task_title);
   }

   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static bool autoconfigure_connect_finder(retro_task_t *task, void *user_data)
{
   autoconfig_handle_t *autoconfig_handle = NULL;
   unsigned *port                         = NULL;

   if (!task || !user_data)
      return false;

   if (task->handler != input_autoconfigure_connect_handler)
      return false;

   autoconfig_handle = (autoconfig_handle_t*)task->state;
   if (!autoconfig_handle)
      return false;

   port = (unsigned*)user_data;
   return (*port == autoconfig_handle->port);
}

bool input_autoconfigure_connect(
      const char *name,
      const char *display_name,
      const char *phys,
      const char *driver,
      unsigned port,
      unsigned vid,
      unsigned pid)
{
   return input_autoconfigure_connect_ex(name, display_name, phys,
         driver, port, vid, pid, 0);
}

/**
 * Queues an asynchronous task to autoconfigure a newly connected device.
 *
 * @param name Device name reported by the driver, used for name-based profile matching. May be NULL.
 * @param display_name Human-readable name, or NULL to use the matched profile.
 * @param phys Physical location string (e.g. USB path), or NULL.
 * @param driver Joypad driver identifier (e.g. "sdl3", "udev").
 * @param port Input port to configure (0 .. MAX_INPUT_DEVICES-1).
 * @param vid USB vendor ID, or 0 if unknown.
 * @param pid USB product ID, or 0 if unknown.
 * @param flags An enum autoconfig_handle_flags bitmask seeding the initial state.
 *
 * @return true if the autoconfigure task was queued, false otherwise.
 * @see input_autoconfigure_connect()
 */
bool input_autoconfigure_connect_ex(
      const char *name,
      const char *display_name,
      const char *phys,
      const char *driver,
      unsigned port,
      unsigned vid,
      unsigned pid,
      uint8_t flags)
{
   task_finder_data_t find_data;
   retro_task_t *task                     = NULL;
   autoconfig_handle_t *autoconfig_handle = NULL;
   bool driver_valid                      = false;
   settings_t *settings                   = config_get_ptr();
   bool autoconfig_enabled                = settings ?
         settings->bools.input_autodetect_enable : false;
   const char *dir_autoconfig             = settings ?
         settings->paths.directory_autoconfig : NULL;
   bool notification_show_autoconfig      = settings ?
         settings->bools.notification_show_autoconfig : true;
   bool notification_show_autoconfig_fails = settings ?
         settings->bools.notification_show_autoconfig_fails : true;

   if (port >= MAX_INPUT_DEVICES)
      return false;

   /* Cannot connect a device that is currently
    * being connected */
   find_data.func     = autoconfigure_connect_finder;
   find_data.userdata = (void*)&port;

   if (task_queue_find(&find_data))
      return false;

   /* Configure handle */
   if (!(autoconfig_handle = (autoconfig_handle_t*)
            calloc(1, sizeof(autoconfig_handle_t))))
      return false;

   autoconfig_handle->port                         = port;
   autoconfig_handle->device_info.vid              = vid;
   autoconfig_handle->device_info.pid              = pid;
   autoconfig_handle->device_info.name[0]          = '\0';
   autoconfig_handle->device_info.display_name[0]  = '\0';
   autoconfig_handle->device_info.phys[0]          = '\0';
   autoconfig_handle->device_info.config_name[0]   = '\0';
   autoconfig_handle->device_info.joypad_driver[0] = '\0';
   autoconfig_handle->device_info.autoconfigured   = false;
   autoconfig_handle->device_info.name_index       = 0;
   autoconfig_handle->flags                        = flags;
   if (autoconfig_enabled)
      autoconfig_handle->flags |= AUTOCONF_FLAG_AUTOCONFIG_ENABLED;
   if (!notification_show_autoconfig)
      autoconfig_handle->flags |= AUTOCONF_FLAG_SUPPRESS_NOTIFICATIONS;
   if (!notification_show_autoconfig_fails)
      autoconfig_handle->flags |= AUTOCONF_FLAG_SUPPRESS_FAILURE_NOTIF;
   autoconfig_handle->dir_autoconfig               = NULL;
   autoconfig_handle->dir_driver_autoconfig        = NULL;
   autoconfig_handle->autoconfig_file              = NULL;

   if (name && *name)
      strlcpy(autoconfig_handle->device_info.name, name,
            sizeof(autoconfig_handle->device_info.name));

   if (display_name && *display_name)
      strlcpy(autoconfig_handle->device_info.display_name, display_name,
            sizeof(autoconfig_handle->device_info.display_name));

   if (phys && *phys)
       strlcpy(autoconfig_handle->device_info.phys, phys,
             sizeof(autoconfig_handle->device_info.phys));

   if ((driver_valid = (driver && *driver)))
      strlcpy(autoconfig_handle->device_info.joypad_driver,
            driver, sizeof(autoconfig_handle->device_info.joypad_driver));

   /* > Have to cache both the base autoconfig directory
    *   and the driver-specific autoconfig directory
    *   - Driver-specific directory is scanned by
    *     default, if available
    *   - If driver-specific directory is unavailable,
    *     we scan the base autoconfig directory as
    *     a fallback */
   if (dir_autoconfig && *dir_autoconfig)
   {
      autoconfig_handle->dir_autoconfig = strdup(dir_autoconfig);

      if (driver_valid)
      {
         char dir_driver_autoconfig[DIR_MAX_LENGTH];
         /* Generate driver-specific autoconfig directory */
         fill_pathname_join_special(dir_driver_autoconfig,
               dir_autoconfig,
               autoconfig_handle->device_info.joypad_driver,
               sizeof(dir_driver_autoconfig));

         if (*dir_driver_autoconfig)
            autoconfig_handle->dir_driver_autoconfig =
                  strdup(dir_driver_autoconfig);
      }
   }

#ifdef HAVE_BLISSBOX
   /* Bliss-Box shenanigans... */
   if (autoconfig_handle->device_info.vid == BLISSBOX_VID)
      input_autoconfigure_blissbox_override_handler(
            (int)autoconfig_handle->device_info.vid,
            (int)autoconfig_handle->device_info.pid,
            autoconfig_handle->device_info.name,
            sizeof(autoconfig_handle->device_info.name));
#endif

   /* If we are reconnecting a device that is already
    * connected and autoconfigured, then there is no need
    * to generate additional 'connection successful'
    * task status messages
    * > Can skip this check if autoconfig notifications
    *   have been disabled by the user */
   if (   !(autoconfig_handle->flags & AUTOCONF_FLAG_SUPPRESS_NOTIFICATIONS)
       && (*autoconfig_handle->device_info.name))
   {
      const char *last_device_name = input_config_get_device_name(port);
      uint16_t last_vid            = input_config_get_device_vid(port);
      uint16_t last_pid            = input_config_get_device_pid(port);
      bool last_autoconfigured     = input_config_get_device_autoconfigured(port);

      if (  (last_device_name && *last_device_name)
          && string_is_equal(autoconfig_handle->device_info.name,
               last_device_name)
          && (autoconfig_handle->device_info.vid == last_vid)
          && (autoconfig_handle->device_info.pid == last_pid)
          && last_autoconfigured)
         autoconfig_handle->flags |= AUTOCONF_FLAG_SUPPRESS_NOTIFICATIONS;
   }

   /* Configure task */
   if (!(task = task_init()))
   {
      free_autoconfig_handle(autoconfig_handle);
      return false;
   }

   task->handler  = input_autoconfigure_connect_handler;
   task->state    = autoconfig_handle;
   task->title    = NULL;
   task->callback = cb_input_autoconfigure_connect;
   task->cleanup  = input_autoconfigure_free;
   task->flags   &= ~RETRO_TASK_FLG_MUTE;

   task_queue_push(task);

   return true;
}

/****************************/
/* Autoconfigure Disconnect */
/****************************/

static void cb_input_autoconfigure_disconnect(
      retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   unsigned port;
   autoconfig_handle_t *autoconfig_handle = NULL;

   if (!task)
      return;

   if (!(autoconfig_handle = (autoconfig_handle_t*)task->state))
      return;

   /* Use local copy of port index for brevity... */
   port = autoconfig_handle->port;

   /* We perform the actual 'disconnect' in this
    * callback, to ensure it occurs on the main thread */
   input_config_clear_device_name(port);
   input_config_clear_device_display_name(port);
   input_config_clear_device_config_name(port);
   input_config_clear_device_joypad_driver(port);
   input_config_set_device_vid(port, 0);
   input_config_set_device_pid(port, 0);
   input_config_set_device_autoconfigured(port, false);
   input_config_reset_autoconfig_binds(port);
}

static void input_autoconfigure_disconnect_handler(retro_task_t *task)
{
   autoconfig_handle_t *autoconfig_handle = NULL;

   if (!task)
      return;

   if ((autoconfig_handle = (autoconfig_handle_t*)task->state))
   {
      char task_title[NAME_MAX_LENGTH + 16];
      const char *device_display_name = NULL;
      /* Removal style */
      task->style = TASK_STYLE_NEGATIVE;

      /* Get display name for task status message */
      device_display_name = autoconfig_handle->device_info.display_name;
      if (!device_display_name || !*device_display_name)
         device_display_name = autoconfig_handle->device_info.name;
      if (!device_display_name || !*device_display_name)
         device_display_name = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE);

      /* Set task title */
      snprintf(task_title, sizeof(task_title),
            msg_hash_to_str(MSG_DEVICE_DISCONNECTED_FROM_PORT_NR),
            device_display_name,
            autoconfig_handle->port + 1);

      task_free_title(task);
      if (!(autoconfig_handle->flags & AUTOCONF_FLAG_SUPPRESS_NOTIFICATIONS))
         task_set_title(task, strdup(task_title));
      if (*task_title)
         RARCH_LOG("[Autoconf] %s.\n", task_title);
   }

   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static bool autoconfigure_disconnect_finder(retro_task_t *task, void *user_data)
{
   autoconfig_handle_t *autoconfig_handle = NULL;
   unsigned *port                         = NULL;

   if (!task || !user_data)
      return false;

   if (task->handler != input_autoconfigure_disconnect_handler)
      return false;

   if (!(autoconfig_handle = (autoconfig_handle_t*)task->state))
      return false;

   port = (unsigned*)user_data;
   return (*port == autoconfig_handle->port);
}

/* Note: There is no real need for autoconfigure
 * 'disconnect' to be a task - we are merely setting
 * a handful of variables. However:
 * - Making it a task means we can call
 *   input_autoconfigure_disconnect() on any thread
 *   thread, and defer the global state changes until
 *   the task queue is handled on the *main* thread
 * - By using a task for both 'connect' and 'disconnect',
 *   we ensure uniformity of OSD status messages */
bool input_autoconfigure_disconnect(unsigned port, const char *name)
{
   task_finder_data_t find_data;
   retro_task_t *task                     = NULL;
   autoconfig_handle_t *autoconfig_handle = NULL;
   settings_t *settings                   = config_get_ptr();
   input_driver_state_t *input_st         = input_state_get_ptr();
   bool notification_show_autoconfig      = settings ? settings->bools.notification_show_autoconfig : true;
   bool pause_on_disconnect               = settings ? settings->bools.pause_on_disconnect : true;
   bool menu_pause_libretro               = settings ? settings->bools.menu_pause_libretro : false;
   bool core_is_running                   = (runloop_state_get_ptr()->flags & RUNLOOP_FLAG_CORE_RUNNING) ? true : false;

   if (port >= MAX_INPUT_DEVICES)
      return false;

   /* Cannot disconnect a device that is currently
    * being disconnected */
   find_data.func     = autoconfigure_disconnect_finder;
   find_data.userdata = (void*)&port;

   if (task_queue_find(&find_data))
      return false;

   /* Configure handle */
   autoconfig_handle = (autoconfig_handle_t*)calloc(1, sizeof(autoconfig_handle_t));

   if (!autoconfig_handle)
      return false;

   autoconfig_handle->port      = port;
   if (!notification_show_autoconfig)
      autoconfig_handle->flags |= AUTOCONF_FLAG_SUPPRESS_NOTIFICATIONS;

   /* Use display_name as name instead since autoconfig display_name
    * is destroyed already, and real name does not matter at this point */
   if (input_st && *input_st->input_device_info[port].display_name)
      strlcpy(autoconfig_handle->device_info.name,
            input_st->input_device_info[port].display_name,
            sizeof(autoconfig_handle->device_info.name));
   else if (name && *name)
      strlcpy(autoconfig_handle->device_info.name,
            name, sizeof(autoconfig_handle->device_info.name));

   /* Configure task */
   if (!(task = task_init()))
   {
      free_autoconfig_handle(autoconfig_handle);
      return false;
   }

   task->handler  = input_autoconfigure_disconnect_handler;
   task->state    = autoconfig_handle;
   task->title    = NULL;
   task->callback = cb_input_autoconfigure_disconnect;
   task->cleanup  = input_autoconfigure_free;

   task_queue_push(task);

   if (pause_on_disconnect && core_is_running)
   {
#ifdef HAVE_MENU
      bool menu_is_alive = (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE)
         ? true : false;
      if (menu_pause_libretro)
      {
         if (!menu_is_alive)
            command_event(CMD_EVENT_MENU_TOGGLE, NULL);
      }
      else
         command_event(CMD_EVENT_PAUSE, NULL);
#else
      command_event(CMD_EVENT_PAUSE, NULL);
#endif
   }

   return true;
}
