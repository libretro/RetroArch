/* 7zFile.c -- File IO
   2009-11-24 : Igor Pavlov : Public domain */

#include <stdint.h>
#define SKIP_STDIO_REDEFINES
#include <streams/file_stream_transforms.h>
#include "7zFile.h"

#ifndef UNDER_CE
#include <errno.h>
#endif

void File_Construct(CSzFile *p)
{
   p->file = NULL;
}

static WRes File_Open(CSzFile *p, const char *name, int writeMode)
{
   p->file = rfopen(name, writeMode ? "wb+" : "rb");
   if (!p->file)
   {
#ifdef UNDER_CE
      return 2; /* ENOENT */
#else
      return errno;
#endif
   }

   return 0;
}

WRes InFile_Open(CSzFile *p, const char *name)
{
   return File_Open(p, name, 0);
}

WRes OutFile_Open(CSzFile *p, const char *name)
{
   return File_Open(p, name, 1);
}

WRes File_Close(CSzFile *p)
{
   if (p->file)
   {
      int res = rfclose((RFILE*)p->file);
      if (res != 0)
         return res;
      p->file = NULL;
   }
   return 0;
}

WRes File_Read(CSzFile *p, void *data, size_t *size)
{
   int64_t originalSize = *size;
   if (originalSize == 0)
      return 0;

   *size = rfread(data, 1, originalSize, (RFILE*)p->file);
   if (*size == originalSize)
      return 0;
   return rferror((RFILE*)p->file);
}

WRes File_Write(CSzFile *p, const void *data, size_t *size)
{
   int64_t originalSize = *size;
   if (originalSize == 0)
      return 0;

   *size = rfwrite(data, 1, originalSize, (RFILE*)p->file);
   if (*size == originalSize)
      return 0;
   return rferror((RFILE*)p->file);
}

WRes File_Seek(CSzFile *p, int64_t *pos, ESzSeek origin)
{
   int whence;
   int64_t res;
   switch (origin)
   {
      case SZ_SEEK_SET:
         whence = SEEK_SET;
         break;
      case SZ_SEEK_CUR:
         whence = SEEK_CUR;
         break;
      case SZ_SEEK_END:
         whence = SEEK_END;
         break;
      default:
         return 1;
   }
   res  = rfseek((RFILE*)p->file, (int64_t)*pos, whence);
   *pos = rftell((RFILE*)p->file);
   return res;
}

WRes File_GetLength(CSzFile *p, uint64_t *length)
{
   int64_t pos  = rftell((RFILE*)p->file);
   int64_t res  = rfseek((RFILE*)p->file, 0, SEEK_END);
   *length      = rftell((RFILE*)p->file);
   rfseek((RFILE*)p->file, pos, SEEK_SET);
   return res;
}


/* ---------- FileSeqInStream ---------- */

static SRes FileSeqInStream_Read(void *pp, void *buf, size_t *size)
{
   CFileSeqInStream *p = (CFileSeqInStream *)pp;
   return File_Read(&p->file, buf, size) == 0 ? SZ_OK : SZ_ERROR_READ;
}

void FileSeqInStream_CreateVTable(CFileSeqInStream *p)
{
   p->s.Read = FileSeqInStream_Read;
}

/* ---------- FileInStream ---------- */

static SRes FileInStream_Read(void *pp, void *buf, size_t *size)
{
   CFileInStream *p = (CFileInStream *)pp;
   return (File_Read(&p->file, buf, size) == 0) ? SZ_OK : SZ_ERROR_READ;
}

static SRes FileInStream_Seek(void *pp, int64_t *pos, ESzSeek origin)
{
   CFileInStream *p = (CFileInStream *)pp;
   return File_Seek(&p->file, pos, origin);
}

void FileInStream_CreateVTable(CFileInStream *p)
{
   p->s.Read = FileInStream_Read;
   p->s.Seek = FileInStream_Seek;
}


/* ---------- FileOutStream ---------- */

static size_t FileOutStream_Write(void *pp, const void *data, size_t size)
{
   CFileOutStream *p = (CFileOutStream *)pp;
   File_Write(&p->file, data, &size);
   return size;
}

void FileOutStream_CreateVTable(CFileOutStream *p)
{
   p->s.Write = FileOutStream_Write;
}

#undef SKIP_STDIO_REDEFINES
