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
   { "Sony - PlayStation Portable", "PSP GAME",         0x008008},
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
         char *dest = str1;
         sprintf(dest + strlen(dest), "-%i", disc_number - 1);
      }
   }
}

static int64_t task_database_cue_get_token(intfstream_t *fd, char *token, uint64_t max_len)
{
   char *c       = token;
   int64_t len   = 0;
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
            if (c == token)
               continue;

            if (!in_string)
            {
               *c = '\0';
               return len;
            }
            break;
         case '\"':
            if (c == token)
            {
               in_string = 1;
               continue;
            }

            *c = '\0';
            return len;
      }

      len++;
      c++;
      if (len == (int64_t)max_len)
      {
         *c = '\0';
         return len;
      }
   }
}

int detect_ps1_game(intfstream_t *fd, char *s, size_t len, const char *filename)
{
   #define DISC_DATA_SIZE_PS1 60000
   int pos;
   char raw_game_id[50];
   char disc_data[DISC_DATA_SIZE_PS1];

   /* Load data into buffer and use pointers */
   if (intfstream_seek(fd, 0, SEEK_SET) < 0)
      return false;

   if (intfstream_read(fd, disc_data, DISC_DATA_SIZE_PS1) <= 0)
      return false;

   disc_data[DISC_DATA_SIZE_PS1 - 1] = '\0';

   for (pos = 0; pos < DISC_DATA_SIZE_PS1; pos++)
   {
      strncpy(raw_game_id, &disc_data[pos], 12);
      raw_game_id[12] = '\0';
      if (     string_is_equal_fast(raw_game_id, "S", STRLEN_CONST("S"))
            || string_is_equal_fast(raw_game_id, "E", STRLEN_CONST("E")))
      {
         if (  string_is_equal_fast(raw_game_id, "SCUS_", STRLEN_CONST("SCUS_"))
            || string_is_equal_fast(raw_game_id, "SLUS_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "SLES_", STRLEN_CONST("SLES_"))
            || string_is_equal_fast(raw_game_id, "SCED_", STRLEN_CONST("SCED_"))
            || string_is_equal_fast(raw_game_id, "SLPS_", STRLEN_CONST("SLPS_"))
            || string_is_equal_fast(raw_game_id, "SLPM_", STRLEN_CONST("SLPM_"))
            || string_is_equal_fast(raw_game_id, "SCPS_", STRLEN_CONST("SCPS_"))
            || string_is_equal_fast(raw_game_id, "SLED_", STRLEN_CONST("SLED_"))
            || string_is_equal_fast(raw_game_id, "SIPS_", STRLEN_CONST("SIPS_"))
            || string_is_equal_fast(raw_game_id, "ESPM_", STRLEN_CONST("ESPM_"))
            || string_is_equal_fast(raw_game_id, "SCES_", STRLEN_CONST("SCES_"))
            || string_is_equal_fast(raw_game_id, "SLKA_", STRLEN_CONST("SLKA_"))
            || string_is_equal_fast(raw_game_id, "SCAJ_", STRLEN_CONST("SCAJ_"))
            )
         {
            raw_game_id[4] = '-';
            if (string_is_equal_fast(&raw_game_id[8], ".", STRLEN_CONST(".")))
            {
               raw_game_id[8] = raw_game_id[9];
               raw_game_id[9] = raw_game_id[10];
            }
            /* A few games have their serial in the form of xx.xxx */
            /* Tanaka Torahiko no Ultra-ryuu Shougi - Ibisha Anaguma-hen (Japan) -> SLPS_02.261 */
            else if (string_is_equal_fast(&raw_game_id[7], ".", STRLEN_CONST(".")))
            {
               raw_game_id[7] = raw_game_id[8];
               raw_game_id[8] = raw_game_id[9];
               raw_game_id[9] = raw_game_id[10];
            }
            raw_game_id[10] = '\0';

            string_remove_all_whitespace(s, raw_game_id);
            cue_append_multi_disc_suffix(s, filename);
            return true;
         }
      }
      else if (string_is_equal_fast(raw_game_id, "LSP-", STRLEN_CONST("LSP-")))
      {
         raw_game_id[10] = '\0';

         string_remove_all_whitespace(s, raw_game_id);
         cue_append_multi_disc_suffix(s, filename);
         return true;
      }
      else if (string_is_equal_fast(raw_game_id, "PSX.EXE", STRLEN_CONST("PSX.EXE")))
      {
         raw_game_id[7] = '\0';

         string_remove_all_whitespace(s, raw_game_id);
         cue_append_multi_disc_suffix(s, filename);
         return true;
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
   return false;
}

int detect_ps2_game(intfstream_t *fd, char *s, size_t len, const char *filename)
{
   #define DISC_DATA_SIZE_PS2 0x84000
   int pos;
   char raw_game_id[50];
   char disc_data[DISC_DATA_SIZE_PS2];

   /* Load data into buffer and use pointers */
   if (intfstream_seek(fd, 0, SEEK_SET) < 0)
      return false;

   if (intfstream_read(fd, disc_data, DISC_DATA_SIZE_PS2) <= 0)
      return false;

   disc_data[DISC_DATA_SIZE_PS2 - 1] = '\0';

   for (pos = 0; pos < DISC_DATA_SIZE_PS2; pos++)
   {
      strncpy(raw_game_id, &disc_data[pos], 12);
      raw_game_id[12] = '\0';
      if (     string_is_equal_fast(raw_game_id, "S", STRLEN_CONST("S"))
            || string_is_equal_fast(raw_game_id, "P", STRLEN_CONST("P"))
            || string_is_equal_fast(raw_game_id, "T", STRLEN_CONST("T"))
            || string_is_equal_fast(raw_game_id, "C", STRLEN_CONST("C"))
            || string_is_equal_fast(raw_game_id, "H", STRLEN_CONST("H"))
            || string_is_equal_fast(raw_game_id, "A", STRLEN_CONST("A"))
            || string_is_equal_fast(raw_game_id, "V", STRLEN_CONST("A"))
            || string_is_equal_fast(raw_game_id, "L", STRLEN_CONST("A"))
            || string_is_equal_fast(raw_game_id, "M", STRLEN_CONST("A"))
            || string_is_equal_fast(raw_game_id, "N", STRLEN_CONST("A"))
            || string_is_equal_fast(raw_game_id, "U", STRLEN_CONST("A"))
            || string_is_equal_fast(raw_game_id, "W", STRLEN_CONST("A"))
            || string_is_equal_fast(raw_game_id, "G", STRLEN_CONST("A"))
            || string_is_equal_fast(raw_game_id, "K", STRLEN_CONST("A"))
            || string_is_equal_fast(raw_game_id, "R", STRLEN_CONST("A"))
         )
      {
         if (  string_is_equal_fast(raw_game_id, "SLPM_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "SLES_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "SCES_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "SLUS_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "SLPS_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "SCED_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "SCUS_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "SCPS_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "SCAJ_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "SLKA_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "SCKA_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "SLAJ_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "TCPS_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "KOEI_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "PBPX_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "PCPX_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "PAPX_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "SCCS_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "ALCH_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "TCES_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "CPCS_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "SLED_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "TLES_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "GUST_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "CF00_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "SCPN_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "SCPM_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "PSXC_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "SLPN_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "ULKS_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "LDTL_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "PKP2_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "WLFD_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "CZP2_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "HAKU_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "SRPM_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "MTP2_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "NMP2_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "ARZE_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "VUGJ_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "ARP2_", STRLEN_CONST("SLUS_"))
            || string_is_equal_fast(raw_game_id, "ROSE_", STRLEN_CONST("SLUS_"))
            )
         {
            raw_game_id[4] = '-';
            if (string_is_equal_fast(&raw_game_id[8], ".", STRLEN_CONST(".")))
            {
               raw_game_id[8] = raw_game_id[9];
               raw_game_id[9] = raw_game_id[10];
            }
            /* A few games have their serial in the form of xx.xxx */
            /* Tanaka Torahiko no Ultra-ryuu Shougi - Ibisha Anaguma-hen (Japan) -> SLPS_02.261 */
            else if (string_is_equal_fast(&raw_game_id[7], ".", STRLEN_CONST(".")))
            {
               raw_game_id[7] = raw_game_id[8];
               raw_game_id[8] = raw_game_id[9];
               raw_game_id[9] = raw_game_id[10];
            }
            raw_game_id[10] = '\0';

            string_remove_all_whitespace(s, raw_game_id);
            cue_append_multi_disc_suffix(s, filename);
            return true;
         }
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
   return false;
}

int detect_psp_game(intfstream_t *fd, char *s, size_t len, const char *filename)
{
   #define DISC_DATA_SIZE_PSP 40000
   int pos;
   char disc_data[DISC_DATA_SIZE_PSP];

   /* Load data into buffer and use pointers */
   if (intfstream_seek(fd, 0, SEEK_SET) < 0)
      return false;

   if (intfstream_read(fd, disc_data, DISC_DATA_SIZE_PSP) <= 0)
      return false;

   disc_data[DISC_DATA_SIZE_PSP - 1] = '\0';

   for (pos = 0; pos < DISC_DATA_SIZE_PSP; pos++)
   {
      strncpy(s, &disc_data[pos], 10);
      s[10] = '\0';
      if (     string_is_equal_fast(s, "U", STRLEN_CONST("U"))
            || string_is_equal_fast(s, "N", STRLEN_CONST("N")))
      {
         if (
            (   string_is_equal_fast(s, "ULES-", STRLEN_CONST("ULES-")))
            || (string_is_equal_fast(s, "ULUS-", STRLEN_CONST("ULUS-")))
            || (string_is_equal_fast(s, "ULJS-", STRLEN_CONST("ULJS-")))

            || (string_is_equal_fast(s, "ULEM-", STRLEN_CONST("ULEM-")))
            || (string_is_equal_fast(s, "ULUM-", STRLEN_CONST("ULUM-")))
            || (string_is_equal_fast(s, "ULJM-", STRLEN_CONST("ULJM-")))

            || (string_is_equal_fast(s, "UCES-", STRLEN_CONST("UCES-")))
            || (string_is_equal_fast(s, "UCUS-", STRLEN_CONST("UCUS-")))
            || (string_is_equal_fast(s, "UCJS-", STRLEN_CONST("UCJS-")))
            || (string_is_equal_fast(s, "UCAS-", STRLEN_CONST("UCAS-")))
            || (string_is_equal_fast(s, "UCKS-", STRLEN_CONST("UCKS-")))

            || (string_is_equal_fast(s, "ULKS-", STRLEN_CONST("ULKS-")))
            || (string_is_equal_fast(s, "ULAS-", STRLEN_CONST("ULAS-")))
            || (string_is_equal_fast(s, "NPEH-", STRLEN_CONST("NPEH-")))
            || (string_is_equal_fast(s, "NPUH-", STRLEN_CONST("NPUH-")))
            || (string_is_equal_fast(s, "NPJH-", STRLEN_CONST("NPJH-")))
            || (string_is_equal_fast(s, "NPHH-", STRLEN_CONST("NPHH-")))

            || (string_is_equal_fast(s, "NPEG-", STRLEN_CONST("NPEG-")))
            || (string_is_equal_fast(s, "NPUG-", STRLEN_CONST("NPUG-")))
            || (string_is_equal_fast(s, "NPJG-", STRLEN_CONST("NPJG-")))
            || (string_is_equal_fast(s, "NPHG-", STRLEN_CONST("NPHG-")))

            || (string_is_equal_fast(s, "NPEZ-", STRLEN_CONST("NPEZ-")))
            || (string_is_equal_fast(s, "NPUZ-", STRLEN_CONST("NPUZ-")))
            || (string_is_equal_fast(s, "NPJZ-", STRLEN_CONST("NPJZ-")))
            )
         {
            cue_append_multi_disc_suffix(s, filename);
            return true;
         }
      }
   }

   return false;
}

int detect_gc_game(intfstream_t *fd, char *s, size_t len, const char *filename)
{
   size_t _len;
   char region_id;
   char pre_game_id[20];
   char raw_game_id[20];

   /* Load raw serial or quit */
   if (intfstream_seek(fd, 0, SEEK_SET) < 0)
      return false;

   if (intfstream_read(fd, raw_game_id, 4) <= 0)
      return false;

   if (string_is_equal_fast(raw_game_id, "RVZ", STRLEN_CONST("RVZ"))
         || string_is_equal_fast(raw_game_id, "WIA", STRLEN_CONST("WIA")))
   {
      if (intfstream_seek(fd, 0x0058, SEEK_SET) < 0)
         return false;
      if (intfstream_read(fd, raw_game_id, 4) <= 0)
         return false;
   }

   raw_game_id[4] = '\0';

   /** Scrub files with bad data and log **/
   if (raw_game_id[0] == '\0' || raw_game_id[0] == ' ')
   {
#ifdef DEBUG
      RARCH_LOG("[Scanner]: Scrubbing: %s\n", filename);
#endif
      return false;
   }

   /** convert raw gamecube serial to redump serial.
   not enough is known about the disc data to properly
   convert every raw serial to redump serial.  it will
   only fail with the following excpetions: the
   subregions of europe P-UKV, P-AUS, X-UKV, X-EUU
   will not match redump.**/

   /** insert prefix **/
   pre_game_id[0  ] = 'D';
   pre_game_id[1  ] = 'L';
   pre_game_id[2  ] = '-';
   pre_game_id[3  ] = 'D';
   pre_game_id[4  ] = 'O';
   pre_game_id[5  ] = 'L';
   pre_game_id[6  ] = '-';
   pre_game_id[7  ] = '\0';

   /** add raw serial **/
   strlcat(pre_game_id, raw_game_id, sizeof(pre_game_id));

   /** check region **/
   region_id = pre_game_id[10];

   /** check multi-disc and insert suffix **/
   cue_append_multi_disc_suffix(pre_game_id, filename);
   _len = strlcpy(s, pre_game_id, len);

   switch (region_id)
   {
      case 'E':
         s[_len  ] = '-';
         s[_len+1] = 'U';
         s[_len+2] = 'S';
         s[_len+3] = 'A';
         s[_len+4] = '\0';
         return true;
      case 'J':
         s[_len  ] = '-';
         s[_len+1] = 'J';
         s[_len+2] = 'P';
         s[_len+3] = 'N';
         s[_len+4] = '\0';
         return true;
      case 'P': /** NYI: P can also be P-UKV, P-AUS **/
      case 'X': /** NYI: X can also be X-UKV, X-EUU **/
         s[_len  ] = '-';
         s[_len+1] = 'E';
         s[_len+2] = 'U';
         s[_len+3] = 'R';
         s[_len+4] = '\0';
         return true;
      case 'Y':
         s[_len  ] = '-';
         s[_len+1] = 'F';
         s[_len+2] = 'A';
         s[_len+3] = 'H';
         s[_len+4] = '\0';
         return true;
      case 'D':
         s[_len  ] = '-';
         s[_len+1] = 'N';
         s[_len+2] = 'O';
         s[_len+3] = 'E';
         s[_len+4] = '\0';
         return true;
      case 'S':
         s[_len  ] = '-';
         s[_len+1] = 'E';
         s[_len+2] = 'S';
         s[_len+3] = 'P';
         s[_len+4] = '\0';
         return true;
      case 'F':
         s[_len  ] = '-';
         s[_len+1] = 'F';
         s[_len+2] = 'R';
         s[_len+3] = 'A';
         s[_len+4] = '\0';
         return true;
      case 'I':
         s[_len  ] = '-';
         s[_len+1] = 'I';
         s[_len+2] = 'T';
         s[_len+3] = 'A';
         s[_len+4] = '\0';
         return true;
      case 'H':
         s[_len  ] = '-';
         s[_len+1] = 'H';
         s[_len+2] = 'O';
         s[_len+3] = 'L';
         s[_len+4] = '\0';
         return true;
      default:
    break;
   }

   return false;
}

int detect_scd_game(intfstream_t *fd, char *s, size_t len, const char *filename)
{
   #define SCD_SERIAL_OFFSET 0x0193
   #define SCD_SERIAL_LEN    11
   #define SCD_REGION_OFFSET 0x0200
   size_t _len;
   char pre_game_id[SCD_SERIAL_LEN+1];
   char raw_game_id[SCD_SERIAL_LEN+1];
   char check_suffix_50[10];
   char region_id;
   size_t length;
   size_t lengthref;
   int index;
   char lgame_id[10];

   /* Load raw serial or quit */
   if (intfstream_seek(fd, SCD_SERIAL_OFFSET, SEEK_SET) < 0)
      return false;

   if (intfstream_read(fd, raw_game_id, SCD_SERIAL_LEN) <= 0)
      return false;

   raw_game_id[SCD_SERIAL_LEN] = '\0';

   /* Load raw region id or quit */
   if (intfstream_seek(fd, SCD_REGION_OFFSET, SEEK_SET) < 0)
      return false;

   if (intfstream_read(fd, &region_id, 1) <= 0)
      return false;

#ifdef DEBUG
   /** Scrub files with bad data and log **/
   if (raw_game_id[0] == '\0' || raw_game_id[0] == ' ' || raw_game_id[0] == '0')
      RARCH_LOG("[Scanner]: Scrubbing: %s\n", filename);
#endif

   /** convert raw Sega - Mega-CD - Sega CD serial to redump serial. **/
   /** process raw serial to a pre serial without spaces **/
   string_remove_all_whitespace(pre_game_id, raw_game_id);  /** rule: remove all spaces from the raw serial globally **/

   /** Dissect this pre serial into parts **/
   length             = strlen(pre_game_id);
   lengthref          = length - 2;
   strncpy(check_suffix_50, &pre_game_id[lengthref], length - 2 + 1);
   check_suffix_50[2] = '\0';

   /** redump serials are built differently for each prefix **/
   if (pre_game_id[0] == 'T' && pre_game_id[1] == '-')
   {
      if (region_id == 'U' || region_id == 'J')
      {
         if ((index = string_index_last_occurance(pre_game_id, '-')) == -1)
            return false;
         strncpy(s, pre_game_id, index);
         s[index] = '\0';
         cue_append_multi_disc_suffix(s, filename);
         return true;
      }
      if ((index = string_index_last_occurance(pre_game_id, '-')) == -1)
         return false;
      strncpy(lgame_id, pre_game_id, index);
      lgame_id[index] = '\0';
      _len            = strlcat(s, lgame_id, len);
      s[_len  ]       = '-';
      s[_len+1]       = '5';
      s[_len+2]       = '0';
      s[_len+3]       = '\0';
      cue_append_multi_disc_suffix(s, filename);
      return true;
   }
   else if (pre_game_id[0] == 'G' && pre_game_id[1] == '-')
   {
      if ((index = string_index_last_occurance(pre_game_id, '-')) == -1)
         return false;
      strncpy(s, pre_game_id, index);
      s[index] = '\0';
      cue_append_multi_disc_suffix(s, filename);
      return true;
   }
   else if (pre_game_id[0] == 'M'
         && pre_game_id[1] == 'K'
         && pre_game_id[2] == '-')
   {
      if (     check_suffix_50[0] == '5'
            && check_suffix_50[1] == '0')
      {
         strncpy(lgame_id, &pre_game_id[3], 4);
         lgame_id[4]     = '\0';
         _len            = strlcat(s, lgame_id, len);
         s[_len  ]       = '-';
         s[_len+1]       = '5';
         s[_len+2]       = '0';
         s[_len+3]       = '\0';
         cue_append_multi_disc_suffix(s, filename);
         return true;
      }
      strncpy(s, &pre_game_id[3], 4);
      s[4] = '\0';
      cue_append_multi_disc_suffix(s, filename);
      return true;
   }
   else
   {
      string_trim_whitespace(raw_game_id);
      strlcpy(s, raw_game_id, len);
      return true;
   }
   return false;
}

int detect_sat_game(intfstream_t *fd, char *s, size_t len, const char *filename)
{
   #define SAT_SERIAL_OFFSET 0x0030
   #define SAT_SERIAL_LEN    9
   #define SAT_REGION_OFFSET 0x0050
   size_t _len, length;
   char raw_game_id[SAT_SERIAL_LEN+1];
   char region_id;
   char check_suffix_5[10];
   char check_suffix_50[10];
   char lgame_id[10];
   char rgame_id[10];

   /* Load raw serial or quit */
   if (intfstream_seek(fd, SAT_SERIAL_OFFSET, SEEK_SET) < 0)
      return false;

   if (intfstream_read(fd, raw_game_id, SAT_SERIAL_LEN) <= 0)
      return false;

   raw_game_id[SAT_SERIAL_LEN] = '\0';

   /* Load raw region id or quit */
   if (intfstream_seek(fd, SAT_REGION_OFFSET, SEEK_SET) < 0)
      return false;

   if (intfstream_read(fd, &region_id, 1) <= 0)
      return false;

   /** Scrub files with bad data and log **/
   if (raw_game_id[0] == '\0' || raw_game_id[0] == ' ')
   {
#ifdef DEBUG
      RARCH_LOG("[Scanner]: Scrubbing: %s\n", filename);
#endif
      return false;
   }

   string_trim_whitespace(raw_game_id);

   /** Dissect this raw serial into parts **/
   length             = strlen(raw_game_id);
   strncpy(check_suffix_5,  &raw_game_id[length - 2], 2);
   check_suffix_5[2]  = '\0';
   strncpy(check_suffix_50, &raw_game_id[length - 2], 2);
   check_suffix_50[2] = '\0';

   /** redump serials are built differently for each region **/
   switch (region_id)
   {
      case 'U':
         if (     raw_game_id[0] == 'M'
               && raw_game_id[1] == 'K'
               && raw_game_id[2] == '-')
         {
            strncpy(s, &raw_game_id[3], length - 3);
            s[length - 3] = '\0';
            cue_append_multi_disc_suffix(s, filename);
         }
         else
         {
            strlcpy(s, raw_game_id, len);
            cue_append_multi_disc_suffix(s, filename);
         }
         return true;
      case 'E':
         strncpy(lgame_id, &raw_game_id[0], 2);
         lgame_id[2] = '\0';
         if (     !strcmp(check_suffix_5, "-5")
               || !strcmp(check_suffix_50, "50"))
         {
            strncpy(rgame_id, &raw_game_id[2], length - 4);
            rgame_id[length - 4] = '\0';
         }
         else
         {
            strncpy(rgame_id, &raw_game_id[2], length - 1);
            rgame_id[length - 1] = '\0';
         }
         strlcat(s, lgame_id, len);
         _len      = strlcat(s, rgame_id, len);
         s[_len  ] = '-';
         s[_len+1] = '5';
         s[_len+2] = '0';
         s[_len+3] = '\0';
         cue_append_multi_disc_suffix(s, filename);
         return true;
      case 'J':
         strlcpy(s, raw_game_id, len);
         cue_append_multi_disc_suffix(s, filename);
         return true;
      default:
         strlcpy(s, raw_game_id, len);
         return true;
   }
   return false;
}

int detect_dc_game(intfstream_t *fd, char *s, size_t len, const char *filename)
{
   size_t _len;
   int total_hyphens;
   int total_hyphens_recalc;
   char pre_game_id[50];
   char raw_game_id[50];
   size_t length;
   size_t length_recalc;
   int index;
   size_t size_t_var;
   char lgame_id[20];
   char rgame_id[20];

   /* Load raw serial or quit */
   if (intfstream_seek(fd, 0x0050, SEEK_SET) < 0)
      return false;

   if (intfstream_read(fd, raw_game_id, 10) <= 0)
      return false;

   raw_game_id[10] = '\0';

   /** Scrub files with bad data and log **/
   if (raw_game_id[0] == '\0' || raw_game_id[0] == ' ')
   {
#ifdef DEBUG
      RARCH_LOG("[Scanner]: Scrubbing: %s\n", filename);
#endif
      return false;
   }

   string_trim_whitespace(raw_game_id);
   string_replace_multi_space_with_single_space(raw_game_id);
   string_replace_whitespace_with_single_character(raw_game_id, '-');
   length        = strlen(raw_game_id);
   total_hyphens = string_count_occurrences_single_character(raw_game_id, '-');

   /** redump serials are built differently for each prefix **/
   if (     raw_game_id[0] == 'T'
         && raw_game_id[1] == '-')
   {
      if (total_hyphens >= 2)
      {
         index                = string_index_last_occurance(raw_game_id, '-');
         if (index < 0)
            return false;
         size_t_var           = (size_t)index;
         strncpy(lgame_id, &raw_game_id[0], size_t_var);
         lgame_id[index]      = '\0';
         strncpy(rgame_id, &raw_game_id[index + 1], length - 1);
         rgame_id[length - 1] = '\0';
         _len                 = strlcat(s, lgame_id, len);
         s[_len  ]            = '-';
         s[_len+1]            = '\0';
         strlcat(s, rgame_id, len);
         cue_append_multi_disc_suffix(s, filename);
         return true;
      }
      if (length <= 7)
      {
         strncpy(s, raw_game_id, 7);
         s[7] = '\0';
         cue_append_multi_disc_suffix(s, filename);
         return true;
      }
      strncpy(lgame_id, raw_game_id, 7);
      lgame_id[7]          = '\0';
      strncpy(rgame_id, &raw_game_id[length - 2], length - 1);
      rgame_id[length - 1] = '\0';
      _len                 = strlcat(s, lgame_id, len);
      s[_len  ]            = '-';
      s[_len+1]            = '\0';
      strlcat(s, rgame_id, len);
      cue_append_multi_disc_suffix(s, filename);
      return true;
   }
   else if (raw_game_id[0] == 'T')
   {
      strncpy(lgame_id, raw_game_id, 1);
      lgame_id[1]          = '\0';
      strncpy(rgame_id, &raw_game_id[1], length - 1);
      rgame_id[length - 1] = '\0';
      _len                 = strlcpy(pre_game_id, lgame_id, sizeof(pre_game_id));
      pre_game_id[_len  ]  = '-';
      pre_game_id[_len+1]  = '\0';
      strlcat(pre_game_id, rgame_id, sizeof(pre_game_id));
      total_hyphens_recalc = string_count_occurrences_single_character(pre_game_id, '-');

      if (total_hyphens_recalc >= 2)
      {
         index                       = string_index_last_occurance(pre_game_id, '-');
         if (index < 0)
            return false;
         size_t_var                  = (size_t)index;
         strncpy(lgame_id, pre_game_id, size_t_var);
         lgame_id[index]             = '\0';
         length_recalc               = strlen(pre_game_id);
         strncpy(rgame_id, &pre_game_id[length_recalc - 2], length_recalc - 1);
         rgame_id[length_recalc - 1] = '\0';
         _len                        = strlcat(s, lgame_id, len);
         s[_len  ]                   = '-';
         s[_len+1]                   = '\0';
         strlcat(s, rgame_id, len);
         cue_append_multi_disc_suffix(s, filename);
      }
      else
      {
         length_recalc = strlen(pre_game_id) - 1;
         if (length_recalc <= 8)
         {
            strncpy(s, pre_game_id, 8);
            s[8] = '\0';
            cue_append_multi_disc_suffix(s, filename);
            return true;
         }
         strncpy(lgame_id, pre_game_id, 7);
         lgame_id[7] = '\0';
         strncpy(rgame_id, &pre_game_id[length_recalc - 2], length_recalc - 1);
         rgame_id[length_recalc - 1] = '\0';
         _len                        = strlcat(s, lgame_id, len);
         s[_len  ]                   = '-';
         s[_len+1]                   = '\0';
         strlcat(s, rgame_id ,len);
         cue_append_multi_disc_suffix(s, filename);
      }
      return true;
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
            return false;
         strncpy(lgame_id, raw_game_id, index - 1);
         lgame_id[index - 1]  = '\0';
         strncpy(rgame_id, &raw_game_id[length - 4], length - 3);
         rgame_id[length - 3] = '\0';
         _len                 = strlcat(s, lgame_id, len);
         s[_len  ]            = '-';
         s[_len+1]            = '\0';
         strlcat(s, rgame_id, len);
         cue_append_multi_disc_suffix(s, filename);
         return true;
      }
      strlcpy(s, raw_game_id, len);
      cue_append_multi_disc_suffix(s, filename);
      return true;
   }
   else if (raw_game_id[0] == 'M'
         && raw_game_id[1] == 'K'
         && raw_game_id[2] == '-')
   {
      if (length <= 8)
      {
         /* For 8 chars serials in 'MK-xxxxx' format, we need to remove 'MK-' to match Redump database
          * Sega GT being the only exception (MK-51053), we have to check if it's not that game first */
         if (string_is_not_equal_fast(raw_game_id, "MK-51053", STRLEN_CONST("MK-51053")))
         {
            strncpy(s, raw_game_id + 3, 5);
            s[5] = '\0';
            cue_append_multi_disc_suffix(s, filename);
            return true;
         }
         strncpy(s, raw_game_id, 8);
         s[8] = '\0';
         cue_append_multi_disc_suffix(s, filename);
         return true;
      }
      strncpy(lgame_id, raw_game_id, 8);
      lgame_id[8]          = '\0';
      strncpy(rgame_id, &raw_game_id[length - 2], length - 1);
      rgame_id[length - 1] = '\0';
      _len                 = strlcat(s, lgame_id, len);
      s[_len  ]            = '-';
      s[_len+1]            = '\0';
      strlcat(s, rgame_id, len);
      cue_append_multi_disc_suffix(s, filename);
      return true;
   }
   else
   {
      strlcpy(s, raw_game_id, len);
      return true;
   }

   return false;
}

int detect_wii_game(intfstream_t *fd, char *s, size_t len, const char *filename)
{
   char raw_game_id[15];

   /* Load raw serial or quit */
   if (intfstream_seek(fd, 0x0000, SEEK_SET) < 0)
      return false;

   if (intfstream_read(fd, raw_game_id, 6) <= 0)
      return false;

   if (string_is_equal_fast(raw_game_id, "WBFS", STRLEN_CONST("WBFS")))
   {
      if (intfstream_seek(fd, 0x0200, SEEK_SET) < 0)
         return false;
      if (intfstream_read(fd, raw_game_id, 6) <= 0)
         return false;
   }

   if (string_is_equal_fast(raw_game_id, "RVZ", STRLEN_CONST("RVZ"))
         || string_is_equal_fast(raw_game_id, "WIA", STRLEN_CONST("WIA")))
   {
      if (intfstream_seek(fd, 0x0058, SEEK_SET) < 0)
         return false;
      if (intfstream_read(fd, raw_game_id, 6) <= 0)
         return false;
   }
   raw_game_id[6] = '\0';

   /** Scrub files with bad data and log **/
   if (raw_game_id[0] == '\0' || raw_game_id[0] == ' ')
   {
#ifdef DEBUG
      RARCH_LOG("[Scanner]: Scrubbing: %s\n", filename);
#endif
      return false;
   }

   cue_append_multi_disc_suffix(s, filename);
   strlcpy(s, raw_game_id, len);
   return true;
}

#if 0
/**
 * Check for an ASCII serial in the first few bits of the ISO (Wii).
 * TODO/FIXME - unused for now
 */
static int detect_serial_ascii_game(intfstream_t *fd, char *s, size_t len)
{
   unsigned pos;
   int number_of_ascii = 0;
   bool rv             = false;

   for (pos = 0; pos < 10000; pos++)
   {
      intfstream_seek(fd, pos, SEEK_SET);
      if (intfstream_read(fd, s, 15) > 0)
      {
         unsigned i;
         s[15]           = '\0';
         number_of_ascii = 0;

         /* When scanning WBFS files, "WBFS" is discovered as the first serial. Ignore it. */
         if (string_is_equal(s, "WBFS"))
            continue;

         /* Loop through until we run out of ASCII characters. */
         for (i = 0; i < 15; i++)
         {
            /* Is the given character ASCII? A-Z, 0-9, - */
            if (  (s[i] == 45) ||
                  (s[i] >= 48 && s[i] <= 57) ||
                  (s[i] >= 65 && s[i] <= 90))
               number_of_ascii++;
            else
               break;
         }

         /* If the length of the text is between 3 and 9 characters,
          * it could be a serial. */
         if (number_of_ascii > 3 && number_of_ascii < 9)
         {
            /* Cut the string off, and return it as a valid serial. */
            s[number_of_ascii]       = '\0';
            rv                       = true;
            break;
         }
      }
   }

   return rv;
}
#endif

int detect_system(intfstream_t *fd, const char **system_name, const char * filename)
{
   int i;
   char magic[50];
#ifdef DEBUG
   RARCH_LOG("[Scanner]: %s\n", msg_hash_to_str(MSG_COMPARING_WITH_KNOWN_MAGIC_NUMBERS));
#endif
   for (i = 0; MAGIC_NUMBERS[i].system_name != NULL; i++)
   {
      if (intfstream_seek(fd, MAGIC_NUMBERS[i].offset, SEEK_SET) >= 0)
      {
         size_t magic_len = strlen(MAGIC_NUMBERS[i].magic);
         if (intfstream_read(fd, magic, magic_len) > 0)
         {
            magic[magic_len] = '\0';
            if (memcmp(MAGIC_NUMBERS[i].magic, magic, magic_len) == 0)
            {
               *system_name = MAGIC_NUMBERS[i].system_name;
#ifdef DEBUG
               RARCH_LOG("[Scanner]: Name: %s\n", filename);
               RARCH_LOG("[Scanner]: System: %s\n", MAGIC_NUMBERS[i].system_name);
#endif
               return true;
            }
         }
      }
   }

#ifdef DEBUG
   RARCH_LOG("[Scanner]: Name: %s\n", filename);
   RARCH_LOG("[Scanner]: System: Unknown\n");
#endif
   return false;
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
      uint64_t *size, char *track_path, uint64_t max_len)
{
   if (*cand_index != -1)
   {
      if ((uint64_t)(*last_index - *cand_index) > *largest)
      {
         *largest    = *last_index - *cand_index;
         strlcpy(track_path, last_file, (size_t)max_len);
         *offset     = *cand_index;
         *size       = *largest;
         *cand_index = -1;
         return true;
      }
      *cand_index    = -1;
   }
   return false;
}

int cue_find_track(const char *cue_path, bool first,
      uint64_t *offset, uint64_t *size, char *track_path, uint64_t max_len)
{
   int rv;
   intfstream_info_t info;
   char tmp_token[MAX_TOKEN_LEN];
   char last_file[PATH_MAX_LENGTH];
   char cue_dir[PATH_MAX_LENGTH];
   intfstream_t *fd           = NULL;
   int64_t last_index         = -1;
   int64_t cand_index         = -1;
   int32_t cand_track         = -1;
   int32_t track              = 0;
   uint64_t largest             = 0;
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
      RARCH_LOG("Could not open CUE file '%s'\n", cue_path);
#endif
      goto error;
   }

#ifdef DEBUG
   RARCH_LOG("Parsing CUE file '%s'...\n", cue_path);
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
                  size, track_path, max_len))
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
      }
      else if (string_is_equal_noncase(tmp_token, "INDEX"))
      {
         int m, s, f;
         task_database_cue_get_token(fd, tmp_token, sizeof(tmp_token));
         task_database_cue_get_token(fd, tmp_token, sizeof(tmp_token));

         if (sscanf(tmp_token, "%02d:%02d:%02d", &m, &s, &f) < 3)
         {
#ifdef DEBUG
            RARCH_LOG("Error parsing time stamp '%s'\n", tmp_token);
#endif
            goto error;
         }

         last_index = (size_t) (((m * 60 + s) * 75) + f) * 2352;

         /* If we've changed tracks since the candidate, update it */
         if (cand_track != -1 && track != cand_track &&
             update_cand(&cand_index, &last_index, &largest,
                last_file, offset,
                size, track_path, max_len))
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
            size, track_path, max_len))
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
   char cue_dir[PATH_MAX_LENGTH];
   cue_dir[0]                 = '\0';

   fill_pathname_basedir(cue_dir, cue_path, sizeof(cue_dir));

   tmp_token[0] = '\0';

   while (task_database_cue_get_token(fd, tmp_token, sizeof(tmp_token)) > 0)
   {
      if (string_is_equal_noncase(tmp_token, "FILE"))
      {
         task_database_cue_get_token(fd, tmp_token, sizeof(tmp_token));
         fill_pathname_join_special(s, cue_dir, tmp_token, (size_t)len);
         return true;
      }
   }

   return false;
}

int gdi_find_track(const char *gdi_path, bool first,
      char *track_path, uint64_t max_len)
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
      RARCH_LOG("Could not open GDI file '%s'\n", gdi_path);
#endif
      goto error;
   }

#ifdef DEBUG
   RARCH_LOG("Parsing GDI file '%s'...\n", gdi_path);
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
         char gdi_dir[PATH_MAX_LENGTH];

         fill_pathname_basedir(gdi_dir, gdi_path, sizeof(gdi_dir));
         fill_pathname_join_special(last_file,
               gdi_dir, tmp_token, sizeof(last_file));

         if ((file_size = intfstream_get_file_size(last_file)) < 0)
            goto error;

         if ((uint64_t)file_size > largest)
         {
            strlcpy(track_path, last_file, (size_t)max_len);

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

bool gdi_next_file(intfstream_t *fd, const char *gdi_path,
      char *path, uint64_t max_len)
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
      char gdi_dir[PATH_MAX_LENGTH];

      fill_pathname_basedir(gdi_dir, gdi_path, sizeof(gdi_dir));
      fill_pathname_join_special(path, gdi_dir, tmp_token, (size_t)max_len);

      /* Disc offset */
      task_database_cue_get_token(fd, tmp_token, sizeof(tmp_token));
      return true;
   }

   return false;
}
