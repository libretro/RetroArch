#include "plugin.h"

#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <string>
#include <vector>

#include "imgui.h"
#include "imguial_fonts.h"
#include "imguial_button.h"

#include "block.h"
#include "set.h"

static const debugger_t* s_info;

namespace
{
  enum
  {
    CORE_FCEUMM = 0xb00bd8c2U
  };

  uint32_t djb2(const char* str)
  {
    uint32_t hash = 5381;                
                                        
    while (*str)                       
    {                                    
      hash = hash * 33 + (uint8_t)*str++;
    }                                    
                                        
    return hash;                         
  }

  class Hunter
  {
  public:
    static void* s_create(void)
    {
      auto self = new Hunter();
      self->create();
      return self;
    }

    static int s_destroy(void* instance, int force)
    {
      auto self = (Hunter*)instance;

      if (self->destroy(force != 0) || force)
      {
        delete self;
        return 1;
      }

      return 0;
    }

    static void s_draw(void* instance)
    {
      auto self = (Hunter*)instance;
      self->draw();
    }
  
  protected:
    struct Region
    {
      std::string name;
      void*       data;
      size_t      size;
    };

    struct Snapshot
    {
      char               name[64];
      std::vector<Block> blocks;
    };

    struct Filter
    {
      char              name[64];
      std::vector<Set*> sets;

      ~Filter()
      {
        for (auto it = sets.begin(); it != sets.end(); ++it)
        {
          Set* set = &*it;
          set->destroy();
          delete set;
        }
      }
    }

    bool     _inited;
    unsigned _temp;
    bool     _cheevosMode;

    std::vector<Region> _regions;
    int _regsel;

    std::vector<Snapshot> _snapshots;
    int _snapsel;

    int _opsel;
    int _sizesel;

    char _value[64];

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
          snprintf(name, sizeof(name), "Memory @%p", (void*)(uintptr_t)mem->start);

          region.name = name;
          region.data = (char*)mem->ptr + mem->offset;
          region.size = mem->len;
          _regions.push_back(region);
        }
      }
    }

    void createSnapshot(bool cheevosMode, const std::vector<Region>& regions, int regsel)
    {
      Snapshot snap;
      snprintf(snap.name, sizeof(snap.name), "Snapshot %u", _temp++);

      if (!cheevosMode)
      {
        Block block;
        block.init(0, regions[regsel].size, (uint8_t*)regions[regsel].data);

        snap.blocks.push_back(block);
        _snapshots.push_back(snap);
        return;
      }

      uint32_t hash = djb2(s_info->coreInfo->getSystemInfo()->library_name);

      switch (hash)
      {
      case CORE_FCEUMM:
        {
          for (unsigned i = 0;; i++)
          {
            const struct retro_memory_descriptor* mem = s_info->coreInfo->getMemoryMap(i);

            if (!mem)
            {
              break;
            }

            if (mem->ptr && mem->len)
            {
              Block block;
              block.init(mem->start, mem->len, (uint8_t*)mem->ptr + mem->offset);
              snap.blocks.push_back(block);
            }
          }

          break;
        }
      }

      _snapshots.push_back(snap);
    }

    bool getValue(uint64_t* value)
    {
      bool valid = _snapsel != 0 && _value[0] != 0;
      *value = 0;

      if (_value[0] == '0' && _value[1] == 'b')
      {
        valid = valid && _value[2] != 0;

        for (int i = 2; _value[i] != 0; i++)
        {
          char k = _value[i];
          *value *= 2;

          if (k == '0' || k == '1')
            *value += k - '0';
          else
            valid = false;
        }
      }
      else if (_value[0] == '0' && _value[1] == 'x')
      {
        valid = valid && _value[2] != 0;

        for (int i = 2; _value[i] != 0; i++)
        {
          char k = tolower(_value[i]);
          *value *= 16;

          if (k >= '0' && k <= '9')
            *value += k - '0';
          else if (k >= 'a' && k <= 'f')
            *value += k - 'a' + 10;
          else
            valid = false;
        }
      }
      else
      {
        for (int i = 0; _value[i] != 0; i++)
        {
          char k = _value[i];
          *value *= 10;

          if (k >= '0' && k <= '9')
            *value += k - '0';
          else
            *valid = false;
        }
      }

      switch (_sizesel)
      {
      case 0: valid = valid && value <= 0xff; break;
      case 1: // fallthrough
      case 2: valid = valid && value <= 0xffff; break;
      case 3: // fallthrough
      case 4: valid = valid && value <= 0xffffffff; break;
      case 5: // fallthrough
      case 6: valid = valid && value <= 0xf; break;
      default: valie = valid && value <= 1; break;
      }

      return valid;
    }

#define FILTER(n) \
  for (auto it = snap->blocks.begin(); it != snap->blocks.end(); ++it) \
  { \
    filter->sets.push_back(it->n(value)); \
  } \
  break;

    void createFilter(Filter* filter, uint64_t value)
    {
      Snapshot* snap = _snapshots[_snapsel];
      Filter filter;

      switch (_opsel)
      {
      case 0: // ==
        switch (_sizesel)
        {
        case  0: FILTER(eq8)
        case  1: FILTER(eq16LE)
        case  2: FILTER(eq16BE)
        case  3: FILTER(eq32LE)
        case  4: FILTER(eq32BE)
        case  5: FILTER(eqlow)
        case  6: FILTER(eqhigh)
        case  7: FILTER(bit<0>)
        case  8: FILTER(bit<1>)
        case  9: FILTER(bit<2>)
        case 10: FILTER(bit<3>)
        case 11: FILTER(bit<4>)
        case 12: FILTER(bit<5>)
        case 13: FILTER(bit<6>)
        case 14: FILTER(bit<7>)
        }

        break;

      case 1: // !=
        switch (_sizesel)
        {
        case 0: FILTER(ne8)
        case 1: FILTER(ne16LE)
        case 2: FILTER(ne16BE)
        case 3: FILTER(ne32LE)
        case 4: FILTER(ne32BE)
        case 5: FILTER(nelow)
        case 6: FILTER(nehigh)
        }

        break;

      case 2: // <
        switch (_sizesel)
        {
        case 0: FILTER(lt8)
        case 1: FILTER(lt16LE)
        case 2: FILTER(lt16BE)
        case 3: FILTER(lt32LE)
        case 4: FILTER(lt32BE)
        case 5: FILTER(ltlow)
        case 6: FILTER(lthigh)
        }

        break;

      case 3: // <=
        switch (_sizesel)
        {
        case 0: FILTER(le8)
        case 1: FILTER(le16LE)
        case 2: FILTER(le16BE)
        case 3: FILTER(le32LE)
        case 4: FILTER(le32BE)
        case 5: FILTER(lelow)
        case 6: FILTER(lehigh)
        }

        break;
      }
    }

    void create()
    {
      _inited = false;
      _temp = 1;
      _cheevosMode = true;
      _regsel = 0;
      _snapsel = 0;
      _opsel = 0;
      _sizesel = 0;
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
          _regsel = 0;
          _snapshots.clear();
          _snapsel = 0;
          _opsel = 0;
          _sizesel = 0;
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

      ImGui::SetNextTreeNodeOpen(true);

      if (ImGui::CollapsingHeader("Snapshots"))
      {
        ImGui::Checkbox("Retro Achievements mode", &_cheevosMode);

        if (!_cheevosMode)
        {
          struct Getter
          {
            static bool description(void* data, int idx, const char** out_text)
            {
              auto regions = (std::vector<Region>*)data;
              *out_text = (*regions)[idx].name.c_str();
              return true;
            }
          };

          ImGui::Separator();
          ImGui::Combo("Region", &_regsel, Getter::description, (void*)&_regions, _regions.size());
        }

        if (ImGui::Button(ICON_FA_PLUS " Create snapshot"))
        {
          createSnapshot(_cheevosMode, _regions, _regsel);
        }

        ImGui::Separator();

        for (auto it = _snapshots.begin(); it != _snapshots.end();)
        {
          Snapshot* snap = &*it;

          char label[128];
          snprintf(label, sizeof(label), "##snapname%p", snap);

          ImGui::InputText(label, snap->name, sizeof(snap->name));
          ImGui::SameLine();

          snprintf(label, sizeof(label), ICON_FA_MINUS " Delete##snapdelete%p", snap);

          if (ImGui::Button(label))
          {
            it = _snapshots.erase(it);
            _snapsel = 0;
          }
          else
          {
            ++it;
          }
        }
      }

      ImGui::SetNextTreeNodeOpen(true);

      if (ImGui::CollapsingHeader("Filters"))
      {
        struct Getter
        {
          static bool description(void* data, int idx, const char** out_text)
          {
            if (idx != 0)
            {
              auto snapshots = (std::vector<Snapshot>*)data;
              *out_text = (*snapshots)[idx - 1].name;
            }
            else
            {
              *out_text = "None";
            }

            return true;
          }
        };

        ImGui::Separator();
        ImGui::Combo("Snapshot", &_snapsel, Getter::description, (void*)&_snapshots, _snapshots.size() + 1);

        static const char* operators[] = {
          "==", "!=", "<", "<=", ">", ">="
        };

        ImGui::Combo("Operator", &_opsel, operators, sizeof(operators) / sizeof(operators[0]));

        static const char* sizes[] = {
          "8 bits", "16 bits LE", "16 bits BE", "32 bits LE", "32 bits BE", "Low nibble", "High nibble",
          "Bit 0", "Bit 1", "Bit 2", "Bit 3", "Bit 4", "Bit 5", "Bit 6", "Bit 7"
        };

        int max = sizeof(sizes) / sizeof(sizes[0]) - 8 * (_opsel > 0);

        if (_sizesel >= max)
        {
          _sizesel = 0;
        }

        ImGui::Combo("Operand size", &_sizesel, sizes, max);
        ImGui::InputText("Value", _value, sizeof(_value));

        uint64_t value;
        bool valid = getValue(&value);

        if (ImGuiAl::Button(ICON_FA_PLUS " Create filter", valid))
        {
          Set* set = createFilter(value);
        }
      }
    }
  };
}

static const debugger_plugin_t plugin =
{
  ICON_FA_SEARCH " Memory Hunter",
  Hunter::s_create,
  Hunter::s_destroy,
  Hunter::s_draw
};

void init_hunter(debugger_register_plugin_t register_plugin, const debugger_t* info)
{
  s_info = info;
  register_plugin(&plugin);
}
