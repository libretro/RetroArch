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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <libretro_vfs.h>
#include <vfs/vfs_implementation.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>
#include <memmap.h>
#include <retro_miscellaneous.h>
#include <encodings/utf.h>

static const int64_t vfs_error_return_value      = -1;

retro_vfs_file_get_path_t filestream_get_path_cb = NULL;
retro_vfs_file_open_t filestream_open_cb         = NULL;
retro_vfs_file_close_t filestream_close_cb       = NULL;
retro_vfs_file_size_t filestream_size_cb         = NULL;
retro_vfs_file_tell_t filestream_tell_cb         = NULL;
retro_vfs_file_seek_t filestream_seek_cb         = NULL;
retro_vfs_file_read_t filestream_read_cb         = NULL;
retro_vfs_file_write_t filestream_write_cb       = NULL;
retro_vfs_file_flush_t filestream_flush_cb       = NULL;
retro_vfs_file_delete_t filestream_delete_cb     = NULL;

struct RFILE
{
   struct retro_vfs_file_handle *hfile;
	bool error_flag;
};

/* VFS Initialization */

void filestream_vfs_init(const struct retro_vfs_interface_info* vfs_info)
{
	const struct retro_vfs_interface* vfs_iface;

	filestream_get_path_cb = NULL;
	filestream_open_cb     = NULL;
	filestream_close_cb    = NULL;
	filestream_tell_cb     = NULL;
	filestream_size_cb     = NULL;
	filestream_seek_cb     = NULL;
	filestream_read_cb     = NULL;
	filestream_write_cb    = NULL;
	filestream_flush_cb    = NULL;
	filestream_delete_cb   = NULL;

	vfs_iface              = vfs_info->iface;

	if (vfs_info->required_interface_version < 
         FILESTREAM_REQUIRED_VFS_VERSION || !vfs_iface)
		return;

	filestream_get_path_cb = vfs_iface->file_get_path;
	filestream_open_cb     = vfs_iface->file_open;
	filestream_close_cb    = vfs_iface->file_close;
	filestream_size_cb     = vfs_iface->file_size;
	filestream_tell_cb     = vfs_iface->file_tell;
	filestream_seek_cb     = vfs_iface->file_seek;
	filestream_read_cb     = vfs_iface->file_read;
	filestream_write_cb    = vfs_iface->file_write;
	filestream_flush_cb    = vfs_iface->file_flush;
	filestream_delete_cb   = vfs_iface->file_delete;
}

/* Callback wrappers */
int64_t filestream_get_size(RFILE *stream)
{
   int64_t output;

   if (filestream_size_cb != NULL)
      output = filestream_size_cb(stream->hfile);
   else
      output = retro_vfs_file_size_impl(stream->hfile);

   if (output == vfs_error_return_value)
      stream->error_flag = true;

   return output;
}

/**
 * filestream_open:
 * @path               : path to file
 * @mode               : file mode to use when opening (read/write)
 * @hints              :
 *
 * Opens a file for reading or writing, depending on the requested mode.
 * Returns a pointer to an RFILE if opened successfully, otherwise NULL.
 **/
RFILE *filestream_open(const char *path, unsigned mode, unsigned hints)
{
   struct retro_vfs_file_handle  *fp = NULL;
	RFILE* output                     = NULL;

	if (filestream_open_cb != NULL)
		fp = (struct retro_vfs_file_handle*)
         filestream_open_cb(path, mode, hints);
	else
		fp = (struct retro_vfs_file_handle*)
         retro_vfs_file_open_impl(path, mode, hints);

	if (!fp)
		return NULL;

	output             = (RFILE*)malloc(sizeof(RFILE));
	output->error_flag = false;
	output->hfile      = fp;
	return output;
}

char *filestream_gets(RFILE *stream, char *s, size_t len)
{
   int c   = 0;
   char *p = NULL;
   if (!stream)
      return NULL;

   /* get max bytes or up to a newline */

   for (p = s, len--; len > 0; len--)
   {
      if ((c = filestream_getc(stream)) == EOF)
         break;
      *p++ = c;
      if (c == '\n')
         break;
   }
   *p = 0;

   if (p == s || c == EOF)
      return NULL;
   return (p);
}

int filestream_getc(RFILE *stream)
{
   char c = 0;
   if (!stream)
      return 0;
   if(filestream_read(stream, &c, 1) == 1)
      return (int)c;
   return EOF;
}

ssize_t filestream_seek(RFILE *stream, ssize_t offset, int whence)
{
   int64_t output;

   if (filestream_seek_cb != NULL)
      output = filestream_seek_cb(stream->hfile, offset, whence);
   else
      output = retro_vfs_file_seek_impl(stream->hfile, offset, whence);

   if (output == vfs_error_return_value)
      stream->error_flag = true;

   return output;
}

int filestream_eof(RFILE *stream)
{
   int64_t current_position = filestream_tell(stream);
   int64_t end_position     = filestream_get_size(stream);

   if (current_position >= end_position)
      return 1;
   return 0;
}


ssize_t filestream_tell(RFILE *stream)
{
   ssize_t output;

   if (filestream_size_cb != NULL)
      output = filestream_tell_cb(stream->hfile);
   else
      output = retro_vfs_file_tell_impl(stream->hfile);

   if (output == vfs_error_return_value)
      stream->error_flag = true;

   return output;
}

void filestream_rewind(RFILE *stream)
{
   if (!stream)
      return;
   filestream_seek(stream, 0L, SEEK_SET);
   stream->error_flag = false;
}

ssize_t filestream_read(RFILE *stream, void *s, size_t len)
{
   int64_t output;

   if (filestream_read_cb != NULL)
      output = filestream_read_cb(stream->hfile, s, len);
   else
      output = retro_vfs_file_read_impl(stream->hfile, s, len);

   if (output == vfs_error_return_value)
      stream->error_flag = true;

   return output;
}

int filestream_flush(RFILE *stream)
{
   int output;

   if (filestream_flush_cb != NULL)
      output = filestream_flush_cb(stream->hfile);
   else
      output = retro_vfs_file_flush_impl(stream->hfile);

   if (output == vfs_error_return_value)
      stream->error_flag = true;

   return output;
}

int filestream_delete(const char *path)
{
   if (filestream_delete_cb != NULL)
      return filestream_delete_cb(path);

   return retro_vfs_file_delete_impl(path);
}

const char *filestream_get_path(RFILE *stream)
{
   if (filestream_get_path_cb != NULL)
      return filestream_get_path_cb(stream->hfile);

   return retro_vfs_file_get_path_impl(stream->hfile);
}

ssize_t filestream_write(RFILE *stream, const void *s, size_t len)
{
   int64_t output;

   if (filestream_write_cb != NULL)
      output = filestream_write_cb(stream->hfile, s, len);
   else
      output = retro_vfs_file_write_impl(stream->hfile, s, len);

   if (output == vfs_error_return_value)
      stream->error_flag = true;

   return output;
}

/* Hack function */
int retro_vfs_file_putc(void *data, int c);

int filestream_putc(RFILE *stream, int c)
{
   return retro_vfs_file_putc(stream->hfile, c);
}

int filestream_vprintf(RFILE *stream, const char* format, va_list args)
{
	static char buffer[8 * 1024];
	int num_chars = vsprintf(buffer, format, args);

	if (num_chars < 0)
		return -1;
	else if (num_chars == 0)
		return 0;

	return filestream_write(stream, buffer, num_chars);
}

int filestream_printf(RFILE *stream, const char* format, ...)
{
	va_list vl;
   int result;
	va_start(vl, format);
	result = filestream_vprintf(stream, format, vl);
	va_end(vl);
	return result;
}

int filestream_error(RFILE *stream)
{
   if (stream && stream->error_flag)
      return 1;
   return 0;
}

int filestream_close(RFILE *stream)
{
   int output;
   void *fp = stream->hfile;

   if (filestream_close_cb != NULL)
      output = filestream_close_cb(fp);
   else
      output = retro_vfs_file_close_impl(fp);

   if (output == 0)
      free(stream);

   return output;
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
int filestream_read_file(const char *path, void **buf, ssize_t *len)
{
   ssize_t ret              = 0;
   int64_t content_buf_size = 0;
   void *content_buf        = NULL;
   RFILE *file              = filestream_open(path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
   {
      fprintf(stderr, "Failed to open %s: %s\n", path, strerror(errno));
      goto error;
   }

   content_buf_size = filestream_get_size(file);

   if (content_buf_size < 0)
      goto error;

   content_buf = malloc(content_buf_size + 1);

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
bool filestream_write_file(const char *path, const void *data, ssize_t size)
{
   ssize_t ret   = 0;
   RFILE *file   = filestream_open(path,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!file)
      return false;

   ret = filestream_write(file, data, size);
   filestream_close(file);

   if (ret != size)
      return false;

   return true;
}
