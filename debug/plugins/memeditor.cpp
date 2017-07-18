#include "plugin.h"

#include <stdio.h>
#include <string>
#include <vector>

#include "imgui.h"
#include "imguial_fonts.h"
#include "imgui_memory_editor.h"

static const debugger_t* s_info;

namespace
{
  class MemEditor
  {
  public:
    static void* s_create(void)
    {
      auto self = new MemEditor();
      self->create();
      return self;
    }

    static int s_destroy(void* instance, int force)
    {
      auto self = (MemEditor*)instance;

      if (self->destroy(force != 0) || force)
      {
        delete self;
        return 1;
      }

      return 0;
    }

    static void s_draw(void* instance)
    {
      auto self = (MemEditor*)instance;
      self->draw();
    }
  
  protected:
    struct Region
    {
      std::string name;
      void*       data;
      size_t      size;
    };

    std::vector<Region> _regions;
    int _selected;
    bool _inited;
    MemoryEditor _editor;

    void initRegions()
    {
      Region region;

      void*  data = s_info->coreInfo->getMemoryData(RETRO_MEMORY_SAVE_RAM);
      size_t size = s_info->coreInfo->getMemorySize(RETRO_MEMORY_SAVE_RAM);

      if (data && size)
      {
        region.name = "Save RAM";
        region.data = data;
        region.size = size;
        _regions.push_back(region);
      }

      data = s_info->coreInfo->getMemoryData(RETRO_MEMORY_RTC);
      size = s_info->coreInfo->getMemorySize(RETRO_MEMORY_RTC);

      if (data && size)
      {
        region.name = "Real Time Clock";
        region.data = data;
        region.size = size;
        _regions.push_back(region);
      }

      data = s_info->coreInfo->getMemoryData(RETRO_MEMORY_SYSTEM_RAM);
      size = s_info->coreInfo->getMemorySize(RETRO_MEMORY_SYSTEM_RAM);

      if (data && size)
      {
        region.name = "System RAM";
        region.data = data;
        region.size = size;
        _regions.push_back(region);
      }

      data = s_info->coreInfo->getMemoryData(RETRO_MEMORY_VIDEO_RAM);
      size = s_info->coreInfo->getMemorySize(RETRO_MEMORY_VIDEO_RAM);

      if (data && size)
      {
        region.name = "Video RAM";
        region.data = data;
        region.size = size;
        _regions.push_back(region);
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
          char name[128];
          snprintf(name, sizeof(name), "Memory @%p", mem->start);

          region.name = name;
          region.data = (char*)mem->ptr + mem->offset;
          region.size = mem->len;
          _regions.push_back(region);
        }
      }
    }

    void create()
    {
      _selected = 0;
      _inited = false;
    }

    bool destroy(bool force)
    {
      (void)force;
      return true;
    }

    void draw()
    {
      if (_inited)
      {
        if (!s_info->rarchInfo->isGameLoaded())
        {
          _regions.clear();
          _selected = 0;
          _inited = false;
        }
      }
      else
      {
        if (s_info->rarchInfo->isGameLoaded())
        {
          initRegions();
          _inited = true;
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
            auto regions = (std::vector<Region>*)data;
            *out_text = (*regions)[idx - 1].name.c_str();
          }

          return true;
        }
      };

      ImGui::Combo("Region", &_selected, Getter::description, (void*)&_regions, _regions.size() + 1);

      if (_selected != 0)
      {
        Region* region = &_regions[_selected - 1];
        _editor.Draw(region->name.c_str(), (unsigned char*)region->data, region->size, 0);
      }
    }
  };
}

static const debugger_plugin_t plugin =
{
  ICON_FA_MICROCHIP " Memory Watch",
  MemEditor::s_create,
  MemEditor::s_destroy,
  MemEditor::s_draw
};

void init_memeditor(debugger_register_plugin_t register_plugin, const debugger_t* info)
{
  s_info = info;
  register_plugin(&plugin);
}
