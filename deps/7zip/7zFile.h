/* 7zFile.h -- File IO
2009-11-24 : Igor Pavlov : Public domain */

#ifndef __7Z_FILE_H
#define __7Z_FILE_H

#include <stdio.h>

#include "7zTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- File ---------- */

typedef struct
{
  void *file;
} CSzFile;

void File_Construct(CSzFile *p);
WRes InFile_Open(CSzFile *p, const char *name);
WRes OutFile_Open(CSzFile *p, const char *name);
WRes File_Close(CSzFile *p);

/* reads max(*size, remain file's size) bytes */
WRes File_Read(CSzFile *p, void *data, size_t *size);

/* writes *size bytes */
WRes File_Write(CSzFile *p, const void *data, size_t *size);

WRes File_Seek(CSzFile *p, int64_t *pos, ESzSeek origin);
WRes File_GetLength(CSzFile *p, uint64_t *length);


/* ---------- FileInStream ---------- */

typedef struct
{
  ISeqInStream s;
  CSzFile file;
} CFileSeqInStream;

void FileSeqInStream_CreateVTable(CFileSeqInStream *p);


typedef struct
{
  ISeekInStream s;
  CSzFile file;
} CFileInStream;

void FileInStream_CreateVTable(CFileInStream *p);


typedef struct
{
  ISeqOutStream s;
  CSzFile file;
} CFileOutStream;

void FileOutStream_CreateVTable(CFileOutStream *p);

#ifdef __cplusplus
}
#endif

#endif
