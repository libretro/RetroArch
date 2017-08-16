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
    static void* s_create(debugger_service_t* service)
    {
      auto self = new MemEditor();
      self->create(service);
      return self;
    }

    static void* s_serve(debugger_service_t* service)
    {
      auto self = new MemEditor();
      self->serve(service);
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
    debugger_service_t* _service;
    bool _showCombo;
    int _selected;
    MemoryEditor _editor;

    void create(debugger_service_t* service)
    {
      struct Locality
      {
        static uint8_t read(size_t address, void* udata)
        {
          auto memory = (debugger_memory_t*)udata;
          auto part = memory->parts;
          auto end = memory->parts + memory->count;

          while (part < end && address >= part->size)
          {
            address -= part->size;
            part++;
          }

          return part < end ? part->pointer[address] : 0;
        }

        static void write(size_t address, uint8_t byte, void* udata)
        {
          auto memory = (debugger_memory_t*)udata;
          auto part = memory->parts;
          auto end = memory->parts + memory->count;

          while (part < end && address >= part->size)
          {
            address -= part->size;
            part++;
          }

          if (part < end)
          {
            part->pointer[address] = byte;
          }
        }

        static int highlight(size_t address, void* udata)
        {
          return 0;
        }
      };

      service->destroyUd = NULL;
      service->editMemory.readOnly = false;
      service->editMemory.read = Locality::read;
      service->editMemory.write = Locality::write;
      service->editMemory.highlight = Locality::highlight;

      _showCombo = true;
      _selected = 0;
      setup(service);
    }

    void serve(debugger_service_t* service)
    {
      _showCombo = false;
      setup(service);
    }

    void setup(debugger_service_t* service)
    {
      _service = service;

      struct Locality
      {
        static MemoryEditor::u8 read(MemoryEditor::u8* data, size_t off)
        {
          auto service = (const debugger_service_t*)data;
          return service->editMemory.read(off, service->userData);
        }

        static void write(MemoryEditor::u8* data, size_t off, MemoryEditor::u8 d)
        {
          auto service = (const debugger_service_t*)data;
          service->editMemory.write(off, d, service->userData);
        }

        static bool highlight(MemoryEditor::u8* data, size_t off)
        {
          auto service = (const debugger_service_t*)data;
          return service->editMemory.highlight(off, service->userData);
        }
      };

      _editor.ReadFn = Locality::read;
      _editor.WriteFn = Locality::write;
      _editor.HighlightFn = Locality::highlight;
      _editor.HighlightColor = IM_COL32(192, 0, 255, 128);
    }

    bool destroy(bool force)
    {
      (void)force;
      return true;
    }

    void draw()
    {
      if (_showCombo)
      {
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

        unsigned count;
        debugger_memory_t* memory = s_info->coreInfo->getMemoryRegions(&count);

        if (_selected > (int)count)
        {
          _selected = 0;
        }

        ImGui::Combo("Region", &_selected, Locality::description, (void*)memory, count + 1);

        if (_selected != 0)
        {
          debugger_memory_t* mem = &memory[_selected - 1];
          _service->userData = mem;
          _service->editMemory.size = mem->size;
          _editor.DrawContents((unsigned char*)_service, mem->size);
        }
      }
      else
      {
        _editor.DrawContents((unsigned char*)_service, _service->editMemory.size);
      }
    }
  };
}

static const unsigned services[] = {DEBUGGER_SERVICE_EDIT_MEMORY, 0};

static const debugger_plugin_t plugin =
{
  ICON_FA_MICROCHIP " Memory Editor",
  services,
  MemEditor::s_create,
  MemEditor::s_serve,
  MemEditor::s_destroy,
  MemEditor::s_draw
};

void init_memeditor(debugger_register_plugin_t register_plugin, const debugger_t* info)
{
  s_info = info;
  register_plugin(&plugin);
}
