#include "rc_hash_internal.h"

#include "../rc_compat.h"

#include <ctype.h>

/* ===================================================== */

static int rc_hash_unheadered_iterator_buffer(char hash[33], const rc_hash_iterator_t* iterator, size_t header_size)
{
  return rc_hash_buffer(hash, iterator->buffer + header_size, iterator->buffer_size - header_size, iterator);
}

static int rc_hash_iterator_buffer(char hash[33], const rc_hash_iterator_t* iterator)
{
  return rc_hash_buffer(hash, iterator->buffer, iterator->buffer_size, iterator);
}

/* ===================================================== */

int rc_hash_7800(char hash[33], const rc_hash_iterator_t* iterator)
{
  /* if the file contains a header, ignore it */
  if (memcmp(&iterator->buffer[1], "ATARI7800", 9) == 0) {
    rc_hash_iterator_verbose(iterator, "Ignoring 7800 header");
    return rc_hash_unheadered_iterator_buffer(hash, iterator, 128);
  }

  return rc_hash_iterator_buffer(hash, iterator);
}

int rc_hash_arcade(char hash[33], const rc_hash_iterator_t* iterator)
{
  /* arcade hash is just the hash of the filename (no extension) - the cores are pretty stringent about having the right ROM data */
  const char* filename = rc_path_get_filename(iterator->path);
  const char* ext = rc_path_get_extension(filename);
  char buffer[128]; /* realistically, this should never need more than ~32 characters */
  size_t filename_length = ext - filename - 1;

  /* fbneo supports loading subsystems by using specific folder names.
   * if one is found, include it in the hash.
   * https://github.com/libretro/FBNeo/blob/master/src/burner/libretro/README.md#emulating-consoles-and-computers
   */
  if (filename > iterator->path + 1) {
    int include_folder = 0;
    const char* folder = filename - 1;
    size_t parent_folder_length = 0;

    do {
      if (folder[-1] == '/' || folder[-1] == '\\')
        break;

      --folder;
    } while (folder > iterator->path);

    parent_folder_length = filename - folder - 1;
    if (parent_folder_length < 16) {
      char* ptr = buffer;
      while (folder < filename - 1)
        *ptr++ = tolower(*folder++);
      *ptr = '\0';

      folder = buffer;
    }

    switch (parent_folder_length) {
      case 3:
        if (memcmp(folder, "nes", 3) == 0 || /* NES */
            memcmp(folder, "fds", 3) == 0 || /* FDS */
            memcmp(folder, "sms", 3) == 0 || /* Master System */
            memcmp(folder, "msx", 3) == 0 || /* MSX */
            memcmp(folder, "ngp", 3) == 0 || /* NeoGeo Pocket */
            memcmp(folder, "pce", 3) == 0 || /* PCEngine */
            memcmp(folder, "chf", 3) == 0 || /* ChannelF */
            memcmp(folder, "sgx", 3) == 0)   /* SuperGrafX */
          include_folder = 1;
        break;
      case 4:
        if (memcmp(folder, "tg16", 4) == 0 || /* TurboGrafx-16 */
            memcmp(folder, "msx1", 4) == 0)   /* MSX */
          include_folder = 1;
        break;
      case 5:
        if (memcmp(folder, "neocd", 5) == 0) /* NeoGeo CD */
          include_folder = 1;
        break;
      case 6:
        if (memcmp(folder, "coleco", 6) == 0 || /* Colecovision */
            memcmp(folder, "sg1000", 6) == 0)   /* SG-1000 */
          include_folder = 1;
        break;
      case 7:
        if (memcmp(folder, "genesis", 7) == 0) /* Megadrive (Genesis) */
          include_folder = 1;
        break;
      case 8:
        if (memcmp(folder, "gamegear", 8) == 0 || /* Game Gear */
            memcmp(folder, "megadriv", 8) == 0 || /* Megadrive */
            memcmp(folder, "pcengine", 8) == 0 || /* PCEngine */
            memcmp(folder, "channelf", 8) == 0 || /* ChannelF */
            memcmp(folder, "spectrum", 8) == 0)   /* ZX Spectrum */
          include_folder = 1;
        break;
      case 9:
        if (memcmp(folder, "megadrive", 9) == 0) /* Megadrive */
          include_folder = 1;
        break;
      case 10:
        if (memcmp(folder, "supergrafx", 10) == 0 || /* SuperGrafX */
            memcmp(folder, "zxspectrum", 10) == 0)   /* ZX Spectrum */
          include_folder = 1;
        break;
      case 12:
        if (memcmp(folder, "mastersystem", 12) == 0 || /* Master System */
            memcmp(folder, "colecovision", 12) == 0)   /* Colecovision */
          include_folder = 1;
        break;
      default:
        break;
    }

    if (include_folder) {
      if (parent_folder_length + filename_length + 1 < sizeof(buffer)) {
        buffer[parent_folder_length] = '_';
        memcpy(&buffer[parent_folder_length + 1], filename, filename_length);
        return rc_hash_buffer(hash, (uint8_t*)&buffer[0], parent_folder_length + filename_length + 1, iterator);
      }
    }
  }

  return rc_hash_buffer(hash, (uint8_t*)filename, filename_length, iterator);
}

static int rc_hash_text(char hash[33], const rc_hash_iterator_t* iterator)
{
  md5_state_t md5;
  const uint8_t* scan = iterator->buffer;
  const uint8_t* stop = iterator->buffer + iterator->buffer_size;

  md5_init(&md5);

  do {
    const uint8_t* line = scan;

    /* find end of line */
    while (scan < stop && *scan != '\r' && *scan != '\n')
      ++scan;

    md5_append(&md5, line, (int)(scan - line));

    /* include a normalized line ending */
    /* NOTE: this causes a line ending to be hashed at the end of the file, even if one was not present */
    md5_append(&md5, (const uint8_t*)"\n", 1);

    /* skip newline */
    if (scan < stop && *scan == '\r')
      ++scan;
    if (scan < stop && *scan == '\n')
      ++scan;

  } while (scan < stop);

  return rc_hash_finalize(iterator, &md5, hash);
}

int rc_hash_arduboy(char hash[33], const rc_hash_iterator_t* iterator)
{
  if (iterator->path && rc_path_compare_extension(iterator->path, "arduboy")) {
#ifndef RC_HASH_NO_ZIP
    return rc_hash_arduboyfx(hash, iterator);
#else
    rc_hash_iterator_verbose(iterator, ".arduboy file processing not enabled");
    return 0;
#endif
  }

  if (!iterator->buffer)
    return rc_hash_buffered_file(hash, RC_CONSOLE_ARDUBOY, iterator);

  /* https://en.wikipedia.org/wiki/Intel_HEX */
  return rc_hash_text(hash, iterator);
}

int rc_hash_lynx(char hash[33], const rc_hash_iterator_t* iterator)
{
  /* if the file contains a header, ignore it */
  if (memcmp(&iterator->buffer[0], "LYNX", 5) == 0) {
    rc_hash_iterator_verbose(iterator, "Ignoring LYNX header");
    return rc_hash_unheadered_iterator_buffer(hash, iterator, 64);
  }

  return rc_hash_iterator_buffer(hash, iterator);
}

int rc_hash_nes(char hash[33], const rc_hash_iterator_t* iterator)
{
  /* if the file contains a header, ignore it */
  if (memcmp(&iterator->buffer[0], "NES\x1a", 4) == 0) {
    rc_hash_iterator_verbose(iterator, "Ignoring NES header");
    return rc_hash_unheadered_iterator_buffer(hash, iterator, 16);
  }

  if (memcmp(&iterator->buffer[0], "FDS\x1a", 4) == 0) {
    rc_hash_iterator_verbose(iterator, "Ignoring FDS header");
    return rc_hash_unheadered_iterator_buffer(hash, iterator, 16);
  }

  return rc_hash_iterator_buffer(hash, iterator);
}

int rc_hash_n64(char hash[33], const rc_hash_iterator_t* iterator)
{
  uint8_t* buffer;
  uint8_t* stop;
  const size_t buffer_size = 65536;
  md5_state_t md5;
  size_t remaining;
  void* file_handle;
  int is_v64 = 0;
  int is_n64 = 0;

  file_handle = rc_file_open(iterator, iterator->path);
  if (!file_handle)
    return rc_hash_iterator_error(iterator, "Could not open file");

  buffer = (uint8_t*)malloc(buffer_size);
  if (!buffer) {
    rc_file_close(iterator, file_handle);
    return rc_hash_iterator_error(iterator, "Could not allocate temporary buffer");
  }
  stop = buffer + buffer_size;

  /* read first byte so we can detect endianness */
  rc_file_seek(iterator, file_handle, 0, SEEK_SET);
  rc_file_read(iterator, file_handle, buffer, 1);

  if (buffer[0] == 0x80) { /* z64 format (big endian [native]) */
  }
  else if (buffer[0] == 0x37) { /* v64 format (byteswapped) */
    rc_hash_iterator_verbose(iterator, "converting v64 to z64");
    is_v64 = 1;
  }
  else if (buffer[0] == 0x40) { /* n64 format (little endian) */
    rc_hash_iterator_verbose(iterator, "converting n64 to z64");
    is_n64 = 1;
  }
  else if (buffer[0] == 0xE8 || buffer[0] == 0x22) { /* ndd format (don't byteswap) */
  }
  else {
    free(buffer);
    rc_file_close(iterator, file_handle);

    rc_hash_iterator_verbose(iterator, "Not a Nintendo 64 ROM");
    return 0;
  }

  /* calculate total file size */
  rc_file_seek(iterator, file_handle, 0, SEEK_END);
  remaining = (size_t)rc_file_tell(iterator, file_handle);
  if (remaining > MAX_BUFFER_SIZE)
    remaining = MAX_BUFFER_SIZE;

  rc_hash_iterator_verbose_formatted(iterator, "Hashing %u bytes", (unsigned)remaining);

  /* begin hashing */
  md5_init(&md5);

  rc_file_seek(iterator, file_handle, 0, SEEK_SET);
  while (remaining >= buffer_size) {
    rc_file_read(iterator, file_handle, buffer, (int)buffer_size);

    if (is_v64)
      rc_hash_byteswap16(buffer, stop);
    else if (is_n64)
      rc_hash_byteswap32(buffer, stop);

    md5_append(&md5, buffer, (int)buffer_size);
    remaining -= buffer_size;
  }

  if (remaining > 0) {
    rc_file_read(iterator, file_handle, buffer, (int)remaining);

    stop = buffer + remaining;
    if (is_v64)
      rc_hash_byteswap16(buffer, stop);
    else if (is_n64)
      rc_hash_byteswap32(buffer, stop);

    md5_append(&md5, buffer, (int)remaining);
  }

  /* cleanup */
  rc_file_close(iterator, file_handle);
  free(buffer);

  return rc_hash_finalize(iterator, &md5, hash);
}

int rc_hash_nintendo_ds(char hash[33], const rc_hash_iterator_t* iterator)
{
  uint8_t header[512];
  uint8_t* hash_buffer;
  uint32_t hash_size, arm9_size, arm9_addr, arm7_size, arm7_addr, icon_addr;
  size_t num_read;
  int64_t offset = 0;
  md5_state_t md5;
  void* file_handle;

  file_handle = rc_file_open(iterator, iterator->path);
  if (!file_handle)
    return rc_hash_iterator_error(iterator, "Could not open file");

  rc_file_seek(iterator, file_handle, 0, SEEK_SET);
  if (rc_file_read(iterator, file_handle, header, sizeof(header)) != 512)
    return rc_hash_iterator_error(iterator, "Failed to read header");

  if (header[0] == 0x2E && header[1] == 0x00 && header[2] == 0x00 && header[3] == 0xEA &&
      header[0xB0] == 0x44 && header[0xB1] == 0x46 && header[0xB2] == 0x96 && header[0xB3] == 0) {
    /* SuperCard header detected, ignore it */
    rc_hash_iterator_verbose(iterator, "Ignoring SuperCard header");

    offset = 512;
    rc_file_seek(iterator, file_handle, offset, SEEK_SET);
    rc_file_read(iterator, file_handle, header, sizeof(header));
  }

  arm9_addr = header[0x20] | (header[0x21] << 8) | (header[0x22] << 16) | (header[0x23] << 24);
  arm9_size = header[0x2C] | (header[0x2D] << 8) | (header[0x2E] << 16) | (header[0x2F] << 24);
  arm7_addr = header[0x30] | (header[0x31] << 8) | (header[0x32] << 16) | (header[0x33] << 24);
  arm7_size = header[0x3C] | (header[0x3D] << 8) | (header[0x3E] << 16) | (header[0x3F] << 24);
  icon_addr = header[0x68] | (header[0x69] << 8) | (header[0x6A] << 16) | (header[0x6B] << 24);

  if (arm9_size + arm7_size > 16 * 1024 * 1024) {
    /* sanity check - code blocks are typically less than 1MB each - assume not a DS ROM */
    return rc_hash_iterator_error_formatted(iterator, "arm9 code size (%u) + arm7 code size (%u) exceeds 16MB", arm9_size, arm7_size);
  }

  hash_size = 0xA00;
  if (arm9_size > hash_size)
    hash_size = arm9_size;
  if (arm7_size > hash_size)
    hash_size = arm7_size;

  hash_buffer = (uint8_t*)malloc(hash_size);
  if (!hash_buffer) {
    rc_file_close(iterator, file_handle);
    return rc_hash_iterator_error_formatted(iterator, "Failed to allocate %u bytes", hash_size);
  }

  md5_init(&md5);

  rc_hash_iterator_verbose(iterator, "Hashing 352 byte header");
  md5_append(&md5, header, 0x160);

  rc_hash_iterator_verbose_formatted(iterator, "Hashing %u byte arm9 code (at %08X)", arm9_size, arm9_addr);

  rc_file_seek(iterator, file_handle, arm9_addr + offset, SEEK_SET);
  rc_file_read(iterator, file_handle, hash_buffer, arm9_size);
  md5_append(&md5, hash_buffer, arm9_size);

  rc_hash_iterator_verbose_formatted(iterator, "Hashing %u byte arm7 code (at %08X)", arm7_size, arm7_addr);

  rc_file_seek(iterator, file_handle, arm7_addr + offset, SEEK_SET);
  rc_file_read(iterator, file_handle, hash_buffer, arm7_size);
  md5_append(&md5, hash_buffer, arm7_size);

  rc_hash_iterator_verbose_formatted(iterator, "Hashing 2560 byte icon and labels data (at %08X)", icon_addr);

  rc_file_seek(iterator, file_handle, icon_addr + offset, SEEK_SET);
  num_read = rc_file_read(iterator, file_handle, hash_buffer, 0xA00);
  if (num_read < 0xA00) {
    /* some homebrew games don't provide a full icon block, and no data after the icon block.
     * if we didn't get a full icon block, fill the remaining portion with 0s
     */
    rc_hash_iterator_verbose_formatted(iterator,
      "Warning: only got %u bytes for icon and labels data, 0-padding to 2560 bytes", (unsigned)num_read);

    memset(&hash_buffer[num_read], 0, 0xA00 - num_read);
  }
  md5_append(&md5, hash_buffer, 0xA00);

  free(hash_buffer);
  rc_file_close(iterator, file_handle);

  return rc_hash_finalize(iterator, &md5, hash);
}

int rc_hash_pce(char hash[33], const rc_hash_iterator_t* iterator)
{
  /* The PCE header doesn't bear any distinguishable marks, so we have to detect
   * it by looking at the file size. The core looks for anything that's 512 bytes
   * more than a multiple of 8KB, so we'll do that too.
   * https://github.com/libretro/beetle-pce-libretro/blob/af28fb0385d00e0292c4703b3aa7e72762b564d2/mednafen/pce/huc.cpp#L196-L202
   */
  if (iterator->buffer_size & 512) {
    rc_hash_iterator_verbose(iterator, "Ignoring PCE header");
    return rc_hash_unheadered_iterator_buffer(hash, iterator, 512);
  }

  return rc_hash_iterator_buffer(hash, iterator);
}

int rc_hash_scv(char hash[33], const rc_hash_iterator_t* iterator)
{
  /* if the file contains a header, ignore it */
  /* https://gitlab.com/MaaaX-EmuSCV/libretro-emuscv/-/blob/master/readme.txt#L211 */
  if (memcmp(iterator->buffer, "EmuSCV", 6) == 0) {
    rc_hash_iterator_verbose(iterator, "Ignoring SCV header");
    return rc_hash_unheadered_iterator_buffer(hash, iterator, 32);
  }

  return rc_hash_iterator_buffer(hash, iterator);
}

int rc_hash_snes(char hash[33], const rc_hash_iterator_t* iterator)
{
  /* if the file contains a header, ignore it */
  uint32_t calc_size = ((uint32_t)iterator->buffer_size / 0x2000) * 0x2000;
  if (iterator->buffer_size - calc_size == 512) {
    rc_hash_iterator_verbose(iterator, "Ignoring SNES header");
    return rc_hash_unheadered_iterator_buffer(hash, iterator, 512);
  }

  return rc_hash_iterator_buffer(hash, iterator);
}
