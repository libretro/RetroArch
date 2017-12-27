/*
 directory.c
 Reading, writing and manipulation of the directory structure on
 a FAT partition

 Copyright (c) 2006 Michael "Chishm" Chisholm

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation and/or
     other materials provided with the distribution.
  3. The name of the author may not be used to endorse or promote products derived
     from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <string.h>
#include <ctype.h>
#include <wchar.h>
#include <wctype.h>
#include <stdlib.h>
#include <stdio.h>

#include "directory.h"
#include "common.h"
#include "partition.h"
#include "file_allocation_table.h"
#include "bit_ops.h"
#include "filetime.h"

/* Directory entry codes */
#define DIR_ENTRY_LAST 0x00
#define DIR_ENTRY_FREE 0xE5

typedef unsigned short ucs2_t;

/* Long file name directory entry */
enum LFN_offset
{
   LFN_offset_ordinal = 0x00,	/* Position within LFN */
   LFN_offset_char0 = 0x01,
   LFN_offset_char1 = 0x03,
   LFN_offset_char2 = 0x05,
   LFN_offset_char3 = 0x07,
   LFN_offset_char4 = 0x09,
   LFN_offset_flag = 0x0B,	/* Should be equal to ATTRIB_LFN */
   LFN_offset_reserved1 = 0x0C,	/* Always 0x00 */
   LFN_offset_checkSum = 0x0D,	/* Checksum of short file name (alias) */
   LFN_offset_char5 = 0x0E,
   LFN_offset_char6 = 0x10,
   LFN_offset_char7 = 0x12,
   LFN_offset_char8 = 0x14,
   LFN_offset_char9 = 0x16,
   LFN_offset_char10 = 0x18,
   LFN_offset_reserved2 = 0x1A,	/* Always 0x0000 */
   LFN_offset_char11 = 0x1C,
   LFN_offset_char12 = 0x1E
};
static const int LFN_offset_table[13]={0x01,0x03,0x05,0x07,0x09,0x0E,0x10,0x12,0x14,0x16,0x18,0x1C,0x1E};

#define LFN_END 0x40
#define LFN_DEL 0x80

static const char ILLEGAL_ALIAS_CHARACTERS[] = "\\/:;*?\"<>|&+,=[] ";
static const char ILLEGAL_LFN_CHARACTERS[] = "\\/:*?\"<>|";

/*
   Returns number of UCS-2 characters needed to encode an LFN
   Returns -1 if it is an invalid LFN
   */
#define ABOVE_UCS_RANGE 0xF0
static int _FAT_directory_lfnLength (const char* name)
{
   unsigned int i;
   int ucsLength;
   const char* tempName = name;
   size_t nameLength    = strnlen(name, NAME_MAX);

   /* Make sure the name is short enough to be valid */
   if ( nameLength >= NAME_MAX)
      return -1;

   /* Make sure it doesn't contain any invalid characters */
   if (strpbrk (name, ILLEGAL_LFN_CHARACTERS) != NULL)
      return -1;

   /* Make sure the name doesn't contain any control codes or codes not representable in UCS-2 */
   for (i = 0; i < nameLength; i++)
   {
      unsigned char ch = (unsigned char) name[i];
      if (ch < 0x20 || ch >= ABOVE_UCS_RANGE)
         return -1;
   }

   /* Convert to UCS-2 and get the resulting length */
   ucsLength = mbsrtowcs(NULL, &tempName, MAX_LFN_LENGTH, NULL);
   if (ucsLength < 0 || ucsLength >= MAX_LFN_LENGTH)
      return -1;

   /* Otherwise it is valid */
   return ucsLength;
}

/*
   Convert a multibyte encoded string into a NUL-terminated UCS-2 string, storing at most len characters
   return number of characters stored
   */
static size_t _FAT_directory_mbstoucs2 (ucs2_t* dst, const char* src, size_t len)
{
   mbstate_t ps = {0};
   wchar_t tempChar;
   int bytes;
   size_t count = 0;

   while (count < len-1 && *src != '\0')
   {
      bytes = mbrtowc (&tempChar, src, MB_CUR_MAX, &ps);
      if (bytes > 0)
      {
         *dst = (ucs2_t)tempChar;
         src += bytes;
         dst++;
         count++;
      }
      else if (bytes == 0)
         break;
      else
         return -1;
   }
   *dst = '\0';

   return count;
}

/*
   Convert a UCS-2 string into a NUL-terminated multibyte string, storing at most len chars
   return number of chars stored, or (size_t)-1 on error
   */
static size_t _FAT_directory_ucs2tombs (char* dst, const ucs2_t* src, size_t len)
{
   mbstate_t ps = {0};
   size_t count = 0;
   int bytes;
   char buff[MB_CUR_MAX];
   int i;

   while (count < len - 1 && *src != '\0')
   {
      bytes = wcrtomb (buff, *src, &ps);
      if (bytes < 0)
         return -1;
      if (count + bytes < len && bytes > 0)
      {
         for (i = 0; i < bytes; i++)
            *dst++ = buff[i];
         src++;
         count += bytes;
      }
      else
         break;
   }
   *dst = L'\0';

   return count;
}

/*
   Case-independent comparison of two multibyte encoded strings
   */
static int _FAT_directory_mbsncasecmp (const char* s1, const char* s2, size_t len1)
{
   wchar_t wc1, wc2;
   mbstate_t ps1 = {0};
   mbstate_t ps2 = {0};
   size_t b1 = 0;
   size_t b2 = 0;

   if (len1 == 0)
      return 0;

   do
   {
      s1 += b1;
      s2 += b2;
      b1 = mbrtowc(&wc1, s1, MB_CUR_MAX, &ps1);
      b2 = mbrtowc(&wc2, s2, MB_CUR_MAX, &ps2);
      if ((int)b1 < 0 || (int)b2 < 0)
         break;
      len1 -= b1;
   } while (len1 > 0 && towlower(wc1) == towlower(wc2) && wc1 != 0);

   return towlower(wc1) - towlower(wc2);
}


static bool _FAT_directory_entryGetAlias (const u8* entryData, char* destName)
{
   char c;
   bool caseInfo;
   int i = 0;
   int j = 0;

   destName[0] = '\0';
   if (entryData[0] != DIR_ENTRY_FREE)
   {
      if (entryData[0] == '.')
      {
         destName[0] = '.';
         if (entryData[1] == '.')
         {
            destName[1] = '.';
            destName[2] = '\0';
         }
         else
            destName[1] = '\0';
      }
      else
      {
         /* Copy the filename from the dirEntry to the string */
         caseInfo = entryData[DIR_ENTRY_caseInfo] & CASE_LOWER_BASE;
         for (i = 0; (i < 8) && (entryData[DIR_ENTRY_name + i] != ' '); i++)
         {
            c = entryData[DIR_ENTRY_name + i];
            destName[i] = (caseInfo ? tolower((unsigned char)c) : c);
         }
         /* Copy the extension from the dirEntry to the string */
         if (entryData[DIR_ENTRY_extension] != ' ')
         {
            destName[i++] = '.';
            caseInfo = entryData[DIR_ENTRY_caseInfo] & CASE_LOWER_EXT;
            for ( j = 0; (j < 3) && (entryData[DIR_ENTRY_extension + j] != ' '); j++)
            {
               c = entryData[DIR_ENTRY_extension + j];
               destName[i++] = (caseInfo ? tolower((unsigned char)c) : c);
            }
         }
         destName[i] = '\0';
      }
   }

   return (destName[0] != '\0');
}

uint32_t _FAT_directory_entryGetCluster (PARTITION* partition, const uint8_t* entryData)
{
   /* Only use high 16 bits of start cluster when we are certain they are correctly defined */
   if (partition->filesysType == FS_FAT32)
      return u8array_to_u16(entryData,DIR_ENTRY_cluster) | (u8array_to_u16(entryData, DIR_ENTRY_clusterHigh) << 16);
   return u8array_to_u16(entryData,DIR_ENTRY_cluster);
}

static bool _FAT_directory_incrementDirEntryPosition (PARTITION* partition, DIR_ENTRY_POSITION* entryPosition, bool extendDirectory)
{
   DIR_ENTRY_POSITION position = *entryPosition;
   uint32_t tempCluster;

   /* Increment offset, wrapping at the end of a sector */
   ++ position.offset;
   if (position.offset == partition->bytesPerSector / DIR_ENTRY_DATA_SIZE)
   {
      position.offset = 0;
      /* Increment sector when wrapping */
      ++ position.sector;
      /* But wrap at the end of a cluster */
      if ((position.sector == partition->sectorsPerCluster) && (position.cluster != FAT16_ROOT_DIR_CLUSTER))
      {
         position.sector = 0;
         /* Move onto the next cluster, making sure there is another cluster to go to */
         tempCluster = _FAT_fat_nextCluster(partition, position.cluster);
         if (tempCluster == CLUSTER_EOF)
         {
            if (extendDirectory)
            {
               tempCluster = _FAT_fat_linkFreeClusterCleared (partition, position.cluster);
               if (!_FAT_fat_isValidCluster(partition, tempCluster))
                  return false;	/* This will only happen if the disc is full */
            }
            else
               return false;		/* Got to the end of the directory, not extending it */
         }
         position.cluster = tempCluster;
      }
      else if ((position.cluster == FAT16_ROOT_DIR_CLUSTER) && (position.sector == (partition->dataStart - partition->rootDirStart)))
         return false;	/* Got to end of root directory, can't extend it */
   }
   *entryPosition = position;
   return true;
}

bool _FAT_directory_getNextEntry (PARTITION* partition, DIR_ENTRY* entry)
{
   DIR_ENTRY_POSITION entryEnd;
   uint8_t entryData[0x20];
   ucs2_t lfn[MAX_LFN_LENGTH];
   bool notFound, found;
   int lfnPos;
   uint8_t chkSum;
   bool lfnExists;
   int i;
   uint8_t lfnChkSum = 0;
   DIR_ENTRY_POSITION entryStart = entry->dataEnd;

   /* Make sure we are using the correct root directory, in case of FAT32 */
   if (entryStart.cluster == FAT16_ROOT_DIR_CLUSTER)
      entryStart.cluster = partition->rootDirCluster;

   entryEnd = entryStart;

   lfnExists = false;

   found = false;
   notFound = false;

   while (!found && !notFound)
   {
      if (_FAT_directory_incrementDirEntryPosition (partition, &entryEnd, false) == false)
      {
         notFound = true;
         break;
      }

      _FAT_cache_readPartialSector (partition->cache, entryData,
            _FAT_fat_clusterToSector(partition, entryEnd.cluster) + entryEnd.sector,
            entryEnd.offset * DIR_ENTRY_DATA_SIZE, DIR_ENTRY_DATA_SIZE);

      if (entryData[DIR_ENTRY_attributes] == ATTRIB_LFN)
      {
         /* It's an LFN */
         if (entryData[LFN_offset_ordinal] & LFN_DEL)
            lfnExists = false;
         else if (entryData[LFN_offset_ordinal] & LFN_END)
         {
            /* Last part of LFN, make sure it isn't deleted using previous if(Thanks MoonLight) */
            entryStart = entryEnd;	/* This is the start of a directory entry */
            lfnExists = true;
            lfnPos = (entryData[LFN_offset_ordinal] & ~LFN_END) * 13;
            if (lfnPos > MAX_LFN_LENGTH - 1)
               lfnPos = MAX_LFN_LENGTH - 1;
            lfn[lfnPos] = '\0';	/* Set end of lfn to null character */
            lfnChkSum = entryData[LFN_offset_checkSum];
         }
         if (lfnChkSum != entryData[LFN_offset_checkSum])
            lfnExists = false;
         if (lfnExists)
         {
            lfnPos = ((entryData[LFN_offset_ordinal] & ~LFN_END) - 1) * 13;
            for (i = 0; i < 13; i++)
            {
               if (lfnPos + i < MAX_LFN_LENGTH - 1)
                  lfn[lfnPos + i] = entryData[LFN_offset_table[i]] | (entryData[LFN_offset_table[i]+1] << 8);
            }
         }
      }
      /* This is a volume name, don't bother with it */
      else if (entryData[DIR_ENTRY_attributes] & ATTRIB_VOL) { }
      else if (entryData[0] == DIR_ENTRY_LAST)
         notFound = true;
      else if ((entryData[0] != DIR_ENTRY_FREE) && (entryData[0] > 0x20) && !(entryData[DIR_ENTRY_attributes] & ATTRIB_VOL))
      {
         if (lfnExists)
         {
            /* Calculate file checksum */
            chkSum = 0;
            /* NOTE: The operation is an unsigned char rotate right */
            for (i=0; i < 11; i++)
               chkSum = ((chkSum & 1) ? 0x80 : 0) + (chkSum >> 1) + entryData[i];

            if (chkSum != lfnChkSum)
            {
               lfnExists = false;
               entry->filename[0] = '\0';
            }
         }

         if (lfnExists)
         {
            /* Failed to convert the file name to UTF-8. Maybe the wrong locale is set? */
            if (_FAT_directory_ucs2tombs (entry->filename, lfn, NAME_MAX) == (size_t)-1)
               return false;
         }
         else
         {
            entryStart = entryEnd;
            _FAT_directory_entryGetAlias (entryData, entry->filename);
         }
         found = true;
      }
   }

   /* If no file is found, return false */
   if (notFound)
      return false;

   /* Fill in the directory entry struct */
   entry->dataStart = entryStart;
   entry->dataEnd   = entryEnd;
   memcpy (entry->entryData, entryData, DIR_ENTRY_DATA_SIZE);
   return true;
}

bool _FAT_directory_getFirstEntry (PARTITION* partition, DIR_ENTRY* entry, uint32_t dirCluster)
{
   entry->dataStart.cluster = dirCluster;
   entry->dataStart.sector  = 0;
   entry->dataStart.offset  = -1; /* Start before the beginning of the directory */

   entry->dataEnd = entry->dataStart;

   return _FAT_directory_getNextEntry (partition, entry);
}

bool _FAT_directory_getRootEntry (PARTITION* partition, DIR_ENTRY* entry)
{
   entry->dataStart.cluster = 0;
   entry->dataStart.sector = 0;
   entry->dataStart.offset = 0;

   entry->dataEnd = entry->dataStart;

   memset (entry->filename, '\0', NAME_MAX);
   entry->filename[0] = '.';

   memset (entry->entryData, 0, DIR_ENTRY_DATA_SIZE);
   memset (entry->entryData, ' ', 11);
   entry->entryData[0] = '.';

   entry->entryData[DIR_ENTRY_attributes] = ATTRIB_DIR;

   u16_to_u8array (entry->entryData, DIR_ENTRY_cluster, partition->rootDirCluster);
   u16_to_u8array (entry->entryData, DIR_ENTRY_clusterHigh, partition->rootDirCluster >> 16);

   return true;
}

bool _FAT_directory_getVolumeLabel (PARTITION* partition, char *label)
{
   DIR_ENTRY entry;
   DIR_ENTRY_POSITION entryEnd;
   uint8_t entryData[DIR_ENTRY_DATA_SIZE];
   int i;
   bool end;

   _FAT_directory_getRootEntry(partition, &entry);

   entryEnd = entry.dataEnd;

   /* Make sure we are using the correct root directory, in case of FAT32 */
   if (entryEnd.cluster == FAT16_ROOT_DIR_CLUSTER)
      entryEnd.cluster = partition->rootDirCluster;

   label[0]='\0';
   label[11]='\0';
   end = false;

   /* this entry should be among the first 3 entries in the root directory table, 
    * if not, then system can have trouble displaying the right volume label */

   while(!end)
   {
      /* error reading */
      if(!_FAT_cache_readPartialSector (partition->cache, entryData,
               _FAT_fat_clusterToSector(partition, entryEnd.cluster) + entryEnd.sector,
               entryEnd.offset * DIR_ENTRY_DATA_SIZE, DIR_ENTRY_DATA_SIZE))
         return false;

      if (entryData[DIR_ENTRY_attributes] == ATTRIB_VOL && entryData[0] != DIR_ENTRY_FREE)
      {
         for (i = 0; i < 11; i++)
            label[i] = entryData[DIR_ENTRY_name + i];
         return true;
      }
      else if (entryData[0] == DIR_ENTRY_LAST)
         end = true;

      if (_FAT_directory_incrementDirEntryPosition (partition, &entryEnd, false) == false)
         end = true;
   }
   return false;
}

bool _FAT_directory_entryFromPosition (PARTITION* partition, DIR_ENTRY* entry)
{
   DIR_ENTRY_POSITION entryStart = entry->dataStart;
   DIR_ENTRY_POSITION entryEnd = entry->dataEnd;
   bool entryStillValid;
   bool finished;
   ucs2_t lfn[MAX_LFN_LENGTH];
   int i;
   int lfnPos;
   uint8_t entryData[DIR_ENTRY_DATA_SIZE];

   memset (entry->filename, '\0', NAME_MAX);

   /* Create an empty directory entry to overwrite the old ones with */
   for ( entryStillValid = true, finished = false;
         entryStillValid && !finished;
         entryStillValid = _FAT_directory_incrementDirEntryPosition (partition, &entryStart, false))
   {
      _FAT_cache_readPartialSector (partition->cache, entryData,
            _FAT_fat_clusterToSector(partition, entryStart.cluster) + entryStart.sector,
            entryStart.offset * DIR_ENTRY_DATA_SIZE, DIR_ENTRY_DATA_SIZE);

      if ((entryStart.cluster == entryEnd.cluster)
            && (entryStart.sector == entryEnd.sector)
            && (entryStart.offset == entryEnd.offset))
      {
         /* Copy the entry data and stop, since this is the 
          * last section of the directory entry */
         memcpy (entry->entryData, entryData, DIR_ENTRY_DATA_SIZE);
         finished = true;
      }
      else
      {
         /* Copy the long file name data */
         lfnPos = ((entryData[LFN_offset_ordinal] & ~LFN_END) - 1) * 13;
         for (i = 0; i < 13; i++)
         {
            if (lfnPos + i < MAX_LFN_LENGTH - 1)
               lfn[lfnPos + i] = entryData[LFN_offset_table[i]] | (entryData[LFN_offset_table[i]+1] << 8);
         }
      }
   }

   if (!entryStillValid)
      return false;

   entryStart = entry->dataStart;
   if ((entryStart.cluster == entryEnd.cluster)
         && (entryStart.sector == entryEnd.sector)
         && (entryStart.offset == entryEnd.offset))
   {
      /* Since the entry doesn't have a long file name, extract the short filename */
      if (!_FAT_directory_entryGetAlias (entry->entryData, entry->filename))
         return false;
   }
   else
   {
      /* Encode the long file name into a multibyte string */
      if (_FAT_directory_ucs2tombs (entry->filename, lfn, NAME_MAX) == (size_t)-1)
         return false;
   }

   return true;
}



bool _FAT_directory_entryFromPath (PARTITION* partition, DIR_ENTRY* entry, const char* path, const char* pathEnd)
{
   size_t dirnameLength;
   const char* nextPathPosition;
   uint32_t dirCluster;
   bool foundFile;
   char alias[MAX_ALIAS_LENGTH];
   const char *pathPosition = path;
   bool found = false;
   bool notFound = false;

   /* Set pathEnd to the end of the path string */
   if (pathEnd == NULL)
      pathEnd = strchr (path, '\0');

   if (pathPosition[0] == DIR_SEPARATOR)
   {
      /* Start at root directory */
      dirCluster = partition->rootDirCluster;
      /* Consume separator(s) */
      while (pathPosition[0] == DIR_SEPARATOR)
         pathPosition++;
      /* If the path is only specifying a directory in the form of "/" return it */
      if (pathPosition >= pathEnd)
      {
         _FAT_directory_getRootEntry (partition, entry);
         found = true;
      }
   }
   /* Start in current working directory */
   else
      dirCluster = partition->cwdCluster;

   while (!found && !notFound)
   {
      /* Get the name of the next required subdirectory within the path */
      nextPathPosition = strchr (pathPosition, DIR_SEPARATOR);
      if (nextPathPosition != NULL)
         dirnameLength = nextPathPosition - pathPosition;
      else
         dirnameLength = strlen(pathPosition);

      /* The path is too long to bother with */
      if (dirnameLength > NAME_MAX)
         return false;

      /* Check for "." or ".." when the dirCluster is root cluster
       * These entries do not exist, so we must fake it */
      if ((dirCluster == partition->rootDirCluster)
            &&  ((strncmp(".", pathPosition, dirnameLength) == 0)
               || (strncmp("..", pathPosition, dirnameLength) == 0)))
      {
         foundFile = true;
         _FAT_directory_getRootEntry(partition, entry);
      }
      else
      {
         /* Look for the directory within the path */
         foundFile = _FAT_directory_getFirstEntry (partition, entry, dirCluster);

         while (foundFile && !found && !notFound)
         {
            /* It hasn't already found the file
             * Check if the filename matches */
            if ((dirnameLength == strnlen(entry->filename, NAME_MAX))
                  && (_FAT_directory_mbsncasecmp(pathPosition, entry->filename, dirnameLength) == 0))
               found = true;

            /* Check if the alias matches */
            _FAT_directory_entryGetAlias (entry->entryData, alias);
            if ((dirnameLength == strnlen(alias, MAX_ALIAS_LENGTH))
                  && (strncasecmp(pathPosition, alias, dirnameLength) == 0))
               found = true;

            /* Make sure that we aren't trying to follow a file instead of a directory in the path */
            if (found && !(entry->entryData[DIR_ENTRY_attributes] & ATTRIB_DIR) && (nextPathPosition != NULL))
               found = false;

            if (!found)
               foundFile = _FAT_directory_getNextEntry (partition, entry);
         }
      }

      if (!foundFile)
      {
         /* Check that the search didn't get to the end of the directory */
         notFound = true;
         found = false;
      }
      /* Check that we reached the end of the path */
      else if ((nextPathPosition == NULL) || (nextPathPosition >= pathEnd))
         found = true;
      else if (entry->entryData[DIR_ENTRY_attributes] & ATTRIB_DIR)
      {
         dirCluster = _FAT_directory_entryGetCluster (partition, entry->entryData);
         if (dirCluster == CLUSTER_ROOT)
            dirCluster = partition->rootDirCluster;
         pathPosition = nextPathPosition;
         /* Consume separator(s) */
         while (pathPosition[0] == DIR_SEPARATOR)
            pathPosition++;
         /* The requested directory was found */
         if (pathPosition >= pathEnd) 
            found = true;
         else
            found = false;
      }
   }

   if (found && !notFound)
   {
      /* On FAT32 it should specify an actual cluster for the root entry,
       * not cluster 0 as on FAT16 */
      if (partition->filesysType == FS_FAT32 && (entry->entryData[DIR_ENTRY_attributes] & ATTRIB_DIR) &&
            _FAT_directory_entryGetCluster (partition, entry->entryData) == CLUSTER_ROOT)
         _FAT_directory_getRootEntry (partition, entry);
      return true;
   }

   return false;
}

bool _FAT_directory_removeEntry (PARTITION* partition, DIR_ENTRY* entry)
{
   DIR_ENTRY_POSITION entryStart = entry->dataStart;
   DIR_ENTRY_POSITION entryEnd = entry->dataEnd;
   bool entryStillValid;
   bool finished;
   uint8_t entryData[DIR_ENTRY_DATA_SIZE];

   /* Create an empty directory entry to overwrite the old ones with */
   for ( entryStillValid = true, finished = false;
         entryStillValid && !finished;
         entryStillValid = _FAT_directory_incrementDirEntryPosition (partition, &entryStart, false))
   {
      _FAT_cache_readPartialSector (partition->cache, entryData, _FAT_fat_clusterToSector(partition, entryStart.cluster) + entryStart.sector, entryStart.offset * DIR_ENTRY_DATA_SIZE, DIR_ENTRY_DATA_SIZE);
      entryData[0] = DIR_ENTRY_FREE;
      _FAT_cache_writePartialSector (partition->cache, entryData, _FAT_fat_clusterToSector(partition, entryStart.cluster) + entryStart.sector, entryStart.offset * DIR_ENTRY_DATA_SIZE, DIR_ENTRY_DATA_SIZE);
      if ((entryStart.cluster == entryEnd.cluster) && (entryStart.sector == entryEnd.sector) && (entryStart.offset == entryEnd.offset))
         finished = true;
   }

   if (!entryStillValid)
      return false;

   return true;
}

static bool _FAT_directory_findEntryGap (PARTITION* partition, DIR_ENTRY* entry, uint32_t dirCluster, size_t size)
{
   DIR_ENTRY_POSITION gapStart;
   DIR_ENTRY_POSITION gapEnd;
   uint8_t entryData[DIR_ENTRY_DATA_SIZE];
   size_t dirEntryRemain;
   bool endOfDirectory, entryStillValid;

   /* Scan Dir for free entry */
   gapEnd.offset = 0;
   gapEnd.sector = 0;
   gapEnd.cluster = dirCluster;

   gapStart = gapEnd;

   entryStillValid = true;
   dirEntryRemain = size;
   endOfDirectory = false;

   while (entryStillValid && !endOfDirectory && (dirEntryRemain > 0))
   {
      _FAT_cache_readPartialSector (partition->cache, entryData,
            _FAT_fat_clusterToSector(partition, gapEnd.cluster) + gapEnd.sector,
            gapEnd.offset * DIR_ENTRY_DATA_SIZE, DIR_ENTRY_DATA_SIZE);
      if (entryData[0] == DIR_ENTRY_LAST)
      {
         if (dirEntryRemain == size)
            gapStart = gapEnd;
         -- dirEntryRemain;
         endOfDirectory = true;
      }
      else if (entryData[0] == DIR_ENTRY_FREE)
      {
         if (dirEntryRemain == size)
            gapStart = gapEnd;
         -- dirEntryRemain;
      }
      else
         dirEntryRemain = size;

      if (!endOfDirectory && (dirEntryRemain > 0))
         entryStillValid = _FAT_directory_incrementDirEntryPosition (partition, &gapEnd, true);
   }

   /* Make sure the scanning didn't fail */
   if (!entryStillValid)
      return false;

   /* Save the start entry, since we know it is valid */
   entry->dataStart = gapStart;

   if (endOfDirectory)
   {
      memset (entryData, DIR_ENTRY_LAST, DIR_ENTRY_DATA_SIZE);
      dirEntryRemain += 1;	/* Increase by one to take account of End Of Directory Marker */

      while ((dirEntryRemain > 0) && entryStillValid)
      {
         /* Get the gapEnd before incrementing it, so the second to last one is saved */
         entry->dataEnd = gapEnd;
         /* Increment gapEnd, moving onto the next entry */
         entryStillValid = _FAT_directory_incrementDirEntryPosition (partition, &gapEnd, true);
         -- dirEntryRemain;
         /* Fill the entry with blanks */
         _FAT_cache_writePartialSector (partition->cache, entryData,
               _FAT_fat_clusterToSector(partition, gapEnd.cluster) + gapEnd.sector,
               gapEnd.offset * DIR_ENTRY_DATA_SIZE, DIR_ENTRY_DATA_SIZE);
      }
      if (!entryStillValid)
         return false;
   }
   else
      entry->dataEnd = gapEnd;

   return true;
}

static bool _FAT_directory_entryExists (PARTITION* partition, const char* name, uint32_t dirCluster)
{
   DIR_ENTRY tempEntry;
   bool foundFile;
   char alias[MAX_ALIAS_LENGTH];
   size_t dirnameLength = strnlen(name, NAME_MAX);

   if (dirnameLength >= NAME_MAX)
      return false;

   /* Make sure the entry doesn't already exist */
   foundFile = _FAT_directory_getFirstEntry (partition, &tempEntry, dirCluster);

   while (foundFile)
   {
      /* It hasn't already found the file
       * Check if the filename matches */
      if ((dirnameLength == strnlen(tempEntry.filename, NAME_MAX))
            && (_FAT_directory_mbsncasecmp(name, tempEntry.filename, dirnameLength) == 0))
         return true;

      /* Check if the alias matches */
      _FAT_directory_entryGetAlias (tempEntry.entryData, alias);
      if ((strncasecmp(name, alias, MAX_ALIAS_LENGTH) == 0))
         return true;
      foundFile = _FAT_directory_getNextEntry (partition, &tempEntry);
   }
   return false;
}

/*
   Creates an alias for a long file name. If the alias is not an exact match for the
   filename, it returns the number of characters in the alias. If the two names match,
   it returns 0. If there was an error, it returns -1.
   */
static int _FAT_directory_createAlias (char* alias, const char* lfn)
{
   bool lossyConversion = false;	/* Set when the alias had to be modified to be valid */
   int lfnPos = 0;
   int aliasPos = 0;
   wchar_t lfnChar;
   int oemChar;
   mbstate_t ps = {0};
   int bytesUsed = 0;
   const char* lfnExt;
   int aliasExtLen;

   /* Strip leading periods */
   while (lfn[lfnPos] == '.')
   {
      lfnPos ++;
      lossyConversion = true;
   }

   /* Primary portion of alias */
   while (aliasPos < 8 && lfn[lfnPos] != '.' && lfn[lfnPos] != '\0')
   {
      bytesUsed = mbrtowc(&lfnChar, lfn + lfnPos, NAME_MAX - lfnPos, &ps);
      if (bytesUsed < 0)
         return -1;

      oemChar = wctob(towupper((wint_t)lfnChar));

      /* Case of letter was changed */
      if (wctob((wint_t)lfnChar) != oemChar)
         lossyConversion = true;

      if (oemChar == ' ')
      {
         /* Skip spaces in filename */
         lossyConversion = true;
         lfnPos += bytesUsed;
         continue;
      }

      if (oemChar == EOF)
      {
         oemChar = '_';		/* Replace unconvertable characters with underscores */
         lossyConversion = true;
      }

      if (strchr (ILLEGAL_ALIAS_CHARACTERS, oemChar) != NULL)
      {
         /* Invalid Alias character */
         oemChar = '_';		/* Replace illegal characters with underscores */
         lossyConversion = true;
      }

      alias[aliasPos] = (char)oemChar;
      aliasPos++;
      lfnPos += bytesUsed;
   }

   /* Name was more than 8 characters long */
   if (lfn[lfnPos] != '.' && lfn[lfnPos] != '\0')
      lossyConversion = true;

   /* Alias extension */
   lfnExt = strrchr (lfn, '.');
   /* More than one period in name */
   if (lfnExt != NULL && lfnExt != strchr (lfn, '.'))
      lossyConversion = true;

   if (lfnExt != NULL && lfnExt[1] != '\0')
   {
      lfnExt++;
      alias[aliasPos] = '.';
      aliasPos++;
      memset (&ps, 0, sizeof(ps));
      for (aliasExtLen = 0; aliasExtLen < MAX_ALIAS_EXT_LENGTH && *lfnExt != '\0'; aliasExtLen++)
      {
         bytesUsed = mbrtowc(&lfnChar, lfnExt, NAME_MAX - lfnPos, &ps);
         if (bytesUsed < 0)
            return -1;
         oemChar = wctob(towupper((wint_t)lfnChar));

         /* Case of letter was changed */
         if (wctob((wint_t)lfnChar) != oemChar)
            lossyConversion = true;
         if (oemChar == ' ')
         {
            /* Skip spaces in alias */
            lossyConversion = true;
            lfnExt += bytesUsed;
            continue;
         }
         if (oemChar == EOF)
         {
            oemChar = '_';		/* Replace unconvertable characters with underscores */
            lossyConversion = true;
         }
         if (strchr (ILLEGAL_ALIAS_CHARACTERS, oemChar) != NULL)
         {
            /* Invalid Alias character */
            oemChar = '_';		/* Replace illegal characters with underscores */
            lossyConversion = true;
         }

         alias[aliasPos] = (char)oemChar;
         aliasPos++;
         lfnExt += bytesUsed;
      }
      /* Extension was more than 3 characters long */
      if (*lfnExt != '\0')
         lossyConversion = true;
   }

   alias[aliasPos] = '\0';
   if (lossyConversion)
      return aliasPos;
   return 0;
}

bool _FAT_directory_addEntry (PARTITION* partition, DIR_ENTRY* entry, uint32_t dirCluster)
{
   size_t entrySize;
   uint8_t lfnEntry[DIR_ENTRY_DATA_SIZE];
   int i,j; /* Must be signed for use when decrementing in for loop */
   char *tmpCharPtr;
   DIR_ENTRY_POSITION curEntryPos;
   bool entryStillValid;
   uint8_t aliasCheckSum = 0;
   char alias [MAX_ALIAS_LENGTH];
   int aliasLen;
   int lfnLen;

   /* Remove trailing spaces */
   for (i = strlen (entry->filename) - 1; (i >= 0) && (entry->filename[i] == ' '); --i)
      entry->filename[i] = '\0';

#if 0
   /* Remove leading spaces */
   for (i = 0; entry->filename[i] == ' '; ++i) ;
   if (i > 0)
      memmove (entry->filename, entry->filename + i, strlen (entry->filename + i));
#endif

   /* Make sure the filename is not 0 length */
   if (strnlen (entry->filename, NAME_MAX) < 1)
      return false;

   /* Make sure the filename is at least a valid LFN */
   lfnLen = _FAT_directory_lfnLength (entry->filename);
   if (lfnLen < 0)
      return false;

   /* Remove junk in filename */
   i = strlen (entry->filename);
   memset (entry->filename + i, '\0', NAME_MAX - i);

   /* Make sure the entry doesn't already exist */
   if (_FAT_directory_entryExists (partition, entry->filename, dirCluster))
      return false;

   /* Clear out alias, so we can generate a new one */
   memset (entry->entryData, ' ', 11);

   if ( strncmp(entry->filename, ".", NAME_MAX) == 0)
   {
      /* "." entry */
      entry->entryData[0] = '.';
      entrySize = 1;
   }
   else if ( strncmp(entry->filename, "..", NAME_MAX) == 0)
   {
      /* ".." entry */
      entry->entryData[0] = '.';
      entry->entryData[1] = '.';
      entrySize = 1;
   }
   else
   {
      /* Normal file name */
      aliasLen = _FAT_directory_createAlias (alias, entry->filename);
      if (aliasLen < 0)
         return false;
      /* It's a normal short filename */
      else if (aliasLen == 0)
         entrySize = 1;
      else
      {
         /* It's a long filename with an alias */
         entrySize = ((lfnLen + LFN_ENTRY_LENGTH - 1) / LFN_ENTRY_LENGTH) + 1;

         /* Generate full alias for all cases except when 
          * the alias is simply an upper case version of the LFN
          * and there isn't already a file with that name 
          */
         if (strncasecmp (alias, entry->filename, MAX_ALIAS_LENGTH) != 0 ||
               _FAT_directory_entryExists (partition, alias, dirCluster))
         {
            /* expand primary part to 8 characters long by padding the end with underscores */
            i = 0;
            j = MAX_ALIAS_PRI_LENGTH;
            /* Move extension to last 3 characters */
            while (alias[i] != '.' && alias[i] != '\0') i++;

            if (i < j)
            {
               memmove (alias + j, alias + i, aliasLen - i + 1);
               /* Pad primary component */
               memset (alias + i, '_', j - i);
            }

            /* Generate numeric tail */
            for (i = 1; i <= MAX_NUMERIC_TAIL; i++)
            {
               j = i;
               tmpCharPtr = alias + MAX_ALIAS_PRI_LENGTH - 1;
               while (j > 0)
               {
                  *tmpCharPtr = '0' + (j % 10); /* ASCII numeric value */
                  tmpCharPtr--;
                  j /= 10;
               }
               *tmpCharPtr = '~';
               if (!_FAT_directory_entryExists (partition, alias, dirCluster))
                  break;
            }

            /* Couldn't get a valid alias */
            if (i > MAX_NUMERIC_TAIL)
               return false;
         }
      }

      /* Copy alias or short file name into directory entry data */
      for (i = 0, j = 0; (j < 8) && (alias[i] != '.') && (alias[i] != '\0'); i++, j++)
         entry->entryData[j] = alias[i];
      while (j < 8)
      {
         entry->entryData[j] = ' ';
         ++ j;
      }
      if (alias[i] == '.')
      {
         /* Copy extension */
         ++ i;
         while ((alias[i] != '\0') && (j < 11))
         {
            entry->entryData[j] = alias[i];
            ++ i;
            ++ j;
         }
      }
      while (j < 11)
      {
         entry->entryData[j] = ' ';
         ++ j;
      }

      /* Generate alias checksum
       * NOTE: The operation is an unsigned char rotate right */
      for (i=0; i < ALIAS_ENTRY_LENGTH; i++)
         aliasCheckSum = ((aliasCheckSum & 1) ? 0x80 : 0) + (aliasCheckSum >> 1) + entry->entryData[i];
   }

   /* Find or create space for the entry */
   if (_FAT_directory_findEntryGap (partition, entry, dirCluster, entrySize) == false)
      return false;

   /* Write out directory entry */
   curEntryPos = entry->dataStart;

   {
      /* lfn is only pushed onto the stack here, reducing overall stack usage */
      ucs2_t lfn[MAX_LFN_LENGTH] = {0};
      _FAT_directory_mbstoucs2 (lfn, entry->filename, MAX_LFN_LENGTH);

      for (entryStillValid = true, i = entrySize; entryStillValid && i > 0;
            entryStillValid = _FAT_directory_incrementDirEntryPosition (partition, &curEntryPos, false), -- i )
      {
         if (i > 1)
         {
            /* Long filename entry */
            lfnEntry[LFN_offset_ordinal] = (i - 1) | ((size_t)i == entrySize ? LFN_END : 0);
            for (j = 0; j < 13; j++)
            {
               if (lfn [(i - 2) * 13 + j] == '\0')
               {
                  if ((j > 1) && (lfn [(i - 2) * 13 + (j-1)] == '\0'))
                     u16_to_u8array (lfnEntry, LFN_offset_table[j], 0xffff);		/* Padding */
                  else
                     u16_to_u8array (lfnEntry, LFN_offset_table[j], 0x0000);		/* Terminating null character */
               }
               else
                  u16_to_u8array (lfnEntry, LFN_offset_table[j], lfn [(i - 2) * 13 + j]);
            }

            lfnEntry[LFN_offset_checkSum] = aliasCheckSum;
            lfnEntry[LFN_offset_flag] = ATTRIB_LFN;
            lfnEntry[LFN_offset_reserved1] = 0;
            u16_to_u8array (lfnEntry, LFN_offset_reserved2, 0);
            _FAT_cache_writePartialSector (partition->cache, lfnEntry, _FAT_fat_clusterToSector(partition, curEntryPos.cluster) + curEntryPos.sector, curEntryPos.offset * DIR_ENTRY_DATA_SIZE, DIR_ENTRY_DATA_SIZE);
         }
         else
         {
            /* Alias & file data */
            _FAT_cache_writePartialSector (partition->cache, entry->entryData, _FAT_fat_clusterToSector(partition, curEntryPos.cluster) + curEntryPos.sector, curEntryPos.offset * DIR_ENTRY_DATA_SIZE, DIR_ENTRY_DATA_SIZE);
         }
      }
   }

   return true;
}

bool _FAT_directory_chdir (PARTITION* partition, const char* path)
{
   DIR_ENTRY entry;

   if (!_FAT_directory_entryFromPath (partition, &entry, path, NULL))
      return false;

   if (!(entry.entryData[DIR_ENTRY_attributes] & ATTRIB_DIR))
      return false;

   partition->cwdCluster = _FAT_directory_entryGetCluster (partition, entry.entryData);

   return true;
}

void _FAT_directory_entryStat (PARTITION* partition, DIR_ENTRY* entry, struct stat *st)
{
   /* Fill in the stat struct
    * Some of the values are faked for the sake of compatibility */
   st->st_dev = _FAT_disc_hostType(partition->disc);					/* The device is the 32bit ioType value */
   st->st_ino = (ino_t)(_FAT_directory_entryGetCluster(partition, entry->entryData));		/* The file serial number is the start cluster */
   st->st_mode = (_FAT_directory_isDirectory(entry) ? S_IFDIR : S_IFREG) |
      (S_IRUSR | S_IRGRP | S_IROTH) |
      (_FAT_directory_isWritable (entry) ? (S_IWUSR | S_IWGRP | S_IWOTH) : 0);		/* Mode bits based on dirEntry ATTRIB byte */
   st->st_nlink = 1;								/* Always one hard link on a FAT file */
   st->st_uid = 1;									/* Faked for FAT */
   st->st_gid = 2;									/* Faked for FAT */
   st->st_rdev = st->st_dev;
   st->st_size = u8array_to_u32 (entry->entryData, DIR_ENTRY_fileSize);		/* File size */
   st->st_atime = _FAT_filetime_to_time_t (
         0,
         u8array_to_u16 (entry->entryData, DIR_ENTRY_aDate)
         );
   st->st_spare1 = 0;
   st->st_mtime = _FAT_filetime_to_time_t (
         u8array_to_u16 (entry->entryData, DIR_ENTRY_mTime),
         u8array_to_u16 (entry->entryData, DIR_ENTRY_mDate)
         );
   st->st_spare2 = 0;
   st->st_ctime = _FAT_filetime_to_time_t (
         u8array_to_u16 (entry->entryData, DIR_ENTRY_cTime),
         u8array_to_u16 (entry->entryData, DIR_ENTRY_cDate)
         );
   st->st_spare3 = 0;
   st->st_blksize = partition->bytesPerSector;				/* Prefered file I/O block size */
   st->st_blocks = (st->st_size + partition->bytesPerSector - 1) / partition->bytesPerSector;	/* File size in blocks */
   st->st_spare4[0] = 0;
   st->st_spare4[1] = 0;
}
