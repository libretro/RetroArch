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

#include <streams/file_stream.h>
#include <vfs/vfs_implementation.h>

struct RFILE
{
	struct retro_vfs_file_handle *hfile;
	bool error_flag;
};

static const int64_t vfs_eror_return_value = -1;

/* Callbacks */

retro_vfs_file_get_path_t filestream_get_path_cb = NULL;
retro_vfs_file_open_t filestream_open_cb = NULL;
retro_vfs_file_close_t filestream_close_cb = NULL;
retro_vfs_file_size_t filestream_size_cb = NULL;
retro_vfs_file_tell_t filestream_tell_cb = NULL;
retro_vfs_file_seek_t filestream_seek_cb = NULL;
retro_vfs_file_read_t filestream_read_cb = NULL;
retro_vfs_file_write_t filestream_write_cb = NULL;
retro_vfs_file_flush_t filestream_flush_cb = NULL;
retro_vfs_file_delete_t filestream_delete_cb = NULL;

/* VFS Initialization */

void filestream_vfs_init(const struct retro_vfs_interface_info* vfs_info)
{
	filestream_get_path_cb = NULL;
	filestream_open_cb = NULL;
	filestream_close_cb = NULL;
	filestream_tell_cb = NULL;
	filestream_size_cb = NULL;
	filestream_seek_cb = NULL;
	filestream_read_cb = NULL;
	filestream_write_cb = NULL;
	filestream_flush_cb = NULL;
	filestream_delete_cb = NULL;

	const struct retro_vfs_interface* vfs_iface = vfs_info->iface;
	if (vfs_info->required_interface_version < FILESTREAM_REQUIRED_VFS_VERSION || vfs_iface == NULL)
	{
		return;
	}

	filestream_get_path_cb = vfs_iface->file_get_path;
	filestream_open_cb = vfs_iface->file_open;
	filestream_close_cb = vfs_iface->file_close;
	filestream_size_cb = vfs_iface->file_size;
	filestream_tell_cb = vfs_iface->file_tell;
	filestream_seek_cb = vfs_iface->file_seek;
	filestream_read_cb = vfs_iface->file_read;
	filestream_write_cb = vfs_iface->file_write;
	filestream_flush_cb = vfs_iface->file_flush;
	filestream_delete_cb = vfs_iface->file_delete;
}

/* Callback wrappers */

RFILE *filestream_open(const char *path, uint64_t flags)
{
	struct retro_vfs_file_handle* fp;

	if (filestream_open_cb != NULL)
		fp = filestream_open_cb(path, flags);
	else
		fp = (struct retro_vfs_file_handle*)retro_vfs_file_open_impl(path, flags);

	if (fp == NULL)
		return NULL;

	RFILE* output = malloc(sizeof(RFILE));
	output->error_flag = false;
	output->hfile = fp;
	return output;
}

int filestream_close(RFILE *stream)
{
	int output;
	struct retro_vfs_file_handle* fp = stream->hfile;

	if (filestream_close_cb != NULL)
		output = filestream_close_cb(fp);
	else
		output = retro_vfs_file_close_impl((libretro_vfs_implementation_file*)fp);

	if (output == 0)
		free(stream);

	return output;
}

int filestream_error(RFILE *stream)
{
	if (stream->error_flag)
		return 1;

	return 0;
}

int64_t filestream_size(RFILE *stream)
{
	int64_t output;

	if (filestream_size_cb != NULL)
		output = filestream_size_cb(stream->hfile);
	else
		output = retro_vfs_file_size_impl((libretro_vfs_implementation_file*)stream->hfile);

	if (output == vfs_eror_return_value)
		stream->error_flag = true;

	return output;
}

int64_t filestream_tell(RFILE *stream)
{
	int64_t output;

	if (filestream_tell_cb != NULL)
		output = filestream_tell_cb(stream->hfile);
	else
		output = retro_vfs_file_tell_impl((libretro_vfs_implementation_file*)stream->hfile);

	if (output == vfs_eror_return_value)
		stream->error_flag = true;

	return output;
}

int64_t filestream_seek(RFILE *stream, int64_t offset)
{
	int64_t output;

	if (filestream_seek_cb != NULL)
		output = filestream_seek_cb(stream->hfile, offset);
	else
		output = retro_vfs_file_seek_impl((libretro_vfs_implementation_file*)stream->hfile, offset);

	if (output == vfs_eror_return_value)
		stream->error_flag = true;

	return output;
}

int64_t filestream_read(RFILE *stream, void *s, uint64_t len)
{
	int64_t output;

	if (filestream_read_cb != NULL)
		output = filestream_read_cb(stream->hfile, s, len);
	else
		output = retro_vfs_file_read_impl((libretro_vfs_implementation_file*)stream->hfile, s, len);

	if (output == vfs_eror_return_value)
		stream->error_flag = true;

	return output;
}

int64_t filestream_write(RFILE *stream, const void *s, uint64_t len)
{
	int64_t output;

	if (filestream_write_cb != NULL)
		output = filestream_write_cb(stream->hfile, s, len);
	else
		output = retro_vfs_file_write_impl((libretro_vfs_implementation_file*)stream->hfile, s, len);

	if (output == vfs_eror_return_value)
		stream->error_flag = true;

	return output;
}

int filestream_flush(RFILE *stream)
{
	int output;

	if (filestream_flush_cb != NULL)
		output = filestream_flush_cb(stream->hfile);
	else
		output = retro_vfs_file_flush_impl((libretro_vfs_implementation_file*)stream->hfile);

	if (output == vfs_eror_return_value)
		stream->error_flag = true;

	return output;
}

int filestream_delete(const char *path)
{
	if (filestream_delete_cb != NULL)
	{
		return filestream_delete_cb(path);
	}

	return retro_vfs_file_delete_impl(path);
}

const char *filestream_get_path(RFILE *stream)
{
	if (filestream_get_path_cb != NULL)
	{
		return filestream_get_path_cb(stream->hfile);
	}

	return retro_vfs_file_get_path_impl((libretro_vfs_implementation_file*)stream->hfile);
}

/* Wrapper-based Implementations */

const char *filestream_get_ext(RFILE *stream)
{
	const char* path;
	const char* output;

	path = filestream_get_path(stream);
	output = strrchr(path, '.');
	return output;
}

int filestream_eof(RFILE *stream)
{
	int64_t current_position = filestream_tell(stream);
	int64_t end_position = filestream_size(stream);

	if (current_position >= end_position)
		return 1;
	return 0;
}

void filestream_rewind(RFILE *stream)
{
	filestream_seek(stream, 0);
	stream->error_flag = false;
}

char *filestream_getline(RFILE *stream)
{
   char* newline     = (char*)malloc(9);
   char* newline_tmp = NULL;
   size_t cur_size   = 8;
   size_t idx        = 0;
   int in            = filestream_getc(stream);

   if (!newline)
      return NULL;

   while (in != EOF && in != '\n')
   {
      if (idx == cur_size)
      {
         cur_size *= 2;
         newline_tmp = (char*)realloc(newline, cur_size + 1);

         if (!newline_tmp)
         {
            free(newline);
            return NULL;
         }

         newline = newline_tmp;
      }

      newline[idx++] = in;
      in             = filestream_getc(stream);
   }

   newline[idx] = '\0';
   return newline; 
}

char *filestream_gets(RFILE *stream, char *s, uint64_t len)
{
   if (!stream)
      return NULL;

   if(filestream_read(stream,s,len)==len)
      return s;
   return NULL;
}

int filestream_getc(RFILE *stream)
{
   char c = 0;
   (void)c;

   if (!stream->hfile)
      return 0;

    if(filestream_read(stream, &c, 1) == 1)
       return (int)c;
    return EOF;
}

int filestream_putc(RFILE *stream, int c)
{
   if (!stream)
      return EOF;

#if defined(HAVE_BUFFERED_IO)
   return fputc(c, stream->fp);
#else
   /* unimplemented */
   return EOF;
#endif
}

uint64_t filestream_vprintf(RFILE *stream, const char* format, va_list args)
{
	static char buffer[8 * 1024];
	uint64_t numChars = vsprintf(buffer, format, args);

	if (numChars < 0)
		return -1;
	else if (numChars == 0)
		return 0;

	return filestream_write(stream, buffer, numChars);
}

uint64_t filestream_printf(RFILE *stream, const char* format, ...)
{
	uint64_t result;
	va_list vl;
	va_start(vl, format);
	result = filestream_vprintf(stream, format, vl);
	va_end(vl);
	return result;
}

/**
 * filestream_read_file:
 * @path             : path to file.
 * @buf              : buffer to allocate and read the contents of the
 *                     file into. Needs to be freed manually.
 *
 * Read the contents of a file into @buf.
 *
 * Returns: number of items read, -1 on error.
 */
int filestream_read_file(const char *path, void **buf, uint64_t *len)
{
   int64_t ret              = 0;
   int64_t content_buf_size = 0;
   void *content_buf        = NULL;
   RFILE *file              = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ);

   if (!file)
   {
      fprintf(stderr, "Failed to open %s: %s\n", path, strerror(errno));
      goto error;
   }

   int64_t size = filestream_size(file);
   if (filestream_seek(file, size) != 0)
      goto error;

   content_buf_size = filestream_tell(file);
   if (content_buf_size < 0)
      goto error;

   filestream_rewind(file);

   content_buf = malloc((size_t)content_buf_size + 1);

   if (!content_buf)
      goto error;

   ret = filestream_read(file, content_buf, content_buf_size);
   if (ret < 0)
   {
      fprintf(stderr, "Failed to read %s: %s\n", path, strerror(errno));
      goto error;
   }

   filestream_close(file);

   *buf    = content_buf;

   /* Allow for easy reading of strings to be safe.
    * Will only work with sane character formatting (Unix). */
   ((char*)content_buf)[ret] = '\0';

   if (len)
      *len = ret;

   return 1;

error:
   if (file)
      filestream_close(file);
   if (content_buf)
      free(content_buf);
   if (len)
      *len = -1;
   *buf = NULL;
   return 0;
}

/**
 * filestream_write_file:
 * @path             : path to file.
 * @data             : contents to write to the file.
 * @size             : size of the contents.
 *
 * Writes data to a file.
 *
 * Returns: true (1) on success, false (0) otherwise.
 */
bool filestream_write_file(const char *path, const void *data, uint64_t size)
{
   int64_t ret   = 0;
   RFILE *file   = filestream_open(path, RETRO_VFS_FILE_ACCESS_WRITE);
   if (!file)
      return false;

   ret = filestream_write(file, data, size);
   filestream_close(file);

   if (ret != size)
      return false;

   return true;
}
