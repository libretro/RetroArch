#include "plugin.h"

#include <stdarg.h>

#include "imgui.h"
#include "imguial_fonts.h"

static const debugger_t* s_info;

namespace
{
  class Info
  {
  public:
    static void* s_create()
    {
      auto self = new Info();
      self->create();
      return self;
    }

    static int s_destroy(void* instance, int force)
    {
      auto self = (Info*)instance;

      if (self->destroy(force) || force)
      {
        delete self;
        return 1;
      }

      return 0;
    }

    static void s_draw(void* instance)
    {
      auto self = (Info*)instance;
      self->draw();
    }
  
  protected:
    static void table(int columns, ...)
    {
      ImGui::Columns(columns, NULL, true);

      va_list args;
      va_start(args, columns);

      for (;;)
      {
        const char* name = va_arg(args, const char*);

        if (name == NULL)
        {
          break;
        }

        ImGui::Separator();
        ImGui::Text("%s", name);
        ImGui::NextColumn();

        for (int col = 1; col < columns; col++)
        {
          switch (va_arg(args, int))
          {
          case 's': ImGui::Text("%s", va_arg(args, const char*)); break;
          case 'd': ImGui::Text("%d", va_arg(args, int)); break;
          case 'u': ImGui::Text("%u", va_arg(args, unsigned)); break;
          case 'f': ImGui::Text("%f", va_arg(args, double)); break;
          case 'b': ImGui::Text("%s", va_arg(args, int) ? "true" : "false"); break;
          }

          ImGui::NextColumn();
        }
      }

      va_end(args);

      ImGui::Columns(1);
      ImGui::Separator();
    }

    void create()
    {
    }

    int destroy(int force)
    {
      (void)force;
      return 1;
    }

    void draw()
    {
      if (ImGui::CollapsingHeader("Basic Information"))
      {
        static const char* pixel_formats[] =
        {
          "0RGB1555", "XRGB8888", "RGB565"
        };
        
        enum retro_pixel_format ndx = s_info->coreInfo->getPixelFormat();
        const char* pixel_format = "Unknown";

        if (ndx != RETRO_PIXEL_FORMAT_UNKNOWN)
        {
          pixel_format = pixel_formats[ndx];
        }

        table(
          2,
          "Api Version",           'u', s_info->coreInfo->getApiVersion(),
          "Region",                's', s_info->coreInfo->getRegion() == RETRO_REGION_NTSC ? "NTSC" : "PAL",
          "Rotation",              'u', s_info->coreInfo->getRotation() * 90,
          "Performance level",     'u', s_info->coreInfo->getPerformanceLevel(),
          "Pixel Format",          's', pixel_format,
          "Supports no Game",      'b', s_info->coreInfo->supportsNoGame(),
          "Supports Achievements", 'b', s_info->coreInfo->supportsCheevos(),
          "Save RAM Size",         'u', s_info->coreInfo->getMemorySize(RETRO_MEMORY_SAVE_RAM),
          "RTC Size",              'u', s_info->coreInfo->getMemorySize(RETRO_MEMORY_RTC),
          "System RAM Size",       'u', s_info->coreInfo->getMemorySize(RETRO_MEMORY_SYSTEM_RAM),
          "Video RAM Size",        'u', s_info->coreInfo->getMemorySize(RETRO_MEMORY_VIDEO_RAM),
          NULL
        );
      }

      if (ImGui::CollapsingHeader("retro_system_info"))
      {
        const struct retro_system_info* info = s_info->coreInfo->getSystemInfo();

        table(
          2,
          "library_name",     's', info->library_name,
          "library_version",  's', info->library_version,
          "valid_extensions", 's', info->valid_extensions,
          "need_fullpath",    'b', info->need_fullpath,
          "block_extract",    'b', info->block_extract,
          NULL
        );
      }

      if (ImGui::CollapsingHeader("retro_system_av_info"))
      {
        const struct retro_system_av_info* av_info = s_info->coreInfo->getSystemAVInfo();

        table(
          2,
          "base_width",   'u', av_info->geometry.base_width,
          "base_height",  'u', av_info->geometry.base_height,
          "max_width",    'u', av_info->geometry.max_width,
          "max_height",   'u', av_info->geometry.max_height,
          "aspect_ratio", 'f', av_info->geometry.aspect_ratio,
          "fps",          'f', av_info->timing.fps,
          "sample_rate",  'f', av_info->timing.sample_rate,
          NULL
        );
      }

      if (ImGui::CollapsingHeader("retro_memory_map"))
      {
        ImGui::Columns(8, NULL, true);
        ImGui::Separator();
        ImGui::Text("flags");
        ImGui::NextColumn();
        ImGui::Text("ptr");
        ImGui::NextColumn();
        ImGui::Text("offset");
        ImGui::NextColumn();
        ImGui::Text("start");
        ImGui::NextColumn();
        ImGui::Text("select");
        ImGui::NextColumn();
        ImGui::Text("disconnect");
        ImGui::NextColumn();
        ImGui::Text("len");
        ImGui::NextColumn();
        ImGui::Text("addrspace");
        ImGui::NextColumn();

        for (unsigned i = 0;; i++)
        {
          const struct retro_memory_descriptor* desc = s_info->coreInfo->getMemoryMap(i);

          if (!desc)
          {
            break;
          }

          char flags[7];

          flags[0] = 'M';

          if ((desc->flags & RETRO_MEMDESC_MINSIZE_8) == RETRO_MEMDESC_MINSIZE_8)
          {
            flags[1] = '8';
          }
          else if ((desc->flags & RETRO_MEMDESC_MINSIZE_4) == RETRO_MEMDESC_MINSIZE_4)
          {
            flags[1] = '4';
          }
          else if ((desc->flags & RETRO_MEMDESC_MINSIZE_2) == RETRO_MEMDESC_MINSIZE_2)
          {
            flags[1] = '2';
          }
          else
          {
            flags[1] = '1';
          }

          flags[2] = 'A';

          if ((desc->flags & RETRO_MEMDESC_ALIGN_8) == RETRO_MEMDESC_ALIGN_8)
          {
            flags[3] = '8';
          }
          else if ((desc->flags & RETRO_MEMDESC_ALIGN_4) == RETRO_MEMDESC_ALIGN_4)
          {
            flags[3] = '4';
          }
          else if ((desc->flags & RETRO_MEMDESC_ALIGN_2) == RETRO_MEMDESC_ALIGN_2)
          {
            flags[3] = '2';
          }
          else
          {
            flags[3] = '1';
          }

          flags[4] = desc->flags & RETRO_MEMDESC_BIGENDIAN ? 'B' : 'b';
          flags[5] = desc->flags & RETRO_MEMDESC_CONST ? 'C' : 'c';
          flags[6] = 0;

          ImGui::Separator();
          ImGui::Text("%s", flags);
          ImGui::NextColumn();
          ImGui::Text("%p", desc->ptr);
          ImGui::NextColumn();
          ImGui::Text("0x%08X", desc->offset);
          ImGui::NextColumn();
          ImGui::Text("0x%08X", desc->start);
          ImGui::NextColumn();
          ImGui::Text("0x%08X", desc->select);
          ImGui::NextColumn();
          ImGui::Text("0x%08X", desc->disconnect);
          ImGui::NextColumn();
          ImGui::Text("0x%08X", desc->len);
          ImGui::NextColumn();
          ImGui::Text("%s", desc->addrspace);
          ImGui::NextColumn();
        }

        ImGui::Columns(1);
        ImGui::Separator();
      }
    }
  };
}

static const debugger_plugin_t plugin =
{
  ICON_FA_INFO_CIRCLE " Information",
  Info::s_create,
  Info::s_destroy,
  Info::s_draw
};

void init_info(debugger_register_plugin_t register_plugin, const debugger_t* info)
{
  s_info = info;
  register_plugin(&plugin);
}
