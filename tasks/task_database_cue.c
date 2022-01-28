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

#include <errno.h>
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
#include "../retroarch.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "../database_info.h"

#include "tasks_internal.h"

#include "../list_special.h"
#include "../msg_hash.h"
#include "../verbosity.h"

#define MAGIC_LEN       17
#define MAX_TOKEN_LEN   255

#ifdef MSB_FIRST
#define MODETEST_VAL    0x00ffffff
#else
#define MODETEST_VAL    0xffffff00
#endif

/* TODO/FIXME - reorder this according to CODING-GUIDELINES
 * and make sure LUT table below conforms */
struct magic_entry
{
   int32_t offset;
   const char *system_name;
   const char *magic;
   int length_magic;
};

static struct magic_entry MAGIC_NUMBERS[] = {
   { 0x000010,   "Sega - Mega-CD - Sega CD",      "\x53\x45\x47\x41\x44\x49\x53\x43\x53\x59\x53\x54\x45\x4d",       14},
   { 0x000010,   "Sega - Saturn",                 "\x53\x45\x47\x41\x20\x53\x45\x47\x41\x53\x41\x54\x55\x52\x4e",   15},
   { 0x000010,   "Sega - Dreamcast",              "\x53\x45\x47\x41\x20\x53\x45\x47\x41\x4b\x41\x54\x41\x4e\x41",   15},
   { 0x000018,   "Nintendo - Wii",                "\x5d\x1c\x9e\xa3",                                               4},
   { 0x00001c,   "Nintendo - GameCube",           "\xc2\x33\x9f\x3d",                                               4},
   { 0x008008,   "Sony - PlayStation Portable",   "\x50\x53\x50\x20\x47\x41\x4d\x45",                               8},
   { 0x008008,   "Sony - PlayStation",            "\x50\x4c\x41\x59\x53\x54\x41\x54\x49\x4f\x4e",                   11},
   { 0x009320,   "Sony - PlayStation",            "\x50\x4c\x41\x59\x53\x54\x41\x54\x49\x4f\x4e",                   11},
   { 0,          NULL,                            NULL,                                                             0}
};

/**
 * Given a filename and position, find the associated disc number.
 */
int cue_find_disc_number(const char* str1, int index)
{
   char disc;
   int disc_number = 0;

   disc = str1[index + 6];

   switch(disc)
   {
      case 'a':
      case 'A':
         disc_number = 1;
         break;
      case 'b':
      case 'B':
         disc_number = 2;
         break;
      case 'c':
      case 'C':
         disc_number = 3;
         break;
      case 'd':
      case 'D':
         disc_number = 4;
         break;
      case 'e':
      case 'E':
         disc_number = 5;
         break;
      case 'f':
      case 'F':
         disc_number = 6;
         break;
      case 'g':
      case 'G':
         disc_number = 7;
         break;
      case 'h':
      case 'H':
         disc_number = 8;
         break;
      case 'i':
      case 'I':
         disc_number = 9;
         break;
      default:
         disc_number = disc - '0';
         break;
   }

   if (disc_number >= 1)
      return disc_number;

   return 0;
}

/**
 * Given a title and filename, append the appropriate disc number to it.
 */
void cue_append_multi_disc_suffix(char * str1, const char *filename)
{
   char *dest = str1;
   int result = 0;
   int disc_number = 0;

   /** check multi-disc and insert suffix **/
   result = string_find_index_substring_string(filename, "(Disc ");
   if (result < 0)
      result = string_find_index_substring_string(filename, "(disc ");
   if (result < 0)
      result = string_find_index_substring_string(filename, "(Disk ");
   if (result < 0)
      result = string_find_index_substring_string(filename, "(disk ");
   if (result >= 0)
   {
      disc_number = cue_find_disc_number(filename, result);
      if (disc_number > 0)
         sprintf(dest+strlen(dest), "-%i", disc_number - 1);
   }
}

static int64_t get_token(intfstream_t *fd, char *token, uint64_t max_len)
{
   char *c       = token;
   int64_t len   = 0;
   int in_string = 0;

   for (;;)
   {
      int64_t rv = (int64_t)intfstream_read(fd, c, 1);
      if (rv == 0)
         return 0;

      if (rv < 1)
      {
         switch (errno)
         {
            case EINTR:
            case EAGAIN:
               continue;
            default:
               return -errno;
         }
      }

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

int detect_ps1_game(intfstream_t *fd, char *game_id, const char *filename)
{
   #define DISC_DATA_SIZE_PS1 60000
   int pos;
   char raw_game_id[50];
   char disc_data[DISC_DATA_SIZE_PS1];
   char hyphen = '-';

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
      if (string_is_equal_fast(raw_game_id, "S", 1) || string_is_equal_fast(raw_game_id, "E", 1))
      {
         if (
            (string_is_equal_fast(raw_game_id, "SCUS_", 5))
            || (string_is_equal_fast(raw_game_id, "SLUS_", 5))
            || (string_is_equal_fast(raw_game_id, "SLES_", 5))
            || (string_is_equal_fast(raw_game_id, "SCED_", 5))
            || (string_is_equal_fast(raw_game_id, "SLPS_", 5))
            || (string_is_equal_fast(raw_game_id, "SLPM_", 5))
            || (string_is_equal_fast(raw_game_id, "SCPS_", 5))
            || (string_is_equal_fast(raw_game_id, "SLED_", 5))
            || (string_is_equal_fast(raw_game_id, "SIPS_", 5))
            || (string_is_equal_fast(raw_game_id, "ESPM_", 5))
            || (string_is_equal_fast(raw_game_id, "SCES_", 5))
            )
         {
            raw_game_id[4] = hyphen;
            if (string_is_equal_fast(&raw_game_id[8], ".", 1))
            {
               raw_game_id[8] = raw_game_id[9];
               raw_game_id[9] = raw_game_id[10];
            }
            raw_game_id[10] = '\0';

            string_remove_all_whitespace(game_id, raw_game_id);
            cue_append_multi_disc_suffix(game_id, filename);
            return true;
         }
         else if (string_is_equal_fast(&disc_data[pos], "LSP-", 4))
         {
            string_remove_all_whitespace(game_id, raw_game_id);
            game_id[10] = '\0';
            cue_append_multi_disc_suffix(game_id, filename);
            return true;
         }
      }
   }

   strcpy(game_id, "XXXXXXXXXX");
   game_id[10] = '\0';
   cue_append_multi_disc_suffix(game_id, filename);
   return true;
}

int detect_psp_game(intfstream_t *fd, char *game_id, const char *filename)
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
      strncpy(game_id, &disc_data[pos], 10);
      game_id[10] = '\0';
      if (string_is_equal_fast(game_id, "U", 1) || string_is_equal_fast(game_id, "N", 1))
      {
         if (
            (string_is_equal_fast(game_id, "ULES-", 5))
            || (string_is_equal_fast(game_id, "ULUS-", 5))
            || (string_is_equal_fast(game_id, "ULJS-", 5))

            || (string_is_equal_fast(game_id, "ULEM-", 5))
            || (string_is_equal_fast(game_id, "ULUM-", 5))
            || (string_is_equal_fast(game_id, "ULJM-", 5))

            || (string_is_equal_fast(game_id, "UCES-", 5))
            || (string_is_equal_fast(game_id, "UCUS-", 5))
            || (string_is_equal_fast(game_id, "UCJS-", 5))
            || (string_is_equal_fast(game_id, "UCAS-", 5))
            || (string_is_equal_fast(game_id, "UCKS-", 5))

            || (string_is_equal_fast(game_id, "ULKS-", 5))
            || (string_is_equal_fast(game_id, "ULAS-", 5))
            || (string_is_equal_fast(game_id, "NPEH-", 5))
            || (string_is_equal_fast(game_id, "NPUH-", 5))
            || (string_is_equal_fast(game_id, "NPJH-", 5))
            || (string_is_equal_fast(game_id, "NPHH-", 5))

            || (string_is_equal_fast(game_id, "NPEG-", 5))
            || (string_is_equal_fast(game_id, "NPUG-", 5))
            || (string_is_equal_fast(game_id, "NPJG-", 5))
            || (string_is_equal_fast(game_id, "NPHG-", 5))

            || (string_is_equal_fast(game_id, "NPEZ-", 5))
            || (string_is_equal_fast(game_id, "NPUZ-", 5))
            || (string_is_equal_fast(game_id, "NPJZ-", 5))
            )
         {
            cue_append_multi_disc_suffix(game_id, filename);
            return true;
         }
      }
   }

   return false;
}

int detect_gc_game(intfstream_t *fd, char *game_id, const char *filename)
{
   char region_id;
   char prefix[] = "DL-DOL-";
   char pre_game_id[20];
   char raw_game_id[20];

   /* Load raw serial or quit */
   if (intfstream_seek(fd, 0, SEEK_SET) < 0)
      return false;

   if (intfstream_read(fd, raw_game_id, 4) <= 0)
      return false;

   raw_game_id[4] = '\0';

   /** Scrub files with bad data and log **/
   if (raw_game_id[0] == '\0' || raw_game_id[0] == ' ')
   {
      RARCH_LOG("[Scanner]: Scrubbing: %s\n", filename);
      return false;
   }

   /** convert raw gamecube serial to redump serial.
   not enough is known about the disc data to properly
   convert every raw serial to redump serial.  it will
   only fail with the following excpetions: the
   subregions of europe P-UKV, P-AUS, X-UKV, X-EUU
   will not match redump.**/

   /** insert prefix **/
   strcpy(pre_game_id, prefix);

   /** add raw serial **/
   strcat(pre_game_id, raw_game_id);

   /** check region **/
   region_id = pre_game_id[10];

   /** check multi-disc and insert suffix **/
   cue_append_multi_disc_suffix(pre_game_id, filename);
   strcpy(game_id, pre_game_id);

   switch (region_id)
   {
      case 'E':
         strcat(game_id, "-USA");
         return true;
      case 'J':
         strcat(game_id, "-JPN");
         return true;
      case 'P': /** NYI: P can also be P-UKV, P-AUS **/
         strcat(game_id, "-EUR");
         return true;
      case 'X': /** NYI: X can also be X-UKV, X-EUU **/
         strcat(game_id, "-EUR");
         return true;
      case 'Y':
         strcat(game_id, "-FAH");
         return true;
      case 'D':
         strcat(game_id, "-NOE");
         return true;
      case 'S':
         strcat(game_id, "-ESP");
         return true;
      case 'F':
         strcat(game_id, "-FRA");
         return true;
      case 'I':
         strcat(game_id, "-ITA");
         return true;
      case 'H':
         strcat(game_id, "-HOL");
         return true;
      default:
         return false;
   }

   return false;
}

int detect_scd_game(intfstream_t *fd, char *game_id, const char *filename)
{
   char hyphen = '-';
   char pre_game_id[15];
   char raw_game_id[15];
   char check_prefix_t_hyp[10];
   char check_suffix_50[10];
   char check_prefix_g_hyp[10];
   char check_prefix_mk_hyp[10];
   char region_id[10];
   int length;
   int lengthref;
   int index;
   char lgame_id[10];
   char rgame_id[] = "-50";

   /* Load raw serial or quit */
   if (intfstream_seek(fd, 0x0193, SEEK_SET) < 0)
      return false;

   if (intfstream_read(fd, raw_game_id, 11) <= 0)
      return false;

   raw_game_id[11] = '\0';

   if (raw_game_id[0] == ' ')
   {
      if (intfstream_seek(fd, 0x0194, SEEK_SET) < 0)
         return false;
      if (intfstream_read(fd, raw_game_id, 11) <= 0)
         return false;
      raw_game_id[11] = '\0';
   }

   /* Load raw region id or quit */
   if (intfstream_seek(fd, 0x0200, SEEK_SET) < 0)
      return false;

   if (intfstream_read(fd, region_id, 1) <= 0)
      return false;

   region_id[1] = '\0';

   /** Scrub files with bad data and log **/
   if (raw_game_id[0] == '\0' || raw_game_id[0] == ' ' || raw_game_id[0] == '0')
   {
      RARCH_LOG("[Scanner]: Scrubbing: %s\n", filename);
   }

   /** convert raw Sega - Mega-CD - Sega CD serial to redump serial. **/
   /** process raw serial to a pre serial without spaces **/
   string_remove_all_whitespace(pre_game_id, raw_game_id);  /** rule: remove all spaces from the raw serial globally **/

   /** disect this pre serial into parts **/
   length = strlen(pre_game_id);
   lengthref = length - 2;
   strncpy(check_prefix_t_hyp, pre_game_id, 2);
   check_prefix_t_hyp[2] = '\0';
   strncpy(check_prefix_g_hyp, pre_game_id, 2);
   check_prefix_g_hyp[2] = '\0';
   strncpy(check_prefix_mk_hyp, pre_game_id, 3);
   check_prefix_mk_hyp[3] = '\0';
   strncpy(check_suffix_50, &pre_game_id[lengthref], length - 2 + 1);
   check_suffix_50[2] = '\0';

   /** redump serials are built differently for each prefix **/
   if (!strcmp(check_prefix_t_hyp, "T-"))
   {
      if (!strcmp(region_id, "U") || !strcmp(region_id, "J"))
      {
         index = string_index_last_occurance(pre_game_id, hyphen);
         if (index == -1)
            return false;
         strncpy(game_id, pre_game_id, index);
         game_id[index] = '\0';
         cue_append_multi_disc_suffix(game_id, filename);
         return true;
      }
      else
      {
         index = string_index_last_occurance(pre_game_id, hyphen);
         if (index == -1)
            return false;
         strncpy(lgame_id, pre_game_id, index);
         lgame_id[index] = '\0';
         strcat(game_id, lgame_id);
         strcat(game_id, rgame_id);
         cue_append_multi_disc_suffix(game_id, filename);
         return true;
      }
   }
   else if (!strcmp(check_prefix_g_hyp, "G-"))
   {
      index = string_index_last_occurance(pre_game_id, hyphen);
      if (index == -1)
         return false;
      strncpy(game_id, pre_game_id, index);
      game_id[index] = '\0';
      cue_append_multi_disc_suffix(game_id, filename);
      return true;
   }
   else if (!strcmp(check_prefix_mk_hyp, "MK-"))
   {
      if (!strcmp(check_suffix_50, "50"))
      {
         strncpy(lgame_id, &pre_game_id[3], 4);
         lgame_id[4] = '\0';
         strcat(game_id, lgame_id);
         strcat(game_id, rgame_id);
         cue_append_multi_disc_suffix(game_id, filename);
         return true;
      }
      else
      {
         strncpy(game_id, &pre_game_id[3], 4);
         game_id[4] = '\0';
         cue_append_multi_disc_suffix(game_id, filename);
         return true;
      }
   }
   else
   {
      string_trim_whitespace(raw_game_id);
      strcpy(game_id, raw_game_id);
      return true;
   }
   return false;
}

int detect_sat_game(intfstream_t *fd, char *game_id, const char *filename)
{
   char hyphen = '-';
   char raw_game_id[15];
   char raw_region_id[15];
   char region_id;
   char check_prefix_t_hyp[10];
   char check_prefix_mk_hyp[10];
   char check_suffix_5[10];
   char check_suffix_50[10];
   int length;
   char lgame_id[10];
   char rgame_id[10];
   char game_id50[] = "-50";

   /* Load raw serial or quit */
   if (intfstream_seek(fd, 0x0030, SEEK_SET) < 0)
      return false;

   if (intfstream_read(fd, raw_game_id, 9) <= 0)
      return false;

   raw_game_id[9] = '\0';

   /* Load raw region id or quit */
   if (intfstream_seek(fd, 0x0050, SEEK_SET) < 0)
      return false;

   if (intfstream_read(fd, raw_region_id, 1) <= 0)
      return false;

   raw_region_id[1] = '\0';

   /** Scrub files with bad data and log **/
   if (raw_game_id[0] == '\0' || raw_game_id[0] == ' ')
   {
      RARCH_LOG("[Scanner]: Scrubbing: %s\n", filename);
      return false;
   }

   region_id = raw_region_id[0];

   string_trim_whitespace(raw_game_id);

   /** disect this raw serial into parts **/
   strncpy(check_prefix_t_hyp, raw_game_id, 2);
   check_prefix_t_hyp[2] = '\0';
   strncpy(check_prefix_mk_hyp, raw_game_id, 3);
   check_prefix_mk_hyp[3] = '\0';
   length = strlen(raw_game_id);
   strncpy(check_suffix_5, &raw_game_id[length - 2], 2);
   check_suffix_5[2] = '\0';
   strncpy(check_suffix_50, &raw_game_id[length - 2], 2);
   check_suffix_50[2] = '\0';

   /** redump serials are built differently for each region **/
   switch (region_id)
   {
      case 'U':
         if (strcmp(check_prefix_mk_hyp, "MK-") == 0)
         {
            strncpy(game_id, &raw_game_id[3], length - 3);
            game_id[length - 3] = '\0';
            cue_append_multi_disc_suffix(game_id, filename);
         }
         else
         {
            strcpy(game_id, raw_game_id);
            cue_append_multi_disc_suffix(game_id, filename);
         }
         return true;
      case 'E':
         strncpy(lgame_id, &raw_game_id[0], 2);
         lgame_id[2] = '\0';
         if (strcmp(check_suffix_5, "-5") == 0 || strcmp(check_suffix_50, "50") == 0)
         {
            strncpy(rgame_id, &raw_game_id[2], length - 4);
            rgame_id[length - 4] = '\0';
         }
         else
         {
            strncpy(rgame_id, &raw_game_id[2], length - 1);
            rgame_id[length - 1] = '\0';
         }
         strcat(game_id, lgame_id);
         strcat(game_id, rgame_id);
         strcat(game_id, game_id50);
         cue_append_multi_disc_suffix(game_id, filename);
         return true;
      case 'J':
         strcpy(game_id, raw_game_id);
         cue_append_multi_disc_suffix(game_id, filename);
         return true;
      default:
         strcpy(game_id, raw_game_id);
         return true;
   }
   return false;
}

int detect_dc_game(intfstream_t *fd, char *game_id, const char *filename)
{
   char hyphen = '-';
   char hyphen_str[] = "-";
   int total_hyphens;
   int total_hyphens_recalc;
   char pre_game_id[50];
   char raw_game_id[50];
   char check_prefix_t_hyp[10];
   char check_prefix_t[10];
   char check_prefix_hdr_hyp[10];
   char check_prefix_mk_hyp[10];
   int length;
   int length_recalc;
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
      RARCH_LOG("[Scanner]: Scrubbing: %s\n", filename);
      return false;
   }

   string_trim_whitespace(raw_game_id);
   string_replace_multi_space_with_single_space(raw_game_id);
   string_replace_whitespace_with_single_character(raw_game_id, hyphen);
   length = strlen(raw_game_id);
   total_hyphens = string_count_occurrences_single_character(raw_game_id, hyphen);

   /** disect this raw serial into parts **/
   strncpy(check_prefix_t_hyp, raw_game_id, 2);
   check_prefix_t_hyp[2] = '\0';
   strncpy(check_prefix_t, raw_game_id, 1);
   check_prefix_t[1] = '\0';
   strncpy(check_prefix_hdr_hyp, raw_game_id, 4);
   check_prefix_hdr_hyp[4] = '\0';
   strncpy(check_prefix_mk_hyp, raw_game_id, 3);
   check_prefix_mk_hyp[3] = '\0';

   /** redump serials are built differently for each prefix **/
   if (!strcmp(check_prefix_t_hyp, "T-"))
   {
      if (total_hyphens >= 2)
      {
         index = string_index_last_occurance(raw_game_id, hyphen);
         if (index < 0)
            return false;
         else
            size_t_var = (size_t)index;
         strncpy(lgame_id, &raw_game_id[0], size_t_var);
         lgame_id[index] = '\0';
         strncpy(rgame_id, &raw_game_id[index + 1], length - 1);
         rgame_id[length - 1] = '\0';
         strcat(game_id, lgame_id);
         strcat(game_id, hyphen_str);
         strcat(game_id, rgame_id);
         cue_append_multi_disc_suffix(game_id, filename);
         return true;
      }
      else
      {
         if (length <= 7)
         {
            strncpy(game_id, raw_game_id, 7);
            game_id[7] = '\0';
            cue_append_multi_disc_suffix(game_id, filename);
            return true;
         }
         else
         {
            strncpy(lgame_id, raw_game_id, 7);
            lgame_id[7] = '\0';
            strncpy(rgame_id, &raw_game_id[length - 2], length - 1);
            rgame_id[length - 1] = '\0';
            strcat(game_id, lgame_id);
            strcat(game_id, hyphen_str);
            strcat(game_id, rgame_id);
            cue_append_multi_disc_suffix(game_id, filename);
            return true;
         }
      }
   }
   else if (!strcmp(check_prefix_t, "T"))
   {
      strncpy(lgame_id, raw_game_id, 1);
      lgame_id[1] = '\0';
      strncpy(rgame_id, &raw_game_id[1], length - 1);
      rgame_id[length - 1] = '\0';
      sprintf(pre_game_id, "%s%s%s", lgame_id, hyphen_str, rgame_id);
      total_hyphens_recalc = string_count_occurrences_single_character(pre_game_id, hyphen);

      if (total_hyphens_recalc >= 2)
      {
         index = string_index_last_occurance(pre_game_id, hyphen);
         if (index < 0)
            return false;
         else
            size_t_var = (size_t)index;
         strncpy(lgame_id, pre_game_id, size_t_var);
         lgame_id[index] = '\0';
         length_recalc = strlen(pre_game_id);
         strncpy(rgame_id, &pre_game_id[length_recalc - 2], length_recalc - 1);
         rgame_id[length_recalc - 1] = '\0';
         strcat(game_id, lgame_id);
         strcat(game_id, hyphen_str);
         strcat(game_id, rgame_id);
         cue_append_multi_disc_suffix(game_id, filename);
         return true;
      }
      else
      {
         length_recalc = strlen(pre_game_id) - 1;
         if (length_recalc <= 8)
         {
            strncpy(game_id, pre_game_id, 8);
            game_id[8] = '\0';
            cue_append_multi_disc_suffix(game_id, filename);
            return true;
         }
         else
         {
            strncpy(lgame_id, pre_game_id, 7);
            lgame_id[7] = '\0';
            strncpy(rgame_id, &pre_game_id[length_recalc - 2], length_recalc - 1);
            rgame_id[length_recalc - 1] = '\0';
            strcat(game_id, lgame_id);
            strcat(game_id, hyphen_str);
            strcat(game_id, rgame_id);
            cue_append_multi_disc_suffix(game_id, filename);
            return true;
         }
      }
   }
   else if (!strcmp(check_prefix_hdr_hyp, "HDR-"))
   {
      if (total_hyphens >= 2)
      {
         index = string_index_last_occurance(raw_game_id, hyphen);
         if (index < 0)
            return false;
         else
            size_t_var = (size_t)index;
         strncpy(lgame_id, raw_game_id, index - 1);
         lgame_id[index - 1] = '\0';
         strncpy(rgame_id, &raw_game_id[length - 4], length - 3);
         rgame_id[length - 3] = '\0';
         strcat(game_id, lgame_id);
         strcat(game_id, hyphen_str);
         strcat(game_id, rgame_id);
         cue_append_multi_disc_suffix(game_id, filename);
         return true;
      }
      else
      {
         strcpy(game_id, raw_game_id);
         cue_append_multi_disc_suffix(game_id, filename);
         return true;
      }
   }
   else if (!strcmp(check_prefix_mk_hyp, "MK-"))
   {

      if (length <= 8)
      {
         strncpy(game_id, raw_game_id, 8);
         game_id[8] = '\0';
         cue_append_multi_disc_suffix(game_id, filename);
         return true;
      }
      else
      {
         strncpy(lgame_id, raw_game_id, 8);
         lgame_id[8] = '\0';
         strncpy(rgame_id, &raw_game_id[length - 2], length - 1);
         rgame_id[length - 1] = '\0';
         strcat(game_id, lgame_id);
         strcat(game_id, hyphen_str);
         strcat(game_id, rgame_id);
         cue_append_multi_disc_suffix(game_id, filename);
         return true;
      }
   }
   else
   {
      strcpy(game_id, raw_game_id);
      return true;
   }

   return false;
}

int detect_wii_game(intfstream_t *fd, char *game_id, const char *filename)
{
   char raw_game_id[15];

   /* Load raw serial or quit */
   if (intfstream_seek(fd, 0x0000, SEEK_SET) < 0)
      return false;

   if (intfstream_read(fd, raw_game_id, 6) <= 0)
      return false;

   raw_game_id[6] = '\0';

   /** Scrub files with bad data and log **/
   if (raw_game_id[0] == '\0' || raw_game_id[0] == ' ')
   {
      RARCH_LOG("[Scanner]: Scrubbing: %s\n", filename);
      return false;
   }

   cue_append_multi_disc_suffix(game_id, filename);
   strcpy(game_id, raw_game_id);
   return true;
}

/**
 * Check for an ASCII serial in the first few bits of the ISO (Wii).
 */
int detect_serial_ascii_game(intfstream_t *fd, char *game_id)
{
   unsigned pos;
   int number_of_ascii = 0;
   bool rv             = false;

   for (pos = 0; pos < 10000; pos++)
   {
      intfstream_seek(fd, pos, SEEK_SET);
      if (intfstream_read(fd, game_id, 15) > 0)
      {
         unsigned i;
         game_id[15]     = '\0';
         number_of_ascii = 0;

         /* When scanning WBFS files, "WBFS" is discovered as the first serial. Ignore it. */
         if (string_is_equal(game_id, "WBFS"))
            continue;

         /* Loop through until we run out of ASCII characters. */
         for (i = 0; i < 15; i++)
         {
            /* Is the given character ASCII? A-Z, 0-9, - */
            if (  (game_id[i] == 45) || 
                  (game_id[i] >= 48 && game_id[i] <= 57) || 
                  (game_id[i] >= 65 && game_id[i] <= 90))
               number_of_ascii++;
            else
               break;
         }

         /* If the length of the text is between 3 and 9 characters, 
          * it could be a serial. */
         if (number_of_ascii > 3 && number_of_ascii < 9)
         {
            /* Cut the string off, and return it as a valid serial. */
            game_id[number_of_ascii] = '\0';
            rv                       = true;
            break;
         }
      }
   }

   return rv;
}

int detect_system(intfstream_t *fd, const char **system_name, const char * filename)
{
   int i;
   char magic[50];

   RARCH_LOG("[Scanner]: %s\n", msg_hash_to_str(MSG_COMPARING_WITH_KNOWN_MAGIC_NUMBERS));
   for (i = 0; MAGIC_NUMBERS[i].system_name != NULL; i++)
   {
      if (intfstream_seek(fd, MAGIC_NUMBERS[i].offset, SEEK_SET) >= 0)
      {
         if (intfstream_read(fd, magic, MAGIC_NUMBERS[i].length_magic) > 0)
         {
            magic[MAGIC_NUMBERS[i].length_magic] = '\0';
            if (memcmp(MAGIC_NUMBERS[i].magic, magic, MAGIC_NUMBERS[i].length_magic) == 0)
            {
               *system_name = MAGIC_NUMBERS[i].system_name;
               RARCH_LOG("[Scanner]: Name: %s\n", filename);
               RARCH_LOG("[Scanner]: System: %s\n", MAGIC_NUMBERS[i].system_name);
               return true;
            }
         }
      }
   }

   RARCH_LOG("[Scanner]: Name: %s\n", filename);
   RARCH_LOG("[Scanner]: System: Unknown\n");
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
   fd                         = (intfstream_t*)intfstream_init(&info);

   if (!fd)
      goto error;

   if (!intfstream_open(fd, cue_path,
            RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE))
   {
      RARCH_LOG("Could not open CUE file '%s': %s\n", cue_path,
            strerror(errno));
      goto error;
   }

   RARCH_LOG("Parsing CUE file '%s'...\n", cue_path);

   tmp_token[0] = '\0';

   rv = -EINVAL;

   while (get_token(fd, tmp_token, sizeof(tmp_token)) > 0)
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

         get_token(fd, tmp_token, sizeof(tmp_token));
         fill_pathname_join(last_file, cue_dir,
               tmp_token, sizeof(last_file));

         file_size = intfstream_get_file_size(last_file);

         get_token(fd, tmp_token, sizeof(tmp_token));

      }
      else if (string_is_equal_noncase(tmp_token, "TRACK"))
      {
         get_token(fd, tmp_token, sizeof(tmp_token));
         get_token(fd, tmp_token, sizeof(tmp_token));
         is_data = !string_is_equal_noncase(tmp_token, "AUDIO");
         ++track;
      }
      else if (string_is_equal_noncase(tmp_token, "INDEX"))
      {
         int m, s, f;
         get_token(fd, tmp_token, sizeof(tmp_token));
         get_token(fd, tmp_token, sizeof(tmp_token));

         if (sscanf(tmp_token, "%02d:%02d:%02d", &m, &s, &f) < 3)
         {
            RARCH_LOG("Error parsing time stamp '%s'\n", tmp_token);
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
   return -errno;
}

bool cue_next_file(intfstream_t *fd,
      const char *cue_path, char *path, uint64_t max_len)
{
   char tmp_token[MAX_TOKEN_LEN];
   char cue_dir[PATH_MAX_LENGTH];
   bool rv                    = false;
   cue_dir[0]                 = '\0';

   fill_pathname_basedir(cue_dir, cue_path, sizeof(cue_dir));

   tmp_token[0] = '\0';

   while (get_token(fd, tmp_token, sizeof(tmp_token)) > 0)
   {
      if (string_is_equal_noncase(tmp_token, "FILE"))
      {
         get_token(fd, tmp_token, sizeof(tmp_token));
         fill_pathname_join(path, cue_dir, tmp_token, (size_t)max_len);
         rv = true;
         break;
      }
   }

   return rv;
}

int gdi_find_track(const char *gdi_path, bool first,
      char *track_path, uint64_t max_len)
{
   int rv;
   intfstream_info_t info;
   char tmp_token[MAX_TOKEN_LEN];
   intfstream_t *fd  = NULL;
   uint64_t largest  = 0;
   int size          = -1;
   int mode          = -1;
   int64_t file_size = -1;

   info.type         = INTFSTREAM_FILE;

   fd                = (intfstream_t*)intfstream_init(&info);

   if (!fd)
      goto error;

   if (!intfstream_open(fd, gdi_path,
            RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE))
   {
      RARCH_LOG("Could not open GDI file '%s': %s\n", gdi_path,
            strerror(errno));
      goto error;
   }

   RARCH_LOG("Parsing GDI file '%s'...\n", gdi_path);

   tmp_token[0] = '\0';

   rv = -EINVAL;

   /* Skip track count */
   get_token(fd, tmp_token, sizeof(tmp_token));

   /* Track number */
   while (get_token(fd, tmp_token, sizeof(tmp_token)) > 0)
   {
      /* Offset */
      if (get_token(fd, tmp_token, sizeof(tmp_token)) <= 0)
      {
         errno = EINVAL;
         goto error;
      }

      /* Mode */
      if (get_token(fd, tmp_token, sizeof(tmp_token)) <= 0)
      {
         errno = EINVAL;
         goto error;
      }

      mode = atoi(tmp_token);

      /* Sector size */
      if (get_token(fd, tmp_token, sizeof(tmp_token)) <= 0)
      {
         errno = EINVAL;
         goto error;
      }

      size = atoi(tmp_token);

      /* File name */
      if (get_token(fd, tmp_token, sizeof(tmp_token)) <= 0)
      {
         errno = EINVAL;
         goto error;
      }

      /* Check for data track */
      if (!(mode == 0 && size == 2352))
      {
         char last_file[PATH_MAX_LENGTH];
         char gdi_dir[PATH_MAX_LENGTH];

         gdi_dir[0]        = last_file[0] = '\0';

         fill_pathname_basedir(gdi_dir, gdi_path, sizeof(gdi_dir));

         fill_pathname_join(last_file,
               gdi_dir, tmp_token, sizeof(last_file));
         file_size = intfstream_get_file_size(last_file);

         if (file_size < 0)
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
      if (get_token(fd, tmp_token, sizeof(tmp_token)) <= 0)
      {
         errno = EINVAL;
         goto error;
      }
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
   return -errno;
}

bool gdi_next_file(intfstream_t *fd, const char *gdi_path,
      char *path, uint64_t max_len)
{
   char tmp_token[MAX_TOKEN_LEN];
   bool rv         = false;

   tmp_token[0]    = '\0';

   /* Skip initial track count */
   if (intfstream_tell(fd) == 0)
      get_token(fd, tmp_token, sizeof(tmp_token));

   get_token(fd, tmp_token, sizeof(tmp_token)); /* Track number */
   get_token(fd, tmp_token, sizeof(tmp_token)); /* Offset       */
   get_token(fd, tmp_token, sizeof(tmp_token)); /* Mode         */
   get_token(fd, tmp_token, sizeof(tmp_token)); /* Sector size  */

   /* File name */
   if (get_token(fd, tmp_token, sizeof(tmp_token)) > 0)
   {
      char gdi_dir[PATH_MAX_LENGTH];

      gdi_dir[0]      = '\0';

      fill_pathname_basedir(gdi_dir, gdi_path, sizeof(gdi_dir));

      fill_pathname_join(path, gdi_dir, tmp_token, (size_t)max_len);

      rv              = true;

      /* Disc offset */
      get_token(fd, tmp_token, sizeof(tmp_token));
   }

   return rv;
}
