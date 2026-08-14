/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (config_file_io.c).
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

/* File access for config_file.
 *
 * The parser core (config_file.c) performs no file I/O: it consumes
 * buffers, the way the codecs under formats/ do, and reaches files
 * only through the config_file_io interface declared in
 * config_file.h.  This translation unit is where the file system
 * actually enters the picture: the filestream/VFS-backed
 * implementation of that interface, the path-based constructors
 * built on it, and config_file_write - everything that opens a file
 * by name lives here, so a build that links only config_file.c gets
 * a pure in-memory parser with no streams/ dependency.
 *
 * Every constructor here self-registers the filestream
 * implementation as the io default (idempotently, never displacing
 * a host-installed interface), so any binary that loads at least
 * one config by path resolves '#include' directives exactly as
 * before this split. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <compat/fopen_utf8.h>
#include <file/config_file.h>
#include <streams/file_stream.h>

static char *config_file_io_fs_read_file(const char *path,
      int64_t *len, void *ud)
{
   void *buf = NULL;
   /* No path_is_valid() first: filestream_read_file() opens the file
    * itself and returns 0 when it cannot, so a preceding stat only
    * repeats the lookup the open already does - and no caller can see
    * its result, since a missing file and an unreadable one both land
    * on the same NULL return.  Dropping it also closes the window
    * between the stat and the open in which the file could change. */
   if (!filestream_read_file(path, &buf, len))
      return NULL;
   /* filestream_read_file NUL-terminates past *len, which is the
    * buffer contract the parser relies on. */
   return (char*)buf;
}

static void config_file_io_fs_free_file(char *buf, void *ud)
{
   free(buf);
}

const config_file_io_t *config_file_io_filestream(void)
{
   static const config_file_io_t config_file_io_fs = {
      config_file_io_fs_read_file,
      config_file_io_fs_free_file,
      NULL
   };
   return &config_file_io_fs;
}

/* Idempotent: never displaces an interface a host installed. */
static void config_file_io_ensure_default(void)
{
   if (!config_file_get_io_default())
      config_file_set_io_default(config_file_io_filestream());
}

/**
 * config_file_new_with_callback:
 *
 * Loads a config file.
 * If @path is NULL, will create an empty config file.
 * Includes cb callbacks  to run custom code during config file processing.
 *
 * @return Returns NULL if file doesn't exist.
 **/
config_file_t *config_file_new_with_callback(
      const char *path, config_file_cb_t *cb)
{
   int ret                  = 0;
   struct config_file *conf = config_file_new_alloc();
   if (!path || !*path)
      return conf;
   config_file_io_ensure_default();
   if ((ret = config_file_load_file(conf, path, cb)) == -1)
   {
      config_file_free(conf);
      return NULL;
   }
   else if (ret == 1)
   {
      free(conf);
      return NULL;
   }
   return conf;
}

/**
 * config_file_new:
 *
 * Loads a config file.
 * If @path is NULL, will create an empty config file.
 *
 * @return Returns NULL if file doesn't exist.
 **/
config_file_t *config_file_new(const char *path)
{
   return config_file_new_with_callback(path, NULL);
}

config_file_t *config_file_new_from_path_to_string(const char *path)
{
   /* Historically this read the file and re-parsed it through
    * config_file_new_from_string; since the line loops were
    * unified the two path constructors have identical behaviour
    * (same parse, same include handling, path set before parsing,
    * no callback), so route through config_file_new - which parses
    * in borrow mode: the conf adopts the file buffer and entries
    * point straight into it, skipping the per-entry key/value
    * allocations and copies the from_string detour forced. */
   return config_file_new(path);
}

/**
 * config_append_file:
 *
 * Loads a new config, and appends its data to @conf.
 * The key-value pairs of the new config file takes priority over the old.
 **/
bool config_append_file(config_file_t *conf, const char *path)
{
   config_file_t *new_conf = config_file_new_from_path_to_string(path);
   if (!new_conf)
      return false;
   return config_file_append_conf(conf, new_conf);
}

/**
 * config_file_write:
 *
 * Write the current config to a file.
 **/
bool config_file_write(config_file_t *conf, const char *path, bool sort)
{
   if (!conf)
      return false;
   if (conf->flags & CONF_FILE_FLG_MODIFIED)
   {
      if (!path || !*path)
         config_file_dump(conf, stdout, sort);
      else
      {
         /* The stdio buffer is heap, not a local.  At 16 KiB it was
          * twice the whole thread stack on the smallest target -
          * GEKKO threads get 8 KiB, see STACKSIZE in
          * rthreads/gx_pthread.h - and this is reached from a task
          * handler, so it runs on the task thread rather than the
          * main one whenever the queue is threaded:
          * input_autoconfigure_connect_handler ->
          * ..._scan_config_files_external -> ..._index_write ->
          * here, which is the path a gamepad being plugged in takes.
          * -fstack-usage put the frame at 16432 bytes.
          *
          * The buffer is the C library's, asked for by passing NULL
          * with a size: it then allocates, owns and releases it with
          * the stream, so there is nothing here to keep alive across
          * the writes or to free afterwards.  A library that declines
          * leaves the stream on its own default, which is slower for
          * small writes rather than fatal - the same outcome the
          * allocation failing used to have.  retro_vfs_file_open_impl()
          * asks the same way, and for a stronger reason there: on
          * Apple, a buffer supplied from outside clears __SMBF and
          * disqualifies fread()'s large-read fast path for the life of
          * the stream. */
         FILE *file = (FILE*)fopen_utf8(path, "wb");
         if (!file)
            return false;
         setvbuf(file, NULL, _IOFBF, 0x4000);
         config_file_dump(conf, file, sort);
         if (file != stdout)
            fclose(file);
         conf->flags &= ~CONF_FILE_FLG_MODIFIED;
      }
   }
   return true;
}
