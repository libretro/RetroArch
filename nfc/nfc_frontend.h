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

#ifndef _NFC_FRONTEND_H
#define _NFC_FRONTEND_H

#include <libretro.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Frontend-side implementations of the retro_nfc_interface function
 * pointers. These are wired into RETRO_ENVIRONMENT_GET_NFC_INTERFACE in
 * runloop.c and dispatch to whichever backend is active (see
 * nfc_frontend.c). All are safe to call before a reader is connected;
 * they degrade to RETRO_NFC_STATUS_UNSUPPORTED / -1 as appropriate. */

bool                  RETRO_CALLCONV nfc_frontend_start_scan(void);
void                  RETRO_CALLCONV nfc_frontend_stop_scan(void);
enum retro_nfc_status RETRO_CALLCONV nfc_frontend_get_status(void);
bool                  RETRO_CALLCONV nfc_frontend_get_tag_info(
      struct retro_nfc_tag_info *info);
int                   RETRO_CALLCONV nfc_frontend_read(
      unsigned offset, uint8_t *buf, unsigned len);
int                   RETRO_CALLCONV nfc_frontend_write(
      unsigned offset, const uint8_t *buf, unsigned len);

/* Version negotiated with the core (bounds struct write-back). */
void nfc_frontend_set_interface_version(unsigned version);

/* --- interface version 5 --- */
int                   RETRO_CALLCONV nfc_frontend_mifare_read_block(
      unsigned sector, unsigned block,
      enum retro_nfc_mifare_key_type key_type,
      const uint8_t *key, uint8_t *out);
int                   RETRO_CALLCONV nfc_frontend_mifare_write_block(
      unsigned sector, unsigned block,
      enum retro_nfc_mifare_key_type key_type,
      const uint8_t *key, const uint8_t *data);
int                   RETRO_CALLCONV nfc_frontend_transceive(
      const uint8_t *tx, unsigned tx_len, uint8_t *rx, unsigned rx_cap);

/* Select the reader source (interface v2). */
bool RETRO_CALLCONV nfc_frontend_set_source(enum retro_nfc_source source);

/* Enumerate/select reader sources (interface v3). */
unsigned RETRO_CALLCONV nfc_frontend_get_source_count(void);
bool     RETRO_CALLCONV nfc_frontend_get_source_info(unsigned index,
      struct retro_nfc_source_info *info);
bool     RETRO_CALLCONV nfc_frontend_select_source(unsigned index);

/* Select an amiibo dump by absolute path (file picker). */
bool     RETRO_CALLCONV nfc_frontend_select_amiibo_path(const char *path);

/* Set the amiibo directory used by the software backend (from the
 * frontend Directory settings). NULL/empty clears the override. */
void nfc_frontend_set_amiibo_dir(const char *dir);

/* Whether the active source is software (not a hardware reader), and
 * whether a hardware reader is built in. */
bool nfc_frontend_active_is_software(void);
bool nfc_frontend_hardware_available(void);

/* Interface query (v4): hardware reader present right now. */
bool RETRO_CALLCONV nfc_frontend_has_hardware(void);

/* Release any backend resources. Called on core unload / deinit.
 * Idempotent. */
void nfc_frontend_free(void);

RETRO_END_DECLS

#endif
