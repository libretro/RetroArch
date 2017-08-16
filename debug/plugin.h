#ifndef DEBUGGER_PLUGIN_H
#define DEBUGGER_PLUGIN_H

#include <libretro.h>
#include <stdarg.h>
#include <stdint.h>

/* Console IDs */
enum
{
   /* Don't change those, the values match the console IDs
    * at retroachievements.org. */
   DEBUGGER_CONSOLE_NONE             = 0,
   DEBUGGER_CONSOLE_MEGA_DRIVE       = 1,
   DEBUGGER_CONSOLE_NINTENDO_64      = 2,
   DEBUGGER_CONSOLE_SUPER_NINTENDO   = 3,
   DEBUGGER_CONSOLE_GAMEBOY          = 4,
   DEBUGGER_CONSOLE_GAMEBOY_ADVANCE  = 5,
   DEBUGGER_CONSOLE_GAMEBOY_COLOR    = 6,
   DEBUGGER_CONSOLE_NINTENDO         = 7,
   DEBUGGER_CONSOLE_PC_ENGINE        = 8,
   DEBUGGER_CONSOLE_SEGA_CD          = 9,
   DEBUGGER_CONSOLE_SEGA_32X         = 10,
   DEBUGGER_CONSOLE_MASTER_SYSTEM    = 11
};

/* Core IDs (djb2 hashes of their library_name in retro_system_info) */
enum
{
  DEBUGGER_CORE_FCEUMM    = 0xb00bd8c2U,
  DEBUGGER_CORE_PICODRIVE = 0x0cc11b6aU
};

enum
{
  DEBUGGER_SERVICE_EDIT_MEMORY = 1,
};

/* Memory regions */
typedef struct
{
  uint8_t* pointer;
  size_t   size;
  size_t   offset;
}
debugger_memory_part_t;

typedef struct
{
  const char* name;
  size_t      size;
  unsigned    count;

  debugger_memory_part_t parts[64];
}
debugger_memory_t;

/* Logging */
typedef struct
{
  void (*vprintf)(enum retro_log_level level, const char* format, va_list args);
  void (*printf)(enum retro_log_level level, const char* format, ...);
  void (*debug)(const char* format, ...);
  void (*info)(const char* format, ...);
  void (*warn)(const char* format, ...);
  void (*error)(const char* format, ...);
}
debugger_log_t;

/* Information from RetroArch */
typedef struct
{
  int (*isCoreLoaded)(void);
  int (*isGameLoaded)(void);
}
debugger_rarch_info_t;

/* Information from the core */
typedef struct
{
  void                                  (*getCoreName)(char* name, size_t size);
  unsigned                              (*getApiVersion)(void);
  const struct retro_system_info*       (*getSystemInfo)(void);
  const struct retro_system_av_info*    (*getSystemAVInfo)(void);
  unsigned                              (*getRegion)(void);
  void*                                 (*getMemoryData)(unsigned id);
  size_t                                (*getMemorySize)(unsigned id);
  unsigned                              (*getRotation)(void);
  unsigned                              (*getPerformanceLevel)(void);
  enum retro_pixel_format               (*getPixelFormat)(void);
  int                                   (*supportsNoGame)(void);
  const struct retro_memory_descriptor* (*getMemoryMap)(unsigned index);
  int                                   (*supportsCheevos)(void);
  unsigned                              (*getConsoleId)(void);
  uint32_t                              (*getCoreId)(void);
  debugger_memory_t*                    (*getMemoryRegions)(unsigned* count);
}
debugger_core_info_t;

typedef struct debugger_service_t debugger_service_t;

typedef struct
{
  void (*startService)(const debugger_service_t* service);
}
debugger_services_t;

typedef struct
{
  const debugger_log_t*        log;
  const debugger_rarch_info_t* rarchInfo;
  const debugger_core_info_t*  coreInfo;
  const debugger_services_t*   services;
}
debugger_t;

struct debugger_service_t
{
  unsigned type;
  void*    userData;
  void     (*destroyUd)(void* udata);

  union
  {
    struct
    {
      bool   readOnly;
      size_t size;

      uint8_t (*read)(size_t address, void* udata);
      void    (*write)(size_t address, uint8_t byte, void* udata);
      int     (*highlight)(size_t address, void* udata);
    }
    editMemory;
  };
};

typedef struct
{
  const char*     name;
  const unsigned* services;

  void* (*create)(debugger_service_t* service);
  void* (*serve)(debugger_service_t* service);
  int   (*destroy)(void* instance, int force);
  void  (*draw)(void* instance);
}
debugger_plugin_t;

typedef void (*debugger_register_plugin_t)(const debugger_plugin_t* plugin);
typedef void (*debugger_init_plugins_t)(debugger_register_plugin_t register_plugin, const debugger_t* info);

#endif /* DEBUGGER_PLUGIN_H */
