/* Copyright  (C) 2010-2017 The RetroArch team
*
* ---------------------------------------------------------------------------------------
* The following license statement only applies to this file (file_stream.c).
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

#include <vfs/vfs_implementation.h>

#if defined(_WIN32)
#  ifdef _MSC_VER
#    define setmode _setmode
#  endif
#  ifdef _XBOX
#    include <xtl.h>
#    define INVALID_FILE_ATTRIBUTES -1
#  else
#    include <io.h>
#    include <fcntl.h>
#    include <direct.h>
#    include <windows.h>
#  endif
#else
#  if defined(PSP)
#    include <pspiofilemgr.h>
#  endif
#  include <sys/types.h>
#  include <sys/stat.h>
#  if !defined(VITA)
#  include <dirent.h>
#  endif
#  include <unistd.h>
#endif

#ifdef __CELLOS_LV2__
#include <cell/cell_fs.h>
#define O_RDONLY CELL_FS_O_RDONLY
#define O_WRONLY CELL_FS_O_WRONLY
#define O_CREAT CELL_FS_O_CREAT
#define O_TRUNC CELL_FS_O_TRUNC
#define O_RDWR CELL_FS_O_RDWR
#else
#include <fcntl.h>
#endif

#include <memmap.h>
#include <retro_miscellaneous.h>

const unsigned HINTS_ACCESS_MASK = 0xff;

enum libretro_file_hints
{
	RFILE_HINT_MMAP = 1 << 8,
	RFILE_HINT_UNBUFFERED = 1 << 9
};

#ifdef VFS_FRONTEND
struct retro_vfs_file_handle
#else
struct libretro_vfs_implementation_file
#endif
{
	uint64_t hints;
	char *path;
	long long int size;
#if defined(PSP)
	SceUID fd;
#else

#define HAVE_BUFFERED_IO 1

#define MODE_STR_READ_UNBUF "rb"
#define MODE_STR_WRITE_UNBUF "wb"
#define MODE_STR_WRITE_PLUS "w+"

#if defined(HAVE_BUFFERED_IO)
	FILE *fp;
#endif
#if defined(HAVE_MMAP)
	uint8_t *mapped;
	uint64_t mappos;
	uint64_t mapsize;
#endif
	int fd;
#endif
};

int64_t retro_vfs_file_seek_internal(libretro_vfs_implementation_file *stream, int64_t offset, int whence);

libretro_vfs_implementation_file *retro_vfs_file_open_impl(const char *path, uint64_t mode)
{
	int            flags = 0;
	int         mode_int = 0;
#if defined(HAVE_BUFFERED_IO)
	const char *mode_str = NULL;
#endif
	libretro_vfs_implementation_file *stream = (libretro_vfs_implementation_file*)calloc(1, sizeof(*stream));

	if (!stream)
		return NULL;

	(void)mode_int;
	(void)flags;

	stream->hints = mode;

#ifdef HAVE_MMAP
	if (stream->hints & RFILE_HINT_MMAP && (stream->hints & HINTS_ACCESS_MASK) == RFILE_MODE_READ)
		stream->hints |= RFILE_HINT_UNBUFFERED;
	else
#endif
		stream->hints &= ~RFILE_HINT_MMAP;

	switch (mode & HINTS_ACCESS_MASK)
	{
	case RETRO_VFS_FILE_ACCESS_READ:
#if  defined(PSP)
		mode_int = 0666;
		flags = PSP_O_RDONLY;
#else
#if defined(HAVE_BUFFERED_IO)
		if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
			mode_str = MODE_STR_READ_UNBUF;
#endif
		/* No "else" here */
		flags = O_RDONLY;
#endif
		break;
	case RETRO_VFS_FILE_ACCESS_WRITE:
#if  defined(PSP)
		mode_int = 0666;
		flags = PSP_O_CREAT | PSP_O_WRONLY | PSP_O_TRUNC;
#else
#if defined(HAVE_BUFFERED_IO)
		if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
			mode_str = MODE_STR_WRITE_UNBUF;
#endif
		else
		{
			flags = O_WRONLY | O_CREAT | O_TRUNC;
#ifndef _WIN32
			flags |= S_IRUSR | S_IWUSR;
#endif
		}
#endif
		break;
	case RETRO_VFS_FILE_ACCESS_READ_WRITE:
#if  defined(PSP)
		mode_int = 0666;
		flags = PSP_O_RDWR;
#else
#if defined(HAVE_BUFFERED_IO)
		if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
			mode_str = MODE_STR_WRITE_PLUS;
#endif
		else
		{
			flags = O_RDWR;
#ifdef _WIN32
			flags |= O_BINARY;
#endif
		}
#endif
		break;
	}

#if  defined(PSP)
	stream->fd = sceIoOpen(path, flags, mode_int);
#else
#if defined(HAVE_BUFFERED_IO)
	if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0 && mode_str)
	{
		stream->fp = fopen(path, mode_str);
		if (!stream->fp)
			goto error;
	}
	else
#endif
	{
		/* FIXME: HAVE_BUFFERED_IO is always 1, but if it is ever changed, open() needs to be changed to _wopen() for WIndows. */
		stream->fd = open(path, flags, mode_int);
		if (stream->fd == -1)
			goto error;
#ifdef HAVE_MMAP
		if (stream->hints & RFILE_HINT_MMAP)
		{
			stream->mappos = 0;
			stream->mapped = NULL;
			stream->mapsize = filestream_size(stream);

			if (stream->mapsize == (uint64_t)-1)
				goto error;

			filestream_rewind(stream);

			stream->mapped = (uint8_t*)mmap((void*)0,
				stream->mapsize, PROT_READ, MAP_SHARED, stream->fd, 0);

			if (stream->mapped == MAP_FAILED)
				stream->hints &= ~RFILE_HINT_MMAP;
		}
#endif
	}
#endif

#if  defined(PSP)
	if (stream->fd == -1)
		goto error;
#endif

	{
		stream->path = strdup(path);
	}

	fseek(stream->fp, 0, SEEK_END);
	stream->size = ftell(stream->fp);
	fseek(stream->fp, 0, SEEK_SET);

	return stream;

error:
	retro_vfs_file_close_impl(stream);
	return NULL;
}

int retro_vfs_file_close_impl(libretro_vfs_implementation_file *stream)
{
	if (!stream)
		goto error;

#if  defined(PSP)
	if (stream->fd > 0)
		sceIoClose(stream->fd);
#else
#if defined(HAVE_BUFFERED_IO)
	if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
	{
		if (stream->fp)
			fclose(stream->fp);
	}
	else
#endif
#ifdef HAVE_MMAP
		if (stream->hints & RFILE_HINT_MMAP)
			munmap(stream->mapped, stream->mapsize);
#endif

	if (stream->fd > 0)
		close(stream->fd);
#endif
	free(stream);

	return 0;

error:
	return -1;
}

int retro_vfs_file_error_impl(libretro_vfs_implementation_file *stream)
{
#if defined(HAVE_BUFFERED_IO)
	return ferror(stream->fp);
#else
	/* stub */
	return 0;
#endif
}

int64_t retro_vfs_file_size_impl(libretro_vfs_implementation_file *stream)
{
	int64_t initial_pos = retro_vfs_file_tell_impl(stream);

	int64_t ret = retro_vfs_file_seek_internal(stream, 0, SEEK_END);
	if (ret == -1)
		return -1;

	int64_t output = retro_vfs_file_tell_impl(stream);
	ret = retro_vfs_file_seek_internal(stream, initial_pos, SEEK_SET);
	if (ret == -1)
		return -1;
	
	return output;
}

int64_t retro_vfs_file_tell_impl(libretro_vfs_implementation_file *stream)
{
	if (!stream)
		goto error;
#if  defined(PSP)
	if (sceIoLseek(stream->fd, 0, SEEK_CUR) < 0)
		goto error;
#else
#if defined(HAVE_BUFFERED_IO)
	if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
		return ftell(stream->fp);
#endif
#ifdef HAVE_MMAP
	/* Need to check stream->mapped because this function
	* is called in filestream_open() */
	if (stream->mapped && stream->hints & RFILE_HINT_MMAP)
		return stream->mappos;
#endif
	if (lseek(stream->fd, 0, SEEK_CUR) < 0)
		goto error;
#endif

	return 0;

error:
	return -1;
}

int64_t retro_vfs_file_seek_impl(libretro_vfs_implementation_file *stream, int64_t offset)
{
	return retro_vfs_file_seek_internal(stream, offset, SEEK_SET);
}

int64_t retro_vfs_file_read_impl(libretro_vfs_implementation_file *stream, void *s, uint64_t len)
{
	if (!stream || !s)
		goto error;
#if  defined(PSP)
	return sceIoRead(stream->fd, s, len);
#else
#if defined(HAVE_BUFFERED_IO)
	if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
		return fread(s, 1, len, stream->fp);
#endif
#ifdef HAVE_MMAP
	if (stream->hints & RFILE_HINT_MMAP)
	{
		if (stream->mappos > stream->mapsize)
			goto error;

		if (stream->mappos + len > stream->mapsize)
			len = stream->mapsize - stream->mappos;

		memcpy(s, &stream->mapped[stream->mappos], len);
		stream->mappos += len;

		return len;
	}
#endif
	return read(stream->fd, s, len);
#endif

error:
	return -1;
}

int64_t retro_vfs_file_write_impl(libretro_vfs_implementation_file *stream, const void *s, uint64_t len)
{
	if (!stream)
		goto error;
#if  defined(PSP)
	return sceIoWrite(stream->fd, s, len);
#else
#if defined(HAVE_BUFFERED_IO)
	if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
		return fwrite(s, 1, len, stream->fp);
#endif
#ifdef HAVE_MMAP
	if (stream->hints & RFILE_HINT_MMAP)
		goto error;
#endif
	return write(stream->fd, s, len);
#endif

error:
	return -1;
}

int retro_vfs_file_flush_impl(libretro_vfs_implementation_file *stream)
{
#if defined(HAVE_BUFFERED_IO)
	if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
		return fflush(stream->fp);
#endif

	return 0;
}

/* No idea how it is supposed to work on PSP/PS3 etc. Should not be affected by buffered IO */
int retro_vfs_file_delete_impl(const char *path)
{
	return remove(path) == 0;
}

const char *retro_vfs_file_get_path_impl(libretro_vfs_implementation_file *stream)
{
	if (!stream)
		return NULL;
	return stream->path;
}

int64_t retro_vfs_file_seek_internal(libretro_vfs_implementation_file *stream, int64_t offset, int whence)
{
	if (!stream)
		goto error;

#if  defined(PSP)
	if (sceIoLseek(stream->fd, (SceOff)offset, whence) == -1)
		goto error;
#else

#if defined(HAVE_BUFFERED_IO)
	if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
		return fseek(stream->fp, (long)offset, whence);
#endif

#ifdef HAVE_MMAP
	/* Need to check stream->mapped because this function is
	* called in filestream_open() */
	if (stream->mapped && stream->hints & RFILE_HINT_MMAP)
	{
		/* fseek() returns error on under/overflow but allows cursor > EOF for
		read-only file descriptors. */
		switch (whence)
		{
		case SEEK_SET:
			if (offset < 0)
				goto error;

			stream->mappos = offset;
			break;

		case SEEK_CUR:
			if ((offset < 0 && stream->mappos + offset > stream->mappos) ||
				(offset > 0 && stream->mappos + offset < stream->mappos))
				goto error;

			stream->mappos += offset;
			break;

		case SEEK_END:
			if (stream->mapsize + offset < stream->mapsize)
				goto error;

			stream->mappos = stream->mapsize + offset;
			break;
		}
		return stream->mappos;
	}
#endif

	if (lseek(stream->fd, offset, whence) < 0)
		goto error;

#endif

	return 0;

error:
	return -1;
}