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

#ifndef _NFC_BACKEND_H
#define _NFC_BACKEND_H

#include <stdint.h>
#include <string.h>

#include <libretro.h>
#include <retro_inline.h>

/* Largest tag image any backend serves (NTAG216 = 924 bytes, rounded up). */
#define NFC_MAX_TAG_BYTES 1024

/* A pluggable NFC reader backend. Each backend is a small vtable; the
 * dispatcher in nfc_frontend.c selects one at runtime. Backends speak
 * only the standard interfaces (a file image, or the kernel NFC genl +
 * AF_NFC command set) so nothing here is tied to a particular driver. */
typedef struct nfc_backend
{
   const char *ident;
   bool                  (*init)(void);
   void                  (*deinit)(void);
   bool                  (*start_scan)(void);
   void                  (*stop_scan)(void);
   enum retro_nfc_status (*get_status)(void);
   bool                  (*get_tag_info)(struct retro_nfc_tag_info *info);
   int                   (*read )(unsigned offset, uint8_t *buf, unsigned len);
   int                   (*write)(unsigned offset, const uint8_t *buf, unsigned len);

   /* --- interface version 5 (optional; leave NULL if unsupported) --- */
   int                   (*mifare_read_block )(unsigned sector, unsigned block,
                               enum retro_nfc_mifare_key_type key_type,
                               const uint8_t *key, uint8_t *out);
   int                   (*mifare_write_block)(unsigned sector, unsigned block,
                               enum retro_nfc_mifare_key_type key_type,
                               const uint8_t *key, const uint8_t *data);
   int                   (*transceive)(const uint8_t *tx, unsigned tx_len,
                               uint8_t *rx, unsigned rx_cap);
} nfc_backend_t;

/* Guess the NTAG product from a full-dump size (amiibo == NTAG215). */
static INLINE enum retro_nfc_tag_type nfc_type_from_size(unsigned size)
{
   switch (size)
   {
      case 180: return RETRO_NFC_TAG_TYPE_NTAG213;   /* 45 pages           */
      case 540: return RETRO_NFC_TAG_TYPE_NTAG215;   /* 135 pages (amiibo) */
      case 924: return RETRO_NFC_TAG_TYPE_NTAG216;   /* 231 pages          */
      default:  break;
   }
   return RETRO_NFC_TAG_TYPE_ISO14443A;
}

/* Extract the NTAG NFCID1 (7 bytes) from a page-0/1 image:
 *   page0 = SN0 SN1 SN2 BCC0
 *   page1 = SN3 SN4 SN5 SN6
 * so UID = raw[0..2] + raw[4..7]. */
static INLINE void nfc_fill_ntag_uid(const uint8_t *raw, unsigned size,
      struct retro_nfc_tag_info *info)
{
   if (size >= 8)
   {
      info->uid[0] = raw[0];
      info->uid[1] = raw[1];
      info->uid[2] = raw[2];
      info->uid[3] = raw[4];
      info->uid[4] = raw[5];
      info->uid[5] = raw[6];
      info->uid[6] = raw[7];
      info->uid_len = 7;
   }
   else
      info->uid_len = 0;
}

#endif
