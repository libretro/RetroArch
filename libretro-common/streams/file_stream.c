/* Copyright  (C) 2010-2020 The RetroArch team
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
#include <stdarg.h>
#include <ctype.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _MSC_VER
#include <compat/msvc.h>
#endif

#include <string/stdstring.h>
#include <streams/file_stream.h>
#define VFS_FRONTEND
#include <vfs/vfs_implementation.h>

#define VFS_ERROR_RETURN_VALUE -1

struct RFILE
{
   struct retro_vfs_file_handle *hfile;
	bool error_flag;
};

static retro_vfs_get_path_t filestream_get_path_cb = NULL;
static retro_vfs_open_t filestream_open_cb         = NULL;
static retro_vfs_close_t filestream_close_cb       = NULL;
static retro_vfs_size_t filestream_size_cb         = NULL;
static retro_vfs_truncate_t filestream_truncate_cb = NULL;
static retro_vfs_tell_t filestream_tell_cb         = NULL;
static retro_vfs_seek_t filestream_seek_cb         = NULL;
static retro_vfs_read_t filestream_read_cb         = NULL;
static retro_vfs_write_t filestream_write_cb       = NULL;
static retro_vfs_flush_t filestream_flush_cb       = NULL;
static retro_vfs_remove_t filestream_remove_cb     = NULL;
static retro_vfs_rename_t filestream_rename_cb     = NULL;

/* VFS Initialization */

void filestream_vfs_init(const struct retro_vfs_interface_info* vfs_info)
{
   const struct retro_vfs_interface *
      vfs_iface           = vfs_info->iface;

   filestream_get_path_cb = NULL;
   filestream_open_cb     = NULL;
   filestream_close_cb    = NULL;
   filestream_tell_cb     = NULL;
   filestream_size_cb     = NULL;
   filestream_truncate_cb = NULL;
   filestream_seek_cb     = NULL;
   filestream_read_cb     = NULL;
   filestream_write_cb    = NULL;
   filestream_flush_cb    = NULL;
   filestream_remove_cb   = NULL;
   filestream_rename_cb   = NULL;

   if (
             (vfs_info->required_interface_version <
             FILESTREAM_REQUIRED_VFS_VERSION)
         || !vfs_iface)
      return;

   filestream_get_path_cb = vfs_iface->get_path;
   filestream_open_cb     = vfs_iface->open;
   filestream_close_cb    = vfs_iface->close;
   filestream_size_cb     = vfs_iface->size;
   filestream_truncate_cb = vfs_iface->truncate;
   filestream_tell_cb     = vfs_iface->tell;
   filestream_seek_cb     = vfs_iface->seek;
   filestream_read_cb     = vfs_iface->read;
   filestream_write_cb    = vfs_iface->write;
   filestream_flush_cb    = vfs_iface->flush;
   filestream_remove_cb   = vfs_iface->remove;
   filestream_rename_cb   = vfs_iface->rename;
}

/* Callback wrappers */
bool filestream_exists(const char *path)
{
   RFILE *dummy           = NULL;

   if (!path || !*path)
      return false;
   if (!(dummy = filestream_open(
         path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE)))
      return false;

   if (filestream_close(dummy) != 0)
      free(dummy);

   dummy = NULL;
   return true;
}

int64_t filestream_get_size(RFILE *stream)
{
   int64_t output;

   if (filestream_size_cb)
      output = filestream_size_cb(stream->hfile);
   else
      output = retro_vfs_file_size_impl(
            (libretro_vfs_implementation_file*)stream->hfile);

   if (output == VFS_ERROR_RETURN_VALUE)
      stream->error_flag = true;

   return output;
}

int64_t filestream_truncate(RFILE *stream, int64_t length)
{
   int64_t output;

   if (filestream_truncate_cb)
      output = filestream_truncate_cb(stream->hfile, length);
   else
      output = retro_vfs_file_truncate_impl(
            (libretro_vfs_implementation_file*)stream->hfile, length);

   if (output == VFS_ERROR_RETURN_VALUE)
      stream->error_flag = true;

   return output;
}

RFILE* filestream_open(const char *path, unsigned mode, unsigned hints)
{
   struct retro_vfs_file_handle  *fp = NULL;
   RFILE* output                     = (RFILE*)malloc(sizeof(RFILE));

   if (!output)
      return NULL;

   if (filestream_open_cb)
      fp = (struct retro_vfs_file_handle*)
         filestream_open_cb(path, mode, hints);
   else
      fp = (struct retro_vfs_file_handle*)
         retro_vfs_file_open_impl(path, mode, hints);

   if (!fp)
   {
      free(output);
      return NULL;
   }

   output->error_flag = false;
   output->hfile      = fp;
   return output;
}

char* filestream_gets(RFILE *stream, char *s, size_t len)
{
   int c   = 0;
   char *p = s;
   if (!stream)
      return NULL;

   /* get max bytes or up to a newline */

   for (len--; len > 0; len--)
   {
      if ((c = filestream_getc(stream)) == EOF)
         break;
      *p++ = c;
      if (c == '\n')
         break;
   }
   *p = 0;

   if (p == s && c == EOF)
      return NULL;
   return (s);
}

int filestream_getc(RFILE *stream)
{
   char c = 0;
   if (stream && filestream_read(stream, &c, 1) == 1)
      return (int)(unsigned char)c;
   return EOF;
}

int filestream_vscanf(RFILE *stream, const char* format, va_list *args)
{
   char buf[4096];
   char subfmt[64];
   va_list args_copy;
   const char *bufiter  = buf;
   int        ret       = 0;
   int64_t startpos     = filestream_tell(stream);
   int64_t maxlen       = filestream_read(stream, buf, sizeof(buf)-1);

   if (maxlen <= 0)
      return EOF;

   buf[maxlen] = '\0';

   /* Have to copy the input va_list here
    * > Calling va_arg() on 'args' directly would
    *   cause the va_list to have an indeterminate value
    *   in the function calling filestream_vscanf(),
    *   leading to unexpected behaviour */
#ifdef __va_copy
   __va_copy(args_copy, *args);
#else
   va_copy(args_copy, *args);
#endif

   while (*format)
   {
      if (*format == '%')
      {
         int sublen;
         char* subfmtiter = subfmt;
         bool asterisk    = false;

         *subfmtiter++    = *format++; /* '%' */

         /* %[*][width][length]specifier */

         if (*format == '*')
         {
            asterisk      = true;
            *subfmtiter++ = *format++;
         }

         while (ISDIGIT((unsigned char)*format))
            *subfmtiter++ = *format++; /* width */

         /* length */
         if (*format == 'h' || *format == 'l')
         {
            if (format[1] == format[0])
               *subfmtiter++ = *format++;
            *subfmtiter++    = *format++;
         }
         else if (
               *format == 'j' ||
               *format == 'z' ||
               *format == 't' ||
               *format == 'L')
         {
            *subfmtiter++ = *format++;
         }

         /* specifier - always a single character (except ]) */
         if (*format == '[')
         {
            while (*format != ']')
               *subfmtiter++ = *format++;
            *subfmtiter++    = *format++;
         }
         else
            *subfmtiter++    = *format++;

         *subfmtiter++       = '%';
         *subfmtiter++       = 'n';
         *subfmtiter++       = '\0';

         if (sizeof(void*) != sizeof(long*))
            abort(); /* all pointers must have the same size */

         if (asterisk)
         {
            int v = sscanf(bufiter, subfmt, &sublen);
            if (v == EOF)
               return EOF;
            if (v != 0)
               break;
         }
         else
         {
            int v = sscanf(bufiter, subfmt, va_arg(args_copy, void*), &sublen);
            if (v == EOF)
               return EOF;
            if (v != 1)
               break;
         }

         ret++;
         bufiter += sublen;
      }
      else if (isspace((unsigned char)*format))
      {
         while (isspace((unsigned char)*bufiter))
            bufiter++;
         format++;
      }
      else
      {
         if (*bufiter != *format)
            break;
         bufiter++;
         format++;
      }
   }

   va_end(args_copy);
   filestream_seek(stream, startpos + (bufiter - buf),
         RETRO_VFS_SEEK_POSITION_START);

   return ret;
}

int filestream_scanf(RFILE *stream, const char* format, ...)
{
   int result;
   va_list vl;
   va_start(vl, format);
   result = filestream_vscanf(stream, format, &vl);
   va_end(vl);
   return result;
}

int64_t filestream_seek(RFILE *stream, int64_t offset, int seek_position)
{
   int64_t output;

   if (filestream_seek_cb)
      output = filestream_seek_cb(stream->hfile, offset, seek_position);
   else
      output = retro_vfs_file_seek_impl(
            (libretro_vfs_implementation_file*)stream->hfile,
            offset, seek_position);

   if (output == VFS_ERROR_RETURN_VALUE)
      stream->error_flag = true;

   return output;
}

int filestream_eof(RFILE *stream)
{
   return filestream_tell(stream) == filestream_get_size(stream) ? EOF : 0;
}

int64_t filestream_tell(RFILE *stream)
{
   int64_t output;

   if (filestream_size_cb)
      output = filestream_tell_cb(stream->hfile);
   else
      output = retro_vfs_file_tell_impl(
            (libretro_vfs_implementation_file*)stream->hfile);

   if (output == VFS_ERROR_RETURN_VALUE)
      stream->error_flag = true;

   return output;
}

void filestream_rewind(RFILE *stream)
{
   if (!stream)
      return;
   filestream_seek(stream, 0L, RETRO_VFS_SEEK_POSITION_START);
   stream->error_flag = false;
}

int64_t filestream_read(RFILE *stream, void *s, int64_t len)
{
   int64_t output;

   if (filestream_read_cb)
      output = filestream_read_cb(stream->hfile, s, len);
   else
      output = retro_vfs_file_read_impl(
            (libretro_vfs_implementation_file*)stream->hfile, s, len);

   if (output == VFS_ERROR_RETURN_VALUE)
      stream->error_flag = true;

   return output;
}

int filestream_flush(RFILE *stream)
{
   int output;

   if (filestream_flush_cb)
      output = filestream_flush_cb(stream->hfile);
   else
      output = retro_vfs_file_flush_impl(
            (libretro_vfs_implementation_file*)stream->hfile);

   if (output == VFS_ERROR_RETURN_VALUE)
      stream->error_flag = true;

   return output;
}

int filestream_delete(const char *path)
{
   if (filestream_remove_cb)
      return filestream_remove_cb(path);

   return retro_vfs_file_remove_impl(path);
}

int filestream_rename(const char *old_path, const char *new_path)
{
   if (filestream_rename_cb)
      return filestream_rename_cb(old_path, new_path);

   return retro_vfs_file_rename_impl(old_path, new_path);
}

const char* filestream_get_path(RFILE *stream)
{
   if (filestream_get_path_cb)
      return filestream_get_path_cb(stream->hfile);

   return retro_vfs_file_get_path_impl(
         (libretro_vfs_implementation_file*)stream->hfile);
}

int64_t filestream_write(RFILE *stream, const void *s, int64_t len)
{
   int64_t output;

   if (filestream_write_cb)
      output = filestream_write_cb(stream->hfile, s, len);
   else
      output = retro_vfs_file_write_impl(
            (libretro_vfs_implementation_file*)stream->hfile, s, len);

   if (output == VFS_ERROR_RETURN_VALUE)
      stream->error_flag = true;

   return output;
}

int filestream_putc(RFILE *stream, int c)
{
   char c_char = (char)c;
   if (!stream)
      return EOF;
   return filestream_write(stream, &c_char, 1) == 1
      ? (int)(unsigned char)c
      : EOF;
}

int filestream_vprintf(RFILE *stream, const char* format, va_list args)
{
   static char buffer[8 * 1024];
   int64_t num_chars = vsnprintf(buffer, sizeof(buffer),
         format, args);

   if (num_chars < 0)
      return -1;
   else if (num_chars == 0)
      return 0;

   return (int)filestream_write(stream, buffer, num_chars);
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
   return (stream && stream->error_flag);
}

int filestream_close(RFILE *stream)
{
   int output;
   struct retro_vfs_file_handle* fp = stream->hfile;

   if (filestream_close_cb)
      output = filestream_close_cb(fp);
   else
      output = retro_vfs_file_close_impl(
            (libretro_vfs_implementation_file*)fp);

   if (output == 0)
      free(stream);

   return output;
}

int64_t filestream_read_file(const char *path, void **buf, int64_t *len)
{
   int64_t ret              = 0;
   int64_t content_buf_size = 0;
   void *content_buf        = NULL;
   RFILE *file              = filestream_open(path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
   {
      *buf = NULL;
      return 0;
   }

   if ((content_buf_size = filestream_get_size(file)) < 0)
      goto error;

   if (!(content_buf = malloc((size_t)(content_buf_size + 1))))
      goto error;
   if ((int64_t)(uint64_t)(content_buf_size + 1) != (content_buf_size + 1))
      goto error;

   if ((ret = filestream_read(file, content_buf, (int64_t)content_buf_size)) <
         0)
      goto error;

   if (filestream_close(file) != 0)
      free(file);

   *buf    = content_buf;

   /* Allow for easy reading of strings to be safe.
    * Will only work with sane character formatting (Unix). */
   ((char*)content_buf)[ret] = '\0';

   if (len)
      *len = ret;

   return 1;

error:
   if (filestream_close(file) != 0)
      free(file);
   if (content_buf)
      free(content_buf);
   if (len)
      *len = -1;
   *buf = NULL;
   return 0;
}

bool filestream_write_file(const char *path, const void *data, int64_t size)
{
   int64_t ret   = 0;
   RFILE *file   = filestream_open(path,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!file)
      return false;
   ret = filestream_write(file, data, size);
   if (filestream_close(file) != 0)
      free(file);
   return (ret == size);
}

char *filestream_getline(RFILE *stream)
{
   char *newline_tmp  = NULL;
   size_t cur_size    = 8;
   size_t idx         = 0;
   int in             = 0;
   char *newline      = (char*)malloc(9);

   if (!stream || !newline)
   {
      if (newline)
         free(newline);
      return NULL;
   }

   in = filestream_getc(stream);

   while (in != EOF && in != '\n')
   {
      if (idx == cur_size)
      {
         cur_size    *= 2;

         if (!(newline_tmp = (char*)realloc(newline, cur_size + 1)))
         {
            free(newline);
            return NULL;
         }

         newline     = newline_tmp;
      }

      newline[idx++] = in;
      in             = filestream_getc(stream);
   }

   newline[idx]      = '\0';
   return newline;
}

libretro_vfs_implementation_file* filestream_get_vfs_handle(RFILE *stream)
{
   return (libretro_vfs_implementation_file*)stream->hfile;
}
