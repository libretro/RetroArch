/* This file provides a series of functions for integrating RetroAchievements with libretro.
 * These functions will be called by a libretro frontend to validate certain expected behaviors
 * and simplify mapping core data to the RAIntegration DLL.
 * 
 * Originally designed to be shared between RALibretro and RetroArch, but will simplify
 * integrating with any other frontends.
 */

#include "rc_libretro.h"

#include "rc_consoles.h"
#include "rc_compat.h"

#include <ctype.h>
#include <string.h>

static rc_libretro_message_callback rc_libretro_verbose_message_callback = NULL;

/* a value that starts with a comma is a CSV.
 * if it starts with an exclamation point, it's everything but the provided value.
 * if it starts with an exclamntion point followed by a comma, it's everything but the CSV values.
 * values are case-insensitive */
typedef struct rc_disallowed_core_settings_t
{
  const char* library_name;
  const rc_disallowed_setting_t* disallowed_settings;
} rc_disallowed_core_settings_t;

static const rc_disallowed_setting_t _rc_disallowed_bsnes_settings[] = {
  { "bsnes_region", "pal" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_dolphin_settings[] = {
  { "dolphin_cheats_enabled", "enabled" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_ecwolf_settings[] = {
  { "ecwolf-invulnerability", "enabled" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_fbneo_settings[] = {
  { "fbneo-allow-patched-romsets", "enabled" },
  { "fbneo-cheat-*", "!,Disabled,0 - Disabled" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_fceumm_settings[] = {
  { "fceumm_region", ",PAL,Dendy" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_gpgx_settings[] = {
  { "genesis_plus_gx_lock_on", ",action replay (pro),game genie" },
  { "genesis_plus_gx_region_detect", "pal" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_gpgx_wide_settings[] = {
  { "genesis_plus_gx_wide_lock_on", ",action replay (pro),game genie" },
  { "genesis_plus_gx_wide_region_detect", "pal" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_mesen_settings[] = {
  { "mesen_region", ",PAL,Dendy" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_mesen_s_settings[] = {
  { "mesen-s_region", "PAL" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_pcsx_rearmed_settings[] = {
  { "pcsx_rearmed_region", "pal" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_picodrive_settings[] = {
  { "picodrive_region", ",Europe,Japan PAL" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_ppsspp_settings[] = {
  { "ppsspp_cheats", "enabled" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_smsplus_settings[] = {
  { "smsplus_region", "pal" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_snes9x_settings[] = {
  { "snes9x_region", "pal" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_virtual_jaguar_settings[] = {
  { "virtualjaguar_pal", "enabled" },
  { NULL, NULL }
};

static const rc_disallowed_core_settings_t rc_disallowed_core_settings[] = {
  { "bsnes-mercury", _rc_disallowed_bsnes_settings },
  { "dolphin-emu", _rc_disallowed_dolphin_settings },
  { "ecwolf", _rc_disallowed_ecwolf_settings },
  { "FCEUmm", _rc_disallowed_fceumm_settings },
  { "FinalBurn Neo", _rc_disallowed_fbneo_settings },
  { "Genesis Plus GX", _rc_disallowed_gpgx_settings },
  { "Genesis Plus GX Wide", _rc_disallowed_gpgx_wide_settings },
  { "Mesen", _rc_disallowed_mesen_settings },
  { "Mesen-S", _rc_disallowed_mesen_s_settings },
  { "PPSSPP", _rc_disallowed_ppsspp_settings },
  { "PCSX-ReARMed", _rc_disallowed_pcsx_rearmed_settings },
  { "PicoDrive", _rc_disallowed_picodrive_settings },
  { "SMS Plus GX", _rc_disallowed_smsplus_settings },
  { "Snes9x", _rc_disallowed_snes9x_settings },
  { "Virtual Jaguar", _rc_disallowed_virtual_jaguar_settings },
  { NULL, NULL }
};

static int rc_libretro_string_equal_nocase(const char* test, const char* value) {
  while (*test) {
    if (tolower(*test++) != tolower(*value++))
      return 0;
  }

  return (*value == '\0');
}

static int rc_libretro_match_value(const char* val, const char* match) {
  /* if value starts with a comma, it's a CSV list of potential matches */
  if (*match == ',') {
    do {
      const char* ptr = ++match;
      size_t size;

      while (*match && *match != ',')
        ++match;

      size = match - ptr;
      if (val[size] == '\0') {
        if (memcmp(ptr, val, size) == 0) {
          return 1;
        }
        else {
          char buffer[128];
          memcpy(buffer, ptr, size);
          buffer[size] = '\0';
          if (rc_libretro_string_equal_nocase(buffer, val))
            return 1;
        }
      }
    } while (*match == ',');

    return 0;
  }

  /* a leading exclamation point means the provided value(s) are not forbidden (are allowed) */
  if (*match == '!')
    return !rc_libretro_match_value(val, &match[1]);

  /* just a single value, attempt to match it */
  return rc_libretro_string_equal_nocase(val, match);
}

int rc_libretro_is_setting_allowed(const rc_disallowed_setting_t* disallowed_settings, const char* setting, const char* value) {
  const char* key;
  size_t key_len;

  for (; disallowed_settings->setting; ++disallowed_settings) {
    key = disallowed_settings->setting;
    key_len = strlen(key);

    if (key[key_len - 1] == '*') {
      if (memcmp(setting, key, key_len - 1) == 0) {
        if (rc_libretro_match_value(value, disallowed_settings->value))
          return 0;
      }
    }
    else {
      if (memcmp(setting, key, key_len + 1) == 0) {
        if (rc_libretro_match_value(value, disallowed_settings->value))
          return 0;
      }
    }
  }

  return 1;
}

const rc_disallowed_setting_t* rc_libretro_get_disallowed_settings(const char* library_name) {
  const rc_disallowed_core_settings_t* core_filter = rc_disallowed_core_settings;
  size_t library_name_length;

  if (!library_name || !library_name[0])
    return NULL;

  library_name_length = strlen(library_name) + 1;
  while (core_filter->library_name) {
    if (memcmp(core_filter->library_name, library_name, library_name_length) == 0)
      return core_filter->disallowed_settings;

    ++core_filter;
  }

  return NULL;
}


unsigned char* rc_libretro_memory_find(const rc_libretro_memory_regions_t* regions, unsigned address) {
  unsigned i;

  for (i = 0; i < regions->count; ++i) {
    const size_t size = regions->size[i];
    if (address < size) {
      if (regions->data[i] == NULL)
        break;

      return &regions->data[i][address];
    }

    address -= (unsigned)size;
  }

  return NULL;
}

void rc_libretro_init_verbose_message_callback(rc_libretro_message_callback callback) {
  rc_libretro_verbose_message_callback = callback;
}

static void rc_libretro_verbose(const char* message) {
  if (rc_libretro_verbose_message_callback)
    rc_libretro_verbose_message_callback(message);
}

static const char* rc_memory_type_str(int type) {
  switch (type)
  {
    case RC_MEMORY_TYPE_SAVE_RAM:
      return "SRAM";
    case RC_MEMORY_TYPE_VIDEO_RAM:
      return "VRAM";
    case RC_MEMORY_TYPE_UNUSED:
      return "UNUSED";
    default:
      break;
  }

  return "SYSTEM RAM";
}

static void rc_libretro_memory_register_region(rc_libretro_memory_regions_t* regions, int type,
                                               unsigned char* data, size_t size, const char* description) {
  if (size == 0)
    return;

  if (regions->count == (sizeof(regions->size) / sizeof(regions->size[0]))) {
    rc_libretro_verbose("Too many memory memory regions to register");
    return;
  }

  if (!data && regions->count > 0 && !regions->data[regions->count - 1]) {
    /* extend null region */
    regions->size[regions->count - 1] += size;
  }
  else if (data && regions->count > 0 &&
           data == (regions->data[regions->count - 1] + regions->size[regions->count - 1])) {
    /* extend non-null region */
    regions->size[regions->count - 1] += size;
  }
  else {
    /* create new region */
    regions->data[regions->count] = data;
    regions->size[regions->count] = size;
    ++regions->count;
  }

  regions->total_size += size;

  if (rc_libretro_verbose_message_callback) {
    char message[128];
    snprintf(message, sizeof(message), "Registered 0x%04X bytes of %s at $%06X (%s)", (unsigned)size,
             rc_memory_type_str(type), (unsigned)(regions->total_size - size), description);
    rc_libretro_verbose_message_callback(message);
  }
}

static void rc_libretro_memory_init_without_regions(rc_libretro_memory_regions_t* regions,
                                                    rc_libretro_get_core_memory_info_func get_core_memory_info) {
  /* no regions specified, assume system RAM followed by save RAM */
  char description[64];
  rc_libretro_core_memory_info_t info;

  snprintf(description, sizeof(description), "offset 0x%06x", 0);

  get_core_memory_info(RETRO_MEMORY_SYSTEM_RAM, &info);
  if (info.size)
    rc_libretro_memory_register_region(regions, RC_MEMORY_TYPE_SYSTEM_RAM, info.data, info.size, description);

  get_core_memory_info(RETRO_MEMORY_SAVE_RAM, &info);
  if (info.size)
    rc_libretro_memory_register_region(regions, RC_MEMORY_TYPE_SAVE_RAM, info.data, info.size, description);
}

static const struct retro_memory_descriptor* rc_libretro_memory_get_descriptor(const struct retro_memory_map* mmap, unsigned real_address, size_t* offset)
{
  const struct retro_memory_descriptor* desc = mmap->descriptors;
  const struct retro_memory_descriptor* end = desc + mmap->num_descriptors;

  for (; desc < end; desc++) {
    if (desc->select == 0) {
      /* if select is 0, attempt to explcitly match the address */
      if (real_address >= desc->start && real_address < desc->start + desc->len) {
        *offset = real_address - desc->start;
        return desc;
      }
    }
    else {
      /* otherwise, attempt to match the address by matching the select bits */
      /* address is in the block if (addr & select) == (start & select) */
      if (((desc->start ^ real_address) & desc->select) == 0) {
        /* calculate the offset within the descriptor, removing any disconnected bits */
        *offset = (real_address & ~desc->disconnect) - desc->start;

        /* sanity check - make sure the descriptor is large enough to hold the target address */
        if (*offset < desc->len)
          return desc;
      }
    }
  }

  *offset = 0;
  return NULL;
}

static void rc_libretro_memory_init_from_memory_map(rc_libretro_memory_regions_t* regions, const struct retro_memory_map* mmap,
                                                    const rc_memory_regions_t* console_regions) {
  char description[64];
  unsigned i;
  unsigned char* region_start;
  unsigned char* desc_start;
  size_t desc_size;
  size_t offset;

  for (i = 0; i < console_regions->num_regions; ++i) {
    const rc_memory_region_t* console_region = &console_regions->region[i];
    size_t console_region_size = console_region->end_address - console_region->start_address + 1;
    unsigned real_address = console_region->real_address;

    while (console_region_size > 0) {
      const struct retro_memory_descriptor* desc = rc_libretro_memory_get_descriptor(mmap, real_address, &offset);
      if (!desc) {
        if (rc_libretro_verbose_message_callback && console_region->type != RC_MEMORY_TYPE_UNUSED) {
          snprintf(description, sizeof(description), "Could not map region starting at $%06X",
                   real_address - console_region->real_address + console_region->start_address);
          rc_libretro_verbose(description);
        }

        rc_libretro_memory_register_region(regions, console_region->type, NULL, console_region_size, "null filler");
        break;
      }

      snprintf(description, sizeof(description), "descriptor %u, offset 0x%06X%s",
               (unsigned)(desc - mmap->descriptors) + 1, (int)offset, desc->ptr ? "" : " [no pointer]");

      if (desc->ptr) {
        desc_start = (uint8_t*)desc->ptr + desc->offset;
        region_start = desc_start + offset;
      }
      else {
        region_start = NULL;
      }

      desc_size = desc->len - offset;

      if (console_region_size > desc_size) {
        if (desc_size == 0) {
          if (rc_libretro_verbose_message_callback && console_region->type != RC_MEMORY_TYPE_UNUSED) {
            snprintf(description, sizeof(description), "Could not map region starting at $%06X",
                     real_address - console_region->real_address + console_region->start_address);
            rc_libretro_verbose(description);
          }

          rc_libretro_memory_register_region(regions, console_region->type, NULL, console_region_size, "null filler");
          console_region_size = 0;
        }
        else {
          rc_libretro_memory_register_region(regions, console_region->type, region_start, desc_size, description);
          console_region_size -= desc_size;
          real_address += (unsigned)desc_size;
        }
      }
      else {
        rc_libretro_memory_register_region(regions, console_region->type, region_start, console_region_size, description);
        console_region_size = 0;
      }
    }
  }
}

static unsigned rc_libretro_memory_console_region_to_ram_type(int region_type) {
  switch (region_type)
  {
    case RC_MEMORY_TYPE_SAVE_RAM:
      return RETRO_MEMORY_SAVE_RAM;
    case RC_MEMORY_TYPE_VIDEO_RAM:
      return RETRO_MEMORY_VIDEO_RAM;
    default:
      break;
  }

  return RETRO_MEMORY_SYSTEM_RAM;
}

static void rc_libretro_memory_init_from_unmapped_memory(rc_libretro_memory_regions_t* regions,
    rc_libretro_get_core_memory_info_func get_core_memory_info, const rc_memory_regions_t* console_regions) {
  char description[64];
  unsigned i, j;
  rc_libretro_core_memory_info_t info;
  size_t offset;

  for (i = 0; i < console_regions->num_regions; ++i) {
    const rc_memory_region_t* console_region = &console_regions->region[i];
    const size_t console_region_size = console_region->end_address - console_region->start_address + 1;
    const unsigned type = rc_libretro_memory_console_region_to_ram_type(console_region->type);
    unsigned base_address = 0;

    for (j = 0; j <= i; ++j) {
      const rc_memory_region_t* console_region2 = &console_regions->region[j];
      if (rc_libretro_memory_console_region_to_ram_type(console_region2->type) == type) {
        base_address = console_region2->start_address;
        break;
      }
    }
    offset = console_region->start_address - base_address;

    get_core_memory_info(type, &info);

    if (offset < info.size) {
      info.size -= offset;

      if (info.data) {
        snprintf(description, sizeof(description), "offset 0x%06X", (int)offset);
        info.data += offset;
      }
      else {
        snprintf(description, sizeof(description), "null filler");
      }
    }
    else {
      if (rc_libretro_verbose_message_callback && console_region->type != RC_MEMORY_TYPE_UNUSED) {
        snprintf(description, sizeof(description), "Could not map region starting at $%06X", console_region->start_address);
        rc_libretro_verbose(description);
      }

      info.data = NULL;
      info.size = 0;
    }

    if (console_region_size > info.size) {
      /* want more than what is available, take what we can and null fill the rest */
      rc_libretro_memory_register_region(regions, console_region->type, info.data, info.size, description);
      rc_libretro_memory_register_region(regions, console_region->type, NULL, console_region_size - info.size, "null filler");
    }
    else {
      /* only take as much as we need */
      rc_libretro_memory_register_region(regions, console_region->type, info.data, console_region_size, description);
    }
  }
}

int rc_libretro_memory_init(rc_libretro_memory_regions_t* regions, const struct retro_memory_map* mmap,
                            rc_libretro_get_core_memory_info_func get_core_memory_info, int console_id) {
  const rc_memory_regions_t* console_regions = rc_console_memory_regions(console_id);
  rc_libretro_memory_regions_t new_regions;
  int has_valid_region = 0;
  unsigned i;

  if (!regions)
    return 0;

  memset(&new_regions, 0, sizeof(new_regions));

  if (console_regions == NULL || console_regions->num_regions == 0)
    rc_libretro_memory_init_without_regions(&new_regions, get_core_memory_info);
  else if (mmap && mmap->num_descriptors != 0)
    rc_libretro_memory_init_from_memory_map(&new_regions, mmap, console_regions);
  else
    rc_libretro_memory_init_from_unmapped_memory(&new_regions, get_core_memory_info, console_regions);

  /* determine if any valid regions were found */
  for (i = 0; i < new_regions.count; i++) {
    if (new_regions.data[i]) {
      has_valid_region = 1;
      break;
    }
  }

  memcpy(regions, &new_regions, sizeof(*regions));
  return has_valid_region;
}

void rc_libretro_memory_destroy(rc_libretro_memory_regions_t* regions) {
  memset(regions, 0, sizeof(*regions));
}
