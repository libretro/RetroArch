#include "rc_hash.h"

#include "rc_hash_internal.h"

#include "../rc_compat.h"

#include <ctype.h>

/* ===================================================== */

static struct rc_hash_cdreader g_cdreader_funcs;
static struct rc_hash_cdreader* g_cdreader = NULL;

void rc_hash_reset_iterator_disc(rc_hash_iterator_t* iterator)
{
  if (g_cdreader)
    memcpy(&iterator->callbacks.cdreader, g_cdreader, sizeof(*g_cdreader));
  else
    rc_hash_get_default_cdreader(&iterator->callbacks.cdreader);
}

void rc_hash_init_custom_cdreader(struct rc_hash_cdreader* reader)
{
  if (reader) {
    memcpy(&g_cdreader_funcs, reader, sizeof(g_cdreader_funcs));
    g_cdreader = &g_cdreader_funcs;
  }
  else {
    g_cdreader = NULL;
  }
}

static void* rc_cd_open_track(const rc_hash_iterator_t* iterator, uint32_t track)
{
  if (iterator->callbacks.cdreader.open_track_iterator)
    return iterator->callbacks.cdreader.open_track_iterator(iterator->path, track, iterator);

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
  if (slash) {
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
  else {
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
  do {
    if (tmp >= buffer + sizeof(buffer) || !*tmp) {
      /* end of this path table block. if the path table spans multiple sectors, keep scanning */
      if (num_sectors > 1) {
        --num_sectors;
        if (rc_cd_read_sector(iterator, track_handle, ++sector, buffer, sizeof(buffer))) {
          tmp = buffer;
          continue;
        }
      }
      break;
    }

    /* filename is 33 bytes into the record and the format is "FILENAME;version" or "DIRECTORY" */
    if ((tmp[32] == filename_length || tmp[33 + filename_length] == ';') &&
        strncasecmp((const char*)(tmp + 33), path, filename_length) == 0) {
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

  do {
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

/* ===================================================== */

int rc_hash_3do(char hash[33], const rc_hash_iterator_t* iterator)
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

  if (memcmp(buffer, operafs_identifier, sizeof(operafs_identifier)) == 0) {
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

    do {
      rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer));

      /* offset to start of entries is at offset 0x10 (assume 0x10 and 0x11 are always 0) */
      offset = buffer[0x12] * 256 + buffer[0x13];

      /* offset to end of entries is at offset 0x0C (assume 0x0C is always 0) */
      stop = buffer[0x0D] * 65536 + buffer[0x0E] * 256 + buffer[0x0F];

      while (offset < stop) {
        if (buffer[offset + 0x03] == 0x02) { /* file */
          if (strcasecmp((const char*)&buffer[offset + 0x20], "LaunchMe") == 0) {
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

    if (size == 0) {
      rc_cd_close_track(iterator, track_handle);
      return rc_hash_iterator_error(iterator, "Could not find LaunchMe");
    }

    sector = block_location / 2048;

    while (size > 2048) {
      rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer));
      md5_append(&md5, buffer, sizeof(buffer));

      ++sector;
      size -= 2048;
    }

    rc_cd_read_sector(iterator, track_handle, sector, buffer, size);
    md5_append(&md5, buffer, (int)size);
  }
  else {
    rc_cd_close_track(iterator, track_handle);
    return rc_hash_iterator_error(iterator, "Not a 3DO CD");
  }

  rc_cd_close_track(iterator, track_handle);

  return rc_hash_finalize(iterator, &md5, hash);
}

int rc_hash_dreamcast(char hash[33], const rc_hash_iterator_t* iterator)
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
  if (track_handle) {
    /* first 256 bytes from first sector should have IP.BIN structure that stores game meta information
     * https://mc.pp.se/dc/ip.bin.html */
    rc_cd_read_sector(iterator, track_handle, rc_cd_first_track_sector(iterator, track_handle), buffer, sizeof(buffer));
  }

  if (memcmp(&buffer[0], "SEGA SEGAKATANA ", 16) != 0) {
    if (track_handle)
      rc_cd_close_track(iterator, track_handle);

    /* not a gd-rom dreamcast file. check for mil-cd by looking for the marker in the first data track */
    track_handle = rc_cd_open_track(iterator, RC_HASH_CDTRACK_FIRST_DATA);
    if (!track_handle)
      return rc_hash_iterator_error(iterator, "Could not open track");

    rc_cd_read_sector(iterator, track_handle, rc_cd_first_track_sector(iterator, track_handle), buffer, sizeof(buffer));
    if (memcmp(&buffer[0], "SEGA SEGAKATANA ", 16) != 0) {
      /* did not find marker on track 3 or first data track */
      rc_cd_close_track(iterator, track_handle);
      return rc_hash_iterator_error(iterator, "Not a Dreamcast CD");
    }
  }

  /* start the hash with the game meta information */
  md5_init(&md5);
  md5_append(&md5, (md5_byte_t*)buffer, 256);

  if (iterator->callbacks.verbose_message) {
    uint8_t* ptr = &buffer[0xFF];
    while (ptr > &buffer[0x80] && ptr[-1] == ' ')
      --ptr;
    *ptr = '\0';

    rc_hash_iterator_verbose_formatted(iterator, "Found Dreamcast CD: %.128s (%.16s)", (const char*)&buffer[0x80], (const char*)&buffer[0x40]);
  }

  /* the boot filename is 96 bytes into the meta information (https://mc.pp.se/dc/ip0000.bin.html) */
  /* remove whitespace from bootfile */
  i = 0;
  while (!isspace((unsigned char)buffer[96 + i]) && i < 16)
    ++i;

  /* sometimes boot file isn't present on meta information.
   * nothing can be done, as even the core doesn't run the game in this case. */
  if (i == 0) {
    rc_cd_close_track(iterator, track_handle);
    return rc_hash_iterator_error(iterator, "Boot executable not specified on IP.BIN");
  }

  memcpy(exe_file, &buffer[96], i);
  exe_file[i] = '\0';

  sector = rc_cd_find_file_sector(iterator, track_handle, exe_file, &size);
  if (sector == 0) {
    rc_cd_close_track(iterator, track_handle);
    return rc_hash_iterator_error(iterator, "Could not locate boot executable");
  }

  if (rc_cd_read_sector(iterator, track_handle, sector, buffer, 1)) {
    /* the boot executable is in the primary data track */
  }
  else {
    rc_cd_close_track(iterator, track_handle);

    /* the boot executable is normally in the last track */
    track_handle = rc_cd_open_track(iterator, RC_HASH_CDTRACK_LAST);
  }

  result = rc_hash_cd_file(&md5, iterator, track_handle, sector, NULL, size, "boot executable");
  rc_cd_close_track(iterator, track_handle);

  rc_hash_finalize(iterator, &md5, hash);
  return result;
}

static int rc_hash_nintendo_disc_partition(md5_state_t* md5, const rc_hash_iterator_t* iterator,
  void* file_handle, const uint32_t part_offset, uint8_t wii_shift)
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
    (quad_buffer[0] << 24) | (quad_buffer[1] << 16) | (quad_buffer[2] << 8) | quad_buffer[3];
  rc_file_read(iterator, file_handle, quad_buffer, 4);
  apploader_trailer_size =
    (quad_buffer[0] << 24) | (quad_buffer[1] << 16) | (quad_buffer[2] << 8) | quad_buffer[3];
  header_size = BASE_HEADER_SIZE + apploader_header_size + apploader_body_size + apploader_trailer_size;
  if (header_size > MAX_HEADER_SIZE) header_size = MAX_HEADER_SIZE;

  /* Hash headers */
  buffer = (uint8_t*)malloc(header_size);
  if (!buffer) {
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
  for (ix = 0; ix < 18; ix++) {
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
  if (!buffer) {
    rc_file_close(iterator, file_handle);
    return rc_hash_iterator_error(iterator, "Could not allocate temporary buffer");
  }

  for (ix = 0; ix < 18; ix++) {
    if (dol_sizes[ix] == 0)
      continue;

    rc_file_seek(iterator, file_handle, part_offset + dol_offsets[ix], SEEK_SET);
    if (ix < 7)
      rc_hash_iterator_verbose_formatted(iterator, "Hashing %u byte main.dol code segment %u", dol_sizes[ix], ix);
    else
      rc_hash_iterator_verbose_formatted(iterator, "Hashing %u byte main.dol data segment %u", dol_sizes[ix], ix - 7);

    remaining_size = dol_sizes[ix];
    while (remaining_size > MAX_CHUNK_SIZE) {
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

int rc_hash_gamecube(char hash[33], const rc_hash_iterator_t* iterator)
{
  md5_state_t md5;
  void* file_handle;

  uint8_t quad_buffer[4];
  uint8_t success;

  file_handle = rc_file_open(iterator, iterator->path);
  if (!file_handle)
    return rc_hash_iterator_error(iterator, "Could not open file");

  md5_init(&md5);
  /* Check Magic Word */
  rc_file_seek(iterator, file_handle, 0x1c, SEEK_SET);
  rc_file_read(iterator, file_handle, quad_buffer, 4);
  if (quad_buffer[0] == 0xC2 && quad_buffer[1] == 0x33 && quad_buffer[2] == 0x9F && quad_buffer[3] == 0x3D)
    success = rc_hash_nintendo_disc_partition(&md5, iterator, file_handle, 0, 0);
  else
    success = rc_hash_iterator_error(iterator, "Not a Gamecube disc");

  /* Finalize */
  rc_file_close(iterator, file_handle);

  if (success)
    return rc_hash_finalize(iterator, &md5, hash);

  return 0;
}

/* helper variable only used for testing */
const char* _rc_hash_jaguar_cd_homebrew_hash = NULL;

int rc_hash_jaguar_cd(char hash[33], const rc_hash_iterator_t* iterator)
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

  for (i = 64; i < sizeof(buffer) - 32 - 4 * 3; i++) {
    if (memcmp(&buffer[i], "TARA IPARPVODED TA AEHDAREA RT I", 32) == 0) {
      byteswapped = 1;
      offset = i + 32 + 4;
      size = (buffer[offset] << 16) | (buffer[offset + 1] << 24) | (buffer[offset + 2]) | (buffer[offset + 3] << 8);
      break;
    }
    else if (memcmp(&buffer[i], "ATARI APPROVED DATA HEADER ATRI ", 32) == 0) {
      byteswapped = 0;
      offset = i + 32 + 4;
      size = (buffer[offset] << 24) | (buffer[offset + 1] << 16) | (buffer[offset + 2] << 8) | (buffer[offset + 3]);
      break;
    }
  }

  if (size == 0) { /* did not see ATARI APPROVED DATA HEADER */
    rc_cd_close_track(iterator, track_handle);
    return rc_hash_iterator_error(iterator, "Not a Jaguar CD");
  }

  i = 0; /* only loop once */
  do {
    md5_init(&md5);

    offset += 4;

    rc_hash_iterator_verbose_formatted(iterator, "Hashing boot executable (%u bytes starting at %u bytes into sector %u)", size, offset, sector);

    if (size > MAX_BUFFER_SIZE)
      size = MAX_BUFFER_SIZE;

    do {
      if (byteswapped)
        rc_hash_byteswap16(buffer, &buffer[sizeof(buffer)]);

      remaining = sizeof(buffer) - offset;
      if (remaining >= size) {
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

int rc_hash_neogeo_cd(char hash[33], const rc_hash_iterator_t* iterator)
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
  if (!sector) {
    rc_cd_close_track(iterator, track_handle);
    return rc_hash_iterator_error(iterator, "Not a NeoGeo CD game disc");
  }

  if (rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer)) == 0) {
    rc_cd_close_track(iterator, track_handle);
    return 0;
  }

  md5_init(&md5);

  buffer[sizeof(buffer) - 1] = '\0';
  ptr = &buffer[0];
  do {
    char* start = ptr;
    while (*ptr && *ptr != '.')
      ++ptr;

    if (strncasecmp(ptr, ".PRG", 4) == 0) {
      ptr += 4;
      *ptr++ = '\0';

      sector = rc_cd_find_file_sector(iterator, track_handle, start, &size);
      if (!sector || !rc_hash_cd_file(&md5, iterator, track_handle, sector, NULL, size, start)) {
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
    return rc_hash_iterator_error(iterator, "Not a PC Engine CD");

  /* normal PC Engine CD will have a header block in sector 1 */
  if (memcmp("PC Engine CD-ROM SYSTEM", &buffer[32], 23) == 0) {
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
    while (num_sectors > 0) {
      rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer));
      md5_append(&md5, buffer, sizeof(buffer));

      ++sector;
      --num_sectors;
    }
  }
  /* GameExpress CDs use a standard Joliet filesystem - locate and hash the BOOT.BIN */
  else if ((sector = rc_cd_find_file_sector(iterator, track_handle, "BOOT.BIN", &size)) != 0 && size < MAX_BUFFER_SIZE) {
    md5_init(&md5);
    while (size > sizeof(buffer)) {
      rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer));
      md5_append(&md5, buffer, sizeof(buffer));

      ++sector;
      size -= sizeof(buffer);
    }

    if (size > 0) {
      rc_cd_read_sector(iterator, track_handle, sector, buffer, size);
      md5_append(&md5, buffer, size);
    }
  }
  else {
    return rc_hash_iterator_error(iterator, "Not a PC Engine CD");
  }

  return rc_hash_finalize(iterator, &md5, hash);
}

int rc_hash_pce_cd(char hash[33], const rc_hash_iterator_t* iterator)
{
  int result;
  void* track_handle = rc_cd_open_track(iterator, RC_HASH_CDTRACK_FIRST_DATA);
  if (!track_handle)
    return rc_hash_iterator_error(iterator, "Could not open track");

  result = rc_hash_pce_track(hash, track_handle, iterator);

  rc_cd_close_track(iterator, track_handle);

  return result;
}

int rc_hash_pcfx_cd(char hash[33], const rc_hash_iterator_t* iterator)
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
  if (memcmp("PC-FX:Hu_CD-ROM", &buffer[0], 15) != 0) {
    rc_cd_close_track(iterator, track_handle);

    /* not found in the largest data track, check track 2 */
    track_handle = rc_cd_open_track(iterator, 2);
    if (!track_handle)
      return rc_hash_iterator_error(iterator, "Could not open track");

    sector = rc_cd_first_track_sector(iterator, track_handle);
    rc_cd_read_sector(iterator, track_handle, sector, buffer, 32);
  }

  if (memcmp("PC-FX:Hu_CD-ROM", &buffer[0], 15) == 0) {
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
    while (num_sectors > 0) {
      rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer));
      md5_append(&md5, buffer, sizeof(buffer));

      ++sector;
      --num_sectors;
    }
  }
  else {
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
  for (ptr = (char*)buffer; *ptr; ++ptr) {
    if (strncmp(ptr, boot_key, boot_key_len) == 0) {
      ptr += boot_key_len;
      while (isspace((unsigned char)*ptr))
        ++ptr;

      if (*ptr == '=') {
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

int rc_hash_psx(char hash[33], const rc_hash_iterator_t* iterator)
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
  if (!sector) {
    sector = rc_cd_find_file_sector(iterator, track_handle, "PSX.EXE", &size);
    if (sector)
      memcpy(exe_name, "PSX.EXE", 8);
  }

  if (!sector) {
    rc_hash_iterator_error(iterator, "Could not locate primary executable");
  }
  else if (rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer)) < sizeof(buffer)) {
    rc_hash_iterator_error(iterator, "Could not read primary executable");
  }
  else {
    if (memcmp(buffer, "PS-X EXE", 7) != 0) {
      rc_hash_iterator_verbose_formatted(iterator, "%s did not contain PS-X EXE marker", exe_name);
    }
    else {
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

int rc_hash_ps2(char hash[33], const rc_hash_iterator_t* iterator)
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
  if (!sector) {
    rc_hash_iterator_error(iterator, "Could not locate primary executable");
  }
  else if (rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer)) < sizeof(buffer)) {
    rc_hash_iterator_error(iterator, "Could not read primary executable");
  }
  else {
    if (memcmp(buffer, "\x7f\x45\x4c\x46", 4) != 0)
      rc_hash_iterator_verbose_formatted(iterator, "%s did not contain ELF marker", exe_name);

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

int rc_hash_psp(char hash[33], const rc_hash_iterator_t* iterator)
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
  if (!sector) {
    rc_cd_close_track(iterator, track_handle);
    return rc_hash_iterator_error(iterator, "Not a PSP game disc");
  }

  md5_init(&md5);
  if (!rc_hash_cd_file(&md5, iterator, track_handle, sector, NULL, size, "PSP_GAME\\PARAM.SFO")) {
    rc_cd_close_track(iterator, track_handle);
    return 0;
  }

  sector = rc_cd_find_file_sector(iterator, track_handle, "PSP_GAME\\SYSDIR\\EBOOT.BIN", &size);
  if (!sector) {
    rc_cd_close_track(iterator, track_handle);
    return rc_hash_iterator_error(iterator, "Could not find primary executable");
  }

  if (!rc_hash_cd_file(&md5, iterator, track_handle, sector, NULL, size, "PSP_GAME\\SYSDIR\\EBOOT.BIN")) {
    rc_cd_close_track(iterator, track_handle);
    return 0;
  }

  rc_cd_close_track(iterator, track_handle);
  return rc_hash_finalize(iterator, &md5, hash);
}

int rc_hash_sega_cd(char hash[33], const rc_hash_iterator_t* iterator)
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
      memcmp(buffer, "SEGA SEGASATURN ", 16) != 0) { /* Sega Saturn */
    return rc_hash_iterator_error(iterator, "Not a Sega CD");
  }

  return rc_hash_buffer(hash, buffer, sizeof(buffer), iterator);
}

static int rc_hash_wii_disc(md5_state_t* md5, const rc_hash_iterator_t* iterator, void* file_handle)
{
  const uint32_t MAIN_HEADER_SIZE = 0x80;
  const uint64_t REGION_CODE_ADDRESS = 0x4E000;
  const uint32_t CLUSTER_SIZE = 0x7C00;
  const uint32_t MAX_CLUSTER_COUNT = 1024;

  uint32_t partition_info_table[8];
  uint32_t total_partition_count = 0;
  uint32_t* partition_table;
  uint64_t tmd_offset;
  uint32_t tmd_size;
  uint64_t part_offset;
  uint64_t part_size;
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
  if (!buffer) {
    rc_file_close(iterator, file_handle);
    return rc_hash_iterator_error(iterator, "Could not allocate temporary buffer");
  }

  rc_hash_iterator_verbose_formatted(iterator, "Hashing %u byte main header for [%c%c%c%c%c%c]",
    MAIN_HEADER_SIZE, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
  rc_file_seek(iterator, file_handle, 0, SEEK_SET);
  rc_file_read(iterator, file_handle, buffer, MAIN_HEADER_SIZE);
  md5_append(md5, buffer, MAIN_HEADER_SIZE);

  /* Hash region code */
  rc_file_seek(iterator, file_handle, REGION_CODE_ADDRESS, SEEK_SET);
  rc_file_read(iterator, file_handle, quad_buffer, 4);
  md5_append(md5, quad_buffer, 4);

  /* Scan partition table */
  rc_file_seek(iterator, file_handle, 0x40000, SEEK_SET);
  for (ix = 0; ix < 8; ix++) {
    rc_file_read(iterator, file_handle, quad_buffer, 4);
    partition_info_table[ix] =
      (quad_buffer[0] << 24) | (quad_buffer[1] << 16) | (quad_buffer[2] << 8) | quad_buffer[3];
    if (ix % 2 == 0)
      total_partition_count += partition_info_table[ix];
  }

  if (total_partition_count == 0) {
    free(buffer);
    rc_file_close(iterator, file_handle);
    return rc_hash_iterator_error(iterator, "No partitions found");
  }

  partition_table = (uint32_t*)malloc(total_partition_count * 4 * 2);
  kx = 0;
  for (jx = 0; jx < 8; jx += 2) {
    rc_file_seek(iterator, file_handle, ((uint64_t)partition_info_table[jx + 1]) << 2, SEEK_SET);
    for (ix = 0; ix < partition_info_table[jx]; ix++) {
      rc_file_read(iterator, file_handle, quad_buffer, 4);
      partition_table[kx++] =
        (quad_buffer[0] << 24) | (quad_buffer[1] << 16) | (quad_buffer[2] << 8) | quad_buffer[3];
      rc_file_read(iterator, file_handle, quad_buffer, 4);
      partition_table[kx++] =
        (quad_buffer[0] << 24) | (quad_buffer[1] << 16) | (quad_buffer[2] << 8) | quad_buffer[3];
    }
  }

  /* Read each partition */
  for (jx = 0; jx < total_partition_count * 2; jx += 2) {
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
      tmd_size = CLUSTER_SIZE;

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

    if (encrypted) {
      cluster_count = (part_size / 0x8000 > MAX_CLUSTER_COUNT) ? MAX_CLUSTER_COUNT : (uint32_t)(part_size / 0x8000);
      rc_hash_iterator_verbose_formatted(iterator, "Hashing %u encrypted clusters (%u bytes)",
        cluster_count, cluster_count * CLUSTER_SIZE);
      for (ix = 0; ix < cluster_count; ix++) {
        rc_file_seek(iterator, file_handle, part_offset + (ix * 0x8000) + 0x400, SEEK_SET);
        rc_file_read(iterator, file_handle, buffer, CLUSTER_SIZE);
        md5_append(md5, buffer, CLUSTER_SIZE);
      }
    }
    else { /* Decrypted */
      if (rc_hash_nintendo_disc_partition(md5, iterator, file_handle, (uint32_t)part_offset, 2) == 0) {
        free(partition_table);
        free(buffer);
        return rc_hash_iterator_error(iterator, "Failed to hash Wii partition");
      }
    }
  }
  free(partition_table);
  free(buffer);
  return 1;
}

static int rc_hash_wiiware(md5_state_t* md5, const rc_hash_iterator_t* iterator, void* file_handle)
{
  uint32_t cert_chain_size, ticket_size, tmd_size;
  uint32_t tmd_start_addr, content_count, content_addr, content_size, buffer_size;
  uint32_t ix;

  uint8_t quad_buffer[4];
  uint8_t* buffer;

  rc_file_seek(iterator, file_handle, 0x08, SEEK_SET);
  rc_file_read(iterator, file_handle, quad_buffer, 4);
  cert_chain_size =
    (quad_buffer[0] << 24) | (quad_buffer[1] << 16) | (quad_buffer[2] << 8) | quad_buffer[3];
  /* Each content is individually aligned to a 0x40-byte boundary. */
  cert_chain_size = (cert_chain_size + 0x3F) & ~0x3F;
  rc_file_seek(iterator, file_handle, 0x10, SEEK_SET);
  rc_file_read(iterator, file_handle, quad_buffer, 4);
  ticket_size =
    (quad_buffer[0] << 24) | (quad_buffer[1] << 16) | (quad_buffer[2] << 8) | quad_buffer[3];
  ticket_size = (ticket_size + 0x3F) & ~0x3F;
  rc_file_read(iterator, file_handle, quad_buffer, 4);
  tmd_size =
    (quad_buffer[0] << 24) | (quad_buffer[1] << 16) | (quad_buffer[2] << 8) | quad_buffer[3];
  tmd_size = (tmd_size + 0x3F) & ~0x3F;
  if (tmd_size > MAX_BUFFER_SIZE)
    tmd_size = MAX_BUFFER_SIZE;

  tmd_start_addr = 0x40 + cert_chain_size + ticket_size;

  /* Hash TMD */
  buffer = (uint8_t*)malloc(tmd_size);
  rc_file_seek(iterator, file_handle, tmd_start_addr, SEEK_SET);
  rc_file_read(iterator, file_handle, buffer, tmd_size);
  rc_hash_iterator_verbose_formatted(iterator, "Hashing %u byte TMD", tmd_size);
  md5_append(md5, buffer, tmd_size);
  free(buffer);

  /* Get count of content sections */
  rc_file_seek(iterator, file_handle, (uint64_t)tmd_start_addr + 0x1de, SEEK_SET);
  rc_file_read(iterator, file_handle, quad_buffer, 2);
  content_count = (quad_buffer[0] << 8) | quad_buffer[1];
  rc_hash_iterator_verbose_formatted(iterator, "Hashing %u content sections", content_count);
  content_addr = tmd_start_addr + tmd_size;
  for (ix = 0; ix < content_count; ix++) {
    /* Get content section size */
    rc_file_seek(iterator, file_handle, (uint64_t)tmd_start_addr + 0x1e4 + 8 + ix * 0x24, SEEK_SET);
    rc_file_read(iterator, file_handle, quad_buffer, 4);
    if (quad_buffer[0] == 0x00 && quad_buffer[1] == 0x00 && quad_buffer[2] == 0x00 && quad_buffer[3] == 0x00) {
      rc_file_read(iterator, file_handle, quad_buffer, 4);
      content_size =
        (quad_buffer[0] << 24) | (quad_buffer[1] << 16) | (quad_buffer[2] << 8) | quad_buffer[3];
      /* Padding between content should be ignored. But because the content data is encrypted,
      the size to hash for each content should be rounded up to the size of an AES block (16 bytes). */
      content_size = (content_size + 0x0F) & ~0x0F;
    }
    else {
      /* size > 4GB, just assume MAX_BUFFER_SIZE */
      content_size = MAX_BUFFER_SIZE;
    }
    buffer_size = (content_size > MAX_BUFFER_SIZE) ? MAX_BUFFER_SIZE : content_size;

    /* Hash content */
    buffer = (uint8_t*)malloc(buffer_size);
    rc_file_seek(iterator, file_handle, content_addr, SEEK_SET);
    rc_file_read(iterator, file_handle, buffer, buffer_size);
    md5_append(md5, buffer, buffer_size);
    content_addr += content_size;
    content_addr = (content_addr + 0x3F) & ~0x3F;
    free(buffer);
  }

  return 1;
}

int rc_hash_wii(char hash[33], const rc_hash_iterator_t* iterator)
{
  md5_state_t md5;
  void* file_handle;

  uint8_t quad_buffer[4];
  uint8_t success;

  file_handle = rc_file_open(iterator, iterator->path);
  if (!file_handle)
    return rc_hash_iterator_error(iterator, "Could not open file");

  md5_init(&md5);
  /* Check Magic Words */
  rc_file_seek(iterator, file_handle, 0x18, SEEK_SET);
  rc_file_read(iterator, file_handle, quad_buffer, 4);
  if (quad_buffer[0] == 0x5D && quad_buffer[1] == 0x1C && quad_buffer[2] == 0x9E && quad_buffer[3] == 0xA3) {
    success = rc_hash_wii_disc(&md5, iterator, file_handle);
  }
  else {
    rc_file_seek(iterator, file_handle, 0x04, SEEK_SET);
    rc_file_read(iterator, file_handle, quad_buffer, 4);
    if (quad_buffer[0] == 'I' && quad_buffer[1] == 's' && quad_buffer[2] == 0x00 && quad_buffer[3] == 0x00)
      success = rc_hash_wiiware(&md5, iterator, file_handle);
    else
      success = rc_hash_iterator_error(iterator, "Not a supported Wii file");
  }

  /* Finalize */
  rc_file_close(iterator, file_handle);

  if (success)
    return rc_hash_finalize(iterator, &md5, hash);

  return 0;
}
