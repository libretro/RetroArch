/* ioapi.c -- IO base function header for compress/uncompress .zip
   files using zlib + zip or unzip API

   Version 1.01e, February 12th, 2005

   Copyright (C) 1998-2005 Gilles Vollant
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../miniz/zlib.h"
#include "ioapi.h"



/* I've found an old Unix (a SunOS 4.1.3_U1) without all SEEK_* defined.... */

#ifndef SEEK_CUR
#define SEEK_CUR    1
#endif

#ifndef SEEK_END
#define SEEK_END    2
#endif

#ifndef SEEK_SET
#define SEEK_SET    0
#endif

voidpf fopen_file_func (
   voidpf opaque,
   const char* filename,
   int mode);

uLong fread_file_func (voidpf opaque, voidpf stream, void* buf, uLong size);
uLong fwrite_file_func (voidpf opaque, voidpf stream, const void* buf, uLong size);
long ftell_file_func (voidpf opaque, voidpf stream);
long fseek_file_func (voidpf opaque, voidpf stream, uLong offset, int origin);
int fclose_file_func (voidpf opaque, voidpf stream);
int ferror_file_func (voidpf opaque, voidpf stream);

voidpf fopen_file_func (voidpf opaque, const char *filename, int mode)
{
    FILE* file = NULL;
    const char* mode_fopen = NULL;
    if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER)==ZLIB_FILEFUNC_MODE_READ)
        mode_fopen = "rb";
    else
    if (mode & ZLIB_FILEFUNC_MODE_EXISTING)
        mode_fopen = "r+b";
    else
    if (mode & ZLIB_FILEFUNC_MODE_CREATE)
        mode_fopen = "wb";

    if ((filename!=NULL) && (mode_fopen != NULL))
        file = fopen(filename, mode_fopen);
    return file;
}


uLong fread_file_func (voidpf opaque, voidpf stream, void *buf, uLong size)
{
    uLong ret;
    ret = (uLong)fread(buf, 1, (size_t)size, (FILE *)stream);
    return ret;
}


uLong fwrite_file_func (voidpf opaque, voidpf stream, const void *buf, uLong size)
{
    uLong ret;
    ret = (uLong)fwrite(buf, 1, (size_t)size, (FILE *)stream);
    return ret;
}

long ftell_file_func (voidpf opaque, voidpf stream)
{
    long ret;
    ret = ftell((FILE *)stream);
    return ret;
}

long fseek_file_func (voidpf opaque, voidpf stream, uLong offset, int origin)
{
    int fseek_origin=0;
    long ret;
    switch (origin)
    {
    case ZLIB_FILEFUNC_SEEK_CUR :
        fseek_origin = SEEK_CUR;
        break;
    case ZLIB_FILEFUNC_SEEK_END :
        fseek_origin = SEEK_END;
        break;
    case ZLIB_FILEFUNC_SEEK_SET :
        fseek_origin = SEEK_SET;
        break;
    default: return -1;
    }
    ret = 0;
    fseek((FILE *)stream, offset, fseek_origin);
    return ret;
}

int fclose_file_func (voidpf opaque, voidpf stream)
{
    int ret;
    ret = fclose((FILE *)stream);
    return ret;
}

int ferror_file_func (voidpf opaque, voidpf stream)
{
    int ret;
    ret = ferror((FILE *)stream);
    return ret;
}

void fill_fopen_filefunc (zlib_filefunc_def*pzlib_filefunc_def)
{
    pzlib_filefunc_def->zopen_file = fopen_file_func;
    pzlib_filefunc_def->zread_file = fread_file_func;
    pzlib_filefunc_def->zwrite_file = fwrite_file_func;
    pzlib_filefunc_def->ztell_file = ftell_file_func;
    pzlib_filefunc_def->zseek_file = fseek_file_func;
    pzlib_filefunc_def->zclose_file = fclose_file_func;
    pzlib_filefunc_def->zerror_file = ferror_file_func;
    pzlib_filefunc_def->opaque = NULL;
}
