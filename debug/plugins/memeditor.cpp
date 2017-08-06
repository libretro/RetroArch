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
    debugger_memory_t* _memory;
    size_t _count;

    int _selected;
    bool _inited;
    MemoryEditor _editor;

    void initRegions()
    {
      _memory = s_info->coreInfo->getMemoryRegions(&_count);
    }

    void create()
    {
      _selected = 0;
      _inited = false;

      struct Locality
      {
        static unsigned char read(unsigned char* data, size_t off)
        {
          auto memory = (debugger_memory_t*)data;
          auto part = memory->parts;
          auto end = memory->parts + memory->count;

          while (part < end && off >= part->size)
          {
            off -= part->size;
            part++;
          }

          return part < end ? part->pointer[off] : 0;
        }

        static void write(unsigned char* data, size_t off, unsigned char d)
        {
          auto memory = (debugger_memory_t*)data;
          auto part = memory->parts;
          auto end = memory->parts + memory->count;

          while (part < end && off >= part->size)
          {
            off -= part->size;
            part++;
          }

          if (part < end)
          {
            part->pointer[off] = d;
          }
        }
      };

      _editor.ReadFn = Locality::read;
      _editor.WriteFn = Locality::write;
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
          _count = 0;
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

      struct Locality
      {
        static bool description(void* data, int idx, const char** out_text)
        {
          if (idx == 0)
          {
            *out_text = "None";
          }
          else
          {
            auto memory = (const debugger_memory_t*)data;
            *out_text = memory[idx - 1].name;
          }

          return true;
        }
      };

      ImGui::Combo("Region", &_selected, Locality::description, (void*)_memory, _count + 1);

      if (_selected != 0)
      {
        debugger_memory_t* memory = &_memory[_selected - 1];
        _editor.Draw(memory->name, (unsigned char*)memory, memory->size);
      }
    }
  };
}

static const debugger_plugin_t plugin =
{
  ICON_FA_MICROCHIP " Memory Editor",
  MemEditor::s_create,
  MemEditor::s_destroy,
  MemEditor::s_draw
};

void init_memeditor(debugger_register_plugin_t register_plugin, const debugger_t* info)
{
  s_info = info;
  register_plugin(&plugin);
}
