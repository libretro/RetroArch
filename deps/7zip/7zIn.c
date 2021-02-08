/* 7zIn.c -- 7z Input functions
   2010-10-29 : Igor Pavlov : Public domain */

#include <stdint.h>
#include <string.h>

#include "7z.h"
#include "7zCrc.h"
#include "CpuArch.h"

uint8_t k7zSignature[k7zSignatureSize] = {'7', 'z', 0xBC, 0xAF, 0x27, 0x1C};

#define NUM_FOLDER_CODERS_MAX 32
#define NUM_CODER_STREAMS_MAX 32

#ifdef MY_CPU_LE_UNALIGN
#define GetUi16(p) (*(const uint16_t *)(p))
#define GetUi32(p) (*(const uint32_t *)(p))
#define GetUi64(p) (*(const uint64_t *)(p))
#else
#define GetUi16(p) (((const uint8_t *)(p))[0] | ((uint16_t)((const uint8_t *)(p))[1] << 8))
#define GetUi32(p) ( \
             ((const uint8_t *)(p))[0]        | \
    ((uint32_t)((const uint8_t *)(p))[1] <<  8) | \
    ((uint32_t)((const uint8_t *)(p))[2] << 16) | \
    ((uint32_t)((const uint8_t *)(p))[3] << 24))
#define GetUi64(p) (GetUi32(p) | ((uint64_t)GetUi32(((const uint8_t *)(p)) + 4) << 32))
#endif

typedef struct _CSzState
{
   uint8_t *Data;
   size_t Size;
}CSzData;

/* Forward declarations */
void SzFolder_Free(CSzFolder *p, ISzAlloc *alloc);
int SzFolder_FindBindPairForOutStream(CSzFolder *p,
      uint32_t outStreamIndex);

void SzCoderInfo_Init(CSzCoderInfo *p)
{
   Buf_Init(&p->Props);
}

void SzCoderInfo_Free(CSzCoderInfo *p, ISzAlloc *alloc)
{
   Buf_Free(&p->Props, alloc);
   SzCoderInfo_Init(p);
}

void SzFolder_Init(CSzFolder *p)
{
   p->Coders           = 0;
   p->BindPairs        = 0;
   p->PackStreams      = 0;
   p->UnpackSizes      = 0;
   p->NumCoders        = 0;
   p->NumBindPairs     = 0;
   p->NumPackStreams   = 0;
   p->UnpackCRCDefined = 0;
   p->UnpackCRC        = 0;
   p->NumUnpackStreams = 0;
}

void SzFolder_Free(CSzFolder *p, ISzAlloc *alloc)
{
   uint32_t i;
   if (p->Coders)
      for (i = 0; i < p->NumCoders; i++)
         SzCoderInfo_Free(&p->Coders[i], alloc);
   IAlloc_Free(alloc, p->Coders);
   IAlloc_Free(alloc, p->BindPairs);
   IAlloc_Free(alloc, p->PackStreams);
   IAlloc_Free(alloc, p->UnpackSizes);
   SzFolder_Init(p);
}

uint32_t SzFolder_GetNumOutStreams(CSzFolder *p)
{
   uint32_t i;
   uint32_t result = 0;
   for (i = 0; i < p->NumCoders; i++)
      result += p->Coders[i].NumOutStreams;
   return result;
}

int SzFolder_FindBindPairForInStream(CSzFolder *p, uint32_t inStreamIndex)
{
   uint32_t i;
   for (i = 0; i < p->NumBindPairs; i++)
      if (p->BindPairs[i].InIndex == inStreamIndex)
         return i;
   return -1;
}


int SzFolder_FindBindPairForOutStream(CSzFolder *p, uint32_t outStreamIndex)
{
   uint32_t i;
   for (i = 0; i < p->NumBindPairs; i++)
      if (p->BindPairs[i].OutIndex == outStreamIndex)
         return i;
   return -1;
}

uint64_t SzFolder_GetUnpackSize(CSzFolder *p)
{
   int i = (int)SzFolder_GetNumOutStreams(p);
   if (i == 0)
      return 0;
   for (i--; i >= 0; i--)
      if (SzFolder_FindBindPairForOutStream(p, i) < 0)
         return p->UnpackSizes[i];
   return 0;
}

void SzFile_Init(CSzFileItem *p)
{
   p->HasStream    = 1;
   p->IsDir        = 0;
   p->IsAnti       = 0;
   p->CrcDefined   = 0;
   p->MTimeDefined = 0;
}

void SzAr_Init(CSzAr *p)
{
   p->PackSizes       = 0;
   p->PackCRCsDefined = 0;
   p->PackCRCs        = 0;
   p->Folders         = 0;
   p->Files           = 0;
   p->NumPackStreams  = 0;
   p->NumFolders      = 0;
   p->NumFiles        = 0;
}

void SzAr_Free(CSzAr *p, ISzAlloc *alloc)
{
   uint32_t i;
   if (p->Folders)
      for (i = 0; i < p->NumFolders; i++)
         SzFolder_Free(&p->Folders[i], alloc);

   IAlloc_Free(alloc, p->PackSizes);
   IAlloc_Free(alloc, p->PackCRCsDefined);
   IAlloc_Free(alloc, p->PackCRCs);
   IAlloc_Free(alloc, p->Folders);
   IAlloc_Free(alloc, p->Files);
   SzAr_Init(p);
}


void SzArEx_Init(CSzArEx *p)
{
   SzAr_Init(&p->db);
   p->FolderStartPackStreamIndex = 0;
   p->PackStreamStartPositions   = 0;
   p->FolderStartFileIndex       = 0;
   p->FileIndexToFolderIndexMap  = 0;
   p->FileNameOffsets            = 0;
   Buf_Init(&p->FileNames);
}

void SzArEx_Free(CSzArEx *p, ISzAlloc *alloc)
{
   IAlloc_Free(alloc, p->FolderStartPackStreamIndex);
   IAlloc_Free(alloc, p->PackStreamStartPositions);
   IAlloc_Free(alloc, p->FolderStartFileIndex);
   IAlloc_Free(alloc, p->FileIndexToFolderIndexMap);

   IAlloc_Free(alloc, p->FileNameOffsets);
   Buf_Free(&p->FileNames, alloc);

   SzAr_Free(&p->db, alloc);
   SzArEx_Init(p);
}

#if 0
uint64_t GetFolderPackStreamSize(int folderIndex, int streamIndex)
{
   return PackSizes[FolderStartPackStreamIndex[folderIndex] + streamIndex];
}

uint64_t GetFilePackSize(int fileIndex)
{
   int folderIndex = FileIndexToFolderIndexMap[fileIndex];
   if (folderIndex >= 0)
   {
      const CSzFolder &folderInfo = Folders[folderIndex];
      if (FolderStartFileIndex[folderIndex] == fileIndex)
         return GetFolderFullPackSize(folderIndex);
   }
   return 0;
}
#endif

#define MY_ALLOC(T, p, size, alloc) { if ((size) == 0) p = 0; else \
   if ((p = (T *)IAlloc_Alloc(alloc, (size) * sizeof(T))) == 0) return SZ_ERROR_MEM; }

static SRes SzArEx_Fill(CSzArEx *p, ISzAlloc *alloc)
{
   uint32_t i;
   uint32_t startPos      = 0;
   uint64_t startPosSize  = 0;
   uint32_t folderIndex   = 0;
   uint32_t indexInFolder = 0;
   MY_ALLOC(uint32_t, p->FolderStartPackStreamIndex, p->db.NumFolders, alloc);
   for (i = 0; i < p->db.NumFolders; i++)
   {
      p->FolderStartPackStreamIndex[i] = startPos;
      startPos += p->db.Folders[i].NumPackStreams;
   }

   MY_ALLOC(uint64_t, p->PackStreamStartPositions, p->db.NumPackStreams, alloc);

   for (i = 0; i < p->db.NumPackStreams; i++)
   {
      p->PackStreamStartPositions[i] = startPosSize;
      startPosSize += p->db.PackSizes[i];
   }

   MY_ALLOC(uint32_t, p->FolderStartFileIndex, p->db.NumFolders, alloc);
   MY_ALLOC(uint32_t, p->FileIndexToFolderIndexMap, p->db.NumFiles, alloc);

   for (i = 0; i < p->db.NumFiles; i++)
   {
      CSzFileItem *file = p->db.Files + i;
      int emptyStream   = !file->HasStream;

      if (emptyStream && indexInFolder == 0)
      {
         p->FileIndexToFolderIndexMap[i] = (uint32_t)-1;
         continue;
      }
      if (indexInFolder == 0)
      {
         /*
            v3.13 incorrectly worked with empty folders
            v4.07: Loop for skipping empty folders
            */
         for (;;)
         {
            if (folderIndex >= p->db.NumFolders)
               return SZ_ERROR_ARCHIVE;
            p->FolderStartFileIndex[folderIndex] = i;
            if (p->db.Folders[folderIndex].NumUnpackStreams != 0)
               break;
            folderIndex++;
         }
      }

      p->FileIndexToFolderIndexMap[i] = folderIndex;

      if (emptyStream)
         continue;

      indexInFolder++;

      if (indexInFolder >= p->db.Folders[folderIndex].NumUnpackStreams)
      {
         folderIndex++;
         indexInFolder = 0;
      }
   }
   return SZ_OK;
}


uint64_t SzArEx_GetFolderStreamPos(
      const CSzArEx *p, uint32_t folderIndex, uint32_t indexInFolder)
{
   return p->dataPos +
      p->PackStreamStartPositions[p->FolderStartPackStreamIndex[folderIndex] + indexInFolder];
}

int SzArEx_GetFolderFullPackSize(
      const CSzArEx *p, uint32_t folderIndex, uint64_t *resSize)
{
   uint32_t i;
   uint64_t size            = 0;
   uint32_t packStreamIndex = p->FolderStartPackStreamIndex[folderIndex];
   CSzFolder        *folder = p->db.Folders + folderIndex;

   for (i = 0; i < folder->NumPackStreams; i++)
   {
      uint64_t t = size + p->db.PackSizes[packStreamIndex + i];
      if (t < size) /* check it */
         return SZ_ERROR_FAIL;
      size = t;
   }
   *resSize = size;
   return SZ_OK;
}


static int TestSignatureCandidate(uint8_t *testuint8_ts)
{
   size_t i;
   for (i = 0; i < k7zSignatureSize; i++)
      if (testuint8_ts[i] != k7zSignature[i])
         return 0;
   return 1;
}

static SRes SzReaduint8_t(CSzData *sd, uint8_t *b)
{
   if (sd->Size == 0)
      return SZ_ERROR_ARCHIVE;
   sd->Size--;
   *b = *sd->Data++;
   return SZ_OK;
}

static SRes SzReaduint8_ts(CSzData *sd, uint8_t *data, size_t size)
{
   size_t i;
   for (i = 0; i < size; i++)
   {
      SRes result = SzReaduint8_t(sd, data + i);
      if (result != 0)
         return result;
   }
   return SZ_OK;
}

static SRes SzReaduint32_t(CSzData *sd, uint32_t *value)
{
   int i;
   *value = 0;
   for (i = 0; i < 4; i++)
   {
      uint8_t b   = 0;
      SRes result = SzReaduint8_t(sd, &b);
      if (result != 0)
         return result;
      *value |= ((uint32_t)(b) << (8 * i));
   }
   return SZ_OK;
}

static SRes SzReadNumber(CSzData *sd, uint64_t *value)
{
   int i;
   uint8_t firstuint8_t = 0;
   uint8_t mask         = 0x80;
   SRes result          = SzReaduint8_t(sd, &firstuint8_t);

   if (result != 0)
      return result;

   *value               = 0;

   for (i = 0; i < 8; i++)
   {
      SRes result;
      uint8_t b         = 0;
      if ((firstuint8_t & mask) == 0)
      {
         uint64_t highPart = firstuint8_t & (mask - 1);
         *value += (highPart << (8 * i));
         return SZ_OK;
      }
      result = SzReaduint8_t(sd, &b);

      if (result != 0)
         return result;
      *value |= ((uint64_t)b << (8 * i));
      mask >>= 1;
   }
   return SZ_OK;
}

static SRes SzReadNumber32(CSzData *sd, uint32_t *value)
{
   uint64_t value64;
   SRes result      = SzReadNumber(sd, &value64);
   if (result != 0)
      return result;
   if (value64 >= 0x80000000)
      return SZ_ERROR_UNSUPPORTED;
   if (value64 >= ((uint64_t)(1) << ((sizeof(size_t) - 1) * 8 + 2)))
      return SZ_ERROR_UNSUPPORTED;
   *value = (uint32_t)value64;
   return SZ_OK;
}

static SRes SzReadID(CSzData *sd, uint64_t *value)
{
   return SzReadNumber(sd, value);
}

static SRes SzSkeepDataSize(CSzData *sd, uint64_t size)
{
   if (size > sd->Size)
      return SZ_ERROR_ARCHIVE;
   sd->Size -= (size_t)size;
   sd->Data += (size_t)size;
   return SZ_OK;
}

static SRes SzSkeepData(CSzData *sd)
{
   uint64_t size;
   SRes result    = SzReadNumber(sd, &size);
   if (result != 0)
      return result;
   return SzSkeepDataSize(sd, size);
}

static SRes SzReadArchiveProperties(CSzData *sd)
{
   for (;;)
   {
      uint64_t type;
      SRes result    = SzReadID(sd, &type);
      if (result != 0)
         return result;
      if (type == k7zIdEnd)
         break;
      SzSkeepData(sd);
   }
   return SZ_OK;
}

static SRes SzWaitAttribute(CSzData *sd, uint64_t attribute)
{
   for (;;)
   {
      uint64_t type;
      SRes result    = SzReadID(sd, &type);
      if (result != 0)
         return result;
      if (type == attribute)
         return SZ_OK;
      if (type == k7zIdEnd)
         return SZ_ERROR_ARCHIVE;
      result = SzSkeepData(sd);
      if (result != 0)
         return result;
   }
}

static SRes SzReadBoolVector(
      CSzData *sd, size_t numItems, uint8_t **v, ISzAlloc *alloc)
{
   size_t i;
   uint8_t b    = 0;
   uint8_t mask = 0;
   MY_ALLOC(uint8_t, *v, numItems, alloc);
   for (i = 0; i < numItems; i++)
   {
      if (mask == 0)
      {
         SRes result = SzReaduint8_t(sd, &b);
         if (result != 0)
            return result;
         mask       = 0x80;
      }
      (*v)[i] = (uint8_t)(((b & mask) != 0) ? 1 : 0);
      mask >>= 1;
   }
   return SZ_OK;
}

static SRes SzReadBoolVector2(CSzData *sd, size_t numItems, uint8_t **v, ISzAlloc *alloc)
{
   size_t i;
   uint8_t allAreDefined = 0;
   SRes result           = SzReaduint8_t(sd, &allAreDefined);

   if (result != 0)
      return result;
   if (allAreDefined == 0)
      return SzReadBoolVector(sd, numItems, v, alloc);
   MY_ALLOC(uint8_t, *v, numItems, alloc);
   for (i = 0; i < numItems; i++)
      (*v)[i] = 1;
   return SZ_OK;
}

static SRes SzReadHashDigests(
      CSzData *sd,
      size_t numItems,
      uint8_t **digestsDefined,
      uint32_t **digests,
      ISzAlloc *alloc)
{
   size_t i;
   SRes result = SzReadBoolVector2(sd, numItems, digestsDefined, alloc);

   if (result != 0)
      return result;

   MY_ALLOC(uint32_t, *digests, numItems, alloc);

   for (i = 0; i < numItems; i++)
   {
      if ((*digestsDefined)[i])
      {
         SRes result = SzReaduint32_t(sd, (*digests) + i);
         if (result != 0)
            return result;
      }
   }
   return SZ_OK;
}

static SRes SzReadPackInfo(
      CSzData *sd,
      uint64_t *dataOffset,
      uint32_t *numPackStreams,
      uint64_t **packSizes,
      uint8_t **packCRCsDefined,
      uint32_t **packCRCs,
      ISzAlloc *alloc)
{
   uint32_t i;
   SRes result = SzReadNumber(sd, dataOffset);

   if (result != 0)
      return result;
   result     = SzReadNumber32(sd, numPackStreams);
   if (result != 0)
      return result;
   result     = SzWaitAttribute(sd, k7zIdSize);
   if (result != 0)
      return result;

   MY_ALLOC(uint64_t, *packSizes, (size_t)*numPackStreams, alloc);

   for (i = 0; i < *numPackStreams; i++)
   {
      result = SzReadNumber(sd, (*packSizes) + i);
      if (result != 0)
         return result;
   }

   for (;;)
   {
      uint64_t type;
      result        = SzReadID(sd, &type);
      if (result != 0)
         return result;
      if (type == k7zIdEnd)
         break;
      if (type == k7zIdCRC)
      {
         result = SzReadHashDigests(sd, (size_t)*numPackStreams, packCRCsDefined, packCRCs, alloc);
         if (result != 0)
            return result;
         continue;
      }
      result = SzSkeepData(sd);
      if (result != 0)
         return result;
   }
   if (*packCRCsDefined == 0)
   {
      MY_ALLOC(uint8_t, *packCRCsDefined, (size_t)*numPackStreams, alloc);
      MY_ALLOC(uint32_t, *packCRCs, (size_t)*numPackStreams, alloc);
      for (i = 0; i < *numPackStreams; i++)
      {
         (*packCRCsDefined)[i] = 0;
         (*packCRCs)[i] = 0;
      }
   }
   return SZ_OK;
}

static SRes SzReadSwitch(CSzData *sd)
{
   uint8_t external = 0;
   SRes result      = SzReaduint8_t(sd, &external);
   if (result != 0)
      return result;
   if (external != 0)
      return SZ_ERROR_UNSUPPORTED;
   return SZ_OK;
}

static SRes SzGetNextFolderItem(CSzData *sd, CSzFolder *folder, ISzAlloc *alloc)
{
   unsigned i;
   uint32_t numBindPairs   = 0;
   uint32_t numPackStreams = 0;
   uint32_t numInStreams   = 0;
   uint32_t numOutStreams  = 0;
   uint32_t numCoders      = 0;
   SRes result             = SzReadNumber32(sd, &numCoders);

   if (result != 0)
      return result;
   if (numCoders > NUM_FOLDER_CODERS_MAX)
      return SZ_ERROR_UNSUPPORTED;
   folder->NumCoders = numCoders;

   MY_ALLOC(CSzCoderInfo, folder->Coders, (size_t)numCoders, alloc);

   for (i = 0; i < numCoders; i++)
      SzCoderInfo_Init(folder->Coders + i);

   for (i = 0; i < numCoders; i++)
   {
      unsigned idSize, j;
      uint8_t longID[15];
      uint8_t mainuint8_t = 0;
      CSzCoderInfo *coder = folder->Coders + i;
      SRes result         = SzReaduint8_t(sd, &mainuint8_t);

      if (result != 0)
         return result;

      idSize              = (unsigned)(mainuint8_t & 0xF);

      result              = SzReaduint8_ts(sd, longID, idSize);

      if (result != 0)
         return result;
      if (idSize > sizeof(coder->MethodID))
         return SZ_ERROR_UNSUPPORTED;

      coder->MethodID     = 0;
      for (j = 0; j < idSize; j++)
         coder->MethodID |= (uint64_t)longID[idSize - 1 - j] << (8 * j);

      if ((mainuint8_t & 0x10) != 0)
      {
         SRes result = SzReadNumber32(sd, &coder->NumInStreams);

         if (result != 0)
            return result;

         result     = SzReadNumber32(sd, &coder->NumOutStreams);
         
         if (result != 0)
            return result;

         if (coder->NumInStreams > NUM_CODER_STREAMS_MAX ||
               coder->NumOutStreams > NUM_CODER_STREAMS_MAX)
            return SZ_ERROR_UNSUPPORTED;
      }
      else
      {
         coder->NumInStreams  = 1;
         coder->NumOutStreams = 1;
      }
      if ((mainuint8_t & 0x20) != 0)
      {
         uint64_t propertiesSize = 0;
         SRes result             = SzReadNumber(sd, &propertiesSize);

         if (result != 0)
            return result;
         if (!Buf_Create(&coder->Props, (size_t)propertiesSize, alloc))
            return SZ_ERROR_MEM;
         result                  = SzReaduint8_ts(sd, coder->Props.data, (size_t)propertiesSize);
         if (result != 0)
            return result;
      }
      while ((mainuint8_t & 0x80) != 0)
      {
         result = SzReaduint8_t(sd, &mainuint8_t);
         if (result != 0)
            return result;
         result = SzSkeepDataSize(sd, (mainuint8_t & 0xF));
         if (result != 0)
            return result;
         if ((mainuint8_t & 0x10) != 0)
         {
            uint32_t n;
            SRes result = SzReadNumber32(sd, &n);
            if (result != 0)
               return result;
            result     = SzReadNumber32(sd, &n);
            if (result != 0)
               return result;
         }
         if ((mainuint8_t & 0x20) != 0)
         {
            uint64_t propertiesSize = 0;
            SRes result             = SzReadNumber(sd, &propertiesSize);
            if (result != 0)
               return result;
            result                  = SzSkeepDataSize(sd, propertiesSize);
            if (result != 0)
               return result;
         }
      }
      numInStreams += coder->NumInStreams;
      numOutStreams += coder->NumOutStreams;
   }

   if (numOutStreams == 0)
      return SZ_ERROR_UNSUPPORTED;

   folder->NumBindPairs = numBindPairs = numOutStreams - 1;
   MY_ALLOC(CSzBindPair, folder->BindPairs, (size_t)numBindPairs, alloc);

   for (i = 0; i < numBindPairs; i++)
   {
      CSzBindPair *bp = folder->BindPairs + i;
      SRes result     = SzReadNumber32(sd, &bp->InIndex);

      if (result != 0)
         return result;
      result          = SzReadNumber32(sd, &bp->OutIndex);
      if (result != 0)
         return result;
   }

   if (numInStreams < numBindPairs)
      return SZ_ERROR_UNSUPPORTED;

   folder->NumPackStreams = numPackStreams = numInStreams - numBindPairs;
   MY_ALLOC(uint32_t, folder->PackStreams, (size_t)numPackStreams, alloc);

   if (numPackStreams == 1)
   {
      for (i = 0; i < numInStreams ; i++)
         if (SzFolder_FindBindPairForInStream(folder, i) < 0)
            break;
      if (i == numInStreams)
         return SZ_ERROR_UNSUPPORTED;
      folder->PackStreams[0] = i;
   }
   else
      for (i = 0; i < numPackStreams; i++)
      {
         SRes result = SzReadNumber32(sd, folder->PackStreams + i);
         if (result != 0)
            return result;
      }
   return SZ_OK;
}

static SRes SzReadUnpackInfo(
      CSzData *sd,
      uint32_t *numFolders,
      CSzFolder **folders,  /* for alloc */
      ISzAlloc *alloc,
      ISzAlloc *allocTemp)
{
   uint32_t i;
   SRes result = SzWaitAttribute(sd, k7zIdFolder);

   if (result != 0)
      return result;

   result     = SzReadNumber32(sd, numFolders);

   if (result != 0)
      return result;

   {
      result  = SzReadSwitch(sd);

      if (result != 0)
         return result;

      MY_ALLOC(CSzFolder, *folders, (size_t)*numFolders, alloc);

      for (i = 0; i < *numFolders; i++)
         SzFolder_Init((*folders) + i);

      for (i = 0; i < *numFolders; i++)
      {
         result = SzGetNextFolderItem(sd, (*folders) + i, alloc);

         if (result != 0)
            return result;
      }
   }

   result = SzWaitAttribute(sd, k7zIdCodersUnpackSize);

   if (result != 0)
      return result;

   for (i = 0; i < *numFolders; i++)
   {
      uint32_t j;
      CSzFolder *folder = (*folders) + i;
      uint32_t numOutStreams = SzFolder_GetNumOutStreams(folder);

      MY_ALLOC(uint64_t, folder->UnpackSizes, (size_t)numOutStreams, alloc);

      for (j = 0; j < numOutStreams; j++)
      {
         SRes result = SzReadNumber(sd, folder->UnpackSizes + j);
         if (result != 0)
            return result;
      }
   }

   for (;;)
   {
      uint64_t type;
      SRes result    = SzReadID(sd, &type);
      if (result != 0)
         return result;
      if (type == k7zIdEnd)
         return SZ_OK;
      if (type == k7zIdCRC)
      {
         uint8_t *crcsDefined = 0;
         uint32_t *crcs       = 0;
         SRes res             = SzReadHashDigests(
               sd, *numFolders, &crcsDefined, &crcs, allocTemp);

         if (res == SZ_OK)
         {
            for (i = 0; i < *numFolders; i++)
            {
               CSzFolder *folder = (*folders) + i;
               folder->UnpackCRCDefined = crcsDefined[i];
               folder->UnpackCRC = crcs[i];
            }
         }
         IAlloc_Free(allocTemp, crcs);
         IAlloc_Free(allocTemp, crcsDefined);
         if (res != 0)
            return res;
         continue;
      }
      result = SzSkeepData(sd);
      if (result != 0)
         return result;
   }
}

static SRes SzReadSubStreamsInfo(
      CSzData *sd,
      uint32_t numFolders,
      CSzFolder *folders,
      uint32_t *numUnpackStreams,
      uint64_t **unpackSizes,
      uint8_t **digestsDefined,
      uint32_t **digests,
      ISzAlloc *allocTemp)
{
   SRes result;
   uint32_t i;
   uint64_t type       = 0;
   uint32_t si         = 0;
   uint32_t numDigests = 0;

   for (i = 0; i < numFolders; i++)
      folders[i].NumUnpackStreams = 1;
   *numUnpackStreams = numFolders;

   for (;;)
   {
      SRes result = SzReadID(sd, &type);
      if (result != 0)
         return result;
      if (type == k7zIdNumUnpackStream)
      {
         *numUnpackStreams = 0;
         for (i = 0; i < numFolders; i++)
         {
            uint32_t numStreams = 0;
            SRes result         = SzReadNumber32(sd, &numStreams);

            if (result != 0)
               return result;
            folders[i].NumUnpackStreams  = numStreams;
            *numUnpackStreams           += numStreams;
         }
         continue;
      }
      if (type == k7zIdCRC || type == k7zIdSize)
         break;
      if (type == k7zIdEnd)
         break;
      result = SzSkeepData(sd);
      if (result != 0)
         return result;
   }

   if (*numUnpackStreams == 0)
   {
      *unpackSizes    = 0;
      *digestsDefined = 0;
      *digests        = 0;
   }
   else
   {
      *unpackSizes = (uint64_t *)IAlloc_Alloc(allocTemp, (size_t)*numUnpackStreams * sizeof(uint64_t));
      if (*unpackSizes == 0)
         return SZ_ERROR_MEM;
      *digestsDefined = (uint8_t *)IAlloc_Alloc(allocTemp, (size_t)*numUnpackStreams * sizeof(uint8_t));
      if (*digestsDefined == 0)
         return SZ_ERROR_MEM;
      *digests = (uint32_t *)IAlloc_Alloc(allocTemp, (size_t)*numUnpackStreams * sizeof(uint32_t));
      if (*digests == 0)
         return SZ_ERROR_MEM;
   }

   for (i = 0; i < numFolders; i++)
   {
      /*
         v3.13 incorrectly worked with empty folders
         v4.07: we check that folder is empty
         */
      uint64_t sum = 0;
      uint32_t j;
      uint32_t numSubstreams = folders[i].NumUnpackStreams;
      if (numSubstreams == 0)
         continue;
      if (type == k7zIdSize)
         for (j = 1; j < numSubstreams; j++)
         {
            uint64_t size;
            SRes result    = SzReadNumber(sd, &size);

            if (result != 0)
               return result;
            (*unpackSizes)[si++] = size;
            sum          += size;
         }
      (*unpackSizes)[si++] = SzFolder_GetUnpackSize(folders + i) - sum;
   }
    
   if (type == k7zIdSize)
   {
      result = SzReadID(sd, &type);
      if (result != 0)
         return result;
   }

   for (i = 0; i < *numUnpackStreams; i++)
   {
      (*digestsDefined)[i] = 0;
      (*digests)[i] = 0;
   }


   for (i = 0; i < numFolders; i++)
   {
      uint32_t numSubstreams = folders[i].NumUnpackStreams;
      if (numSubstreams != 1 || !folders[i].UnpackCRCDefined)
         numDigests += numSubstreams;
   }


   si = 0;
   for (;;)
   {
      if (type == k7zIdCRC)
      {
         int digestIndex = 0;
         uint8_t *digestsDefined2 = 0;
         uint32_t *digests2 = 0;
         SRes res = SzReadHashDigests(sd, numDigests, &digestsDefined2, &digests2, allocTemp);
         if (res == SZ_OK)
         {
            for (i = 0; i < numFolders; i++)
            {
               CSzFolder *folder = folders + i;
               uint32_t numSubstreams = folder->NumUnpackStreams;
               if (numSubstreams == 1 && folder->UnpackCRCDefined)
               {
                  (*digestsDefined)[si] = 1;
                  (*digests)[si] = folder->UnpackCRC;
                  si++;
               }
               else
               {
                  uint32_t j;
                  for (j = 0; j < numSubstreams; j++, digestIndex++)
                  {
                     (*digestsDefined)[si] = digestsDefined2[digestIndex];
                     (*digests)[si] = digests2[digestIndex];
                     si++;
                  }
               }
            }
         }
         IAlloc_Free(allocTemp, digestsDefined2);
         IAlloc_Free(allocTemp, digests2);
         if (res != 0)
            return res;
      }
      else if (type == k7zIdEnd)
         return SZ_OK;
      else
      {
         result = SzSkeepData(sd);
         if (result != 0)
            return result;
      }
      result = SzReadID(sd, &type);
      if (result != 0)
         return result;
   }
}


static SRes SzReadStreamsInfo(
      CSzData *sd,
      uint64_t *dataOffset,
      CSzAr *p,
      uint32_t *numUnpackStreams,
      uint64_t **unpackSizes, /* allocTemp */
      uint8_t **digestsDefined,   /* allocTemp */
      uint32_t **digests,        /* allocTemp */
      ISzAlloc *alloc,
      ISzAlloc *allocTemp)
{
   for (;;)
   {
      uint64_t type;
      SRes result    = SzReadID(sd, &type);

      if (result != 0)
         return result;
      if ((uint64_t)(int)type != type)
         return SZ_ERROR_UNSUPPORTED;
      switch((int)type)
      {
         case k7zIdEnd:
            return SZ_OK;
         case k7zIdPackInfo:
            {
               SRes result = SzReadPackInfo(sd, dataOffset, &p->NumPackStreams,
                        &p->PackSizes, &p->PackCRCsDefined, &p->PackCRCs, alloc);
               if (result != 0)
                  return result;

               break;
            }
         case k7zIdUnpackInfo:
            {
               SRes result = SzReadUnpackInfo(sd, &p->NumFolders, &p->Folders, alloc, allocTemp);
               if (result != 0)
                  return result;
               break;
            }
         case k7zIdSubStreamsInfo:
            {
               SRes result = SzReadSubStreamsInfo(sd, p->NumFolders, p->Folders,
                     numUnpackStreams, unpackSizes, digestsDefined, digests, allocTemp);
               if (result != 0)
                  return result;
               break;
            }
         default:
            return SZ_ERROR_UNSUPPORTED;
      }
   }
}

size_t SzArEx_GetFileNameUtf16(const CSzArEx *p,
      size_t fileIndex, uint16_t *dest)
{
   size_t len = p->FileNameOffsets[fileIndex + 1] - 
      p->FileNameOffsets[fileIndex];
   if (dest != 0)
   {
      size_t i;
      const uint8_t *src = p->FileNames.data + 
         (p->FileNameOffsets[fileIndex] * 2);
      for (i = 0; i < len; i++)
         dest[i] = GetUi16(src + i * 2);
   }
   return len;
}

static SRes SzReadFileNames(const uint8_t *p,
      size_t size, uint32_t numFiles, size_t *sizes)
{
   uint32_t i;
   size_t pos = 0;
   for (i = 0; i < numFiles; i++)
   {
      sizes[i] = pos;
      for (;;)
      {
         if (pos >= size)
            return SZ_ERROR_ARCHIVE;
         if (p[pos * 2] == 0 && p[pos * 2 + 1] == 0)
            break;
         pos++;
      }
      pos++;
   }
   sizes[i] = pos;
   return (pos == size) ? SZ_OK : SZ_ERROR_ARCHIVE;
}

static SRes SzReadHeader2(
      CSzArEx *p,   /* allocMain */
      CSzData *sd,
      uint64_t **unpackSizes,  /* allocTemp */
      uint8_t **digestsDefined,    /* allocTemp */
      uint32_t **digests,         /* allocTemp */
      uint8_t **emptyStreamVector, /* allocTemp */
      uint8_t **emptyFileVector,   /* allocTemp */
      uint8_t **lwtVector,         /* allocTemp */
      ISzAlloc *allocMain,
      ISzAlloc *allocTemp)
{
   uint32_t i;
   uint64_t type;
   uint32_t numUnpackStreams = 0;
   uint32_t numFiles         = 0;
   CSzFileItem *files        = 0;
   uint32_t numEmptyStreams  = 0;
   SRes result               = SzReadID(sd, &type);

   if (result != 0)
      return result;

   if (type == k7zIdArchiveProperties)
   {
      result = SzReadArchiveProperties(sd);
      if (result != 0)
         return result;
      result = SzReadID(sd, &type);
      if (result != 0)
         return result;
   }


   if (type == k7zIdMainStreamsInfo)
   {
      result     = SzReadStreamsInfo(sd,
               &p->dataPos,
               &p->db,
               &numUnpackStreams,
               unpackSizes,
               digestsDefined,
               digests, allocMain, allocTemp);
      if (result != 0)
         return result;
      p->dataPos += p->startPosAfterHeader;
      result      = SzReadID(sd, &type);
      if (result != 0)
         return result;
   }

   if (type == k7zIdEnd)
      return SZ_OK;
   if (type != k7zIdFilesInfo)
      return SZ_ERROR_ARCHIVE;

   result         = SzReadNumber32(sd, &numFiles);
   if (result != 0)
      return result;
   p->db.NumFiles = numFiles;

   MY_ALLOC(CSzFileItem, files, (size_t)numFiles, allocMain);

   p->db.Files = files;
   for (i = 0; i < numFiles; i++)
      SzFile_Init(files + i);

   for (;;)
   {
      uint64_t size;
      SRes result    = SzReadID(sd, &type);

      if (result != 0)
         return result;
      if (type == k7zIdEnd)
         break;

      result = SzReadNumber(sd, &size);

      if (result != 0)
         return result;
      if (size > sd->Size)
         return SZ_ERROR_ARCHIVE;

      if ((uint64_t)(int)type != type)
      {
         SRes result = SzSkeepDataSize(sd, size);
         if (result != 0)
            return result;
      }
      else
         switch((int)type)
         {
            case k7zIdName:
               {
                  SRes result       = SzReadSwitch(sd);
                  size_t namesSize  = (size_t)size - 1;
                  if (result != 0)
                     return result;
                  if ((namesSize & 1) != 0)
                     return SZ_ERROR_ARCHIVE;
                  if (!Buf_Create(&p->FileNames, namesSize, allocMain))
                     return SZ_ERROR_MEM;
                  MY_ALLOC(size_t, p->FileNameOffsets, numFiles + 1, allocMain);
                  memcpy(p->FileNames.data, sd->Data, namesSize);
                  result = SzReadFileNames(sd->Data, namesSize >> 1, numFiles, p->FileNameOffsets);
                  if (result != 0)
                     return result;
                  result = SzSkeepDataSize(sd, namesSize);
                  if (result != 0)
                     return result;
                  break;
               }
            case k7zIdEmptyStream:
               {
                  SRes result    = SzReadBoolVector(sd, numFiles, emptyStreamVector, allocTemp);
                  if (result != 0)
                     return result;
                  numEmptyStreams = 0;
                  for (i = 0; i < numFiles; i++)
                     if ((*emptyStreamVector)[i])
                        numEmptyStreams++;
                  break;
               }
            case k7zIdEmptyFile:
               {
                  SRes result = SzReadBoolVector(sd, numEmptyStreams, emptyFileVector, allocTemp);
                  if (result != 0)
                     return result;
                  break;
               }
            case k7zIdWinAttributes:
               {
                  SRes result = SzReadBoolVector2(sd, numFiles, lwtVector, allocTemp);
                  if (result != 0)
                     return result;
                  result      = SzReadSwitch(sd);
                  if (result != 0)
                     return result;

                  for (i = 0; i < numFiles; i++)
                  {
                     CSzFileItem   *f = &files[i];
                     uint8_t  defined = (*lwtVector)[i];
                     f->AttribDefined = defined;
                     f->Attrib        = 0;
                     if (defined)
                     {
                        result = SzReaduint32_t(sd, &f->Attrib);
                        if (result != 0)
                           return result;
                     }
                  }
                  IAlloc_Free(allocTemp, *lwtVector);
                  *lwtVector = NULL;
                  break;
               }
            case k7zIdMTime:
               {
                  SRes result = SzReadBoolVector2(sd,
                           numFiles, lwtVector, allocTemp);
                  if (result != 0)
                     return result;
                  result      = SzReadSwitch(sd);
                  if (result != 0)
                     return result;

                  for (i = 0; i < numFiles; i++)
                  {
                     CSzFileItem  *f = &files[i];
                     uint8_t defined = (*lwtVector)[i];
                     f->MTimeDefined = defined;
                     f->MTime.Low    = f->MTime.High = 0;
                     if (defined)
                     {
                        result = SzReaduint32_t(sd, &f->MTime.Low);
                        if (result != 0)
                           return result;
                        result = SzReaduint32_t(sd, &f->MTime.High);
                        if (result != 0)
                           return result;
                     }
                  }
                  IAlloc_Free(allocTemp, *lwtVector);
                  *lwtVector = NULL;
                  break;
               }
            default:
               {
                  result = SzSkeepDataSize(sd, size);
                  if (result != 0)
                     return result;
               }
         }
   }

   {
      uint32_t emptyFileIndex = 0;
      uint32_t sizeIndex      = 0;
      for (i = 0; i < numFiles; i++)
      {
         CSzFileItem *file   = files + i;
         file->IsAnti        = 0;

         if (*emptyStreamVector == 0)
            file->HasStream  = 1;
         else
            file->HasStream  = (uint8_t)((*emptyStreamVector)[i] ? 0 : 1);

         if (file->HasStream)
         {
            file->IsDir      = 0;
            file->Size       = (*unpackSizes)[sizeIndex];
            file->Crc        = (*digests)[sizeIndex];
            file->CrcDefined = (uint8_t)(*digestsDefined)[sizeIndex];
            sizeIndex++;
         }
         else
         {
            if (*emptyFileVector == 0)
               file->IsDir = 1;
            else
               file->IsDir = (uint8_t)
                  ((*emptyFileVector)[emptyFileIndex] ? 0 : 1);
            emptyFileIndex++;
            file->Size       = 0;
            file->Crc        = 0;
            file->CrcDefined = 0;
         }
      }
   }
   return SzArEx_Fill(p, allocMain);
}

static SRes SzReadHeader(
      CSzArEx *p,
      CSzData *sd,
      ISzAlloc *allocMain,
      ISzAlloc *allocTemp)
{
   uint64_t *unpackSizes      = 0;
   uint8_t *digestsDefined    = 0;
   uint32_t *digests          = 0;
   uint8_t *emptyStreamVector = 0;
   uint8_t *emptyFileVector   = 0;
   uint8_t *lwtVector         = 0;
   SRes res                   = SzReadHeader2(p, sd,
         &unpackSizes, &digestsDefined, &digests,
         &emptyStreamVector, &emptyFileVector, &lwtVector,
         allocMain, allocTemp);
   IAlloc_Free(allocTemp, unpackSizes);
   IAlloc_Free(allocTemp, digestsDefined);
   IAlloc_Free(allocTemp, digests);
   IAlloc_Free(allocTemp, emptyStreamVector);
   IAlloc_Free(allocTemp, emptyFileVector);
   IAlloc_Free(allocTemp, lwtVector);
   return res;
}

static SRes SzReadAndDecodePackedStreams2(
      ILookInStream *inStream,
      CSzData *sd,
      CBuf *outBuffer,
      uint64_t baseOffset,
      CSzAr *p,
      uint64_t **unpackSizes,
      uint8_t **digestsDefined,
      uint32_t **digests,
      ISzAlloc *allocTemp)
{
   SRes res;
   CSzFolder *folder         = NULL;
   uint64_t dataStartPos     = 0;
   uint64_t unpackSize       = 0;
   uint32_t numUnpackStreams = 0;
   SRes result               = SzReadStreamsInfo(sd, &dataStartPos, p,
            &numUnpackStreams,  unpackSizes, digestsDefined, digests,
            allocTemp, allocTemp);

   if (result != 0)
      return result;

   dataStartPos += baseOffset;
   if (p->NumFolders != 1)
      return SZ_ERROR_ARCHIVE;

   folder     = p->Folders;
   unpackSize = SzFolder_GetUnpackSize(folder);

   result     = LookInStream_SeekTo(inStream, dataStartPos);

   if (result != 0)
      return result;

   if (!Buf_Create(outBuffer, (size_t)unpackSize, allocTemp))
      return SZ_ERROR_MEM;

   res = SzFolder_Decode(folder, p->PackSizes,
         inStream, dataStartPos,
         outBuffer->data, (size_t)unpackSize, allocTemp);
   if (res != 0)
      return res;
   if (folder->UnpackCRCDefined)
      if (CrcCalc(outBuffer->data, (size_t)unpackSize) != folder->UnpackCRC)
         return SZ_ERROR_CRC;
   return SZ_OK;
}

static SRes SzReadAndDecodePackedStreams(
      ILookInStream *inStream,
      CSzData *sd,
      CBuf *outBuffer,
      uint64_t baseOffset,
      ISzAlloc *allocTemp)
{
   CSzAr p;
   SRes res;
   uint64_t *unpackSizes   = 0;
   uint8_t *digestsDefined = 0;
   uint32_t *digests       = 0;

   SzAr_Init(&p);

   res = SzReadAndDecodePackedStreams2(inStream, sd, outBuffer, baseOffset,
         &p, &unpackSizes, &digestsDefined, &digests,
         allocTemp);

   SzAr_Free(&p, allocTemp);
   IAlloc_Free(allocTemp, unpackSizes);
   IAlloc_Free(allocTemp, digestsDefined);
   IAlloc_Free(allocTemp, digests);
   return res;
}

static SRes SzArEx_Open2(
      CSzArEx *p,
      ILookInStream *inStream,
      ISzAlloc *allocMain,
      ISzAlloc *allocTemp)
{
   SRes res;
   CBuf buffer;
   uint8_t header[k7zStartHeaderSize];
   uint64_t nextHeaderOffset, nextHeaderSize;
   size_t nextHeaderSizeT;
   uint32_t nextHeaderCRC;
   int64_t startArcPos  = 0;
   SRes result          = inStream->Seek(inStream, &startArcPos, SZ_SEEK_CUR);

   if (result != 0)
      return result;

   result               = LookInStream_Read2(inStream, header,
         k7zStartHeaderSize,
         SZ_ERROR_NO_ARCHIVE);

   if (result != 0)
      return result;

   if (!TestSignatureCandidate(header))
      return SZ_ERROR_NO_ARCHIVE;
   if (header[6] != k7zMajorVersion)
      return SZ_ERROR_UNSUPPORTED;

   nextHeaderOffset       = GetUi64(header + 12);
   nextHeaderSize         = GetUi64(header + 20);
   nextHeaderCRC          = GetUi32(header + 28);

   p->startPosAfterHeader = startArcPos + k7zStartHeaderSize;

   if (CrcCalc(header + 12, 20) != GetUi32(header + 8))
      return SZ_ERROR_CRC;

   nextHeaderSizeT = (size_t)nextHeaderSize;
   if (nextHeaderSizeT != nextHeaderSize)
      return SZ_ERROR_MEM;
   if (nextHeaderSizeT == 0)
      return SZ_OK;
   if (nextHeaderOffset > nextHeaderOffset + nextHeaderSize ||
         nextHeaderOffset > nextHeaderOffset + nextHeaderSize + k7zStartHeaderSize)
      return SZ_ERROR_NO_ARCHIVE;

   {
      int64_t pos  = 0;
      SRes result  = inStream->Seek(inStream, &pos, SZ_SEEK_END);

      if (result != 0)
         return result;
       
      if ((uint64_t)pos < startArcPos + nextHeaderOffset ||
            (uint64_t)pos < startArcPos + k7zStartHeaderSize + nextHeaderOffset ||
            (uint64_t)pos < startArcPos + k7zStartHeaderSize + nextHeaderOffset + nextHeaderSize)
         return SZ_ERROR_INPUT_EOF;
   }

   result = LookInStream_SeekTo(inStream, startArcPos + k7zStartHeaderSize + nextHeaderOffset);
   if (result != 0)
      return result;

   if (!Buf_Create(&buffer, nextHeaderSizeT, allocTemp))
      return SZ_ERROR_MEM;

   res = LookInStream_Read(inStream, buffer.data, nextHeaderSizeT);
   if (res == SZ_OK)
   {
      res = SZ_ERROR_ARCHIVE;
      if (CrcCalc(buffer.data, nextHeaderSizeT) == nextHeaderCRC)
      {
         CSzData sd;
         uint64_t type;
         sd.Data = buffer.data;
         sd.Size = buffer.size;
         res = SzReadID(&sd, &type);
         if (res == SZ_OK)
         {
            if (type == k7zIdEncodedHeader)
            {
               CBuf outBuffer;
               Buf_Init(&outBuffer);
               res = SzReadAndDecodePackedStreams(inStream, &sd, &outBuffer, p->startPosAfterHeader, allocTemp);
               if (res != SZ_OK)
                  Buf_Free(&outBuffer, allocTemp);
               else
               {
                  Buf_Free(&buffer, allocTemp);
                  buffer.data = outBuffer.data;
                  buffer.size = outBuffer.size;
                  sd.Data = buffer.data;
                  sd.Size = buffer.size;
                  res = SzReadID(&sd, &type);
               }
            }
         }
         if (res == SZ_OK)
         {
            if (type == k7zIdHeader)
               res = SzReadHeader(p, &sd, allocMain, allocTemp);
            else
               res = SZ_ERROR_UNSUPPORTED;
         }
      }
   }
   Buf_Free(&buffer, allocTemp);
   return res;
}

SRes SzArEx_Open(CSzArEx *p, ILookInStream *inStream,
      ISzAlloc *allocMain, ISzAlloc *allocTemp)
{
   SRes res = SzArEx_Open2(p, inStream, allocMain, allocTemp);
   if (res != SZ_OK)
      SzArEx_Free(p, allocMain);
   return res;
}

SRes SzArEx_Extract(
      const CSzArEx *p,
      ILookInStream *inStream,
      uint32_t fileIndex,
      uint32_t *blockIndex,
      uint8_t **outBuffer,
      size_t *outBufferSize,
      size_t *offset,
      size_t *outSizeProcessed,
      ISzAlloc *allocMain,
      ISzAlloc *allocTemp)
{
   uint32_t folderIndex = p->FileIndexToFolderIndexMap[fileIndex];
   SRes res             = SZ_OK;
   *offset              = 0;
   *outSizeProcessed    = 0;

   if (folderIndex == (uint32_t)-1)
   {
      IAlloc_Free(allocMain, *outBuffer);
      *blockIndex    = folderIndex;
      *outBuffer     = 0;
      *outBufferSize = 0;
      return SZ_OK;
   }

   if (*outBuffer == 0 || *blockIndex != folderIndex)
   {
      SRes result;
      CSzFolder *folder       = p->db.Folders + folderIndex;
      uint64_t unpackSizeSpec = SzFolder_GetUnpackSize(folder);
      size_t unpackSize       = (size_t)unpackSizeSpec;
      uint64_t startOffset    = SzArEx_GetFolderStreamPos(p, folderIndex, 0);

      if (unpackSize != unpackSizeSpec)
         return SZ_ERROR_MEM;
      *blockIndex = folderIndex;
      IAlloc_Free(allocMain, *outBuffer);
      *outBuffer = 0;
      result     = LookInStream_SeekTo(inStream, startOffset);
      if (result != 0)
         return result;

      if (res == SZ_OK)
      {
         *outBufferSize = unpackSize;
         if (unpackSize != 0)
         {
            *outBuffer = (uint8_t *)IAlloc_Alloc(allocMain, unpackSize);
            if (*outBuffer == 0)
               res = SZ_ERROR_MEM;
         }

         if (res == SZ_OK)
         {
            res = SzFolder_Decode(folder,
                  p->db.PackSizes + p->FolderStartPackStreamIndex[folderIndex],
                  inStream, startOffset,
                  *outBuffer, unpackSize, allocTemp);
            if (res == SZ_OK)
            {
               if (folder->UnpackCRCDefined)
               {
                  if (CrcCalc(*outBuffer, unpackSize) != folder->UnpackCRC)
                     res = SZ_ERROR_CRC;
               }
            }
         }
      }
   }

   if (res == SZ_OK)
   {
      uint32_t i;
      CSzFileItem *fileItem = p->db.Files + fileIndex;
      *offset = 0;
      for (i = p->FolderStartFileIndex[folderIndex]; i < fileIndex; i++)
         *offset += (uint32_t)p->db.Files[i].Size;
      *outSizeProcessed = (size_t)fileItem->Size;
      if (*offset + *outSizeProcessed > *outBufferSize)
         return SZ_ERROR_FAIL;
      if (fileItem->CrcDefined && CrcCalc(*outBuffer + *offset, *outSizeProcessed) != fileItem->Crc)
         res = SZ_ERROR_CRC;
   }
   return res;
}
