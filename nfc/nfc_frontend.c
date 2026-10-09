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

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>

#include <libretro.h>
#include <compat/strl.h>

#include "nfc_backend.h"
#include "nfc_frontend.h"
#include "../verbosity.h"

#ifdef NFC_HAVE_AFNFC
/* Real kernel NFC backend (nfc/nfc_afnfc.c), built with HAVE_NFC_AFNFC. */
extern const nfc_backend_t nfc_backend_afnfc;
extern bool nfc_afnfc_available(void);
#endif

/* "amiibo directory" software backend (nfc/nfc_backend_amiibo.c); always built. */
extern const nfc_backend_t nfc_backend_amiibo;
unsigned nfc_amiibo_source_count(void);
bool     nfc_amiibo_source_name(unsigned index, char *out, size_t out_len);
bool     nfc_amiibo_source_select(unsigned index);
bool     nfc_amiibo_select_path(const char *path);
void     nfc_amiibo_set_dir(const char *dir);
void     nfc_amiibo_free(void);

/* =====================================================================
 * File-simulator backend (developer/testing; RETRO_NFC_BACKEND=file)
 *
 * Backs the interface with a single .bin on disk so the whole
 * environ -> core -> read/write path can be exercised without hardware.
 * Point it at a tag dump with RETRO_NFC_TAG_FILE (default: ./nfc_tag.bin).
 * A tag is "present" after start_scan() whenever the file exists and is
 * readable; writes go to the in-memory image and are flushed back to the
 * file. Swapping the .bin between scans emulates a tag swap.
 * =================================================================== */

static uint8_t  nfc_file_buf[NFC_MAX_TAG_BYTES];
static unsigned nfc_file_size     = 0;
static bool     nfc_file_ready    = false;
static bool     nfc_file_scanning = false;
static bool     nfc_file_present  = false;
static char     nfc_file_path[4096];

static const char *nfc_file_resolve_path(void)
{
   const char *env = getenv("RETRO_NFC_TAG_FILE");
   if (env && *env)
      return env;
   return "nfc_tag.bin";
}

static bool nfc_file_load(void)
{
   long   sz;
   size_t got;
   FILE  *f = fopen(nfc_file_path, "rb");
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
   got = fread(nfc_file_buf, 1, (size_t)sz, f);
   fclose(f);
   if (got != (size_t)sz)
      return false;
   nfc_file_size = (unsigned)sz;
   return true;
}

static bool nfc_file_flush(void)
{
   size_t put;
   FILE  *f = fopen(nfc_file_path, "wb");
   if (!f)
      return false;
   put = fwrite(nfc_file_buf, 1, nfc_file_size, f);
   fclose(f);
   return put == nfc_file_size;
}

static bool nfc_file_init(void)
{
   strlcpy(nfc_file_path, nfc_file_resolve_path(), sizeof(nfc_file_path));
   nfc_file_ready    = true;
   nfc_file_scanning = false;
   nfc_file_present  = false;
   RARCH_LOG("[NFC] file backend: tag image path \"%s\".\n", nfc_file_path);
   return true;
}

static void nfc_file_deinit(void)
{
   nfc_file_ready    = false;
   nfc_file_scanning = false;
   nfc_file_present  = false;
}

static bool nfc_file_start_scan(void)
{
   nfc_file_scanning = true;
   /* (Re)load on each scan start so swapping the .bin is picked up. */
   if (nfc_file_load())
   {
      nfc_file_present = true;
      RARCH_LOG("[NFC] file backend: tag present (%u bytes).\n",
            nfc_file_size);
   }
   else
   {
      nfc_file_present = false;
      RARCH_LOG("[NFC] file backend: no readable tag at \"%s\".\n",
            nfc_file_path);
   }
   return true;
}

static void nfc_file_stop_scan(void)
{
   nfc_file_scanning = false;
   nfc_file_present  = false;
}

static enum retro_nfc_status nfc_file_get_status(void)
{
   if (!nfc_file_ready)    return RETRO_NFC_STATUS_UNSUPPORTED;
   if (nfc_file_present)   return RETRO_NFC_STATUS_TAG_PRESENT;
   if (nfc_file_scanning)  return RETRO_NFC_STATUS_SCANNING;
   return RETRO_NFC_STATUS_IDLE;
}

static bool nfc_file_get_tag_info(struct retro_nfc_tag_info *info)
{
   if (!nfc_file_present || !info)
      return false;
   memset(info, 0, sizeof(*info));
   info->type     = nfc_type_from_size(nfc_file_size);
   info->caps     = RETRO_NFC_CAP_NTAG;   /* file: linear r/w only */
   info->mem_size = nfc_file_size;
   nfc_fill_ntag_uid(nfc_file_buf, nfc_file_size, info);
   return true;
}

static int nfc_file_read(unsigned offset, uint8_t *buf, unsigned len)
{
   if (!nfc_file_present || !buf)
      return -1;
   if (offset >= nfc_file_size)
      return -1;
   if (offset + len > nfc_file_size)
      len = nfc_file_size - offset;
   memcpy(buf, nfc_file_buf + offset, len);
   return (int)len;
}

static int nfc_file_write(unsigned offset, const uint8_t *buf, unsigned len)
{
   if (!nfc_file_present || !buf)
      return -1;
   if (offset >= nfc_file_size)
      return -1;
   if (offset + len > nfc_file_size)
      len = nfc_file_size - offset;
   memcpy(nfc_file_buf + offset, buf, len);
   if (!nfc_file_flush())
      return -1;
   return (int)len;
}

static const nfc_backend_t nfc_backend_file = {
   "file",
   nfc_file_init,       nfc_file_deinit,
   nfc_file_start_scan, nfc_file_stop_scan,
   nfc_file_get_status, nfc_file_get_tag_info,
   nfc_file_read,       nfc_file_write
};

/* =====================================================================
 * Dispatch
 * =================================================================== */

static const nfc_backend_t *nfc_cur      = NULL;
static bool                 nfc_init_done = false;
static bool                 nfc_scanning  = false;

static const nfc_backend_t *nfc_select_backend(void)
{
   const char *sel = getenv("RETRO_NFC_BACKEND");
#ifdef NFC_HAVE_AFNFC
   if (sel && !strcmp(sel, "afnfc"))
      return &nfc_backend_afnfc;
#endif
   if (sel && !strcmp(sel, "file"))
      return &nfc_backend_file;
   if (sel && *sel && strcmp(sel, "amiibo") && strcmp(sel, "software"))
      RARCH_WARN("[NFC] unknown RETRO_NFC_BACKEND \"%s\"; using software.\n", sel);
   /* The amiibo-directory software reader is the default backend. */
   return &nfc_backend_amiibo;
}

static bool nfc_ensure_init(void)
{
   if (nfc_init_done)
      return nfc_cur != NULL;
   nfc_init_done = true;
   nfc_cur       = nfc_select_backend();
   if (nfc_cur && nfc_cur->init && !nfc_cur->init())
   {
      RARCH_ERR("[NFC] backend \"%s\" init failed.\n", nfc_cur->ident);
      nfc_cur = NULL;
   }
   if (nfc_cur)
      RARCH_LOG("[NFC] active backend: %s.\n", nfc_cur->ident);
   return nfc_cur != NULL;
}

bool RETRO_CALLCONV nfc_frontend_start_scan(void)
{
   if (!nfc_ensure_init())
      return false;
   nfc_scanning = true;
   return nfc_cur->start_scan ? nfc_cur->start_scan() : false;
}

void RETRO_CALLCONV nfc_frontend_stop_scan(void)
{
   nfc_scanning = false;
   if (!nfc_ensure_init())
      return;
   if (nfc_cur->stop_scan)
      nfc_cur->stop_scan();
}

enum retro_nfc_status RETRO_CALLCONV nfc_frontend_get_status(void)
{
   if (!nfc_ensure_init())
      return RETRO_NFC_STATUS_UNSUPPORTED;
   return nfc_cur->get_status
      ? nfc_cur->get_status() : RETRO_NFC_STATUS_UNSUPPORTED;
}

/* Interface version negotiated with the running core. Bounds how much of
 * struct retro_nfc_tag_info is written back, so a core built against an
 * older (smaller) struct is never overrun when the struct grows. */
static unsigned nfc_iface_version = RETRO_NFC_INTERFACE_VERSION;

void nfc_frontend_set_interface_version(unsigned version)
{
   nfc_iface_version = version;
}

bool RETRO_CALLCONV nfc_frontend_get_tag_info(struct retro_nfc_tag_info *info)
{
   struct retro_nfc_tag_info full;
   size_t size = sizeof(full);
   if (!info || !nfc_ensure_init() || !nfc_cur->get_tag_info)
      return false;
   memset(&full, 0, sizeof(full));
   if (!nfc_cur->get_tag_info(&full))
      return false;
   if (nfc_iface_version < 5)
      size = offsetof(struct retro_nfc_tag_info, caps);
   memcpy(info, &full, size);
   return true;
}

int RETRO_CALLCONV nfc_frontend_read(unsigned offset, uint8_t *buf, unsigned len)
{
   if (!nfc_ensure_init())
      return -1;
   return nfc_cur->read ? nfc_cur->read(offset, buf, len) : -1;
}

int RETRO_CALLCONV nfc_frontend_write(unsigned offset,
      const uint8_t *buf, unsigned len)
{
   if (!nfc_ensure_init())
      return -1;
   return nfc_cur->write ? nfc_cur->write(offset, buf, len) : -1;
}

/* --- interface version 5 --- */

int RETRO_CALLCONV nfc_frontend_mifare_read_block(unsigned sector, unsigned block,
      enum retro_nfc_mifare_key_type key_type, const uint8_t *key, uint8_t *out)
{
   if (!nfc_ensure_init())
      return RETRO_NFC_ERR_UNSUPPORTED;
   return nfc_cur->mifare_read_block
      ? nfc_cur->mifare_read_block(sector, block, key_type, key, out)
      : RETRO_NFC_ERR_UNSUPPORTED;
}

int RETRO_CALLCONV nfc_frontend_mifare_write_block(unsigned sector, unsigned block,
      enum retro_nfc_mifare_key_type key_type, const uint8_t *key,
      const uint8_t *data)
{
   if (!nfc_ensure_init())
      return RETRO_NFC_ERR_UNSUPPORTED;
   return nfc_cur->mifare_write_block
      ? nfc_cur->mifare_write_block(sector, block, key_type, key, data)
      : RETRO_NFC_ERR_UNSUPPORTED;
}

int RETRO_CALLCONV nfc_frontend_transceive(const uint8_t *tx, unsigned tx_len,
      uint8_t *rx, unsigned rx_cap)
{
   if (!nfc_ensure_init())
      return RETRO_NFC_ERR_UNSUPPORTED;
   return nfc_cur->transceive
      ? nfc_cur->transceive(tx, tx_len, rx, rx_cap)
      : RETRO_NFC_ERR_UNSUPPORTED;
}

/* Switch the active backend at runtime (interface v2 set_source). */
static bool nfc_switch_backend(const nfc_backend_t *b)
{
   if (!b)
      return false;
   if (nfc_cur == b)
      return true;
   if (nfc_cur && nfc_cur->deinit)
      nfc_cur->deinit();
   nfc_cur       = b;
   nfc_init_done = true;
   if (nfc_cur->init && !nfc_cur->init())
   {
      RARCH_ERR("[NFC] backend \"%s\" init failed.\n", nfc_cur->ident);
      nfc_cur = NULL;
      return false;
   }
   RARCH_LOG("[NFC] active backend: %s.\n", nfc_cur->ident);

   /* If a scan was in progress, resume it on the new backend so the selected
    * reader/tag becomes available without waiting for the next start_scan. */
   if (nfc_scanning && nfc_cur->start_scan)
      nfc_cur->start_scan();
   return true;
}

/* True unless a physical reader is the active source (drives amiibo file
 * picker visibility - the picker is only for software dumps). */
bool nfc_frontend_active_is_software(void)
{
#ifdef NFC_HAVE_AFNFC
   return nfc_cur != &nfc_backend_afnfc;
#else
   return true;
#endif
}

/* True when a hardware reader is built in (so a core may offer the
 * hardware/software choice). */
bool nfc_frontend_hardware_available(void)
{
#ifdef NFC_HAVE_AFNFC
   return nfc_afnfc_available();   /* runtime: a reader may come and go */
#else
   return false;
#endif
}

/* Interface query (v4): is a hardware reader available right now? Lets a core
 * show/hide its hardware/software option live. */
bool RETRO_CALLCONV nfc_frontend_has_hardware(void)
{
   return nfc_frontend_hardware_available();
}

/* Runtime count of hardware reader slots (0 or 1). */
static unsigned nfc_hw_slots(void)
{
   return nfc_frontend_hardware_available() ? 1u : 0u;
}

bool RETRO_CALLCONV nfc_frontend_set_source(enum retro_nfc_source source)
{
   switch (source)
   {
      case RETRO_NFC_SOURCE_HARDWARE:
#ifdef NFC_HAVE_AFNFC
         if (nfc_frontend_hardware_available())
            return nfc_switch_backend(&nfc_backend_afnfc);
         RARCH_WARN("[NFC] no hardware reader present; using software.\n");
#else
         RARCH_WARN("[NFC] hardware reader not built in; using software.\n");
#endif
         return nfc_switch_backend(&nfc_backend_amiibo);
      case RETRO_NFC_SOURCE_SOFTWARE:
      case RETRO_NFC_SOURCE_AUTO:   /* software is the default; never auto-forced */
      default:
         return nfc_switch_backend(&nfc_backend_amiibo);
   }
}

/* Enumeration (interface v3): a hardware reader (when built in) followed by
 * each stored amiibo dump. Index 0..NFC_HW_SLOTS-1 are hardware; the rest map
 * onto the amiibo directory listing. */
unsigned RETRO_CALLCONV nfc_frontend_get_source_count(void)
{
   return nfc_hw_slots() + nfc_amiibo_source_count();
}

bool RETRO_CALLCONV nfc_frontend_get_source_info(unsigned index,
      struct retro_nfc_source_info *info)
{
   if (!info)
      return false;
   memset(info, 0, sizeof(*info));

   {
      unsigned hw = nfc_hw_slots();
      if (index < hw)
      {
         strlcpy(info->name, "Hardware reader", sizeof(info->name));
         info->is_hardware = true;
         return true;
      }
      info->is_hardware = false;
      return nfc_amiibo_source_name(index - hw, info->name, sizeof(info->name));
   }
}

bool RETRO_CALLCONV nfc_frontend_select_source(unsigned index)
{
   unsigned hw = nfc_hw_slots();
#ifdef NFC_HAVE_AFNFC
   if (index < hw)
      return nfc_switch_backend(&nfc_backend_afnfc);
#endif
   if (index < hw)   /* hw claimed a slot but afnfc not built: shouldn't happen */
      return false;

   if (!nfc_amiibo_source_select(index - hw))
      return false;

   /* If the software backend is already active, switching won't re-run its
    * start_scan, so reload the newly-selected dump here (when a scan is in
    * progress). Otherwise switch to it, which loads the dump via the
    * scan carry-over. */
   if (nfc_cur == &nfc_backend_amiibo)
   {
      if (nfc_scanning && nfc_backend_amiibo.start_scan)
         nfc_backend_amiibo.start_scan();
      return true;
   }
   return nfc_switch_backend(&nfc_backend_amiibo);
}

/* Select an amiibo dump by absolute path (RetroArch's amiibo file picker). */
bool RETRO_CALLCONV nfc_frontend_select_amiibo_path(const char *path)
{
   if (!nfc_amiibo_select_path(path))
      return false;
   if (nfc_cur == &nfc_backend_amiibo)
   {
      if (nfc_scanning && nfc_backend_amiibo.start_scan)
         nfc_backend_amiibo.start_scan();
      return true;
   }
   return nfc_switch_backend(&nfc_backend_amiibo);
}

/* Point the software backend at the frontend-configured amiibo directory. */
void nfc_frontend_set_amiibo_dir(const char *dir)
{
   nfc_amiibo_set_dir(dir);
}


void nfc_frontend_free(void)
{
   if (nfc_cur && nfc_cur->deinit)
      nfc_cur->deinit();
   nfc_amiibo_free();
   nfc_cur       = NULL;
   nfc_init_done = false;
   nfc_scanning  = false;
}
