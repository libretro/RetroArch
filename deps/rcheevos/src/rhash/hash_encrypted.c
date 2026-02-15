#include "rc_hash_internal.h"

#include "../rc_compat.h"

#include "aes.h"

/* ===================================================== */

static rc_hash_3ds_get_cia_normal_key_func _3ds_get_cia_normal_key_func = NULL;
static rc_hash_3ds_get_ncch_normal_keys_func _3ds_get_ncch_normal_keys_func = NULL;

void rc_hash_reset_iterator_encrypted(rc_hash_iterator_t* iterator)
{
  iterator->callbacks.encryption.get_3ds_cia_normal_key = _3ds_get_cia_normal_key_func;
  iterator->callbacks.encryption.get_3ds_ncch_normal_keys = _3ds_get_ncch_normal_keys_func;
}

void rc_hash_init_3ds_get_cia_normal_key_func(rc_hash_3ds_get_cia_normal_key_func func)
{
  _3ds_get_cia_normal_key_func = func;
}

void rc_hash_init_3ds_get_ncch_normal_keys_func(rc_hash_3ds_get_ncch_normal_keys_func func)
{
  _3ds_get_ncch_normal_keys_func = func;
}

/* ===================================================== */

static int rc_hash_nintendo_3ds_ncch(md5_state_t* md5, void* file_handle, uint8_t header[0x200],
  struct AES_ctx* cia_aes, const rc_hash_iterator_t* iterator)
{
  struct AES_ctx ncch_aes;
  uint8_t* hash_buffer;
  uint64_t exefs_offset, exefs_real_size;
  uint32_t exefs_buffer_size;
  uint8_t primary_key[AES_KEYLEN], secondary_key[AES_KEYLEN];
  uint8_t fixed_key_flag, no_crypto_flag, seed_crypto_flag;
  uint8_t crypto_method, secondary_key_x_slot;
  uint16_t ncch_version;
  uint32_t i;
  uint8_t primary_key_y[AES_KEYLEN], program_id[sizeof(uint64_t)];
  uint8_t iv[AES_BLOCKLEN], cia_iv[AES_BLOCKLEN];
  uint8_t exefs_section_name[8];
  uint64_t exefs_section_offset, exefs_section_size;

  exefs_offset = ((uint32_t)header[0x1A3] << 24) | (header[0x1A2] << 16) | (header[0x1A1] << 8) | header[0x1A0];
  exefs_real_size = ((uint32_t)header[0x1A7] << 24) | (header[0x1A6] << 16) | (header[0x1A5] << 8) | header[0x1A4];

  /* Offset and size are in "media units" (1 media unit = 0x200 bytes) */
  exefs_offset *= 0x200;
  exefs_real_size *= 0x200;

  if (exefs_real_size > MAX_BUFFER_SIZE)
    exefs_buffer_size = MAX_BUFFER_SIZE;
  else
    exefs_buffer_size = (uint32_t)exefs_real_size;

  /* This region is technically optional, but it should always be present for executable content (i.e. games) */
  if (exefs_offset == 0 || exefs_real_size == 0)
    return rc_hash_iterator_error(iterator, "ExeFS was not available");

  /* NCCH flag 7 is a bitfield of various crypto related flags */
  fixed_key_flag = header[0x188 + 7] & 0x01;
  no_crypto_flag = header[0x188 + 7] & 0x04;
  seed_crypto_flag = header[0x188 + 7] & 0x20;

  ncch_version = (header[0x113] << 8) | header[0x112];

  if (no_crypto_flag == 0) {
    rc_hash_iterator_verbose(iterator, "Encrypted NCCH detected");

    if (fixed_key_flag != 0) {
      /* Fixed crypto key means all 0s for both keys */
      memset(primary_key, 0, sizeof(primary_key));
      memset(secondary_key, 0, sizeof(secondary_key));
      rc_hash_iterator_verbose(iterator, "Using fixed key crypto");
    }
    else {
      if (iterator->callbacks.encryption.get_3ds_ncch_normal_keys == NULL)
        return rc_hash_iterator_error(iterator, "An encrypted NCCH was detected, but the NCCH normal keys callback was not set");

      /* Primary key y is just the first 16 bytes of the header */
      memcpy(primary_key_y, header, sizeof(primary_key_y));

      /* NCCH flag 3 indicates which secondary key x slot is used */
      crypto_method = header[0x188 + 3];

      switch (crypto_method) {
        case 0x00:
          rc_hash_iterator_verbose(iterator, "Using NCCH crypto method v1");
          secondary_key_x_slot = 0x2C;
          break;
        case 0x01:
          rc_hash_iterator_verbose(iterator, "Using NCCH crypto method v2");
          secondary_key_x_slot = 0x25;
          break;
        case 0x0A:
          rc_hash_iterator_verbose(iterator, "Using NCCH crypto method v3");
          secondary_key_x_slot = 0x18;
          break;
        case 0x0B:
          rc_hash_iterator_verbose(iterator, "Using NCCH crypto method v4");
          secondary_key_x_slot = 0x1B;
          break;
        default:
          return rc_hash_iterator_error_formatted(iterator, "Invalid crypto method %02X", (unsigned)crypto_method);
      }

      /* We only need the program id if we're doing seed crypto */
      if (seed_crypto_flag != 0) {
        rc_hash_iterator_verbose(iterator, "Using seed crypto");
        memcpy(program_id, &header[0x118], sizeof(program_id));
      }

      if (iterator->callbacks.encryption.get_3ds_ncch_normal_keys(primary_key_y, secondary_key_x_slot, seed_crypto_flag != 0 ? program_id : NULL, primary_key, secondary_key) == 0)
        return rc_hash_iterator_error(iterator, "Could not obtain NCCH normal keys");
    }

    switch (ncch_version) {
      case 0:
      case 2:
        rc_hash_iterator_verbose(iterator, "Detected NCCH version 0/2");
        for (i = 0; i < 8; i++) {
          /* First 8 bytes is the partition id in reverse byte order */
          iv[7 - i] = header[0x108 + i];
        }

        /* Magic number for ExeFS */
        iv[8] = 2;

        /* Rest of the bytes are 0 */
        memset(&iv[9], 0, sizeof(iv) - 9);
        break;

      case 1:
        rc_hash_iterator_verbose(iterator, "Detected NCCH version 1");
        for (i = 0; i < 8; i++) {
          /* First 8 bytes is the partition id in normal byte order */
          iv[i] = header[0x108 + i];
        }

        /* Next 4 bytes are 0 */
        memset(&iv[8], 0, 4);

        /* Last 4 bytes is the ExeFS byte offset in big endian */
        iv[12] = (exefs_offset >> 24) & 0xFF;
        iv[13] = (exefs_offset >> 16) & 0xFF;
        iv[14] = (exefs_offset >> 8) & 0xFF;
        iv[15] = exefs_offset & 0xFF;
        break;

      default:
        return rc_hash_iterator_error_formatted(iterator, "Invalid NCCH version %04X", (unsigned)ncch_version);
    }
  }

  /* ASSERT: file position must be +0x200 from start of NCCH (i.e. end of header) */
  exefs_offset -= 0x200;

  if (cia_aes) {
    /* CBC decryption works by setting the IV to the encrypted previous block.
     * Normally this means we would need to decrypt the data between the header and the ExeFS so the CIA AES state is correct.
     * However, we can abuse how CBC decryption works and just set the IV to last block we would otherwise decrypt.
     * We don't care about the data betweeen the header and ExeFS, so this works fine. */

    rc_file_seek(iterator, file_handle, (int64_t)exefs_offset - AES_BLOCKLEN, SEEK_CUR);
    if (rc_file_read(iterator, file_handle, cia_iv, AES_BLOCKLEN) != AES_BLOCKLEN)
      return rc_hash_iterator_error(iterator, "Could not read NCCH data");

    AES_ctx_set_iv(cia_aes, cia_iv);
  }
  else {
    /* No encryption present, just skip over the in-between data */
    rc_file_seek(iterator, file_handle, (int64_t)exefs_offset, SEEK_CUR);
  }

  hash_buffer = (uint8_t*)malloc(exefs_buffer_size);
  if (!hash_buffer)
    return rc_hash_iterator_error_formatted(iterator, "Failed to allocate %u bytes", (unsigned)exefs_buffer_size);

  /* Clear out crypto flags to ensure we get the same hash for decrypted and encrypted ROMs */
  memset(&header[0x114], 0, 4);
  header[0x188 + 3] = 0;
  header[0x188 + 7] &= ~(0x20 | 0x04 | 0x01);

  rc_hash_iterator_verbose(iterator, "Hashing 512 byte NCCH header");
  md5_append(md5, header, 0x200);

  rc_hash_iterator_verbose_formatted(iterator, "Hashing %u bytes for ExeFS (at NCCH offset %08X%08X)",
    (unsigned)exefs_buffer_size, (unsigned)(exefs_offset >> 32), (unsigned)exefs_offset);

  if (rc_file_read(iterator, file_handle, hash_buffer, exefs_buffer_size) != exefs_buffer_size) {
    free(hash_buffer);
    return rc_hash_iterator_error(iterator, "Could not read ExeFS data");
  }

  if (cia_aes) {
    rc_hash_iterator_verbose(iterator, "Performing CIA decryption for ExeFS");
    AES_CBC_decrypt_buffer(cia_aes, hash_buffer, exefs_buffer_size);
  }

  if (no_crypto_flag == 0) {
    rc_hash_iterator_verbose(iterator, "Performing NCCH decryption for ExeFS");

    AES_init_ctx_iv(&ncch_aes, primary_key, iv);
    AES_CTR_xcrypt_buffer(&ncch_aes, hash_buffer, 0x200);

    for (i = 0; i < 8; i++) {
      memcpy(exefs_section_name, &hash_buffer[i * 16], sizeof(exefs_section_name));
      exefs_section_offset = ((uint32_t)hash_buffer[i * 16 + 11] << 24) | (hash_buffer[i * 16 + 10] << 16) | (hash_buffer[i * 16 + 9] << 8) | hash_buffer[i * 16 + 8];
      exefs_section_size = ((uint32_t)hash_buffer[i * 16 + 15] << 24) | (hash_buffer[i * 16 + 14] << 16) | (hash_buffer[i * 16 + 13] << 8) | hash_buffer[i * 16 + 12];

      /* 0 size indicates an unused section */
      if (exefs_section_size == 0)
        continue;

      /* Offsets must be aligned by a media unit */
      if (exefs_section_offset & 0x1FF)
        return rc_hash_iterator_error(iterator, "ExeFS section offset is misaligned");

      /* Offset is relative to the end of the header */
      exefs_section_offset += 0x200;

      /* Check against malformed sections */
      if (exefs_section_offset + ((exefs_section_size + 0x1FF) & ~(uint64_t)0x1FF) > (uint64_t)exefs_real_size)
        return rc_hash_iterator_error(iterator, "ExeFS section would overflow");

      if (memcmp(exefs_section_name, "icon", 4) == 0 ||
          memcmp(exefs_section_name, "banner", 6) == 0) {
        /* Align size up by a media unit */
        exefs_section_size = (exefs_section_size + 0x1FF) & ~(uint64_t)0x1FF;
        AES_init_ctx(&ncch_aes, primary_key);
      }
      else {
        /* We don't align size up here, as the padding bytes will use the primary key rather than the secondary key */
        AES_init_ctx(&ncch_aes, secondary_key);
      }

      /* In theory, the section offset + size could be greater than the buffer size */
      /* In practice, this likely never occurs, but just in case it does, ignore the section or constrict the size */
      if (exefs_section_offset + exefs_section_size > exefs_buffer_size) {
        if (exefs_section_offset >= exefs_buffer_size)
          continue;

        exefs_section_size = exefs_buffer_size - exefs_section_offset;
      }

      exefs_section_name[7] = '\0';
      rc_hash_iterator_verbose_formatted(iterator, "Decrypting ExeFS file %s at ExeFS offset %08X with size %08X",
        (const char*)exefs_section_name, (unsigned)exefs_section_offset, (unsigned)exefs_section_size);

      AES_CTR_xcrypt_buffer(&ncch_aes, &hash_buffer[exefs_section_offset], exefs_section_size & ~(uint64_t)0xF);

      if (exefs_section_size & 0x1FF) {
        /* Handle padding bytes, these always use the primary key */
        exefs_section_offset += exefs_section_size;
        exefs_section_size = 0x200 - (exefs_section_size & 0x1FF);

        rc_hash_iterator_verbose_formatted(iterator, "Decrypting ExeFS padding at ExeFS offset %08X with size %08X",
          (unsigned)exefs_section_offset, (unsigned)exefs_section_size);

        /* Align our decryption start to an AES block boundary */
        if (exefs_section_size & 0xF) {
          /* We're a little evil here re-using the IV like this, but this seems to be the best way to deal with this... */
          memcpy(iv, ncch_aes.Iv, sizeof(iv));
          exefs_section_offset &= ~(uint64_t)0xF;

          /* First decrypt these last bytes using the secondary key */
          AES_CTR_xcrypt_buffer(&ncch_aes, &hash_buffer[exefs_section_offset], 0x10 - (exefs_section_size & 0xF));

          /* Now re-encrypt these bytes using the primary key */
          AES_init_ctx_iv(&ncch_aes, primary_key, iv);
          AES_CTR_xcrypt_buffer(&ncch_aes, &hash_buffer[exefs_section_offset], 0x10 - (exefs_section_size & 0xF));

          /* All of the padding can now be decrypted using the primary key */
          AES_ctx_set_iv(&ncch_aes, iv);
          exefs_section_size += 0x10 - (exefs_section_size & 0xF);
        }

        AES_init_ctx(&ncch_aes, primary_key);
        AES_CTR_xcrypt_buffer(&ncch_aes, &hash_buffer[exefs_section_offset], (size_t)exefs_section_size);
      }
    }
  }

  md5_append(md5, hash_buffer, exefs_buffer_size);

  free(hash_buffer);
  return 1;
}

static uint32_t rc_hash_nintendo_3ds_cia_signature_size(uint8_t header[0x200], const rc_hash_iterator_t* iterator)
{
  uint32_t signature_type;

  signature_type = ((uint32_t)header[0] << 24) | (header[1] << 16) | (header[2] << 8) | header[3];
  switch (signature_type) {
    case 0x010000:
    case 0x010003:
      return 0x200 + 0x3C;

    case 0x010001:
    case 0x010004:
      return 0x100 + 0x3C;

    case 0x010002:
    case 0x010005:
      return 0x3C + 0x40;

    default:
      return rc_hash_iterator_error_formatted(iterator, "Invalid signature type %08X", (unsigned)signature_type);
  }
}

static int rc_hash_nintendo_3ds_cia(md5_state_t* md5, void* file_handle, uint8_t header[0x200],
  const rc_hash_iterator_t* iterator)
{
  const uint32_t CIA_HEADER_SIZE = 0x2020; /* Yes, this is larger than the header[0x200], but we only use the beginning of the header */
  const uint64_t CIA_ALIGNMENT_MASK = 64 - 1; /* sizes are aligned by 64 bytes */
  struct AES_ctx aes;
  uint8_t iv[AES_BLOCKLEN], normal_key[AES_KEYLEN], title_key[AES_KEYLEN], title_id[sizeof(uint64_t)];
  uint32_t cert_size, tik_size, tmd_size;
  int64_t cert_offset, tik_offset, tmd_offset, content_offset;
  uint32_t signature_size, i;
  uint16_t content_count;
  uint8_t common_key_index;

  cert_size = ((uint32_t)header[0x0B] << 24) | (header[0x0A] << 16) | (header[0x09] << 8) | header[0x08];
  tik_size = ((uint32_t)header[0x0F] << 24) | (header[0x0E] << 16) | (header[0x0D] << 8) | header[0x0C];
  tmd_size = ((uint32_t)header[0x13] << 24) | (header[0x12] << 16) | (header[0x11] << 8) | header[0x10];

  cert_offset = (CIA_HEADER_SIZE + CIA_ALIGNMENT_MASK) & ~CIA_ALIGNMENT_MASK;
  tik_offset = (cert_offset + cert_size + CIA_ALIGNMENT_MASK) & ~CIA_ALIGNMENT_MASK;
  tmd_offset = (tik_offset + tik_size + CIA_ALIGNMENT_MASK) & ~CIA_ALIGNMENT_MASK;
  content_offset = (tmd_offset + tmd_size + CIA_ALIGNMENT_MASK) & ~CIA_ALIGNMENT_MASK;

  /* Check if this CIA is encrypted, if it isn't, we can hash it right away */

  rc_file_seek(iterator, file_handle, tmd_offset, SEEK_SET);
  if (rc_file_read(iterator, file_handle, header, 4) != 4)
    return rc_hash_iterator_error(iterator, "Could not read TMD signature type");

  signature_size = rc_hash_nintendo_3ds_cia_signature_size(header, iterator);
  if (signature_size == 0)
    return 0; /* rc_hash_nintendo_3ds_cia_signature_size will call rc_hash_error, so we don't need to do so here */

  rc_file_seek(iterator, file_handle, signature_size + 0x9E, SEEK_CUR);
  if (rc_file_read(iterator, file_handle, header, 2) != 2)
    return rc_hash_iterator_error(iterator, "Could not read TMD content count");

  content_count = (header[0] << 8) | header[1];

  rc_file_seek(iterator, file_handle, 0x9C4 - 0x9E - 2, SEEK_CUR);
  for (i = 0; i < content_count; i++) {
    if (rc_file_read(iterator, file_handle, header, 0x30) != 0x30)
      return rc_hash_iterator_error(iterator, "Could not read TMD content chunk");

    /* Content index 0 is the main content (i.e. the 3DS executable)  */
    if (((header[4] << 8) | header[5]) == 0)
      break;

    content_offset += ((uint32_t)header[0xC] << 24) | (header[0xD] << 16) | (header[0xE] << 8) | header[0xF];
  }

  if (i == content_count)
    return rc_hash_iterator_error(iterator, "Could not find main content chunk in TMD");

  if ((header[7] & 1) == 0) {
    /* Not encrypted, we can hash the NCCH immediately */
    rc_file_seek(iterator, file_handle, content_offset, SEEK_SET);
    if (rc_file_read(iterator, file_handle, header, 0x200) != 0x200)
      return rc_hash_iterator_error(iterator, "Could not read NCCH header");

    if (memcmp(&header[0x100], "NCCH", 4) != 0)
      return rc_hash_iterator_error_formatted(iterator, "NCCH header was not at %08X%08X", (unsigned)(content_offset >> 32), (unsigned)content_offset);

    return rc_hash_nintendo_3ds_ncch(md5, file_handle, header, NULL, iterator);
  }

  if (iterator->callbacks.encryption.get_3ds_cia_normal_key == NULL)
    return rc_hash_iterator_error(iterator, "An encrypted CIA was detected, but the CIA normal key callback was not set");

  /* Acquire the encrypted title key, title id, and common key index from the ticket */
  /* These will be needed to decrypt the title key, and that will be needed to decrypt the CIA */

  rc_file_seek(iterator, file_handle, tik_offset, SEEK_SET);
  if (rc_file_read(iterator, file_handle, header, 4) != 4)
    return rc_hash_iterator_error(iterator, "Could not read ticket signature type");

  signature_size = rc_hash_nintendo_3ds_cia_signature_size(header, iterator);
  if (signature_size == 0)
    return 0;

  rc_file_seek(iterator, file_handle, signature_size, SEEK_CUR);
  if (rc_file_read(iterator, file_handle, header, 0xB2) != 0xB2)
    return rc_hash_iterator_error(iterator, "Could not read ticket data");

  memcpy(title_key, &header[0x7F], sizeof(title_key));
  memcpy(title_id, &header[0x9C], sizeof(title_id));
  common_key_index = header[0xB1];

  if (common_key_index > 5)
    return rc_hash_iterator_error_formatted(iterator, "Invalid common key index %02X", (unsigned)common_key_index);

  if (iterator->callbacks.encryption.get_3ds_cia_normal_key(common_key_index, normal_key) == 0)
    return rc_hash_iterator_error_formatted(iterator, "Could not obtain common key %02X", (unsigned)common_key_index);

  memset(iv, 0, sizeof(iv));
  memcpy(iv, title_id, sizeof(title_id));
  AES_init_ctx_iv(&aes, normal_key, iv);

  /* Finally, decrypt the title key */
  AES_CBC_decrypt_buffer(&aes, title_key, sizeof(title_key));

  /* Now we can hash the NCCH */

  rc_file_seek(iterator, file_handle, content_offset, SEEK_SET);
  if (rc_file_read(iterator, file_handle, header, 0x200) != 0x200)
    return rc_hash_iterator_error(iterator, "Could not read NCCH header");

  memset(iv, 0, sizeof(iv)); /* Content index is iv (which is always 0 for main content) */
  AES_init_ctx_iv(&aes, title_key, iv);
  AES_CBC_decrypt_buffer(&aes, header, 0x200);

  if (memcmp(&header[0x100], "NCCH", 4) != 0)
    return rc_hash_iterator_error_formatted(iterator, "NCCH header was not at %08X%08X", (unsigned)(content_offset >> 32), (unsigned)content_offset);

  return rc_hash_nintendo_3ds_ncch(md5, file_handle, header, &aes, iterator);
}

static int rc_hash_nintendo_3ds_3dsx(md5_state_t* md5, void* file_handle, uint8_t header[0x200], const rc_hash_iterator_t* iterator)
{
  uint8_t* hash_buffer;
  uint32_t header_size, reloc_header_size, code_size;
  int64_t code_offset;

  header_size = (header[5] << 8) | header[4];
  reloc_header_size = (header[7] << 8) | header[6];
  code_size = ((uint32_t)header[0x13] << 24) | (header[0x12] << 16) | (header[0x11] << 8) | header[0x10];

  /* 3 relocation headers are in-between the 3DSX header and code segment */
  code_offset = header_size + reloc_header_size * 3;

  if (code_size > MAX_BUFFER_SIZE)
    code_size = MAX_BUFFER_SIZE;

  hash_buffer = (uint8_t*)malloc(code_size);
  if (!hash_buffer)
    return rc_hash_iterator_error_formatted(iterator, "Failed to allocate %u bytes", (unsigned)code_size);

  rc_file_seek(iterator, file_handle, code_offset, SEEK_SET);

  rc_hash_iterator_verbose_formatted(iterator, "Hashing %u bytes for 3DSX (at %08X)", (unsigned)code_size, (unsigned)code_offset);

  if (rc_file_read(iterator, file_handle, hash_buffer, code_size) != code_size) {
    free(hash_buffer);
    return rc_hash_iterator_error(iterator, "Could not read 3DSX code segment");
  }

  md5_append(md5, hash_buffer, code_size);

  free(hash_buffer);
  return 1;
}

int rc_hash_nintendo_3ds(char hash[33], const rc_hash_iterator_t* iterator)
{
  md5_state_t md5;
  void* file_handle;
  uint8_t header[0x200]; /* NCCH and NCSD headers are both 0x200 bytes */
  int64_t header_offset;

  file_handle = rc_file_open(iterator, iterator->path);
  if (!file_handle)
    return rc_hash_iterator_error(iterator, "Could not open file");

  rc_file_seek(iterator, file_handle, 0, SEEK_SET);

  /* If we don't have a full header, this is probably not a 3DS ROM */
  if (rc_file_read(iterator, file_handle, header, sizeof(header)) != sizeof(header)) {
    rc_file_close(iterator, file_handle);
    return rc_hash_iterator_error(iterator, "Could not read 3DS ROM header");
  }

  md5_init(&md5);

  if (memcmp(&header[0x100], "NCSD", 4) == 0) {
    /* A NCSD container contains 1-8 NCCH partitions */
    /* The first partition (index 0) is reserved for executable content */
    header_offset = ((uint32_t)header[0x123] << 24) | (header[0x122] << 16) | (header[0x121] << 8) | header[0x120];
    /* Offset is in "media units" (1 media unit = 0x200 bytes) */
    header_offset *= 0x200;

    /* We include the NCSD header in the hash, as that will ensure different versions of a game result in a different hash
     * This is due to some revisions / languages only ever changing other NCCH paritions (e.g. the game manual)
     */
    rc_hash_iterator_verbose(iterator, "Hashing 512 byte NCSD header");
    md5_append(&md5, header, sizeof(header));

    rc_hash_iterator_verbose_formatted(iterator,
      "Detected NCSD header, seeking to NCCH partition at %08X%08X",
      (unsigned)(header_offset >> 32), (unsigned)header_offset);

    rc_file_seek(iterator, file_handle, header_offset, SEEK_SET);
    if (rc_file_read(iterator, file_handle, header, sizeof(header)) != sizeof(header)) {
      rc_file_close(iterator, file_handle);
      return rc_hash_iterator_error(iterator, "Could not read 3DS NCCH header");
    }

    if (memcmp(&header[0x100], "NCCH", 4) != 0) {
      rc_file_close(iterator, file_handle);
      return rc_hash_iterator_error_formatted(iterator, "3DS NCCH header was not at %08X%08X", (unsigned)(header_offset >> 32), (unsigned)header_offset);
    }
  }

  if (memcmp(&header[0x100], "NCCH", 4) == 0) {
    if (rc_hash_nintendo_3ds_ncch(&md5, file_handle, header, NULL, iterator)) {
      rc_file_close(iterator, file_handle);
      return rc_hash_finalize(iterator, &md5, hash);
    }

    rc_file_close(iterator, file_handle);
    return rc_hash_iterator_error(iterator, "Failed to hash 3DS NCCH container");
  }

  /* Couldn't identify either an NCSD or NCCH */

  /* Try to identify this as a CIA */
  if (header[0] == 0x20 && header[1] == 0x20 && header[2] == 0x00 && header[3] == 0x00) {
    rc_hash_iterator_verbose(iterator, "Detected CIA, attempting to find executable NCCH");

    if (rc_hash_nintendo_3ds_cia(&md5, file_handle, header, iterator)) {
      rc_file_close(iterator, file_handle);
      return rc_hash_finalize(iterator, &md5, hash);
    }

    rc_file_close(iterator, file_handle);
    return rc_hash_iterator_error(iterator, "Failed to hash 3DS CIA container");
  }

  /* This might be a homebrew game, try to detect that */
  if (memcmp(&header[0], "3DSX", 4) == 0) {
    rc_hash_iterator_verbose(iterator, "Detected 3DSX");

    if (rc_hash_nintendo_3ds_3dsx(&md5, file_handle, header, iterator)) {
      rc_file_close(iterator, file_handle);
      return rc_hash_finalize(iterator, &md5, hash);
    }

    rc_file_close(iterator, file_handle);
    return rc_hash_iterator_error(iterator, "Failed to hash 3DS 3DSX container");
  }

  /* Raw ELF marker (AXF/ELF files) */
  if (memcmp(&header[0], "\x7f\x45\x4c\x46", 4) == 0) {
    rc_hash_iterator_verbose(iterator, "Detected AXF/ELF file, hashing entire file");

    /* Don't bother doing anything fancy here, just hash entire file */
    rc_file_close(iterator, file_handle);
    return rc_hash_whole_file(hash, iterator);
  }

  rc_file_close(iterator, file_handle);
  return rc_hash_iterator_error(iterator, "Not a 3DS ROM");
}
