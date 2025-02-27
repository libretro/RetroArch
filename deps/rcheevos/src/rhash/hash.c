#include "rc_hash.h"

#include "rc_hash_internal.h"

#include "../rc_compat.h"

#include "md5.h"

#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <share.h>
#endif

const char* rc_path_get_filename(const char* path);
static int rc_hash_from_file(char hash[33], uint32_t console_id, const rc_hash_iterator_t* iterator);

/* ===================================================== */

static rc_hash_message_callback g_error_message_callback = NULL;
rc_hash_message_callback g_verbose_message_callback = NULL;

static void rc_hash_dispatch_message_va(const rc_hash_message_callback callback, const char* format, va_list args)
{
  char buffer[1024];

#ifdef __STDC_SECURE_LIB__
  vsprintf_s(buffer, sizeof(buffer), format, args);
#elif __STDC_VERSION__ >= 199901L /* vsnprintf requires c99 */
  vsnprintf(buffer, sizeof(buffer), format, args);
#else /* c89 doesn't have a size-limited vsprintf function - assume the buffer is large enough */
  vsprintf(buffer, format, args);
#endif

  callback(buffer);
}

void rc_hash_init_error_message_callback(rc_hash_message_callback callback)
{
  g_error_message_callback = callback;
}

static rc_hash_message_callback rc_hash_get_error_message_callback(const rc_hash_callbacks_t* callbacks)
{
  if (callbacks && callbacks->error_message)
    return callbacks->error_message;

  if (g_error_message_callback)
    return g_error_message_callback;

  if (callbacks && callbacks->verbose_message)
    return callbacks->verbose_message;

  if (g_verbose_message_callback)
    return g_verbose_message_callback;

  return NULL;
}

int rc_hash_error(const rc_hash_callbacks_t* callbacks, const char* message)
{
  rc_hash_message_callback message_callback = rc_hash_get_error_message_callback(callbacks);

  if (message_callback)
    message_callback(message);

  return 0;
}

int rc_hash_error_formatted(const rc_hash_callbacks_t* callbacks, const char* format, ...)
{
  rc_hash_message_callback message_callback = rc_hash_get_error_message_callback(callbacks);

  if (message_callback) {
    va_list args;
    va_start(args, format);
    rc_hash_dispatch_message_va(message_callback, format, args);
    va_end(args);
  }

  return 0;
}

int rc_hash_iterator_error(const rc_hash_iterator_t* iterator, const char* message)
{
  rc_hash_message_callback message_callback = rc_hash_get_error_message_callback(&iterator->callbacks);

  if (message_callback)
    message_callback(message);

  return 0;
}

int rc_hash_iterator_error_formatted(const rc_hash_iterator_t* iterator, const char* format, ...)
{
  rc_hash_message_callback message_callback = rc_hash_get_error_message_callback(&iterator->callbacks);

  if (message_callback) {
    va_list args;
    va_start(args, format);
    rc_hash_dispatch_message_va(message_callback, format, args);
    va_end(args);
  }

  return 0;
}

void rc_hash_init_verbose_message_callback(rc_hash_message_callback callback)
{
  g_verbose_message_callback = callback;
}

void rc_hash_verbose(const rc_hash_callbacks_t* callbacks, const char* message)
{
  if (callbacks->verbose_message)
    callbacks->verbose_message(message);
  else if (g_verbose_message_callback)
    g_verbose_message_callback(message);
}

void rc_hash_verbose_formatted(const rc_hash_callbacks_t* callbacks, const char* format, ...)
{
  if (callbacks && callbacks->verbose_message) {
    va_list args;
    va_start(args, format);
    rc_hash_dispatch_message_va(callbacks->verbose_message, format, args);
    va_end(args);
  }
  else if (g_verbose_message_callback) {
    va_list args;
    va_start(args, format);
    rc_hash_dispatch_message_va(g_verbose_message_callback, format, args);
    va_end(args);
  }
}

void rc_hash_iterator_verbose(const rc_hash_iterator_t* iterator, const char* message)
{
  rc_hash_verbose(&iterator->callbacks, message);
}

void rc_hash_iterator_verbose_formatted(const rc_hash_iterator_t* iterator, const char* format, ...)
{
  if (iterator->callbacks.verbose_message) {
    va_list args;
    va_start(args, format);
    rc_hash_dispatch_message_va(iterator->callbacks.verbose_message, format, args);
    va_end(args);
  }
  else if (g_verbose_message_callback) {
    va_list args;
    va_start(args, format);
    rc_hash_dispatch_message_va(g_verbose_message_callback, format, args);
    va_end(args);
  }
}

/* ===================================================== */

static struct rc_hash_filereader g_filereader_funcs;
static struct rc_hash_filereader* g_filereader = NULL;

#if defined(WINVER) && WINVER >= 0x0500
static void* filereader_open(const char* path)
{
  /* Windows requires using wchar APIs for Unicode paths */
  /* Note that MultiByteToWideChar will only be defined for >= Windows 2000 */
  wchar_t* wpath;
  int wpath_length;
  FILE* fp;

  /* Calculate wpath length from path */
  wpath_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
  if (wpath_length == 0) /* 0 indicates error (this is likely from invalid UTF-8) */
    return NULL;

  wpath = (wchar_t*)malloc(wpath_length * sizeof(wchar_t));
  if (!wpath)
    return NULL;

  if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, wpath_length) == 0)
  {
    free(wpath);
    return NULL;
  }

 #if defined(__STDC_SECURE_LIB__)
  /* have to use _SH_DENYNO because some cores lock the file while its loaded */
  fp = _wfsopen(wpath, L"rb", _SH_DENYNO);
 #else
  fp = _wfopen(wpath, L"rb");
 #endif

  free(wpath);
  return fp;
}
#else /* !WINVER >= 0x0500 */
static void* filereader_open(const char* path)
{
 #if defined(__STDC_SECURE_LIB__)
  #if defined(WINVER)
   /* have to use _SH_DENYNO because some cores lock the file while its loaded */
   return _fsopen(path, "rb", _SH_DENYNO);
  #else /* !WINVER */
   FILE *fp;
   fopen_s(&fp, path, "rb");
   return fp;
  #endif
 #else /* !__STDC_SECURE_LIB__ */
  return fopen(path, "rb");
 #endif
}
#endif /* WINVER >= 0x0500 */

static void filereader_seek(void* file_handle, int64_t offset, int origin)
{
#if defined(_WIN32)
  _fseeki64((FILE*)file_handle, offset, origin);
#elif defined(_LARGEFILE64_SOURCE)
  fseeko64((FILE*)file_handle, offset, origin);
#else
  fseek((FILE*)file_handle, offset, origin);
#endif
}

static int64_t filereader_tell(void* file_handle)
{
#if defined(_WIN32)
  return _ftelli64((FILE*)file_handle);
#elif defined(_LARGEFILE64_SOURCE)
  return ftello64((FILE*)file_handle);
#else
  return ftell((FILE*)file_handle);
#endif
}

static size_t filereader_read(void* file_handle, void* buffer, size_t requested_bytes)
{
  return fread(buffer, 1, requested_bytes, (FILE*)file_handle);
}

static void filereader_close(void* file_handle)
{
  fclose((FILE*)file_handle);
}

/* for unit tests - normally would call rc_hash_init_custom_filereader(NULL) */
void rc_hash_reset_filereader(void)
{
  g_filereader = NULL;
}

void rc_hash_init_custom_filereader(struct rc_hash_filereader* reader)
{
  /* initialize with defaults first */
  g_filereader_funcs.open = filereader_open;
  g_filereader_funcs.seek = filereader_seek;
  g_filereader_funcs.tell = filereader_tell;
  g_filereader_funcs.read = filereader_read;
  g_filereader_funcs.close = filereader_close;

  /* hook up any provided custom handlers */
  if (reader) {
    if (reader->open)
      g_filereader_funcs.open = reader->open;

    if (reader->seek)
      g_filereader_funcs.seek = reader->seek;

    if (reader->tell)
      g_filereader_funcs.tell = reader->tell;

    if (reader->read)
      g_filereader_funcs.read = reader->read;

    if (reader->close)
      g_filereader_funcs.close = reader->close;
  }

  g_filereader = &g_filereader_funcs;
}

void* rc_file_open(const rc_hash_iterator_t* iterator, const char* path)
{
  void* handle = NULL;

  if (!iterator->callbacks.filereader.open) {
    rc_hash_iterator_error(iterator, "No callback registered for opening files");
  } else {
    handle = iterator->callbacks.filereader.open(path);
    if (handle)
      rc_hash_iterator_verbose_formatted(iterator, "Opened %s", rc_path_get_filename(path));
  }

  return handle;
}

void rc_file_seek(const rc_hash_iterator_t* iterator, void* file_handle, int64_t offset, int origin)
{
  if (iterator->callbacks.filereader.seek)
    iterator->callbacks.filereader.seek(file_handle, offset, origin);
}

int64_t rc_file_tell(const rc_hash_iterator_t* iterator, void* file_handle)
{
  return iterator->callbacks.filereader.tell ? iterator->callbacks.filereader.tell(file_handle) : 0;
}

size_t rc_file_read(const rc_hash_iterator_t* iterator, void* file_handle, void* buffer, int requested_bytes)
{
  return iterator->callbacks.filereader.read ? iterator->callbacks.filereader.read(file_handle, buffer, requested_bytes) : 0;
}

void rc_file_close(const rc_hash_iterator_t* iterator, void* file_handle)
{
  if (iterator->callbacks.filereader.close)
    iterator->callbacks.filereader.close(file_handle);
}

int64_t rc_file_size(const rc_hash_iterator_t* iterator, const char* path)
{
  int64_t size = 0;

  /* don't use rc_file_open to avoid log statements */
  if (!iterator->callbacks.filereader.open) {
    rc_hash_iterator_error(iterator, "No callback registered for opening files");
  } else {
    void* handle = iterator->callbacks.filereader.open(path);
    if (handle) {
      rc_file_seek(iterator, handle, 0, SEEK_END);
      size = rc_file_tell(iterator, handle);
      rc_file_close(iterator, handle);
    }
  }

  return size;
}

/* ===================================================== */

static struct rc_hash_cdreader g_cdreader_funcs;
static struct rc_hash_cdreader* g_cdreader = NULL;

void rc_hash_init_custom_cdreader(struct rc_hash_cdreader* reader)
{
  if (reader)
  {
    memcpy(&g_cdreader_funcs, reader, sizeof(g_cdreader_funcs));
    g_cdreader = &g_cdreader_funcs;
  }
  else
  {
    g_cdreader = NULL;
  }
}

static void* rc_cd_open_track(const rc_hash_iterator_t* iterator, uint32_t track)
{
  if (iterator->callbacks.cdreader.open_track_filereader)
    return iterator->callbacks.cdreader.open_track_filereader(iterator->path, track, &iterator->callbacks.filereader);

  if (iterator->callbacks.cdreader.open_track)
    return iterator->callbacks.cdreader.open_track(iterator->path, track);

  if (g_cdreader && g_cdreader->open_track)
    return g_cdreader->open_track(iterator->path, track);

  rc_hash_iterator_error(iterator, "no hook registered for cdreader_open_track");
  return NULL;
}

static size_t rc_cd_read_sector(const rc_hash_iterator_t* iterator, void* track_handle, uint32_t sector, void* buffer, size_t requested_bytes)
{
  if (iterator->callbacks.cdreader.read_sector)
    return iterator->callbacks.cdreader.read_sector(track_handle, sector, buffer, requested_bytes);

  if (g_cdreader && g_cdreader->read_sector)
    return g_cdreader->read_sector(track_handle, sector, buffer, requested_bytes);

  rc_hash_iterator_error(iterator, "no hook registered for cdreader_read_sector");
  return 0;
}

static uint32_t rc_cd_first_track_sector(const rc_hash_iterator_t* iterator, void* track_handle)
{
  if (iterator->callbacks.cdreader.first_track_sector)
    return iterator->callbacks.cdreader.first_track_sector(track_handle);

  if (g_cdreader && g_cdreader->first_track_sector)
    return g_cdreader->first_track_sector(track_handle);

  rc_hash_iterator_error(iterator, "no hook registered for cdreader_first_track_sector");
  return 0;
}

static void rc_cd_close_track(const rc_hash_iterator_t* iterator, void* track_handle)
{
  if (iterator->callbacks.cdreader.close_track)
    iterator->callbacks.cdreader.close_track(track_handle);
  else if (g_cdreader && g_cdreader->close_track)
    g_cdreader->close_track(track_handle);
  else
    rc_hash_iterator_error(iterator, "no hook registered for cdreader_close_track");
}

static uint32_t rc_cd_find_file_sector(const rc_hash_iterator_t* iterator, void* track_handle, const char* path, uint32_t* size)
{
  uint8_t buffer[2048], *tmp;
  int sector;
  uint32_t num_sectors = 0;
  size_t filename_length;
  const char* slash;

  if (!track_handle)
    return 0;

  /* we start at the root. don't need to explicitly find it */
  if (*path == '\\')
    ++path;

  filename_length = strlen(path);
  slash = strrchr(path, '\\');
  if (slash)
  {
    /* find the directory record for the first part of the path */
    memcpy(buffer, path, slash - path);
    buffer[slash - path] = '\0';

    sector = rc_cd_find_file_sector(iterator, track_handle, (const char *)buffer, NULL);
    if (!sector)
      return 0;

    ++slash;
    filename_length -= (slash - path);
    path = slash;
  }
  else
  {
    uint32_t logical_block_size;

    /* find the cd information */
    if (!rc_cd_read_sector(iterator, track_handle, rc_cd_first_track_sector(iterator, track_handle) + 16, buffer, 256))
      return 0;

    /* the directory_record starts at 156, the sector containing the table of contents is 2 bytes into that.
     * https://www.cdroller.com/htm/readdata.html
     */
    sector = buffer[156 + 2] | (buffer[156 + 3] << 8) | (buffer[156 + 4] << 16);

    /* if the table of contents spans more than one sector, it's length of section will exceed it's logical block size */
    logical_block_size = (buffer[128] | (buffer[128 + 1] << 8)); /* logical block size */
    if (logical_block_size == 0) {
      num_sectors = 1;
    } else {
      num_sectors = (buffer[156 + 10] | (buffer[156 + 11] << 8) | (buffer[156 + 12] << 16) | (buffer[156 + 13] << 24)); /* length of section */
      num_sectors /= logical_block_size;
    }
  }

  /* fetch and process the directory record */
  if (!rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer)))
    return 0;

  tmp = buffer;
  do
  {
    if (tmp >= buffer + sizeof(buffer) || !*tmp)
    {
      /* end of this path table block. if the path table spans multiple sectors, keep scanning */
      if (num_sectors > 1)
      {
        --num_sectors;
        if (rc_cd_read_sector(iterator, track_handle, ++sector, buffer, sizeof(buffer)))
        {
          tmp = buffer;
          continue;
        }
      }
      break;
    }

    /* filename is 33 bytes into the record and the format is "FILENAME;version" or "DIRECTORY" */
    if ((tmp[32] == filename_length || tmp[33 + filename_length] == ';') &&
        strncasecmp((const char*)(tmp + 33), path, filename_length) == 0)
    {
      sector = tmp[2] | (tmp[3] << 8) | (tmp[4] << 16);

      rc_hash_iterator_verbose_formatted(iterator, "Found %s at sector %d", path, sector);

      if (size)
        *size = tmp[10] | (tmp[11] << 8) | (tmp[12] << 16) | (tmp[13] << 24);

      return sector;
    }

    /* the first byte of the record is the length of the record */
    tmp += *tmp;
  } while (1);

  return 0;
}

/* ===================================================== */

#ifndef RC_HASH_NO_ENCRYPTED

static rc_hash_3ds_get_cia_normal_key_func _3ds_get_cia_normal_key_func = NULL;
static rc_hash_3ds_get_ncch_normal_keys_func _3ds_get_ncch_normal_keys_func = NULL;

void rc_hash_init_3ds_get_cia_normal_key_func(rc_hash_3ds_get_cia_normal_key_func func)
{
  _3ds_get_cia_normal_key_func = func;
}

void rc_hash_init_3ds_get_ncch_normal_keys_func(rc_hash_3ds_get_ncch_normal_keys_func func)
{
  _3ds_get_ncch_normal_keys_func = func;
}

#endif

/* ===================================================== */

const char* rc_path_get_filename(const char* path)
{
  const char* ptr = path + strlen(path);
  do
  {
    if (ptr[-1] == '/' || ptr[-1] == '\\')
      break;

    --ptr;
  } while (ptr > path);

  return ptr;
}

const char* rc_path_get_extension(const char* path)
{
  const char* ptr = path + strlen(path);
  do
  {
    if (ptr[-1] == '.')
      return ptr;

    --ptr;
  } while (ptr > path);

  return path + strlen(path);
}

int rc_path_compare_extension(const char* path, const char* ext)
{
  size_t path_len = strlen(path);
  size_t ext_len = strlen(ext);
  const char* ptr = path + path_len - ext_len;
  if (ptr[-1] != '.')
    return 0;

  if (memcmp(ptr, ext, ext_len) == 0)
    return 1;

  do
  {
    if (tolower(*ptr) != *ext)
      return 0;

    ++ext;
    ++ptr;
  } while (*ptr);

  return 1;
}

/* ===================================================== */

void rc_hash_byteswap16(uint8_t* buffer, const uint8_t* stop)
{
  uint32_t* ptr = (uint32_t*)buffer;
  const uint32_t* stop32 = (const uint32_t*)stop;
  while (ptr < stop32)
  {
    uint32_t temp = *ptr;
    temp = (temp & 0xFF00FF00) >> 8 |
           (temp & 0x00FF00FF) << 8;
    *ptr++ = temp;
  }
}

void rc_hash_byteswap32(uint8_t* buffer, const uint8_t* stop)
{
  uint32_t* ptr = (uint32_t*)buffer;
  const uint32_t* stop32 = (const uint32_t*)stop;
  while (ptr < stop32)
  {
    uint32_t temp = *ptr;
    temp = (temp & 0xFF000000) >> 24 |
           (temp & 0x00FF0000) >> 8 |
           (temp & 0x0000FF00) << 8 |
           (temp & 0x000000FF) << 24;
    *ptr++ = temp;
  }
}

int rc_hash_finalize(const rc_hash_iterator_t* iterator, md5_state_t* md5, char hash[33])
{
  md5_byte_t digest[16];

  md5_finish(md5, digest);

  /* NOTE: sizeof(hash) is 4 because it's still treated like a pointer, despite specifying a size */
  snprintf(hash, 33, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
    digest[0], digest[1], digest[2], digest[3], digest[4], digest[5], digest[6], digest[7],
    digest[8], digest[9], digest[10], digest[11], digest[12], digest[13], digest[14], digest[15]
  );

  rc_hash_iterator_verbose_formatted(iterator, "Generated hash %s", hash);

  return 1;
}

int rc_hash_buffer(char hash[33], const uint8_t* buffer, size_t buffer_size, const rc_hash_iterator_t* iterator)
{
  md5_state_t md5;

  if (buffer_size > MAX_BUFFER_SIZE)
    buffer_size = MAX_BUFFER_SIZE;

  md5_init(&md5);

  md5_append(&md5, buffer, (int)buffer_size);

  rc_hash_iterator_verbose_formatted(iterator, "Hashing %u byte buffer", (unsigned)buffer_size);

  return rc_hash_finalize(iterator, &md5, hash);
}

static int rc_hash_cd_file(md5_state_t* md5, const rc_hash_iterator_t* iterator, void* track_handle, uint32_t sector, const char* name, uint32_t size, const char* description)
{
  uint8_t buffer[2048];
  size_t num_read;

  if ((num_read = rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer))) < sizeof(buffer))
    return rc_hash_iterator_error_formatted(iterator, "Could not read %s", description);

  if (size > MAX_BUFFER_SIZE)
    size = MAX_BUFFER_SIZE;

  if (name)
    rc_hash_iterator_verbose_formatted(iterator, "Hashing %s title (%u bytes) and contents (%u bytes) ", name, (unsigned)strlen(name), size);
  else
    rc_hash_iterator_verbose_formatted(iterator, "Hashing %s contents (%u bytes @ sector %u)", description, size, sector);

  if (size < (unsigned)num_read) /* we read a whole sector - only hash the part containing file data */
    num_read = (size_t)size;

  do
  {
    md5_append(md5, buffer, (int)num_read);

    if (size <= (unsigned)num_read)
      break;
    size -= (unsigned)num_read;

    ++sector;
    if (size >= sizeof(buffer))
      num_read = rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer));
    else
      num_read = rc_cd_read_sector(iterator, track_handle, sector, buffer, size);
  } while (num_read > 0);

  return 1;
}

static int rc_hash_3do(char hash[33], const rc_hash_iterator_t* iterator)
{
  uint8_t buffer[2048];
  const uint8_t operafs_identifier[7] = { 0x01, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x01 };
  void* track_handle;
  md5_state_t md5;
  int sector;
  int block_size, block_location;
  int offset, stop;
  size_t size = 0;

  track_handle = rc_cd_open_track(iterator, 1);
  if (!track_handle)
    return rc_hash_iterator_error(iterator, "Could not open track");

  /* the Opera filesystem stores the volume information in the first 132 bytes of sector 0
   * https://github.com/barbeque/3dodump/blob/master/OperaFS-Format.md
   */
  rc_cd_read_sector(iterator, track_handle, 0, buffer, 132);

  if (memcmp(buffer, operafs_identifier, sizeof(operafs_identifier)) == 0)
  {
    rc_hash_iterator_verbose_formatted(iterator, "Found 3DO CD, title=%.32s", &buffer[0x28]);

    /* include the volume header in the hash */
    md5_init(&md5);
    md5_append(&md5, buffer, 132);

    /* the block size is at offset 0x4C (assume 0x4C is always 0) */
    block_size = buffer[0x4D] * 65536 + buffer[0x4E] * 256 + buffer[0x4F];

    /* the root directory block location is at offset 0x64 (and duplicated several
     * times, but we just look at the primary record) (assume 0x64 is always 0)*/
    block_location = buffer[0x65] * 65536 + buffer[0x66] * 256 + buffer[0x67];

    /* multiply the block index by the block size to get the real address */
    block_location *= block_size;

    /* convert that to a sector and read it */
    sector = block_location / 2048;

    do
    {
      rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer));

      /* offset to start of entries is at offset 0x10 (assume 0x10 and 0x11 are always 0) */
      offset = buffer[0x12] * 256 + buffer[0x13];

      /* offset to end of entries is at offset 0x0C (assume 0x0C is always 0) */
      stop = buffer[0x0D] * 65536 + buffer[0x0E] * 256 + buffer[0x0F];

      while (offset < stop)
      {
        if (buffer[offset + 0x03] == 0x02) /* file */
        {
          if (strcasecmp((const char*)&buffer[offset + 0x20], "LaunchMe") == 0)
          {
            /* the block size is at offset 0x0C (assume 0x0C is always 0) */
            block_size = buffer[offset + 0x0D] * 65536 + buffer[offset + 0x0E] * 256 + buffer[offset + 0x0F];

            /* the block location is at offset 0x44 (assume 0x44 is always 0) */
            block_location = buffer[offset + 0x45] * 65536 + buffer[offset + 0x46] * 256 + buffer[offset + 0x47];
            block_location *= block_size;

            /* the file size is at offset 0x10 (assume 0x10 is always 0) */
            size = (size_t)buffer[offset + 0x11] * 65536 + buffer[offset + 0x12] * 256 + buffer[offset + 0x13];

            rc_hash_iterator_verbose_formatted(iterator, "Hashing header (%u bytes) and %.32s (%u bytes) ", 132, &buffer[offset + 0x20], (unsigned)size);

            break;
          }
        }

        /* the number of extra copies of the file is at offset 0x40 (assume 0x40-0x42 are always 0) */
        offset += 0x48 + buffer[offset + 0x43] * 4;
      }

      if (size != 0)
        break;

      /* did not find the file, see if the directory listing is continued in another sector */
      offset = buffer[0x02] * 256 + buffer[0x03];

      /* no more sectors to search*/
      if (offset == 0xFFFF)
        break;

      /* get next sector */
      offset *= block_size;
      sector = (block_location + offset) / 2048;
    } while (1);

    if (size == 0)
    {
      rc_cd_close_track(iterator, track_handle);
      return rc_hash_iterator_error(iterator, "Could not find LaunchMe");
    }

    sector = block_location / 2048;

    while (size > 2048)
    {
      rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer));
      md5_append(&md5, buffer, sizeof(buffer));

      ++sector;
      size -= 2048;
    }

    rc_cd_read_sector(iterator, track_handle, sector, buffer, size);
    md5_append(&md5, buffer, (int)size);
  }
  else
  {
    rc_cd_close_track(iterator, track_handle);
    return rc_hash_iterator_error(iterator, "Not a 3DO CD");
  }

  rc_cd_close_track(iterator, track_handle);

  return rc_hash_finalize(iterator, &md5, hash);
}

/* helper variable only used for testing */
const char* _rc_hash_jaguar_cd_homebrew_hash = NULL;

static int rc_hash_jaguar_cd(char hash[33], const rc_hash_iterator_t* iterator)
{
  uint8_t buffer[2352];
  void* track_handle;
  md5_state_t md5;
  int byteswapped = 0;
  uint32_t size = 0;
  uint32_t offset = 0;
  uint32_t sector = 0;
  uint32_t remaining;
  uint32_t i;

  /* Jaguar CD header is in the first sector of the first data track OF THE SECOND SESSION.
   * The first track must be an audio track, but may be a warning message or actual game audio */
  track_handle = rc_cd_open_track(iterator, RC_HASH_CDTRACK_FIRST_OF_SECOND_SESSION);
  if (!track_handle)
    return rc_hash_iterator_error(iterator, "Could not open track");

  /* The header is an unspecified distance into the first sector, but usually two bytes in.
   * It consists of 64 bytes of "TAIR" or "ATRI" repeating, depending on whether or not the data 
   * is byteswapped. Then another 32 byte that reads "ATARI APPROVED DATA HEADER ATRI "
   * (possibly byteswapped). Then a big-endian 32-bit value for the address where the boot code
   * should be loaded, and a second big-endian 32-bit value for the size of the boot code. */ 
  sector = rc_cd_first_track_sector(iterator, track_handle);
  rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer));

  for (i = 64; i < sizeof(buffer) - 32 - 4 * 3; i++)
  {
    if (memcmp(&buffer[i], "TARA IPARPVODED TA AEHDAREA RT I", 32) == 0)
    {
      byteswapped = 1;
      offset = i + 32 + 4;
      size = (buffer[offset] << 16) | (buffer[offset + 1] << 24) | (buffer[offset + 2]) | (buffer[offset + 3] << 8);
      break;
    }
    else if (memcmp(&buffer[i], "ATARI APPROVED DATA HEADER ATRI ", 32) == 0)
    {
      byteswapped = 0;
      offset = i + 32 + 4;
      size = (buffer[offset] << 24) | (buffer[offset + 1] << 16) | (buffer[offset + 2] << 8) | (buffer[offset + 3]);
      break;
    }
  }

  if (size == 0) /* did not see ATARI APPROVED DATA HEADER */
  {
    rc_cd_close_track(iterator, track_handle);
    return rc_hash_iterator_error(iterator, "Not a Jaguar CD");
  }

  i = 0; /* only loop once */
  do
  {
    md5_init(&md5);

    offset += 4;

    rc_hash_iterator_verbose_formatted(iterator, "Hashing boot executable (%u bytes starting at %u bytes into sector %u)", size, offset, sector);

    if (size > MAX_BUFFER_SIZE)
      size = MAX_BUFFER_SIZE;

    do
    {
      if (byteswapped)
        rc_hash_byteswap16(buffer, &buffer[sizeof(buffer)]);

      remaining = sizeof(buffer) - offset;
      if (remaining >= size)
      {
        md5_append(&md5, &buffer[offset], size);
        size = 0;
        break;
      }

      md5_append(&md5, &buffer[offset], remaining);
      size -= remaining;
      offset = 0;
    } while (rc_cd_read_sector(iterator, track_handle, ++sector, buffer, sizeof(buffer)) == sizeof(buffer));

    rc_cd_close_track(iterator, track_handle);

    if (size > 0)
      return rc_hash_iterator_error(iterator, "Not enough data");

    rc_hash_finalize(iterator, &md5, hash);

    /* homebrew games all seem to have the same boot executable and store the actual game code in track 2.
     * if we generated something other than the homebrew hash, return it. assume all homebrews are byteswapped. */
    if (strcmp(hash, "254487b59ab21bc005338e85cbf9fd2f") != 0 || !byteswapped) {
      if (_rc_hash_jaguar_cd_homebrew_hash == NULL || strcmp(hash, _rc_hash_jaguar_cd_homebrew_hash) != 0)
        return 1;
    }

    /* if we've already been through the loop a second time, just return the hash */
    if (i == 1)
      return 1;
    ++i;

    rc_hash_iterator_verbose_formatted(iterator, "Potential homebrew at sector %u, checking for KART data in track 2", sector);

    track_handle = rc_cd_open_track(iterator, 2);
    if (!track_handle)
      return rc_hash_iterator_error(iterator, "Could not open track");

    /* track 2 of the homebrew code has the 64 bytes or ATRI followed by 32 bytes of "ATARI APPROVED DATA HEADER ATRI!",
     * then 64 bytes of KART repeating. */
    sector = rc_cd_first_track_sector(iterator, track_handle);
    rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer));
    if (memcmp(&buffer[0x5E], "RT!IRTKA", 8) != 0)
      return rc_hash_iterator_error(iterator, "Homebrew executable not found in track 2");

    /* found KART data*/
    rc_hash_iterator_verbose(iterator, "Found KART data in track 2");

    offset = 0xA6;
    size = (buffer[offset] << 16) | (buffer[offset + 1] << 24) | (buffer[offset + 2]) | (buffer[offset + 3] << 8);
  } while (1);
}

static int rc_hash_neogeo_cd(char hash[33], const rc_hash_iterator_t* iterator)
{
  char buffer[1024], *ptr;
  void* track_handle;
  uint32_t sector;
  uint32_t size;
  md5_state_t md5;

  track_handle = rc_cd_open_track(iterator, 1);
  if (!track_handle)
    return rc_hash_iterator_error(iterator, "Could not open track");

  /* https://wiki.neogeodev.org/index.php?title=IPL_file, https://wiki.neogeodev.org/index.php?title=PRG_file
   * IPL file specifies data to be loaded before the game starts. PRG files are the executable code
   */
  sector = rc_cd_find_file_sector(iterator, track_handle, "IPL.TXT", &size);
  if (!sector)
  {
    rc_cd_close_track(iterator, track_handle);
    return rc_hash_iterator_error(iterator, "Not a NeoGeo CD game disc");
  }

  if (rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer)) == 0)
  {
    rc_cd_close_track(iterator, track_handle);
    return 0;
  }

  md5_init(&md5);

  buffer[sizeof(buffer) - 1] = '\0';
  ptr = &buffer[0];
  do
  {
    char* start = ptr;
    while (*ptr && *ptr != '.')
      ++ptr;

    if (strncasecmp(ptr, ".PRG", 4) == 0)
    {
      ptr += 4;
      *ptr++ = '\0';

      sector = rc_cd_find_file_sector(iterator, track_handle, start, &size);
      if (!sector || !rc_hash_cd_file(&md5, iterator, track_handle, sector, NULL, size, start))
      {
        rc_cd_close_track(iterator, track_handle);
        return rc_hash_iterator_error_formatted(iterator, "Could not read %.16s", start);
      }
    }

    while (*ptr && *ptr != '\n')
      ++ptr;
    if (*ptr != '\n')
      break;
    ++ptr;
  } while (*ptr != '\0' && *ptr != '\x1a');

  rc_cd_close_track(iterator, track_handle);
  return rc_hash_finalize(iterator, &md5, hash);
}

static int rc_hash_nintendo_disc_partition(md5_state_t* md5,
                                           const rc_hash_iterator_t* iterator,
                                           void* file_handle,
                                           const uint32_t part_offset,
                                           uint8_t wii_shift)
{
  const uint32_t BASE_HEADER_SIZE = 0x2440;
  const uint32_t MAX_HEADER_SIZE = 1024 * 1024;

  uint32_t apploader_header_size, apploader_body_size, apploader_trailer_size, header_size;

  uint8_t quad_buffer[4];
  uint8_t addr_buffer[0xD8];
  uint8_t* buffer;

  uint64_t dol_offset;
  uint64_t dol_offsets[18];
  uint64_t dol_sizes[18];

  uint8_t ix;
  uint64_t remaining_size;
  const uint32_t MAX_CHUNK_SIZE = 1024 * 1024;

  /* GetApploaderSize */
  rc_file_seek(iterator, file_handle, part_offset + BASE_HEADER_SIZE + 0x14, SEEK_SET);
  apploader_header_size = 0x20;
  rc_file_read(iterator, file_handle, quad_buffer, 4);
  apploader_body_size =
    ((uint32_t)quad_buffer[0] << 24) | (quad_buffer[1] << 16) | (quad_buffer[2] << 8) | quad_buffer[3];
  rc_file_read(iterator, file_handle, quad_buffer, 4);
  apploader_trailer_size =
    ((uint32_t)quad_buffer[0] << 24) | (quad_buffer[1] << 16) | (quad_buffer[2] << 8) | quad_buffer[3];
  header_size = BASE_HEADER_SIZE + apploader_header_size + apploader_body_size + apploader_trailer_size;
  if (header_size > MAX_HEADER_SIZE) header_size = MAX_HEADER_SIZE;

  /* Hash headers */
  buffer = (uint8_t*)malloc(header_size);
  if (!buffer)
  {
    rc_file_close(iterator, file_handle);
    return rc_hash_iterator_error(iterator, "Could not allocate temporary buffer");
  }
  rc_file_seek(iterator, file_handle, part_offset, SEEK_SET);
  rc_file_read(iterator, file_handle, buffer, header_size);
  rc_hash_iterator_verbose_formatted(iterator, "Hashing %u byte partition header", header_size);
  md5_append(md5, buffer, header_size);

  /* GetBootDOLOffset
   * Base header size is guaranteed larger than 0x423 therefore buffer contains dol_offset right now
   */
  dol_offset = (((uint64_t)buffer[0x420] << 24) |
                ((uint64_t)buffer[0x421] << 16) |
                ((uint64_t)buffer[0x422] << 8) |
                (uint64_t)buffer[0x423]) << wii_shift;
  free(buffer);

  /* Find offsets and sizes for the 7 main.dol code segments and 11 main.dol data segments */
  rc_file_seek(iterator, file_handle, part_offset + dol_offset, SEEK_SET);
  rc_file_read(iterator, file_handle, addr_buffer, 0xD8);
  for (ix = 0; ix < 18; ix++)
  {
    dol_offsets[ix] = (((uint64_t)addr_buffer[0x0 + ix * 4] << 24) |
                       ((uint64_t)addr_buffer[0x1 + ix * 4] << 16) |
                       ((uint64_t)addr_buffer[0x2 + ix * 4] << 8) |
                       (uint64_t)addr_buffer[0x3 + ix * 4]) << wii_shift;
    dol_sizes[ix] = (((uint64_t)addr_buffer[0x90 + ix * 4] << 24) |
                     ((uint64_t)addr_buffer[0x91 + ix * 4] << 16) |
                     ((uint64_t)addr_buffer[0x92 + ix * 4] << 8) |
                     (uint64_t)addr_buffer[0x93 + ix * 4]) << wii_shift;
  }

  /* Iterate through the 18 main.dol segments and hash each */
  buffer = (uint8_t*)malloc(MAX_CHUNK_SIZE);
  if (!buffer)
  {
    rc_file_close(iterator, file_handle);
    return rc_hash_iterator_error(iterator, "Could not allocate temporary buffer");
  }
  for (ix = 0; ix < 18; ix++)
  {
    if (dol_sizes[ix] == 0)
      continue;
    rc_file_seek(iterator, file_handle, part_offset + dol_offsets[ix], SEEK_SET);
    if (ix < 7)
      rc_hash_iterator_verbose_formatted(iterator, "Hashing %u byte main.dol code segment %u", dol_sizes[ix], ix);
    else
      rc_hash_iterator_verbose_formatted(iterator, "Hashing %u byte main.dol data segment %u", dol_sizes[ix], ix - 7);
    remaining_size = dol_sizes[ix];
    while (remaining_size > MAX_CHUNK_SIZE)
    {
      rc_file_read(iterator, file_handle, buffer, MAX_CHUNK_SIZE);
      md5_append(md5, buffer, MAX_CHUNK_SIZE);
      remaining_size -= MAX_CHUNK_SIZE;
    }
    rc_file_read(iterator, file_handle, buffer, (int32_t)remaining_size);
    md5_append(md5, buffer, (int32_t)remaining_size);
  }

  free(buffer);
  return 1;
}

static int rc_hash_wii(md5_state_t* md5,
                       const rc_hash_iterator_t* iterator,
                       void* file_handle)
{
  const uint32_t MAIN_HEADER_SIZE = 0x80;
  const uint64_t REGION_CODE_ADDRESS = 0x4E000;
  const uint32_t CLUSTER_SIZE = 0x7C00;
  const uint32_t MAX_CLUSTER_COUNT = 1024;

  uint32_t partition_info_table[8];
  uint8_t total_partition_count = 0;
  uint32_t* partition_table;
  uint32_t tmd_offset;
  uint32_t tmd_size;
  uint32_t part_offset;
  uint32_t part_size;
  uint32_t cluster_count;

  uint8_t quad_buffer[4];
  uint8_t* buffer;

  uint32_t ix, jx, kx;
  uint8_t encrypted;

  /* Check encryption byte - if 0x61 is 0, disc is encrypted */
  rc_file_seek(iterator, file_handle, 0x61, SEEK_SET);
  rc_file_read(iterator, file_handle, quad_buffer, 1);
  encrypted = (quad_buffer[0] == 0);

  /* Hash main headers */
  buffer = (uint8_t*)malloc(CLUSTER_SIZE);
  if (!buffer)
  {
    rc_file_close(iterator, file_handle);
    return rc_hash_iterator_error(iterator, "Could not allocate temporary buffer");
  }
  rc_file_seek(iterator, file_handle, 0, SEEK_SET);
  rc_file_read(iterator, file_handle, buffer, MAIN_HEADER_SIZE);

  rc_hash_iterator_verbose_formatted(iterator, "Hashing %u byte main header for [%c%c%c%c%c%c]",
                                     MAIN_HEADER_SIZE, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
  md5_append(md5, buffer, MAIN_HEADER_SIZE);

  /* Hash region code */
  rc_file_seek(iterator, file_handle, REGION_CODE_ADDRESS, SEEK_SET);
  rc_file_read(iterator, file_handle, quad_buffer, 4);
  md5_append(md5, quad_buffer, 4);

  /* Scan partition table */
  rc_file_seek(iterator, file_handle, 0x40000, SEEK_SET);
  for (ix = 0; ix < 8; ix++)
  {
    rc_file_read(iterator, file_handle, quad_buffer, 4);
    partition_info_table[ix] =
      ((uint32_t)quad_buffer[0] << 24) | (quad_buffer[1] << 16) | (quad_buffer[2] << 8) | quad_buffer[3];
    if (ix % 2 == 0)
      total_partition_count += partition_info_table[ix];
  }
  partition_table = (uint32_t*)malloc(total_partition_count * 4 * 2);
  kx = 0;
  for (jx = 0; jx < 8; jx += 2)
  {
    rc_file_seek(iterator, file_handle, ((uint64_t)partition_info_table[jx + 1]) << 2, SEEK_SET);
    for (ix = 0; ix < partition_info_table[jx]; ix++)
    {
      rc_file_read(iterator, file_handle, quad_buffer, 4);
      partition_table[kx++] =
        ((uint32_t)quad_buffer[0] << 24) | (quad_buffer[1] << 16) | (quad_buffer[2] << 8) | quad_buffer[3];
      rc_file_read(iterator, file_handle, quad_buffer, 4);
      partition_table[kx++] =
        ((uint32_t)quad_buffer[0] << 24) | (quad_buffer[1] << 16) | (quad_buffer[2] << 8) | quad_buffer[3];
    }
  }

  /* Read each partition */
  for (jx = 0; jx < (uint32_t)total_partition_count * 2; jx += 2)
  {
    /* Don't hash Update partition*/
    if (partition_table[jx + 1] == 1)
      continue;

    /* Hash title metadata */
    rc_file_seek(iterator, file_handle, ((uint64_t)partition_table[jx] << 2) + 0x2A4, SEEK_SET);
    rc_file_read(iterator, file_handle, quad_buffer, 4);
    tmd_size =
      (quad_buffer[0] << 24) | (quad_buffer[1] << 16) | (quad_buffer[2] << 8) | quad_buffer[3];
    rc_file_read(iterator, file_handle, quad_buffer, 4);
    tmd_offset =
      ((uint64_t)((quad_buffer[0] << 24) | (quad_buffer[1] << 16) | (quad_buffer[2] << 8) | quad_buffer[3])) << 2;

    if (tmd_size > CLUSTER_SIZE)
    {
      tmd_size = CLUSTER_SIZE;
    }
    rc_file_seek(iterator, file_handle, ((uint64_t)partition_table[jx] << 2) + tmd_offset, SEEK_SET);
    rc_file_read(iterator, file_handle, buffer, tmd_size);
    rc_hash_iterator_verbose_formatted(iterator, "Hashing %u byte title metadata (partition type %u)",
                                       tmd_size, partition_table[jx + 1]);
    md5_append(md5, buffer, tmd_size);

    /* Hash partition */
    rc_file_seek(iterator, file_handle, ((uint64_t)partition_table[jx] << 2) + 0x2B8, SEEK_SET);
    rc_file_read(iterator, file_handle, quad_buffer, 4);
    part_offset =
      ((uint64_t)((quad_buffer[0] << 24) | (quad_buffer[1] << 16) | (quad_buffer[2] << 8) | quad_buffer[3])) << 2;
    rc_file_read(iterator, file_handle, quad_buffer, 4);
    part_size =
      ((uint64_t)((quad_buffer[0] << 24) | (quad_buffer[1] << 16) | (quad_buffer[2] << 8) | quad_buffer[3])) << 2;

    if (encrypted)
    {
      cluster_count = (part_size / 0x8000 > MAX_CLUSTER_COUNT) ? MAX_CLUSTER_COUNT : part_size / 0x8000;
      rc_hash_iterator_verbose_formatted(iterator, "Hashing %u encrypted clusters (%u bytes)",
                                         cluster_count, cluster_count * CLUSTER_SIZE);
      buffer = (uint8_t*)malloc(CLUSTER_SIZE);
      for (ix = 0; ix < cluster_count; ix++)
      {
        rc_file_seek(iterator, file_handle, part_offset + (ix * 0x8000) + 0x400, SEEK_SET);
        rc_file_read(iterator, file_handle, buffer, CLUSTER_SIZE);
        md5_append(md5, buffer, CLUSTER_SIZE);
      }
    }
    else /* Decrypted */
    {
      if (rc_hash_nintendo_disc_partition(md5, iterator, file_handle, part_offset, 2) == 0)
      {
        free(partition_table);
        free(buffer);
        return 0;
      }
    }
  }
  free(partition_table);
  free(buffer);
  return 1;
}

static int rc_hash_nintendo_disc(char hash[33], const rc_hash_iterator_t* iterator)
{
  md5_state_t md5;
  void* file_handle;

  uint8_t quad_buffer[4];
  uint8_t success;

  file_handle = rc_file_open(iterator, iterator->path);
  if (!file_handle)
    return rc_hash_iterator_error(iterator, "Could not open file");

  md5_init(&md5);
  /* Check Gamecube */
  rc_file_seek(iterator, file_handle, 0x1c, SEEK_SET);
  rc_file_read(iterator, file_handle, quad_buffer, 4);
  if (quad_buffer[0] == 0xC2 && quad_buffer[1] == 0x33 && quad_buffer[2] == 0x9F && quad_buffer[3] == 0x3D)
  {
    success = rc_hash_nintendo_disc_partition(&md5, iterator, file_handle, 0, 0);
  } else {
    /* Check Wii */
    rc_file_seek(iterator, file_handle, 0x18, SEEK_SET);
    rc_file_read(iterator, file_handle, quad_buffer, 4);
    if (quad_buffer[0] == 0x5D && quad_buffer[1] == 0x1C && quad_buffer[2] == 0x9E && quad_buffer[3] == 0xA3)
    {
      success = rc_hash_wii(&md5, iterator, file_handle);
    } else {
      success = rc_hash_iterator_error(iterator, "Not a supported Nintendo disc");
    }
  }

  /* Finalize */
  rc_file_close(iterator, file_handle);

  return success == 0 ? 0 : rc_hash_finalize(iterator, &md5, hash);
}

static int rc_hash_pce_track(char hash[33], void* track_handle, const rc_hash_iterator_t* iterator)
{
  uint8_t buffer[2048];
  md5_state_t md5;
  uint32_t sector, num_sectors;
  uint32_t size;

  /* the PC-Engine uses the second sector to specify boot information and program name.
   * the string "PC Engine CD-ROM SYSTEM" should exist at 32 bytes into the sector
   * http://shu.sheldows.com/shu/download/pcedocs/pce_cdrom.html
   */
  if (rc_cd_read_sector(iterator, track_handle, rc_cd_first_track_sector(iterator, track_handle) + 1, buffer, 128) < 128)
  {
    return rc_hash_iterator_error(iterator, "Not a PC Engine CD");
  }

  /* normal PC Engine CD will have a header block in sector 1 */
  if (memcmp("PC Engine CD-ROM SYSTEM", &buffer[32], 23) == 0)
  {
    /* the title of the disc is the last 22 bytes of the header */
    md5_init(&md5);
    md5_append(&md5, &buffer[106], 22);

    buffer[128] = '\0';
    rc_hash_iterator_verbose_formatted(iterator, "Found PC Engine CD, title=%.22s", &buffer[106]);

    /* the first three bytes specify the sector of the program data, and the fourth byte
     * is the number of sectors.
     */
    sector = (buffer[0] << 16) + (buffer[1] << 8) + buffer[2];
    num_sectors = buffer[3];

    rc_hash_iterator_verbose_formatted(iterator, "Hashing %d sectors starting at sector %d", num_sectors, sector);

    sector += rc_cd_first_track_sector(iterator, track_handle);
    while (num_sectors > 0)
    {
      rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer));
      md5_append(&md5, buffer, sizeof(buffer));

      ++sector;
      --num_sectors;
    }
  }
  /* GameExpress CDs use a standard Joliet filesystem - locate and hash the BOOT.BIN */
  else if ((sector = rc_cd_find_file_sector(iterator, track_handle, "BOOT.BIN", &size)) != 0 && size < MAX_BUFFER_SIZE)
  {
    md5_init(&md5);
    while (size > sizeof(buffer))
    {
      rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer));
      md5_append(&md5, buffer, sizeof(buffer));

      ++sector;
      size -= sizeof(buffer);
    }

    if (size > 0)
    {
      rc_cd_read_sector(iterator, track_handle, sector, buffer, size);
      md5_append(&md5, buffer, size);
    }
  }
  else
  {
    return rc_hash_iterator_error(iterator, "Not a PC Engine CD");
  }

  return rc_hash_finalize(iterator, &md5, hash);
}

static int rc_hash_pce_cd(char hash[33], const rc_hash_iterator_t* iterator)
{
  int result;
  void* track_handle = rc_cd_open_track(iterator, RC_HASH_CDTRACK_FIRST_DATA);
  if (!track_handle)
    return rc_hash_iterator_error(iterator, "Could not open track");

  result = rc_hash_pce_track(hash, track_handle, iterator);

  rc_cd_close_track(iterator, track_handle);

  return result;
}

static int rc_hash_pcfx_cd(char hash[33], const rc_hash_iterator_t* iterator)
{
  uint8_t buffer[2048];
  void* track_handle;
  md5_state_t md5;
  int sector, num_sectors;

  /* PC-FX executable can be in any track. Assume it's in the largest data track and check there first */
  track_handle = rc_cd_open_track(iterator, RC_HASH_CDTRACK_LARGEST);
  if (!track_handle)
    return rc_hash_iterator_error(iterator, "Could not open track");

  /* PC-FX CD will have a header marker in sector 0 */
  sector = rc_cd_first_track_sector(iterator, track_handle);
  rc_cd_read_sector(iterator, track_handle, sector, buffer, 32);
  if (memcmp("PC-FX:Hu_CD-ROM", &buffer[0], 15) != 0)
  {
    rc_cd_close_track(iterator, track_handle);

    /* not found in the largest data track, check track 2 */
    track_handle = rc_cd_open_track(iterator, 2);
    if (!track_handle)
      return rc_hash_iterator_error(iterator, "Could not open track");

    sector = rc_cd_first_track_sector(iterator, track_handle);
    rc_cd_read_sector(iterator, track_handle, sector, buffer, 32);
  }

  if (memcmp("PC-FX:Hu_CD-ROM", &buffer[0], 15) == 0)
  {
    /* PC-FX boot header fills the first two sectors of the disc
     * https://bitbucket.org/trap15/pcfxtools/src/master/pcfx-cdlink.c
     * the important stuff is the first 128 bytes of the second sector (title being the first 32) */
    rc_cd_read_sector(iterator, track_handle, sector + 1, buffer, 128);

    md5_init(&md5);
    md5_append(&md5, buffer, 128);

    rc_hash_iterator_verbose_formatted(iterator, "Found PC-FX CD, title=%.32s", &buffer[0]);

    /* the program sector is in bytes 33-36 (assume byte 36 is 0) */
    sector = (buffer[34] << 16) + (buffer[33] << 8) + buffer[32];

    /* the number of sectors the program occupies is in bytes 37-40 (assume byte 40 is 0) */
    num_sectors = (buffer[38] << 16) + (buffer[37] << 8) + buffer[36];

    rc_hash_iterator_verbose_formatted(iterator, "Hashing %d sectors starting at sector %d", num_sectors, sector);

    sector += rc_cd_first_track_sector(iterator, track_handle);
    while (num_sectors > 0)
    {
      rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer));
      md5_append(&md5, buffer, sizeof(buffer));

      ++sector;
      --num_sectors;
    }
  }
  else
  {
    int result = 0;
    rc_cd_read_sector(iterator, track_handle, sector + 1, buffer, 128);

    /* some PC-FX CDs still identify as PCE CDs */
    if (memcmp("PC Engine CD-ROM SYSTEM", &buffer[32], 23) == 0)
      result = rc_hash_pce_track(hash, track_handle, iterator);

    rc_cd_close_track(iterator, track_handle);
    if (result)
      return result;

    return rc_hash_iterator_error(iterator, "Not a PC-FX CD");
  }

  rc_cd_close_track(iterator, track_handle);

  return rc_hash_finalize(iterator, &md5, hash);
}

static int rc_hash_dreamcast(char hash[33], const rc_hash_iterator_t* iterator)
{
  uint8_t buffer[256] = "";
  void* track_handle;
  char exe_file[32] = "";
  uint32_t size;
  uint32_t sector;
  int result = 0;
  md5_state_t md5;
  int i = 0;

  /* track 03 is the data track that contains the TOC and IP.BIN */
  track_handle = rc_cd_open_track(iterator, 3);
  if (track_handle)
  {
    /* first 256 bytes from first sector should have IP.BIN structure that stores game meta information
     * https://mc.pp.se/dc/ip.bin.html */
    rc_cd_read_sector(iterator, track_handle, rc_cd_first_track_sector(iterator, track_handle), buffer, sizeof(buffer));
  }

  if (memcmp(&buffer[0], "SEGA SEGAKATANA ", 16) != 0)
  {
    if (track_handle)
      rc_cd_close_track(iterator, track_handle);

    /* not a gd-rom dreamcast file. check for mil-cd by looking for the marker in the first data track */
    track_handle = rc_cd_open_track(iterator, RC_HASH_CDTRACK_FIRST_DATA);
    if (!track_handle)
      return rc_hash_iterator_error(iterator, "Could not open track");

    rc_cd_read_sector(iterator, track_handle, rc_cd_first_track_sector(iterator, track_handle), buffer, sizeof(buffer));
    if (memcmp(&buffer[0], "SEGA SEGAKATANA ", 16) != 0)
    {
      /* did not find marker on track 3 or first data track */
      rc_cd_close_track(iterator, track_handle);
      return rc_hash_iterator_error(iterator, "Not a Dreamcast CD");
    }
  }

  /* start the hash with the game meta information */
  md5_init(&md5);
  md5_append(&md5, (md5_byte_t*)buffer, 256);

  if (iterator->callbacks.verbose_message)
  {
    char message[256];
    uint8_t* ptr = &buffer[0xFF];
    while (ptr > &buffer[0x80] && ptr[-1] == ' ')
      --ptr;
    *ptr = '\0';

    snprintf(message, sizeof(message), "Found Dreamcast CD: %.128s (%.16s)", (const char*)&buffer[0x80], (const char*)&buffer[0x40]);
    iterator->callbacks.verbose_message(message);
  }

  /* the boot filename is 96 bytes into the meta information (https://mc.pp.se/dc/ip0000.bin.html) */
  /* remove whitespace from bootfile */
  i = 0;
  while (!isspace((unsigned char)buffer[96 + i]) && i < 16)
    ++i;

  /* sometimes boot file isn't present on meta information.
   * nothing can be done, as even the core doesn't run the game in this case. */
  if (i == 0)
  {
    rc_cd_close_track(iterator, track_handle);
    return rc_hash_iterator_error(iterator, "Boot executable not specified on IP.BIN");
  }

  memcpy(exe_file, &buffer[96], i);
  exe_file[i] = '\0';

  sector = rc_cd_find_file_sector(iterator, track_handle, exe_file, &size);
  if (sector == 0)
  {
    rc_cd_close_track(iterator, track_handle);
    return rc_hash_iterator_error(iterator, "Could not locate boot executable");
  }

  if (rc_cd_read_sector(iterator, track_handle, sector, buffer, 1))
  {
    /* the boot executable is in the primary data track */
  }
  else
  {
    rc_cd_close_track(iterator, track_handle);

    /* the boot executable is normally in the last track */
    track_handle = rc_cd_open_track(iterator, RC_HASH_CDTRACK_LAST);
  }

  result = rc_hash_cd_file(&md5, iterator, track_handle, sector, NULL, size, "boot executable");
  rc_cd_close_track(iterator, track_handle);

  rc_hash_finalize(iterator, &md5, hash);
  return result;
}

static int rc_hash_find_playstation_executable(const rc_hash_iterator_t* iterator, void* track_handle,
                                               const char* boot_key, const char* cdrom_prefix,
                                               char exe_name[], uint32_t exe_name_size, uint32_t* exe_size)
{
  uint8_t buffer[2048];
  uint32_t size;
  char* ptr;
  char* start;
  const size_t boot_key_len = strlen(boot_key);
  const size_t cdrom_prefix_len = strlen(cdrom_prefix);
  int sector;

  sector = rc_cd_find_file_sector(iterator, track_handle, "SYSTEM.CNF", NULL);
  if (!sector)
    return 0;

  size = (uint32_t)rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer) - 1);
  buffer[size] = '\0';

  sector = 0;
  for (ptr = (char*)buffer; *ptr; ++ptr)
  {
    if (strncmp(ptr, boot_key, boot_key_len) == 0)
    {
      ptr += boot_key_len;
      while (isspace((unsigned char)*ptr))
        ++ptr;

      if (*ptr == '=')
      {
        ++ptr;
        while (isspace((unsigned char)*ptr))
          ++ptr;

        if (strncmp(ptr, cdrom_prefix, cdrom_prefix_len) == 0)
          ptr += cdrom_prefix_len;
        while (*ptr == '\\')
          ++ptr;

        start = ptr;
        while (!isspace((unsigned char)*ptr) && *ptr != ';')
          ++ptr;

        size = (uint32_t)(ptr - start);
        if (size >= exe_name_size)
          size = exe_name_size - 1;

        memcpy(exe_name, start, size);
        exe_name[size] = '\0';

        rc_hash_iterator_verbose_formatted(iterator, "Looking for boot executable: %s", exe_name);

        sector = rc_cd_find_file_sector(iterator, track_handle, exe_name, exe_size);
        break;
      }
    }

    /* advance to end of line */
    while (*ptr && *ptr != '\n')
      ++ptr;
  }

  return sector;
}

static int rc_hash_psx(char hash[33], const rc_hash_iterator_t* iterator)
{
  uint8_t buffer[32];
  char exe_name[64] = "";
  void* track_handle;
  uint32_t sector;
  uint32_t size;
  int result = 0;
  md5_state_t md5;

  track_handle = rc_cd_open_track(iterator, 1);
  if (!track_handle)
    return rc_hash_iterator_error(iterator, "Could not open track");

  sector = rc_hash_find_playstation_executable(iterator, track_handle, "BOOT", "cdrom:", exe_name, sizeof(exe_name), &size);
  if (!sector)
  {
    sector = rc_cd_find_file_sector(iterator, track_handle, "PSX.EXE", &size);
    if (sector)
      memcpy(exe_name, "PSX.EXE", 8);
  }

  if (!sector)
  {
    rc_hash_iterator_error(iterator, "Could not locate primary executable");
  }
  else if (rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer)) < sizeof(buffer))
  {
    rc_hash_iterator_error(iterator, "Could not read primary executable");
  }
  else
  {
    if (memcmp(buffer, "PS-X EXE", 7) != 0)
    {
      rc_hash_iterator_verbose_formatted(iterator, "%s did not contain PS-X EXE marker", exe_name);
    }
    else
    {
      /* the PS-X EXE header specifies the executable size as a 4-byte value 28 bytes into the header, which doesn't
       * include the header itself. We want to include the header in the hash, so append another 2048 to that value.
       */
      size = (((uint8_t)buffer[31] << 24) | ((uint8_t)buffer[30] << 16) | ((uint8_t)buffer[29] << 8) | (uint8_t)buffer[28]) + 2048;
    }

    /* there's a few games that use a singular engine and only differ via their data files. luckily, they have unique
     * serial numbers, and use the serial number as the boot file in the standard way. include the boot file name in the hash.
     */
    md5_init(&md5);
    md5_append(&md5, (md5_byte_t*)exe_name, (int)strlen(exe_name));

    result = rc_hash_cd_file(&md5, iterator, track_handle, sector, exe_name, size, "primary executable");
    rc_hash_finalize(iterator, &md5, hash);
  }

  rc_cd_close_track(iterator, track_handle);

  return result;
}

static int rc_hash_ps2(char hash[33], const rc_hash_iterator_t* iterator)
{
  uint8_t buffer[4];
  char exe_name[64] = "";
  void* track_handle;
  uint32_t sector;
  uint32_t size;
  int result = 0;
  md5_state_t md5;

  track_handle = rc_cd_open_track(iterator, 1);
  if (!track_handle)
    return rc_hash_iterator_error(iterator, "Could not open track");

  sector = rc_hash_find_playstation_executable(iterator, track_handle, "BOOT2", "cdrom0:", exe_name, sizeof(exe_name), &size);
  if (!sector)
  {
    rc_hash_iterator_error(iterator, "Could not locate primary executable");
  }
  else if (rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer)) < sizeof(buffer))
  {
    rc_hash_iterator_error(iterator, "Could not read primary executable");
  }
  else
  {
    if (memcmp(buffer, "\x7f\x45\x4c\x46", 4) != 0)
    {
      rc_hash_iterator_verbose_formatted(iterator, "%s did not contain ELF marker", exe_name);
    }

    /* there's a few games that use a singular engine and only differ via their data files. luckily, they have unique
     * serial numbers, and use the serial number as the boot file in the standard way. include the boot file name in the hash.
     */
    md5_init(&md5);
    md5_append(&md5, (md5_byte_t*)exe_name, (int)strlen(exe_name));

    result = rc_hash_cd_file(&md5, iterator, track_handle, sector, exe_name, size, "primary executable");
    rc_hash_finalize(iterator, &md5, hash);
  }

  rc_cd_close_track(iterator, track_handle);

  return result;
}

static int rc_hash_psp(char hash[33], const rc_hash_iterator_t* iterator)
{
  void* track_handle;
  uint32_t sector;
  uint32_t size;
  md5_state_t md5;

  /* https://www.psdevwiki.com/psp/PBP
   * A PBP file is an archive containing the PARAM.SFO, primary executable, and a bunch of metadata.
   * While we could extract the PARAM.SFO and primary executable to mimic the normal PSP hashing logic,
   * it's easier to just hash the entire file. This also helps alleviate issues where the primary
   * executable is just a game engine and the only differentiating data would be the metadata. */
  if (rc_path_compare_extension(iterator->path, "pbp"))
    return rc_hash_whole_file(hash, iterator);

  track_handle = rc_cd_open_track(iterator, 1);
  if (!track_handle)
    return rc_hash_iterator_error(iterator, "Could not open track");

  /* http://www.romhacking.net/forum/index.php?topic=30899.0
   * PSP_GAME/PARAM.SFO contains key/value pairs identifying the game for the system (i.e. serial number,
   * name, version). PSP_GAME/SYSDIR/EBOOT.BIN is the encrypted primary executable.
   */
  sector = rc_cd_find_file_sector(iterator, track_handle, "PSP_GAME\\PARAM.SFO", &size);
  if (!sector)
  {
    rc_cd_close_track(iterator, track_handle);
    return rc_hash_iterator_error(iterator, "Not a PSP game disc");
  }

  md5_init(&md5);
  if (!rc_hash_cd_file(&md5, iterator, track_handle, sector, NULL, size, "PSP_GAME\\PARAM.SFO"))
  {
    rc_cd_close_track(iterator, track_handle);
    return 0;
  }

  sector = rc_cd_find_file_sector(iterator, track_handle, "PSP_GAME\\SYSDIR\\EBOOT.BIN", &size);
  if (!sector)
  {
    rc_cd_close_track(iterator, track_handle);
    return rc_hash_iterator_error(iterator, "Could not find primary executable");
  }

  if (!rc_hash_cd_file(&md5, iterator, track_handle, sector, NULL, size, "PSP_GAME\\SYSDIR\\EBOOT.BIN"))
  {
    rc_cd_close_track(iterator, track_handle);
    return 0;
  }

  rc_cd_close_track(iterator, track_handle);
  return rc_hash_finalize(iterator, &md5, hash);
}

static int rc_hash_sega_cd(char hash[33], const rc_hash_iterator_t* iterator)
{
  uint8_t buffer[512];
  void* track_handle;

  track_handle = rc_cd_open_track(iterator, 1);
  if (!track_handle)
    return rc_hash_iterator_error(iterator, "Could not open track");

  /* the first 512 bytes of sector 0 are a volume header and ROM header that uniquely identify the game.
   * After that is an arbitrary amount of code that ensures the game is being run in the correct region.
   * Then more arbitrary code follows that actually starts the boot process. Somewhere in there, the
   * primary executable is loaded. In many cases, a single game will have multiple executables, so even
   * if we could determine the primary one, it's just the tip of the iceberg. As such, we've decided that
   * hashing the volume and ROM headers is sufficient for identifying the game, and we'll have to trust
   * that our players aren't modifying anything else on the disc.
   */
  rc_cd_read_sector(iterator, track_handle, 0, buffer, sizeof(buffer));
  rc_cd_close_track(iterator, track_handle);

  if (memcmp(buffer, "SEGADISCSYSTEM  ", 16) != 0 && /* Sega CD */
      memcmp(buffer, "SEGA SEGASATURN ", 16) != 0)   /* Sega Saturn */
  {
    return rc_hash_iterator_error(iterator, "Not a Sega CD");
  }

  return rc_hash_buffer(hash, buffer, sizeof(buffer), iterator);
}

struct rc_buffered_file
{
  const uint8_t* read_ptr;
  const uint8_t* data;
  size_t data_size;
};

static struct rc_buffered_file rc_buffered_file;

static void* rc_file_open_buffered_file(const char* path)
{
  struct rc_buffered_file* handle = (struct rc_buffered_file*)malloc(sizeof(struct rc_buffered_file));
  (void)path;

  if (handle)
    memcpy(handle, &rc_buffered_file, sizeof(rc_buffered_file));

  return handle;
}

static void rc_file_seek_buffered_file(void* file_handle, int64_t offset, int origin)
{
  struct rc_buffered_file* buffered_file = (struct rc_buffered_file*)file_handle;
  switch (origin)
  {
    case SEEK_SET: buffered_file->read_ptr = buffered_file->data + offset; break;
    case SEEK_CUR: buffered_file->read_ptr += offset; break;
    case SEEK_END: buffered_file->read_ptr = buffered_file->data + buffered_file->data_size + offset; break;
  }

  if (buffered_file->read_ptr < buffered_file->data)
    buffered_file->read_ptr = buffered_file->data;
  else if (buffered_file->read_ptr > buffered_file->data + buffered_file->data_size)
    buffered_file->read_ptr = buffered_file->data + buffered_file->data_size;
}

static int64_t rc_file_tell_buffered_file(void* file_handle)
{
  struct rc_buffered_file* buffered_file = (struct rc_buffered_file*)file_handle;
  return (buffered_file->read_ptr - buffered_file->data);
}

static size_t rc_file_read_buffered_file(void* file_handle, void* buffer, size_t requested_bytes)
{
  struct rc_buffered_file* buffered_file = (struct rc_buffered_file*)file_handle;
  const int64_t remaining = buffered_file->data_size - (buffered_file->read_ptr - buffered_file->data);
  if ((int)requested_bytes > remaining)
     requested_bytes = (int)remaining;

  memcpy(buffer, buffered_file->read_ptr, requested_bytes);
  buffered_file->read_ptr += requested_bytes;
  return requested_bytes;
}

static void rc_file_close_buffered_file(void* file_handle)
{
  free(file_handle);
}

static int rc_hash_file_from_buffer(char hash[33], uint32_t console_id, const rc_hash_iterator_t* iterator)
{
  int result;

  rc_hash_iterator_t buffered_file_iterator;
  memset(&buffered_file_iterator, 0, sizeof(buffered_file_iterator));
  memcpy(&buffered_file_iterator.callbacks, &iterator->callbacks, sizeof(iterator->callbacks));

  buffered_file_iterator.callbacks.filereader.open = rc_file_open_buffered_file;
  buffered_file_iterator.callbacks.filereader.close = rc_file_close_buffered_file;
  buffered_file_iterator.callbacks.filereader.read = rc_file_read_buffered_file;
  buffered_file_iterator.callbacks.filereader.seek = rc_file_seek_buffered_file;
  buffered_file_iterator.callbacks.filereader.tell = rc_file_tell_buffered_file;
  buffered_file_iterator.path = "memory stream";

  rc_buffered_file.data = rc_buffered_file.read_ptr = iterator->buffer;
  rc_buffered_file.data_size = iterator->buffer_size;

  result = rc_hash_from_file(hash, console_id, &buffered_file_iterator);

  buffered_file_iterator.path = NULL;
  rc_hash_destroy_iterator(&buffered_file_iterator);
  return result;
}

static int rc_hash_from_buffer(char hash[33], uint32_t console_id, const rc_hash_iterator_t* iterator)
{
  switch (console_id)
  {
    default:
      return rc_hash_iterator_error_formatted(iterator, "Unsupported console for buffer hash: %d", console_id);

    case RC_CONSOLE_AMSTRAD_PC:
    case RC_CONSOLE_APPLE_II:
    case RC_CONSOLE_ARCADIA_2001:
    case RC_CONSOLE_ATARI_2600:
    case RC_CONSOLE_ATARI_JAGUAR:
    case RC_CONSOLE_COLECOVISION:
    case RC_CONSOLE_COMMODORE_64:
    case RC_CONSOLE_ELEKTOR_TV_GAMES_COMPUTER:
    case RC_CONSOLE_FAIRCHILD_CHANNEL_F:
    case RC_CONSOLE_GAMEBOY:
    case RC_CONSOLE_GAMEBOY_ADVANCE:
    case RC_CONSOLE_GAMEBOY_COLOR:
    case RC_CONSOLE_GAME_GEAR:
    case RC_CONSOLE_INTELLIVISION:
    case RC_CONSOLE_INTERTON_VC_4000:
    case RC_CONSOLE_MAGNAVOX_ODYSSEY2:
    case RC_CONSOLE_MASTER_SYSTEM:
    case RC_CONSOLE_MEGA_DRIVE:
    case RC_CONSOLE_MEGADUCK:
    case RC_CONSOLE_MSX:
    case RC_CONSOLE_NEOGEO_POCKET:
    case RC_CONSOLE_ORIC:
    case RC_CONSOLE_PC8800:
    case RC_CONSOLE_POKEMON_MINI:
    case RC_CONSOLE_SEGA_32X:
    case RC_CONSOLE_SG1000:
    case RC_CONSOLE_SUPERVISION:
    case RC_CONSOLE_TI83:
    case RC_CONSOLE_TIC80:
    case RC_CONSOLE_UZEBOX:
    case RC_CONSOLE_VECTREX:
    case RC_CONSOLE_VIRTUAL_BOY:
    case RC_CONSOLE_WASM4:
    case RC_CONSOLE_WONDERSWAN:
    case RC_CONSOLE_ZX_SPECTRUM:
      return rc_hash_buffer(hash, iterator->buffer, iterator->buffer_size, iterator);

#ifndef RC_HASH_NO_ROM
    case RC_CONSOLE_ARDUBOY:
      /* https://en.wikipedia.org/wiki/Intel_HEX */
      return rc_hash_text(hash, iterator);

    case RC_CONSOLE_ATARI_7800:
      return rc_hash_7800(hash, iterator);

    case RC_CONSOLE_ATARI_LYNX:
      return rc_hash_lynx(hash, iterator);

    case RC_CONSOLE_FAMICOM_DISK_SYSTEM:
    case RC_CONSOLE_NINTENDO:
      return rc_hash_nes(hash, iterator);

    case RC_CONSOLE_PC_ENGINE: /* NOTE: does not support PCEngine CD */
      return rc_hash_pce(hash, iterator);

    case RC_CONSOLE_SUPER_CASSETTEVISION:
      return rc_hash_scv(hash, iterator);

    case RC_CONSOLE_SUPER_NINTENDO:
      return rc_hash_snes(hash, iterator);
#endif

    case RC_CONSOLE_NINTENDO_64:
    case RC_CONSOLE_NINTENDO_3DS:
    case RC_CONSOLE_NINTENDO_DS:
    case RC_CONSOLE_NINTENDO_DSI:
      return rc_hash_file_from_buffer(hash, console_id, iterator);
  }
}

int rc_hash_whole_file(char hash[33], const rc_hash_iterator_t* iterator)
{
  md5_state_t md5;
  uint8_t* buffer;
  int64_t size;
  const size_t buffer_size = 65536;
  void* file_handle;
  size_t remaining;
  int result = 0;

  file_handle = rc_file_open(iterator, iterator->path);
  if (!file_handle)
    return rc_hash_iterator_error(iterator, "Could not open file");

  rc_file_seek(iterator, file_handle, 0, SEEK_END);
  size = rc_file_tell(iterator, file_handle);

  if (size > MAX_BUFFER_SIZE) {
    rc_hash_iterator_verbose_formatted(iterator, "Hashing first %u bytes (of %u bytes) of %s", MAX_BUFFER_SIZE, (unsigned)size, rc_path_get_filename(iterator->path));
    remaining = MAX_BUFFER_SIZE;
  }
  else {
    rc_hash_iterator_verbose_formatted(iterator, "Hashing %s (%u bytes)", rc_path_get_filename(iterator->path), (unsigned)size);
    remaining = (size_t)size;
  }

  md5_init(&md5);

  buffer = (uint8_t*)malloc(buffer_size);
  if (buffer) {
    rc_file_seek(iterator, file_handle, 0, SEEK_SET);
    while (remaining >= buffer_size) {
      rc_file_read(iterator, file_handle, buffer, (int)buffer_size);
      md5_append(&md5, buffer, (int)buffer_size);
      remaining -= buffer_size;
    }

    if (remaining > 0) {
      rc_file_read(iterator, file_handle, buffer, (int)remaining);
      md5_append(&md5, buffer, (int)remaining);
    }

    free(buffer);
    result = rc_hash_finalize(iterator, &md5, hash);
  }

  rc_file_close(iterator, file_handle);
  return result;
}

static int rc_hash_buffered_file(char hash[33], uint32_t console_id, const rc_hash_iterator_t* iterator)
{
  uint8_t* buffer;
  int64_t size;
  int result = 0;
  void* file_handle;

  file_handle = rc_file_open(iterator, iterator->path);
  if (!file_handle)
    return rc_hash_iterator_error(iterator, "Could not open file");

  rc_file_seek(iterator, file_handle, 0, SEEK_END);
  size = rc_file_tell(iterator, file_handle);

  if (size > MAX_BUFFER_SIZE) {
    rc_hash_iterator_verbose_formatted(iterator, "Buffering first %u bytes (of %d bytes) of %s", MAX_BUFFER_SIZE, (unsigned)size, rc_path_get_filename(iterator->path));
    size = MAX_BUFFER_SIZE;
  }
  else {
    rc_hash_iterator_verbose_formatted(iterator, "Buffering %s (%d bytes)", rc_path_get_filename(iterator->path), (unsigned)size);
  }

  buffer = (uint8_t*)malloc((size_t)size);
  if (buffer) {
    rc_hash_iterator_t buffer_iterator;
    memset(&buffer_iterator, 0, sizeof(buffer_iterator));
    memcpy(&buffer_iterator.callbacks, &iterator->callbacks, sizeof(iterator->callbacks));
    buffer_iterator.buffer = buffer;
    buffer_iterator.buffer_size = (size_t)size;

    rc_file_seek(iterator, file_handle, 0, SEEK_SET);
    rc_file_read(iterator, file_handle, buffer, (int)size);

    result = rc_hash_from_buffer(hash, console_id, &buffer_iterator);

    free(buffer);
  }

  rc_file_close(iterator, file_handle);
  return result;
}

static int rc_hash_path_is_absolute(const char* path)
{
  if (!path[0])
    return 0;

  /* "/path/to/file" or "\path\to\file" */
  if (path[0] == '/' || path[0] == '\\')
    return 1;

  /* "C:\path\to\file" */
  if (path[1] == ':' && path[2] == '\\')
    return 1;

  /* "scheme:/path/to/file" */
  while (*path)
  {
    if (path[0] == ':' && path[1] == '/')
      return 1;

    ++path;
  }

  return 0;
}

static const char* rc_hash_get_first_item_from_playlist(const rc_hash_iterator_t* iterator) {
  char buffer[1024];
  char* disc_path;
  char* ptr, *start, *next;
  size_t num_read, path_len, file_len;
  void* file_handle;

  file_handle = rc_file_open(iterator, iterator->path);
  if (!file_handle) {
    rc_hash_iterator_error(iterator, "Could not open playlist");
    return NULL;
  }

  num_read = rc_file_read(iterator, file_handle, buffer, sizeof(buffer) - 1);
  buffer[num_read] = '\0';

  rc_file_close(iterator, file_handle);

  ptr = start = buffer;
  do {
    /* ignore empty and commented lines */
    while (*ptr == '#' || *ptr == '\r' || *ptr == '\n') {
      while (*ptr && *ptr != '\n')
        ++ptr;
      if (*ptr)
        ++ptr;
    }

    /* find and extract the current line */
    start = ptr;
    while (*ptr && *ptr != '\n')
      ++ptr;
    next = ptr;

    /* remove trailing whitespace - especially '\r' */
    while (ptr > start && isspace((unsigned char)ptr[-1]))
      --ptr;

    /* if we found a non-empty line, break out of the loop to process it */
    file_len = ptr - start;
    if (file_len)
      break;

    /* did we reach the end of the file? */
    if (!*next)
      return NULL;

    /* if the line only contained whitespace, keep searching */
    ptr = next + 1;
  } while (1);

  rc_hash_iterator_verbose_formatted(iterator, "Extracted %.*s from playlist", (int)file_len, start);

  start[file_len++] = '\0';
  if (rc_hash_path_is_absolute(start))
    path_len = 0;
  else
    path_len = rc_path_get_filename(iterator->path) - iterator->path;

  disc_path = (char*)malloc(path_len + file_len + 1);
  if (!disc_path)
    return NULL;

  if (path_len)
    memcpy(disc_path, iterator->path, path_len);

  memcpy(&disc_path[path_len], start, file_len);
  return disc_path;
}

static int rc_hash_generate_from_playlist(char hash[33], uint32_t console_id, const rc_hash_iterator_t* iterator) {
  rc_hash_iterator_t first_file_iterator;
  const char* disc_path;
  int result;

  rc_hash_iterator_verbose_formatted(iterator, "Processing playlist: %s", rc_path_get_filename(iterator->path));

  disc_path = rc_hash_get_first_item_from_playlist(iterator);
  if (!disc_path)
    return rc_hash_iterator_error(iterator, "Failed to get first item from playlist");

  memset(&first_file_iterator, 0, sizeof(first_file_iterator));
  memcpy(&first_file_iterator.callbacks, &iterator->callbacks, sizeof(iterator->callbacks));
  first_file_iterator.path = disc_path; /* rc_hash_destory_iterator will free */

  result = rc_hash_from_file(hash, console_id, &first_file_iterator);

  rc_hash_destroy_iterator(&first_file_iterator);
  return result;
}

static int rc_hash_from_file(char hash[33], uint32_t console_id, const rc_hash_iterator_t* iterator)
{
  const char* path = iterator->path;

  switch (console_id) {
    default:
      return rc_hash_iterator_error_formatted(iterator, "Unsupported console for file hash: %d", console_id);

    case RC_CONSOLE_ARCADIA_2001:
    case RC_CONSOLE_ATARI_2600:
    case RC_CONSOLE_ATARI_JAGUAR:
    case RC_CONSOLE_COLECOVISION:
    case RC_CONSOLE_ELEKTOR_TV_GAMES_COMPUTER:
    case RC_CONSOLE_FAIRCHILD_CHANNEL_F:
    case RC_CONSOLE_GAMEBOY:
    case RC_CONSOLE_GAMEBOY_ADVANCE:
    case RC_CONSOLE_GAMEBOY_COLOR:
    case RC_CONSOLE_GAME_GEAR:
    case RC_CONSOLE_INTELLIVISION:
    case RC_CONSOLE_INTERTON_VC_4000:
    case RC_CONSOLE_MAGNAVOX_ODYSSEY2:
    case RC_CONSOLE_MASTER_SYSTEM:
    case RC_CONSOLE_MEGADUCK:
    case RC_CONSOLE_NEOGEO_POCKET:
    case RC_CONSOLE_ORIC:
    case RC_CONSOLE_POKEMON_MINI:
    case RC_CONSOLE_SEGA_32X:
    case RC_CONSOLE_SG1000:
    case RC_CONSOLE_SUPERVISION:
    case RC_CONSOLE_TI83:
    case RC_CONSOLE_TIC80:
    case RC_CONSOLE_UZEBOX:
    case RC_CONSOLE_VECTREX:
    case RC_CONSOLE_VIRTUAL_BOY:
    case RC_CONSOLE_WASM4:
    case RC_CONSOLE_WONDERSWAN:
    case RC_CONSOLE_ZX_SPECTRUM:
      /* generic whole-file hash - don't buffer */
      return rc_hash_whole_file(hash, iterator);

    case RC_CONSOLE_MEGA_DRIVE:
      /* generic whole-file hash with m3u support - don't buffer */
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, iterator);

      return rc_hash_whole_file(hash, iterator);

    case RC_CONSOLE_ARDUBOY:
    case RC_CONSOLE_ATARI_7800:
    case RC_CONSOLE_ATARI_LYNX:
    case RC_CONSOLE_FAMICOM_DISK_SYSTEM:
    case RC_CONSOLE_NINTENDO:
    case RC_CONSOLE_PC_ENGINE:
    case RC_CONSOLE_SUPER_CASSETTEVISION:
    case RC_CONSOLE_SUPER_NINTENDO:
      /* additional logic whole-file hash - buffer then call rc_hash_generate_from_buffer */
      return rc_hash_buffered_file(hash, console_id, iterator);

    case RC_CONSOLE_AMSTRAD_PC:
    case RC_CONSOLE_APPLE_II:
    case RC_CONSOLE_COMMODORE_64:
    case RC_CONSOLE_MSX:
    case RC_CONSOLE_PC8800:
      /* generic whole-file hash with m3u support - don't buffer */
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, iterator);

      return rc_hash_whole_file(hash, iterator);

    case RC_CONSOLE_3DO:
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, iterator);

      return rc_hash_3do(hash, iterator);

#ifndef RC_HASH_NO_ROM
    case RC_CONSOLE_ARCADE:
      return rc_hash_arcade(hash, iterator);
#endif

    case RC_CONSOLE_ATARI_JAGUAR_CD:
      return rc_hash_jaguar_cd(hash, iterator);

    case RC_CONSOLE_DREAMCAST:
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, iterator);

      return rc_hash_dreamcast(hash, iterator);

    case RC_CONSOLE_GAMECUBE:
    case RC_CONSOLE_WII:
      return rc_hash_nintendo_disc(hash, iterator);

#ifndef RC_HASH_NO_ZIP
    case RC_CONSOLE_MS_DOS:
      return rc_hash_ms_dos(hash, iterator);
#endif

    case RC_CONSOLE_NEO_GEO_CD:
      return rc_hash_neogeo_cd(hash, iterator);

#ifndef RC_HASH_NO_ROM
    case RC_CONSOLE_NINTENDO_64:
      return rc_hash_n64(hash, iterator);
#endif

#ifndef RC_HASH_NO_ENCRYPTED
    case RC_CONSOLE_NINTENDO_3DS:
      return rc_hash_nintendo_3ds(hash, iterator);
#endif

#ifndef RC_HASH_NO_ROM
    case RC_CONSOLE_NINTENDO_DS:
    case RC_CONSOLE_NINTENDO_DSI:
      return rc_hash_nintendo_ds(hash, iterator);
#endif

    case RC_CONSOLE_PC_ENGINE_CD:
      if (rc_path_compare_extension(path, "cue") || rc_path_compare_extension(path, "chd"))
        return rc_hash_pce_cd(hash, iterator);

      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, iterator);

      return rc_hash_buffered_file(hash, console_id, iterator);

    case RC_CONSOLE_PCFX:
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, iterator);

      return rc_hash_pcfx_cd(hash, iterator);

    case RC_CONSOLE_PLAYSTATION:
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, iterator);

      return rc_hash_psx(hash, iterator);

    case RC_CONSOLE_PLAYSTATION_2:
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, iterator);

      return rc_hash_ps2(hash, iterator);

    case RC_CONSOLE_PSP:
      return rc_hash_psp(hash, iterator);

    case RC_CONSOLE_SEGA_CD:
    case RC_CONSOLE_SATURN:
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, iterator);

      return rc_hash_sega_cd(hash, iterator);
  }
}

static void rc_hash_initialize_iterator_from_path(rc_hash_iterator_t* iterator, const char* path);

static void rc_hash_iterator_append_console(struct rc_hash_iterator* iterator, uint8_t console_id) {
  int i = 0;
  while (iterator->consoles[i] != 0) {
    if (iterator->consoles[i] == console_id)
      return;

    ++i;
  }

  iterator->consoles[i] = console_id;
}

static void rc_hash_reset_iterator(rc_hash_iterator_t* iterator) {
  memset(iterator, 0, sizeof(*iterator));

  iterator->callbacks.verbose_message = g_verbose_message_callback;
  iterator->callbacks.error_message = g_error_message_callback;

  if (g_filereader) {
    memcpy(&iterator->callbacks.filereader, g_filereader, sizeof(*g_filereader));
  } else if (!iterator->callbacks.filereader.open) {
    iterator->callbacks.filereader.open = filereader_open;
    iterator->callbacks.filereader.close = filereader_close;
    iterator->callbacks.filereader.seek = filereader_seek;
    iterator->callbacks.filereader.tell = filereader_tell;
    iterator->callbacks.filereader.read = filereader_read;
  }

  if (g_cdreader)
    memcpy(&iterator->callbacks.cdreader, g_cdreader, sizeof(*g_cdreader));
  else
    rc_hash_get_default_cdreader(&iterator->callbacks.cdreader);

#ifndef RC_HASH_NO_ENCRYPTED
  iterator->callbacks.encryption.get_3ds_cia_normal_key = _3ds_get_cia_normal_key_func;
  iterator->callbacks.encryption.get_3ds_ncch_normal_keys = _3ds_get_ncch_normal_keys_func;
#endif
}

static void rc_hash_initialize_iterator_single(rc_hash_iterator_t* iterator, const char* path, int data) {
  (void)path;
  iterator->consoles[0] = (uint8_t)data;
}

static void rc_hash_initialize_iterator_single_with_path(rc_hash_iterator_t* iterator, const char* path, int data) {
  iterator->consoles[0] = (uint8_t)data;

  if (!iterator->path)
    iterator->path = strdup(path);
}

static void rc_hash_initialize_iterator_bin(rc_hash_iterator_t* iterator, const char* path, int data) {
  (void)data;

  if (iterator->buffer_size == 0) {
    /* raw bin file may be a CD track. if it's more than 32MB, try a CD hash. */
    const int64_t size = rc_file_size(iterator, path);
    if (size > 32 * 1024 * 1024) {
      iterator->consoles[0] = RC_CONSOLE_3DO; /* 4DO supports directly opening the bin file */
      iterator->consoles[1] = RC_CONSOLE_PLAYSTATION; /* PCSX ReARMed supports directly opening the bin file*/
      iterator->consoles[2] = RC_CONSOLE_PLAYSTATION_2; /* PCSX2 supports directly opening the bin file*/
      iterator->consoles[3] = RC_CONSOLE_SEGA_CD; /* Genesis Plus GX supports directly opening the bin file*/

      /* fallback to megadrive which just does a full hash. */
      iterator->consoles[4] = RC_CONSOLE_MEGA_DRIVE;
      return;
    }
  }

  /* bin is associated with MegaDrive, Sega32X, Atari 2600, Watara Supervision, MegaDuck,
   * Fairchild Channel F, Arcadia 2001, Interton VC 4000, and Super Cassette Vision.
   * Since they all use the same hashing algorithm, only specify one of them */
  iterator->consoles[0] = RC_CONSOLE_MEGA_DRIVE;
}

static void rc_hash_initialize_iterator_chd(rc_hash_iterator_t* iterator, const char* path, int data) {
  (void)data;

  iterator->consoles[0] = RC_CONSOLE_PLAYSTATION;
  iterator->consoles[1] = RC_CONSOLE_PLAYSTATION_2;
  iterator->consoles[2] = RC_CONSOLE_DREAMCAST;
  iterator->consoles[3] = RC_CONSOLE_SEGA_CD; /* ASSERT: handles both Sega CD and Saturn */
  iterator->consoles[4] = RC_CONSOLE_PSP;
  iterator->consoles[5] = RC_CONSOLE_PC_ENGINE_CD;
  iterator->consoles[6] = RC_CONSOLE_3DO;
  iterator->consoles[7] = RC_CONSOLE_NEO_GEO_CD;
  iterator->consoles[8] = RC_CONSOLE_PCFX;

  if (!iterator->path)
    iterator->path = strdup(path);
}

static void rc_hash_initialize_iterator_cue(rc_hash_iterator_t* iterator, const char* path, int data) {
  (void)data;

  iterator->consoles[0] = RC_CONSOLE_PLAYSTATION;
  iterator->consoles[1] = RC_CONSOLE_PLAYSTATION_2;
  iterator->consoles[2] = RC_CONSOLE_DREAMCAST;
  iterator->consoles[3] = RC_CONSOLE_SEGA_CD; /* ASSERT: handles both Sega CD and Saturn */
  iterator->consoles[4] = RC_CONSOLE_PC_ENGINE_CD;
  iterator->consoles[5] = RC_CONSOLE_3DO;
  iterator->consoles[6] = RC_CONSOLE_PCFX;
  iterator->consoles[7] = RC_CONSOLE_NEO_GEO_CD;
  iterator->consoles[8] = RC_CONSOLE_ATARI_JAGUAR_CD;

  if (!iterator->path)
    iterator->path = strdup(path);
}

static void rc_hash_initialize_iterator_d88(rc_hash_iterator_t* iterator, const char* path, int data) {
  (void)path;
  (void)data;

  iterator->consoles[0] = RC_CONSOLE_PC8800;
  iterator->consoles[1] = RC_CONSOLE_SHARPX1;
}

static void rc_hash_initialize_iterator_dsk(rc_hash_iterator_t* iterator, const char* path, int data) {
  size_t size = iterator->buffer_size;
  if (size == 0)
    size = (size_t)rc_file_size(iterator, path);

  (void)data;

  if (size == 512 * 9 * 80) { /* 360KB */
    /* FAT-12 3.5" DD (512 byte sectors, 9 sectors per track, 80 tracks per side */
    /* FAT-12 5.25" DD double-sided (512 byte sectors, 9 sectors per track, 80 tracks per side */
    iterator->consoles[0] = RC_CONSOLE_MSX;
  }
  else if (size == 512 * 9 * 80 * 2) { /* 720KB */
    /* FAT-12 3.5" DD double-sided (512 byte sectors, 9 sectors per track, 80 tracks per side */
    iterator->consoles[0] = RC_CONSOLE_MSX;
  }
  else if (size == 512 * 9 * 40) { /* 180KB */
    /* FAT-12 5.25" DD (512 byte sectors, 9 sectors per track, 40 tracks per side */
    iterator->consoles[0] = RC_CONSOLE_MSX;

    /* AMSDOS 3" - 40 tracks */
    iterator->consoles[1] = RC_CONSOLE_AMSTRAD_PC;
  }
  else if (size == 256 * 16 * 35) { /* 140KB */
    /* Apple II new format - 256 byte sectors, 16 sectors per track, 35 tracks per side */
    iterator->consoles[0] = RC_CONSOLE_APPLE_II;
  }
  else if (size == 256 * 13 * 35) { /* 113.75KB */
    /* Apple II old format - 256 byte sectors, 13 sectors per track, 35 tracks per side */
    iterator->consoles[0] = RC_CONSOLE_APPLE_II;
  }

  /* once a best guess has been identified, make sure the others are added as fallbacks */

  /* check MSX first, as Apple II isn't supported by RetroArch, and RAppleWin won't use the iterator */
  rc_hash_iterator_append_console(iterator, RC_CONSOLE_MSX);
  rc_hash_iterator_append_console(iterator, RC_CONSOLE_AMSTRAD_PC);
  rc_hash_iterator_append_console(iterator, RC_CONSOLE_ZX_SPECTRUM);
  rc_hash_iterator_append_console(iterator, RC_CONSOLE_APPLE_II);
}

static void rc_hash_initialize_iterator_iso(rc_hash_iterator_t* iterator, const char* path, int data) {
  (void)data;

  iterator->consoles[0] = RC_CONSOLE_PLAYSTATION_2;
  iterator->consoles[1] = RC_CONSOLE_PSP;
  iterator->consoles[2] = RC_CONSOLE_3DO;
  iterator->consoles[3] = RC_CONSOLE_SEGA_CD; /* ASSERT: handles both Sega CD and Saturn */
  iterator->consoles[4] = RC_CONSOLE_GAMECUBE;

  if (!iterator->path)
    iterator->path = strdup(path);
}

static void rc_hash_initialize_iterator_m3u(rc_hash_iterator_t* iterator, const char* path, int data) {
  (void)data;

  /* temporarily set the iterator path to the m3u file so we can extract the
   * path of the first disc. rc_hash_get_first_item_from_playlist will return
   * an allocated string or NULL, so rc_hash_destroy_iterator won't get tripped
   * up by the non-allocted value we're about to assign.
   */
  iterator->path = path;
  iterator->path = rc_hash_get_first_item_from_playlist(iterator);
  if (!iterator->path) /* did not find a disc */
    return;

  iterator->buffer = NULL; /* ignore buffer; assume it's the m3u contents */

  rc_hash_initialize_iterator_from_path(iterator, iterator->path);
}

static void rc_hash_initialize_iterator_nib(rc_hash_iterator_t* iterator, const char* path, int data) {
  (void)path;
  (void)data;

  iterator->consoles[0] = RC_CONSOLE_APPLE_II;
  iterator->consoles[1] = RC_CONSOLE_COMMODORE_64;
}

static void rc_hash_initialize_iterator_rom(rc_hash_iterator_t* iterator, const char* path, int data) {
  (void)path;
  (void)data;

  /* rom is associated with MSX, Thomson TO-8, and Fairchild Channel F.
   * Since they all use the same hashing algorithm, only specify one of them */
  iterator->consoles[0] = RC_CONSOLE_MSX;
}

static void rc_hash_initialize_iterator_tap(rc_hash_iterator_t* iterator, const char* path, int data) {
  (void)path;
  (void)data;

  /* also Oric and ZX Spectrum, but all are full file hashes */
  iterator->consoles[0] = RC_CONSOLE_COMMODORE_64;
}

static const rc_hash_iterator_ext_handler_entry_t rc_hash_iterator_ext_handlers[] = {
  { "2d", rc_hash_initialize_iterator_single, RC_CONSOLE_SHARPX1 },
  { "3ds", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_3DS },
  { "3dsx", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_3DS },
  { "7z", rc_hash_initialize_iterator_single_with_path, RC_CONSOLE_ARCADE },
  { "83g", rc_hash_initialize_iterator_single, RC_CONSOLE_TI83 }, /* http://tibasicdev.wikidot.com/file-extensions */
  { "83p", rc_hash_initialize_iterator_single, RC_CONSOLE_TI83 },
  { "a26", rc_hash_initialize_iterator_single, RC_CONSOLE_ATARI_2600 },
  { "a78", rc_hash_initialize_iterator_single, RC_CONSOLE_ATARI_7800 },
  { "app", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_3DS },
  { "axf", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_3DS },
  { "bin", rc_hash_initialize_iterator_bin, 0 },
  { "bs", rc_hash_initialize_iterator_single, RC_CONSOLE_SUPER_NINTENDO },
  { "cart", rc_hash_initialize_iterator_single, RC_CONSOLE_SUPER_CASSETTEVISION },
  { "cas", rc_hash_initialize_iterator_single, RC_CONSOLE_MSX },
  { "cci", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_3DS },
  { "chd", rc_hash_initialize_iterator_chd, 0 },
  { "chf", rc_hash_initialize_iterator_single, RC_CONSOLE_FAIRCHILD_CHANNEL_F },
  { "cia", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_3DS },
  { "col", rc_hash_initialize_iterator_single, RC_CONSOLE_COLECOVISION },
  { "csw", rc_hash_initialize_iterator_single, RC_CONSOLE_ZX_SPECTRUM },
  { "cue", rc_hash_initialize_iterator_cue, 0 },
  { "cxi", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_3DS },
  { "d64", rc_hash_initialize_iterator_single, RC_CONSOLE_COMMODORE_64 },
  { "d88", rc_hash_initialize_iterator_d88, 0 },
  { "dosz", rc_hash_initialize_iterator_single, RC_CONSOLE_MS_DOS },
  { "dsk", rc_hash_initialize_iterator_dsk, 0 },
  { "elf", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_3DS },
  { "fd", rc_hash_initialize_iterator_single, RC_CONSOLE_THOMSONTO8 },
  { "fds", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO },
  { "fig", rc_hash_initialize_iterator_single, RC_CONSOLE_SUPER_NINTENDO },
  { "gb", rc_hash_initialize_iterator_single, RC_CONSOLE_GAMEBOY },
  { "gba", rc_hash_initialize_iterator_single, RC_CONSOLE_GAMEBOY_ADVANCE },
  { "gbc", rc_hash_initialize_iterator_single, RC_CONSOLE_GAMEBOY_COLOR },
  { "gdi", rc_hash_initialize_iterator_single, RC_CONSOLE_DREAMCAST },
  { "gg", rc_hash_initialize_iterator_single, RC_CONSOLE_GAME_GEAR },
  { "hex", rc_hash_initialize_iterator_single, RC_CONSOLE_ARDUBOY },
  { "iso", rc_hash_initialize_iterator_iso, 0 },
  { "jag", rc_hash_initialize_iterator_single, RC_CONSOLE_ATARI_JAGUAR },
  { "k7", rc_hash_initialize_iterator_single, RC_CONSOLE_THOMSONTO8 }, /* tape */
  { "lnx", rc_hash_initialize_iterator_single, RC_CONSOLE_ATARI_LYNX },
  { "m3u", rc_hash_initialize_iterator_m3u, 0 },
  { "m5", rc_hash_initialize_iterator_single, RC_CONSOLE_THOMSONTO8 }, /* cartridge */
  { "m7", rc_hash_initialize_iterator_single, RC_CONSOLE_THOMSONTO8 }, /* cartridge */
  { "md", rc_hash_initialize_iterator_single, RC_CONSOLE_MEGA_DRIVE },
  { "min", rc_hash_initialize_iterator_single, RC_CONSOLE_POKEMON_MINI },
  { "mx1", rc_hash_initialize_iterator_single, RC_CONSOLE_MSX },
  { "mx2", rc_hash_initialize_iterator_single, RC_CONSOLE_MSX },
  { "n64", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_64 },
  { "ndd", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_64 },
  { "nds", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_DS }, /* handles both DS and DSi */
  { "nes", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO },
  { "ngc", rc_hash_initialize_iterator_single, RC_CONSOLE_NEOGEO_POCKET },
  { "nib", rc_hash_initialize_iterator_nib, 0 },
  { "pbp", rc_hash_initialize_iterator_single, RC_CONSOLE_PSP },
  { "pce", rc_hash_initialize_iterator_single, RC_CONSOLE_PC_ENGINE },
  { "pgm", rc_hash_initialize_iterator_single, RC_CONSOLE_ELEKTOR_TV_GAMES_COMPUTER },
  { "pzx", rc_hash_initialize_iterator_single, RC_CONSOLE_ZX_SPECTRUM },
  { "ri", rc_hash_initialize_iterator_single, RC_CONSOLE_MSX },
  { "rom", rc_hash_initialize_iterator_rom, 0 },
  { "sap", rc_hash_initialize_iterator_single, RC_CONSOLE_THOMSONTO8 }, /* disk */
  { "scl", rc_hash_initialize_iterator_single, RC_CONSOLE_ZX_SPECTRUM },
  { "sfc", rc_hash_initialize_iterator_single, RC_CONSOLE_SUPER_NINTENDO },
  { "sg", rc_hash_initialize_iterator_single, RC_CONSOLE_SG1000 },
  { "sgx", rc_hash_initialize_iterator_single, RC_CONSOLE_PC_ENGINE },
  { "smc", rc_hash_initialize_iterator_single, RC_CONSOLE_SUPER_NINTENDO },
  { "sv", rc_hash_initialize_iterator_single, RC_CONSOLE_SUPERVISION },
  { "swc", rc_hash_initialize_iterator_single, RC_CONSOLE_SUPER_NINTENDO },
  { "tap", rc_hash_initialize_iterator_tap, 0 },
  { "tic", rc_hash_initialize_iterator_single, RC_CONSOLE_TIC80 },
  { "trd", rc_hash_initialize_iterator_single, RC_CONSOLE_ZX_SPECTRUM },
  { "tvc", rc_hash_initialize_iterator_single, RC_CONSOLE_ELEKTOR_TV_GAMES_COMPUTER },
  { "tzx", rc_hash_initialize_iterator_single, RC_CONSOLE_ZX_SPECTRUM },
  { "uze", rc_hash_initialize_iterator_single, RC_CONSOLE_UZEBOX },
  { "v64", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_64 },
  { "vb", rc_hash_initialize_iterator_single, RC_CONSOLE_VIRTUAL_BOY },
  { "wasm", rc_hash_initialize_iterator_single, RC_CONSOLE_WASM4 },
  { "woz", rc_hash_initialize_iterator_single, RC_CONSOLE_APPLE_II },
  { "wsc", rc_hash_initialize_iterator_single, RC_CONSOLE_WONDERSWAN },
  { "z64", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_64 },
  { "zip", rc_hash_initialize_iterator_single_with_path, RC_CONSOLE_ARCADE }
};

const rc_hash_iterator_ext_handler_entry_t* rc_hash_get_iterator_ext_handlers(size_t* num_handlers) {
  *num_handlers = sizeof(rc_hash_iterator_ext_handlers) / sizeof(rc_hash_iterator_ext_handlers[0]);
  return rc_hash_iterator_ext_handlers;
}

static int rc_hash_iterator_find_handler(const void* left, const void* right) {
  const rc_hash_iterator_ext_handler_entry_t* left_handler =
    (const rc_hash_iterator_ext_handler_entry_t*)left;
  const rc_hash_iterator_ext_handler_entry_t* right_handler =
    (const rc_hash_iterator_ext_handler_entry_t*)right;

  return strcmp(left_handler->ext, right_handler->ext);
}

static void rc_hash_initialize_iterator_from_path(rc_hash_iterator_t* iterator, const char* path) {
  size_t num_handlers;
  const rc_hash_iterator_ext_handler_entry_t* handlers = rc_hash_get_iterator_ext_handlers(&num_handlers);
  const rc_hash_iterator_ext_handler_entry_t* handler;
  rc_hash_iterator_ext_handler_entry_t search;
  const char* ext = rc_path_get_extension(path);
  size_t index;

  /* lowercase the extension as we copy it into the search object */
  memset(&search, 0, sizeof(search));
  for (index = 0; index < sizeof(search.ext) - 1; ++index) {
    const int c = (int)ext[index];
    if (!c)
      break;

    search.ext[index] = tolower(c);
  }

  /* find the handler for the extension */
  handler = bsearch(&search, handlers, num_handlers, sizeof(*handler), rc_hash_iterator_find_handler);
  if (handler) {
    handler->handler(iterator, path, handler->data);
  } else {
    /* if we didn't match the extension, default to something that does a whole file hash */
    if (!iterator->consoles[0])
      iterator->consoles[0] = RC_CONSOLE_GAMEBOY;
  }
}

void rc_hash_initialize_iterator(rc_hash_iterator_t* iterator, const char* path, const uint8_t* buffer, size_t buffer_size)
{
  rc_hash_reset_iterator(iterator);
  iterator->buffer = buffer;
  iterator->buffer_size = buffer_size;

  rc_hash_initialize_iterator_from_path(iterator, path);

  if (iterator->callbacks.verbose_message) {
    char message[256];
    int count = 0;
    while (iterator->consoles[count])
      ++count;

    snprintf(message, sizeof(message), "Found %d potential consoles for %s file extension", count, rc_path_get_extension(path));
    iterator->callbacks.verbose_message(message);
  }

  if (!iterator->buffer && !iterator->path)
    iterator->path = strdup(path);
}

void rc_hash_destroy_iterator(rc_hash_iterator_t* iterator) {
  if (iterator->path) {
    free((void*)iterator->path);
    iterator->path = NULL;
  }
}

int rc_hash_iterate(char hash[33], rc_hash_iterator_t* iterator) {
  int next_console;
  int result = 0;

  do {
    next_console = iterator->consoles[iterator->index];
    if (next_console == 0) {
      hash[0] = '\0';
      break;
    }

    ++iterator->index;

    rc_hash_iterator_verbose_formatted(iterator, "Trying console %d", next_console);

    result = rc_hash_generate(hash, next_console, iterator);
  } while (!result);

  return result;
}

int rc_hash_generate(char hash[33], uint32_t console_id, const rc_hash_iterator_t* iterator) {
  if (iterator->buffer)
    return rc_hash_from_buffer(hash, console_id, iterator);

  return rc_hash_generate_from_file(hash, console_id, iterator->path);
}

int rc_hash_generate_from_buffer(char hash[33], uint32_t console_id, const uint8_t* buffer, size_t buffer_size) {
  rc_hash_iterator_t iterator;
  int result;

  rc_hash_reset_iterator(&iterator);
  iterator.buffer = buffer;
  iterator.buffer_size = buffer_size;

  result = rc_hash_from_buffer(hash, console_id, &iterator);

  rc_hash_destroy_iterator(&iterator);

  return result;
}

int rc_hash_generate_from_file(char hash[33], uint32_t console_id, const char* path){
  rc_hash_iterator_t iterator;
  int result;

  rc_hash_reset_iterator(&iterator);
  iterator.path = path;

  result = rc_hash_from_file(hash, console_id, &iterator);

  iterator.path = NULL; /* prevent free. we didn't strdup */

  rc_hash_destroy_iterator(&iterator);

  return result;
}
