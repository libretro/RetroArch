#include "pluginman.h"

#include "imguidock.h"
#include "imguial_log.h"
#include "imguial_fonts.h"

#include <vector>
#include <map>

#include <string/stdstring.h>
#include "../core.h"
#include "../retroarch.h"
#include "../core.h"
#include "../gfx/video_driver.h"
#include "../managers/core_option_manager.h"
#include "../cheevos/cheevos.h"
#include "../content.h"

// Log functions for the plugins
static ImGuiAl::Log      s_debugger_logger;

// Memory regions
static debugger_memory_t s_debugger_memory[64];
static unsigned          s_debugger_memory_count = 0;

// Registered plugins
static std::vector<const debugger_plugin_t*> s_debugger_plugins;

// Running services
struct Running
{
  void*                    instance;
  const debugger_plugin_t* plugin;
  debugger_service_t       service;
};

static std::vector<Running> s_debugger_running;

static void s_debugger_vprintf(enum retro_log_level level, const char* format, va_list args)
{
  ImGuiAl::Log::Level l;

  switch (level)
  {
  case RETRO_LOG_ERROR: l = ImGuiAl::Log::kError; break;
  case RETRO_LOG_WARN:  l = ImGuiAl::Log::kWarn; break;
  case RETRO_LOG_DEBUG: l = ImGuiAl::Log::kDebug; break;
  default:              l = ImGuiAl::Log::kInfo; break;
  }

  s_debugger_logger.VPrintf(l, format, args);
}

static void s_debugger_printf(enum retro_log_level level, const char* format, ...)
{
  va_list args;
  va_start(args, format);
  s_debugger_vprintf(level, format, args);
  va_end(args);
}

static void s_debugger_debug(const char* format, ...)
{
  va_list args;
  va_start(args,format);
  s_debugger_vprintf(RETRO_LOG_DEBUG, format, args);
  va_end(args);
}

static void s_debugger_info(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  s_debugger_vprintf(RETRO_LOG_INFO, format, args);
  va_end(args);
}

static void s_debugger_warn(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  s_debugger_vprintf(RETRO_LOG_WARN, format, args);
  va_end(args);
}

static void s_debugger_error(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  s_debugger_vprintf(RETRO_LOG_ERROR, format, args);
  va_end(args);
}

static const debugger_log_t s_debugger_log =
{
  s_debugger_vprintf,
  s_debugger_printf,
  s_debugger_debug,
  s_debugger_info,
  s_debugger_warn,
  s_debugger_error
};

// RetroArch info functions for the plugins

static int s_debugger_isCoreLoaded()
{
  return core_is_inited();
}

static int s_debugger_isGameLoaded()
{
  return core_is_game_loaded();
}

static const debugger_rarch_info_t s_debugger_rarchInfo =
{
  s_debugger_isCoreLoaded,
  s_debugger_isGameLoaded
};

// Core info functions for the plugins

static void s_debugger_getCoreName(char* name, size_t size)
{
  if (size > 0)
  {
    rarch_system_info_t *system = runloop_get_system_info();

    if (!string_is_empty(system->info.library_name))
    {
      strncpy(name, system->info.library_name, size);
    }
    else
    {
      name[0] = 0;
    }
  }
}

static unsigned s_debugger_getApiVersion()
{
  retro_ctx_api_info_t api;
  core_api_version(&api);
  return api.version;
}

static const struct retro_system_info* s_debugger_getSystemInfo()
{
  rarch_system_info_t* system = runloop_get_system_info();
  return &system->info;
}

static const struct retro_system_av_info* s_debugger_getSystemAVInfo()
{
  return video_viewport_get_system_av_info();
}

static unsigned s_debugger_getRegion()
{
  retro_ctx_region_info_t region;
  core_get_region(&region);
  return region.region;
}

static void* s_debugger_getMemoryData(unsigned id)
{
  retro_ctx_memory_info_t mem;
  mem.id = id;
  core_get_memory(&mem);
  return mem.data;
}

static size_t s_debugger_getMemorySize(unsigned id)
{
  retro_ctx_memory_info_t mem;
  mem.id = id;
  core_get_memory(&mem);
  return mem.size;
}

static unsigned s_debugger_getRotation()
{
  rarch_system_info_t* system = runloop_get_system_info();
  return system->rotation;
}

static unsigned s_debugger_getPerformanceLevel()
{
  rarch_system_info_t* system = runloop_get_system_info();
  return system->performance_level;
}

static enum retro_pixel_format s_debugger_getPixelFormat()
{
  return video_driver_get_pixel_format();
}

static int s_debugger_supportsNoGame()
{
  bool contentless, is_inited;

  content_get_status(&contentless, &is_inited);
  return contentless;
}

static const struct retro_memory_descriptor* s_debugger_getMemoryMap(unsigned index)
{
  rarch_system_info_t* system = runloop_get_system_info();

  if (index < system->mmaps.num_descriptors)
  {
    return &system->mmaps.descriptors[index].core;
  }

  return NULL;
}

static int s_debugger_supportsCheevos(void)
{
#ifdef HAVE_CHEEVOS
  return cheevos_get_support_cheevos();
#else
  return 0;
#endif
}

static unsigned s_debugger_getConsoleId(void)
{
  return cheevos_get_console_id();
}

static uint32_t s_debugger_getCoreId(void)
{
  rarch_system_info_t* system = runloop_get_system_info();
  const char* libname = system->info.library_name;
  uint32_t hash = 5381;

  while (*libname)
  {
    hash = hash * 33 + (uint8_t)*libname++;
  }

  return hash;
}

static debugger_memory_t* s_debugger_getMemoryRegions(unsigned* count)
{
  *count = s_debugger_memory_count;
  return s_debugger_memory;
}

static const debugger_core_info_t s_debugger_coreInfo =
{
  s_debugger_getCoreName,
  s_debugger_getApiVersion,
  s_debugger_getSystemInfo,
  s_debugger_getSystemAVInfo,
  s_debugger_getRegion,
  s_debugger_getMemoryData,
  s_debugger_getMemorySize,
  s_debugger_getRotation,
  s_debugger_getPerformanceLevel,
  s_debugger_getPixelFormat,
  s_debugger_supportsNoGame,
  s_debugger_getMemoryMap,
  s_debugger_supportsCheevos,
  s_debugger_getConsoleId,
  s_debugger_getCoreId,
  s_debugger_getMemoryRegions
};

static void s_debugger_startService(const debugger_service_t* service)
{
  size_t count = s_debugger_plugins.size();

  for (size_t i = 0; i < count; i++)
  {
    auto& plugin = s_debugger_plugins[i];
    const unsigned* type = plugin->services;

    if (type != NULL)
    {
      for (; *type != 0; type++)
      {
        if (*type == service->type)
        {
          Running dummy;
          s_debugger_running.push_back(dummy);

          Running& running = s_debugger_running[s_debugger_running.size() - 1];

          running.service = *service;
          running.plugin = plugin;
          running.instance = plugin->serve(&running.service);

          if (running.instance == NULL)
          {
            s_debugger_running.pop_back();
          }

          return;
        }
      }
    }    
  }
}

static debugger_services_t s_debugger_services_ =
{
  s_debugger_startService
};

// Plugin interface
const debugger_t s_debugger =
{
  &s_debugger_log,
  &s_debugger_rarchInfo,
  &s_debugger_coreInfo,
  &s_debugger_services_
};

static void s_debugger_fillRegionWithRange(debugger_memory_t* mem, size_t begin, size_t end)
{
  rarch_system_info_t* system = runloop_get_system_info();
  unsigned ndx = 0;
  size_t size = 0;

  for (unsigned i = 0; i < system->mmaps.num_descriptors; i++)
  {
    const struct retro_memory_descriptor* desc = &system->mmaps.descriptors[i].core;

    if (desc->start >= begin && desc->start < end)
    {
      // RAM
      if (ndx == sizeof(mem->parts) / sizeof(mem->parts[0]))
      {
        return;
      }

      mem->parts[ndx].pointer = (uint8_t*)desc->ptr + desc->offset;
      mem->parts[ndx].size = desc->len;
      mem->parts[ndx].offset = desc->start;
      ndx++;
      size += desc->len;
    }
  }

  struct Compare
  {
    static int cmp(const void* e1, const void* e2)
    {
      auto p1 = (const debugger_memory_part_t*)e1;
      auto p2 = (const debugger_memory_part_t*)e2;

      return p1->offset < p2->offset ? -1 : p1->offset > p2->offset ? 1 : 0;
    }
  };

  qsort(mem->parts, ndx, sizeof(debugger_memory_part_t), Compare::cmp);
  mem->size = size;
  mem->count = ndx;
  mem->parts[0].offset = 0;

  debugger_memory_part_t* part = &mem->parts[1];

  for (unsigned i = 1; i < ndx; i++, part++)
  {
    part->offset = part[-1].offset + part[-1].size;
  }
}

static void s_debugger_getMemoryRegionsFceumm()
{
  s_debugger_memory[0].name = "ram";
  s_debugger_fillRegionWithRange(&s_debugger_memory[0], 0x0000, 0x0800);

  s_debugger_memory[1].name = "wram";
  s_debugger_fillRegionWithRange(&s_debugger_memory[1], 0x6000, 0x8000);

  s_debugger_memory_count = 2;
}

static void s_debugger_getMemoryRegionsPicoDrive()
{
  s_debugger_memory[0].name = "ram";
  s_debugger_memory[0].size = s_debugger_getMemorySize(RETRO_MEMORY_SYSTEM_RAM);
  s_debugger_memory[0].count = 1;
  s_debugger_memory[0].parts[0].pointer = (uint8_t*)s_debugger_getMemoryData(RETRO_MEMORY_SYSTEM_RAM);
  s_debugger_memory[0].parts[0].size = s_debugger_memory[0].size;
  s_debugger_memory[0].parts[0].offset = 0;

  s_debugger_memory_count = 1;
}

static void s_debugger_initMemoryRegions()
{
  s_debugger_memory_count = 0;

  switch (s_debugger_getCoreId())
  {
  case DEBUGGER_CORE_FCEUMM:    s_debugger_getMemoryRegionsFceumm(); break;
  case DEBUGGER_CORE_PICODRIVE: s_debugger_getMemoryRegionsPicoDrive(); break;
  }
}

static void s_debugger_register_plugin(const debugger_plugin_t* plugin)
{
  s_debugger_plugins.push_back(plugin);
}

void debugger_pluginman_init()
{
  // Setup logger
  static const char* actions[] =
  {
    ICON_FA_FILES_O " Copy",
    ICON_FA_FILE_O " Clear",
    NULL
  };

  if (s_debugger_logger.Init(0, actions))
  {
    s_debugger_logger.SetLabel(ImGuiAl::Log::kDebug, ICON_FA_BUG " Debug");
    s_debugger_logger.SetLabel(ImGuiAl::Log::kInfo, ICON_FA_INFO " Info");
    s_debugger_logger.SetLabel(ImGuiAl::Log::kWarn, ICON_FA_EXCLAMATION_TRIANGLE " Warn");
    s_debugger_logger.SetLabel(ImGuiAl::Log::kError, ICON_FA_BOMB " Error");
    s_debugger_logger.SetCumulativeLabel(ICON_FA_SORT_AMOUNT_DESC " Cumulative");
    s_debugger_logger.SetFilterHeaderLabel(ICON_FA_FILTER " Filters");
    s_debugger_logger.SetFilterLabel(ICON_FA_SEARCH " Filter (inc,-exc)");
  }

  void init_info(debugger_register_plugin_t register_plugin, const debugger_t* info);
  void init_memeditor(debugger_register_plugin_t register_plugin, const debugger_t* info);
  void init_hunter(debugger_register_plugin_t register_plugin, const debugger_t* info);

  static const debugger_init_plugins_t plugins[] =
  {
    init_info,
    init_memeditor,
    init_hunter
  };

  for (int i = 0; i < sizeof(plugins) / sizeof(plugins[0]); i++)
  {
    plugins[i](s_debugger_register_plugin, &s_debugger);
  }
}

void debugger_pluginman_deinit()
{
  size_t count = s_debugger_running.size();

  for (size_t i = 0; i < count; i++)
  {
    Running& running = s_debugger_running[i];

    if (running.service.destroyUd != NULL)
    {
      running.service.destroyUd(running.service.userData);
    }

    running.plugin->destroy(running.instance, 1);
  }

  s_debugger_running.clear();
}

void debugger_pluginman_draw()
{
  if (s_debugger_memory_count != 0)
  {
    if (!s_debugger_isGameLoaded())
    {
      s_debugger_memory_count = 0;
    }
  }
  else
  {
    if (s_debugger_isGameLoaded())
    {
      s_debugger_initMemoryRegions();
    }
  }

  if (ImGui::BeginDock(ICON_FA_PLUG " Plugins"))
  {
    size_t count = s_debugger_plugins.size();

    for (size_t i = 0; i < count; i++)
    {
      const debugger_plugin_t* plugin = s_debugger_plugins[i];
      ImGui::PushID(plugin);

      char label[1024];
      snprintf(label, sizeof(label), ICON_FA_PLUS " Create##%p", plugin);

      if (ImGui::Button(label))
      {
        Running dummy;
        s_debugger_running.push_back(dummy);

        Running& running = s_debugger_running[s_debugger_running.size() - 1];

        memset(&running.service, 0, sizeof(running.service));
        running.plugin = plugin;
        running.instance = plugin->create(&running.service);

        if (running.instance == NULL)
        {
          s_debugger_running.pop_back();
        }
      }

      ImGui::SameLine();
      ImGui::Text("%s", plugin->name);
      ImGui::PopID();
    }
  }

  ImGui::EndDock();

  if (ImGui::BeginDock(ICON_FA_COMMENT " Log"))
  {
    s_debugger_logger.Draw();
  }

  ImGui::EndDock();

  size_t count = s_debugger_running.size();

  for (size_t i = 0; i < count;)
  {
    Running& running = s_debugger_running[i];
    ImGui::PushID(running.instance);

    char label[1024];
    snprintf(label, sizeof(label), "%s##%p", running.plugin->name, running.instance);

    bool keep = true;

    if (ImGui::BeginDock(label, &keep))
    {
      running.plugin->draw(running.instance);
    }

    ImGui::EndDock();

    if (keep)
    {
      i++;
    }
    else
    {
      s_debugger_running.erase(s_debugger_running.begin() + i);
    }

    ImGui::PopID();
  }
}
