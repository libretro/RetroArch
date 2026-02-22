/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2019 - Brad Parker
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

#include <ctype.h>
#include <string.h>

#include <retro_miscellaneous.h>
#include <retro_endianness.h>
#include <compat/strcasestr.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <retro_endianness.h>
#include <streams/file_stream.h>
#include <streams/interface_stream.h>
#include <string/stdstring.h>
#include "task_database_cue.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "../database_info.h"

#include "tasks_internal.h"

#include "../msg_hash.h"
#include "../verbosity.h"

#define MAX_TOKEN_LEN   255

#ifdef MSB_FIRST
#define MODETEST_VAL    0x00ffffff
#else
#define MODETEST_VAL    0xffffff00
#endif

static struct magic_entry MAGIC_NUMBERS[] = {
   { "Nintendo - GameCube",         "\xc2\x33\x9f\x3d", 0x00001c},
   { "Nintendo - GameCube",         "\xc2\x33\x9f\x3d", 0x000074}, /* RVZ, WIA */
   { "Nintendo - Wii",              "\x5d\x1c\x9e\xa3", 0x000018},
   { "Nintendo - Wii",              "\x5d\x1c\x9e\xa3", 0x000218}, /* WBFS */
   { "Nintendo - Wii",              "\x5d\x1c\x9e\xa3", 0x000070}, /* RVZ, WIA */
   { "Sega - Dreamcast",            "SEGA SEGAKATANA",  0x000010},
   { "Sega - Mega-CD - Sega CD",    "SEGADISCSYSTEM",   0x000010},
   { "Sega - Saturn",               "SEGA SEGASATURN",  0x000010},
   { "Sony - PlayStation",          "Sony Computer ",   0x0024f8}, /* PS1 CD license string, PS2 CD doesnt have this string */
   { "Sony - PlayStation 2",        "PLAYSTATION",      0x009320}, /* PS1 CD and PS2 CD */
   { "Sony - PlayStation 2",        "PLAYSTATION",      0x008008}, /* PS2 DVD */
   { "Sony - PlayStation 2",        "           ",      0x008008}, /* PS2 DVD */
   { "Sony - PlayStation Portable", "PSP GAME",         0x008008},
   /* CD-i magic number should start with \0 at position 0 but it throws off later strlen() */
   { "Philips - CD-i",              "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00", 0x000001},
   { NULL,                          NULL,               0}
};

/**
 * Given a filename and position, find the associated disc number.
 */
static int cue_find_disc_number(const char* str1, char disc)
{
   switch (disc)
   {
      case 'a':
      case 'A':
         return 1;
      case 'b':
      case 'B':
         return 2;
      case 'c':
      case 'C':
         return 3;
      case 'd':
      case 'D':
         return 4;
      case 'e':
      case 'E':
         return 5;
      case 'f':
      case 'F':
         return 6;
      case 'g':
      case 'G':
         return 7;
      case 'h':
      case 'H':
         return 8;
      case 'i':
      case 'I':
         return 9;
      default:
         if ((disc - '0') >= 1)
            return (disc - '0');
         break;
   }

   return 0;
}

/**
 * Given a title and filename, append the appropriate disc number to it.
 */
static void cue_append_multi_disc_suffix(char * str1, const char *filename)
{
   /* Check multi-disc and insert suffix */
   int result = string_find_index_substring_string(filename, "(Disc ");
   if (result < 0)
      result = string_find_index_substring_string(filename, "(disc ");
   if (result < 0)
      result = string_find_index_substring_string(filename, "(Disk ");
   if (result < 0)
      result = string_find_index_substring_string(filename, "(disk ");
   if (result >= 0)
   {
      int disc_number = cue_find_disc_number(filename, filename[result + 6]);
      if (disc_number > 0)
      {
         char *dest   = str1;
         size_t __len = strlen(dest);
         snprintf(dest + __len, PATH_MAX_LENGTH - __len, "-%i", disc_number - 1);
      }
   }
}

static int64_t task_database_cue_get_token(intfstream_t *fd, char *s,
   uint64_t len)
{
   char *c       = s;
   int64_t _len  = 0;
   int in_string = 0;

   for (;;)
   {
      int64_t rv = (int64_t)intfstream_read(fd, c, 1);
      if (rv == 0)
         return 0;
      else if (rv < 0)
         return -1;

      switch (*c)
      {
         case ' ':
         case '\t':
         case '\r':
         case '\n':
            if (c == s)
               continue;

            if (!in_string)
            {
               *c = '\0';
               return _len;
            }
            break;
         case '\"':
            if (c == s)
            {
               in_string = 1;
               continue;
            }

            *c = '\0';
            return _len;
      }

      _len++;
      c++;
      if (_len == (int64_t)len)
      {
         *c = '\0';
         return _len;
      }
   }
}

#define DISC_DATA_SIZE_PS1 60000

int detect_ps1_game(intfstream_t *fd, char *s, size_t len,
   const char *filename)
{
   int pos;
   char raw_game_id[50];
   char *disc_data = (char*)malloc(DISC_DATA_SIZE_PS1);
   if (!disc_data)
      return 0;
   if (  intfstream_seek(fd, 0, SEEK_SET) < 0
      || intfstream_read(fd, disc_data, DISC_DATA_SIZE_PS1) <= 0)
   {
      free(disc_data);
      return 0;
   }
   disc_data[DISC_DATA_SIZE_PS1 - 1] = '\0';

   for (pos = 0; pos < DISC_DATA_SIZE_PS1; pos++)
   {
      strlcpy(raw_game_id, &disc_data[pos], sizeof(raw_game_id));
      raw_game_id[12] = '\0';

      if (raw_game_id[0] == 'S' || raw_game_id[0] == 'E')
      {
         if (  memcmp(raw_game_id, "SCUS_", 5) == 0
            || memcmp(raw_game_id, "SLUS_", 5) == 0
            || memcmp(raw_game_id, "SLES_", 5) == 0
            || memcmp(raw_game_id, "SCED_", 5) == 0
            || memcmp(raw_game_id, "SLPS_", 5) == 0
            || memcmp(raw_game_id, "SLPM_", 5) == 0
            || memcmp(raw_game_id, "SCPS_", 5) == 0
            || memcmp(raw_game_id, "SLED_", 5) == 0
            || memcmp(raw_game_id, "SIPS_", 5) == 0
            || memcmp(raw_game_id, "ESPM_", 5) == 0
            || memcmp(raw_game_id, "SCES_", 5) == 0
            || memcmp(raw_game_id, "SLKA_", 5) == 0
            || memcmp(raw_game_id, "SCAJ_", 5) == 0
            )
         {
            raw_game_id[4] = '-';
            if (raw_game_id[8] == '.')
            {
               raw_game_id[8] = raw_game_id[9];
               raw_game_id[9] = raw_game_id[10];
            }
            else if (raw_game_id[7] == '.')
            {
               raw_game_id[7] = raw_game_id[8];
               raw_game_id[8] = raw_game_id[9];
               raw_game_id[9] = raw_game_id[10];
            }
            raw_game_id[10] = '\0';
            string_remove_all_whitespace(s, raw_game_id);
            cue_append_multi_disc_suffix(s, filename);
            free(disc_data);
            return 1;
         }
      }
      else if (memcmp(raw_game_id, "LSP-", 4) == 0)
      {
         raw_game_id[10] = '\0';
         string_remove_all_whitespace(s, raw_game_id);
         cue_append_multi_disc_suffix(s, filename);
         free(disc_data);
         return 1;
      }
      else if (memcmp(raw_game_id, "PSX.EXE", 7) == 0)
      {
         raw_game_id[7] = '\0';
         string_remove_all_whitespace(s, raw_game_id);
         cue_append_multi_disc_suffix(s, filename);
         free(disc_data);
         return 0;
      }
   }

   s[0 ] = 'X';
   s[1 ] = 'X';
   s[2 ] = 'X';
   s[3 ] = 'X';
   s[4 ] = 'X';
   s[5 ] = 'X';
   s[6 ] = 'X';
   s[7 ] = 'X';
   s[8 ] = 'X';
   s[9 ] = 'X';
   s[10] = '\0';
   cue_append_multi_disc_suffix(s, filename);
   free(disc_data);
   return 0;
}

int detect_ps2_game(intfstream_t *fd, char *s, size_t len,
   const char *filename)
{
   #define DISC_DATA_SIZE_PS2 600000
   int pos;
   char raw_game_id[50];
   char *disc_data;

   /* Load data into buffer and use pointers */
   if (intfstream_seek(fd, 0, SEEK_SET) < 0)
      return 0;

   disc_data = (char*)malloc(DISC_DATA_SIZE_PS2);

   if (intfstream_read(fd, disc_data, DISC_DATA_SIZE_PS2) <= 0)
   {
      free(disc_data);
      return 0;
   }

   disc_data[DISC_DATA_SIZE_PS2 - 1] = '\0';

   for (pos = 0; pos < DISC_DATA_SIZE_PS2 - 12; pos++)
   {
      /* Quick first-character check to avoid unnecessary work */
      char c = disc_data[pos];
      if (  c != 'S' && c != 'P' && c != 'T' && c != 'C'
         && c != 'H' && c != 'A' && c != 'V' && c != 'L'
         && c != 'M' && c != 'N' && c != 'U' && c != 'W'
         && c != 'G' && c != 'K' && c != 'R')
         continue;

      strlcpy(raw_game_id, &disc_data[pos], 13);

      if (  memcmp(raw_game_id, "SLPM_", STRLEN_CONST("SLPM_")) == 0
         || memcmp(raw_game_id, "SLES_", STRLEN_CONST("SLES_")) == 0
         || memcmp(raw_game_id, "SCES_", STRLEN_CONST("SCES_")) == 0
         || memcmp(raw_game_id, "SLUS_", STRLEN_CONST("SLUS_")) == 0
         || memcmp(raw_game_id, "SLPS_", STRLEN_CONST("SLPS_")) == 0
         || memcmp(raw_game_id, "SCED_", STRLEN_CONST("SCED_")) == 0
         || memcmp(raw_game_id, "SCUS_", STRLEN_CONST("SCUS_")) == 0
         || memcmp(raw_game_id, "SCPS_", STRLEN_CONST("SCPS_")) == 0
         || memcmp(raw_game_id, "SCAJ_", STRLEN_CONST("SCAJ_")) == 0
         || memcmp(raw_game_id, "SLKA_", STRLEN_CONST("SLKA_")) == 0
         || memcmp(raw_game_id, "SCKA_", STRLEN_CONST("SCKA_")) == 0
         || memcmp(raw_game_id, "SLAJ_", STRLEN_CONST("SLAJ_")) == 0
         || memcmp(raw_game_id, "TCPS_", STRLEN_CONST("TCPS_")) == 0
         || memcmp(raw_game_id, "KOEI_", STRLEN_CONST("KOEI_")) == 0
         || memcmp(raw_game_id, "PBPX_", STRLEN_CONST("PBPX_")) == 0
         || memcmp(raw_game_id, "PCPX_", STRLEN_CONST("PCPX_")) == 0
         || memcmp(raw_game_id, "PAPX_", STRLEN_CONST("PAPX_")) == 0
         || memcmp(raw_game_id, "SCCS_", STRLEN_CONST("SCCS_")) == 0
         || memcmp(raw_game_id, "ALCH_", STRLEN_CONST("ALCH_")) == 0
         || memcmp(raw_game_id, "TCES_", STRLEN_CONST("TCES_")) == 0
         || memcmp(raw_game_id, "CPCS_", STRLEN_CONST("CPCS_")) == 0
         || memcmp(raw_game_id, "SLED_", STRLEN_CONST("SLED_")) == 0
         || memcmp(raw_game_id, "TLES_", STRLEN_CONST("TLES_")) == 0
         || memcmp(raw_game_id, "GUST_", STRLEN_CONST("GUST_")) == 0
         || memcmp(raw_game_id, "CF00_", STRLEN_CONST("CF00_")) == 0
         || memcmp(raw_game_id, "SCPN_", STRLEN_CONST("SCPN_")) == 0
         || memcmp(raw_game_id, "SCPM_", STRLEN_CONST("SCPM_")) == 0
         || memcmp(raw_game_id, "PSXC_", STRLEN_CONST("PSXC_")) == 0
         || memcmp(raw_game_id, "SLPN_", STRLEN_CONST("SLPN_")) == 0
         || memcmp(raw_game_id, "ULKS_", STRLEN_CONST("ULKS_")) == 0
         || memcmp(raw_game_id, "LDTL_", STRLEN_CONST("LDTL_")) == 0
         || memcmp(raw_game_id, "PKP2_", STRLEN_CONST("PKP2_")) == 0
         || memcmp(raw_game_id, "WLFD_", STRLEN_CONST("WLFD_")) == 0
         || memcmp(raw_game_id, "CZP2_", STRLEN_CONST("CZP2_")) == 0
         || memcmp(raw_game_id, "HAKU_", STRLEN_CONST("HAKU_")) == 0
         || memcmp(raw_game_id, "SRPM_", STRLEN_CONST("SRPM_")) == 0
         || memcmp(raw_game_id, "MTP2_", STRLEN_CONST("MTP2_")) == 0
         || memcmp(raw_game_id, "NMP2_", STRLEN_CONST("NMP2_")) == 0
         || memcmp(raw_game_id, "ARZE_", STRLEN_CONST("ARZE_")) == 0
         || memcmp(raw_game_id, "VUGJ_", STRLEN_CONST("VUGJ_")) == 0
         || memcmp(raw_game_id, "ARP2_", STRLEN_CONST("ARP2_")) == 0
         || memcmp(raw_game_id, "ROSE_", STRLEN_CONST("ROSE_")) == 0
         )
      {
         raw_game_id[4] = '-';
         if (raw_game_id[8] == '.')
         {
            raw_game_id[8] = raw_game_id[9];
            raw_game_id[9] = raw_game_id[10];
            raw_game_id[10] = '\0';
         }
         /* A few games have their serial in the form of xx.xxx */
         /* Tanaka Torahiko no Ultra-ryuu Shougi - Ibisha Anaguma-hen (Japan) -> SLPS_02.261 */
         else if (raw_game_id[7] == '.')
         {
            raw_game_id[7] = raw_game_id[8];
            raw_game_id[8] = raw_game_id[9];
            raw_game_id[9] = raw_game_id[10];
            raw_game_id[10] = '\0';
         }
         else
            raw_game_id[10] = '\0';

         /* Fix corrupted digits (control char 0x12 -> '3') */
         if (raw_game_id[8] == '\x12')
            raw_game_id[8] = '3';
         if (raw_game_id[9] == '\x12')
            raw_game_id[9] = '3';

         string_remove_all_whitespace(s, raw_game_id);
         cue_append_multi_disc_suffix(s, filename);
         free(disc_data);
         return 1;
      }
   }

   s[0 ] = 'X';
   s[1 ] = 'X';
   s[2 ] = 'X';
   s[3 ] = 'X';
   s[4 ] = 'X';
   s[5 ] = 'X';
   s[6 ] = 'X';
   s[7 ] = 'X';
   s[8 ] = 'X';
   s[9 ] = 'X';
   s[10] = '\0';
   cue_append_multi_disc_suffix(s, filename);
   free(disc_data);
   return 0;
}

int detect_psp_game(intfstream_t *fd, char *s, size_t len,
   const char *filename)
{
#define DISC_DATA_SIZE_PSP 300000
   int pos;
   ssize_t bytes_read;
   char *disc_data = (char*)malloc(DISC_DATA_SIZE_PSP);
   if (!disc_data)
      return 0;

   if (intfstream_seek(fd, 0, SEEK_SET) < 0)
   {
      free(disc_data);
      return 0;
   }

   bytes_read = intfstream_read(fd, disc_data, DISC_DATA_SIZE_PSP);
   if (bytes_read <= 0)
   {
      free(disc_data);
      return 0;
   }

   for (pos = 0; pos <= bytes_read - 10; pos++)
   {
      const char *p = &disc_data[pos];
      if (*p != 'U' && *p != 'N')
         continue;
      if (
            memcmp(p, "ULES-", 5) == 0
         || memcmp(p, "ULUS-", 5) == 0
         || memcmp(p, "ULJS-", 5) == 0
         || memcmp(p, "ULEM-", 5) == 0
         || memcmp(p, "ULUM-", 5) == 0
         || memcmp(p, "ULJM-", 5) == 0
         || memcmp(p, "UCES-", 5) == 0
         || memcmp(p, "UCUS-", 5) == 0
         || memcmp(p, "UCJS-", 5) == 0
         || memcmp(p, "UCAS-", 5) == 0
         || memcmp(p, "UCKS-", 5) == 0
         || memcmp(p, "ULKS-", 5) == 0
         || memcmp(p, "ULAS-", 5) == 0
         || memcmp(p, "NPEH-", 5) == 0
         || memcmp(p, "NPUH-", 5) == 0
         || memcmp(p, "NPJH-", 5) == 0
         || memcmp(p, "NPHH-", 5) == 0
         || memcmp(p, "NPEG-", 5) == 0
         || memcmp(p, "NPUG-", 5) == 0
         || memcmp(p, "NPJG-", 5) == 0
         || memcmp(p, "NPHG-", 5) == 0
         || memcmp(p, "NPEZ-", 5) == 0
         || memcmp(p, "NPUZ-", 5) == 0
         || memcmp(p, "NPJZ-", 5) == 0
         )
      {
         strlcpy(s, p, MIN(len, 11));
         cue_append_multi_disc_suffix(s, filename);
         free(disc_data);
         return 1;
      }
   }
   free(disc_data);
   return 0;
}

size_t detect_gc_game(intfstream_t *fd, char *s, size_t len,
   const char *filename)
{
   char region_id;
   char pre_game_id[20];
   char raw_game_id[20];
   size_t _len = 0;

   /* Load raw serial or quit */
   if (intfstream_seek(fd, 0, SEEK_SET) < 0)
      return 0;

   if (intfstream_read(fd, raw_game_id, 4) <= 0)
      return 0;

   if (     memcmp(raw_game_id, "RVZ", STRLEN_CONST("RVZ")) == 0
         || memcmp(raw_game_id, "WIA", STRLEN_CONST("WIA")) == 0
      )
   {
      if (intfstream_seek(fd, 0x0058, SEEK_SET) < 0)
         return 0;
      if (intfstream_read(fd, raw_game_id, 4) <= 0)
         return 0;
   }

   raw_game_id[4] = '\0';

   /** Scrub files with bad data and log **/
   if (raw_game_id[0] == '\0' || raw_game_id[0] == ' ')
   {
#ifdef DEBUG
      RARCH_LOG("[Scanner] Scrubbing: \"%s\".\n", filename);
#endif
      return 0;
   }

   /** convert raw gamecube serial to redump serial.
   not enough is known about the disc data to properly
   convert every raw serial to redump serial.  it will
   only fail with the following exceptions: the
   subregions of europe P-UKV, P-AUS, X-UKV, X-EUU
   will not match redump.**/

   /** insert prefix **/
   _len = strlcpy(pre_game_id, "DL-DOL-", sizeof(pre_game_id));
   /** add raw serial **/
   strlcpy(pre_game_id + _len, raw_game_id, sizeof(pre_game_id) - _len);

   /** check region **/
   region_id = pre_game_id[10];

   /** check multi-disc and insert suffix **/
   cue_append_multi_disc_suffix(pre_game_id, filename);
   _len = strlcpy(s, pre_game_id, len);

   switch (region_id)
   {
      case 'E':
         _len += strlcpy(s + _len, "-USA", len - _len);
         break;
      case 'J':
         _len += strlcpy(s + _len, "-JPN", len - _len);
         break;
      case 'P': /** NYI: P can also be P-UKV, P-AUS **/
      case 'X': /** NYI: X can also be X-UKV, X-EUU **/
         _len += strlcpy(s + _len, "-EUR", len - _len);
         break;
      case 'Y':
         _len += strlcpy(s + _len, "-FAH", len - _len);
         break;
      case 'D':
         _len += strlcpy(s + _len, "-NOE", len - _len);
         break;
      case 'S':
         _len += strlcpy(s + _len, "-ESP", len - _len);
         break;
      case 'F':
         _len += strlcpy(s + _len, "-FRA", len - _len);
         break;
      case 'I':
         _len += strlcpy(s + _len, "-ITA", len - _len);
         break;
      case 'H':
         _len += strlcpy(s + _len, "-HOL", len - _len);
         break;
      default:
         return 0;
   }

   return _len;
}

int detect_scd_game(intfstream_t *fd, char *s, size_t len,
   const char *filename)
{
   #define SCD_SERIAL_OFFSET 0x0193
   #define SCD_SERIAL_LEN    11
   #define SCD_REGION_OFFSET 0x0200
   int index;
   size_t _len;
   char pre_game_id[SCD_SERIAL_LEN+1];
   char raw_game_id[SCD_SERIAL_LEN+1];
   char check_suffix_50[10];
   char region_id;
   char lgame_id[10];

   /* Load raw serial or quit */
   if (intfstream_seek(fd, SCD_SERIAL_OFFSET, SEEK_SET) < 0)
      return 0;
   if (intfstream_read(fd, raw_game_id, SCD_SERIAL_LEN) <= 0)
      return 0;
   raw_game_id[SCD_SERIAL_LEN] = '\0';

   /* Load raw region id or quit */
   if (intfstream_seek(fd, SCD_REGION_OFFSET, SEEK_SET) < 0)
      return 0;
   if (intfstream_read(fd, &region_id, 1) <= 0)
      return 0;

#ifdef DEBUG
   /** Scrub files with bad data and log **/
   if (raw_game_id[0] == '\0' || raw_game_id[0] == ' ' || raw_game_id[0] == '0')
      RARCH_LOG("[Scanner] Scrubbing: \"%s\".\n", filename);
#endif

   /** Convert raw Sega - Mega-CD - Sega CD serial to redump serial. **/
   /** process raw serial to a pre serial without spaces **/
   _len = string_remove_all_whitespace(pre_game_id, raw_game_id);
   /** Force minimum serial length since it is assumed **/
   if (_len < 2)
      _len = 2;
   strlcpy(check_suffix_50, &pre_game_id[_len - 2], sizeof(check_suffix_50));
   check_suffix_50[2] = '\0';

   /** redump serials are built differently for each prefix **/
   if (     pre_game_id[0] == 'T'
         && pre_game_id[1] == '-')
   {
      if (region_id == 'U' || region_id == 'J')
      {
         if ((index = string_index_last_occurance(pre_game_id, '-')) == -1)
            return 0;
         strlcpy(s, pre_game_id, (size_t)(index + 1));
         cue_append_multi_disc_suffix(s, filename);
         return 1;
      }
      if ((index = string_index_last_occurance(pre_game_id, '-')) == -1)
         return 0;
      strlcpy(lgame_id, pre_game_id, (size_t)(index + 1));
      _len        = strlcpy(s, lgame_id, len);
      s[  _len]   = '-';
      s[++_len]   = '5';
      s[++_len]   = '0';
      s[++_len]   = '\0';
      cue_append_multi_disc_suffix(s, filename);
   }
   else if (pre_game_id[0] == 'G'
         && pre_game_id[1] == '-')
   {
      if ((index = string_index_last_occurance(pre_game_id, '-')) == -1)
         return 0;
      strlcpy(s, pre_game_id, (size_t)(index + 1));
      cue_append_multi_disc_suffix(s, filename);
   }
   else if (pre_game_id[0] == 'M'
         && pre_game_id[1] == 'K'
         && pre_game_id[2] == '-')
   {
      if (     check_suffix_50[0] == '5'
            && check_suffix_50[1] == '0')
      {
         strlcpy(lgame_id, &pre_game_id[3], 5);
         _len        = strlcpy(s, lgame_id, len);
         s[  _len]   = '-';
         s[++_len]   = '5';
         s[++_len]   = '0';
         s[++_len]   = '\0';
      }
      else
         strlcpy(s, &pre_game_id[3], 5);
      cue_append_multi_disc_suffix(s, filename);
   }
   else
   {
      string_trim_whitespace_right(raw_game_id);
      string_trim_whitespace_left(raw_game_id);
      strlcpy(s, raw_game_id, len);
   }

   return 1;
}

int detect_sat_game(intfstream_t *fd, char *s, size_t len,
   const char *filename)
{
   #define SAT_SERIAL_OFFSET 0x0030
   #define SAT_SERIAL_LEN    9
   #define SAT_REGION_OFFSET 0x0050
   size_t _len, raw_len;
   char raw_game_id[SAT_SERIAL_LEN+1];
   char region_id;
   char check_suffix[3];
   char lgame_id[3];
   char rgame_id[10];

   /* Load raw serial or quit */
   if (intfstream_seek(fd, SAT_SERIAL_OFFSET, SEEK_SET) < 0)
      return 0;
   if (intfstream_read(fd, raw_game_id, SAT_SERIAL_LEN) <= 0)
      return 0;
   raw_game_id[SAT_SERIAL_LEN] = '\0';

   /* Load raw region id or quit */
   if (intfstream_seek(fd, SAT_REGION_OFFSET, SEEK_SET) < 0)
      return 0;
   if (intfstream_read(fd, &region_id, 1) <= 0)
      return 0;

   /** Scrub files with bad data and log **/
   if (raw_game_id[0] == '\0' || raw_game_id[0] == ' ')
   {
#ifdef DEBUG
      RARCH_LOG("[Scanner] Scrubbing: \"%s\".\n", filename);
#endif
      return 0;
   }

   string_trim_whitespace_right(raw_game_id);
   string_trim_whitespace_left(raw_game_id);

   /** Dissect this raw serial into parts **/
   raw_len = strlen(raw_game_id);

   if (raw_len < 2)
   {
      strlcpy(s, raw_game_id, len);
      return 1;
   }

   strlcpy(check_suffix, &raw_game_id[raw_len - 2], sizeof(check_suffix));

   /** redump serials are built differently for each region **/
   switch (region_id)
   {
      case 'B':  /* Brazil/multi-region (BKUT) */
      case 'T':  /* Taiwan/Asia */
      case 'K':  /* Korea */
      case 'A':  /* Americas */
      case 'U':  /* USA */
         if (     raw_game_id[0] == 'M'
               && raw_game_id[1] == 'K'
               && raw_game_id[2] == '-')
         {
            const char *serial_start = &raw_game_id[3];
            /* Strip leading zeros from serial number */
            while (*serial_start == '0' && *(serial_start + 1) != '\0')
               serial_start++;
            strlcpy(s, serial_start, len);
         }
         else
            strlcpy(s, raw_game_id, len);
         cue_append_multi_disc_suffix(s, filename);
         break;

      case 'E':
         strlcpy(lgame_id, raw_game_id, sizeof(lgame_id));
         if (     !strcmp(check_suffix, "-5")
               || !strcmp(check_suffix, "50"))
         {
            /* Copy chars from index 2, excluding last 2 chars of raw serial.
             * Clamp to rgame_id buffer size to prevent overflow. */
            size_t copy_len = raw_len - 4; /* chars to copy (excluding index 0-1 and last 2) */
            if (copy_len >= sizeof(rgame_id))
               copy_len = sizeof(rgame_id) - 1;
            memcpy(rgame_id, &raw_game_id[2], copy_len);
            rgame_id[copy_len] = '\0';
         }
         else
            strlcpy(rgame_id, &raw_game_id[2], sizeof(rgame_id));

         _len      = strlcpy(s, lgame_id, len);
         _len     += strlcpy(s + _len, rgame_id, len - _len);

         if (_len + 3 < len)
         {
            s[  _len] = '-';
            s[++_len] = '5';
            s[++_len] = '0';
            s[++_len] = '\0';
         }
         else
            s[_len] = '\0';

         cue_append_multi_disc_suffix(s, filename);
         break;

      case 'J':
         strlcpy(s, raw_game_id, len);
         cue_append_multi_disc_suffix(s, filename);
         break;

      default:
         strlcpy(s, raw_game_id, len);
         break;
   }
   return 1;
}

int detect_dc_game(intfstream_t *fd, char *s, size_t len,
   const char *filename)
{
   int index;
   int total_hyphens;
   int total_hyphens_recalc;
   size_t _len, __len, ___len;
   char pre_game_id[50];
   char raw_game_id[50];
   char lgame_id[20];
   char rgame_id[20];
   char region_id;

   /* Load raw serial or quit */
   if (intfstream_seek(fd, 0x0050, SEEK_SET) < 0)
      return 0;

   if (intfstream_read(fd, raw_game_id, 10) <= 0)
      return 0;

   if (intfstream_seek(fd, 0x0042, SEEK_SET) < 0)
      return false;

   if (intfstream_read(fd, &region_id, 1) <= 0)
      return false;

   raw_game_id[10] = '\0';

   /** Scrub files with bad data and log **/
   if (     raw_game_id[0] == '\0'
         || raw_game_id[0] == ' ')
   {
#ifdef DEBUG
      RARCH_LOG("[Scanner] Scrubbing: \"%s\".\n", filename);
#endif
      return 0;
   }

   string_trim_whitespace_right(raw_game_id);
   string_trim_whitespace_left(raw_game_id);
   string_replace_multi_space_with_single_space(raw_game_id);
   string_replace_whitespace_with_single_character(raw_game_id, '-');
   __len         = strlen(raw_game_id);
   total_hyphens = string_count_occurrences_single_character(raw_game_id, '-');

   /** redump serials are built differently for each prefix **/
   if (     raw_game_id[0] == 'T'
         && raw_game_id[1] == '-')
   {
      if (total_hyphens >= 2)
      {
         index             = string_index_last_occurance(raw_game_id, '-');
         if (index < 0)
            return 0;
         strlcpy(lgame_id, &raw_game_id[0], (size_t)index + 1);
         strlcpy(rgame_id, &raw_game_id[index + 1], sizeof(rgame_id));
         _len              = strlcpy(s, lgame_id, len);
         s[  _len]         = '-';
         s[++_len]         = '\0';
         strlcpy(s + _len, rgame_id, len - _len);
      }
      else if (__len <= 7)
         strlcpy(s, raw_game_id, 8);
      else
      {
         strlcpy(lgame_id, raw_game_id, 8);
         strlcpy(rgame_id, &raw_game_id[__len - 2], sizeof(rgame_id));
         _len              = strlcpy(s, lgame_id, len);
         s[  _len]         = '-';
         s[++_len]         = '\0';
         strlcpy(s + _len, rgame_id, len - _len);
      }
      cue_append_multi_disc_suffix(s, filename);
   }
   else if (raw_game_id[0] == 'T')
   {
      strlcpy(lgame_id, raw_game_id, 2);
      strlcpy(rgame_id, &raw_game_id[1], sizeof(rgame_id));
      _len                 = strlcpy(pre_game_id, lgame_id, sizeof(pre_game_id) - 2);
      pre_game_id[  _len]  = '-';
      pre_game_id[++_len]  = '\0';
      strlcpy(pre_game_id + _len, rgame_id, sizeof(pre_game_id) - _len);
      total_hyphens_recalc = string_count_occurrences_single_character(pre_game_id, '-');

      if (total_hyphens_recalc >= 2)
      {
         index             = string_index_last_occurance(pre_game_id, '-');
         if (index < 0)
            return 0;
         strlcpy(lgame_id, pre_game_id, (size_t)index + 1);
         ___len            = strlen(pre_game_id);
      }
      else
      {
         ___len = strlen(pre_game_id) - 1;
         if (___len <= 8)
         {
            strlcpy(s, pre_game_id, 9);
            cue_append_multi_disc_suffix(s, filename);
            return 1;
         }
         strlcpy(lgame_id, pre_game_id, 8);
      }
      strlcpy(rgame_id, &pre_game_id[___len - 2], sizeof(rgame_id));
      _len                        = strlcpy(s, lgame_id, len);
      s[  _len]                   = '-';
      s[++_len]                   = '\0';
      strlcpy(s + _len, rgame_id, len - _len);
      cue_append_multi_disc_suffix(s, filename);
   }
   else if (raw_game_id[0] == 'H'
         && raw_game_id[1] == 'D'
         && raw_game_id[2] == 'R'
         && raw_game_id[3] == '-')
   {
      if (total_hyphens >= 2)
      {
         index = string_index_last_occurance(raw_game_id, '-');
         if (index < 0)
            return 0;
         strlcpy(lgame_id, raw_game_id, (size_t)index);
         strlcpy(rgame_id, &raw_game_id[__len - 4], sizeof(rgame_id));
         _len                 = strlcpy(s, lgame_id, len);
         s[  _len]            = '-';
         s[++_len]            = '\0';
         strlcpy(s + _len, rgame_id, len - _len);
      }
      else
         strlcpy(s, raw_game_id, len);
      cue_append_multi_disc_suffix(s, filename);
   }
   else if (raw_game_id[0] == 'M'
         && raw_game_id[1] == 'K'
         && raw_game_id[2] == '-')
   {
      if (__len <= 8)
      {
         /* For 8 character serials in 'MK-xxxxx' format, 
          * we need to remove 'MK-' to match Redump database
          * Sega GT being the only exception (MK-51053), 
          * we have to check if it's not that game first */
         if (memcmp(raw_game_id, "MK-51053", STRLEN_CONST("MK-51053")) != 0)
         {
            /* Europe region serials need the MK- prefix and -50 postfix for database match. */
            if (region_id == 'E')
            {
               strlcpy(s, raw_game_id, 9);
               s[ 8] = '-';
               s[ 9] = '5';
               s[10] = '0';
               s[11] = '\0';
            }
            else
            {
               strlcpy(s, raw_game_id + 3, 6);
            }
         }
         else
            strlcpy(s, raw_game_id, 9);
      }
      else
      {
         strlcpy(lgame_id, raw_game_id, 9);
         strlcpy(rgame_id, &raw_game_id[__len - 2], sizeof(rgame_id));
         _len                 = strlcpy(s, lgame_id, len);
         s[  _len]            = '-';
         s[++_len]            = '\0';
         strlcpy(s + _len, rgame_id, len - _len);
      }
      cue_append_multi_disc_suffix(s, filename);
   }
   else
      strlcpy(s, raw_game_id, len);

   return 1;
}

size_t detect_wii_game(intfstream_t *fd, char *s, size_t len,
   const char *filename)
{
   char raw_game_id[15];

   /* Load raw serial or quit */
   if (intfstream_seek(fd, 0x0000, SEEK_SET) < 0)
      return 0;

   if (intfstream_read(fd, raw_game_id, 6) <= 0)
      return 0;

   if (memcmp(raw_game_id, "WBFS", STRLEN_CONST("WBFS")) == 0)
   {
      if (intfstream_seek(fd, 0x0200, SEEK_SET) < 0)
         return 0;
      if (intfstream_read(fd, raw_game_id, 6) <= 0)
         return 0;
   }

   if (     memcmp(raw_game_id, "RVZ", STRLEN_CONST("RVZ")) == 0
         || memcmp(raw_game_id, "WIA", STRLEN_CONST("WIA")) == 0
      )
   {
      if (intfstream_seek(fd, 0x0058, SEEK_SET) < 0)
         return 0;
      if (intfstream_read(fd, raw_game_id, 6) <= 0)
         return 0;
   }
   raw_game_id[6] = '\0';

   /** Scrub files with bad data and log **/
   if (     raw_game_id[0] == '\0'
         || raw_game_id[0] == ' ')
   {
#ifdef DEBUG
      RARCH_LOG("[Scanner] Scrubbing: \"%s\".\n", filename);
#endif
      return 0;
   }

   cue_append_multi_disc_suffix(s, filename);
   return strlcpy(s, raw_game_id, len);
}

int detect_system(intfstream_t *fd, const char **system_name,
   const char * filename)
{
   int i;
   char magic[50];
#ifdef DEBUG
   RARCH_LOG("[Scanner] %s\n", msg_hash_to_str(MSG_COMPARING_WITH_KNOWN_MAGIC_NUMBERS));
#endif
   for (i = 0; MAGIC_NUMBERS[i].system_name != NULL; i++)
   {
      if (intfstream_seek(fd, MAGIC_NUMBERS[i].offset, SEEK_SET) >= 0)
      {
         size_t _len = strlen(MAGIC_NUMBERS[i].magic);
         if (intfstream_read(fd, magic, _len) > 0)
         {
            magic[_len] = '\0';
            if (memcmp(MAGIC_NUMBERS[i].magic, magic, _len) == 0)
            {
               *system_name = MAGIC_NUMBERS[i].system_name;
#ifdef DEBUG
               RARCH_LOG("[Scanner] Name: %s\n", filename);
               RARCH_LOG("[Scanner] System: %s\n", MAGIC_NUMBERS[i].system_name);
#endif
               return 1;
            }
         }
      }
   }

#ifdef DEBUG
   RARCH_LOG("[Scanner] Name: %s\n", filename);
   RARCH_LOG("[Scanner] System: Unknown\n");
#endif
   return 0;
}

static int64_t intfstream_get_file_size(const char *path)
{
   int64_t rv;
   intfstream_t *fd = intfstream_open_file(path,
         RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!fd)
      return -1;
   rv = intfstream_get_size(fd);
   intfstream_close(fd);
   free(fd);
   return rv;
}

static bool update_cand(int64_t *cand_index, int64_t *last_index,
      uint64_t *largest, char *last_file, uint64_t *offset,
      size_t *size, char *s, size_t len)
{
   if (*cand_index != -1)
   {
      if ((uint64_t)(*last_index - *cand_index) > *largest)
      {
         size_t _len;
         *largest    = *last_index - *cand_index;
         _len        = strlcpy(s, last_file, len);
         *offset     = *cand_index;
         *size       = (size_t)*largest;
         *cand_index = -1;
         return _len;
      }
      *cand_index    = -1;
   }
   return 0;
}

int cue_find_track(const char *cue_path, bool first,
      uint64_t *offset, size_t *size, char *s, size_t len)
{
   int rv;
   intfstream_info_t info;
   char tmp_token[MAX_TOKEN_LEN];
   char last_file[PATH_MAX_LENGTH];
   char cue_dir[DIR_MAX_LENGTH];
   intfstream_t *fd           = NULL;
   int64_t last_index         = -1;
   int64_t cand_index         = -1;
   int32_t cand_track         = -1;
   int32_t track              = 0;
   uint64_t largest           = 0;
   int64_t volatile file_size = -1;
   bool is_data               = false;
   cue_dir[0] = last_file[0]  = '\0';

   fill_pathname_basedir(cue_dir, cue_path, sizeof(cue_dir));

   info.type                  = INTFSTREAM_FILE;

   if (!(fd = (intfstream_t*)intfstream_init(&info)))
      goto error;

   if (!intfstream_open(fd, cue_path,
            RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE))
   {
#ifdef DEBUG
      RARCH_LOG("[Scanner] Could not open CUE file \"%s\".\n", cue_path);
#endif
      goto error;
   }

#ifdef DEBUG
   RARCH_LOG("[Scanner] Parsing CUE file \"%s\"...\n", cue_path);
#endif

   tmp_token[0] = '\0';

   rv = -1;

   while (task_database_cue_get_token(fd, tmp_token, sizeof(tmp_token)) > 0)
   {
      if (string_is_equal_noncase(tmp_token, "FILE"))
      {
         /* Set last index to last EOF */
         if (file_size != -1)
            last_index = file_size;

         /* We're changing files since the candidate, update it */
         if (update_cand(&cand_index, &last_index,
                  &largest, last_file, offset,
                  size, s, len))
         {
            rv = 0;
            if (first)
               goto clean;
         }

         task_database_cue_get_token(fd, tmp_token, sizeof(tmp_token));
         fill_pathname_join_special(last_file, cue_dir,
               tmp_token, sizeof(last_file));

         file_size = intfstream_get_file_size(last_file);

         task_database_cue_get_token(fd, tmp_token, sizeof(tmp_token));

      }
      else if (string_is_equal_noncase(tmp_token, "TRACK"))
      {
         task_database_cue_get_token(fd, tmp_token, sizeof(tmp_token));
         task_database_cue_get_token(fd, tmp_token, sizeof(tmp_token));
         is_data = !string_is_equal_noncase(tmp_token, "AUDIO");
         ++track;

         /* Special case: CD-i stores data in AUDIO-labeled tracks */
         /* Check if track 1 is AUDIO but contains CD-i magic bytes */
         if (!is_data && track == 1 && last_file[0] != '\0')
         {
            uint8_t magic_buf[12];
            intfstream_info_t track_info;
            intfstream_t *track_fd    = NULL;
            const uint8_t cdi_magic[] = {0x00, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};

            track_info.type = INTFSTREAM_FILE;
            if ((track_fd = (intfstream_t*)intfstream_init(&track_info)))
            {
               if (intfstream_open(track_fd, last_file,
                     RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE))
               {
                  if (intfstream_read(track_fd, magic_buf, sizeof(magic_buf)) == sizeof(magic_buf))
                  {
                     if (memcmp(magic_buf, cdi_magic, sizeof(cdi_magic)) == 0)
                     {
                        is_data = true;  /* CD-i AUDIO track contains data */
#ifdef DEBUG
                        RARCH_LOG("[Scanner] Detected CD-i data in AUDIO track 1\n");
#endif
                     }
                  }
                  intfstream_close(track_fd);
               }
               free(track_fd);
            }
         }
      }
      else if (string_is_equal_noncase(tmp_token, "INDEX"))
      {
         int _m, _s, _f;
         task_database_cue_get_token(fd, tmp_token, sizeof(tmp_token));
         task_database_cue_get_token(fd, tmp_token, sizeof(tmp_token));

         {
            const char *ptr = tmp_token;
            char *end       = NULL;

            _m = (int)strtol(ptr, &end, 10);
            if (!end || *end != ':')
            {
#ifdef DEBUG
               RARCH_LOG("[Scanner] Error parsing time stamp \"%s\".\n", tmp_token);
#endif
               goto error;
            }

            ptr = end + 1;
            _s  = (int)strtol(ptr, &end, 10);
            if (!end || *end != ':')
            {
#ifdef DEBUG
               RARCH_LOG("[Scanner] Error parsing time stamp \"%s\".\n", tmp_token);
#endif
               goto error;
            }

            ptr = end + 1;
            _f  = (int)strtol(ptr, &end, 10);
            if (!end || (*end != '\0' && *end != ' ' && *end != '\t'))
            {
#ifdef DEBUG
               RARCH_LOG("[Scanner] Error parsing time stamp \"%s\".\n", tmp_token);
#endif
               goto error;
            }
         }

         last_index = (size_t)(((_m * 60 + _s) * 75) + _f) * 2352;
         /* If we've changed tracks since the candidate, update it */
         if (     (cand_track != -1)
               && (track != cand_track)
               && update_cand(&cand_index, &last_index, &largest,
                  last_file, offset,
                  size, s, len))
         {
            rv = 0;
            if (first)
               goto clean;
         }
         if (!is_data)
            continue;
         if (cand_index == -1)
         {
            cand_index = last_index;
            cand_track = track;
         }
      }
   }

   if (file_size != -1)
      last_index = file_size;

   if (update_cand(&cand_index, &last_index,
            &largest, last_file, offset,
            size, s, len))
      rv = 0;

clean:
   intfstream_close(fd);
   free(fd);
   return rv;

error:
   if (fd)
   {
      intfstream_close(fd);
      free(fd);
   }
   return -1;
}

bool cue_next_file(intfstream_t *fd,
      const char *cue_path, char *s, uint64_t len)
{
   char tmp_token[MAX_TOKEN_LEN];

   tmp_token[0] = '\0';

   while (task_database_cue_get_token(fd, tmp_token, sizeof(tmp_token)) > 0)
   {
      if (string_is_equal_noncase(tmp_token, "FILE"))
      {
         char cue_dir[DIR_MAX_LENGTH];
         fill_pathname_basedir(cue_dir, cue_path, sizeof(cue_dir));
         task_database_cue_get_token(fd, tmp_token, sizeof(tmp_token));
         fill_pathname_join_special(s, cue_dir, tmp_token, (size_t)len);
         return 1;
      }
   }
   return 0;
}

int gdi_find_track(const char *gdi_path, bool first, char *s, size_t len)
{
   intfstream_info_t info;
   char tmp_token[MAX_TOKEN_LEN];
   intfstream_t *fd  = NULL;
   uint64_t largest  = 0;
   int rv            = -1;
   int size          = -1;
   int mode          = -1;
   int64_t file_size = -1;

   info.type         = INTFSTREAM_FILE;

   if (!(fd = (intfstream_t*)intfstream_init(&info)))
      goto error;

   if (!intfstream_open(fd, gdi_path,
            RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE))
   {
#ifdef DEBUG
      RARCH_LOG("[Scanner] Could not open GDI file \"%s\".\n", gdi_path);
#endif
      goto error;
   }

#ifdef DEBUG
   RARCH_LOG("[Scanner] Parsing GDI file \"%s\"...\n", gdi_path);
#endif

   tmp_token[0] = '\0';

   /* Skip track count */
   task_database_cue_get_token(fd, tmp_token, sizeof(tmp_token));

   /* Track number */
   while (task_database_cue_get_token(fd, tmp_token, sizeof(tmp_token)) > 0)
   {
      /* Offset */
      if (task_database_cue_get_token(fd, tmp_token, sizeof(tmp_token)) <= 0)
         goto error;

      /* Mode */
      if (task_database_cue_get_token(fd, tmp_token, sizeof(tmp_token)) <= 0)
         goto error;

      mode = atoi(tmp_token);

      /* Sector size */
      if (task_database_cue_get_token(fd, tmp_token, sizeof(tmp_token)) <= 0)
         goto error;

      size = atoi(tmp_token);

      /* File name */
      if (task_database_cue_get_token(fd, tmp_token, sizeof(tmp_token)) <= 0)
         goto error;

      /* Check for data track */
      if (!(mode == 0 && size == 2352))
      {
         char last_file[PATH_MAX_LENGTH];
         char gdi_dir[DIR_MAX_LENGTH];

         fill_pathname_basedir(gdi_dir, gdi_path, sizeof(gdi_dir));
         fill_pathname_join_special(last_file,
               gdi_dir, tmp_token, sizeof(last_file));

         if ((file_size = intfstream_get_file_size(last_file)) < 0)
            goto error;

         if ((uint64_t)file_size > largest)
         {
            strlcpy(s, last_file, len);

            rv      = 0;
            largest = file_size;

            if (first)
               goto clean;
         }
      }

      /* Disc offset (not used?) */
      if (task_database_cue_get_token(fd, tmp_token, sizeof(tmp_token)) <= 0)
         goto error;
   }

clean:
   intfstream_close(fd);
   free(fd);
   return rv;

error:
   if (fd)
   {
      intfstream_close(fd);
      free(fd);
   }
   return -1;
}

size_t gdi_next_file(intfstream_t *fd, const char *gdi_path,
      char *s, size_t len)
{
   char tmp_token[MAX_TOKEN_LEN];

   tmp_token[0]    = '\0';

   /* Skip initial track count */
   if (intfstream_tell(fd) == 0)
      task_database_cue_get_token(fd, tmp_token, sizeof(tmp_token));

   task_database_cue_get_token(fd, tmp_token, sizeof(tmp_token)); /* Track number */
   task_database_cue_get_token(fd, tmp_token, sizeof(tmp_token)); /* Offset       */
   task_database_cue_get_token(fd, tmp_token, sizeof(tmp_token)); /* Mode         */
   task_database_cue_get_token(fd, tmp_token, sizeof(tmp_token)); /* Sector size  */

   /* File name */
   if (task_database_cue_get_token(fd, tmp_token, sizeof(tmp_token)) > 0)
   {
      size_t _len;
      char gdi_dir[DIR_MAX_LENGTH];
      fill_pathname_basedir(gdi_dir, gdi_path, sizeof(gdi_dir));
      _len = fill_pathname_join_special(s, gdi_dir, tmp_token, len);

      /* Disc offset */
      task_database_cue_get_token(fd, tmp_token, sizeof(tmp_token));
      return _len;
   }
   return 0;
}

int intfstream_get_serial(intfstream_t *fd, char *s, size_t len,
   const char *filename)
{
   const char *system_name = NULL;
   if (detect_system(fd, &system_name, filename) >= 1)
   {
      size_t system_len = strlen(system_name);
      if (string_starts_with_size(system_name, "Sony", STRLEN_CONST("Sony")))
      {
         if (   STRLEN_CONST("Sony - PlayStation Portable") == system_len
             && memcmp(system_name, "Sony - PlayStation Portable", system_len) == 0)
         {
            if (detect_psp_game(fd, s, len, filename) != 0)
               return 1;
         }
         else if (   STRLEN_CONST("Sony - PlayStation") == system_len
                  && memcmp(system_name, "Sony - PlayStation", system_len) == 0)
         {
            if (detect_ps1_game(fd, s, len, filename) != 0)
               return 1;
         }
         else if (   STRLEN_CONST("Sony - PlayStation 2") == system_len
                  && memcmp(system_name, "Sony - PlayStation 2", system_len) == 0)
         {
            if (detect_ps2_game(fd, s, len, filename) != 0)
               return 1;
         }
      }
      else if (string_starts_with_size(system_name, "Nintendo", STRLEN_CONST("Nintendo")))
      {
         if (   STRLEN_CONST("Nintendo - GameCube") == system_len
             && memcmp(system_name, "Nintendo - GameCube", system_len) == 0)
         {
            if (detect_gc_game(fd, s, len, filename) != 0)
               return 1;
         }
         else if (   STRLEN_CONST("Nintendo - Wii") == system_len
                  && memcmp(system_name, "Nintendo - Wii", system_len) == 0)
         {
            if (detect_wii_game(fd, s, len, filename) != 0)
               return 1;
         }
      }
      else if (string_starts_with_size(system_name, "Sega", STRLEN_CONST("Sega")))
      {
         if (   STRLEN_CONST("Sega - Mega-CD - Sega CD") == system_len
             && memcmp(system_name, "Sega - Mega-CD - Sega CD", system_len) == 0)
         {
            if (detect_scd_game(fd, s, len, filename) != 0)
               return 1;
         }
         else if (   STRLEN_CONST("Sega - Saturn") == system_len
                  && memcmp(system_name, "Sega - Saturn", system_len) == 0)
         {
            if (detect_sat_game(fd, s, len, filename) != 0)
               return 1;
         }
         else if (   STRLEN_CONST("Sega - Dreamcast") == system_len
                  && memcmp(system_name, "Sega - Dreamcast", system_len) == 0)
         {
            if (detect_dc_game(fd, s, len, filename) != 0)
               return 1;
         }
      }
      /* Philips CD-i has no serial entry on disc,
       * use default fallback to CRC */
   }
   return 0;
}

bool intfstream_file_get_serial(const char *name,
      uint64_t offset, int64_t size, char *s, size_t len, uint64_t *fsize)
{
   int rv;
   uint8_t *data     = NULL;
   int64_t file_size = -1;
   intfstream_t *fd  = intfstream_open_file(name,
         RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!fd)
      return 0;

   if (intfstream_seek(fd, 0, SEEK_END) == -1)
      goto error;

   file_size = intfstream_tell(fd);
   *fsize = file_size;

   if (intfstream_seek(fd, 0, SEEK_SET) == -1)
      goto error;

   if (file_size < 0)
      goto error;

   if (offset != 0 || size < file_size)
   {
      if (intfstream_seek(fd, (int64_t)offset, SEEK_SET) == -1)
         goto error;

      data = (uint8_t*)malloc(size);

      if (intfstream_read(fd, data, size) != (int64_t) size)
      {
         free(data);
         goto error;
      }

      intfstream_close(fd);
      free(fd);
      if (!(fd = intfstream_open_memory(data, RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE,
            size)))
      {
         free(data);
         return 0;
      }
   }

   rv = intfstream_get_serial(fd, s, len, name);
   intfstream_close(fd);
   free(fd);
   free(data);
   return rv;

error:
   intfstream_close(fd);
   free(fd);
   return 0;
}

int task_database_cue_get_serial(const char *name, char *s, size_t len,
   uint64_t *filesize)
{
   char track_path[PATH_MAX_LENGTH];
   uint64_t offset  = 0;
   size_t _len      = 0;

   track_path[0]    = '\0';

   if (cue_find_track(name, true, &offset, &_len,
         track_path, sizeof(track_path)) < 0)
   {
#ifdef DEBUG
      RARCH_LOG("[Scanner] %s\n",
            msg_hash_to_str(MSG_COULD_NOT_FIND_VALID_DATA_TRACK));
#endif
      return 0;
   }

   return intfstream_file_get_serial(track_path, offset, _len, s, len,
   filesize);
}

int task_database_gdi_get_serial(const char *name, char *s, size_t len,
   uint64_t *filesize)
{
   char track_path[PATH_MAX_LENGTH];

   track_path[0] = '\0';

   if (gdi_find_track(name, true,
               track_path, sizeof(track_path)) < 0)
   {
#ifdef DEBUG
      RARCH_LOG("[Scanner] %s\n",
            msg_hash_to_str(MSG_COULD_NOT_FIND_VALID_DATA_TRACK));
#endif
      return 0;
   }

   return intfstream_file_get_serial(track_path, 0, INT64_MAX, s, len,
   filesize);
}

/* Helper function to detect if a CHD file is a CD-i disc
 * CD-i discs store data in AUDIO-labeled tracks, so we need
 * to explicitly open track 1 for scanning */
bool is_chd_file_cdi(const char *path)
{
   uint8_t magic[12];
   const uint8_t cdi_magic[] = {0x00, 0xff, 0xff, 0xff, 0xff, 0xff,
                                0xff, 0xff, 0xff, 0xff, 0xff, 0x00};
   bool is_cdi      = false;
   /* Try to open track 1 explicitly */
   intfstream_t *fd = intfstream_open_chd_track(path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE,
         1);  /* Explicit track number, not CHDSTREAM_TRACK_FIRST_DATA */

   if (!fd)
      return 0;

   /* Read and check CD-i magic bytes at offset 0 */
   if (intfstream_read(fd, magic, sizeof(magic)) == sizeof(magic))
      is_cdi = (memcmp(magic, cdi_magic, sizeof(cdi_magic)) == 0);

   intfstream_close(fd);
   free(fd);
   return is_cdi;
}

int task_database_chd_get_serial(const char *name, char *serial,
   size_t len, uint64_t *filesize)
{
   int result;
   /* CD-i discs store data in AUDIO-labeled tracks, so we must
    * explicitly open track 1 instead of using CHDSTREAM_TRACK_FIRST_DATA */
   int32_t track    = is_chd_file_cdi(name) ? 1 : CHDSTREAM_TRACK_FIRST_DATA;
   intfstream_t *fd = intfstream_open_chd_track(
         name,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE,
         track);
   if (!fd)
      return 0;

   /* TODO/FIXME: get the full CHD size instead */
   *filesize = intfstream_get_size(fd);
   result    = intfstream_get_serial(fd, serial, len, name);
   intfstream_close(fd);
   free(fd);
   return result;
}

bool intfstream_file_get_crc_and_size(const char *name,
      uint64_t offset, int64_t len, uint32_t *crc, uint64_t *size)
{
   bool rv;
   intfstream_t *fd  = intfstream_open_file(name,
         RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   uint8_t *data     = NULL;
   int64_t file_size = -1;

   if (!fd)
      return 0;

   if (intfstream_seek(fd, 0, SEEK_END) == -1)
      goto error;

   file_size = intfstream_tell(fd);
   *size = file_size;

   if (intfstream_seek(fd, 0, SEEK_SET) == -1)
      goto error;

   if (file_size < 0)
      goto error;

   if (offset != 0 || len < file_size)
   {
      if (intfstream_seek(fd, (int64_t)offset, SEEK_SET) == -1)
         goto error;

      data = (uint8_t*)malloc(len);

      if (intfstream_read(fd, data, len) != (int64_t)len)
         goto error;

      intfstream_close(fd);
      free(fd);
      fd = intfstream_open_memory(data, RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE, len);

      if (!fd)
         goto error;
   }

   rv = intfstream_get_crc(fd, crc);
   intfstream_close(fd);
   free(fd);
   free(data);
   return rv;

error:
   if (fd)
   {
      intfstream_close(fd);
      free(fd);
   }
   if (data)
      free(data);
   return 0;
}

int task_database_cue_get_crc_and_size(const char *name, uint32_t *crc,
   uint64_t *size)
{
   char track_path[PATH_MAX_LENGTH];
   uint64_t offset  = 0;
   size_t _len      = 0;

   track_path[0]    = '\0';

   if (cue_find_track(name, false, &offset, &_len,
         track_path, sizeof(track_path)) < 0)
   {
#ifdef DEBUG
      RARCH_LOG("[Scanner] %s\n",
            msg_hash_to_str(MSG_COULD_NOT_FIND_VALID_DATA_TRACK));
#endif
      return 0;
   }
   return intfstream_file_get_crc_and_size(track_path, offset,
   _len, crc, size);
}

int task_database_gdi_get_crc_and_size(const char *name, uint32_t *crc,
   uint64_t *size)
{
   char track_path[PATH_MAX_LENGTH];

   track_path[0] = '\0';

   if (gdi_find_track(name, false,
       track_path, sizeof(track_path)) < 0)
   {
#ifdef DEBUG
      RARCH_LOG("[Scanner] %s\n",
            msg_hash_to_str(MSG_COULD_NOT_FIND_VALID_DATA_TRACK));
#endif
      return 0;
   }
   return intfstream_file_get_crc_and_size(track_path, 0, INT64_MAX, crc, size);
}

bool task_database_chd_get_crc_and_size(const char *name, uint32_t *crc,
   uint64_t *size)
{
   bool found_crc   = false;
   /* CD-i discs store data in AUDIO-labeled tracks, so we must
    * explicitly open track 1 instead of using CHDSTREAM_TRACK_PRIMARY */
   int32_t track    = is_chd_file_cdi(name) ? 1 : CHDSTREAM_TRACK_PRIMARY;
   intfstream_t *fd = intfstream_open_chd_track(
         name,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE,
         track);
   if (!fd)
      return 0;

   *size     = intfstream_get_size(fd);
   found_crc = intfstream_get_crc(fd, crc);
   if (fd)
   {
      intfstream_close(fd);
      free(fd);
   }
   return found_crc;
}
