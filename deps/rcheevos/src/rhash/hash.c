#ifdef RARCH_INTERNAL
 #include "../../include/rc_hash.h"
#else
 #include "rc_hash.h"
#endif

#include "../rcheevos/compat.h"

#include "md5.h"

#include <ctype.h>

/* arbitrary limit to prevent allocating and hashing large files */
#define MAX_BUFFER_SIZE 64 * 1024 * 1024

const char* rc_path_get_filename(const char* path);

/* ===================================================== */

static rc_hash_message_callback error_message_callback = NULL;
rc_hash_message_callback verbose_message_callback = NULL;

void rc_hash_init_error_message_callback(rc_hash_message_callback callback)
{
  error_message_callback = callback;
}

int rc_hash_error(const char* message)
{
  if (error_message_callback)
    error_message_callback(message);

  return 0;
}

void rc_hash_init_verbose_message_callback(rc_hash_message_callback callback)
{
  verbose_message_callback = callback;
}

static void rc_hash_verbose(const char* message)
{
  if (verbose_message_callback)
    verbose_message_callback(message);
}

/* ===================================================== */

static struct rc_hash_filereader filereader_funcs;
static struct rc_hash_filereader* filereader = NULL;

void rc_hash_init_custom_filereader(struct rc_hash_filereader* reader)
{
  memcpy(&filereader_funcs, reader, sizeof(filereader_funcs));
  filereader = &filereader_funcs;
}

static void* filereader_open(const char* path)
{
  return fopen(path, "rb");
}

static void filereader_seek(void* file_handle, size_t offset, int origin)
{
  fseek((FILE*)file_handle, (long)offset, origin);
}

static size_t filereader_tell(void* file_handle)
{
  return ftell((FILE*)file_handle);
}

static size_t filereader_read(void* file_handle, void* buffer, size_t requested_bytes)
{
  return fread(buffer, 1, requested_bytes, (FILE*)file_handle);
}

static void filereader_close(void* file_handle)
{
  fclose((FILE*)file_handle);
}

void* rc_file_open(const char* path)
{
  void* handle;

  if (!filereader)
  {
    filereader_funcs.open = filereader_open;
    filereader_funcs.seek = filereader_seek;
    filereader_funcs.tell = filereader_tell;
    filereader_funcs.read = filereader_read;
    filereader_funcs.close = filereader_close;

    filereader = &filereader_funcs;
  }

  handle = filereader->open(path);
  if (handle && verbose_message_callback)
  {
    char message[1024];
    snprintf(message, sizeof(message), "Opened %s", rc_path_get_filename(path));
    verbose_message_callback(message);
  }

  return handle;
}

void rc_file_seek(void* file_handle, size_t offset, int origin)
{
  if (filereader)
    filereader->seek(file_handle, offset, origin);
}

size_t rc_file_tell(void* file_handle)
{
  return (filereader) ? filereader->tell(file_handle) : 0;
}

size_t rc_file_read(void* file_handle, void* buffer, int requested_bytes)
{
  return (filereader) ? filereader->read(file_handle, buffer, requested_bytes) : 0;
}

void rc_file_close(void* file_handle)
{
  if (filereader)
    filereader->close(file_handle);
}

/* ===================================================== */

static struct rc_hash_cdreader cdreader_funcs;
static struct rc_hash_cdreader* cdreader = NULL;

void rc_hash_init_custom_cdreader(struct rc_hash_cdreader* reader)
{
  memcpy(&cdreader_funcs, reader, sizeof(cdreader_funcs));
  cdreader = &cdreader_funcs;
}

static void* rc_cd_open_track(const char* path, uint32_t track)
{
  if (cdreader && cdreader->open_track)
    return cdreader->open_track(path, track);

  rc_hash_error("no hook registered for cdreader_open_track");
  return NULL;
}

static size_t rc_cd_read_sector(void* track_handle, uint32_t sector, void* buffer, size_t requested_bytes)
{
  if (cdreader && cdreader->read_sector)
    return cdreader->read_sector(track_handle, sector, buffer, requested_bytes);

  rc_hash_error("no hook registered for cdreader_read_sector");
  return 0;
}

static void rc_cd_close_track(void* track_handle)
{
  if (cdreader && cdreader->close_track)
  {
    cdreader->close_track(track_handle);
    return;
  }

  rc_hash_error("no hook registered for cdreader_close_track");
}

static uint32_t rc_cd_find_file_sector(void* track_handle, const char* path, unsigned* size)
{
  uint8_t buffer[2048], *tmp;
  int sector;
  size_t filename_length;
  const char* slash;

  if (!track_handle)
    return 0;

  filename_length = strlen(path);
  slash = strrchr(path, '\\');
  if (slash)
  {
    /* find the directory record for the first part of the path */
    memcpy(buffer, path, slash - path);
    buffer[slash - path] = '\0';

    sector = rc_cd_find_file_sector(track_handle, (const char *)buffer, NULL);
    if (!sector)
      return 0;

    ++slash;
    filename_length -= (slash - path);
    path = slash;
  }
  else
  {
    /* find the cd information (always 16 frames in) */
    if (!rc_cd_read_sector(track_handle, 16, buffer, 256))
      return 0;

    /* the directory_record starts at 156, the sector containing the table of contents is 2 bytes into that.
     * https://www.cdroller.com/htm/readdata.html
     */
    sector = buffer[156 + 2] | (buffer[156 + 3] << 8) | (buffer[156 + 4] << 16);
  }

  /* fetch and process the directory record */
  if (!rc_cd_read_sector(track_handle, sector, buffer, sizeof(buffer)))
    return 0;

  tmp = buffer;
  while (tmp < buffer + sizeof(buffer))
  {
    if (!*tmp)
      return 0;

    /* filename is 33 bytes into the record and the format is "FILENAME;version" or "DIRECTORY" */
    if ((tmp[33 + filename_length] == ';' || tmp[33 + filename_length] == '\0') &&
        strncasecmp((const char*)(tmp + 33), path, filename_length) == 0)
    {
      sector = tmp[2] | (tmp[3] << 8) | (tmp[4] << 16);

      if (verbose_message_callback)
      {
        snprintf((char*)buffer, sizeof(buffer), "Found %s at sector %d", path, sector);
        verbose_message_callback((const char*)buffer);
      }

      if (size)
        *size = tmp[10] | (tmp[11] << 8) | (tmp[12] << 16) | (tmp[13] << 24);

      return sector;
    }

    /* the first byte of the record is the length of the record */
    tmp += *tmp;
  }

  return 0;
}

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

static const char* rc_path_get_extension(const char* path)
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

static int rc_hash_finalize(md5_state_t* md5, char hash[33])
{
  md5_byte_t digest[16];

  md5_finish(md5, digest);

  /* NOTE: sizeof(hash) is 4 because it's still treated like a pointer, despite specifying a size */
  snprintf(hash, 33, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
    digest[0], digest[1], digest[2], digest[3], digest[4], digest[5], digest[6], digest[7],
    digest[8], digest[9], digest[10], digest[11], digest[12], digest[13], digest[14], digest[15]
  );

  if (verbose_message_callback)
  {
    char message[128];
    snprintf(message, sizeof(message), "Generated hash %s", hash);
    verbose_message_callback(message);
  }

  return 1;
}

static int rc_hash_buffer(char hash[33], uint8_t* buffer, size_t buffer_size)
{
  md5_state_t md5;
  md5_init(&md5);

  if (buffer_size > MAX_BUFFER_SIZE)
    buffer_size = MAX_BUFFER_SIZE;

  md5_append(&md5, buffer, (int)buffer_size);

  if (verbose_message_callback)
  {
    char message[128];
    snprintf(message, sizeof(message), "Hashing %u byte buffer", (unsigned)buffer_size);
    verbose_message_callback(message);
  }

  return rc_hash_finalize(&md5, hash);
}

static int rc_hash_3do(char hash[33], const char* path)
{
  uint8_t buffer[2048];
  const uint8_t operafs_identifier[7] = { 0x01, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x01 };
  void* track_handle;
  md5_state_t md5;
  int sector;
  int block_size, block_location;
  int offset, stop;
  size_t size = 0;

  track_handle = rc_cd_open_track(path, 1);
  if (!track_handle)
    return rc_hash_error("Could not open track");

  /* the Opera filesystem stores the volume information in the first 132 bytes of sector 0
   * https://github.com/barbeque/3dodump/blob/master/OperaFS-Format.md
   */
  rc_cd_read_sector(track_handle, 0, buffer, 132);

  if (memcmp(buffer, operafs_identifier, sizeof(operafs_identifier)) == 0)
  {
    if (verbose_message_callback)
    {
      char message[128];
      snprintf(message, sizeof(message), "Found 3DO CD, title=%.32s", &buffer[0x28]);
      verbose_message_callback(message);
    }

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
      rc_cd_read_sector(track_handle, sector, buffer, sizeof(buffer));

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
            size = buffer[offset + 0x11] * 65536 + buffer[offset + 0x12] * 256 + buffer[offset + 0x13];

            if (verbose_message_callback)
            {
              char message[128];
              snprintf(message, sizeof(message), "Hashing header (%u bytes) and %.32s (%u bytes) ", 132, &buffer[offset + 0x20], (unsigned)size);
              verbose_message_callback(message);
            }

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
      rc_cd_close_track(track_handle);
      return rc_hash_error("Could not find LaunchMe");
    }

    sector = block_location / 2048;

    while (size > 2048)
    {
      rc_cd_read_sector(track_handle, sector, buffer, sizeof(buffer));
      md5_append(&md5, buffer, sizeof(buffer));

      ++sector;
      size -= 2048;
    }

    rc_cd_read_sector(track_handle, sector, buffer, size);
    md5_append(&md5, buffer, (int)size);
  }
  else
  {
    rc_cd_close_track(track_handle);
    return rc_hash_error("Not a 3DO CD");
  }

  rc_cd_close_track(track_handle);

  return rc_hash_finalize(&md5, hash);
}

static int rc_hash_arcade(char hash[33], const char* path)
{
  /* arcade hash is just the hash of the filename (no extension) - the cores are pretty stringent about having the right ROM data */
  const char* filename = rc_path_get_filename(path);
  const char* ext = rc_path_get_extension(filename);
  size_t filename_length = ext - filename - 1;

  /* fbneo supports loading subsystems by using specific folder names.
   * if one is found, include it in the hash.
   * https://github.com/libretro/FBNeo/blob/master/src/burner/libretro/README.md#emulating-consoles
   */
  if (filename > path + 1)
  {
    int include_folder = 0;
    const char* folder = filename - 1;
    size_t parent_folder_length = 0;

    do
    {
      if (folder[-1] == '/' || folder[-1] == '\\')
        break;

      --folder;
    } while (folder > path);

    parent_folder_length = filename - folder - 1;
    switch (parent_folder_length)
    {
      case 3:
        if (memcmp(folder, "nes", 3) == 0 ||
            memcmp(folder, "fds", 3) == 0 ||
            memcmp(folder, "sms", 3) == 0 ||
            memcmp(folder, "msx", 3) == 0 ||
            memcmp(folder, "ngp", 3) == 0 ||
            memcmp(folder, "pce", 3) == 0 ||
            memcmp(folder, "sgx", 3) == 0)
          include_folder = 1;
        break;
      case 4:
        if (memcmp(folder, "tg16", 4) == 0)
          include_folder = 1;
        break;
      case 6:
        if (memcmp(folder, "coleco", 6) == 0 ||
            memcmp(folder, "sg1000", 6) == 0)
          include_folder = 1;
        break;
      case 8:
        if (memcmp(folder, "gamegear", 8) == 0 ||
            memcmp(folder, "megadriv", 8) == 0 ||
            memcmp(folder, "spectrum", 8) == 0)
          include_folder = 1;
        break;
      default:
        break;
    }

    if (include_folder)
    {
      char buffer[128]; /* realistically, this should never need more than ~20 characters */
      if (parent_folder_length + filename_length + 1 < sizeof(buffer))
      {
        memcpy(&buffer[0], folder, parent_folder_length);
        buffer[parent_folder_length] = '_';
        memcpy(&buffer[parent_folder_length + 1], filename, filename_length);
        return rc_hash_buffer(hash, (uint8_t*)&buffer[0], parent_folder_length + filename_length + 1);
      }
    }
  }

  return rc_hash_buffer(hash, (uint8_t*)filename, filename_length);
}

static int rc_hash_lynx(char hash[33], uint8_t* buffer, size_t buffer_size)
{
  /* if the file contains a header, ignore it */
  if (buffer[0] == 'L' && buffer[1] == 'Y' && buffer[2] == 'N' && buffer[3] == 'X' && buffer[4] == 0)
  {
    rc_hash_verbose("Ignoring LYNX header");

    buffer += 64;
    buffer_size -= 64;
  }

  return rc_hash_buffer(hash, buffer, buffer_size);
}

static int rc_hash_nes(char hash[33], uint8_t* buffer, size_t buffer_size)
{
  /* if the file contains a header, ignore it */
  if (buffer[0] == 'N' && buffer[1] == 'E' && buffer[2] == 'S' && buffer[3] == 0x1A)
  {
    rc_hash_verbose("Ignoring NES header");

    buffer += 16;
    buffer_size -= 16;
  }
  else if (buffer[0] == 'F' && buffer[1] == 'D' && buffer[2] == 'S' && buffer[3] == 0x1A)
  {
      rc_hash_verbose("Ignoring FDS header");

      buffer += 16;
      buffer_size -= 16;
  }

  return rc_hash_buffer(hash, buffer, buffer_size);
}

static int rc_hash_nintendo_ds(char hash[33], const char* path)
{
  uint8_t header[512];
  uint8_t* hash_buffer;
  unsigned int hash_size, arm9_size, arm9_addr, arm7_size, arm7_addr, icon_addr;
  size_t num_read;
  int offset = 0;
  md5_state_t md5;
  void* file_handle;

  file_handle = rc_file_open(path);
  if (!file_handle)
    return rc_hash_error("Could not open file");

  rc_file_seek(file_handle, 0, SEEK_SET);
  if (rc_file_read(file_handle, header, sizeof(header)) != 512)
    return rc_hash_error("Failed to read header");

  if (header[0] == 0x2E && header[1] == 0x00 && header[2] == 0x00 && header[3] == 0xEA &&
    header[0xB0] == 0x44 && header[0xB1] == 0x46 && header[0xB2] == 0x96 && header[0xB3] == 0)
  {
    /* SuperCard header detected, ignore it */
    rc_hash_verbose("Ignoring SuperCard header");

    offset = 512;
    rc_file_seek(file_handle, offset, SEEK_SET);
    rc_file_read(file_handle, header, sizeof(header));
  }

  arm9_addr = header[0x20] | (header[0x21] << 8) | (header[0x22] << 16) | (header[0x23] << 24);
  arm9_size = header[0x2C] | (header[0x2D] << 8) | (header[0x2E] << 16) | (header[0x2F] << 24);
  arm7_addr = header[0x30] | (header[0x31] << 8) | (header[0x32] << 16) | (header[0x33] << 24);
  arm7_size = header[0x3C] | (header[0x3D] << 8) | (header[0x3E] << 16) | (header[0x3F] << 24);
  icon_addr = header[0x68] | (header[0x69] << 8) | (header[0x6A] << 16) | (header[0x6B] << 24);

  if (arm9_size + arm7_size > 16 * 1024 * 1024)
  {
    /* sanity check - code blocks are typically less than 1MB each - assume not a DS ROM */
    snprintf((char*)header, sizeof(header), "arm9 code size (%u) + arm7 code size (%u) exceeds 16MB", arm9_size, arm7_size);
    return rc_hash_error((const char*)header);
  }

  hash_size = 0xA00;
  if (arm9_size > hash_size)
    hash_size = arm9_size;
  if (arm7_size > hash_size)
    hash_size = arm7_size;

  hash_buffer = (uint8_t*)malloc(hash_size);
  if (!hash_buffer)
  {
    rc_file_close(file_handle);

    snprintf((char*)header, sizeof(header), "Failed to allocate %u bytes", hash_size);
    return rc_hash_error((const char*)header);
  }

  md5_init(&md5);

  rc_hash_verbose("Hashing 352 byte header");
  md5_append(&md5, header, 0x160);

  if (verbose_message_callback)
  {
    snprintf((char*)header, sizeof(header), "Hashing %u byte arm9 code (at %08X)", arm9_size, arm9_addr);
    verbose_message_callback((const char*)header);
  }

  rc_file_seek(file_handle, arm9_addr + offset, SEEK_SET);
  rc_file_read(file_handle, hash_buffer, arm9_size);
  md5_append(&md5, hash_buffer, arm9_size);

  if (verbose_message_callback)
  {
    snprintf((char*)header, sizeof(header), "Hashing %u byte arm7 code (at %08X)", arm7_size, arm7_addr);
    verbose_message_callback((const char*)header);
  }

  rc_file_seek(file_handle, arm7_addr + offset, SEEK_SET);
  rc_file_read(file_handle, hash_buffer, arm7_size);
  md5_append(&md5, hash_buffer, arm7_size);

  if (verbose_message_callback)
  {
    snprintf((char*)header, sizeof(header), "Hashing 2560 byte icon and labels data (at %08X)", icon_addr);
    verbose_message_callback((const char*)header);
  }

  rc_file_seek(file_handle, icon_addr + offset, SEEK_SET);
  num_read = rc_file_read(file_handle, hash_buffer, 0xA00);
  if (num_read < 0xA00)
  {
    /* some homebrew games don't provide a full icon block, and no data after the icon block.
     * if we didn't get a full icon block, fill the remaining portion with 0s
     */
    if (verbose_message_callback)
    {
      snprintf((char*)header, sizeof(header), "Warning: only got %u bytes for icon and labels data, 0-padding to 2560 bytes", (unsigned)num_read);
      verbose_message_callback((const char*)header);
    }

    memset(&hash_buffer[num_read], 0, 0xA00 - num_read);
  }
  md5_append(&md5, hash_buffer, 0xA00);

  free(hash_buffer);
  rc_file_close(file_handle);

  return rc_hash_finalize(&md5, hash);
}

static int rc_hash_pce_cd(char hash[33], const char* path)
{
  uint8_t buffer[2048];
  void* track_handle;
  md5_state_t md5;
  int sector, num_sectors;
  unsigned size;

  track_handle = rc_cd_open_track(path, 0);
  if (!track_handle)
    return rc_hash_error("Could not open track");

  /* the PC-Engine uses the second sector to specify boot information and program name.
   * the string "PC Engine CD-ROM SYSTEM" should exist at 32 bytes into the sector
   * http://shu.sheldows.com/shu/download/pcedocs/pce_cdrom.html
   */
  rc_cd_read_sector(track_handle, 1, buffer, 128);

  /* normal PC Engine CD will have a header block in sector 1 */
  if (memcmp("PC Engine CD-ROM SYSTEM", &buffer[32], 23) == 0)
  {
    /* the title of the disc is the last 22 bytes of the header */
    md5_init(&md5);
    md5_append(&md5, &buffer[106], 22);

    if (verbose_message_callback)
    {
      char message[128];
      buffer[128] = '\0';
      snprintf(message, sizeof(message), "Found PC Engine CD, title=%.22s", &buffer[106]);
      verbose_message_callback(message);
    }

    /* the first three bytes specify the sector of the program data, and the fourth byte
     * is the number of sectors.
     */
    sector = (buffer[0] << 16) + (buffer[1] << 8) + buffer[2];
    num_sectors = buffer[3];

    if (verbose_message_callback)
    {
      char message[128];
      snprintf(message, sizeof(message), "Hashing %d sectors starting at sector %d", num_sectors, sector);
      verbose_message_callback(message);
    }

    while (num_sectors > 0)
    {
      rc_cd_read_sector(track_handle, sector, buffer, sizeof(buffer));
      md5_append(&md5, buffer, sizeof(buffer));

      ++sector;
      --num_sectors;
    }
  }
  /* GameExpress CDs use a standard Joliet filesystem - locate and hash the BOOT.BIN */
  else if ((sector = rc_cd_find_file_sector(track_handle, "BOOT.BIN", &size)) != 0 && size < MAX_BUFFER_SIZE)
  {
    md5_init(&md5);
    while (size > sizeof(buffer))
    {
      rc_cd_read_sector(track_handle, sector, buffer, sizeof(buffer));
      md5_append(&md5, buffer, sizeof(buffer));

      ++sector;
      size -= sizeof(buffer);
    }

    if (size > 0)
    {
      rc_cd_read_sector(track_handle, sector, buffer, size);
      md5_append(&md5, buffer, size);
    }
  }
  else
  {
    rc_cd_close_track(track_handle);
    return rc_hash_error("Not a PC Engine CD");
  }

  rc_cd_close_track(track_handle);

  return rc_hash_finalize(&md5, hash);
}

static int rc_hash_pcfx_cd(char hash[33], const char* path)
{
  uint8_t buffer[2048];
  void* track_handle;
  md5_state_t md5;
  int sector, num_sectors;

  track_handle = rc_cd_open_track(path, 0);
  if (!track_handle)
    return rc_hash_error("Could not open track");

  /* PC-FX boot header fills the first two sectors of the disc
   * https://bitbucket.org/trap15/pcfxtools/src/master/pcfx-cdlink.c
   */

  /* PC-FX CD will have a header marker in sector 0 */
  rc_cd_read_sector(track_handle, 0, buffer, 32);
  if (memcmp("PC-FX:Hu_CD-ROM", &buffer[0], 15) == 0)
  {
    /* the important stuff is the first 128 bytes of the second sector (title being the first 32) */
    rc_cd_read_sector(track_handle, 1, buffer, 128);

    md5_init(&md5);
    md5_append(&md5, buffer, 128);

    if (verbose_message_callback)
    {
      char message[128];
      buffer[128] = '\0';
      snprintf(message, sizeof(message), "Found PC-FX CD, title=%.32s", &buffer[0]);
      verbose_message_callback(message);
    }

    /* the program sector is in bytes 33-36 (assume byte 36 is 0) */
    sector = (buffer[34] << 16) + (buffer[33] << 8) + buffer[32];

    /* the number of sectors the program occupies is in bytes 37-40 (assume byte 40 is 0) */
    num_sectors = (buffer[38] << 16) + (buffer[37] << 8) + buffer[36];

    if (verbose_message_callback)
    {
      char message[128];
      snprintf(message, sizeof(message), "Hashing %d sectors starting at sector %d", num_sectors, sector);
      verbose_message_callback(message);
    }

    while (num_sectors > 0)
    {
      rc_cd_read_sector(track_handle, sector, buffer, sizeof(buffer));
      md5_append(&md5, buffer, sizeof(buffer));

      ++sector;
      --num_sectors;
    }
  }
  else
  {
    rc_cd_read_sector(track_handle, 1, buffer, 128);
    rc_cd_close_track(track_handle);

    /* some PC-FX CDs still identify as PCE CDs */
    if (memcmp("PC Engine CD-ROM SYSTEM", &buffer[32], 23) == 0)
      return rc_hash_pce_cd(hash, path);

    return rc_hash_error("Not a PC-FX CD");
  }

  rc_cd_close_track(track_handle);

  return rc_hash_finalize(&md5, hash);
}

static int rc_hash_psx(char hash[33], const char* path)
{
  uint8_t buffer[2048];
  char exe_name[64] = "";
  char* ptr;
  char* start;
  void* track_handle;
  uint32_t sector;
  unsigned size;
  size_t num_read;
  int result = 0;
  md5_state_t md5;

  track_handle = rc_cd_open_track(path, 1);
  if (!track_handle)
    return rc_hash_error("Could not open track");

  sector = rc_cd_find_file_sector(track_handle, "SYSTEM.CNF", NULL);
  if (!sector)
  {
    sector = rc_cd_find_file_sector(track_handle, "PSX.EXE", &size);
    if (sector)
      strcpy(exe_name, "PSX.EXE");
  }
  else
  {
    size = (unsigned)rc_cd_read_sector(track_handle, sector, buffer, sizeof(buffer) - 1);
    buffer[size] = '\0';

    for (ptr = (char*)buffer; *ptr; ++ptr)
    {
      if (strncmp(ptr, "BOOT", 4) == 0)
      {
        ptr += 4;
        while (isspace((unsigned char)*ptr))
          ++ptr;

        if (*ptr == '=')
        {
          ++ptr;
          while (isspace((unsigned char)*ptr))
            ++ptr;

          if (strncmp(ptr, "cdrom:", 6) == 0)
            ptr += 6;
          if (*ptr == '\\')
            ++ptr;

          start = ptr;
          while (!isspace((unsigned char)*ptr) && *ptr != ';')
            ++ptr;

          size = (unsigned)(ptr - start);
          if (size >= sizeof(exe_name))
            size = sizeof(exe_name) - 1;

          memcpy(exe_name, start, size);
          exe_name[size] = '\0';

          if (verbose_message_callback)
          {
            snprintf((char*)buffer, sizeof(buffer), "Looking for boot executable: %s", exe_name);
            verbose_message_callback((const char*)buffer);
          }

          sector = rc_cd_find_file_sector(track_handle, exe_name, &size);
          break;
        }
      }

      /* advance to end of line */
      while (*ptr && *ptr != '\n')
        ++ptr;
    }
  }

  if (!sector)
  {
    rc_hash_error("Could not locate primary executable");
  }
  else if ((num_read = rc_cd_read_sector(track_handle, sector, buffer, sizeof(buffer))) < sizeof(buffer))
  {
    rc_hash_error("Could not read primary executable");
  }
  else
  {
    if (memcmp(buffer, "PS-X EXE", 7) != 0)
    {
      if (verbose_message_callback)
      {
        char message[128];
        snprintf(message, sizeof(message), "%s did not contain PS-X EXE marker", exe_name);
        verbose_message_callback(message);
      }
    }
    else
    {
      /* the PS-X EXE header specifies the executable size as a 4-byte value 28 bytes into the header, which doesn't
       * include the header itself. We want to include the header in the hash, so append another 2048 to that value.
       */
      size = (((uint8_t)buffer[31] << 24) | ((uint8_t)buffer[30] << 16) | ((uint8_t)buffer[29] << 8) | (uint8_t)buffer[28]) + 2048;
    }

    if (size > MAX_BUFFER_SIZE)
      size = MAX_BUFFER_SIZE;

    if (verbose_message_callback)
    {
      char message[128];
      snprintf(message, sizeof(message), "Hashing %s title (%u bytes) and contents (%u bytes) ", exe_name, (unsigned)strlen(exe_name), size);
      verbose_message_callback(message);
    }

    /* there's also a few games that are use a singular engine and only differ via their data files. luckily, they have
     * unique serial numbers, and use the serial number as the boot file in the standard way. include the boot file in the hash
     */
    md5_init(&md5);
    md5_append(&md5, (md5_byte_t*)exe_name, (int)strlen(exe_name));

    do
    {
      md5_append(&md5, buffer, (int)num_read);

      size -= (unsigned)num_read;
      if (size == 0)
        break;

      ++sector;
      if (size >= sizeof(buffer))
        num_read = rc_cd_read_sector(track_handle, sector, buffer, sizeof(buffer));
      else
        num_read = rc_cd_read_sector(track_handle, sector, buffer, size);
    } while (num_read > 0);

    result = rc_hash_finalize(&md5, hash);
  }

  rc_cd_close_track(track_handle);

  return result;
}

static int rc_hash_sega_cd(char hash[33], const char* path)
{
  uint8_t buffer[512];
  void* track_handle;

  track_handle = rc_cd_open_track(path, 1);
  if (!track_handle)
    return rc_hash_error("Could not open track");

  /* the first 512 bytes of sector 0 are a volume header and ROM header that uniquely identify the game.
   * After that is an arbitrary amount of code that ensures the game is being run in the correct region.
   * Then more arbitrary code follows that actually starts the boot process. Somewhere in there, the
   * primary executable is loaded. In many cases, a single game will have multiple executables, so even
   * if we could determine the primary one, it's just the tip of the iceberg. As such, we've decided that
   * hashing the volume and ROM headers is sufficient for identifying the game, and we'll have to trust
   * that our players aren't modifying anything else on the disc.
   */
  rc_cd_read_sector(track_handle, 0, buffer, sizeof(buffer));

  return rc_hash_buffer(hash, buffer, sizeof(buffer));
}

static int rc_hash_snes(char hash[33], uint8_t* buffer, size_t buffer_size)
{
  /* if the file contains a header, ignore it */
  uint32_t calc_size = ((uint32_t)buffer_size / 0x2000) * 0x2000;
  if (buffer_size - calc_size == 512)
  {
    rc_hash_verbose("Ignoring SNES header");

    buffer += 512;
    buffer_size -= 512;
  }

  return rc_hash_buffer(hash, buffer, buffer_size);
}

int rc_hash_generate_from_buffer(char hash[33], int console_id, uint8_t* buffer, size_t buffer_size)
{
  switch (console_id)
  {
    default:
    {
      char message[128];
      snprintf(message, sizeof(message), "Unsupported console for buffer hash: %d", console_id);
      return rc_hash_error(message);
    }

    case RC_CONSOLE_APPLE_II:
    case RC_CONSOLE_ATARI_2600:
    case RC_CONSOLE_ATARI_7800:
    case RC_CONSOLE_ATARI_JAGUAR:
    case RC_CONSOLE_COLECOVISION:
    case RC_CONSOLE_GAMEBOY:
    case RC_CONSOLE_GAMEBOY_ADVANCE:
    case RC_CONSOLE_GAMEBOY_COLOR:
    case RC_CONSOLE_GAME_GEAR:
    case RC_CONSOLE_INTELLIVISION:
    case RC_CONSOLE_MASTER_SYSTEM:
    case RC_CONSOLE_MEGA_DRIVE:
    case RC_CONSOLE_MSX:
    case RC_CONSOLE_NEOGEO_POCKET:
    case RC_CONSOLE_NINTENDO_64:
    case RC_CONSOLE_ORIC:
    case RC_CONSOLE_PC_ENGINE: /* NOTE: does not support PCEngine CD */
    case RC_CONSOLE_PC8800:
    case RC_CONSOLE_POKEMON_MINI:
    case RC_CONSOLE_SEGA_32X:
    case RC_CONSOLE_SG1000:
    case RC_CONSOLE_VECTREX:
    case RC_CONSOLE_VIRTUAL_BOY:
    case RC_CONSOLE_WONDERSWAN:
      return rc_hash_buffer(hash, buffer, buffer_size);

    case RC_CONSOLE_ATARI_LYNX:
      return rc_hash_lynx(hash, buffer, buffer_size);

    case RC_CONSOLE_NINTENDO:
      return rc_hash_nes(hash, buffer, buffer_size);

    case RC_CONSOLE_SUPER_NINTENDO:
      return rc_hash_snes(hash, buffer, buffer_size);
  }
}

static int rc_hash_whole_file(char hash[33], int console_id, const char* path)
{
  md5_state_t md5;
  uint8_t* buffer;
  size_t size;
  const size_t buffer_size = 65536;
  void* file_handle;
  int result = 0;

  file_handle = rc_file_open(path);
  if (!file_handle)
    return rc_hash_error("Could not open file");

  rc_file_seek(file_handle, 0, SEEK_END);
  size = rc_file_tell(file_handle);

  if (verbose_message_callback)
  {
    char message[1024];
    if (size > MAX_BUFFER_SIZE)
      snprintf(message, sizeof(message), "Hashing first %u bytes (of %u bytes) of %s", MAX_BUFFER_SIZE, (unsigned)size, rc_path_get_filename(path));
    else
      snprintf(message, sizeof(message), "Hashing %s (%u bytes)", rc_path_get_filename(path), (unsigned)size);
    verbose_message_callback(message);
  }

  if (size > MAX_BUFFER_SIZE)
    size = MAX_BUFFER_SIZE;

  md5_init(&md5);

  buffer = (uint8_t*)malloc(buffer_size);
  if (buffer)
  {
    rc_file_seek(file_handle, 0, SEEK_SET);
    while (size >= buffer_size)
    {
      rc_file_read(file_handle, buffer, (int)buffer_size);
      md5_append(&md5, buffer, (int)buffer_size);
      size -= buffer_size;
    }

    if (size > 0)
    {
      rc_file_read(file_handle, buffer, (int)size);
      md5_append(&md5, buffer, (int)size);
    }

    free(buffer);
    result = rc_hash_finalize(&md5, hash);
  }

  rc_file_close(file_handle);
  return result;
}

static int rc_hash_buffered_file(char hash[33], int console_id, const char* path)
{
  uint8_t* buffer;
  size_t size;
  int result = 0;
  void* file_handle;

  file_handle = rc_file_open(path);
  if (!file_handle)
    return rc_hash_error("Could not open file");

  rc_file_seek(file_handle, 0, SEEK_END);
  size = rc_file_tell(file_handle);

  if (verbose_message_callback)
  {
    char message[1024];
    if (size > MAX_BUFFER_SIZE)
      snprintf(message, sizeof(message), "Buffering first %u bytes (of %d bytes) of %s", MAX_BUFFER_SIZE, (unsigned)size, rc_path_get_filename(path));
    else
      snprintf(message, sizeof(message), "Buffering %s (%d bytes)", rc_path_get_filename(path), (unsigned)size);
    verbose_message_callback(message);
  }

  if (size > MAX_BUFFER_SIZE)
    size = MAX_BUFFER_SIZE;

  buffer = (uint8_t*)malloc(size);
  if (buffer)
  {
    rc_file_seek(file_handle, 0, SEEK_SET);
    rc_file_read(file_handle, buffer, (int)size);

    result = rc_hash_generate_from_buffer(hash, console_id, buffer, size);

    free(buffer);
  }

  rc_file_close(file_handle);
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

static const char* rc_hash_get_first_item_from_playlist(const char* path)
{
  char buffer[1024];
  char* disc_path;
  char* ptr, *start, *next;
  size_t num_read, path_len, file_len;
  void* file_handle;

  file_handle = rc_file_open(path);
  if (!file_handle)
  {
    rc_hash_error("Could not open playlist");
    return NULL;
  }

  num_read = rc_file_read(file_handle, buffer, sizeof(buffer) - 1);
  buffer[num_read] = '\0';

  rc_file_close(file_handle);

  ptr = buffer;
  do
  {
    /* ignore empty and commented lines */
    while (*ptr == '#' || *ptr == '\r' || *ptr == '\n')
    {
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

  if (verbose_message_callback)
  {
    char message[1024];
    snprintf(message, sizeof(message), "Extracted %.*s from playlist", (int)file_len, start);
    verbose_message_callback(message);
  }

  start[file_len++] = '\0';
  if (rc_hash_path_is_absolute(start))
    path_len = 0;
  else
    path_len = rc_path_get_filename(path) - path;

  disc_path = (char*)malloc(path_len + file_len + 1);
  if (!disc_path)
    return NULL;

  if (path_len)
    memcpy(disc_path, path, path_len);

  memcpy(&disc_path[path_len], start, file_len);
  return disc_path;
}

static int rc_hash_generate_from_playlist(char hash[33], int console_id, const char* path)
{
  int result;
  const char* disc_path;

  if (verbose_message_callback)
  {
    char message[1024];
    snprintf(message, sizeof(message), "Processing playlist: %s", rc_path_get_filename(path));
    verbose_message_callback(message);
  }

  disc_path = rc_hash_get_first_item_from_playlist(path);
  if (!disc_path)
    return rc_hash_error("Failed to get first item from playlist");

  result = rc_hash_generate_from_file(hash, console_id, disc_path);

  free((void*)disc_path);
  return result;
}

int rc_hash_generate_from_file(char hash[33], int console_id, const char* path)
{
  switch (console_id)
  {
    default:
    {
      char buffer[128];
      snprintf(buffer, sizeof(buffer), "Unsupported console for file hash: %d", console_id);
      return rc_hash_error(buffer);
    }

    case RC_CONSOLE_APPLE_II:
    case RC_CONSOLE_ATARI_2600:
    case RC_CONSOLE_ATARI_7800:
    case RC_CONSOLE_ATARI_JAGUAR:
    case RC_CONSOLE_COLECOVISION:
    case RC_CONSOLE_GAMEBOY:
    case RC_CONSOLE_GAMEBOY_ADVANCE:
    case RC_CONSOLE_GAMEBOY_COLOR:
    case RC_CONSOLE_GAME_GEAR:
    case RC_CONSOLE_INTELLIVISION:
    case RC_CONSOLE_MASTER_SYSTEM:
    case RC_CONSOLE_MEGA_DRIVE:
    case RC_CONSOLE_NEOGEO_POCKET:
    case RC_CONSOLE_NINTENDO_64:
    case RC_CONSOLE_ORIC:
    case RC_CONSOLE_POKEMON_MINI:
    case RC_CONSOLE_SEGA_32X:
    case RC_CONSOLE_SG1000:
    case RC_CONSOLE_VECTREX:
    case RC_CONSOLE_VIRTUAL_BOY:
    case RC_CONSOLE_WONDERSWAN:
      /* generic whole-file hash - don't buffer */
      return rc_hash_whole_file(hash, console_id, path);

    case RC_CONSOLE_MSX:
    case RC_CONSOLE_PC8800:
      /* generic whole-file hash with m3u support - don't buffer */
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, path);

      return rc_hash_whole_file(hash, console_id, path);

    case RC_CONSOLE_ATARI_LYNX:
    case RC_CONSOLE_NINTENDO:
    case RC_CONSOLE_SUPER_NINTENDO:
      /* additional logic whole-file hash - buffer then call rc_hash_generate_from_buffer */
      return rc_hash_buffered_file(hash, console_id, path);

    case RC_CONSOLE_3DO:
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, path);

      return rc_hash_3do(hash, path);

    case RC_CONSOLE_ARCADE:
      return rc_hash_arcade(hash, path);

    case RC_CONSOLE_NINTENDO_DS:
      return rc_hash_nintendo_ds(hash, path);

    case RC_CONSOLE_PC_ENGINE:
      if (rc_path_compare_extension(path, "cue") || rc_path_compare_extension(path, "chd"))
        return rc_hash_pce_cd(hash, path);

      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, path);

      return rc_hash_whole_file(hash, console_id, path);

    case RC_CONSOLE_PCFX:
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, path);

      return rc_hash_pcfx_cd(hash, path);

    case RC_CONSOLE_PLAYSTATION:
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, path);

      return rc_hash_psx(hash, path);

    case RC_CONSOLE_SEGA_CD:
    case RC_CONSOLE_SATURN:
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, path);

      return rc_hash_sega_cd(hash, path);
  }
}

static void rc_hash_iterator_append_console(struct rc_hash_iterator* iterator, int console_id)
{
  int i = 0;
  while (iterator->consoles[i] != 0)
  {
    if (iterator->consoles[i] == console_id)
      return;

    ++i;
  }

  iterator->consoles[i] = console_id;
}

static void rc_hash_initialize_dsk_iterator(struct rc_hash_iterator* iterator, const char* path)
{
  size_t size = iterator->buffer_size;
  if (size == 0)
  {
    /* attempt to use disk size to determine system */
    void* file = rc_file_open(path);
    if (file)
    {
      rc_file_seek(file, 0, SEEK_END);
      size = rc_file_tell(file);
      rc_file_close(file);
    }
  }

  if (size == 512 * 9 * 80) /* 360KB */
  {
    /* FAT-12 3.5" DD (512 byte sectors, 9 sectors per track, 80 tracks per side */
    /* FAT-12 5.25" DD double-sided (512 byte sectors, 9 sectors per track, 80 tracks per side */
    iterator->consoles[0] = RC_CONSOLE_MSX;
  }
  else if (size == 512 * 9 * 80 * 2) /* 720KB */
  {
    /* FAT-12 3.5" DD double-sided (512 byte sectors, 9 sectors per track, 80 tracks per side */
    iterator->consoles[0] = RC_CONSOLE_MSX;
  }
  else if (size == 512 * 9 * 40) /* 180KB */
  {
    /* FAT-12 5.25" DD (512 byte sectors, 9 sectors per track, 40 tracks per side */
    iterator->consoles[0] = RC_CONSOLE_MSX;
  }
  else if (size == 256 * 16 * 35) /* 140KB */
  {
    /* Apple II new format - 256 byte sectors, 16 sectors per track, 35 tracks per side */
    iterator->consoles[0] = RC_CONSOLE_APPLE_II;
  }
  else if (size == 256 * 13 * 35) /* 113.75KB */
  {
    /* Apple II old format - 256 byte sectors, 13 sectors per track, 35 tracks per side */
    iterator->consoles[0] = RC_CONSOLE_APPLE_II;
  }

  /* once a best guess has been identified, make sure the others are added as fallbacks */

  /* check MSX first, as Apple II isn't supported by RetroArch, and RAppleWin won't use the iterator */
  rc_hash_iterator_append_console(iterator, RC_CONSOLE_MSX);
  rc_hash_iterator_append_console(iterator, RC_CONSOLE_APPLE_II);
}

void rc_hash_initialize_iterator(struct rc_hash_iterator* iterator, const char* path, uint8_t* buffer, size_t buffer_size)
{
  int need_path = !buffer;

  memset(iterator, 0, sizeof(*iterator));
  iterator->buffer = buffer;
  iterator->buffer_size = buffer_size;

  iterator->consoles[0] = 0;

  do
  {
    const char* ext = rc_path_get_extension(path);
    switch (tolower(*ext))
    {
      case '7':
        if (rc_path_compare_extension(ext, "7z"))
        {
          /* decompressing zip file not supported */
          iterator->consoles[0] = RC_CONSOLE_ARCADE;
          need_path = 1;
        }
        break;

      case 'a':
        if (rc_path_compare_extension(ext, "a78"))
        {
          iterator->consoles[0] = RC_CONSOLE_ATARI_7800;
        }
        break;

      case 'b':
        if (rc_path_compare_extension(ext, "bin"))
        {
           if (buffer_size == 0)
           {
              /* raw bin file may be a CD track. if it's more than 32MB, try a CD hash. */
              void* file = rc_file_open(path);
              if (file)
              {
                 size_t size;

                 rc_file_seek(file, 0, SEEK_END);
                 size = rc_file_tell(file);
                 rc_file_close(file);

                 if (size > 32 * 1024 * 1024)
                 {
                    iterator->consoles[0] = RC_CONSOLE_3DO; /* 4DO supports directly opening the bin file */
                    iterator->consoles[1] = RC_CONSOLE_PLAYSTATION; /* PCSX ReARMed supports directly opening the bin file*/

                    /* SEGA CD hash doesn't have any logic to ensure it's being used against a SEGA CD, so it should always be last */
                    iterator->consoles[2] = RC_CONSOLE_SEGA_CD; /* Genesis Plus GX supports directly opening the bin file*/

                    /* fallback to megadrive - will only be checked if SEGA CD hash does not match */
                    iterator->consoles[3] = RC_CONSOLE_MEGA_DRIVE;
                    break;
                 }
              }
           }

          /* bin is associated with MegaDrive, Sega32X and Atari 2600. Since they all use the same
           * hashing algorithm, only specify one of them */
          iterator->consoles[0] = RC_CONSOLE_MEGA_DRIVE;
        }
        else if (rc_path_compare_extension(ext, "bs"))
        {
           iterator->consoles[0] = RC_CONSOLE_SUPER_NINTENDO;
        }
        break;

      case 'c':
        if (rc_path_compare_extension(ext, "cue") || rc_path_compare_extension(ext, "chd"))
        {
          iterator->consoles[0] = RC_CONSOLE_PLAYSTATION;
          iterator->consoles[1] = RC_CONSOLE_PC_ENGINE;
          iterator->consoles[2] = RC_CONSOLE_3DO;
          iterator->consoles[3] = RC_CONSOLE_PCFX;
          /* SEGA CD hash doesn't have any logic to ensure it's being used against a SEGA CD, so it should always be last */
          iterator->consoles[4] = RC_CONSOLE_SEGA_CD;
          need_path = 1;
        }
        else if (rc_path_compare_extension(ext, "col"))
        {
          iterator->consoles[0] = RC_CONSOLE_COLECOVISION;
        }
        else if (rc_path_compare_extension(ext, "cas"))
        {
          iterator->consoles[0] = RC_CONSOLE_MSX;
        }
        break;

      case 'd':
        if (rc_path_compare_extension(ext, "dsk"))
        {
          rc_hash_initialize_dsk_iterator(iterator, path);
        }
        else if (rc_path_compare_extension(ext, "d88"))
        {
          iterator->consoles[0] = RC_CONSOLE_PC8800;
        }
        break;

      case 'f':
        if (rc_path_compare_extension(ext, "fig"))
        {
          iterator->consoles[0] = RC_CONSOLE_SUPER_NINTENDO;
        }
        else if (rc_path_compare_extension(ext, "fds"))
        {
          iterator->consoles[0] = RC_CONSOLE_NINTENDO;
        }
        break;

      case 'g':
        if (rc_path_compare_extension(ext, "gba"))
        {
          iterator->consoles[0] = RC_CONSOLE_GAMEBOY_ADVANCE;
        }
        else if (rc_path_compare_extension(ext, "gbc"))
        {
          iterator->consoles[0] = RC_CONSOLE_GAMEBOY_COLOR;
        }
        else if (rc_path_compare_extension(ext, "gb"))
        {
          iterator->consoles[0] = RC_CONSOLE_GAMEBOY;
        }
        else if (rc_path_compare_extension(ext, "gg"))
        {
          iterator->consoles[0] = RC_CONSOLE_GAME_GEAR;
        }
        break;

      case 'i':
        if (rc_path_compare_extension(ext, "iso"))
        {
          iterator->consoles[0] = RC_CONSOLE_3DO;
          iterator->consoles[1] = RC_CONSOLE_SEGA_CD;
          need_path = 1;
        }
        break;

      case 'j':
        if (rc_path_compare_extension(ext, "jag"))
        {
          iterator->consoles[0] = RC_CONSOLE_ATARI_JAGUAR;
        }
        break;

      case 'l':
        if (rc_path_compare_extension(ext, "lnx"))
        {
          iterator->consoles[0] = RC_CONSOLE_ATARI_LYNX;
        }
        break;

      case 'm':
        if (rc_path_compare_extension(ext, "m3u"))
        {
          const char* disc_path = rc_hash_get_first_item_from_playlist(path);
          if (!disc_path) /* did not find a disc */
            return;

          path = iterator->path = disc_path;
          continue; /* retry with disc_path */
        }
        else if (rc_path_compare_extension(ext, "md"))
        {
          iterator->consoles[0] = RC_CONSOLE_MEGA_DRIVE;
        }
        else if (rc_path_compare_extension(ext, "min"))
        {
          iterator->consoles[0] = RC_CONSOLE_POKEMON_MINI;
        }
        else if (rc_path_compare_extension(ext, "mx1"))
        {
          iterator->consoles[0] = RC_CONSOLE_MSX;
        }
        else if (rc_path_compare_extension(ext, "mx2"))
        {
          iterator->consoles[0] = RC_CONSOLE_MSX;
        }
        break;

      case 'n':
        if (rc_path_compare_extension(ext, "nes"))
        {
          iterator->consoles[0] = RC_CONSOLE_NINTENDO;
        }
        else if (rc_path_compare_extension(ext, "nds"))
        {
          iterator->consoles[0] = RC_CONSOLE_NINTENDO_DS;
        }
        else if (rc_path_compare_extension(ext, "n64"))
        {
          iterator->consoles[0] = RC_CONSOLE_NINTENDO_64;
        }
        else if (rc_path_compare_extension(ext, "ngc"))
        {
          iterator->consoles[0] = RC_CONSOLE_NEOGEO_POCKET;
        }
        break;

      case 'p':
        if (rc_path_compare_extension(ext, "pce"))
        {
          iterator->consoles[0] = RC_CONSOLE_PC_ENGINE;
        }
        break;

      case 'r':
        if (rc_path_compare_extension(ext, "rom"))
        {
          iterator->consoles[0] = RC_CONSOLE_MSX;
        }
        if (rc_path_compare_extension(ext, "ri"))
        {
          iterator->consoles[0] = RC_CONSOLE_MSX;
        }
        break;

      case 's':
        if (rc_path_compare_extension(ext, "smc") ||
            rc_path_compare_extension(ext, "sfc") ||
            rc_path_compare_extension(ext, "swc"))
        {
          iterator->consoles[0] = RC_CONSOLE_SUPER_NINTENDO;
        }
        else if (rc_path_compare_extension(ext, "sg"))
        {
          iterator->consoles[0] = RC_CONSOLE_SG1000;
        }
        else if (rc_path_compare_extension(ext, "sgx"))
        {
          iterator->consoles[0] = RC_CONSOLE_PC_ENGINE;
        }
        break;

      case 't':
        if (rc_path_compare_extension(ext, "tap"))
        {
          iterator->consoles[0] = RC_CONSOLE_ORIC;
        }
        break;

      case 'v':
        if (rc_path_compare_extension(ext, "vb"))
        {
          iterator->consoles[0] = RC_CONSOLE_VIRTUAL_BOY;
        }
        break;

      case 'w':
        if (rc_path_compare_extension(ext, "wsc"))
        {
          iterator->consoles[0] = RC_CONSOLE_WONDERSWAN;
        }
        else if (rc_path_compare_extension(ext, "woz"))
        {
          iterator->consoles[0] = RC_CONSOLE_APPLE_II;
        }
        break;

      case 'z':
        if (rc_path_compare_extension(ext, "zip"))
        {
          /* decompressing zip file not supported */
          iterator->consoles[0] = RC_CONSOLE_ARCADE;
          need_path = 1;
        }
        break;
    }

    if (verbose_message_callback)
    {
      char message[256];
      int count = 0;
      while (iterator->consoles[count])
        ++count;

      snprintf(message, sizeof(message), "Found %d potential consoles for %s file extension", count, ext);
      verbose_message_callback(message);
    }

    /* loop is only for specific cases that redirect to another file - like m3u */
    break;
  } while (1);

  if (need_path && !iterator->path)
    iterator->path = strdup(path);

  /* if we didn't match the extension, default to something that does a whole file hash */
  if (!iterator->consoles[0])
    iterator->consoles[0] = RC_CONSOLE_GAMEBOY;
}

void rc_hash_destroy_iterator(struct rc_hash_iterator* iterator)
{
  if (iterator->path)
  {
    free((void*)iterator->path);
    iterator->path = NULL;
  }
}

int rc_hash_iterate(char hash[33], struct rc_hash_iterator* iterator)
{
  int next_console;
  int result = 0;

  do
  {
    next_console = iterator->consoles[iterator->index];
    if (next_console == 0)
    {
      hash[0] = '\0';
      break;
    }

    ++iterator->index;

    if (verbose_message_callback)
    {
      char message[128];
      snprintf(message, sizeof(message), "Trying console %d", next_console);
      verbose_message_callback(message);
    }

    if (iterator->buffer)
      result = rc_hash_generate_from_buffer(hash, next_console, iterator->buffer, iterator->buffer_size);
    else
      result = rc_hash_generate_from_file(hash, next_console, iterator->path);

  } while (!result);

  return result;
}
