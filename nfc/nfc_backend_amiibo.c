/*  RetroArch - A frontend for libretro.
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

/* ===================================================================
 * "amiibo directory" software backend (the default reader)
 *
 * A virtual reader served from a directory of raw amiibo/NTAG dumps.
 * Point it at a folder with the Amiibo directory setting (default:
 * <system>/amiibo; RETRO_NFC_AMIIBO_DIR overrides when neither is set). It serves one dump as the tag
 * currently in the field, chosen either by index from the enumerated list
 * (nfc_amiibo_source_*) or by absolute path from RetroArch's amiibo file
 * picker (nfc_amiibo_select_path). Dumps are listed by file name.
 *
 * The directory is scanned recursively and files that are not amiibo-sized
 * are skipped. Writes go to the in-memory image and are flushed to the dump
 * file, so a title's amiibo save-back persists.
 * =================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <retro_dirent.h>
#include <string/stdstring.h>

#include "nfc_backend.h"
#include "../verbosity.h"

#define NFC_AMIIBO_SCAN_MAX_DEPTH 8
#define NFC_AMIIBO_MIN_BYTES      540  /* smallest real amiibo dump (NTAG215) */
#define NFC_AMIIBO_MAX_BYTES      924  /* largest NTAG21x image (NTAG216)    */

typedef struct
{
   char path[1024];
   char name[64];
} amiibo_entry_t;

/* Grown on demand so a large library (hundreds of dumps across nested
 * folders) is fully enumerated rather than capped. */
static amiibo_entry_t *amiibo_list      = NULL;
static unsigned        amiibo_list_cap  = 0;
static unsigned        amiibo_list_len  = 0;
static int      amiibo_selected  = -1;   /* index into amiibo_list, or -1  */
static bool     amiibo_reswap    = false; /* signal a fresh tag placement  */
static char     amiibo_pick[1024] = "";   /* explicit path (file picker)   */

static uint8_t  amiibo_buf[NFC_MAX_TAG_BYTES];
static unsigned amiibo_size      = 0;
static bool     amiibo_scanning  = false;
static bool     amiibo_present   = false;
static char     amiibo_path[1024];
static char     amiibo_cfg_dir[1024] = "";

void nfc_amiibo_set_dir(const char *dir)
{
   if (dir && *dir)
      strlcpy(amiibo_cfg_dir, dir, sizeof(amiibo_cfg_dir));
   else
      amiibo_cfg_dir[0] = '\0';
}

static const char *amiibo_dir(void)
{
   const char *env;
   if (amiibo_cfg_dir[0])
      return amiibo_cfg_dir;
   env = getenv("RETRO_NFC_AMIIBO_DIR");
   if (env && *env)
      return env;
   return "amiibo";
}

static bool has_bin_ext(const char *name)
{
   size_t n = strlen(name);
   return n > 4 && string_is_equal_case_insensitive(name + n - 4, ".bin");
}

/* scan counters, reset per rescan */
static unsigned scan_added, scan_skipped;

static bool amiibo_list_grow(void)
{
   unsigned ncap = amiibo_list_cap ? amiibo_list_cap * 2 : 64;
   amiibo_entry_t *n = (amiibo_entry_t*)realloc(amiibo_list, ncap * sizeof(*n));
   if (!n)
      return false;
   amiibo_list     = n;
   amiibo_list_cap = ncap;
   return true;
}

/* Validate one candidate .bin by size and append it, named by file name.
 * Non-amiibo files (keys, stray data) are logged and skipped. */
static void amiibo_try_add(const char *path)
{
   amiibo_entry_t *e;
   const char *base;
   size_t n;
   long   sz;
   FILE  *f = fopen(path, "rb");

   if (!f)
   {
      scan_skipped++;
      RARCH_WARN("[NFC] amiibo: cannot open \"%s\".\n", path);
      return;
   }
   fseek(f, 0, SEEK_END);
   sz = ftell(f);
   fclose(f);
   if (sz < NFC_AMIIBO_MIN_BYTES || sz > NFC_AMIIBO_MAX_BYTES)
   {
      scan_skipped++;
      RARCH_LOG("[NFC] amiibo: skipping \"%s\" (%ld bytes, not an amiibo dump).\n",
            path, sz);
      return;
   }

   if (amiibo_list_len == amiibo_list_cap && !amiibo_list_grow())
   {
      RARCH_ERR("[NFC] amiibo: out of memory building the tag list.\n");
      return;
   }
   e = &amiibo_list[amiibo_list_len];
   strlcpy(e->path, path, sizeof(e->path));

   base = strrchr(path, '/');
   base = base ? base + 1 : path;
   strlcpy(e->name, base, sizeof(e->name));
   n = strlen(e->name);
   if (n > 4 && string_is_equal_case_insensitive(e->name + n - 4, ".bin"))
      e->name[n - 4] = '\0';

   amiibo_list_len++;
   scan_added++;
}

/* Recurse into the amiibo directory, adding every *.bin dump found. */
static void amiibo_scan_dir(const char *dir, int depth)
{
   struct RDIR *d = retro_opendir(dir);
   if (!d)
      return;
   while (retro_readdir(d))
   {
      const char *nm = retro_dirent_get_name(d);
      char sub[1024];
      if (!nm || nm[0] == '.')            /* skip ".", "..", hidden */
         continue;
      fill_pathname_join_special(sub, dir, nm, sizeof(sub));
      if (retro_dirent_is_dir(d, NULL))
      {
         if (depth < NFC_AMIIBO_SCAN_MAX_DEPTH)
            amiibo_scan_dir(sub, depth + 1);
      }
      else if (has_bin_ext(nm))
         amiibo_try_add(sub);
   }
   retro_closedir(d);
}

static void amiibo_rescan(void)
{
   const char *dir = amiibo_dir();
   const char *sel = getenv("RETRO_NFC_AMIIBO");
   char  keep[1024];
   int   want = -1;

   keep[0] = '\0';
   if (amiibo_selected >= 0 && (unsigned)amiibo_selected < amiibo_list_len)
      strlcpy(keep, amiibo_list[amiibo_selected].path, sizeof(keep));

   amiibo_list_len = 0;
   scan_added = scan_skipped = 0;

   RARCH_LOG("[NFC] amiibo: scanning \"%s\" for dumps.\n", dir);
   amiibo_scan_dir(dir, 0);
   RARCH_LOG("[NFC] amiibo: %u dump(s) found, %u file(s) skipped.\n",
         amiibo_list_len, scan_skipped);
   if (amiibo_list_len == 0)
      RARCH_WARN("[NFC] amiibo: no dumps under \"%s\" - check the Amiibo directory.\n",
            dir);

   /* re-resolve the selection: previous path, else RETRO_NFC_AMIIBO, else 0 */
   if (keep[0])
   {
      unsigned i;
      for (i = 0; i < amiibo_list_len; i++)
         if (string_is_equal(amiibo_list[i].path, keep))
            { want = (int)i; break; }
   }
   if (want < 0 && sel && *sel && !strchr(sel, '/') && !strchr(sel, '\\'))
   {
      unsigned i;
      for (i = 0; i < amiibo_list_len; i++)
      {
         const char *b = strrchr(amiibo_list[i].path, '/');
         b = b ? b + 1 : amiibo_list[i].path;
         if (string_is_equal(b, sel))
            { want = (int)i; break; }
      }
   }
   /* No auto-load: leave nothing selected unless a previous selection, the
    * file picker, or RETRO_NFC_AMIIBO chose one. The tag is placed on the
    * reader explicitly. */
   amiibo_selected = want;
}

/* --- enumeration + selection, consumed by the frontend dispatcher ------ */

unsigned nfc_amiibo_source_count(void)
{
   amiibo_rescan();
   return amiibo_list_len;
}

bool nfc_amiibo_source_name(unsigned index, char *out, size_t out_len)
{
   if (index >= amiibo_list_len)
      return false;
   strlcpy(out, amiibo_list[index].name, out_len);
   return true;
}

bool nfc_amiibo_source_select(unsigned index)
{
   if (amiibo_list_len == 0)
      amiibo_rescan();
   if (index >= amiibo_list_len)
      return false;
   amiibo_selected = (int)index;
   amiibo_pick[0]  = '\0';   /* an index selection overrides a picked path */
   amiibo_reswap   = true;   /* present it as a freshly-placed tag */
   RARCH_LOG("[NFC] amiibo backend: selected \"%s\".\n",
         amiibo_list[index].name);
   return true;
}

/* Serve a specific dump by absolute path (RetroArch's amiibo file picker),
 * independent of the scanned list. */
bool nfc_amiibo_select_path(const char *path)
{
   if (!path || !*path)
      return false;
   strlcpy(amiibo_pick, path, sizeof(amiibo_pick));
   amiibo_reswap = true;
   RARCH_LOG("[NFC] amiibo backend: picked \"%s\".\n", path);
   return true;
}

/* --- tag image I/O ----------------------------------------------------- */

static bool amiibo_load(void)
{
   long   sz;
   size_t got;
   FILE  *f = fopen(amiibo_path, "rb");
   if (!f)
      return false;
   fseek(f, 0, SEEK_END);
   sz = ftell(f);
   fseek(f, 0, SEEK_SET);
   if (sz <= 0 || sz > NFC_MAX_TAG_BYTES)
   {
      fclose(f);
      return false;
   }
   got = fread(amiibo_buf, 1, (size_t)sz, f);
   fclose(f);
   if (got != (size_t)sz)
      return false;
   amiibo_size = (unsigned)sz;
   return true;
}

static bool amiibo_flush(void)
{
   size_t put;
   FILE  *f = fopen(amiibo_path, "wb");
   if (!f)
      return false;
   put = fwrite(amiibo_buf, 1, amiibo_size, f);
   fclose(f);
   return put == amiibo_size;
}

static bool amiibo_init(void)
{
   amiibo_scanning = false;
   amiibo_present  = false;
   amiibo_size     = 0;
   RARCH_LOG("[NFC] amiibo backend: directory \"%s\".\n", amiibo_dir());
   return true;
}

static void amiibo_deinit(void)
{
   amiibo_scanning = false;
   amiibo_present  = false;
}

static bool amiibo_start_scan(void)
{
   amiibo_scanning = true;
   amiibo_reswap   = true;   /* each arm is a fresh tap (reloaded below) */

   /* An explicit pick (file picker) wins; otherwise serve the list
    * selection, rescanning first if the listing/selection is stale. */
   if (amiibo_pick[0])
   {
      strlcpy(amiibo_path, amiibo_pick, sizeof(amiibo_path));
      if (amiibo_load())
      {
         amiibo_present = true;
         RARCH_LOG("[NFC] amiibo backend: \"%s\" present (%u bytes).\n",
               amiibo_path, amiibo_size);
         return true;
      }
      RARCH_WARN("[NFC] amiibo backend: picked file \"%s\" unreadable.\n",
            amiibo_pick);
   }

   if (amiibo_selected < 0 || (unsigned)amiibo_selected >= amiibo_list_len)
      amiibo_rescan();

   if (amiibo_selected >= 0 && (unsigned)amiibo_selected < amiibo_list_len)
   {
      strlcpy(amiibo_path, amiibo_list[amiibo_selected].path, sizeof(amiibo_path));
      if (amiibo_load())
      {
         amiibo_present = true;
         RARCH_LOG("[NFC] amiibo backend: \"%s\" present (%u bytes).\n",
               amiibo_list[amiibo_selected].name, amiibo_size);
         return true;
      }
   }

   amiibo_present = false;
   RARCH_LOG("[NFC] amiibo backend: no readable dump in \"%s\".\n",
         amiibo_dir());
   return true;
}

static void amiibo_stop_scan(void)
{
   amiibo_scanning = false;
   amiibo_present  = false;
}

static enum retro_nfc_status amiibo_get_status(void)
{
   if (amiibo_present)
   {
      if (amiibo_reswap)
      {
         amiibo_reswap = false;   /* report the swap exactly once */
         return RETRO_NFC_STATUS_SCANNING;
      }
      return RETRO_NFC_STATUS_TAG_PRESENT;
   }
   if (amiibo_scanning)
      return RETRO_NFC_STATUS_SCANNING;
   return RETRO_NFC_STATUS_IDLE;
}

static bool amiibo_get_tag_info(struct retro_nfc_tag_info *info)
{
   if (!amiibo_present || !info)
      return false;
   memset(info, 0, sizeof(*info));
   nfc_fill_ntag_uid(amiibo_buf, amiibo_size, info);
   /* Every dump here is an amiibo, so present it as an NTAG215 of the
    * standard 540-byte size even when the file carries extra trailing bytes
    * (some dumps are 572). The full image is still kept for save-back. */
   info->type     = RETRO_NFC_TAG_TYPE_NTAG215;
   info->mem_size = (amiibo_size < 540) ? amiibo_size : 540;
   info->caps     = RETRO_NFC_CAP_NTAG;   /* software file: linear r/w only */
   return true;
}

static int amiibo_read(unsigned offset, uint8_t *buf, unsigned len)
{
   if (!amiibo_present || offset > amiibo_size)
      return -1;
   if (offset + len > amiibo_size)
      len = amiibo_size - offset;
   memcpy(buf, amiibo_buf + offset, len);
   return (int)len;
}

static int amiibo_write(unsigned offset, const uint8_t *buf, unsigned len)
{
   if (!amiibo_present || offset > amiibo_size)
      return -1;
   if (offset + len > amiibo_size)
      len = amiibo_size - offset;
   memcpy(amiibo_buf + offset, buf, len);
   if (!amiibo_flush())
      return -1;
   return (int)len;
}

/* Drop the enumerated list and any selection (core unload). */
void nfc_amiibo_free(void)
{
   free(amiibo_list);
   amiibo_list      = NULL;
   amiibo_list_cap  = 0;
   amiibo_list_len  = 0;
   amiibo_selected  = -1;
   amiibo_pick[0]   = '\0';
   amiibo_reswap    = false;
   amiibo_present   = false;
   amiibo_scanning  = false;
   amiibo_size      = 0;
}

const nfc_backend_t nfc_backend_amiibo = {
   "amiibo",
   amiibo_init,
   amiibo_deinit,
   amiibo_start_scan,
   amiibo_stop_scan,
   amiibo_get_status,
   amiibo_get_tag_info,
   amiibo_read,
   amiibo_write,
};
