/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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
#include <stdint.h>
#include <sys/types.h>

#include <string.h>
#include "../miscellaneous.h"

#include "7zip/7z.h"
#include "7zip/7zAlloc.h"
#include "7zip/7zCrc.h"
#include "7zip/7zFile.h"
#include "7zip/7zVersion.h"

static ISzAlloc g_Alloc = { SzAlloc, SzFree };

static int Buf_EnsureSize(CBuf *dest, size_t size)
{
   if (dest->size >= size)
      return 1;
   Buf_Free(dest, &g_Alloc);
   return Buf_Create(dest, size, &g_Alloc);
}

#ifndef _WIN32

static Byte kUtf8Limits[5] = { 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

static Bool Utf16_To_Utf8(Byte *dest, size_t *destLen, const UInt16 *src, size_t srcLen)
{
   size_t destPos = 0, srcPos = 0;
   for (;;)
   {
      unsigned numAdds;
      UInt32 value;
      if (srcPos == srcLen)
      {
         *destLen = destPos;
         return True;
      }
      value = src[srcPos++];
      if (value < 0x80)
      {
         if (dest)
            dest[destPos] = (char)value;
         destPos++;
         continue;
      }
      if (value >= 0xD800 && value < 0xE000)
      {
         UInt32 c2;
         if (value >= 0xDC00 || srcPos == srcLen)
            break;
         c2 = src[srcPos++];
         if (c2 < 0xDC00 || c2 >= 0xE000)
            break;
         value = (((value - 0xD800) << 10) | (c2 - 0xDC00)) + 0x10000;
      }
      for (numAdds = 1; numAdds < 5; numAdds++)
         if (value < (((UInt32)1) << (numAdds * 5 + 6)))
            break;
      if (dest)
         dest[destPos] = (char)(kUtf8Limits[numAdds - 1] + (value >> (6 * numAdds)));
      destPos++;
      do
      {
         numAdds--;
         if (dest)
            dest[destPos] = (char)(0x80 + ((value >> (6 * numAdds)) & 0x3F));
         destPos++;
      }
      while (numAdds != 0);
   }
   *destLen = destPos;
   return False;
}

static SRes Utf16_To_Utf8Buf(CBuf *dest, const UInt16 *src, size_t srcLen)
{
   size_t destLen = 0;
   Bool res;
   Utf16_To_Utf8(NULL, &destLen, src, srcLen);
   destLen += 1;
   if (!Buf_EnsureSize(dest, destLen))
      return SZ_ERROR_MEM;
   res = Utf16_To_Utf8(dest->data, &destLen, src, srcLen);
   dest->data[destLen] = 0;
   return res ? SZ_OK : SZ_ERROR_FAIL;
}
#endif

static SRes Utf16_To_Char(CBuf *buf, const UInt16 *s, int fileMode)
{
   int len = 0;
   for (len = 0; s[len] != '\0'; len++);

#ifdef _WIN32
   {
      int size = len * 3 + 100;
      if (!Buf_EnsureSize(buf, size))
         return SZ_ERROR_MEM;
      {
         char defaultChar = '_';
         BOOL defUsed;
         int numChars = WideCharToMultiByte(fileMode ?
               (
#ifdef UNDER_CE
                     CP_ACP
#else
                     AreFileApisANSI() ? CP_ACP : CP_OEMCP
#endif
               ) : CP_OEMCP,
               0, s, len, (char *)buf->data, size, &defaultChar, &defUsed);
         if (numChars == 0 || numChars >= size)
            return SZ_ERROR_FAIL;
         buf->data[numChars] = 0;
         return SZ_OK;
      }
   }
#else
   fileMode = fileMode;
   return Utf16_To_Utf8Buf(buf, s, len);
#endif
}


static SRes ConvertUtf16toCharString(const UInt16 *s, char *outstring)
{
   CBuf buf;
   SRes res;
   Buf_Init(&buf);
   res = Utf16_To_Char(&buf, s, 0);

   if (res == SZ_OK)
      strncpy(outstring,(const char *)buf.data,PATH_MAX);

   Buf_Free(&buf, &g_Alloc);
   return res;
}


// Extract the relative path relative_path from a 7z archive archive_path and allocate a buf for it to write it in.
long read_7zip_file(const char * archive_path, const char *relative_path, void **buf)
{
   CFileInStream archiveStream;
   CLookToRead lookStream;
   CSzArEx db;
   SRes res;
   ISzAlloc allocImp;
   ISzAlloc allocTempImp;
   UInt16 *temp = NULL;
   size_t tempSize = 0;
   long outsize = -1;

   //These are the allocation routines - currently using the non-standard 7zip choices.
   allocImp.Alloc = SzAlloc;
   allocImp.Free = SzFree;
   allocTempImp.Alloc = SzAllocTemp;
   allocTempImp.Free = SzFreeTemp;

   if (InFile_Open(&archiveStream.file, archive_path))
   {
      RARCH_ERR("Could not open %s as 7z archive\n.",archive_path);
      return -1;
   }
   else
   {
      RARCH_LOG_OUTPUT("Openend archive %s. Now trying to extract %s\n",archive_path,relative_path);
   }
   FileInStream_CreateVTable(&archiveStream);
   LookToRead_CreateVTable(&lookStream, False);
   lookStream.realStream = &archiveStream.s;
   LookToRead_Init(&lookStream);
   CrcGenerateTable();
   SzArEx_Init(&db);
   res = SzArEx_Open(&db, &lookStream.s, &allocImp, &allocTempImp);
   if (res == SZ_OK)
   {
      UInt32 i;
      UInt32 blockIndex = 0xFFFFFFFF;
      Byte *outBuffer = 0;
      size_t outBufferSize = 0;

      for (i = 0; i < db.db.NumFiles; i++)
      {
         size_t offset = 0;
         size_t outSizeProcessed = 0;
         const CSzFileItem *f = db.db.Files + i;
         size_t len;
         if (f->IsDir)
         {
            //we skip over everything, which is not a directory.
            continue;
         }
         len = SzArEx_GetFileNameUtf16(&db, i, NULL);
         if (len > tempSize)
         {
            SzFree(NULL, temp);
            tempSize = len;
            temp = (UInt16 *)SzAlloc(NULL, tempSize * sizeof(temp[0]));
            if (temp == 0)
            {
               res = SZ_ERROR_MEM;
               break;
            }
         }
         SzArEx_GetFileNameUtf16(&db, i, temp);
         char infile[PATH_MAX];
         res = ConvertUtf16toCharString(temp,infile);

         UInt64 filesize = f->Size;
         (void)filesize;

         if (strcmp(infile,relative_path) == 0)
         {
            res = SzArEx_Extract(&db, &lookStream.s, i,&blockIndex, &outBuffer, &outBufferSize,&offset, &outSizeProcessed,&allocImp, &allocTempImp);
            if (res != SZ_OK)
            {
               break; //This goes to the error section.
            }
            outsize = outSizeProcessed;
            *buf = outBuffer+offset;

            //We could either use the 7Zip allocated buffer or create our own and use it.
            //We would however need to realloc anyways, because RetroArch expects a \0 at the end, therefore we allocate new, copy and free the old one.
            *buf = malloc(outsize + 1);

            ((char*)(*buf))[outsize] = '\0';
            memcpy(*buf,outBuffer+offset,outsize);
            IAlloc_Free(&allocImp, outBuffer);
            break;
         }
      }
   }
   SzArEx_Free(&db, &allocImp);
   SzFree(NULL, temp);

   File_Close(&archiveStream.file);

   if (res == SZ_OK)
      return outsize;

   //Error handling:
   if (res == SZ_ERROR_UNSUPPORTED)
      RARCH_ERR("7Zip decoder doesn't support this archive\n");
   else if (res == SZ_ERROR_MEM)
      RARCH_ERR("7Zip decoder could not allocate memory\n");
   else if (res == SZ_ERROR_CRC)
      RARCH_ERR("7Zip decoder encountered a CRC error in the archive\n");
   else
      RARCH_ERR("\nUnspecified error in 7-ZIP archive, error number was: #%d\n", res);
   return -1;
}
