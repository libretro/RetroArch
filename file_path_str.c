/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "file_path_special.h"

const char *file_path_str(enum file_path_enum enum_idx)
{
   switch (enum_idx)
   {
      case FILE_PATH_PROGRAM_NAME:
#if defined(IS_SALAMANDER)
         return "RetroArch Salamander";
#else
         return "RetroArch";
#endif
      case FILE_PATH_DETECT:
         return "DETECT";
      case FILE_PATH_CONTENT_BASENAME:
         return "content.png";
      case FILE_PATH_LUTRO_PLAYLIST:
         return "Lutro.lpl";
      case FILE_PATH_NUL:
         return "nul";
      case FILE_PATH_LOG_WARN:
         return "[WARN]";
      case FILE_PATH_LOG_ERROR:
         return "[ERROR]";
      case FILE_PATH_LOG_INFO:
         return "[INFO]";
      case FILE_PATH_CGP_EXTENSION:
         return ".cgp";
      case FILE_PATH_GLSLP_EXTENSION:
         return ".glslp";
      case FILE_PATH_SLANGP_EXTENSION:
         return ".slangp";
      case FILE_PATH_AUTO_EXTENSION:
         return ".auto";
      case FILE_PATH_BSV_EXTENSION:
         return ".bsv";
      case FILE_PATH_OPT_EXTENSION:
         return ".opt";
      case FILE_PATH_CORE_INFO_EXTENSION:
         return ".info";
      case FILE_PATH_CONFIG_EXTENSION:
         return ".cfg";
      case FILE_PATH_REMAP_EXTENSION:
         return ".rmp";
      case FILE_PATH_RTC_EXTENSION:
         return ".rtc";
      case FILE_PATH_CHT_EXTENSION:
         return ".cht";
      case FILE_PATH_SRM_EXTENSION:
         return ".srm";
      case FILE_PATH_STATE_EXTENSION:
         return ".state";
      case FILE_PATH_LPL_EXTENSION:
         return ".lpl";
      case FILE_PATH_LPL_EXTENSION_NO_DOT:
         return "lpl";
      case FILE_PATH_PNG_EXTENSION:
         return ".png";
      case FILE_PATH_JPEG_EXTENSION:
         return ".jpeg";
      case FILE_PATH_BMP_EXTENSION:
         return ".bmp";
      case FILE_PATH_TGA_EXTENSION:
         return ".tga";
      case FILE_PATH_JPG_EXTENSION:
         return ".jpg";
      case FILE_PATH_UPS_EXTENSION:
         return ".ups";
      case FILE_PATH_IPS_EXTENSION:
         return ".ips";
      case FILE_PATH_BPS_EXTENSION:
         return ".bps";
      case FILE_PATH_RDB_EXTENSION:
         return ".rdb";
      case FILE_PATH_ZIP_EXTENSION:
         return ".zip";
      case FILE_PATH_7Z_EXTENSION:
         return ".7z";
      case FILE_PATH_INDEX_URL:
         return ".index";
      case FILE_PATH_INDEX_DIRS_URL:
         return ".index-dirs";
      case FILE_PATH_INDEX_EXTENDED_URL:
         return ".index-extended";
      case FILE_PATH_CORE_THUMBNAILS_URL:
         return "http://thumbnailpacks.libretro.com";
      case FILE_PATH_LAKKA_URL:
         return "http://mirror.lakka.tv/nightly";
      case FILE_PATH_SHADERS_GLSL_ZIP:
         return "shaders_glsl.zip";
      case FILE_PATH_SHADERS_CG_ZIP:
         return "shaders_cg.zip";
      case FILE_PATH_DATABASE_RDB_ZIP:
         return "database-rdb.zip";
      case FILE_PATH_OVERLAYS_ZIP:
         return "overlays.zip";
      case FILE_PATH_CORE_INFO_ZIP:
         return "info.zip";
      case FILE_PATH_CHEATS_ZIP:
         return "cheats.zip";
      case FILE_PATH_ASSETS_ZIP:
         return "assets.zip";
      case FILE_PATH_AUTOCONFIG_ZIP:
         return "autoconfig.zip";
      case FILE_PATH_CONTENT_HISTORY:
         return "content_history.lpl";
      case FILE_PATH_CONTENT_MUSIC_HISTORY:
         return "content_music_history.lpl";
      case FILE_PATH_CONTENT_VIDEO_HISTORY:
         return "content_video_history.lpl";
      case FILE_PATH_CONTENT_IMAGE_HISTORY:
         return "content_image_history.lpl";
      case FILE_PATH_CORE_OPTIONS_CONFIG:
         return "retroarch-core-options.cfg";
      case FILE_PATH_MAIN_CONFIG:
         return "retroarch.cfg";
      case FILE_PATH_BACKGROUND_IMAGE:
         return "bg.png";
      case FILE_PATH_TTF_FONT:
         return "font.ttf";
      case FILE_PATH_UNKNOWN:
      default:
         break;
   }

   return "null";
}
