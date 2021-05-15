#ifndef RC_LIBRETRO_H
#define RC_LIBRETRO_H

/* this file comes from the libretro repository, which is not an explicit submodule.
 * the integration must set up paths appropriately to find it. */
#include <libretro.h>

#ifdef __cplusplus
extern "C" {
#endif

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

/*****************************************************************************\
| Memory Mapping                                                              |
\*****************************************************************************/

/* specifies a function to call for verbose logging */
typedef void (*rc_libretro_message_callback)(const char*);
void rc_libretro_init_verbose_message_callback(rc_libretro_message_callback callback);

#define RC_LIBRETRO_MAX_MEMORY_REGIONS 32
typedef struct rc_libretro_memory_regions_t
{
  unsigned char* data[RC_LIBRETRO_MAX_MEMORY_REGIONS];
  size_t size[RC_LIBRETRO_MAX_MEMORY_REGIONS];
  size_t total_size;
  unsigned count;
} rc_libretro_memory_regions_t;

typedef struct rc_libretro_core_memory_info_t
{
  unsigned char* data;
  size_t size;
} rc_libretro_core_memory_info_t;

typedef void (*rc_libretro_get_core_memory_info_func)(unsigned id, rc_libretro_core_memory_info_t* info);

int rc_libretro_memory_init(rc_libretro_memory_regions_t* regions, const struct retro_memory_map* mmap,
                            rc_libretro_get_core_memory_info_func get_core_memory_info, int console_id);
void rc_libretro_memory_destroy(rc_libretro_memory_regions_t* regions);

unsigned char* rc_libretro_memory_find(const rc_libretro_memory_regions_t* regions, unsigned address);

#ifdef __cplusplus
}
#endif

#endif /* RC_LIBRETRO_H */
