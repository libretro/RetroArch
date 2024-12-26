#ifndef RC_LIBRETRO_H
#define RC_LIBRETRO_H

#ifdef __cplusplus
extern "C" {
#endif

/* this file comes from the libretro repository, which is not an explicit submodule.
 * the integration must set up paths appropriately to find it. */
#include <libretro.h>

#include <stddef.h>
#include <stdint.h>

/*****************************************************************************\
| Disallowed Settings                                                         |
\*****************************************************************************/

typedef struct rc_disallowed_setting_t
{
  const char* setting;
  const char* value;
} rc_disallowed_setting_t;

const rc_disallowed_setting_t* rc_libretro_get_disallowed_settings(const char* library_name);
int rc_libretro_is_setting_allowed(const rc_disallowed_setting_t* disallowed_settings, const char* setting, const char* value);
int rc_libretro_is_system_allowed(const char* library_name, uint32_t console_id);

/*****************************************************************************\
| Memory Mapping                                                              |
\*****************************************************************************/

/* specifies a function to call for verbose logging */
typedef void (*rc_libretro_message_callback)(const char*);
void rc_libretro_init_verbose_message_callback(rc_libretro_message_callback callback);

#define RC_LIBRETRO_MAX_MEMORY_REGIONS 32
typedef struct rc_libretro_memory_regions_t
{
  uint8_t* data[RC_LIBRETRO_MAX_MEMORY_REGIONS];
  size_t size[RC_LIBRETRO_MAX_MEMORY_REGIONS];
  size_t total_size;
  uint32_t count;
} rc_libretro_memory_regions_t;

typedef struct rc_libretro_core_memory_info_t
{
  uint8_t* data;
  size_t size;
} rc_libretro_core_memory_info_t;

typedef void (*rc_libretro_get_core_memory_info_func)(uint32_t id, rc_libretro_core_memory_info_t* info);

int rc_libretro_memory_init(rc_libretro_memory_regions_t* regions, const struct retro_memory_map* mmap,
                            rc_libretro_get_core_memory_info_func get_core_memory_info, int console_id);
void rc_libretro_memory_destroy(rc_libretro_memory_regions_t* regions);

uint8_t* rc_libretro_memory_find(const rc_libretro_memory_regions_t* regions, uint32_t address);
uint8_t* rc_libretro_memory_find_avail(const rc_libretro_memory_regions_t* regions, uint32_t address, uint32_t* avail);
uint32_t rc_libretro_memory_read(const rc_libretro_memory_regions_t* regions, uint32_t address, uint8_t* buffer, uint32_t num_bytes);

/*****************************************************************************\
| Disk Identification                                                         |
\*****************************************************************************/

typedef struct rc_libretro_hash_entry_t
{
  uint32_t                         path_djb2;
  uint32_t                         game_id;
  char                             hash[33];
} rc_libretro_hash_entry_t;

typedef struct rc_libretro_hash_set_t
{
  struct rc_libretro_hash_entry_t* entries;
  uint16_t                         entries_count;
  uint16_t                         entries_size;
} rc_libretro_hash_set_t;

typedef int (*rc_libretro_get_image_path_func)(uint32_t index, char* buffer, size_t buffer_size);

void rc_libretro_hash_set_init(struct rc_libretro_hash_set_t* hash_set,
                               const char* m3u_path, rc_libretro_get_image_path_func get_image_path);
void rc_libretro_hash_set_destroy(struct rc_libretro_hash_set_t* hash_set);

void rc_libretro_hash_set_add(struct rc_libretro_hash_set_t* hash_set,
                              const char* path, uint32_t game_id, const char hash[33]);
const char* rc_libretro_hash_set_get_hash(const struct rc_libretro_hash_set_t* hash_set, const char* path);
int rc_libretro_hash_set_get_game_id(const struct rc_libretro_hash_set_t* hash_set, const char* hash);

#ifdef __cplusplus
}
#endif

#endif /* RC_LIBRETRO_H */
