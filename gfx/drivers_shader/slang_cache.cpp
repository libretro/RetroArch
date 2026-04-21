#include "slang_cache.h"
#include "glslang_util.h"
#include "slang_process.h"

#include <stdlib.h>
#include <string.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <vfs/vfs.h>
#include <compat/strl.h>
#include <lrc_hash.h>

#include "../../configuration.h"
#include "../../verbosity.h"

#define SPIRV_CACHE_VERSION 1
#define SPIRV_CACHE_SUBDIR  "spirv"

/**
 * Get the full path to the SPIR-V cache directory
 *
 * @param cache_dir_out Output buffer for the cache directory path (must be at least PATH_MAX_LENGTH)
 * @param cache_dir_out_len Size of the output buffer
 * @return true on success, false if cache dir is not configured
 */
static bool spirv_cache_get_dir(char *cache_dir_out, size_t cache_dir_out_len)
{
   settings_t *settings = config_get_ptr();

   if (!settings || !settings->paths.directory_cache[0])
      return false;

   /* Build the spirv subdirectory path */
   snprintf(cache_dir_out, cache_dir_out_len, "%s/%s",
         settings->paths.directory_cache, SPIRV_CACHE_SUBDIR);

   return true;
}

/**
 * Ensure the SPIR-V cache directory exists
 *
 * @return true if directory exists or was created, false on error
 */
static bool spirv_cache_ensure_dir(void)
{
   char cache_dir[PATH_MAX_LENGTH];

   if (!spirv_cache_get_dir(cache_dir, sizeof(cache_dir)))
      return false;

   return path_mkdir(cache_dir);
}

/**
 * Get the full path to a cache file for a given hash
 *
 * @param hash Hash string (64 characters)
 * @param cache_file_out Output buffer for the full cache file path
 * @param cache_file_out_len Size of the output buffer
 * @return true on success, false on error
 */
static bool spirv_cache_get_filename(const char *hash,
      char *cache_file_out, size_t cache_file_out_len)
{
   char cache_dir[PATH_MAX_LENGTH];

   if (!spirv_cache_get_dir(cache_dir, sizeof(cache_dir)))
      return false;

   snprintf(cache_file_out, cache_file_out_len, "%s/%s.spirv",
         cache_dir, hash);

   return true;
}

/**
 * Write a null-terminated string to a file with length prefix
 *
 * @param file File pointer (opened in binary mode)
 * @param str String to write (may be NULL or empty)
 * @return true on success, false on error
 */
static bool spirv_cache_write_string(RFILE *file, const std::string &str)
{
   uint32_t len = str.length();

   if (filestream_write(file, &len, sizeof(uint32_t)) != sizeof(uint32_t))
      return false;

   if (len > 0 && filestream_write(file, str.c_str(), len) != len)
      return false;

   return true;
}

/**
 * Read a null-terminated string from a file
 *
 * @param file File pointer (opened in binary mode)
 * @param str_out Output string
 * @return true on success, false on error
 */
static bool spirv_cache_read_string(RFILE *file, std::string &str_out)
{
   uint32_t len;

   if (filestream_read(file, &len, sizeof(uint32_t)) != sizeof(uint32_t))
      return false;

   if (len == 0)
   {
      str_out.clear();
      return true;
   }

   /* Allocate and read string */
   char *buf = new char[len + 1];
   if (!buf)
      return false;

   if (filestream_read(file, buf, len) != len)
   {
      delete[] buf;
      return false;
   }

   buf[len] = '\0';
   str_out = buf;
   delete[] buf;

   return true;
}

extern "C" {

bool spirv_cache_compute_hash(const char *vertex_source, const char *fragment_source,
      char *hash_out)
{
   if (!vertex_source || !fragment_source || !hash_out)
      return false;

   /* Build combined hash input: vertex + "|" + fragment */
   size_t vertex_len = strlen(vertex_source);
   size_t fragment_len = strlen(fragment_source);
   size_t total_len = vertex_len + 1 + fragment_len;  /* 1 for "|" separator */

   uint8_t *combined = new uint8_t[total_len];
   if (!combined)
      return false;

   memcpy(combined, vertex_source, vertex_len);
   combined[vertex_len] = '|';
   memcpy(combined + vertex_len + 1, fragment_source, fragment_len);

   /* Compute SHA256 hash using libretro-common */
   sha256_hash(hash_out, combined, total_len);

   delete[] combined;
   return true;
}

bool spirv_cache_load(const char *hash, struct glslang_output *output)
{
   RFILE *file;
   char cache_file[PATH_MAX_LENGTH];
   uint8_t version;
   uint32_t vertex_size, fragment_size, param_count, i;
   uint16_t rt_format;

   if (!hash || !output)
      return false;

   if (!spirv_cache_get_filename(hash, cache_file, sizeof(cache_file)))
      return false;

   file = filestream_open(cache_file, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!file)
      return false; /* Cache file doesn't exist yet */

   /* Read version */
   if (filestream_read(file, &version, sizeof(uint8_t)) != sizeof(uint8_t))
      goto error;

   if (version != SPIRV_CACHE_VERSION)
      goto error; /* Version mismatch */

   /* Read vertex SPIR-V */
   if (filestream_read(file, &vertex_size, sizeof(uint32_t)) != sizeof(uint32_t))
      goto error;

   if (vertex_size > 0)
   {
      output->vertex.resize(vertex_size);
      if (filestream_read(file, output->vertex.data(), vertex_size * sizeof(uint32_t)) != (int64_t)(vertex_size * sizeof(uint32_t)))
         goto error;
   }

   /* Read fragment SPIR-V */
   if (filestream_read(file, &fragment_size, sizeof(uint32_t)) != sizeof(uint32_t))
      goto error;

   if (fragment_size > 0)
   {
      output->fragment.resize(fragment_size);
      if (filestream_read(file, output->fragment.data(), fragment_size * sizeof(uint32_t)) != (int64_t)(fragment_size * sizeof(uint32_t)))
         goto error;
   }

   /* Read parameters count */
   if (filestream_read(file, &param_count, sizeof(uint32_t)) != sizeof(uint32_t))
      goto error;

   if (param_count > 0)
      output->meta.parameters.resize(param_count);

   /* Read each parameter */
   for (i = 0; i < param_count; i++)
   {
      glslang_parameter &param = output->meta.parameters[i];

      if (!spirv_cache_read_string(file, param.id))
         goto error;

      if (!spirv_cache_read_string(file, param.desc))
         goto error;

      if (filestream_read(file, &param.initial, sizeof(float)) != sizeof(float))
         goto error;
      if (filestream_read(file, &param.minimum, sizeof(float)) != sizeof(float))
         goto error;
      if (filestream_read(file, &param.maximum, sizeof(float)) != sizeof(float))
         goto error;
      if (filestream_read(file, &param.step, sizeof(float)) != sizeof(float))
         goto error;
   }

   /* Read shader name */
   if (!spirv_cache_read_string(file, output->meta.name))
      goto error;

   /* Read render target format */
   if (filestream_read(file, &rt_format, sizeof(uint16_t)) != sizeof(uint16_t))
      goto error;
   output->meta.rt_format = (enum glslang_format)rt_format;

   filestream_close(file);

   RARCH_LOG("[Slang Cache] Loaded shader cache for hash: %.16s...\n", hash);

   return true;

error:
   filestream_close(file);
   return false;
}

bool spirv_cache_save(const char *hash, const struct glslang_output *output)
{
   RFILE *file;
   char cache_file[PATH_MAX_LENGTH];
   uint8_t version = SPIRV_CACHE_VERSION;
   uint32_t vertex_size, fragment_size, param_count, i;
   uint16_t rt_format;

   if (!hash || !output)
      return false;

   /* Ensure cache directory exists */
   if (!spirv_cache_ensure_dir())
      return false;

   if (!spirv_cache_get_filename(hash, cache_file, sizeof(cache_file)))
      return false;

   file = filestream_open(cache_file, RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!file)
      return false;

   /* Write version */
   if (filestream_write(file, &version, sizeof(uint8_t)) != sizeof(uint8_t))
      goto error;

   /* Write vertex SPIR-V */
   vertex_size = output->vertex.size();
   if (filestream_write(file, &vertex_size, sizeof(uint32_t)) != sizeof(uint32_t))
      goto error;
   if (vertex_size > 0)
   {
      if (filestream_write(file, output->vertex.data(), vertex_size * sizeof(uint32_t)) != (int64_t)(vertex_size * sizeof(uint32_t)))
         goto error;
   }

   /* Write fragment SPIR-V */
   fragment_size = output->fragment.size();
   if (filestream_write(file, &fragment_size, sizeof(uint32_t)) != sizeof(uint32_t))
      goto error;
   if (fragment_size > 0)
   {
      if (filestream_write(file, output->fragment.data(), fragment_size * sizeof(uint32_t)) != (int64_t)(fragment_size * sizeof(uint32_t)))
         goto error;
   }

   /* Write parameters */
   param_count = output->meta.parameters.size();
   if (filestream_write(file, &param_count, sizeof(uint32_t)) != sizeof(uint32_t))
      goto error;

   for (i = 0; i < param_count; i++)
   {
      const glslang_parameter &param = output->meta.parameters[i];

      if (!spirv_cache_write_string(file, param.id))
         goto error;
      if (!spirv_cache_write_string(file, param.desc))
         goto error;

      if (filestream_write(file, &param.initial, sizeof(float)) != sizeof(float))
         goto error;
      if (filestream_write(file, &param.minimum, sizeof(float)) != sizeof(float))
         goto error;
      if (filestream_write(file, &param.maximum, sizeof(float)) != sizeof(float))
         goto error;
      if (filestream_write(file, &param.step, sizeof(float)) != sizeof(float))
         goto error;
   }

   /* Write shader name */
   if (!spirv_cache_write_string(file, output->meta.name))
      goto error;

   /* Write render target format */
   rt_format = (uint16_t)output->meta.rt_format;
   if (filestream_write(file, &rt_format, sizeof(uint16_t)) != sizeof(uint16_t))
      goto error;

   filestream_close(file);

   RARCH_LOG("[Slang Cache] Saved shader cache for hash: %.16s...\n", hash);

   return true;

error:
   filestream_close(file);
   filestream_delete(cache_file); /* Clean up partial file on error */
   return false;
}

} /* extern "C" */
