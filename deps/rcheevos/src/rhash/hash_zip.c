#include "rc_hash_internal.h"

#include "../rc_compat.h"

struct rc_hash_zip_idx
{
  size_t length;
  uint8_t* data;
};

static int rc_hash_zip_idx_sort(const void* a, const void* b)
{
  struct rc_hash_zip_idx* A = (struct rc_hash_zip_idx*)a, * B = (struct rc_hash_zip_idx*)b;
  size_t len = (A->length < B->length ? A->length : B->length);
  return memcmp(A->data, B->data, len);
}

typedef int (RC_CCONV* rc_hash_zip_filter_t)(const char* filename, uint32_t filename_len, uint64_t decomp_size, void* userdata);

static int rc_hash_zip_file(md5_state_t* md5, void* file_handle,
                            const rc_hash_iterator_t* iterator,
                            rc_hash_zip_filter_t filter_func, void* filter_userdata)
{
  uint8_t buf[2048], *alloc_buf, *cdir_start, *cdir_max, *cdir, *hashdata, eocdirhdr_size, cdirhdr_size, nparents;
  uint32_t cdir_entry_len;
  size_t sizeof_idx, indices_offset, alloc_size;
  int64_t i_file, archive_size, ecdh_ofs, total_files, cdir_size, cdir_ofs;
  struct rc_hash_zip_idx* hashindices, *hashindex;

  rc_file_seek(iterator, file_handle, 0, SEEK_END);
  archive_size = rc_file_tell(iterator, file_handle);

  /* Basic sanity checks - reject files which are too small */
  eocdirhdr_size = 22; /* the 'end of central directory header' is 22 bytes */
  if (archive_size < eocdirhdr_size)
    return rc_hash_iterator_error(iterator, "ZIP is too small");

  /* Macros used for reading ZIP and writing to a buffer for hashing (undefined again at the end of the function) */
  #define RC_ZIP_READ_LE16(p) ((uint16_t)(((const uint8_t*)(p))[0]) | ((uint16_t)(((const uint8_t*)(p))[1]) << 8U))
  #define RC_ZIP_READ_LE32(p) ((uint32_t)(((const uint8_t*)(p))[0]) | ((uint32_t)(((const uint8_t*)(p))[1]) << 8U) | ((uint32_t)(((const uint8_t*)(p))[2]) << 16U) | ((uint32_t)(((const uint8_t*)(p))[3]) << 24U))
  #define RC_ZIP_READ_LE64(p) ((uint64_t)(((const uint8_t*)(p))[0]) | ((uint64_t)(((const uint8_t*)(p))[1]) << 8U) | ((uint64_t)(((const uint8_t*)(p))[2]) << 16U) | ((uint64_t)(((const uint8_t*)(p))[3]) << 24U) | ((uint64_t)(((const uint8_t*)(p))[4]) << 32U) | ((uint64_t)(((const uint8_t*)(p))[5]) << 40U) | ((uint64_t)(((const uint8_t*)(p))[6]) << 48U) | ((uint64_t)(((const uint8_t*)(p))[7]) << 56U))
  #define RC_ZIP_WRITE_LE32(p,v) { ((uint8_t*)(p))[0] = (uint8_t)((uint32_t)(v) & 0xFF); ((uint8_t*)(p))[1] = (uint8_t)(((uint32_t)(v) >> 8) & 0xFF); ((uint8_t*)(p))[2] = (uint8_t)(((uint32_t)(v) >> 16) & 0xFF); ((uint8_t*)(p))[3] = (uint8_t)((uint32_t)(v) >> 24); }
  #define RC_ZIP_WRITE_LE64(p,v) { ((uint8_t*)(p))[0] = (uint8_t)((uint64_t)(v) & 0xFF); ((uint8_t*)(p))[1] = (uint8_t)(((uint64_t)(v) >> 8) & 0xFF); ((uint8_t*)(p))[2] = (uint8_t)(((uint64_t)(v) >> 16) & 0xFF); ((uint8_t*)(p))[3] = (uint8_t)(((uint64_t)(v) >> 24) & 0xFF); ((uint8_t*)(p))[4] = (uint8_t)(((uint64_t)(v) >> 32) & 0xFF); ((uint8_t*)(p))[5] = (uint8_t)(((uint64_t)(v) >> 40) & 0xFF); ((uint8_t*)(p))[6] = (uint8_t)(((uint64_t)(v) >> 48) & 0xFF); ((uint8_t*)(p))[7] = (uint8_t)((uint64_t)(v) >> 56); }

  /* Find the end of central directory record by scanning the file from the end towards the beginning */
  for (ecdh_ofs = archive_size - sizeof(buf); ; ecdh_ofs -= (sizeof(buf) - 3)) {
    int i, n = sizeof(buf);
    if (ecdh_ofs < 0)
      ecdh_ofs = 0;
    if (n > archive_size)
      n = (int)archive_size;

    rc_file_seek(iterator, file_handle, ecdh_ofs, SEEK_SET);
    if (rc_file_read(iterator, file_handle, buf, n) != (size_t)n)
      return rc_hash_iterator_error(iterator, "ZIP read error");

    for (i = n - 4; i >= 0; --i) {
      if (buf[i] == 'P' && RC_ZIP_READ_LE32(buf + i) == 0x06054b50) /* end of central directory header signature */
        break;
    }

    if (i >= 0) {
      ecdh_ofs += i;
      break;
    }

    if (!ecdh_ofs || (archive_size - ecdh_ofs) >= (0xFFFF + eocdirhdr_size))
      return rc_hash_iterator_error(iterator, "Failed to find ZIP central directory");
  }

  /* Read and verify the end of central directory record. */
  rc_file_seek(iterator, file_handle, ecdh_ofs, SEEK_SET);
  if (rc_file_read(iterator, file_handle, buf, eocdirhdr_size) != eocdirhdr_size)
    return rc_hash_iterator_error(iterator, "Failed to read ZIP central directory");

  /* Read central dir information from end of central directory header */
  total_files = RC_ZIP_READ_LE16(buf + 0x0A);
  cdir_size = RC_ZIP_READ_LE32(buf + 0x0C);
  cdir_ofs = RC_ZIP_READ_LE32(buf + 0x10);

  /* Check if this is a Zip64 file. In the block of code below:
   * - 20 is the size of the ZIP64 end of central directory locator
   * - 56 is the size of the ZIP64 end of central directory header
   */
  if ((cdir_ofs == 0xFFFFFFFF || cdir_size == 0xFFFFFFFF || total_files == 0xFFFF) && ecdh_ofs >= (20 + 56)) {
    /* Read the ZIP64 end of central directory locator if it actually exists */
    rc_file_seek(iterator, file_handle, ecdh_ofs - 20, SEEK_SET);
    if (rc_file_read(iterator, file_handle, buf, 20) == 20 && RC_ZIP_READ_LE32(buf) == 0x07064b50) { /* locator signature */
      /* Found the locator, now read the actual ZIP64 end of central directory header */
      int64_t ecdh64_ofs = (int64_t)RC_ZIP_READ_LE64(buf + 0x08);
      if (ecdh64_ofs <= (archive_size - 56)) {
        rc_file_seek(iterator, file_handle, ecdh64_ofs, SEEK_SET);
        if (rc_file_read(iterator, file_handle, buf, 56) == 56 && RC_ZIP_READ_LE32(buf) == 0x06064b50) { /* header signature */
          total_files = RC_ZIP_READ_LE64(buf + 0x20);
          cdir_size = RC_ZIP_READ_LE64(buf + 0x28);
          cdir_ofs = RC_ZIP_READ_LE64(buf + 0x30);
        }
      }
    }
  }

  /* Basic verificaton of central directory (limit to a 256MB content directory) */
  cdirhdr_size = 46; /* the 'central directory header' is 46 bytes */
  if ((cdir_size >= 0x10000000) || (cdir_size < total_files * cdirhdr_size) || ((cdir_ofs + cdir_size) > archive_size))
    return rc_hash_iterator_error(iterator, "Central directory of ZIP file is invalid");

  /* Allocate once for both directory and our temporary sort index (memory aligned to sizeof(rc_hash_zip_idx)) */
  sizeof_idx = sizeof(struct rc_hash_zip_idx);
  indices_offset = (size_t)((cdir_size + sizeof_idx - 1) / sizeof_idx * sizeof_idx);
  alloc_size = (size_t)(indices_offset + total_files * sizeof_idx);
  alloc_buf = (uint8_t*)malloc(alloc_size);

  /* Read entire central directory to a buffer */
  if (!alloc_buf)
    return rc_hash_iterator_error(iterator, "Could not allocate temporary buffer");

  rc_file_seek(iterator, file_handle, cdir_ofs, SEEK_SET);
  if ((int64_t)rc_file_read(iterator, file_handle, alloc_buf, (int)cdir_size) != cdir_size) {
    free(alloc_buf);
    return rc_hash_iterator_error(iterator, "Failed to read central directory of ZIP file");
  }

  cdir_start = alloc_buf;
  cdir_max = cdir_start + cdir_size - cdirhdr_size;
  cdir = cdir_start;

  /* Write our temporary hash data to the same buffer we read the central directory from.
   * We can do that because the amount of data we keep for each file is guaranteed to be less than the file record.
   */
  hashdata = alloc_buf;
  hashindices = (struct rc_hash_zip_idx*)(alloc_buf + indices_offset);
  hashindex = hashindices;

  /* Now process the central directory file records */
  for (i_file = nparents = 0, cdir = cdir_start; i_file < total_files && cdir >= cdir_start && cdir <= cdir_max; i_file++, cdir += cdir_entry_len) {
    const uint8_t* name, * name_end;
    uint32_t signature = RC_ZIP_READ_LE32(cdir + 0x00);
    uint32_t method = RC_ZIP_READ_LE16(cdir + 0x0A);
    uint32_t crc32 = RC_ZIP_READ_LE32(cdir + 0x10);
    uint64_t comp_size = RC_ZIP_READ_LE32(cdir + 0x14);
    uint64_t decomp_size = RC_ZIP_READ_LE32(cdir + 0x18);
    uint32_t filename_len = RC_ZIP_READ_LE16(cdir + 0x1C);
    int32_t  extra_len = RC_ZIP_READ_LE16(cdir + 0x1E);
    int32_t  comment_len = RC_ZIP_READ_LE16(cdir + 0x20);
    int32_t  external_attr = RC_ZIP_READ_LE16(cdir + 0x26);
    uint64_t local_hdr_ofs = RC_ZIP_READ_LE32(cdir + 0x2A);
    cdir_entry_len = cdirhdr_size + filename_len + extra_len + comment_len;

    if (signature != 0x02014b50) /* expected central directory entry signature */
      break;

    /* Ignore records describing a directory (we only hash file records) */
    name = (cdir + cdirhdr_size);
    if (name[filename_len - 1] == '/' || name[filename_len - 1] == '\\' || (external_attr & 0x10))
      continue;

    /* Handle Zip64 fields */
    if (decomp_size == 0xFFFFFFFF || comp_size == 0xFFFFFFFF || local_hdr_ofs == 0xFFFFFFFF) {
      int invalid = 0;
      const uint8_t* x = cdir + cdirhdr_size + filename_len, * xEnd, * field, * fieldEnd;
      for (xEnd = x + extra_len; (x + (sizeof(uint16_t) * 2)) < xEnd; x = fieldEnd) {
        field = x + (sizeof(uint16_t) * 2);
        fieldEnd = field + RC_ZIP_READ_LE16(x + 2);
        if (RC_ZIP_READ_LE16(x) != 0x0001 || fieldEnd > xEnd)
          continue; /* Not the Zip64 extended information extra field */

        if (decomp_size == 0xFFFFFFFF) {
          if ((unsigned)(fieldEnd - field) < sizeof(uint64_t)) {
            invalid = 1;
            break;
          }

          decomp_size = RC_ZIP_READ_LE64(field);
          field += sizeof(uint64_t);
        }

        if (comp_size == 0xFFFFFFFF) {
          if ((unsigned)(fieldEnd - field) < sizeof(uint64_t)) {
            invalid = 1;
            break;
          }

          comp_size = RC_ZIP_READ_LE64(field);
          field += sizeof(uint64_t);
        }

        if (local_hdr_ofs == 0xFFFFFFFF) {
          if ((unsigned)(fieldEnd - field) < sizeof(uint64_t)) {
            invalid = 1;
            break;
          }

          local_hdr_ofs = RC_ZIP_READ_LE64(field);
          field += sizeof(uint64_t);
        }

        break;
      }

      if (invalid) {
        free(alloc_buf);
        return rc_hash_iterator_error(iterator, "Encountered invalid Zip64 file");
      }
    }

    /* Basic sanity check on file record */
    /* 30 is the length of the local directory header preceeding the compressed data */
    if ((!method && decomp_size != comp_size) || (decomp_size && !comp_size) ||
        ((local_hdr_ofs + 30 + comp_size) > (uint64_t)archive_size)) {
      free(alloc_buf);
      return rc_hash_iterator_error(iterator, "Encountered invalid entry in ZIP central directory");
    }

    if (filter_func) {
      int filtered = filter_func((const char*)name, filename_len, decomp_size, filter_userdata);
      if (filtered < 0) {
        free(alloc_buf);
        return 0;
      }

      if (filtered) /* this file shouldn't be hashed */
        continue;
    }

    /* Write the pointer and length of the data we record about this file */
    hashindex->data = hashdata;
    hashindex->length = filename_len + 1 + 4 + 8;
    hashindex++;

    rc_hash_iterator_verbose_formatted(iterator, "File in ZIP: %.*s (%u bytes, CRC32 = %08X)", filename_len, (const char*)name, (unsigned)decomp_size, crc32);

    /* Convert and store the file name in the hash data buffer */
    for (name_end = name + filename_len; name != name_end; name++) {
      *(hashdata++) =
        (*name == '\\' ? '/' : /* convert back-slashes to regular slashes */
          (*name >= 'A' && *name <= 'Z') ? (*name | 0x20) : /* convert upper case letters to lower case */
          *name); /* else use the byte as-is */
    }

    /* Add zero terminator, CRC32 and decompressed size to the hash data buffer */
    *(hashdata++) = '\0';
    RC_ZIP_WRITE_LE32(hashdata, crc32);
    hashdata += 4;
    RC_ZIP_WRITE_LE64(hashdata, decomp_size);
    hashdata += 8;
  }

  rc_hash_iterator_verbose_formatted(iterator, "Hashing %u files in ZIP archive", (unsigned)(hashindex - hashindices));

  /* Sort the file list indices */
  qsort(hashindices, (hashindex - hashindices), sizeof(struct rc_hash_zip_idx), rc_hash_zip_idx_sort);

  /* Hash the data in the order of the now sorted indices */
  for (; hashindices != hashindex; hashindices++)
    md5_append(md5, hashindices->data, (int)hashindices->length);

  free(alloc_buf);

  return 1;

  #undef RC_ZIP_READ_LE16
  #undef RC_ZIP_READ_LE32
  #undef RC_ZIP_READ_LE64
  #undef RC_ZIP_WRITE_LE32
  #undef RC_ZIP_WRITE_LE64
}

/* ===================================================== */

static int rc_hash_arduboyfx_filter(const char* filename, uint32_t filename_len, uint64_t decomp_size, void* userdata)
{
  (void)decomp_size;
  (void)userdata;

  /* An .arduboy file is a zip file containing an info.json pointing at one or more bin
   * and hex files. It can also contain a bunch of screenshots, but we don't care about
   * those. As they're also referenced in the info.json, we have to ignore that too.
   * Instead of ignoring the info.json and all image files, only process any bin/hex files */
  if (filename_len > 4) {
    const char* ext = &filename[filename_len - 4];
    if (strncasecmp(ext, ".hex", 4) == 0 || strncasecmp(ext, ".bin", 4) == 0)
      return 0; /* keep hex and bin */
  }

  return 1; /* filter everything else */
}

int rc_hash_arduboyfx(char hash[33], const rc_hash_iterator_t* iterator)
{
  md5_state_t md5;
  int res;

  void* file_handle = rc_file_open(iterator, iterator->path);
  if (!file_handle)
    return rc_hash_iterator_error(iterator, "Could not open file");

  md5_init(&md5);
  res = rc_hash_zip_file(&md5, file_handle, iterator, rc_hash_arduboyfx_filter, NULL);
  rc_file_close(iterator, file_handle);

  if (!res)
    return 0;

  return rc_hash_finalize(iterator, &md5, hash);
}

/* ===================================================== */

struct rc_hash_ms_dos_dosz_state {
  const char* path;
  const struct rc_hash_ms_dos_dosz_state* child;

  md5_state_t* md5;
  const rc_hash_iterator_t* iterator;
  void* file_handle;
  uint32_t nparents;
};

static int rc_hash_dosz(struct rc_hash_ms_dos_dosz_state* dosz);

static int rc_hash_ms_dos_parent(const struct rc_hash_ms_dos_dosz_state* child,
                                 const char* parentname, uint32_t parentname_len)
{
  const char* lastfslash = strrchr(child->path, '/');
  const char* lastbslash = strrchr(child->path, '\\');
  const char* lastslash = (lastbslash > lastfslash ? lastbslash : lastfslash);
  size_t dir_len = (lastslash ? (lastslash + 1 - child->path) : 0);
  char* parent_path = (char*)malloc(dir_len + parentname_len + 1);
  struct rc_hash_ms_dos_dosz_state parent;
  const struct rc_hash_ms_dos_dosz_state* check;
  void* parent_handle;
  int parent_res;

  /* Build the path of the parent by combining the directory of the current file with the name */
  if (!parent_path)
    return rc_hash_iterator_error(child->iterator, "Could not allocate temporary buffer");

  memcpy(parent_path, child->path, dir_len);
  memcpy(parent_path + dir_len, parentname, parentname_len);
  parent_path[dir_len + parentname_len] = '\0';

  /* Make sure there is no recursion where a parent DOSZ is an already seen child DOSZ */
  for (check = child->child; check; check = check->child) {
    if (!strcmp(check->path, parent_path)) {
      free(parent_path);
      return rc_hash_iterator_error(child->iterator, "Invalid DOSZ file with recursive parents");
    }
  }

  /* Try to open the parent DOSZ file */
  parent_handle = rc_file_open(child->iterator, parent_path);
  if (!parent_handle) {
    rc_hash_iterator_error_formatted(child->iterator, "DOSZ parent file '%s' does not exist", parent_path);
    free(parent_path);
    return 0;
  }

  /* Fully hash the parent DOSZ ahead of the child */
  memcpy(&parent, child, sizeof(parent));
  parent.path = parent_path;
  parent.child = child;
  parent.file_handle = parent_handle;
  parent_res = rc_hash_dosz(&parent);
  rc_file_close(child->iterator, parent_handle);
  free(parent_path);
  return parent_res;
}

static int rc_hash_ms_dos_dosc(const struct rc_hash_ms_dos_dosz_state* dosz)
{
  size_t path_len = strlen(dosz->path);
  if (dosz->path[path_len - 1] == 'z' || dosz->path[path_len - 1] == 'Z') {
    void* file_handle;
    char* dosc_path = strdup(dosz->path);
    if (!dosc_path)
      return rc_hash_iterator_error(dosz->iterator, "Could not allocate temporary buffer");

    /* Swap the z to c and use the same capitalization, hash the file if it exists */
    dosc_path[path_len - 1] = (dosz->path[path_len - 1] == 'z' ? 'c' : 'C');
    file_handle = rc_file_open(dosz->iterator, dosc_path);
    free(dosc_path);

    if (file_handle) {
      /* Hash the entire contents of the DOSC file */
      int res = rc_hash_zip_file(dosz->md5, file_handle, dosz->iterator, NULL, NULL);
      rc_file_close(dosz->iterator, file_handle);
      if (!res)
        return 0;
    }
  }

  return 1;
}

static int rc_hash_dosz_filter(const char* filename, uint32_t filename_len, uint64_t decomp_size, void* userdata)
{
  struct rc_hash_ms_dos_dosz_state* dosz = (struct rc_hash_ms_dos_dosz_state*)userdata;

  /* A DOSZ file can contain a special empty <base>.dosz.parent file in its root which means a parent dosz file is used */
  if (decomp_size == 0 && filename_len > 7 &&
    strncasecmp(&filename[filename_len - 7], ".parent", 7) == 0 &&
    !memchr(filename, '/', filename_len) &&
    !memchr(filename, '\\', filename_len))
  {
    /* A DOSZ file can only have one parent file */
    if (dosz->nparents++)
      return -1;

    /* process the parent. if it fails, stop */
    if (!rc_hash_ms_dos_parent(dosz, filename, (filename_len - 7)))
      return -1;

    /* We don't hash this meta file so a user is free to rename it and the parent file */
    return 1;
  }

  return 0;
}

static int rc_hash_dosz(struct rc_hash_ms_dos_dosz_state* dosz)
{
  if (!rc_hash_zip_file(dosz->md5, dosz->file_handle, dosz->iterator, rc_hash_dosz_filter, dosz))
    return 0;

  /* A DOSZ file can only have one parent file */
  if (dosz->nparents > 1)
    return rc_hash_iterator_error(dosz->iterator, "Invalid DOSZ file with multiple parents");

  /* Check if an associated .dosc file exists */
  if (!rc_hash_ms_dos_dosc(dosz))
    return 0;

  return 1;
}

int rc_hash_ms_dos(char hash[33], const rc_hash_iterator_t* iterator)
{
  struct rc_hash_ms_dos_dosz_state dosz;
  md5_state_t md5;
  int res;

  void* file_handle = rc_file_open(iterator, iterator->path);
  if (!file_handle)
    return rc_hash_iterator_error(iterator, "Could not open file");

  memset(&dosz, 0, sizeof(dosz));
  dosz.path = iterator->path;
  dosz.file_handle = file_handle;
  dosz.iterator = iterator;
  dosz.md5 = &md5;

  md5_init(&md5);
  res = rc_hash_dosz(&dosz);
  rc_file_close(iterator, file_handle);

  if (!res)
    return 0;

  return rc_hash_finalize(iterator, &md5, hash);
}
