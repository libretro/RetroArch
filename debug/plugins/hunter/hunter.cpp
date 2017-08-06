#include "plugin.h"

#include <stdio.h>
#include <stdint.h>
#include <cinttypes>
#include <ctype.h>
#include <string>
#include <vector>

#include "imgui.h"
#include "imguial_fonts.h"
#include "imguial_button.h"

#include "block.h"
#include "set.h"

static const char* sizeNames[] = {
  "8 bits", "16 bits LE", "16 bits BE", "32 bits LE", "32 bits BE", "Low nibble", "High nibble",
  "Bit 0", "Bit 1", "Bit 2", "Bit 3", "Bit 4", "Bit 5", "Bit 6", "Bit 7"
};

static const char* operatorNames[] = {
  "==", "!=", "<", "<=", ">", ">="
};

static const debugger_t* s_info;

namespace
{
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
    struct Snapshot
    {
      char     name[64];
      Block    blocks[64];
      unsigned count;
      size_t   sizes;
    };

    struct Filter
    {
      char     name[4096];
      Set      sets[64];
      unsigned count;
      size_t   total;
      size_t   sizes;
    };

    bool     _inited;
    unsigned _temp;
    bool     _cheevosMode;

    debugger_memory_t* _regions;
    unsigned _regionsCount;
    int _regionsSel;

    std::vector<Snapshot> _snapshots;

    int _snapsCmpSel1;
    int _snapsCmpSel2;
    int _snapsCmpOp;
    int _snapsCmpSize;

    int _snapCmpSel;
    int _snapCmpOp;
    int _snapCmpSize;

    char _value[64];

    std::vector<Filter> _filters;

    int _filtersCmpSel1;
    int _filtersCmpSel2;
    int _filtersCmpOp;

    void createSnapshot()
    {
      Snapshot snap;
      snprintf(snap.name, sizeof(snap.name), "Snapshot %u", _temp++);
      snap.count = 0;
      snap.sizes = 0;

      if (!_cheevosMode)
      {
        debugger_memory_part_t* part = _regions[_regionsSel].parts;

        for (unsigned i = 0; i < _regions[_regionsSel].count; i++, part++)
        {
          if (snap.count < sizeof(snap.blocks) / sizeof(snap.blocks[0]))
          {
            Block* block = &snap.blocks[snap.count++];
            block->init(0, part->size, part->pointer);
          }
        }
      }
      else
      {
        for (unsigned i = 0; i < _regionsCount; i++)
        {
          debugger_memory_part_t* part = _regions[i].parts;

          for (unsigned j = 0; j < _regions[i].count; j++, part++)
          {
            if (snap.count < sizeof(snap.blocks) / sizeof(snap.blocks[0]))
            {
              Block* block = &snap.blocks[snap.count++];
              block->init(0, part->size, part->pointer);
            }
          }
        }
      }

      _snapshots.push_back(snap);
    }

    bool getValue(uint64_t* value)
    {
      bool valid = _snapCmpSel != 0 && _value[0] != 0;
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
            valid = false;
        }
      }

      switch (_snapCmpSize)
      {
      case 0: valid = valid && *value <= 0xff; break;
      case 1: // fallthrough
      case 2: valid = valid && *value <= 0xffff; break;
      case 3: // fallthrough
      case 4: valid = valid && *value <= 0xffffffff; break;
      case 5: // fallthrough
      case 6: valid = valid && *value <= 0xf; break;
      default: valid = valid && *value <= 1; break;
      }

      return valid;
    }

#define FILTER_FROM_SNAP(n) \
  for (unsigned i = 0; i < snap->count; i++) \
  { \
    if (filter.count < sizeof(filter.sets) / sizeof(filter.sets[0])) { \
      Set* set = &filter.sets[filter.count++]; \
      snap->blocks[i].n(set, value); \
      filter.total += set->count(); \
      filter.sizes += set->size(); \
    } \
  } \
  break;

#define FILTERS_FROM_SNAP(n) \
  case  0: FILTER_FROM_SNAP(n ## 8) \
  case  1: FILTER_FROM_SNAP(n ## 16LE) \
  case  2: FILTER_FROM_SNAP(n ## 16BE) \
  case  3: FILTER_FROM_SNAP(n ## 32LE) \
  case  4: FILTER_FROM_SNAP(n ## 32BE) \
  case  5: FILTER_FROM_SNAP(n ## low) \
  case  6: FILTER_FROM_SNAP(n ## high)

    void createFilterFromSnapshot(uint64_t value)
    {
      Snapshot* snap = &_snapshots[_snapCmpSel - 1];

      Filter filter;
      filter.count = 0;
      filter.total = 0;
      filter.sizes = 0;

      switch (_snapCmpOp)
      {
      case 0:
        switch (_snapCmpSize)
        {
        FILTERS_FROM_SNAP(eq)
        case  7: FILTER_FROM_SNAP(bit<0>)
        case  8: FILTER_FROM_SNAP(bit<1>)
        case  9: FILTER_FROM_SNAP(bit<2>)
        case 10: FILTER_FROM_SNAP(bit<3>)
        case 11: FILTER_FROM_SNAP(bit<4>)
        case 12: FILTER_FROM_SNAP(bit<5>)
        case 13: FILTER_FROM_SNAP(bit<6>)
        case 14: FILTER_FROM_SNAP(bit<7>)
        }

        break;

      case 1: switch (_snapCmpSize) { FILTERS_FROM_SNAP(ne) } break;
      case 2: switch (_snapCmpSize) { FILTERS_FROM_SNAP(lt) } break;
      case 3: switch (_snapCmpSize) { FILTERS_FROM_SNAP(le) } break;
      case 4: switch (_snapCmpSize) { FILTERS_FROM_SNAP(gt) } break;
      case 5: switch (_snapCmpSize) { FILTERS_FROM_SNAP(ge) } break;
      }

      snprintf(filter.name, sizeof(filter.name), "((%s) %s %" PRIu64 " (%s)) (%" PRIu64 " hits)", snap->name, operatorNames[_snapCmpOp], value, sizeNames[_snapCmpSize], filter.total);
      _filters.push_back(filter);
    }

#define FILTER_FROM_SNAPS(n) \
  for (unsigned i = 0; i < snap1->count; i++) \
  { \
    if (filter.count < sizeof(filter.sets) / sizeof(filter.sets[0])) { \
      Set* set = &filter.sets[filter.count++]; \
      snap1->blocks[i].n(set, &snap2->blocks[i]); \
      filter.total += set->count(); \
      filter.sizes += set->size(); \
    } \
  } \
  break;

#define FILTERS_FROM_SNAPS(n) \
  case  0: FILTER_FROM_SNAPS(n ## 8) \
  case  1: FILTER_FROM_SNAPS(n ## 16LE) \
  case  2: FILTER_FROM_SNAPS(n ## 16BE) \
  case  3: FILTER_FROM_SNAPS(n ## 32LE) \
  case  4: FILTER_FROM_SNAPS(n ## 32BE) \
  case  5: FILTER_FROM_SNAPS(n ## low) \
  case  6: FILTER_FROM_SNAPS(n ## high)

    void createFilterFromSnapshots()
    {
      Snapshot* snap1 = &_snapshots[_snapsCmpSel1 - 1];
      Snapshot* snap2 = &_snapshots[_snapsCmpSel2 - 1];

      Filter filter;
      filter.count = 0;
      filter.total = 0;
      filter.sizes = 0;

      switch (_snapsCmpOp)
      {
      case 0: switch (_snapsCmpSize) { FILTERS_FROM_SNAPS(eq) } break;
      case 1: switch (_snapsCmpSize) { FILTERS_FROM_SNAPS(ne) } break;
      case 2: switch (_snapsCmpSize) { FILTERS_FROM_SNAPS(lt) } break;
      case 3: switch (_snapsCmpSize) { FILTERS_FROM_SNAPS(le) } break;
      case 4: switch (_snapsCmpSize) { FILTERS_FROM_SNAPS(gt) } break;
      case 5: switch (_snapsCmpSize) { FILTERS_FROM_SNAPS(ge) } break;
      }

      snprintf(filter.name, sizeof(filter.name), "((%s) %s (%s) (%s)) (%" PRIu64 " hits)", snap1->name, operatorNames[_snapsCmpOp], snap2->name, sizeNames[_snapsCmpSize], filter.total);
      _filters.push_back(filter);
    }

#define FILTER_FROM_FILTERS(n) \
  for (unsigned i = 0; i < filter1->count; i++) \
  { \
    Set* set = &filter.sets[filter.count++]; \
    filter1->sets[i].n(set, &filter2->sets[i]); \
    filter.total += set->count(); \
    filter.sizes += set->size(); \
  } \
  break;

    void createFilterFromFilters()
    {
      Filter* filter1 = &_filters[_filtersCmpSel1 - 1];
      Filter* filter2 = _filtersCmpSel2 == 0 ? nullptr : &_filters[_filtersCmpSel2 - 1];

      Filter filter;
      filter.count = 0;
      filter.total = 0;
      filter.sizes = 0;
      
      switch (_filtersCmpOp)
      {
      case 0: FILTER_FROM_FILTERS(intersection);
      case 1: FILTER_FROM_FILTERS(union_);
      case 2: FILTER_FROM_FILTERS(difference);

      case 3:
        for (unsigned i = 0; i < filter1->count; i++)
        {
          Set* set = &filter.sets[filter.count++];
          filter1->sets[i].negate(set);
          filter.total += set->count();
          filter.sizes += set->size();
        }

        break;
      }

      switch (_filtersCmpOp)
      {
      case 0: snprintf(filter.name, sizeof(filter.name), "((%s) * (%s)) (%" PRIu64 " hits)", filter1->name, filter2->name, filter.total); break;
      case 1: snprintf(filter.name, sizeof(filter.name), "((%s) + (%s)) (%" PRIu64 " hits)", filter1->name, filter2->name, filter.total); break;
      case 2: snprintf(filter.name, sizeof(filter.name), "((%s) - (%s)) (%" PRIu64 " hits)", filter1->name, filter2->name, filter.total); break;
      case 3: snprintf(filter.name, sizeof(filter.name), "(~(%s)) (%" PRIu64 " hits)", filter1->name, filter.total); break;
      }

      _filters.push_back(filter);
    }

    void showSnapshotsList()
    {
      ImGui::PushID(__FUNCTION__);
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
              auto mem = (const debugger_memory_t*)data;
              *out_text = mem[idx].name;
              return true;
            }
          };

          ImGui::Separator();
          ImGui::Combo("Region", &_regionsSel, Getter::description, (void*)_regions, _regionsCount);
        }

        if (ImGui::Button(ICON_FA_PLUS " Create snapshot"))
        {
          createSnapshot();
        }

        ImGui::Separator();

        for (auto it = _snapshots.begin(); it != _snapshots.end();)
        {
          Snapshot* snap = &*it;
          ImGui::PushID(snap);

          ImGui::InputText("", snap->name, sizeof(snap->name));
          ImGui::SameLine();

          if (ImGui::Button(ICON_FA_MINUS " Delete"))
          {
            for (unsigned i = 0; i < it->count; i++)
            {
              it->blocks[i].destroy();
            }

            it = _snapshots.erase(it);
            _snapCmpSel = 0;
            _snapsCmpSel1 = 0;
            _snapsCmpSel2 = 0;
          }
          else
          {
            ++it;
          }

          ImGui::PopID();
        }
      }
      
      ImGui::PopID();
    }

    void showFiltersFromSnapshot()
    {
      ImGui::PushID(__FUNCTION__);

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

      ImGui::Combo("Snapshot", &_snapCmpSel, Getter::description, (void*)&_snapshots, _snapshots.size() + 1);
      ImGui::Combo("Operator", &_snapCmpOp, operatorNames, sizeof(operatorNames) / sizeof(operatorNames[0]));

      int max = sizeof(sizeNames) / sizeof(sizeNames[0]) - 8 * (_snapCmpOp > 0);

      if (_snapCmpSize >= max)
      {
        _snapCmpSize = 0;
      }

      ImGui::Combo("Operand size", &_snapCmpSize, sizeNames, max);
      ImGui::InputText("Value", _value, sizeof(_value));

      uint64_t value;
      bool valid = getValue(&value);

      if (ImGuiAl::Button(ICON_FA_PLUS " Create filter", valid))
      {
        createFilterFromSnapshot(value);
      }

      ImGui::PopID();
    }

    void showFiltersFromSnapshots()
    {
      ImGui::PushID(__FUNCTION__);

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
      
      ImGui::Combo("1st Snapshot", &_snapsCmpSel1, Getter::description, (void*)&_snapshots, _snapshots.size() + 1);
      ImGui::Combo("Operator", &_snapsCmpOp, operatorNames, sizeof(operatorNames) / sizeof(operatorNames[0]));
      ImGui::Combo("2nd Snapshot", &_snapsCmpSel2, Getter::description, (void*)&_snapshots, _snapshots.size() + 1);

      bool valid = _snapsCmpSel1 != 0 && _snapsCmpSel2 != 0;

      if (valid)
      {
        Snapshot* snap1 = &_snapshots[_snapsCmpSel1 - 1];
        Snapshot* snap2 = &_snapshots[_snapsCmpSel2 - 1];

        valid = valid && snap1->count == snap2->count;

        for (unsigned i = 0; i < snap1->count; i++)
        {
          valid = valid && snap1->blocks[i].compatible(&snap2->blocks[i]);
        }
      }

      if (ImGuiAl::Button(ICON_FA_PLUS " Create filter", valid))
      {
        createFilterFromSnapshots();
      }

      ImGui::PopID();
    }

    void showFiltersFromFilters()
    {
      ImGui::PushID(__FUNCTION__);

      struct Getter
      {
        static bool description(void* data, int idx, const char** out_text)
        {
          if (idx != 0)
          {
            auto filters = (std::vector<Filter>*)data;
            *out_text = (*filters)[idx - 1].name;
          }
          else
          {
            *out_text = "None";
          }

          return true;
        }
      };

      ImGui::Combo("1st Filter", &_filtersCmpSel1, Getter::description, (void*)&_filters, _filters.size() + 1);

      static const char* operators2[] = {
        "Intersection", "Union", "Difference", "Complement"
      };

      ImGui::Combo("Operator", &_filtersCmpOp, operators2, sizeof(operators2) / sizeof(operators2[0]));
      ImGui::Combo("2nd Filter", &_filtersCmpSel2, Getter::description, (void*)&_filters, _filters.size() + 1);

      bool valid = _filtersCmpSel1 != 0;
      valid = valid && ((_filtersCmpOp != 3 && _filtersCmpSel2 != 0) || (_filtersCmpOp == 3 && _filtersCmpSel2 == 0));

      if (valid && _filtersCmpOp != 3)
      {
        Filter* filter1 = _filtersCmpSel1 == 0 ? nullptr : &_filters[_filtersCmpSel1 - 1];
        Filter* filter2 = _filtersCmpSel2 == 0 ? nullptr : &_filters[_filtersCmpSel2 - 1];

        valid = valid && filter1->count == filter2->count;

        for (unsigned i = 0; i < filter1->count; i++)
        {
          valid = valid && filter1->sets[i].compatible(&filter2->sets[i]);
        }
      }

      if (ImGuiAl::Button(ICON_FA_PLUS " Create filter", valid))
      {
        createFilterFromFilters();
      }

      ImGui::PopID();
    }

    void showFiltersList()
    {
      ImGui::PushID(__FUNCTION__);

      for (auto it = _filters.begin(); it != _filters.end();)
      {
        Filter* filter = &*it;
        ImGui::PushID(filter);

        ImGui::InputText("", filter->name, sizeof(filter->name));
        ImGui::SameLine();

        if (ImGui::Button(ICON_FA_MINUS " Delete"))
        {
          for (unsigned i = 0; i < it->count; i++)
          {
            it->sets[i].destroy();
          }

          it = _filters.erase(it);
          _filtersCmpSel1 = 0;
          _filtersCmpSel2 = 0;
        }
        else
        {
          ++it;
        }

        ImGui::SameLine();

        if (ImGui::Button(ICON_FA_MINUS " Show"))
        {
          for (unsigned i = 0; i < filter->count; i++)
          {
            Set* set = &filter->sets[i];
            size_t addr;

            if (set->first(&addr))
            {
              do
              {
                s_info->log->info("%" PRIx64, addr);
              }
              while (set->next(&addr));
            }
          }
        }

        ImGui::PopID();
      }

      ImGui::PopID();
    }

    void clear()
    {
      _regionsCount = 0;
      _regionsSel = 0;
      _snapshots.clear();
      _snapCmpSel = 0;
      _snapCmpOp = 0;
      _snapCmpSize = 0;
      _snapsCmpSel1 = 0;
      _snapsCmpSel2 = 0;
      _snapsCmpOp = 0;
      _snapsCmpSize = 0;
      _filtersCmpSel1 = 0;
      _filtersCmpSel2 = 0;
      _filtersCmpOp = 0;
      _inited = false;
    }

    void create()
    {
      _temp = 1;
      _cheevosMode = true;

      clear();
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
          clear();
        }
      }
      else
      {
        if (s_info->rarchInfo->isGameLoaded())
        {
          _regions = s_info->coreInfo->getMemoryRegions(&_regionsCount);
          _inited = true;
        }
      }

      showSnapshotsList();

      ImGui::SetNextTreeNodeOpen(true);

      if (ImGui::CollapsingHeader("Filters"))
      {
        ImGui::Columns(3);
        showFiltersFromSnapshot();
        ImGui::NextColumn();
        showFiltersFromSnapshots();
        ImGui::NextColumn();
        showFiltersFromFilters();
        ImGui::Columns(1);
        ImGui::Separator();
        showFiltersList();
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
