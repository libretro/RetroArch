/* Copyright  (C) 2010-2017 The RetroArch team
*
* ---------------------------------------------------------------------------------------
* The following license statement only applies to this file (vfs_implementation.c).
* ---------------------------------------------------------------------------------------
*
* Permission is hereby granted, free of charge,
* to any person obtaining a copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation the rights to
* use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
* and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vfs/vfs_implementation.h>
#include <string/stdstring.h>
#include <memmap.h>
#include <retro_miscellaneous.h>
#include <encodings/utf.h>

#ifdef VFS_FRONTEND
struct retro_vfs_file_handle
#else
struct libretro_vfs_implementation_file
#endif
{
   void *empty;
};

int64_t retro_vfs_file_seek_internal(libretro_vfs_implementation_file *stream, int64_t offset, int whence)
{
   return -1;
}

libretro_vfs_implementation_file *retro_vfs_file_open_impl(const char *path, uint64_t mode)
{
   libretro_vfs_implementation_file *stream = (libretro_vfs_implementation_file*)calloc(1, sizeof(*stream));

   if (!stream)
      return NULL;

   return stream;
}

int retro_vfs_file_close_impl(libretro_vfs_implementation_file *stream)
{
	return -1;
}

int retro_vfs_file_error_impl(libretro_vfs_implementation_file *stream)
{
   return 0;
}

int64_t retro_vfs_file_size_impl(libretro_vfs_implementation_file *stream)
{
   return -1;
}

int64_t retro_vfs_file_tell_impl(libretro_vfs_implementation_file *stream)
{
	return -1;
}

int64_t retro_vfs_file_seek_impl(libretro_vfs_implementation_file *stream, int64_t offset)
{
	return retro_vfs_file_seek_internal(stream, offset, SEEK_SET);
}

int64_t retro_vfs_file_read_impl(libretro_vfs_implementation_file *stream, void *s, uint64_t len)
{
	return -1;
}

int64_t retro_vfs_file_write_impl(libretro_vfs_implementation_file *stream, const void *s, uint64_t len)
{
	return -1;
}

int retro_vfs_file_flush_impl(libretro_vfs_implementation_file *stream)
{
	return 0;
}

int retro_vfs_file_delete_impl(const char *path)
{
   return 0;
}

const char *retro_vfs_file_get_path_impl(libretro_vfs_implementation_file *stream)
{
   return NULL;
}

