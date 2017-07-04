#include "plugin.h"

#include <stdio.h>

#include "imgui.h"
#include "imguial_fonts.h"
#include "imgui_memory_editor.h"

static const debugger_t* s_info;

namespace
{
  class MemoryWatch
  {
  public:
    static void* s_create(void)
    {
      auto self = new MemoryWatch();
      self->create();
      return self;
    }

    static void s_destroy(void* instance)
    {
      auto self = (MemoryWatch*)instance;
      self->destroy();
      delete self;
    }

    static void s_draw(void* instance)
    {
      auto self = (MemoryWatch*)instance;
      self->draw();
    }
  
  protected:
    int _selected;

    void create()
    {
      _selected = 0;
    }

    void destroy() {}

    void draw()
    {
      struct Region
      {
        const char name[64] = {0};
        void*      data;
        size_t     size;
      };

      Region regions[64];
      unsigned count = 0;

      void*  data = s_info->coreInfo->getMemoryData(RETRO_MEMORY_SAVE_RAM);
      size_t size = s_info->coreInfo->getMemorySize(RETRO_MEMORY_SAVE_RAM);

      if (data && size)
      {
        strcpy(regions[count].name, "Save RAM");
        regions[count].data = data;
        regions[count++].size = size;
      }

      data = s_info->coreInfo->getMemoryData(RETRO_MEMORY_RTC);
      size = s_info->coreInfo->getMemorySize(RETRO_MEMORY_RTC);

      if (data && size)
      {
        strcpy(regions[count].name, "Real Time Clock");
        regions[count].data = data;
        regions[count++].size = size;
      }

      data = s_info->coreInfo->getMemoryData(RETRO_MEMORY_SYSTEM_RAM);
      size = s_info->coreInfo->getMemorySize(RETRO_MEMORY_SYSTEM_RAM);

      if (data && size)
      {
        strcpy(regions[count].name, "System RAM");
        regions[count].data = data;
        regions[count++].size = size;
      }

      data = s_info->coreInfo->getMemoryData(RETRO_MEMORY_VIDEO_RAM);
      size = s_info->coreInfo->getMemorySize(RETRO_MEMORY_VIDEO_RAM);

      if (data && size)
      {
        strcpy(regions[count].name, "Video RAM");
        regions[count].data = data;
        regions[count++].size = size;
      }

      for (unsigned i = 0;; i++)
      {
        const struct retro_memory_descriptor* mem = s_info->coreInfo->getMemoryMap(i);

        if (!mem)
        {
          break;
        }

        if (mem->ptr && mem->len)
        {
          snprintf(regions[count].name, sizeof(regions[count].name), "Memory @%p", mem->start);
          regions[count].data = (char*)mem->ptr + mem->offset;
          regions[count++].size = mem->len;
        }
      }

      struct Getter
      {
        static bool description(void* data, int idx, const char** out_text)
        {
          if (idx == 0)
          {
            *out_text = "None";
          }
          else
          {
            auto regions = (Region*)data;
            *out_text = regions[idx - 1].name;
          }

          return true;
        }
      };

      ImGui::Combo("Region", &_selected, Getter::description, (void*)&regions, count + 1);

      if (_selected != 0)
      {
        static MemoryEditor editor;

        Region* region = regions + _selected - 1;
        editor.Draw(region->name, (unsigned char*)region->data, region->size, 0);
      }
    }
  };
}

static const debugger_plugin_t plugin =
{
  sizeof(debugger_plugin_t),
  ICON_FA_MICROCHIP " Memory Watch",
  MemoryWatch::s_create,
  MemoryWatch::s_destroy,
  MemoryWatch::s_draw
};

void init_plugin(debugger_register_plugin_t register_plugin, const debugger_t* info)
{
  s_info = info;

  register_plugin(&plugin);
}
