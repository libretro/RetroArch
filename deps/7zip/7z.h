/* 7z.h -- 7z interface
2010-03-11 : Igor Pavlov : Public domain */

#ifndef __7Z_H
#define __7Z_H

#include "7zBuf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define k7zStartHeaderSize 0x20
#define k7zSignatureSize 6
extern uint8_t k7zSignature[k7zSignatureSize];
#define k7zMajorVersion 0

enum EIdEnum
{
  k7zIdEnd,
  k7zIdHeader,
  k7zIdArchiveProperties,
  k7zIdAdditionalStreamsInfo,
  k7zIdMainStreamsInfo,
  k7zIdFilesInfo,
  k7zIdPackInfo,
  k7zIdUnpackInfo,
  k7zIdSubStreamsInfo,
  k7zIdSize,
  k7zIdCRC,
  k7zIdFolder,
  k7zIdCodersUnpackSize,
  k7zIdNumUnpackStream,
  k7zIdEmptyStream,
  k7zIdEmptyFile,
  k7zIdAnti,
  k7zIdName,
  k7zIdCTime,
  k7zIdATime,
  k7zIdMTime,
  k7zIdWinAttributes,
  k7zIdComment,
  k7zIdEncodedHeader,
  k7zIdStartPos,
  k7zIdDummy
};

typedef struct
{
  uint32_t NumInStreams;
  uint32_t NumOutStreams;
  uint64_t MethodID;
  CBuf Props;
} CSzCoderInfo;

void SzCoderInfo_Init(CSzCoderInfo *p);
void SzCoderInfo_Free(CSzCoderInfo *p, ISzAlloc *alloc);

typedef struct
{
  uint32_t InIndex;
  uint32_t OutIndex;
} CSzBindPair;

typedef struct
{
  CSzCoderInfo *Coders;
  CSzBindPair *BindPairs;
  uint32_t *PackStreams;
  uint64_t *UnpackSizes;
  uint32_t NumCoders;
  uint32_t NumBindPairs;
  uint32_t NumPackStreams;
  int UnpackCRCDefined;
  uint32_t UnpackCRC;

  uint32_t NumUnpackStreams;
} CSzFolder;

void SzFolder_Init(CSzFolder *p);
uint64_t SzFolder_GetUnpackSize(CSzFolder *p);
int SzFolder_FindBindPairForInStream(CSzFolder *p, uint32_t inStreamIndex);
uint32_t SzFolder_GetNumOutStreams(CSzFolder *p);
uint64_t SzFolder_GetUnpackSize(CSzFolder *p);

SRes SzFolder_Decode(const CSzFolder *folder, const uint64_t *packSizes,
    ILookInStream *stream, uint64_t startPos,
    uint8_t *outBuffer, size_t outSize, ISzAlloc *allocMain);

typedef struct
{
  uint32_t Low;
  uint32_t High;
} CNtfsFileTime;

typedef struct
{
  CNtfsFileTime MTime;
  uint64_t Size;
  uint32_t Crc;
  uint32_t Attrib;
  uint8_t HasStream;
  uint8_t IsDir;
  uint8_t IsAnti;
  uint8_t CrcDefined;
  uint8_t MTimeDefined;
  uint8_t AttribDefined;
} CSzFileItem;

void SzFile_Init(CSzFileItem *p);

typedef struct
{
  uint64_t *PackSizes;
  uint8_t *PackCRCsDefined;
  uint32_t *PackCRCs;
  CSzFolder *Folders;
  CSzFileItem *Files;
  uint32_t NumPackStreams;
  uint32_t NumFolders;
  uint32_t NumFiles;
} CSzAr;

void SzAr_Init(CSzAr *p);
void SzAr_Free(CSzAr *p, ISzAlloc *alloc);


/*
  SzExtract extracts file from archive

  *outBuffer must be 0 before first call for each new archive.

  Extracting cache:
    If you need to decompress more than one file, you can send
    these values from previous call:
      *blockIndex,
      *outBuffer,
      *outBufferSize
    You can consider "*outBuffer" as cache of solid block. If your archive is solid,
    it will increase decompression speed.

    If you use external function, you can declare these 3 cache variables
    (blockIndex, outBuffer, outBufferSize) as static in that external function.

    Free *outBuffer and set *outBuffer to 0, if you want to flush cache.
*/

typedef struct
{
  CSzAr db;

  uint64_t startPosAfterHeader;
  uint64_t dataPos;

  uint32_t *FolderStartPackStreamIndex;
  uint64_t *PackStreamStartPositions;
  uint32_t *FolderStartFileIndex;
  uint32_t *FileIndexToFolderIndexMap;

  size_t *FileNameOffsets; /* in 2-byte steps */
  CBuf FileNames;  /* UTF-16-LE */
} CSzArEx;

void SzArEx_Init(CSzArEx *p);
void SzArEx_Free(CSzArEx *p, ISzAlloc *alloc);
uint64_t SzArEx_GetFolderStreamPos(const CSzArEx *p, uint32_t folderIndex, uint32_t indexInFolder);
int SzArEx_GetFolderFullPackSize(const CSzArEx *p, uint32_t folderIndex, uint64_t *resSize);

/*
if dest == NULL, the return value specifies the required size of the buffer,
  in 16-bit characters, including the null-terminating character.
if dest != NULL, the return value specifies the number of 16-bit characters that
  are written to the dest, including the null-terminating character. */

size_t SzArEx_GetFileNameUtf16(const CSzArEx *p, size_t fileIndex, uint16_t *dest);

SRes SzArEx_Extract(
    const CSzArEx *db,
    ILookInStream *inStream,
    uint32_t fileIndex,         /* index of file */
    uint32_t *blockIndex,       /* index of solid block */
    uint8_t **outBuffer,         /* pointer to pointer to output buffer (allocated with allocMain) */
    size_t *outBufferSize,    /* buffer size for output buffer */
    size_t *offset,           /* offset of stream for required file in *outBuffer */
    size_t *outSizeProcessed, /* size of file in *outBuffer */
    ISzAlloc *allocMain,
    ISzAlloc *allocTemp);


/*
SzArEx_Open Errors:
SZ_ERROR_NO_ARCHIVE
SZ_ERROR_ARCHIVE
SZ_ERROR_UNSUPPORTED
SZ_ERROR_MEM
SZ_ERROR_CRC
SZ_ERROR_INPUT_EOF
SZ_ERROR_FAIL
*/

SRes SzArEx_Open(CSzArEx *p, ILookInStream *inStream, ISzAlloc *allocMain, ISzAlloc *allocTemp);

#ifdef __cplusplus
}
#endif

#endif
